

#include "menubar.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "cellselection.h"
#include "mainwindow.h"
#include "menubarpopup.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/toonzfolders.h"

// TnzTools includes
#include "tools/toolcommandids.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tconvert.h"
#include "tsystem.h"

// Qt includes
#include <QIcon>
#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QShortcut>
#include <QDesktopServices>
#include <QCheckBox>
#include <QtDebug>
#include <QXmlStreamReader>

void UrlOpener::open() { QDesktopServices::openUrl(m_url); }

UrlOpener dvHome(QUrl("http://www.toonz.com/"));
UrlOpener manual(QUrl("file:///C:/gmt/butta/M&C in EU.pdf"));

TEnv::IntVar LockRoomTabToggle("LockRoomTabToggle", 0);

//=============================================================================
// RoomTabWidget
//-----------------------------------------------------------------------------

RoomTabWidget::RoomTabWidget(QWidget *parent)
    : QTabBar(parent)
    , m_clickedTabIndex(-1)
    , m_tabToDeleteIndex(-1)
    , m_renameTabIndex(-1)
    , m_renameTextField(new DVGui::LineEdit(this))
    , m_isLocked(LockRoomTabToggle != 0) {
  m_renameTextField->hide();
  connect(m_renameTextField, SIGNAL(editingFinished()), this,
          SLOT(updateTabName()));
}

//-----------------------------------------------------------------------------

RoomTabWidget::~RoomTabWidget() {}

//-----------------------------------------------------------------------------

void RoomTabWidget::swapIndex(int firstIndex, int secondIndex) {
  QString firstText = tabText(firstIndex);
  removeTab(firstIndex);
  insertTab(secondIndex, firstText);
  emit indexSwapped(firstIndex, secondIndex);

  setCurrentIndex(secondIndex);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mousePressEvent(QMouseEvent *event) {
  m_renameTextField->hide();
  if (event->button() == Qt::LeftButton) {
    m_clickedTabIndex = tabAt(event->pos());
    if (m_clickedTabIndex < 0) return;
    setCurrentIndex(m_clickedTabIndex);
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  if (event->buttons()) {
    int tabIndex = tabAt(event->pos());
    if (tabIndex == m_clickedTabIndex || tabIndex < 0 || tabIndex >= count() ||
        m_clickedTabIndex < 0)
      return;
    swapIndex(m_clickedTabIndex, tabIndex);
    m_clickedTabIndex = tabIndex;
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseReleaseEvent(QMouseEvent *event) {
  m_clickedTabIndex = -1;
}

//-----------------------------------------------------------------------------
/*! Set a text field with focus in event position to edit tab name.
 */
void RoomTabWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  int index = tabAt(event->pos());
  if (index < 0) return;
  m_renameTabIndex     = index;
  DVGui::LineEdit *fld = m_renameTextField;
  fld->setText(tabText(index));
  fld->setGeometry(tabRect(index));
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (m_isLocked) return;
  m_tabToDeleteIndex = -1;
  QMenu *menu        = new QMenu(this);
  QAction *newRoom   = menu->addAction(tr("New Room"));
  connect(newRoom, SIGNAL(triggered()), SLOT(addNewTab()));

  int index = tabAt(event->pos());
  if (index >= 0) {
    m_tabToDeleteIndex = index;
    if (index != currentIndex()) {
      QAction *deleteRoom =
          menu->addAction(tr("Delete Room \"%1\"").arg(tabText(index)));
      connect(deleteRoom, SIGNAL(triggered()), SLOT(deleteTab()));
    }
#if defined(_WIN32) || defined(_CYGWIN_)
    /*- customize menubar -*/
    QAction *customizeMenuBar = menu->addAction(
        tr("Customize Menu Bar of Room \"%1\"").arg(tabText(index)));
    connect(customizeMenuBar, SIGNAL(triggered()), SLOT(onCustomizeMenuBar()));
#endif
  }
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void RoomTabWidget::updateTabName() {
  int index = m_renameTabIndex;
  if (index < 0) return;
  m_renameTabIndex = -1;
  QString newName  = m_renameTextField->text();
  setTabText(index, newName);
  m_renameTextField->hide();
  emit renameTabRoom(index, newName);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::addNewTab() {
  insertTab(0, tr("Room"));
  emit insertNewTabRoom();
}

//-----------------------------------------------------------------------------

void RoomTabWidget::deleteTab() {
  assert(m_tabToDeleteIndex != -1);

  QString question(tr("Are you sure you want to remove room %1")
                       .arg(tabText(m_tabToDeleteIndex)));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  emit deleteTabRoom(m_tabToDeleteIndex);
  removeTab(m_tabToDeleteIndex);
  m_tabToDeleteIndex = -1;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::setIsLocked(bool lock) {
  m_isLocked        = lock;
  LockRoomTabToggle = (lock) ? 1 : 0;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::onCustomizeMenuBar() {
  /*- use m_tabToDeleteIndex for index of a room of which menubar is to be
   * customized -*/
  assert(m_tabToDeleteIndex != -1);

  emit customizeMenuBar(m_tabToDeleteIndex);
}

//=============================================================================
// StackedMenuBar
//-----------------------------------------------------------------------------

StackedMenuBar::StackedMenuBar(QWidget *parent) : QStackedWidget(parent) {
  setObjectName("StackedMenuBar");
}

//---------------------------------------------------------------------------------

void StackedMenuBar::createMenuBarByName(const QString &roomName) {
  std::cout << "create " << roomName.toStdString() << std::endl;
#if defined(_WIN32) || defined(_CYGWIN_)
  if (roomName == "Cleanup")
    addWidget(createCleanupMenuBar());
  else if (roomName == "PltEdit")
    addWidget(createPltEditMenuBar());
  else if (roomName == "InknPaint")
    addWidget(createInknPaintMenuBar());
  else if (roomName == "Xsheet" || roomName == "Schematic" ||
           roomName == "QAR" || roomName == "Flip")
    addWidget(createXsheetMenuBar());
  else if (roomName == "Batches")
    addWidget(createBatchesMenuBar());
  else if (roomName == "Browser")
    addWidget(createBrowserMenuBar());
  else /*-- どれにもあてはまらない場合は全てのコマンドの入ったメニューバーを作る
          --*/
    addWidget(createFullMenuBar());
#else
  /* OSX では stacked menu が動いていないのでとりあえず full のみ作成する */
  addWidget(createFullMenuBar());
#endif
}

//---------------------------------------------------------------------------------

void StackedMenuBar::loadAndAddMenubar(const TFilePath &fp) {
#if defined(_WIN32) || defined(_CYGWIN_)
  QMenuBar *menuBar = loadMenuBar(fp);
  if (menuBar)
    addWidget(menuBar);
  else
    addWidget(createFullMenuBar());
#else
  /* OSX では stacked menu が動いていないのでとりあえず full のみ作成する */
  addWidget(createFullMenuBar());
#endif
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::loadMenuBar(const TFilePath &fp) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return 0;
  }

  QXmlStreamReader reader(&file);

  QMenuBar *menuBar = new QMenuBar(this);
  if (reader.readNextStartElement()) {
    if (reader.name() == "menubar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "menu") {
          QString title = reader.attributes().value("title").toString();
          /*- Menu title will be translated if the title is registered in
           * translation file -*/
          QMenu *menu = new QMenu(tr(title.toStdString().c_str()));

          if (readMenuRecursive(reader, menu))
            menuBar->addMenu(menu);
          else {
            reader.raiseError(tr("Failed to load menu %1").arg(title));
            delete menu;
          }

        } else if (reader.name() == "command") {
          QString cmdName = reader.readElementText();

          QAction *action = CommandManager::instance()->getAction(
              cmdName.toStdString().c_str());
          if (action)
            menuBar->addAction(action);
          else
            reader.raiseError(tr("Failed to add command %1").arg(cmdName));
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  }

  if (reader.hasError()) {
    delete menuBar;
    return 0;
  }
  return menuBar;
}

//---------------------------------------------------------------------------------

bool StackedMenuBar::readMenuRecursive(QXmlStreamReader &reader, QMenu *menu) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "menu") {
      QString title  = reader.attributes().value("title").toString();
      QMenu *subMenu = new QMenu(tr(title.toStdString().c_str()));

      if (readMenuRecursive(reader, subMenu))
        menu->addMenu(subMenu);
      else {
        reader.raiseError(tr("Failed to load menu %1").arg(title));
        delete subMenu;
        return false;
      }

    } else if (reader.name() == "command") {
      QString cmdName = reader.readElementText();
      addMenuItem(menu, cmdName.toStdString().c_str());
    } else if (reader.name() == "command_debug") {
#ifndef NDEBUG
      QString cmdName = reader.readElementText();
      addMenuItem(menu, cmdName.toStdString().c_str());
#else
      reader.skipCurrentElement();
#endif
    } else if (reader.name() == "separator") {
      menu->addSeparator();
      reader.skipCurrentElement();
    } else
      reader.skipCurrentElement();
  }

  return !reader.hasError();
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createCleanupMenuBar() {
  QMenuBar *cleanupMenuBar = new QMenuBar(this);
  //----Files Menu
  QMenu *filesMenu = addMenu(tr("Files"), cleanupMenuBar);
  addMenuItem(filesMenu, MI_LoadLevel);
  addMenuItem(filesMenu, MI_LoadFolder);
  addMenuItem(filesMenu, MI_SaveLevel);
  addMenuItem(filesMenu, MI_SaveLevelAs);
  addMenuItem(filesMenu, MI_ExportLevel);
  addMenuItem(filesMenu, MI_OpenRecentLevel);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_LoadScene);
  addMenuItem(filesMenu, MI_SaveScene);
  addMenuItem(filesMenu, MI_SaveSceneAs);
  addMenuItem(filesMenu, MI_SaveAll);
  addMenuItem(filesMenu, MI_OpenRecentScene);
  addMenuItem(filesMenu, MI_RevertScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_ConvertFileWithInput);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_NewScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_Quit);

  //----Scan Menu
  QMenu *scanMenu = addMenu(tr("Scan"), cleanupMenuBar);
  addMenuItem(scanMenu, MI_DefineScanner);
  addMenuItem(scanMenu, MI_ScanSettings);
  addMenuItem(scanMenu, MI_Scan);
  addMenuItem(scanMenu, MI_SetScanCropbox);
  addMenuItem(scanMenu, MI_ResetScanCropbox);

  //----Settings Menu
  QMenu *settingsMenu = addMenu(tr("Settings"), cleanupMenuBar);
  addMenuItem(settingsMenu, MI_CleanupSettings);
  settingsMenu->addSeparator();
  addMenuItem(settingsMenu, MI_OutputSettings);
  addMenuItem(settingsMenu, MI_LevelSettings);

  //----Processing Menu
  QMenu *processingMenu = addMenu(tr("Processing"), cleanupMenuBar);
  addMenuItem(processingMenu, MI_CameraTest);
  addMenuItem(processingMenu, MI_OpacityCheck);
  addMenuItem(processingMenu, MI_CleanupPreview);
  addMenuItem(processingMenu, MI_Cleanup);

  //---Edit Menu
  QMenu *editMenu = addMenu(tr("Edit"), cleanupMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_NextFrame);
  addMenuItem(editMenu, MI_PrevFrame);
  addMenuItem(editMenu, MI_FirstFrame);
  addMenuItem(editMenu, MI_LastFrame);
  addMenuItem(editMenu, MI_TestAnimation);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  addMenuItem(editMenu, MI_PasteInto);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Clear);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);

  //----Windows Menu
  QMenu *windowsMenu = addMenu(tr("Windows"), cleanupMenuBar);
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenStyleControl);
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  windowsMenu->addSeparator();
  QMenu *otherWindowsMenu = windowsMenu->addMenu(tr("Other Windows"));
  {
    addMenuItem(otherWindowsMenu, MI_OpenBatchServers);
    addMenuItem(otherWindowsMenu, MI_OpenTasks);
    addMenuItem(otherWindowsMenu, MI_OpenColorModel);
    addMenuItem(otherWindowsMenu, MI_OpenFileViewer);
    addMenuItem(otherWindowsMenu, MI_OpenFunctionEditor);
    addMenuItem(otherWindowsMenu, MI_OpenFilmStrip);
    addMenuItem(otherWindowsMenu, MI_OpenPalette);
    addMenuItem(otherWindowsMenu, MI_OpenFileBrowser2);
    addMenuItem(otherWindowsMenu, MI_OpenSchematic);
    addMenuItem(otherWindowsMenu, MI_OpenStudioPalette);
    addMenuItem(otherWindowsMenu, MI_OpenToolbar);
    addMenuItem(otherWindowsMenu, MI_OpenToolOptionBar);
    addMenuItem(otherWindowsMenu, MI_OpenHistoryPanel);
    addMenuItem(otherWindowsMenu, MI_OpenTMessage);
  }

  //----Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), cleanupMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  addMenuItem(customizeMenu, MI_SceneSettings);
  customizeMenu->addSeparator();
  QMenu *viewPartsMenu = customizeMenu->addMenu(tr("View"));
  {
    addMenuItem(viewPartsMenu, MI_ViewCamera);
    addMenuItem(viewPartsMenu, MI_ViewTable);
    addMenuItem(viewPartsMenu, MI_FieldGuide);
    addMenuItem(viewPartsMenu, MI_SafeArea);
    addMenuItem(viewPartsMenu, MI_ViewBBox);
    addMenuItem(viewPartsMenu, MI_ViewColorcard);
  }
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  //----Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), cleanupMenuBar);
  addMenuItem(helpMenu, MI_About);

  return cleanupMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createPltEditMenuBar() {
  QMenuBar *pltEditMenuBar = new QMenuBar(this);

  //---Files Menu
  QMenu *filesMenu = addMenu(tr("Files"), pltEditMenuBar);
  addMenuItem(filesMenu, MI_LoadLevel);
  addMenuItem(filesMenu, MI_LoadFolder);
  addMenuItem(filesMenu, MI_SaveLevel);
  addMenuItem(filesMenu, MI_SaveLevelAs);
  addMenuItem(filesMenu, MI_ExportLevel);
  addMenuItem(filesMenu, MI_OpenRecentLevel);
  addMenuItem(filesMenu, MI_LevelSettings);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_NewLevel);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_OverwritePalette);
  addMenuItem(filesMenu, MI_SavePaletteAs);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_LoadScene);
  addMenuItem(filesMenu, MI_SaveScene);
  addMenuItem(filesMenu, MI_SaveSceneAs);
  addMenuItem(filesMenu, MI_SaveAll);
  addMenuItem(filesMenu, MI_OpenRecentScene);
  addMenuItem(filesMenu, MI_RevertScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_ConvertFileWithInput);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_NewScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_Quit);

  //---Tools Menu
  QMenu *toolsMenu = addMenu(tr("Tools"), pltEditMenuBar);
  addMenuItem(toolsMenu, T_StylePicker);
  addMenuItem(toolsMenu, T_RGBPicker);
  addMenuItem(toolsMenu, T_Tape);
  toolsMenu->addSeparator();
  addMenuItem(toolsMenu, T_Fill);
  addMenuItem(toolsMenu, T_Brush);
  addMenuItem(toolsMenu, T_PaintBrush);
  addMenuItem(toolsMenu, T_Geometric);
  addMenuItem(toolsMenu, T_Type);
  toolsMenu->addSeparator();
  addMenuItem(toolsMenu, T_Eraser);
  QMenu *moreToolsMenu = toolsMenu->addMenu(tr("More Tools"));
  {
    addMenuItem(moreToolsMenu, T_Edit);
    addMenuItem(moreToolsMenu, T_Selection);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_ControlPointEditor);
    addMenuItem(moreToolsMenu, T_Pinch);
    addMenuItem(moreToolsMenu, T_Pump);
    addMenuItem(moreToolsMenu, T_Magnet);
    addMenuItem(moreToolsMenu, T_Bender);
    addMenuItem(moreToolsMenu, T_Iron);
    addMenuItem(moreToolsMenu, T_Cutter);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_Skeleton);
    addMenuItem(moreToolsMenu, T_Tracker);
    addMenuItem(moreToolsMenu, T_Hook);
    addMenuItem(moreToolsMenu, T_Plastic);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_Zoom);
    addMenuItem(moreToolsMenu, T_Rotate);
    addMenuItem(moreToolsMenu, T_Hand);
  }

  //---Edit Menu
  QMenu *editMenu = addMenu(tr("Edit"), pltEditMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_NextFrame);
  addMenuItem(editMenu, MI_PrevFrame);
  addMenuItem(editMenu, MI_FirstFrame);
  addMenuItem(editMenu, MI_LastFrame);
  addMenuItem(editMenu, MI_TestAnimation);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  addMenuItem(editMenu, MI_PasteInto);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Clear);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);

  //---Checks Menu
  QMenu *checksMenu = addMenu(tr("Checks"), pltEditMenuBar);
  addMenuItem(checksMenu, MI_TCheck);
  addMenuItem(checksMenu, MI_BCheck);
  addMenuItem(checksMenu, MI_ICheck);
  addMenuItem(checksMenu, MI_Ink1Check);
  addMenuItem(checksMenu, MI_PCheck);

  //---Render Menu
  QMenu *renderMenu = addMenu(tr("Render"), pltEditMenuBar);
  addMenuItem(renderMenu, MI_PreviewSettings);
  addMenuItem(renderMenu, MI_Preview);
  addMenuItem(renderMenu, MI_SavePreviewedFrames);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_OutputSettings);
  addMenuItem(renderMenu, MI_Render);

  //---Windows Menu
  QMenu *windowsMenu = addMenu(tr("Windows"), pltEditMenuBar);
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenFileViewer);
  addMenuItem(windowsMenu, MI_OpenFilmStrip);
  addMenuItem(windowsMenu, MI_OpenPalette);
  addMenuItem(windowsMenu, MI_OpenStudioPalette);
  addMenuItem(windowsMenu, MI_OpenStyleControl);
  addMenuItem(windowsMenu, MI_OpenLevelView);
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  windowsMenu->addSeparator();
  QMenu *otherWindowsMenu = windowsMenu->addMenu(tr("Other Windows"));
  {
    addMenuItem(otherWindowsMenu, MI_OpenBatchServers);
    addMenuItem(otherWindowsMenu, MI_OpenColorModel);
    addMenuItem(otherWindowsMenu, MI_OpenFunctionEditor);
    addMenuItem(otherWindowsMenu, MI_OpenFileBrowser2);
    addMenuItem(otherWindowsMenu, MI_OpenSchematic);
    addMenuItem(otherWindowsMenu, MI_OpenTasks);
    addMenuItem(otherWindowsMenu, MI_OpenToolbar);
    addMenuItem(otherWindowsMenu, MI_OpenToolOptionBar);
    addMenuItem(otherWindowsMenu, MI_OpenHistoryPanel);
    addMenuItem(otherWindowsMenu, MI_OpenTMessage);
  }

  //---Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), pltEditMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  addMenuItem(customizeMenu, MI_SceneSettings);
  customizeMenu->addSeparator();
  QMenu *viewPartsMenu = customizeMenu->addMenu(tr("View"));
  {
    addMenuItem(viewPartsMenu, MI_ViewCamera);
    addMenuItem(viewPartsMenu, MI_ViewTable);
    addMenuItem(viewPartsMenu, MI_FieldGuide);
    addMenuItem(viewPartsMenu, MI_SafeArea);
    addMenuItem(viewPartsMenu, MI_ViewColorcard);
  }
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  //---Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), pltEditMenuBar);
  addMenuItem(helpMenu, MI_About);

  return pltEditMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createInknPaintMenuBar() {
  QMenuBar *inknPaintMenuBar = new QMenuBar(this);

  //---Files Menu
  QMenu *filesMenu = addMenu(tr("Files"), inknPaintMenuBar);
  addMenuItem(filesMenu, MI_LoadLevel);
  addMenuItem(filesMenu, MI_LoadFolder);
  addMenuItem(filesMenu, MI_SaveLevel);
  addMenuItem(filesMenu, MI_SaveLevelAs);
  addMenuItem(filesMenu, MI_OpenRecentLevel);
  addMenuItem(filesMenu, MI_ExportLevel);
  addMenuItem(filesMenu, MI_LevelSettings);
  addMenuItem(filesMenu, MI_CanvasSize);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_NewLevel);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_OverwritePalette);
  addMenuItem(filesMenu, MI_SavePaletteAs);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_LoadColorModel);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_ApplyMatchLines);
  addMenuItem(filesMenu, MI_DeleteMatchLines);
  addMenuItem(filesMenu, MI_DeleteInk);
  addMenuItem(filesMenu, MI_MergeCmapped);
  addMenuItem(filesMenu, MI_MergeColumns);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_LoadScene);
  addMenuItem(filesMenu, MI_SaveScene);
  addMenuItem(filesMenu, MI_SaveSceneAs);
  addMenuItem(filesMenu, MI_SaveAll);
  addMenuItem(filesMenu, MI_OpenRecentScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_NewScene);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_Quit);

  //---Tools Menu
  QMenu *toolsMenu = addMenu(tr("Tools"), inknPaintMenuBar);
  addMenuItem(toolsMenu, T_StylePicker);
  addMenuItem(toolsMenu, T_RGBPicker);
  addMenuItem(toolsMenu, T_Tape);
  toolsMenu->addSeparator();
  addMenuItem(toolsMenu, T_Fill);
  addMenuItem(toolsMenu, T_Brush);
  addMenuItem(toolsMenu, T_PaintBrush);
  addMenuItem(toolsMenu, T_Geometric);
  addMenuItem(toolsMenu, T_Type);
  toolsMenu->addSeparator();
  addMenuItem(toolsMenu, T_Eraser);
  toolsMenu->addSeparator();
  QMenu *moreToolsMenu = toolsMenu->addMenu(tr("More Tools"));
  {
    addMenuItem(moreToolsMenu, T_Edit);
    addMenuItem(moreToolsMenu, T_Selection);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_ControlPointEditor);
    addMenuItem(moreToolsMenu, T_Pinch);
    addMenuItem(moreToolsMenu, T_Pump);
    addMenuItem(moreToolsMenu, T_Magnet);
    addMenuItem(moreToolsMenu, T_Bender);
    addMenuItem(moreToolsMenu, T_Iron);
    addMenuItem(moreToolsMenu, T_Cutter);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_Skeleton);
    addMenuItem(moreToolsMenu, T_Tracker);
    addMenuItem(moreToolsMenu, T_Hook);
    addMenuItem(moreToolsMenu, T_Plastic);
    moreToolsMenu->addSeparator();
    addMenuItem(moreToolsMenu, T_Zoom);
    addMenuItem(moreToolsMenu, T_Rotate);
    addMenuItem(moreToolsMenu, T_Hand);
  }

  //---Draw Menu
  QMenu *drawMenu = addMenu(tr("Draw"), inknPaintMenuBar);
  addMenuItem(drawMenu, MI_ShiftTrace);
  addMenuItem(drawMenu, MI_EditShift);
  addMenuItem(drawMenu, MI_NoShift);
  addMenuItem(drawMenu, MI_ResetShift);
  drawMenu->addSeparator();
  addMenuItem(drawMenu, MI_RasterizePli);

  //---Edit Menu
  QMenu *editMenu = addMenu(tr("Edit"), inknPaintMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_NextFrame);
  addMenuItem(editMenu, MI_PrevFrame);
  addMenuItem(editMenu, MI_FirstFrame);
  addMenuItem(editMenu, MI_LastFrame);
  addMenuItem(editMenu, MI_TestAnimation);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  addMenuItem(editMenu, MI_PasteInto);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Clear);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);

  //---Checks Menu
  QMenu *checksMenu = addMenu(tr("Checks"), inknPaintMenuBar);
  addMenuItem(checksMenu, MI_TCheck);
  addMenuItem(checksMenu, MI_BCheck);
  addMenuItem(checksMenu, MI_ICheck);
  addMenuItem(checksMenu, MI_Ink1Check);
  addMenuItem(checksMenu, MI_PCheck);
  addMenuItem(checksMenu, MI_GCheck);
  addMenuItem(checksMenu, MI_ACheck);
  addMenuItem(checksMenu, MI_IOnly);

  //---Windows Menu
  QMenu *windowsMenu = addMenu(tr("Windows"), inknPaintMenuBar);
  addMenuItem(windowsMenu, MI_OpenStyleControl);
  addMenuItem(windowsMenu, MI_OpenPltGizmo);
  addMenuItem(windowsMenu, MI_OpenPalette);
  addMenuItem(windowsMenu, MI_OpenStudioPalette);
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  addMenuItem(windowsMenu, MI_OpenColorModel);
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenFilmStrip);
  addMenuItem(windowsMenu, MI_OpenToolbar);
  addMenuItem(windowsMenu, MI_OpenToolOptionBar);
  windowsMenu->addSeparator();
  QMenu *otherWindowsMenu = windowsMenu->addMenu(tr("Other Windows"));
  {
    addMenuItem(otherWindowsMenu, MI_OpenBatchServers);
    addMenuItem(otherWindowsMenu, MI_OpenFileViewer);
    addMenuItem(otherWindowsMenu, MI_OpenFunctionEditor);
    addMenuItem(otherWindowsMenu, MI_OpenFileBrowser2);
    addMenuItem(otherWindowsMenu, MI_OpenSchematic);
    addMenuItem(otherWindowsMenu, MI_OpenTasks);
    addMenuItem(otherWindowsMenu, MI_OpenHistoryPanel);
    addMenuItem(otherWindowsMenu, MI_OpenTMessage);
  }

  //---Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), inknPaintMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  addMenuItem(customizeMenu, MI_SceneSettings);
  customizeMenu->addSeparator();
  QMenu *viewPartsMenu = customizeMenu->addMenu(tr("View"));
  {
    addMenuItem(viewPartsMenu, MI_ViewCamera);
    addMenuItem(viewPartsMenu, MI_ViewTable);
    addMenuItem(viewPartsMenu, MI_FieldGuide);
    addMenuItem(viewPartsMenu, MI_SafeArea);
    addMenuItem(viewPartsMenu, MI_ViewColorcard);
  }
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  //---Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), inknPaintMenuBar);
  addMenuItem(helpMenu, MI_About);

  return inknPaintMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createXsheetMenuBar() {
  QMenuBar *xsheetMenuBar = new QMenuBar(this);
  //----Xsheet Menu
  QMenu *xsheetMenu = addMenu(tr("Xsheet"), xsheetMenuBar);
  addMenuItem(xsheetMenu, MI_LoadScene);
  addMenuItem(xsheetMenu, MI_SaveScene);
  addMenuItem(xsheetMenu, MI_SaveSceneAs);
  addMenuItem(xsheetMenu, MI_SaveAll);
  addMenuItem(xsheetMenu, MI_OpenRecentScene);
  addMenuItem(xsheetMenu, MI_RevertScene);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_NewScene);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_PrintXsheet);
  addMenuItem(xsheetMenu, MI_Export);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_Quit);

  //----Subxsheet Menu
  QMenu *subxsheetMenu = addMenu(tr("Subxsheet"), xsheetMenuBar);
  addMenuItem(subxsheetMenu, MI_OpenChild);
  addMenuItem(subxsheetMenu, MI_CloseChild);
  addMenuItem(subxsheetMenu, MI_Collapse);
  addMenuItem(subxsheetMenu, MI_ToggleEditInPlace);
  addMenuItem(subxsheetMenu, MI_Resequence);
  addMenuItem(subxsheetMenu, MI_SaveSubxsheetAs);
  addMenuItem(subxsheetMenu, MI_LoadSubSceneFile);
  addMenuItem(subxsheetMenu, MI_CloneChild);
  addMenuItem(subxsheetMenu, MI_ExplodeChild);

  //----Levels Menu
  QMenu *levelsMenu = addMenu(tr("Levels"), xsheetMenuBar);
  addMenuItem(levelsMenu, MI_LoadLevel);
  addMenuItem(levelsMenu, MI_LoadFolder);
  addMenuItem(levelsMenu, MI_SaveLevel);
  addMenuItem(levelsMenu, MI_SaveLevelAs);
  addMenuItem(levelsMenu, MI_ExportLevel);
  addMenuItem(levelsMenu, MI_OpenRecentLevel);
  addMenuItem(levelsMenu, MI_LevelSettings);
  addMenuItem(levelsMenu, MI_CanvasSize);
  levelsMenu->addSeparator();
  addMenuItem(levelsMenu, MI_NewLevel);
  levelsMenu->addSeparator();
  addMenuItem(levelsMenu, MI_ViewFile);
  addMenuItem(levelsMenu, MI_FileInfo);
  levelsMenu->addSeparator();
  addMenuItem(levelsMenu, MI_CloneLevel);
  addMenuItem(levelsMenu, MI_ReplaceLevel);
  levelsMenu->addSeparator();
  addMenuItem(levelsMenu, MI_AddFrames);
  addMenuItem(levelsMenu, MI_Renumber);
  addMenuItem(levelsMenu, MI_RevertToCleanedUp);
  addMenuItem(levelsMenu, MI_ConvertToVectors);
  addMenuItem(levelsMenu, MI_ConvertFileWithInput);
  addMenuItem(levelsMenu, MI_Tracking);
  addMenuItem(levelsMenu, MI_ExposeResource);
  addMenuItem(levelsMenu, MI_EditLevel);
  addMenuItem(levelsMenu, MI_RemoveUnused);

  //----Cells Menu
  QMenu *cellsMenu = addMenu(tr("Cells"), xsheetMenuBar);
  addMenuItem(cellsMenu, MI_Dup);
  addMenuItem(cellsMenu, MI_Reverse);
  addMenuItem(cellsMenu, MI_Rollup);
  addMenuItem(cellsMenu, MI_Rolldown);
  addMenuItem(cellsMenu, MI_Swing);
  addMenuItem(cellsMenu, MI_Random);
  addMenuItem(cellsMenu, MI_TimeStretch);
  cellsMenu->addSeparator();
  QMenu *reframeSubMenu = cellsMenu->addMenu(tr("Reframe"));
  {
    addMenuItem(reframeSubMenu, MI_Reframe1);
    addMenuItem(reframeSubMenu, MI_Reframe2);
    addMenuItem(reframeSubMenu, MI_Reframe3);
    addMenuItem(reframeSubMenu, MI_Reframe4);
  }
  QMenu *stepMenu = cellsMenu->addMenu(tr("Step"));
  {
    addMenuItem(stepMenu, MI_Step2);
    addMenuItem(stepMenu, MI_Step3);
    addMenuItem(stepMenu, MI_Step4);
  }
  QMenu *eachMenu = cellsMenu->addMenu(tr("Each"));
  {
    addMenuItem(eachMenu, MI_Each2);
    addMenuItem(eachMenu, MI_Each3);
    addMenuItem(eachMenu, MI_Each4);
  }

  //----Edit Menu
  QMenu *editMenu = addMenu(tr("Edit"), xsheetMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  addMenuItem(editMenu, MI_PasteInto);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Clear);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_InsertSceneFrame);
  addMenuItem(editMenu, MI_RemoveSceneFrame);
  addMenuItem(editMenu, MI_InsertGlobalKeyframe);
  addMenuItem(editMenu, MI_RemoveGlobalKeyframe);

  //---Render Menu
  QMenu *renderMenu = addMenu(tr("Render"), xsheetMenuBar);
  addMenuItem(renderMenu, MI_PreviewSettings);
  addMenuItem(renderMenu, MI_Preview);
  addMenuItem(renderMenu, MI_SavePreviewedFrames);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_OutputSettings);
  addMenuItem(renderMenu, MI_Render);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_Link);

  //----Windows Menu
  QMenu *windowsMenu = addMenu(tr("Windows"), xsheetMenuBar);
  addMenuItem(windowsMenu, MI_OpenSchematic);
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenFunctionEditor);
  addMenuItem(windowsMenu, MI_OpenFileViewer);
  addMenuItem(windowsMenu, MI_OpenFilmStrip);
  addMenuItem(windowsMenu, MI_OpenLevelView);
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  windowsMenu->addSeparator();
  QMenu *otherWindowsMenu = windowsMenu->addMenu(tr("Other Windows"));
  {
    addMenuItem(otherWindowsMenu, MI_OpenBatchServers);
    addMenuItem(otherWindowsMenu, MI_OpenColorModel);
    addMenuItem(otherWindowsMenu, MI_OpenPalette);
    addMenuItem(otherWindowsMenu, MI_OpenFileBrowser2);
    addMenuItem(otherWindowsMenu, MI_OpenStudioPalette);
    addMenuItem(otherWindowsMenu, MI_OpenStyleControl);
    addMenuItem(otherWindowsMenu, MI_OpenTasks);
    addMenuItem(otherWindowsMenu, MI_OpenToolbar);
    addMenuItem(otherWindowsMenu, MI_OpenToolOptionBar);
    addMenuItem(otherWindowsMenu, MI_OpenHistoryPanel);
    addMenuItem(otherWindowsMenu, MI_OpenTMessage);
  }

  //---Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), xsheetMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  addMenuItem(customizeMenu, MI_SceneSettings);
  customizeMenu->addSeparator();
  QMenu *viewPartsMenu = customizeMenu->addMenu(tr("View"));
  {
    addMenuItem(viewPartsMenu, MI_ViewCamera);
    addMenuItem(viewPartsMenu, MI_ViewTable);
    addMenuItem(viewPartsMenu, MI_FieldGuide);
    addMenuItem(viewPartsMenu, MI_SafeArea);
    addMenuItem(viewPartsMenu, MI_ViewBBox);
    addMenuItem(viewPartsMenu, MI_ViewColorcard);
  }
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  //---Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), xsheetMenuBar);
  addMenuItem(helpMenu, MI_About);

  return xsheetMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createBatchesMenuBar() {
  QMenuBar *batchesMenuBar = new QMenuBar(this);

  //---Files Menu
  QMenu *filesMenu = addMenu(tr("Files"), batchesMenuBar);
  addMenuItem(filesMenu, MI_Quit);

  //----Windows Menu
  QMenu *windowsMenu = addMenu(tr("Windows"), batchesMenuBar);
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenBatchServers);
  addMenuItem(windowsMenu, MI_OpenTasks);

  //---Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), batchesMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  //---Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), batchesMenuBar);
  addMenuItem(helpMenu, MI_About);

  return batchesMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createBrowserMenuBar() {
  QMenuBar *browserMenuBar = new QMenuBar(this);

  //---Files Menu
  QMenu *filesMenu = addMenu(tr("Files"), browserMenuBar);
  addMenuItem(filesMenu, MI_NewProject);
  addMenuItem(filesMenu, MI_ProjectSettings);
  addMenuItem(filesMenu, MI_SaveDefaultSettings);
  filesMenu->addSeparator();
  addMenuItem(filesMenu, MI_Quit);

  //---Customize Menu
  QMenu *customizeMenu = addMenu(tr("Customize"), browserMenuBar);
  addMenuItem(customizeMenu, MI_Preferences);
  addMenuItem(customizeMenu, MI_ShortcutPopup);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, MI_DockingCheck);
  customizeMenu->addSeparator();
  addMenuItem(customizeMenu, "MI_RunScript");
  addMenuItem(customizeMenu, "MI_OpenScriptConsole");
#ifndef NDEBUG
  addMenuItem(customizeMenu, "MI_ReloadStyle");
#endif

  // iwsw commented out temporarily
  // customizeMenu->addSeparator();
  // addMenuItem(customizeMenu, "MI_ToonShadedImageToTLVByFolder");

  //---Help Menu
  QMenu *helpMenu = addMenu(tr("Help"), browserMenuBar);
  addMenuItem(helpMenu, MI_About);

  return browserMenuBar;
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createFullMenuBar() {
  QMenuBar *fullMenuBar = new QMenuBar(this);
  // Menu' FILE
  QMenu *fileMenu = addMenu(tr("File"), fullMenuBar);
  addMenuItem(fileMenu, MI_NewScene);
  addMenuItem(fileMenu, MI_LoadScene);
  addMenuItem(fileMenu, MI_SaveAll);
  addMenuItem(fileMenu, MI_SaveScene);
  addMenuItem(fileMenu, MI_SaveSceneAs);
  addMenuItem(fileMenu, MI_OpenRecentScene);
  addMenuItem(fileMenu, MI_RevertScene);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_LoadFolder);
  addMenuItem(fileMenu, MI_LoadSubSceneFile);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_ConvertFileWithInput);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_LoadColorModel);
  fileMenu->addSeparator();
  QMenu *projectManagementMenu = fileMenu->addMenu(tr("Project Management"));
  {
    addMenuItem(projectManagementMenu, MI_NewProject);
    addMenuItem(projectManagementMenu, MI_ProjectSettings);
    projectManagementMenu->addSeparator();
    addMenuItem(projectManagementMenu, MI_SaveDefaultSettings);
  }
  fileMenu->addSeparator();
  QMenu *importMenu = fileMenu->addMenu(tr("Import"));
  { addMenuItem(importMenu, MI_ImportMagpieFile); }
  QMenu *exportMenu = fileMenu->addMenu(tr("Export"));
  {
    addMenuItem(exportMenu, MI_SoundTrack);
    addMenuItem(exportMenu, MI_ExportXDTS);
    addMenuItem(exportMenu, MI_ExportXsheetPDF);
#if defined(x64)
    addMenuItem(exportMenu, MI_StopMotionExportImageSequence);
#endif
    addMenuItem(exportMenu, MI_ExportTvpJson);
  }
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_PrintXsheet);
  addMenuItem(fileMenu, MI_Print);
  addMenuItem(fileMenu, MI_Export);
  fileMenu->addSeparator();
  QMenu *scriptMenu = fileMenu->addMenu(tr("Script"));
  {
    addMenuItem(scriptMenu, "MI_RunScript");
    addMenuItem(scriptMenu, "MI_OpenScriptConsole");
  }
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_Preferences);
  addMenuItem(fileMenu, MI_ShortcutPopup);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_ClearCacheFolder);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_Quit);

  // Menu' EDIT
  QMenu *editMenu = addMenu(tr("Edit"), fullMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  // addMenuItem(editMenu, MI_PasteNew);
  addMenuItem(editMenu, MI_PasteInto);
  addMenuItem(editMenu, MI_PasteDuplicate);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
  addMenuItem(editMenu, MI_Clear);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);
  editMenu->addSeparator();
  QMenu *groupMenu = editMenu->addMenu(tr("Group"));
  {
    addMenuItem(groupMenu, MI_Group);
    addMenuItem(groupMenu, MI_Ungroup);
    addMenuItem(groupMenu, MI_EnterGroup);
    addMenuItem(groupMenu, MI_ExitGroup);
  }
  editMenu->addSeparator();
  QMenu *arrangeMenu = editMenu->addMenu(tr("Arrange"));
  {
    addMenuItem(arrangeMenu, MI_BringToFront);
    addMenuItem(arrangeMenu, MI_BringForward);
    addMenuItem(arrangeMenu, MI_SendBackward);
    addMenuItem(arrangeMenu, MI_SendBack);
  }

  // Menu' SCAN CLEANUP
  QMenu *scanCleanupMenu = addMenu(tr("Scan && Cleanup"), fullMenuBar);
  addMenuItem(scanCleanupMenu, MI_DefineScanner);
  addMenuItem(scanCleanupMenu, MI_ScanSettings);
  addMenuItem(scanCleanupMenu, MI_Scan);
  addMenuItem(scanCleanupMenu, MI_SetScanCropbox);
  addMenuItem(scanCleanupMenu, MI_ResetScanCropbox);
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_CleanupSettings);
  addMenuItem(scanCleanupMenu, MI_CleanupPreview);
  addMenuItem(scanCleanupMenu, MI_CameraTest);
  addMenuItem(scanCleanupMenu, MI_Cleanup);
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_PencilTest);
#ifdef LINETEST
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_Autocenter);
#endif

  // Menu' LEVEL
  QMenu *levelMenu = addMenu(tr("Level"), fullMenuBar);
  QMenu *newMenu   = levelMenu->addMenu(tr("New"));
  {
    addMenuItem(newMenu, MI_NewLevel);
    newMenu->addSeparator();
    addMenuItem(newMenu, MI_NewToonzRasterLevel);
    addMenuItem(newMenu, MI_NewVectorLevel);
    addMenuItem(newMenu, MI_NewRasterLevel);
    addMenuItem(newMenu, MI_NewNoteLevel);
  }
  addMenuItem(levelMenu, MI_LoadLevel);
  addMenuItem(levelMenu, MI_SaveLevel);
  addMenuItem(levelMenu, MI_SaveLevelAs);
  addMenuItem(levelMenu, MI_SaveAllLevels);
  addMenuItem(levelMenu, MI_OpenRecentLevel);
  addMenuItem(levelMenu, MI_ExportLevel);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_AddFrames);
  addMenuItem(levelMenu, MI_Renumber);
  addMenuItem(levelMenu, MI_ReplaceLevel);
  addMenuItem(levelMenu, MI_RevertToCleanedUp);
  addMenuItem(levelMenu, MI_RevertToLastSaved);
  addMenuItem(levelMenu, MI_Tracking);
  levelMenu->addSeparator();
  QMenu *adjustMenu = levelMenu->addMenu(tr("Adjust"));
  {
    addMenuItem(adjustMenu, MI_BrightnessAndContrast);
    addMenuItem(adjustMenu, MI_AdjustLevels);
    addMenuItem(adjustMenu, MI_AdjustThickness);
    addMenuItem(adjustMenu, MI_Antialias);
    addMenuItem(adjustMenu, MI_Binarize);
    addMenuItem(adjustMenu, MI_LinesFade);
  }
  QMenu *optimizeMenu = levelMenu->addMenu(tr("Optimize"));
  {
    addMenuItem(optimizeMenu, MI_RemoveEndpoints);
    addMenuItem(optimizeMenu, MI_ConvertVectorToVector);
  }
  QMenu *convertMenu = levelMenu->addMenu(tr("Convert"));
  {
    addMenuItem(convertMenu, MI_ConvertToVectors);
    addMenuItem(convertMenu, MI_ConvertToToonzRaster);
  }
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_ExposeResource);
  addMenuItem(levelMenu, MI_EditLevel);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_CanvasSize);
  addMenuItem(levelMenu, MI_LevelSettings);
  addMenuItem(levelMenu, MI_FileInfo);
  addMenuItem(levelMenu, MI_ReplaceParentDirectory);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_RemoveUnused);

  // Menu' XSHEET
  QMenu *xsheetMenu = addMenu(tr("Xsheet"), fullMenuBar);
  addMenuItem(xsheetMenu, MI_SceneSettings);
  addMenuItem(xsheetMenu, MI_CameraSettings);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_OpenChild);
  addMenuItem(xsheetMenu, MI_CloseChild);
  addMenuItem(xsheetMenu, MI_SaveSubxsheetAs);
  addMenuItem(xsheetMenu, MI_Collapse);
  addMenuItem(xsheetMenu, MI_Resequence);
  addMenuItem(xsheetMenu, MI_CloneChild);
  addMenuItem(xsheetMenu, MI_ExplodeChild);
  addMenuItem(xsheetMenu, MI_ToggleEditInPlace);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_ApplyMatchLines);
  addMenuItem(xsheetMenu, MI_MergeCmapped);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_MergeColumns);
  addMenuItem(xsheetMenu, MI_DeleteMatchLines);
  addMenuItem(xsheetMenu, MI_DeleteInk);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_InsertFx);
  addMenuItem(xsheetMenu, MI_NewOutputFx);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_InsertSceneFrame);
  addMenuItem(xsheetMenu, MI_RemoveSceneFrame);
  addMenuItem(xsheetMenu, MI_InsertGlobalKeyframe);
  addMenuItem(xsheetMenu, MI_RemoveGlobalKeyframe);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_LipSyncPopup);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_RemoveEmptyColumns);

  // Menu' CELLS
  QMenu *cellsMenu = addMenu(tr("Cells"), fullMenuBar);
  addMenuItem(cellsMenu, MI_Reverse);
  addMenuItem(cellsMenu, MI_Swing);
  addMenuItem(cellsMenu, MI_Random);
  addMenuItem(cellsMenu, MI_Increment);
  addMenuItem(cellsMenu, MI_Dup);
  cellsMenu->addSeparator();
  QMenu *reframeMenu = cellsMenu->addMenu(tr("Reframe"));
  {
    addMenuItem(reframeMenu, MI_Reframe1);
    addMenuItem(reframeMenu, MI_Reframe2);
    addMenuItem(reframeMenu, MI_Reframe3);
    addMenuItem(reframeMenu, MI_Reframe4);
    addMenuItem(reframeMenu, MI_ReframeWithEmptyInbetweens);
  }
  QMenu *stepMenu = cellsMenu->addMenu(tr("Step"));
  {
    addMenuItem(stepMenu, MI_IncreaseStep);
    addMenuItem(stepMenu, MI_DecreaseStep);
    addMenuItem(stepMenu, MI_ResetStep);
    addMenuItem(stepMenu, MI_Step2);
    addMenuItem(stepMenu, MI_Step3);
    addMenuItem(stepMenu, MI_Step4);
  }
  QMenu *eachMenu = cellsMenu->addMenu(tr("Each"));
  {
    addMenuItem(eachMenu, MI_Each2);
    addMenuItem(eachMenu, MI_Each3);
    addMenuItem(eachMenu, MI_Each4);
  }
  addMenuItem(cellsMenu, MI_Rollup);
  addMenuItem(cellsMenu, MI_Rolldown);
  addMenuItem(cellsMenu, MI_TimeStretch);
  addMenuItem(cellsMenu, MI_AutoInputCellNumber);
  cellsMenu->addSeparator();
  QMenu *drawingSubMenu = cellsMenu->addMenu(tr("Drawing Substitution"));
  {
    addMenuItem(drawingSubMenu, MI_DrawingSubForward);
    addMenuItem(drawingSubMenu, MI_DrawingSubBackward);
    addMenuItem(drawingSubMenu, MI_DrawingSubGroupForward);
    addMenuItem(drawingSubMenu, MI_DrawingSubGroupBackward);
  }
  cellsMenu->addSeparator();
  addMenuItem(cellsMenu, MI_Autorenumber);
  addMenuItem(cellsMenu, MI_CreateBlankDrawing);
  addMenuItem(cellsMenu, MI_Duplicate);
  addMenuItem(cellsMenu, MI_MergeFrames);
  addMenuItem(cellsMenu, MI_CloneLevel);
  cellsMenu->addSeparator();
  addMenuItem(cellsMenu, MI_FillEmptyCell);

  // Menu' PLAY
  QMenu *playMenu = addMenu(tr("Play"), fullMenuBar);
  addMenuItem(playMenu, MI_Play);
  addMenuItem(playMenu, MI_Pause);
  addMenuItem(playMenu, MI_Loop);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_FirstFrame);
  addMenuItem(playMenu, MI_LastFrame);
  addMenuItem(playMenu, MI_PrevFrame);
  addMenuItem(playMenu, MI_NextFrame);
  addMenuItem(playMenu, MI_PrevStep);
  addMenuItem(playMenu, MI_NextStep);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_PrevDrawing);
  addMenuItem(playMenu, MI_NextDrawing);
  addMenuItem(playMenu, MI_PrevKeyframe);
  addMenuItem(playMenu, MI_NextKeyframe);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_Link);

  // Menu' RENDER
  QMenu *renderMenu = addMenu(tr("Render"), fullMenuBar);
  addMenuItem(renderMenu, MI_PreviewSettings);
  addMenuItem(renderMenu, MI_Preview);
  // addMenuItem(renderMenu, MI_SavePreview);
  addMenuItem(renderMenu, MI_SavePreviewedFrames);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_OutputSettings);
  addMenuItem(renderMenu, MI_Render);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_FastRender);

  // Menu' VIEW
  QMenu *viewMenu = addMenu(tr("View"), fullMenuBar);
  addMenuItem(viewMenu, MI_ViewTable);
  addMenuItem(viewMenu, MI_ViewCamera);
  addMenuItem(viewMenu, MI_ViewColorcard);
  addMenuItem(viewMenu, MI_ViewBBox);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_SafeArea);
  addMenuItem(viewMenu, MI_FieldGuide);
  addMenuItem(viewMenu, MI_ViewRuler);
  addMenuItem(viewMenu, MI_ViewGuide);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_TCheck);
  addMenuItem(viewMenu, MI_ICheck);
  addMenuItem(viewMenu, MI_Ink1Check);
  addMenuItem(viewMenu, MI_PCheck);
  addMenuItem(viewMenu, MI_IOnly);
  addMenuItem(viewMenu, MI_BCheck);
  addMenuItem(viewMenu, MI_GCheck);
  addMenuItem(viewMenu, MI_ACheck);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_ShiftTrace);
  addMenuItem(viewMenu, MI_EditShift);
  addMenuItem(viewMenu, MI_NoShift);
  addMenuItem(viewMenu, MI_ResetShift);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_VectorGuidedDrawing);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_RasterizePli);

  // Menu' WINDOWS
  QMenu *windowsMenu   = addMenu(tr("Windows"), fullMenuBar);
  QMenu *workspaceMenu = windowsMenu->addMenu(tr("Workspace"));
  {
    addMenuItem(workspaceMenu, MI_DockingCheck);
    workspaceMenu->addSeparator();
    addMenuItem(workspaceMenu, MI_ResetRoomLayout);
  }
  addMenuItem(windowsMenu, MI_OpenCommandToolbar);
  addMenuItem(windowsMenu, MI_OpenToolbar);
  addMenuItem(windowsMenu, MI_OpenToolOptionBar);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenStyleControl);
  addMenuItem(windowsMenu, MI_OpenPalette);
  addMenuItem(windowsMenu, MI_OpenStudioPalette);
  addMenuItem(windowsMenu, MI_OpenColorModel);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenLevelView);
  addMenuItem(windowsMenu, MI_OpenFileViewer);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  addMenuItem(windowsMenu, MI_OpenFunctionEditor);
  addMenuItem(windowsMenu, MI_OpenSchematic);
  addMenuItem(windowsMenu, MI_FxParamEditor);
  addMenuItem(windowsMenu, MI_OpenFilmStrip);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenFileBrowser2);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenCleanupSettings);
  addMenuItem(windowsMenu, MI_OpenTasks);
  addMenuItem(windowsMenu, MI_OpenBatchServers);
  addMenuItem(windowsMenu, MI_OpenTMessage);
  addMenuItem(windowsMenu, MI_OpenHistoryPanel);
  addMenuItem(windowsMenu, MI_AudioRecording);
#if defined(x64)
  addMenuItem(windowsMenu, MI_OpenStopMotionPanel);
#endif
  addMenuItem(windowsMenu, MI_StartupPopup);
  addMenuItem(windowsMenu, MI_OpenGuidedDrawingControls);
  // windowsMenu->addSeparator();
  // addMenuItem(windowsMenu, MI_OpenExport);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_MaximizePanel);
  addMenuItem(windowsMenu, MI_FullScreenWindow);

  // Menu' HELP
  QMenu *helpMenu = addMenu(tr("Help"), fullMenuBar);
  addMenuItem(helpMenu, MI_OpenOnlineManual);
  addMenuItem(helpMenu, MI_OpenWhatsNew);
  addMenuItem(helpMenu, MI_OpenCommunityForum);
  helpMenu->addSeparator();
  addMenuItem(helpMenu, MI_OpenReportABug);
  helpMenu->addSeparator();
  addMenuItem(helpMenu, MI_About);

// addMenuItem(fileMenu, MI_TestAnimation);
// fileMenu->addSeparator();
#ifndef NDEBUG
  addMenuItem(fileMenu, "MI_ReloadStyle");
#endif

  return fullMenuBar;
}

//-----------------------------------------------------------------------------

QMenu *StackedMenuBar::addMenu(const QString &menuName, QMenuBar *menuBar) {
  QMenu *menu = new QMenu(menuName, menuBar);
  menuBar->addMenu(menu);
  return menu;
}

//-----------------------------------------------------------------------------

void StackedMenuBar::addMenuItem(QMenu *menu, const char *cmdId) {
  QAction *action = CommandManager::instance()->getAction(cmdId);
  if (!action) return;
  assert(action);  // check MainWindow::defineActions() if assert fails
  menu->addAction(action);
}

//-----------------------------------------------------------------------------

void StackedMenuBar::onIndexSwapped(int firstIndex, int secondIndex) {
  assert(firstIndex >= 0 && secondIndex >= 0);
  QWidget *menuBar = widget(firstIndex);
  removeWidget(menuBar);
  insertWidget(secondIndex, menuBar);
}

//-----------------------------------------------------------------------------

void StackedMenuBar::insertNewMenuBar() {
  insertWidget(0, createFullMenuBar());
}

//-----------------------------------------------------------------------------

void StackedMenuBar::deleteMenuBar(int index) {
  QWidget *menuBar = widget(index);
  removeWidget(menuBar);
  delete menuBar;
}

//-----------------------------------------------------------------------------

void StackedMenuBar::doCustomizeMenuBar(int index) {
  MainWindow *mainWin =
      dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
  assert(mainWin);
  Room *room = mainWin->getRoom(index);
  if (!room) return;

  MenuBarPopup mbPopup(room);

  if (mbPopup.exec()) {
    /*- OKが押され、roomname_menubar.xmlが更新された状態 -*/
    /*- xmlファイルからメニューバーを作り直して格納 -*/
    std::string mbFileName = room->getPath().getName() + "_menubar.xml";
    TFilePath mbPath       = ToonzFolder::getMyRoomsDir() + mbFileName;
    if (!TFileStatus(mbPath).isReadable()) {
      DVGui::warning(tr("Cannot open menubar settings file %1")
                         .arg(QString::fromStdString(mbFileName)));
      return;
    }
    QMenuBar *newMenu = loadMenuBar(mbPath);
    if (!newMenu) {
      DVGui::warning(tr("Failed to create menubar"));
      return;
    }

    QWidget *oldMenu = widget(index);
    removeWidget(oldMenu);
    insertWidget(index, newMenu);
    delete oldMenu;

    setCurrentIndex(index);
  }
}

//=============================================================================
// DvTopBar
//-----------------------------------------------------------------------------

TopBar::TopBar(QWidget *parent) : QToolBar(parent) {
  setAllowedAreas(Qt::TopToolBarArea);
  setMovable(false);
  setFloatable(false);
  setObjectName("TopBar");

  m_containerFrame = new QFrame(this);
  m_roomTabBar     = new RoomTabWidget(this);
  m_stackedMenuBar = new StackedMenuBar(this);
  m_lockRoomCB     = new QCheckBox(this);

  m_containerFrame->setObjectName("TopBarTabContainer");
  m_roomTabBar->setObjectName("TopBarTab");
  m_roomTabBar->setDrawBase(false);
  m_lockRoomCB->setObjectName("EditToolLockButton");
  m_lockRoomCB->setToolTip(tr("Lock Rooms Tab"));
  m_lockRoomCB->setChecked(m_roomTabBar->isLocked());

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setSpacing(0);
  mainLayout->setMargin(0);
  {
    QVBoxLayout *menuLayout = new QVBoxLayout();
    menuLayout->setSpacing(0);
    menuLayout->setMargin(0);
    {
      menuLayout->addStretch(1);
      menuLayout->addWidget(m_stackedMenuBar, 0);
      menuLayout->addStretch(1);
    }
    mainLayout->addLayout(menuLayout);
    mainLayout->addStretch(1);
    mainLayout->addWidget(m_roomTabBar, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(m_lockRoomCB, 0);
  }
  m_containerFrame->setLayout(mainLayout);
  addWidget(m_containerFrame);

  bool ret = true;
  ret      = ret && connect(m_roomTabBar, SIGNAL(currentChanged(int)),
                       m_stackedMenuBar, SLOT(setCurrentIndex(int)));

  ret = ret && connect(m_roomTabBar, SIGNAL(indexSwapped(int, int)),
                       m_stackedMenuBar, SLOT(onIndexSwapped(int, int)));
  ret = ret && connect(m_roomTabBar, SIGNAL(insertNewTabRoom()),
                       m_stackedMenuBar, SLOT(insertNewMenuBar()));
  ret = ret && connect(m_roomTabBar, SIGNAL(deleteTabRoom(int)),
                       m_stackedMenuBar, SLOT(deleteMenuBar(int)));
  ret = ret && connect(m_roomTabBar, SIGNAL(customizeMenuBar(int)),
                       m_stackedMenuBar, SLOT(doCustomizeMenuBar(int)));
  ret = ret && connect(m_lockRoomCB, SIGNAL(toggled(bool)), m_roomTabBar,
                       SLOT(setIsLocked(bool)));
  assert(ret);
}
