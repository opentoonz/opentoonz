

#include "mainwindow.h"

// Tnz6 includes
#include "menubar.h"
#include "menubarcommandids.h"
#include "xsheetviewer.h"
#include "viewerpane.h"
#include "flipbook.h"
#include "messagepanel.h"
#include "iocommand.h"
#include "tapp.h"
#include "comboviewerpane.h"
#include "startuppopup.h"
#include "tooloptionsshortcutinvoker.h"
#include "custompanelmanager.h"

// TnzTools includes
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/updatechecker.h"
#include "toonzqt/paletteviewer.h"
#include "toonzqt/seethroughwindow.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/stage2.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tproject.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "timagecache.h"
#include "tthread.h"

// Qt includes
#include <QStackedWidget>
#include <QSettings>
#include <QApplication>
#include <QGLPixelBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#ifdef _WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

TEnv::IntVar ViewCameraToggleAction("ViewCameraToggleAction", 1);
TEnv::IntVar ViewTableToggleAction("ViewTableToggleAction", 1);
TEnv::IntVar FieldGuideToggleAction("FieldGuideToggleAction", 0);
TEnv::IntVar ViewBBoxToggleAction("ViewBBoxToggleAction1", 1);
TEnv::IntVar EditInPlaceToggleAction("EditInPlaceToggleAction", 0);
TEnv::IntVar RasterizePliToggleAction("RasterizePliToggleAction", 0);
TEnv::IntVar SafeAreaToggleAction("SafeAreaToggleAction", 0);
TEnv::IntVar ViewColorcardToggleAction("ViewColorcardToggleAction", 1);
TEnv::IntVar ViewGuideToggleAction("ViewGuideToggleAction", 1);
TEnv::IntVar ViewRulerToggleAction("ViewRulerToggleAction", 1);
TEnv::IntVar TCheckToggleAction("TCheckToggleAction", 0);
TEnv::IntVar ICheckToggleAction("ICheckToggleAction", 0);
TEnv::IntVar Ink1CheckToggleAction("Ink1CheckToggleAction", 0);
TEnv::IntVar PCheckToggleAction("PCheckToggleAction", 0);
TEnv::IntVar IOnlyToggleAction("IOnlyToggleAction", 0);
TEnv::IntVar BCheckToggleAction("BCheckToggleAction", 0);
TEnv::IntVar GCheckToggleAction("GCheckToggleAction", 0);
TEnv::IntVar ACheckToggleAction("ACheckToggleAction", 0);
TEnv::IntVar LinkToggleAction("LinkToggleAction", 0);
TEnv::IntVar DockingCheckToggleAction("DockingCheckToggleAction", 0);
TEnv::IntVar ShiftTraceToggleAction("ShiftTraceToggleAction", 0);
TEnv::IntVar EditShiftToggleAction("EditShiftToggleAction", 0);
TEnv::IntVar NoShiftToggleAction("NoShiftToggleAction", 0);
TEnv::IntVar TouchGestureControl("TouchGestureControl", 0);

//=============================================================================
namespace {
//=============================================================================

// layout file name may be overwritten by the argument
std::string layoutsFileName           = "layouts.txt";
const std::string currentRoomFileName = "currentRoom.txt";
bool scrambledRooms                   = false;

//=============================================================================

bool readRoomList(std::vector<TFilePath> &roomPaths,
                  const QString &argumentLayoutFileName) {
  bool argumentLayoutFileLoaded = false;

  TFilePath fp;
  /*-レイアウトファイルが指定されている場合--*/
  if (!argumentLayoutFileName.isEmpty()) {
    fp = ToonzFolder::getRoomsFile(argumentLayoutFileName.toStdString());
    if (!TFileStatus(fp).doesExist()) {
      DVGui::warning("Room layout file " + argumentLayoutFileName +
                     " not found!");
      fp = ToonzFolder::getRoomsFile(layoutsFileName);
      if (!TFileStatus(fp).doesExist()) return false;
    } else {
      argumentLayoutFileLoaded = true;
      layoutsFileName          = argumentLayoutFileName.toStdString();
    }
  } else {
    fp = ToonzFolder::getRoomsFile(layoutsFileName);
    if (!TFileStatus(fp).doesExist()) return false;
  }

  Tifstream is(fp);
  for (;;) {
    char buffer[1024];
    is.getline(buffer, sizeof(buffer));
    if (is.eof()) break;
    char *s = buffer;
    while (*s == ' ' || *s == '\t') s++;
    char *t = s;
    while (*t && *t != '\r' && *t != '\n') t++;
    while (t > s && (t[-1] == ' ' || t[-1] == '\t')) t--;
    t[0] = '\0';
    if (s[0] == '\0') continue;
    TFilePath roomPath = fp.getParentDir() + s;
    roomPaths.push_back(roomPath);
  }

  return argumentLayoutFileLoaded;
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<TFilePath> &roomPaths) {
  TFilePath fp = ToonzFolder::getMyRoomsDir() + layoutsFileName;
  TSystem::touchParentDir(fp);
  Tofstream os(fp);
  if (!os) return;
  for (int i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    assert(roomPath.getParentDir() == fp.getParentDir());
    os << roomPath.withoutParentDir() << "\n";
  }
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<Room *> &rooms) {
  std::vector<TFilePath> roomPaths;
  for (int i = 0; i < (int)rooms.size(); i++)
    roomPaths.push_back(rooms[i]->getPath());
  writeRoomList(roomPaths);
}

//-----------------------------------------------------------------------------

void makePrivate(Room *room) {
  TFilePath layoutDir       = ToonzFolder::getMyRoomsDir();
  TFilePath roomPath        = room->getPath();
  std::string mbSrcFileName = roomPath.getName() + "_menubar.xml";
  if (roomPath == TFilePath() || roomPath.getParentDir() != layoutDir) {
    int count = 1;
    for (;;) {
      roomPath = layoutDir + ("room" + std::to_string(count++) + ".ini");
      if (!TFileStatus(roomPath).doesExist()) break;
    }
    room->setPath(roomPath);
    TSystem::touchParentDir(roomPath);
    room->save();
  }
  /*- create private menubar settings if not exists -*/
  std::string mbDstFileName = roomPath.getName() + "_menubar.xml";
  TFilePath myMBPath        = layoutDir + mbDstFileName;
  if (!TFileStatus(myMBPath).isReadable()) {
    TFilePath templateRoomMBPath =
        ToonzFolder::getTemplateRoomsDir() + mbSrcFileName;
    if (TFileStatus(templateRoomMBPath).doesExist())
      TSystem::copyFile(myMBPath, templateRoomMBPath);
    else {
      TFilePath templateFullMBPath =
          ToonzFolder::getTemplateRoomsDir() + "menubar_template.xml";
      if (TFileStatus(templateFullMBPath).doesExist())
        TSystem::copyFile(myMBPath, templateFullMBPath);
      else
        DVGui::warning(
            QObject::tr("Cannot open menubar settings template file. "
                        "Re-installing Toonz will solve this problem."));
    }
  }
}

//-----------------------------------------------------------------------------

void makePrivate(std::vector<Room *> &rooms) {
  for (int i = 0; i < (int)rooms.size(); i++) makePrivate(rooms[i]);
}

// major version  :  7 bits
// minor version  :  8 bits
// revision number: 16 bits
int get_version_code_from(std::string ver) {
  int version = 0;

  // major version: assume that the major version is less than 127.
  std::string::size_type const a = ver.find('.');
  std::string const major = (a == std::string::npos) ? ver : ver.substr(0, a);
  version += std::stoi(major) << 24;
  if ((a == std::string::npos) || (a + 1 == ver.length())) {
    return version;
  }

  // minor version: assume that the minor version is less than 255.
  std::string::size_type const b = ver.find('.', a + 1);
  std::string const minor        = (b == std::string::npos)
                                       ? ver.substr(a + 1)
                                       : ver.substr(a + 1, b - a - 1);
  version += std::stoi(minor) << 16;
  if ((b == std::string::npos) || (b + 1 == ver.length())) {
    return version;
  }

  // revision number: assume that the revision number is less than 32767.
  version += std::stoi(ver.substr(b + 1));

  return version;
}

}  // namespace
//=============================================================================

//=============================================================================
// Room
//-----------------------------------------------------------------------------

void Room::save() {
  DockLayout *layout = dockLayout();

  // Now save layout state
  DockLayout::State state        = layout->saveState();
  std::vector<QRect> &geometries = state.first;

  QSettings settings(toQString(getPath()), QSettings::IniFormat);
  settings.remove("");

  settings.beginGroup("room");

  int i;
  for (i = 0; i < layout->count(); ++i) {
    settings.beginGroup("pane_" + QString::number(i));
    TPanel *pane = static_cast<TPanel *>(layout->itemAt(i)->widget());
    settings.setValue("name", pane->objectName());
    settings.setValue("geometry", geometries[i]);  // Use passed geometry
    if (SaveLoadQSettings *persistent =
            dynamic_cast<SaveLoadQSettings *>(pane->widget()))
      persistent->save(settings);
    if (pane->getViewType() != -1)
      // If panel has different viewtypes, store current one
      settings.setValue("viewtype", pane->getViewType());
    if (pane->objectName() == "FlipBook") {
      // Store flipbook's identification number
      FlipBook *flip = static_cast<FlipBook *>(pane->widget());
      settings.setValue("index", flip->getPoolIndex());
    }
    settings.endGroup();
  }

  // Adding hierarchy string
  settings.setValue("hierarchy", state.second);
  settings.setValue("name", QVariant(QString(m_name)));

  settings.endGroup();
}

//-----------------------------------------------------------------------------

void Room::load(const TFilePath &fp) {
  QSettings settings(toQString(fp), QSettings::IniFormat);

  setPath(fp);

  DockLayout *layout = dockLayout();

  settings.beginGroup("room");
  QStringList itemsList = settings.childGroups();

  std::vector<QRect> geometries;
  unsigned int i;
  for (i = 0; i < itemsList.size(); i++) {
    // Panel i
    // NOTE: Panels have to be retrieved in the precise order they were saved.
    // settings.beginGroup(itemsList[i]);  //NO! itemsList has lexicographical
    // ordering!!
    settings.beginGroup("pane_" + QString::number(i));

    TPanel *pane = 0;
    QString paneObjectName;

    // Retrieve panel name
    QVariant name = settings.value("name");
    if (name.canConvert(QVariant::String)) {
      // Allocate panel
      paneObjectName          = name.toString();
      std::string paneStrName = paneObjectName.toStdString();
      pane = TPanelFactory::createPanel(this, paneObjectName);
      if (SaveLoadQSettings *persistent =
              dynamic_cast<SaveLoadQSettings *>(pane->widget()))
        persistent->load(settings);
    }

    if (!pane) {
      // Allocate a message panel
      MessagePanel *message = new MessagePanel(this);
      message->setWindowTitle(name.toString());
      message->setMessage(
          "This panel is not supported by the currently set license!");

      pane = message;
      pane->setPanelType(paneObjectName.toStdString());
      pane->setObjectName(paneObjectName);
    }

    pane->setObjectName(paneObjectName);

    // Add panel to room
    addDockWidget(pane);

    // Store its geometry
    geometries.push_back(settings.value("geometry").toRect());

    // Restore view type if present
    if (settings.contains("viewtype"))
      pane->setViewType(settings.value("viewtype").toInt());

    // Restore flipbook pool indices
    if (paneObjectName == "FlipBook") {
      int index = settings.value("index").toInt();
      dynamic_cast<FlipBook *>(pane->widget())->setPoolIndex(index);
    }

    settings.endGroup();
  }

  // resolve resize events here to avoid unwanted minimize of floating viewer
  qApp->processEvents();

  DockLayout::State state(geometries, settings.value("hierarchy").toString());

  layout->restoreState(state);

  setName(settings.value("name").toString());
}

//=============================================================================
// MainWindow
//-----------------------------------------------------------------------------

MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
    , m_saveSettingsOnQuit(true)
    , m_oldRoomIndex(0)
    , m_layoutName("") {
  // store a main window pointer in advance of making its contents
  TApp::instance()->setMainWindow(this);

  m_toolsActionGroup = new QActionGroup(this);
  m_toolsActionGroup->setExclusive(true);
  m_currentRoomsChoice = Preferences::instance()->getCurrentRoomChoice();
  defineActions();
  // user defined shortcuts will be loaded here
  CommandManager::instance()->loadShortcuts();

  // initialize tool options shortcuts
  ToolOptionsShortcutInvoker::instance()->initialize();

  TApp::instance()->getCurrentScene()->setDirtyFlag(false);

  // La menuBar altro non è che una toolbar
  // in cui posso inserire quanti custom widget voglio.
  m_topBar = new TopBar(this);

  addToolBar(m_topBar);
  addToolBarBreak(Qt::TopToolBarArea);

  m_stackedWidget = new QStackedWidget(this);

  // For the style sheet
  m_stackedWidget->setObjectName("MainStackedWidget");
  m_stackedWidget->setFrameStyle(QFrame::StyledPanel);

  // To give a border to the stackedWidget.
  /*QFrame *centralWidget = new QFrame(this);
centralWidget->setFrameStyle(QFrame::StyledPanel);
centralWidget->setObjectName("centralWidget");
QHBoxLayout *centralWidgetLayout = new QHBoxLayout;
centralWidgetLayout->setMargin(3);
centralWidgetLayout->addWidget(m_stackedWidget);
centralWidget->setLayout(centralWidgetLayout);*/

  setCentralWidget(m_stackedWidget);

  // Leggo i settings
  readSettings(argumentLayoutFileName);

  // Setto le stanze
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();
  connect(m_stackedWidget, SIGNAL(currentChanged(int)),
          SLOT(onCurrentRoomChanged(int)));
  connect(roomTabWidget, SIGNAL(currentChanged(int)), m_stackedWidget,
          SLOT(setCurrentIndex(int)));

  /*-- タイトルバーにScene名を表示する --*/
  connect(TApp::instance()->getCurrentScene(), SIGNAL(nameSceneChanged()), this,
          SLOT(changeWindowTitle()));
  changeWindowTitle();

  // Connetto i comandi che sono in RoomTabWidget
  connect(roomTabWidget, SIGNAL(indexSwapped(int, int)),
          SLOT(onIndexSwapped(int, int)));
  connect(roomTabWidget, SIGNAL(insertNewTabRoom()), SLOT(insertNewRoom()));
  connect(roomTabWidget, SIGNAL(deleteTabRoom(int)), SLOT(deleteRoom(int)));
  connect(roomTabWidget, SIGNAL(renameTabRoom(int, const QString)),
          SLOT(renameRoom(int, const QString)));

  setCommandHandler("MI_Quit", this, &MainWindow::onQuit);
  setCommandHandler("MI_Undo", this, &MainWindow::onUndo);
  setCommandHandler("MI_Redo", this, &MainWindow::onRedo);
  setCommandHandler("MI_NewScene", this, &MainWindow::onNewScene);
  setCommandHandler("MI_LoadScene", this, &MainWindow::onLoadScene);
  setCommandHandler("MI_LoadSubSceneFile", this, &MainWindow::onLoadSubScene);
  setCommandHandler("MI_ResetRoomLayout", this, &MainWindow::resetRoomsLayout);
  setCommandHandler(MI_AutoFillToggle, this, &MainWindow::autofillToggle);

  setCommandHandler(MI_About, this, &MainWindow::onAbout);
  setCommandHandler(MI_OpenOnlineManual, this, &MainWindow::onOpenOnlineManual);
  setCommandHandler(MI_OpenWhatsNew, this, &MainWindow::onOpenWhatsNew);
  setCommandHandler(MI_OpenCommunityForum, this,
                    &MainWindow::onOpenCommunityForum);
  setCommandHandler(MI_OpenReportABug, this, &MainWindow::onOpenReportABug);

  setCommandHandler(MI_MaximizePanel, this, &MainWindow::maximizePanel);
  setCommandHandler(MI_FullScreenWindow, this, &MainWindow::fullScreenWindow);
  setCommandHandler(MI_SeeThroughWindow, this, &MainWindow::seeThroughWindow);
  setCommandHandler("MI_NewVectorLevel", this,
                    &MainWindow::onNewVectorLevelButtonPressed);
  setCommandHandler("MI_NewToonzRasterLevel", this,
                    &MainWindow::onNewToonzRasterLevelButtonPressed);
  setCommandHandler("MI_NewRasterLevel", this,
                    &MainWindow::onNewRasterLevelButtonPressed);
  setCommandHandler(MI_ClearCacheFolder, this, &MainWindow::clearCacheFolder);
  setCommandHandler("MI_NewMetaLevel", this,
                    &MainWindow::onNewMetaLevelButtonPressed);
  // remove ffmpegCache if still exists from crashed exit
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }

  connect(TApp::instance(), SIGNAL(activeViewerChanged()), this,
          SLOT(onActiveViewerChanged()));
}

//-----------------------------------------------------------------------------

MainWindow::~MainWindow() {
  TEnv::saveAllEnvVariables();
  // cleanup ffmpeg cache
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;

  TProject *project   = scene->getProject();
  QString projectName = QString::fromStdString(project->getName().getName());

  QString sceneName = QString::fromStdWString(scene->getSceneName());

  if (sceneName.isEmpty()) sceneName = tr("Untitled");
  if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");

  /*--- レイアウトファイル名を頭に表示させる ---*/
  if (!m_layoutName.isEmpty()) sceneName.prepend(m_layoutName + " : ");

  QString name = sceneName + " [" + projectName + "] : " +
                 QString::fromStdString(TEnv::getApplicationFullName());

  setWindowTitle(name);
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle(QString &str) { setWindowTitle(str); }

//-----------------------------------------------------------------------------

void MainWindow::startupFloatingPanels() {
  // Show all floating panels
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  int i;
  for (i = 0; i < currDockLayout->count(); ++i) {
    TPanel *currPanel = static_cast<TPanel *>(currDockLayout->widgetAt(i));
    if (currPanel->isFloating()) currPanel->show();
  }
}

//-----------------------------------------------------------------------------

Room *MainWindow::getRoom(int index) const {
  assert(index >= 0 && index < getRoomCount());
  return dynamic_cast<Room *>(m_stackedWidget->widget(index));
}

//-----------------------------------------------------------------------------
/*! Roomを名前から探す
 */
Room *MainWindow::getRoomByName(QString &roomName) {
  for (int i = 0; i < getRoomCount(); i++) {
    Room *room = dynamic_cast<Room *>(m_stackedWidget->widget(i));
    if (room) {
      if (room->getName() == roomName) return room;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------

int MainWindow::getRoomCount() const { return m_stackedWidget->count(); }

//-----------------------------------------------------------------------------

void MainWindow::refreshWriteSettings() { writeSettings(); }

//-----------------------------------------------------------------------------

void MainWindow::readSettings(const QString &argumentLayoutFileName) {
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();

  /*-- Pageを追加すると同時にMenubarを追加する --*/
  StackedMenuBar *stackedMenuBar = m_topBar->getStackedMenuBar();

  std::vector<Room *> rooms;

  // leggo l'elenco dei layout
  std::vector<TFilePath> roomPaths;

  if (readRoomList(roomPaths, argumentLayoutFileName)) {
    if (!argumentLayoutFileName.isEmpty()) {
      /*--
       * タイトルバーに表示するレイアウト名を作る：_layoutがあればそこから省く。無ければ.txtのみ省く
       * --*/
      int pos      = (argumentLayoutFileName.indexOf("_layout") == -1)
                         ? argumentLayoutFileName.indexOf(".txt")
                         : argumentLayoutFileName.indexOf("_layout");
      m_layoutName = argumentLayoutFileName.left(pos);
    }
  }

  int i;
  for (i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    if (TFileStatus(roomPath).doesExist()) {
      Room *room = new Room(this);
      room->load(roomPath);
      m_stackedWidget->addWidget(room);
      roomTabWidget->addTab(room->getName());

      /*- ここでMenuBarファイルをロードする -*/
      std::string mbFileName = roomPath.getName() + "_menubar.xml";
      stackedMenuBar->loadAndAddMenubar(ToonzFolder::getRoomsFile(mbFileName));

      // room->setDockOptions(QMainWindow::DockOptions(
      //  (QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks) &
      //  ~QMainWindow::AllowTabbedDocks));
      rooms.push_back(room);
    }
  }

  // Read the flipbook history
  FlipBookPool::instance()->load(ToonzFolder::getMyModuleDir() +
                                 TFilePath("fliphistory.ini"));

  /*- レイアウト設定ファイルが見つからなかった場合、初期Roomの生成 -*/
  // Se leggendo i settings non ho inizializzato le stanze lo faccio ora.
  // Puo' accadere se si buttano i file di inizializzazione.
  if (rooms.empty()) {
    // CleanupRoom
    Room *cleanupRoom = createCleanupRoom();
    m_stackedWidget->addWidget(cleanupRoom);
    rooms.push_back(cleanupRoom);
    stackedMenuBar->createMenuBarByName(cleanupRoom->getName());

    // PltEditRoom
    Room *pltEditRoom = createPltEditRoom();
    m_stackedWidget->addWidget(pltEditRoom);
    rooms.push_back(pltEditRoom);
    stackedMenuBar->createMenuBarByName(pltEditRoom->getName());

    // InknPaintRoom
    Room *inknPaintRoom = createInknPaintRoom();
    m_stackedWidget->addWidget(inknPaintRoom);
    rooms.push_back(inknPaintRoom);
    stackedMenuBar->createMenuBarByName(inknPaintRoom->getName());

    // XsheetRoom
    Room *xsheetRoom = createXsheetRoom();
    m_stackedWidget->addWidget(xsheetRoom);
    rooms.push_back(xsheetRoom);
    stackedMenuBar->createMenuBarByName(xsheetRoom->getName());

    // BatchesRoom
    Room *batchesRoom = createBatchesRoom();
    m_stackedWidget->addWidget(batchesRoom);
    rooms.push_back(batchesRoom);
    stackedMenuBar->createMenuBarByName(batchesRoom->getName());

    // BrowserRoom
    Room *browserRoom = createBrowserRoom();
    m_stackedWidget->addWidget(browserRoom);
    rooms.push_back(browserRoom);
    stackedMenuBar->createMenuBarByName(browserRoom->getName());
  }

  /*- If the layout files were loaded from template, then save them as private
   * ones -*/
  makePrivate(rooms);
  writeRoomList(rooms);

  // Imposto la stanza corrente
  TFilePath fp = ToonzFolder::getRoomsFile(currentRoomFileName);
  Tifstream is(fp);
  std::string currentRoomName;
  is >> currentRoomName;
  if (currentRoomName != "") {
    int count = m_stackedWidget->count();
    int index;
    for (index = 0; index < count; index++)
      if (getRoom(index)->getName().toStdString() == currentRoomName) break;
    if (index < count) {
      m_oldRoomIndex = index;
      roomTabWidget->setCurrentIndex(index);
      m_stackedWidget->setCurrentIndex(index);
    }
  }

  RecentFiles::instance()->loadRecentFiles();
  CustomPanelManager::instance()->loadCustomPanelEntries();
}

//-----------------------------------------------------------------------------

void MainWindow::writeSettings() {
  std::vector<Room *> rooms;
  int i;

  // Flipbook history
  DockLayout *currRoomLayout(getCurrentRoom()->dockLayout());
  for (i = 0; i < currRoomLayout->count(); ++i) {
    // Remove all floating flipbooks from current room and return them into
    // the flipbook pool.
    TPanel *panel = static_cast<TPanel *>(currRoomLayout->itemAt(i)->widget());
    if (panel->isFloating() && panel->getPanelType() == "FlipBook") {
      currRoomLayout->removeWidget(panel);
      FlipBook *flipbook = static_cast<FlipBook *>(panel->widget());
      FlipBookPool::instance()->push(flipbook);
      --i;
    }
  }

  FlipBookPool::instance()->save();

  // Room layouts
  for (i = 0; i < m_stackedWidget->count(); i++) {
    Room *room = getRoom(i);
    rooms.push_back(room);
    room->save();
  }
  if (m_currentRoomsChoice == Preferences::instance()->getCurrentRoomChoice()) {
    writeRoomList(rooms);
  }

  // Current room settings
  Tofstream os(ToonzFolder::getMyRoomsDir() + currentRoomFileName);
  os << getCurrentRoom()->getName().toStdString();

  // Main window settings
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("mainwindow.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);

  settings.setValue("MainWindowGeometry", saveGeometry());
}

//-----------------------------------------------------------------------------

Room *MainWindow::createCleanupRoom() {
  Room *cleanupRoom = new Room(this);
  cleanupRoom->setName("Cleanup");
  cleanupRoom->setObjectName("CleanupRoom");

  m_topBar->getRoomTabWidget()->addTab(tr("Cleanup"));

  DockLayout *layout = cleanupRoom->dockLayout();

  // Viewer
  TPanel *viewer = TPanelFactory::createPanel(cleanupRoom, "ComboViewer");
  if (viewer) {
    cleanupRoom->addDockWidget(viewer);
    layout->dockItem(viewer);
    ComboViewerPanel *cvp = qobject_cast<ComboViewerPanel *>(viewer);
    if (cvp)
      // hide all parts
      cvp->setVisiblePartsFlag(VPPARTS_None);
  }

  // CleanupSettings
  TPanel *cleanupSettingsPane =
      TPanelFactory::createPanel(cleanupRoom, "CleanupSettings");
  if (cleanupSettingsPane) {
    cleanupRoom->addDockWidget(cleanupSettingsPane);
    layout->dockItem(cleanupSettingsPane, viewer, Region::right);
  }

  // Xsheet
  TPanel *xsheetPane = TPanelFactory::createPanel(cleanupRoom, "Xsheet");
  if (xsheetPane) {
    cleanupRoom->addDockWidget(xsheetPane);
    layout->dockItem(xsheetPane, viewer, Region::bottom);
  }

  return cleanupRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createPltEditRoom() {
  Room *pltEditRoom = new Room(this);
  pltEditRoom->setName("PltEdit");
  pltEditRoom->setObjectName("PltEditRoom");

  m_topBar->getRoomTabWidget()->addTab(tr("PltEdit"));

  DockLayout *layout = pltEditRoom->dockLayout();

  // Viewer
  TPanel *viewer = TPanelFactory::createPanel(pltEditRoom, "ComboViewer");
  if (viewer) {
    pltEditRoom->addDockWidget(viewer);
    layout->dockItem(viewer);

    ComboViewerPanel *cvp = qobject_cast<ComboViewerPanel *>(viewer);
    if (cvp) cvp->setVisiblePartsFlag(VPPARTS_TOOLBAR | VPPARTS_TOOLOPTIONS);
  }

  // Palette
  TPanel *palettePane = TPanelFactory::createPanel(pltEditRoom, "LevelPalette");
  if (palettePane) {
    pltEditRoom->addDockWidget(palettePane);
    layout->dockItem(palettePane, viewer, Region::bottom);
  }

  // StyleEditor
  TPanel *styleEditorPane =
      TPanelFactory::createPanel(pltEditRoom, "StyleEditor");
  if (styleEditorPane) {
    pltEditRoom->addDockWidget(styleEditorPane);
    layout->dockItem(styleEditorPane, viewer, Region::left);
  }

  // Xsheet
  TPanel *xsheetPane = TPanelFactory::createPanel(pltEditRoom, "Xsheet");
  if (xsheetPane) {
    pltEditRoom->addDockWidget(xsheetPane);
    layout->dockItem(xsheetPane, palettePane, Region::left);
  }

  // Studio Palette
  TPanel *studioPaletteViewer =
      TPanelFactory::createPanel(pltEditRoom, "StudioPalette");
  if (studioPaletteViewer) {
    pltEditRoom->addDockWidget(studioPaletteViewer);
    layout->dockItem(studioPaletteViewer, xsheetPane, Region::left);
  }

  return pltEditRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createInknPaintRoom() {
  Room *inknPaintRoom = new Room(this);
  inknPaintRoom->setName("InknPaint");
  inknPaintRoom->setObjectName("InknPaintRoom");

  m_topBar->getRoomTabWidget()->addTab(tr("InknPaint"));

  DockLayout *layout = inknPaintRoom->dockLayout();

  // Viewer
  TPanel *viewer = TPanelFactory::createPanel(inknPaintRoom, "ComboViewer");
  if (viewer) {
    inknPaintRoom->addDockWidget(viewer);
    layout->dockItem(viewer);
  }

  // Palette
  TPanel *palettePane =
      TPanelFactory::createPanel(inknPaintRoom, "LevelPalette");
  if (palettePane) {
    inknPaintRoom->addDockWidget(palettePane);
    layout->dockItem(palettePane, viewer, Region::bottom);
  }

  // Filmstrip
  TPanel *filmStripPane =
      TPanelFactory::createPanel(inknPaintRoom, "FilmStrip");
  if (filmStripPane) {
    inknPaintRoom->addDockWidget(filmStripPane);
    layout->dockItem(filmStripPane, viewer, Region::right);
  }

  return inknPaintRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createXsheetRoom() {
  Room *xsheetRoom = new Room(this);
  xsheetRoom->setName("Xsheet");
  xsheetRoom->setObjectName("XsheetRoom");

  m_topBar->getRoomTabWidget()->addTab(tr("Xsheet"));

  DockLayout *layout = xsheetRoom->dockLayout();

  // Xsheet
  TPanel *xsheetPane = TPanelFactory::createPanel(xsheetRoom, "Xsheet");
  if (xsheetPane) {
    xsheetRoom->addDockWidget(xsheetPane);
    layout->dockItem(xsheetPane);
  }

  // FunctionEditor
  TPanel *functionEditorPane =
      TPanelFactory::createPanel(xsheetRoom, "FunctionEditor");
  if (functionEditorPane) {
    xsheetRoom->addDockWidget(functionEditorPane);
    layout->dockItem(functionEditorPane, xsheetPane, Region::right);
  }

  return xsheetRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createBatchesRoom() {
  Room *batchesRoom = new Room(this);
  batchesRoom->setName("Batches");
  batchesRoom->setObjectName("BatchesRoom");

  m_topBar->getRoomTabWidget()->addTab("Batches");

  DockLayout *layout = batchesRoom->dockLayout();

  // Tasks
  TPanel *tasksViewer = TPanelFactory::createPanel(batchesRoom, "Tasks");
  if (tasksViewer) {
    batchesRoom->addDockWidget(tasksViewer);
    layout->dockItem(tasksViewer);
  }

  // BatchServers
  TPanel *batchServersViewer =
      TPanelFactory::createPanel(batchesRoom, "BatchServers");
  if (batchServersViewer) {
    batchesRoom->addDockWidget(batchServersViewer);
    layout->dockItem(batchServersViewer, tasksViewer, Region::right);
  }

  return batchesRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createBrowserRoom() {
  Room *browserRoom = new Room(this);
  browserRoom->setName("Browser");
  browserRoom->setObjectName("BrowserRoom");

  m_topBar->getRoomTabWidget()->addTab("Browser");

  DockLayout *layout = browserRoom->dockLayout();

  // Browser
  TPanel *browserPane = TPanelFactory::createPanel(browserRoom, "Browser");
  if (browserPane) {
    browserRoom->addDockWidget(browserPane);
    layout->dockItem(browserPane);
  }

  // Scene Cast
  TPanel *sceneCastPanel = TPanelFactory::createPanel(browserRoom, "SceneCast");
  if (sceneCastPanel) {
    browserRoom->addDockWidget(sceneCastPanel);
    layout->dockItem(sceneCastPanel, browserPane, Region::bottom);
  }

  return browserRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::getCurrentRoom() const {
  return getRoom(m_stackedWidget->currentIndex());
}

//-----------------------------------------------------------------------------

void MainWindow::onUndo() {
  // Must wait for current save to finish, just in case
  while (TApp::instance()->isSaveInProgress())
    ;

  ToolHandle *toolH = TApp::instance()->getCurrentTool();

  // do not use undo if tool is currently in use
  if (toolH->getTool()->isUndoable()) {
    bool ret = TUndoManager::manager()->undo();
    if (!ret) DVGui::error(QObject::tr("No more Undo operations available."));
  }
}

//-----------------------------------------------------------------------------

void MainWindow::onRedo() {
  // Must wait for current save to finish, just in case
  while (TApp::instance()->isSaveInProgress())
    ;

  bool ret = TUndoManager::manager()->redo();
  if (!ret) DVGui::error(QObject::tr("No more Redo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onNewScene() {
  IoCmd::newScene();
  CommandManager *cm = CommandManager::instance();
  cm->setChecked(MI_ShiftTrace, false);
  cm->setChecked(MI_EditShift, false);
  cm->setChecked(MI_NoShift, false);
  cm->setChecked(MI_VectorGuidedDrawing, false);
}

//-----------------------------------------------------------------------------

void MainWindow::onLoadScene() { IoCmd::loadScene(); }

//-----------------------------------------------------------------------------

void MainWindow::onLoadSubScene() { IoCmd::loadSubScene(); }
//-----------------------------------------------------------------------------

void MainWindow::onUpgradeTabPro() {}

//-----------------------------------------------------------------------------

void MainWindow::onAbout() {
  QLabel *label  = new QLabel();
  QPixmap pixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
  pixmap.setDevicePixelRatio(getDevicePixelRatio(this));
  label->setPixmap(pixmap);

  DVGui::Dialog *dialog = new DVGui::Dialog(this, true);
  dialog->setWindowTitle(tr("About OpenToonz"));
  dialog->setTopMargin(0);
  dialog->addWidget(label);

  QString name = QString::fromStdString(TEnv::getApplicationFullName());
  name += " (built " __DATE__ " " __TIME__ ")";
  dialog->addWidget(new QLabel(name));

  QPushButton *button = new QPushButton(tr("Close"), dialog);
  button->setDefault(true);
  dialog->addButtonBarWidget(button);
  connect(button, SIGNAL(clicked()), dialog, SLOT(accept()));

  dialog->exec();
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenOnlineManual() {
  QDesktopServices::openUrl(QUrl(tr("http://opentoonz.readthedocs.io")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenWhatsNew() {
  QDesktopServices::openUrl(
      QUrl(tr("https://github.com/opentoonz/opentoonz/releases/latest")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenCommunityForum() {
  QDesktopServices::openUrl(
      QUrl(tr("https://groups.google.com/forum/#!forum/opentoonz_en")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenReportABug() {
  QString str = QString(
      tr("To report a bug, click on the button below to open a web browser "
         "window for OpenToonz's Issues page on https://github.com.  Click on "
         "the 'New issue' button and fill out the form."));

  std::vector<QString> buttons = {QObject::tr("Report a Bug"),
                                  QObject::tr("Close")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons, 1);
  if (ret == 1)
    QDesktopServices::openUrl(
        QUrl("https://github.com/opentoonz/opentoonz/issues"));
}
//-----------------------------------------------------------------------------

void MainWindow::autofillToggle() {
  TPaletteHandle *h = TApp::instance()->getCurrentPalette();
  h->toggleAutopaint();
}

void MainWindow::resetRoomsLayout() {
  if (!m_saveSettingsOnQuit) return;

  QString message(tr("Reset rooms to their default?"));
  message += "\n" + tr("All user rooms will be lost!");

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Reset Rooms"), message,
      QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

  if (ret != QMessageBox::Yes) return;

  // Reflect changes to initwizard.cpp: void UIPage::resetRoom()

  m_saveSettingsOnQuit = false;

  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  if (layoutDir != TFilePath()) {
    // TSystem::deleteFile(layoutDir);
    TSystem::rmDirTree(layoutDir);
  }
  /*if (layoutDir != TFilePath()) {
          try {
                  TFilePathSet fpset;
                  TSystem::readDirectory(fpset, layoutDir, true, false);
                  for (auto const& path : fpset) {
                          QString fn = toQString(path.withoutParentDir());
                          if (fn.startsWith("room") || fn.startsWith("popups"))
  {
                                  TSystem::deleteFile(path);
                          }
                  }
          } catch (...) {
          }
  }*/

  DVGui::info(
      QObject::tr("The rooms will be reset the next time you run OpenToonz."));

  ret = QMessageBox::question(
      this, tr("Reset Rooms"), tr("You must restart OpenToonz, close it now?"),
      QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

  if (ret == QMessageBox::Yes) close();
}

void MainWindow::maximizePanel() {
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  if (currDockLayout->getMaximized() &&
      currDockLayout->getMaximized()->isMaximized()) {
    currDockLayout->getMaximized()->maximizeDock();  // release maximization
    return;
  }

  QPoint p            = mapFromGlobal(QCursor::pos());
  QWidget *currWidget = currDockLayout->containerOf(p);
  DockWidget *currW   = dynamic_cast<DockWidget *>(currWidget);
  if (currW) currW->maximizeDock();
}

void MainWindow::fullScreenWindow() {
  if (isFullScreen()) {
    if (m_wasMaximized)
      showMaximized();
    else
      showNormal();
  } else {
#if defined(_WIN32)
    // http://doc.qt.io/qt-5/windows-issues.html#fullscreen-opengl-based-windows
    this->winId();
    QWindowsWindowFunctions::setHasBorderInFullScreen(this->windowHandle(),
                                                      true);
#endif
    m_wasMaximized = isMaximized();
    this->showFullScreen();
  }
}

void MainWindow::seeThroughWindow() {
  SeeThroughWindowMode::instance()->toggleMode(this);
}

//-----------------------------------------------------------------------------

void MainWindow::onCurrentRoomChanged(int newRoomIndex) {
  Room *oldRoom            = getRoom(m_oldRoomIndex);
  Room *newRoom            = getRoom(newRoomIndex);
  QList<TPanel *> paneList = oldRoom->findChildren<TPanel *>();

  // Change the parent of all the floating dockWidgets
  for (int i = 0; i < paneList.size(); i++) {
    TPanel *pane = paneList.at(i);
    if (pane->isFloating() && !pane->isHidden()) {
      QRect oldGeometry = pane->geometry();
      // Just setting the new parent is not enough for the new layout manager.
      // Must be removed from the old and added to the new.
      oldRoom->removeDockWidget(pane);
      newRoom->addDockWidget(pane);
      // pane->setParent(newRoom);

      // Some flags are lost when the parent changes.
      pane->setFloating(true);
      pane->setGeometry(oldGeometry);
      pane->show();
    }
  }
  m_oldRoomIndex = newRoomIndex;
  TSelection::setCurrent(0);
}

//-----------------------------------------------------------------------------

void MainWindow::onIndexSwapped(int firstIndex, int secondIndex) {
  assert(firstIndex >= 0 && secondIndex >= 0);
  Room *mainWindow = getRoom(firstIndex);
  m_stackedWidget->removeWidget(mainWindow);
  m_stackedWidget->insertWidget(secondIndex, mainWindow);
}

//-----------------------------------------------------------------------------

void MainWindow::insertNewRoom() {
  Room *room = new Room(this);
  room->setName("room");
  if (m_saveSettingsOnQuit) makePrivate(room);
  m_stackedWidget->insertWidget(0, room);

  // Finally, old room index is increased by 1
  m_oldRoomIndex++;
}

//-----------------------------------------------------------------------------

void MainWindow::deleteRoom(int index) {
  Room *room = getRoom(index);

  TFilePath fp = room->getPath();
  try {
    TSystem::deleteFile(fp);
  } catch (...) {
    DVGui::error(tr("Cannot delete") + toQString(fp));
    // Se non ho rimosso la stanza devo rimettere il tab!!
    m_topBar->getRoomTabWidget()->insertTab(index, room->getName());
    return;
  }

  /*- delete menubar settings file as well -*/
  std::string mbFileName = fp.getName() + "_menubar.xml";
  TFilePath mbFp         = fp.getParentDir() + mbFileName;
  TSystem::deleteFile(mbFp);

  // The old room index must be updated if index < of it
  if (index < m_oldRoomIndex) m_oldRoomIndex--;

  m_stackedWidget->removeWidget(room);
  delete room;
}

//-----------------------------------------------------------------------------

void MainWindow::renameRoom(int index, const QString name) {
  Room *room = getRoom(index);
  room->setName(name);
  if (m_saveSettingsOnQuit) room->save();
}

//-----------------------------------------------------------------------------

void MainWindow::onMenuCheckboxChanged() {
  QAction *action = qobject_cast<QAction *>(sender());

  int isChecked = action->isChecked();

  CommandManager *cm = CommandManager::instance();

  if (cm->getAction(MI_ViewCamera) == action)
    ViewCameraToggleAction = isChecked;
  else if (cm->getAction(MI_ViewTable) == action)
    ViewTableToggleAction = isChecked;
  else if (cm->getAction(MI_ToggleEditInPlace) == action)
    EditInPlaceToggleAction = isChecked;
  else if (cm->getAction(MI_ViewBBox) == action)
    ViewBBoxToggleAction = isChecked;
  else if (cm->getAction(MI_FieldGuide) == action)
    FieldGuideToggleAction = isChecked;
  else if (cm->getAction(MI_RasterizePli) == action) {
    if (!QGLPixelBuffer::hasOpenGLPbuffers()) isChecked = 0;
    RasterizePliToggleAction = isChecked;
  } else if (cm->getAction(MI_SafeArea) == action)
    SafeAreaToggleAction = isChecked;
  else if (cm->getAction(MI_ViewColorcard) == action)
    ViewColorcardToggleAction = isChecked;
  else if (cm->getAction(MI_ViewGuide) == action)
    ViewGuideToggleAction = isChecked;
  else if (cm->getAction(MI_ViewRuler) == action)
    ViewRulerToggleAction = isChecked;
  else if (cm->getAction(MI_TCheck) == action)
    TCheckToggleAction = isChecked;
  else if (cm->getAction(MI_DockingCheck) == action)
    DockingCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ICheck) == action)
    ICheckToggleAction = isChecked;
  else if (cm->getAction(MI_Ink1Check) == action)
    Ink1CheckToggleAction = isChecked;
  else if (cm->getAction(MI_PCheck) == action)
    PCheckToggleAction = isChecked;
  else if (cm->getAction(MI_BCheck) == action)
    BCheckToggleAction = isChecked;
  else if (cm->getAction(MI_GCheck) == action)
    GCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ACheck) == action)
    ACheckToggleAction = isChecked;
  else if (cm->getAction(MI_IOnly) == action)
    IOnlyToggleAction = isChecked;
  else if (cm->getAction(MI_Link) == action)
    LinkToggleAction = isChecked;
  else if (cm->getAction(MI_ShiftTrace) == action)
    ShiftTraceToggleAction = isChecked;
  else if (cm->getAction(MI_EditShift) == action)
    EditShiftToggleAction = isChecked;
  else if (cm->getAction(MI_NoShift) == action)
    NoShiftToggleAction = isChecked;
  else if (cm->getAction(MI_TouchGestureControl) == action)
    TouchGestureControl = isChecked;
}

//-----------------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent *event) {
  getCurrentRoom()->layout()->setEnabled(true);  // See main function in
                                                 // main.cpp
  if (Preferences::instance()->isStartupPopupEnabled() &&
      !m_startupPopupShown) {
    StartupPopup *startupPopup = new StartupPopup();
    startupPopup->show();
    m_startupPopupShown = true;
  }
}
extern const char *applicationName;
extern const char *applicationVersion;
//-----------------------------------------------------------------------------
void MainWindow::checkForUpdates() {
  // Since there is only a single version of Opentoonz, we can do a simple check
  // against a string
  QString updateUrl("http://opentoonz.github.io/opentoonz-version.txt");

  m_updateChecker = new UpdateChecker(updateUrl);
  connect(m_updateChecker, SIGNAL(done(bool)), this,
          SLOT(onUpdateCheckerDone(bool)));
}
//-----------------------------------------------------------------------------

void MainWindow::onUpdateCheckerDone(bool error) {
  if (error) {
    // Get the last update date
    return;
  }

  int const software_version =
      get_version_code_from(TEnv::getApplicationVersion());
  int const latest_version =
      get_version_code_from(m_updateChecker->getLatestVersion().toStdString());
  if (software_version < latest_version) {
    QStringList buttons;
    buttons.push_back(QObject::tr("Visit Web Site"));
    buttons.push_back(QObject::tr("Cancel"));
    DVGui::MessageAndCheckboxDialog *dialog = DVGui::createMsgandCheckbox(
        DVGui::INFORMATION,
        QObject::tr("An update is available for this software.\nVisit the Web "
                    "site for more information."),
        QObject::tr("Check for the latest version on launch."), buttons, 0,
        Qt::Checked);
    int ret = dialog->exec();
    if (dialog->getChecked() == Qt::Unchecked)
      Preferences::instance()->setValue(latestVersionCheckEnabled, false);
    dialog->deleteLater();
    if (ret == 1) {
      // Write the new last date to file
      QDesktopServices::openUrl(QObject::tr("https://opentoonz.github.io/e/"));
    }
  }

  disconnect(m_updateChecker);
  m_updateChecker->deleteLater();
}

//-----------------------------------------------------------------------------

void MainWindow::onActiveViewerChanged() {
  // sync the command state to the button state of the activated viewer
  SceneViewer *activeViewer = TApp::instance()->getActiveViewer();
  if (!activeViewer) return;
  BaseViewerPanel *bvp = qobject_cast<BaseViewerPanel *>(
      activeViewer->parentWidget()->parentWidget());
  if (!bvp) return;
  bool prev, subCamPrev;
  bvp->getPreviewButtonStates(prev, subCamPrev);

  CommandManager::instance()
      ->getAction(MI_ToggleViewerPreview)
      ->setChecked(prev);
  CommandManager::instance()
      ->getAction(MI_ToggleViewerSubCameraPreview)
      ->setChecked(subCamPrev);
}

//-----------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event) {
  // il riferimento alla variabile booleana doExit viene passato a tutti gli
  // slot che catturano l'emissione
  // del segnale. Impostando a false tale variabile, l'applicazione non viene
  // chiusa!
  bool doExit = true;
  emit exit(doExit);
  if (!doExit) {
    event->ignore();
    return;
  }

  if (!IoCmd::saveSceneIfNeeded(QApplication::tr("Quit"))) {
    event->ignore();
    return;
  }

  // sto facendo quit. interrompo (il prima possibile) le threads
  IconGenerator::instance()->clearRequests();

  if (m_saveSettingsOnQuit) writeSettings();

  // svuoto la directory di output
  TFilePath outputsDir = TEnv::getStuffDir() + "outputs";
  TFilePathSet fps;
  if (TFileStatus(outputsDir).isDirectory()) {
    TSystem::readDirectory(fps, outputsDir);
    TFilePathSet::iterator fpsIt;
    for (fpsIt = fps.begin(); fpsIt != fps.end(); ++fpsIt) {
      TFilePath fp = *fpsIt;
      if (TSystem::doesExistFileOrLevel(fp)) {
        try {
          TSystem::removeFileOrLevel(fp);
        } catch (...) {
        }
      }
    }
  }

  TImageCache::instance()->clear(true);

  event->accept();
  TThread::shutdown();
  qApp->quit();
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createAction(const char *id, const char *name,
                                  const QString &defaultShortcut,
                                  CommandType type, const char *iconSVGName) {
  QAction *action = new DVAction(tr(name), this);

  // For "edit" category menu commands may behave various function
  // according to the current selection.
  // Now the command name can be adjusted on switching the selection.
  // Here we explicitly register the default text in order to recover the
  // command name if the selection does not specify the alternative command
  // name.
  if (type == MenuEditCommandType) action->setIconText(tr(name));

#if !defined(_WIN32)
  bool visible = Preferences::instance()->getBoolValue(showIconsInMenu);
  action->setIconVisibleInMenu(visible);
#endif

  // BUG_WORKAROUND: #20230627
  // In Qt5.15.2 - Windows, QMenu stylesheet has alignment issue when one item
  // has icon and another has not one. (See
  // https://bugreports.qt.io/browse/QTBUG-90242 for details.) To avoid the
  // problem here we attach the temporary icons for every actions which have no
  // icons. The temporary icon is visible in 20x20 pixel size (for command bar)
  // with first 2 upper-case-letters in the command id, and is transparent in
  // 16x16 pixel size so that it is hidden in menu bar. I put the transparent
  // icon instead of using setIconVisibleInMenu(false) to avoid another
  // alignment issue that the shortcut text overlaps command name.
  if (!iconSVGName || !*iconSVGName) {
#ifdef _WIN32
    action->setIcon(createTemporaryIconFromName(name));
#endif
    // do nothing for other platforms
  } else
    action->setIcon(createQIcon(iconSVGName, false, true));
  addAction(action);
#ifdef MACOSX
  // To prevent the wrong menu items (due to MacOS menu naming conventions),
  // from
  // taking Preferences, Quit, or About roles (sometimes happens unexpectedly in
  // translations) - all menu items should have "NoRole"
  //  except for Preferences, Quit, and About
  if (strcmp(id, MI_Preferences) == 0)
    action->setMenuRole(QAction::PreferencesRole);
  else if (strcmp(id, MI_Quit) == 0)
    action->setMenuRole(QAction::QuitRole);
  else if (strcmp(id, MI_About) == 0)
    action->setMenuRole(QAction::AboutRole);
  else
    action->setMenuRole(QAction::NoRole);
#endif
  CommandManager::instance()->define(id, type, defaultShortcut.toStdString(),
                                     action, iconSVGName);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRightClickMenuAction(const char *id,
                                                const char *name,
                                                const QString &defaultShortcut,
                                                const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, RightClickMenuCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuFileAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuFileCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuEditAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuEditCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuScanCleanupAction(const char *id,
                                                 const char *name,
                                                 const QString &defaultShortcut,
                                                 const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuScanCleanupCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuLevelAction(const char *id, const char *name,
                                           const QString &defaultShortcut,
                                           const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuLevelCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuXsheetAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuXsheetCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuCellsAction(const char *id, const char *name,
                                           const QString &defaultShortcut,
                                           const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuCellsCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuViewAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuViewCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuWindowsAction(const char *id, const char *name,
                                             const QString &defaultShortcut,
                                             const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuWindowsCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuPlayAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuPlayCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuRenderAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuRenderCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuHelpAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, MenuHelpCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRGBAAction(const char *id, const char *name,
                                      const QString &defaultShortcut,
                                      const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, RGBACommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createFillAction(const char *id, const char *name,
                                      const QString &defaultShortcut,
                                      const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, FillCommandType, iconSVGName);
}
//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuAction(const char *id, const char *name,
                                      QList<QString> list,
                                      bool isForRecentFiles) {
  QMenu *menu     = new DVMenuAction(tr(name), this, list, isForRecentFiles);
  QAction *action = menu->menuAction();
  CommandManager::instance()->define(id, MenuCommandType, "", action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createViewerAction(const char *id, const char *name,
                                        const QString &defaultShortcut,
                                        const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, ZoomCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createVisualizationButtonAction(const char *id,
                                                     const char *name,
                                                     const char *iconSVGName) {
  return createAction(id, name, "", VisualizationButtonCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMiscAction(const char *id, const char *name,
                                      const char *defaultShortcut) {
  QAction *action = new DVAction(tr(name), this);
  CommandManager::instance()->define(id, MiscCommandType, defaultShortcut,
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolOptionsAction(const char *id, const char *name,
                                             const QString &defaultShortcut,
                                             const char *iconSVGName) {
  QAction *action = new DVAction(tr(name), this);
  if (iconSVGName && *iconSVGName)
    action->setIcon(createQIcon(iconSVGName, false, true));
  addAction(action);
  CommandManager::instance()->define(id, ToolModifierCommandType,
                                     defaultShortcut.toStdString(), action,
                                     iconSVGName);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createStopMotionAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName) {
  return createAction(id, name, defaultShortcut, StopMotionCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToggle(const char *id, const char *name,
                                  const QString &defaultShortcut,
                                  bool startStatus, CommandType type,
                                  const char *iconSVGName) {
  QAction *action = createAction(id, name, defaultShortcut, type, iconSVGName);
  // Remove if the icon is not set. Checkbox will be drawn by style sheet.
  if (!iconSVGName || !*iconSVGName) action->setIcon(QIcon());
#if defined(_WIN32)
  else {
    bool visible = Preferences::instance()->getBoolValue(showIconsInMenu);
    action->setIconVisibleInMenu(visible);
  }
#endif
  action->setCheckable(true);
  if (startStatus == true) action->trigger();
  bool ret =
      connect(action, SIGNAL(changed()), this, SLOT(onMenuCheckboxChanged()));
  assert(ret);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolAction(const char *id, const char *iconName,
                                      const char *name,
                                      const QString &defaultShortcut) {
  QIcon icon      = createQIcon(iconName);
  QAction *action = new DVAction(icon, tr(name), this);
  action->setCheckable(true);
  action->setActionGroup(m_toolsActionGroup);

  // When the viewer is maximized (not fullscreen) the toolbar is hidden and the
  // actions are disabled,
  // so the tool shortcuts don't work.
  // Adding tool actions to the main window solve this problem!
  addAction(action);

  CommandManager::instance()->define(
      id, ToolCommandType, defaultShortcut.toStdString(), action, iconName);
  return action;
}

//-----------------------------------------------------------------------------

void MainWindow::defineActions() {
  QAction *menuAct;

  // Menu - File

  createMenuFileAction(MI_NewScene, QT_TR_NOOP("&New Scene"), "Ctrl+N",
                       "new_scene");
  createMenuFileAction(MI_LoadScene, QT_TR_NOOP("&Load Scene..."), "Ctrl+L",
                       "load_scene");
  createMenuFileAction(MI_SaveScene, QT_TR_NOOP("&Save Scene"), "Ctrl+Shift+S",
                       "save_scene");
  createMenuFileAction(MI_SaveSceneAs, QT_TR_NOOP("&Save Scene As..."), "",
                       "save_scene_as");
  createMenuFileAction(MI_SaveAll, QT_TR_NOOP("&Save All"), "Ctrl+S",
                       "saveall");
  menuAct = createMenuFileAction(MI_RevertScene, QT_TR_NOOP("&Revert Scene"),
                                 "", "revert_scene");
  menuAct->setEnabled(false);
  QList<QString> files;
  createMenuFileAction(MI_LoadFolder, QT_TR_NOOP("&Load Folder..."), "",
                       "load_folder");
  createMenuFileAction(MI_LoadSubSceneFile,
                       QT_TR_NOOP("&Load As Sub-xsheet..."), "",
                       "load_as_sub_xsheet");
  createMenuAction(MI_OpenRecentScene, QT_TR_NOOP("&Open Recent Scene File"),
                   files);
  createMenuAction(MI_OpenRecentLevel, QT_TR_NOOP("&Open Recent Level File"),
                   files);
  createMenuFileAction(MI_ClearRecentScene,
                       QT_TR_NOOP("&Clear Recent Scene File List"), "");
  createMenuFileAction(MI_ClearRecentLevel,
                       QT_TR_NOOP("&Clear Recent level File List"), "");
  createMenuFileAction(MI_ConvertFileWithInput, QT_TR_NOOP("&Convert File..."),
                       "", "convert");
  createMenuFileAction(MI_ConvertTZPInFolder,
                       QT_TR_NOOP("&Convert TZP Files In Folder..."), "");
  createMenuFileAction(MI_LoadColorModel, QT_TR_NOOP("&Load Color Model..."),
                       "", "load_colormodel");
  createMenuFileAction(MI_ImportMagpieFile,
                       QT_TR_NOOP("&Import Toonz Lip Sync File..."), "",
                       "dialogue_import");
  createMenuFileAction(MI_NewProject, QT_TR_NOOP("&New Project..."), "",
                       "new_project");
  createMenuFileAction(MI_ProjectSettings, QT_TR_NOOP("&Project Settings..."),
                       "", "project_settings");
  createMenuFileAction(MI_SaveDefaultSettings,
                       QT_TR_NOOP("&Save Default Settings"), "",
                       "save_default_settings");
  createMenuFileAction(MI_SoundTrack, QT_TR_NOOP("&Export Soundtrack"), "");
  createMenuFileAction(MI_Preferences, QT_TR_NOOP("&Preferences..."), "Ctrl+U",
                       "gear");
  createMenuFileAction(MI_ShortcutPopup, QT_TR_NOOP("&Configure Shortcuts..."),
                       "", "shortcuts");
  createMenuFileAction(MI_PrintXsheet, QT_TR_NOOP("&Print Xsheet"), "",
                       "printer");

  createMenuFileAction(MI_ExportXsheetPDF, QT_TR_NOOP("&Export Xsheet to PDF"),
                       "");

  createMenuFileAction(
      MI_ExportXDTS,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Export Exchange Digital Time Sheet (XDTS)"),
      "");
  createMenuFileAction(
      MI_ExportOCA,
      QT_TRANSLATE_NOOP("MainWindow", "Export Open Cel Animation (OCA)"), "",
      "export_oca");
  createMenuFileAction(
      MI_ImportOCA,
      QT_TRANSLATE_NOOP("MainWindow", "Import Open Cel Animation (OCA)"), "",
      "import_oca");
  createMenuFileAction(
      MI_ExportTvpJson,
      QT_TRANSLATE_NOOP("MainWindow", "Export TVPaint JSON File"), "");
  createMenuFileAction("MI_RunScript", QT_TR_NOOP("Run Script..."), "",
                       "run_script");
  createMenuFileAction("MI_OpenScriptConsole",
                       QT_TR_NOOP("Open Script Console..."), "", "console");
  createMenuFileAction(MI_Print, QT_TR_NOOP("&Print Current Frame..."),
                       "Ctrl+P", "printer");
  createMenuFileAction(MI_Quit, QT_TR_NOOP("&Quit"), "Ctrl+Q", "quit");
#ifndef NDEBUG
  createMenuFileAction("MI_ReloadStyle", QT_TR_NOOP("Reload qss"), "");
#endif
  createMenuAction(MI_LoadRecentImage, QT_TR_NOOP("&Load Recent Image Files"),
                   files);
  createMenuFileAction(MI_ClearRecentImage,
                       QT_TR_NOOP("&Clear Recent Flipbook Image List"), "");
  createMenuFileAction(MI_ClearCacheFolder, QT_TR_NOOP("&Clear Cache Folder"),
                       "", "clear_cache");
  createMenuFileAction(MI_ExportCurrentScene,
                       QT_TR_NOOP("&Export Current Scene"), "");
  createMenuFileAction(MI_ExportCameraTrack, QT_TR_NOOP("&Export Camera Track"),
                       "");

  // Menu - Edit

  createMenuEditAction(MI_SelectAll, QT_TR_NOOP("&Select All"), "Ctrl+A",
                       "select_all");
  createMenuEditAction(MI_InvertSelection, QT_TR_NOOP("&Invert Selection"), "",
                       "invert_selection");
  createMenuEditAction(MI_Undo, QT_TR_NOOP("&Undo"), "Ctrl+Z", "undo");
  createMenuEditAction(MI_Redo, QT_TR_NOOP("&Redo"), "Ctrl+Y", "redo");
  createMenuEditAction(MI_Cut, QT_TR_NOOP("&Cut"), "Ctrl+X", "cut");
  createMenuEditAction(MI_Copy, QT_TR_NOOP("&Copy"), "Ctrl+C", "content_copy");
  createMenuEditAction(MI_Paste, QT_TR_NOOP("&Paste Insert"), "Ctrl+V",
                       "paste");
  createMenuEditAction(MI_PasteAbove, QT_TR_NOOP("&Paste Insert Above/After"),
                       "Ctrl+Shift+V", "paste_above_after");
  createMenuEditAction(MI_PasteDuplicate, QT_TR_NOOP("&Paste as a Copy"), "",
                       "paste_duplicate");
  createMenuEditAction(MI_PasteInto, QT_TR_NOOP("&Paste Into"), "",
                       "paste_into");
  createMenuEditAction(MI_Clear, QT_TR_NOOP("&Delete"), "Del", "delete");
  createMenuEditAction(MI_Insert, QT_TR_NOOP("&Insert"), "Ins", "insert");
  createMenuEditAction(MI_InsertAbove, QT_TR_NOOP("&Insert Above/After"),
                       "Shift+Ins", "insert_above_after");
  createMenuEditAction(MI_Group, QT_TR_NOOP("&Group"), "Ctrl+G", "group");
  createMenuEditAction(MI_Ungroup, QT_TR_NOOP("&Ungroup"), "Ctrl+Shift+G",
                       "ungroup");
  createMenuEditAction(MI_EnterGroup, QT_TR_NOOP("&Enter Group"), "",
                       "enter_group");
  createMenuEditAction(MI_ExitGroup, QT_TR_NOOP("&Exit Group"), "",
                       "leave_group");
  createMenuEditAction(MI_SendBack, QT_TR_NOOP("&Move to Back"), "Ctrl+[",
                       "move_to_back");
  createMenuEditAction(MI_SendBackward, QT_TR_NOOP("&Move Back One"), "[",
                       "move_back_one");
  createMenuEditAction(MI_BringForward, QT_TR_NOOP("&Move Forward One"), "]",
                       "move_forward_one");
  createMenuEditAction(MI_BringToFront, QT_TR_NOOP("&Move to Front"), "Ctrl+]",
                       "move_to_front");

  // Menu - Scan & Cleanup

  createMenuScanCleanupAction(
      MI_DefineScanner, QT_TR_NOOP("&Define Scanner..."), "", "scanner_define");
  createMenuScanCleanupAction(MI_ScanSettings, QT_TR_NOOP("&Scan Settings..."),
                              "", "scanner_settings");
  createMenuScanCleanupAction(MI_Scan, QT_TR_NOOP("&Scan"), "", "scanner");
  createMenuScanCleanupAction(MI_Autocenter, QT_TR_NOOP("&Autocenter..."), "",
                              "autocenter");
  menuAct = createToggle(MI_SetScanCropbox, QT_TR_NOOP("&Set Cropbox"), "", 0,
                         MenuScanCleanupCommandType, "set_cropbox");
  SetScanCropboxCheck::instance()->setToggle(menuAct);
  QString scannerType = QSettings().value("CurrentScannerType").toString();
  if (scannerType == "TWAIN") menuAct->setDisabled(true);
  menuAct = createMenuScanCleanupAction(
      MI_ResetScanCropbox, QT_TR_NOOP("&Reset Cropbox"), "", "reset_cropbox");
  if (scannerType == "TWAIN") menuAct->setDisabled(true);
  createMenuScanCleanupAction(MI_CleanupSettings,
                              QT_TR_NOOP("&Cleanup Settings..."), "",
                              "cleanup_settings");
  menuAct = createToggle(MI_CleanupPreview, QT_TR_NOOP("&Preview Cleanup"), "",
                         0, MenuScanCleanupCommandType, "cleanup_preview");
  CleanupPreviewCheck::instance()->setToggle(menuAct);
  menuAct = createToggle(MI_CameraTest, QT_TR_NOOP("&Camera Test"), "", 0,
                         MenuScanCleanupCommandType, "camera_test");
  CameraTestCheck::instance()->setToggle(menuAct);
  createToggle(MI_OpacityCheck, QT_TR_NOOP("&Opacity Check"), "Alt+1", false,
               MenuScanCleanupCommandType, "opacity_check");
  createMenuScanCleanupAction(MI_Cleanup, QT_TR_NOOP("&Cleanup"), "",
                              "cleanup");
  createMenuScanCleanupAction(MI_PencilTest, QT_TR_NOOP("&Camera Capture..."),
                              "", "camera_capture");

  // Menu - Level

  createMenuLevelAction(MI_NewLevel, QT_TR_NOOP("&New Level..."), "Alt+N",
                        "new_level");
  createMenuLevelAction(MI_NewVectorLevel, QT_TR_NOOP("&New Vector Level"), "",
                        "new_vector_level");
  createMenuLevelAction(MI_NewToonzRasterLevel,
                        QT_TR_NOOP("&New Toonz Raster Level"), "",
                        "new_toonz_raster_level");
  createMenuLevelAction(MI_NewRasterLevel, QT_TR_NOOP("&New Raster Level"), "",
                        "new_raster_level");
  createMenuFileAction(MI_NewMetaLevel, QT_TR_NOOP("&New Assistant Level"),
                       "new_meta_level");
  createMenuLevelAction(MI_LoadLevel, QT_TR_NOOP("&Load Level..."), "",
                        "load_level");
  createMenuLevelAction(MI_SaveLevel, QT_TR_NOOP("&Save Level"), "",
                        "save_level");
  createMenuLevelAction(MI_SaveAllLevels, QT_TR_NOOP("&Save All Levels"), "",
                        "save_all_levels");
  createMenuLevelAction(MI_SaveLevelAs, QT_TR_NOOP("&Save Level As..."), "",
                        "save_level_as");
  createMenuLevelAction(MI_ExportLevel, QT_TR_NOOP("&Export Level..."), "",
                        "export_level");
  createMenuLevelAction(MI_RemoveEndpoints,
                        QT_TR_NOOP("&Remove Vector Overflow"), "",
                        "remove_vector_overflow");
  createMenuLevelAction(MI_AddFrames, QT_TR_NOOP("&Add Frames..."), "",
                        "add_cells");
  createMenuLevelAction(MI_Renumber, QT_TR_NOOP("&Renumber..."), "",
                        "renumber");
  createMenuLevelAction(MI_ReplaceLevel, QT_TR_NOOP("&Replace Level..."), "",
                        "replace_level");
  createMenuLevelAction(MI_RevertToCleanedUp,
                        QT_TR_NOOP("&Revert to Cleaned Up"), "",
                        "revert_level_to_cleanup");
  createMenuLevelAction(MI_RevertToLastSaved, QT_TR_NOOP("&Reload"), "",
                        "reload_level");
  createMenuLevelAction(MI_ExposeResource, QT_TR_NOOP("&Expose in Xsheet"), "");
  createMenuLevelAction(MI_EditLevel, QT_TR_NOOP("&Display in Level Strip"),
                        "");
  createMenuLevelAction(MI_LevelSettings, QT_TR_NOOP("&Level Settings..."), "",
                        "level_settings");
  createMenuLevelAction(MI_AdjustLevels, QT_TR_NOOP("Adjust Levels..."), "",
                        "histograms");
  createMenuLevelAction(MI_AdjustThickness, QT_TR_NOOP("Adjust Thickness..."),
                        "", "thickness");
  createMenuLevelAction(MI_Antialias, QT_TR_NOOP("&Antialias..."), "",
                        "antialias");
  createMenuLevelAction(MI_Binarize, QT_TR_NOOP("&Binarize..."), "",
                        "binarize");
  createMenuLevelAction(MI_BrightnessAndContrast,
                        QT_TR_NOOP("&Brightness and Contrast..."), "",
                        "brightness_contrast");
  createMenuLevelAction(MI_LinesFade, QT_TR_NOOP("&Color Fade..."), "",
                        "colorfade");
  menuAct = createMenuLevelAction(MI_CanvasSize, QT_TR_NOOP("&Canvas Size..."),
                                  "", "resize");
  menuAct->setDisabled(true);
  createMenuLevelAction(MI_FileInfo, QT_TR_NOOP("&Info..."), "", "info");
  createMenuLevelAction(MI_RemoveUnused,
                        QT_TR_NOOP("&Remove All Unused Levels"), "",
                        "remove_unused_levels");
  createMenuLevelAction(MI_ReplaceParentDirectory,
                        QT_TR_NOOP("&Replace Parent Directory..."), "",
                        "replace_parent_directory");
  createMenuLevelAction(MI_NewNoteLevel, QT_TR_NOOP("New Note Level"), "",
                        "new_note_level");
  createMenuLevelAction(MI_ConvertToVectors,
                        QT_TR_NOOP("Convert to Vectors..."), "", "convert");
  createMenuLevelAction(MI_ConvertToToonzRaster,
                        QT_TR_NOOP("Vectors to Toonz Raster"), "");
  createMenuLevelAction(
      MI_ConvertVectorToVector,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Replace Vectors with Simplified Vectors"),
      "");
  createMenuLevelAction(MI_Tracking, QT_TR_NOOP("Tracking..."), "", "focus");

  // Menu - Xsheet

  createMenuXsheetAction(MI_SceneSettings, QT_TR_NOOP("&Scene Settings..."), "",
                         "scene_settings");
  createMenuXsheetAction(MI_CameraSettings, QT_TR_NOOP("&Camera Settings..."),
                         "", "camera_settings");
  createMenuXsheetAction(MI_OpenChild, QT_TR_NOOP("&Open Sub-Xsheet"), "",
                         "sub_enter");
  createMenuXsheetAction(MI_CloseChild, QT_TR_NOOP("&Close Sub-Xsheet"), "",
                         "sub_leave");
  createMenuXsheetAction(MI_ExplodeChild, QT_TR_NOOP("Explode Sub-Xsheet"), "",
                         "sub_explode");
  createMenuXsheetAction(MI_Collapse, QT_TR_NOOP("Collapse"), "",
                         "sub_collapse");
  createToggle(MI_ToggleEditInPlace, QT_TR_NOOP("&Toggle Edit In Place"), "",
               EditInPlaceToggleAction ? 1 : 0, MenuXsheetCommandType,
               "sub_edit_in_place");
  createMenuXsheetAction(MI_SaveSubxsheetAs,
                         QT_TR_NOOP("&Save Sub-Xsheet As..."), "",
                         "sub_xsheet_saveas");
  createMenuXsheetAction(MI_Resequence, QT_TR_NOOP("Resequence"), "",
                         "resequence");
  createMenuXsheetAction(MI_CloneChild, QT_TR_NOOP("Clone Sub-Xsheet"), "",
                         "sub_clone");
  createMenuXsheetAction(MI_ApplyMatchLines,
                         QT_TR_NOOP("&Apply Match Lines..."), "",
                         "apply_match_lines");
  createMenuXsheetAction(MI_MergeCmapped, QT_TR_NOOP("&Merge Tlv Levels..."),
                         "", "merge_levels_tlv");
  createMenuXsheetAction(MI_DeleteMatchLines, QT_TR_NOOP("&Delete Match Lines"),
                         "", "delete_match_lines");
  createMenuXsheetAction(MI_DeleteInk, QT_TR_NOOP("&Delete Lines..."), "",
                         "delete_lines");
  createMenuXsheetAction(MI_MergeColumns, QT_TR_NOOP("&Merge Levels"), "",
                         "merge_levels");
  createMenuXsheetAction(MI_InsertFx, QT_TR_NOOP("&New FX..."), "Ctrl+F",
                         "fx_logo");
  createMenuXsheetAction(MI_NewOutputFx, QT_TR_NOOP("&New Output"), "Alt+O",
                         "output");
  createMenuXsheetAction(MI_InsertSceneFrame, QT_TR_NOOP("Insert Frame"), "",
                         "insert_frame");
  createMenuXsheetAction(MI_RemoveSceneFrame, QT_TR_NOOP("Remove Frame"), "",
                         "remove_frame");
  createMenuXsheetAction(MI_InsertGlobalKeyframe,
                         QT_TR_NOOP("Insert Multiple Keys"), "",
                         "insert_multiple_keys");
  createMenuXsheetAction(MI_RemoveGlobalKeyframe,
                         QT_TR_NOOP("Remove Multiple Keys"), "",
                         "remove_multiple_keys");
  createMenuXsheetAction(MI_RemoveEmptyColumns,
                         QT_TR_NOOP("Remove Empty Columns"), "",
                         "remove_empty_columns");
  createMenuXsheetAction(MI_LipSyncPopup,
                         QT_TR_NOOP("&Apply Lip Sync Data to Column"), "Alt+L",
                         "dialogue");
  createMenuXsheetAction(MI_AutoLipSyncPopup,
                         QT_TR_NOOP("&Apply Auto Lip Sync to Column"),
                         "Ctrl+Alt+L", "dialogue");

  createMenuXsheetAction(MI_ToggleTaggedFrame,
                         QT_TR_NOOP("Toggle Navigation Tag"), "",
                         "toggle_nav_tag");
  createMenuXsheetAction(MI_NextTaggedFrame, QT_TR_NOOP("Next Tag"), "",
                         "next_nav_tag");
  createMenuXsheetAction(MI_PrevTaggedFrame, QT_TR_NOOP("Previous Tag"), "",
                         "prev_nav_tag");
  createMenuXsheetAction(MI_EditTaggedFrame, QT_TR_NOOP("Edit Tag"), "", "");
  createMenuXsheetAction(MI_ClearTags, QT_TR_NOOP("Remove Tags"), "", "");
  CommandManager::instance()->enable(MI_NextTaggedFrame, false);
  CommandManager::instance()->enable(MI_PrevTaggedFrame, false);
  CommandManager::instance()->enable(MI_EditTaggedFrame, false);
  CommandManager::instance()->enable(MI_ClearTags, false);

  // Menu - Cells

  createMenuCellsAction(MI_MergeFrames, QT_TR_NOOP("&Merge"), "", "merge");
  createMenuCellsAction(MI_Reverse, QT_TR_NOOP("&Reverse"), "", "reverse");
  createMenuCellsAction(MI_Swing, QT_TR_NOOP("&Swing"), "", "swing");
  createMenuCellsAction(MI_Random, QT_TR_NOOP("&Random"), "", "random");
  createMenuCellsAction(MI_Increment, QT_TR_NOOP("&Autoexpose"), "",
                        "autoexpose");
  createMenuCellsAction(MI_Dup, QT_TR_NOOP("&Repeat..."), "", "repeat");
  createMenuCellsAction(MI_ResetStep, QT_TR_NOOP("&Reset Step"), "",
                        "step_reset");
  createMenuCellsAction(MI_IncreaseStep, QT_TR_NOOP("&Increase Step"), "'",
                        "step_plus");
  createMenuCellsAction(MI_DecreaseStep, QT_TR_NOOP("&Decrease Step"), ";",
                        "step_minus");
  createMenuCellsAction(MI_Step2, QT_TR_NOOP("&Step 2"), "", "step_2");
  createMenuCellsAction(MI_Step3, QT_TR_NOOP("&Step 3"), "", "step_3");
  createMenuCellsAction(MI_Step4, QT_TR_NOOP("&Step 4"), "", "step_4");
  createMenuCellsAction(MI_Each2, QT_TR_NOOP("&Each 2"), "");
  createMenuCellsAction(MI_Each3, QT_TR_NOOP("&Each 3"), "");
  createMenuCellsAction(MI_Each4, QT_TR_NOOP("&Each 4"), "");
  createMenuCellsAction(MI_Rollup, QT_TR_NOOP("&Roll Up"), "", "rollup");
  createMenuCellsAction(MI_Rolldown, QT_TR_NOOP("&Roll Down"), "", "rolldown");
  createMenuCellsAction(MI_TimeStretch, QT_TR_NOOP("&Time Stretch..."), "",
                        "time_stretch");
  createMenuCellsAction(MI_CreateBlankDrawing,
                        QT_TR_NOOP("&Create Blank Drawing"), "Alt+D",
                        "add_cell");
  createMenuCellsAction(MI_Duplicate, QT_TR_NOOP("&Duplicate Drawing  "), "D",
                        "duplicate_drawing");
  createMenuCellsAction(MI_Autorenumber, QT_TR_NOOP("&Autorenumber"), "",
                        "renumber");
  createMenuCellsAction(MI_CloneLevel, QT_TR_NOOP("&Clone Cells"), "",
                        "clone_cells");
  createMenuCellsAction(MI_DrawingSubForward,
                        QT_TR_NOOP("Drawing Substitution Forward"), "W");
  createMenuCellsAction(MI_DrawingSubBackward,
                        QT_TR_NOOP("Drawing Substitution Backward"), "Q");
  createMenuCellsAction(MI_DrawingSubGroupForward,
                        QT_TR_NOOP("Similar Drawing Substitution Forward"),
                        "Alt+W");
  createMenuCellsAction(MI_DrawingSubGroupBackward,
                        QT_TR_NOOP("Similar Drawing Substitution Backward"),
                        "Alt+Q");
  createMenuCellsAction(MI_Reframe1, QT_TR_NOOP("Reframe on 1's"), "", "on_1s");
  createMenuCellsAction(MI_Reframe2, QT_TR_NOOP("Reframe on 2's"), "", "on_2s");
  createMenuCellsAction(MI_Reframe3, QT_TR_NOOP("Reframe on 3's"), "", "on_3s");
  createMenuCellsAction(MI_Reframe4, QT_TR_NOOP("Reframe on 4's"), "", "on_4s");
  createMenuCellsAction(MI_ReframeWithEmptyInbetweens,
                        QT_TR_NOOP("Reframe with Empty Inbetweens..."), "",
                        "on_with_empty");
  createMenuCellsAction(MI_AutoInputCellNumber,
                        QT_TR_NOOP("Auto Input Cell Number..."), "",
                        "auto_input_cell_number");
  createMenuCellsAction(MI_FillEmptyCell, QT_TR_NOOP("&Fill In Empty Cells"),
                        "", "fill_empty_cells");

  // Menu - Play

  createToggle(MI_Link, QT_TR_NOOP("Link Flipbooks"), "",
               LinkToggleAction ? 1 : 0, MenuPlayCommandType, "flipbook_link");
  createMenuPlayAction(MI_Play, QT_TR_NOOP("Play"), "P", "play");
  createMenuPlayAction(MI_ShortPlay, QT_TR_NOOP("Short Play"), "Alt+P");
  createMenuPlayAction(MI_Loop, QT_TR_NOOP("Loop"), "L", "loop");
  createMenuPlayAction(MI_Pause, QT_TR_NOOP("Pause"), "", "pause");
  createMenuPlayAction(MI_FirstFrame, QT_TR_NOOP("First Frame"), "Alt+,",
                       "framefirst");
  createMenuPlayAction(MI_LastFrame, QT_TR_NOOP("Last Frame"), "Alt+.",
                       "framelast");
  createMenuPlayAction(MI_PrevFrame, QT_TR_NOOP("Previous Frame"), "Shift+,",
                       "frameprev");
  createMenuPlayAction(MI_NextFrame, QT_TR_NOOP("Next Frame"), "Shift+.",
                       "framenext");
  createMenuPlayAction(MI_NextDrawing, QT_TR_NOOP("Next Drawing"), ".",
                       "next_drawing");
  createMenuPlayAction(MI_PrevDrawing, QT_TR_NOOP("Previous Drawing"), ",",
                       "prev_drawing");
  createMenuPlayAction(MI_NextStep, QT_TR_NOOP("Next Step"), "", "nextstep");
  createMenuPlayAction(MI_PrevStep, QT_TR_NOOP("Previous Step"), "",
                       "prevstep");
  createMenuPlayAction(MI_NextKeyframe, QT_TR_NOOP("Next Key"), "Ctrl+.",
                       "nextkey");
  createMenuPlayAction(MI_PrevKeyframe, QT_TR_NOOP("Previous Key"), "Ctrl+,",
                       "prevkey");
  createMenuPlayAction(MI_ToggleBlankFrames, QT_TR_NOOP("Toggle Blank Frames"),
                       "", "blankframes");

  // Menu - Render

  createMenuRenderAction(MI_OutputSettings, QT_TR_NOOP("&Output Settings..."),
                         "Ctrl+O", "output_settings");
  createMenuRenderAction(MI_PreviewSettings, QT_TR_NOOP("&Preview Settings..."),
                         "", "preview_settings");
  createMenuRenderAction(MI_Render, QT_TR_NOOP("&Render"), "Ctrl+Shift+R",
                         "render");
  createMenuRenderAction(MI_FastRender, QT_TR_NOOP("&Fast Render to MP4"),
                         "Alt+R", "fast_render_mp4");
  createMenuRenderAction(MI_Preview, QT_TR_NOOP("&Preview"), "Ctrl+R",
                         "preview");
  createMenuRenderAction(MI_SavePreviewedFrames,
                         QT_TR_NOOP("&Save Previewed Frames"), "",
                         "save_previewed_frames");
  createToggle(MI_ToggleViewerPreview, QT_TR_NOOP("Toggle Viewer Preview"), "",
               false, MenuRenderCommandType, "pane_preview");
  createToggle(MI_ToggleViewerSubCameraPreview,
               QT_TR_NOOP("Toggle Viewer Sub-camera Preview"), "", false,
               MenuRenderCommandType, "pane_subpreview");

  createRightClickMenuAction(MI_OpenPltGizmo, QT_TR_NOOP("&Palette Gizmo"), "",
                             "palettegizmo");
  createRightClickMenuAction(MI_EraseUnusedStyles,
                             QT_TR_NOOP("&Delete Unused Styles"), "",
                             "delete_unused_styles");

  // Menu - View

  createToggle(MI_ViewCamera, QT_TR_NOOP("&Camera Box"), "",
               ViewCameraToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewTable, QT_TR_NOOP("&Table"), "",
               ViewTableToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_FieldGuide, QT_TR_NOOP("&Field Guide"), "Shift+G",
               FieldGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewBBox, QT_TR_NOOP("&Raster Bounding Box"), "",
               ViewBBoxToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_SafeArea, QT_TR_NOOP("&Safe Area"), "",
               SafeAreaToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewColorcard, QT_TR_NOOP("&Camera BG Color"), "",
               ViewColorcardToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewGuide, QT_TR_NOOP("&Guide"), "",
               ViewGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewRuler, QT_TR_NOOP("&Ruler"), "",
               ViewRulerToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_TCheck, QT_TR_NOOP("&Transparency Check  "), "",
               TCheckToggleAction ? 1 : 0, MenuViewCommandType,
               "transparency_check");
  menuAct = createToggle(MI_ICheck, QT_TR_NOOP("&Ink Check"), "",
                         ICheckToggleAction ? 1 : 0, MenuViewCommandType,
                         "ink_check");
  // make InkCheck and Ink1Check exclusive
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(onInkCheckTriggered(bool)));
  menuAct = createToggle(MI_Ink1Check, QT_TR_NOOP("&Ink#1 Check"), "",
                         Ink1CheckToggleAction ? 1 : 0, MenuViewCommandType,
                         "ink_no1_check");
  // make InkCheck and Ink1Check exclusive
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(onInk1CheckTriggered(bool)));
  createToggle(MI_PCheck, QT_TR_NOOP("&Paint Check"), "",
               PCheckToggleAction ? 1 : 0, MenuViewCommandType, "paint_check");
  createToggle(MI_IOnly, QT_TR_NOOP("Inks &Only"), "",
               IOnlyToggleAction ? 1 : 0, MenuViewCommandType, "inks_only");
  createToggle(MI_GCheck, QT_TR_NOOP("&Fill Check"), "",
               GCheckToggleAction ? 1 : 0, MenuViewCommandType, "fill_check");
  createToggle(MI_BCheck, QT_TR_NOOP("&Black BG Check"), "",
               BCheckToggleAction ? 1 : 0, MenuViewCommandType,
               "blackbg_check");
  createToggle(MI_ACheck, QT_TR_NOOP("&Gap Check"), "",
               ACheckToggleAction ? 1 : 0, MenuViewCommandType, "gap_check");
  createToggle(MI_ShiftTrace, QT_TR_NOOP("Shift and Trace"), "", false,
               MenuViewCommandType, "shift_and_trace");
  createToggle(MI_EditShift, QT_TR_NOOP("Edit Shift"), "", false,
               MenuViewCommandType, "shift_and_trace_edit");
  createToggle(MI_NoShift, QT_TR_NOOP("No Shift"), "", false,
               MenuViewCommandType, "shift_and_trace_no_shift");
  CommandManager::instance()->enable(MI_EditShift, false);
  CommandManager::instance()->enable(MI_NoShift, false);
  createAction(MI_ResetShift, QT_TR_NOOP("Reset Shift"), "",
               MenuViewCommandType, "shift_and_trace_reset");
  createToggle(MI_VectorGuidedDrawing, QT_TR_NOOP("Vector Guided Drawing"), "",
               Preferences::instance()->isGuidedDrawingEnabled(),
               MenuViewCommandType, "view_guided_drawing");
  if (QGLPixelBuffer::hasOpenGLPbuffers())
    createToggle(MI_RasterizePli, QT_TR_NOOP("&Visualize Vector As Raster"), "",
                 RasterizePliToggleAction ? 1 : 0, MenuViewCommandType,
                 "view_vector_as_raster");
  else
    RasterizePliToggleAction = 0;

  // Menu - Windows

  createMenuWindowsAction(MI_OpenFileBrowser, QT_TR_NOOP("&File Browser"), "",
                          "filebrowser");
  createMenuWindowsAction(MI_OpenPreproductionBoard,
                          QT_TR_NOOP("&Preproduction Board"), "", "");
  createMenuWindowsAction(MI_OpenFileViewer, QT_TR_NOOP("&Flipbook"), "",
                          "flipbook");
  createMenuWindowsAction(MI_OpenFunctionEditor, QT_TR_NOOP("&Function Editor"),
                          "", "function_editor");
  createMenuWindowsAction(MI_OpenFilmStrip, QT_TR_NOOP("&Level Strip"), "",
                          "level_strip");
  createMenuWindowsAction(MI_OpenPalette, QT_TR_NOOP("&Palette"), "",
                          "palette");
  createMenuWindowsAction(MI_OpenTasks, QT_TR_NOOP("&Tasks"), "", "tasks");
  createMenuWindowsAction(MI_OpenBatchServers, QT_TR_NOOP("&Batch Servers"), "",
                          "batchservers");
  createMenuWindowsAction(MI_OpenTMessage, QT_TR_NOOP("&Message Center"), "",
                          "messagecenter");
  createMenuWindowsAction(MI_OpenColorModel, QT_TR_NOOP("&Color Model"), "",
                          "colormodel");
  createMenuWindowsAction(MI_OpenStudioPalette, QT_TR_NOOP("&Studio Palette"),
                          "", "studiopalette");
  createMenuWindowsAction(MI_OpenSchematic, QT_TR_NOOP("&Schematic"), "",
                          "schematic");
  createMenuWindowsAction(MI_FxParamEditor, QT_TR_NOOP("&FX Editor"), "Ctrl+K",
                          "fx_settings");
  createMenuWindowsAction(MI_OpenCleanupSettings,
                          QT_TR_NOOP("&Cleanup Settings"), "",
                          "cleanup_settings");
  createMenuWindowsAction(MI_OpenFileBrowser2, QT_TR_NOOP("&Scene Cast"), "",
                          "scenecast");
  createMenuWindowsAction(MI_OpenStyleControl, QT_TR_NOOP("&Style Editor"), "",
                          "styleeditor");
  createMenuWindowsAction(MI_OpenToolbar, QT_TR_NOOP("&Toolbar"), "", "tool");
  createMenuWindowsAction(MI_OpenToolOptionBar, QT_TR_NOOP("&Tool Option Bar"),
                          "", "tool_options");
  createMenuWindowsAction(MI_OpenCommandToolbar, QT_TR_NOOP("&Command Bar"), "",
                          "star");
#if defined(x64)
  createMenuWindowsAction(MI_OpenStopMotionPanel,
                          QT_TR_NOOP("&Stop Motion Controls"), "");
#endif
  createMenuWindowsAction(MI_OpenLevelView, QT_TR_NOOP("&Viewer"), "",
                          "viewer");
  createMenuWindowsAction(MI_OpenXshView, QT_TR_NOOP("&Xsheet"), "", "xsheet");
  createMenuWindowsAction(MI_OpenTimelineView, QT_TR_NOOP("&Timeline"), "",
                          "timeline");
  createMenuWindowsAction(MI_OpenComboViewer, QT_TR_NOOP("&ComboViewer"), "",
                          "comboviewer");
  createMenuWindowsAction(MI_OpenHistoryPanel, QT_TR_NOOP("&History"), "Ctrl+H",
                          "history");
  createMenuWindowsAction(MI_AudioRecording, QT_TR_NOOP("Record Audio"),
                          "Alt+A", "recordaudio");
  createMenuWindowsAction(MI_ResetRoomLayout,
                          QT_TR_NOOP("&Reset to Default Rooms"), "");
  createMenuWindowsAction(MI_MaximizePanel, QT_TR_NOOP("Toggle Maximize Panel"),
                          "`", "fit_to_window");
  createMenuWindowsAction(MI_FullScreenWindow,
                          QT_TR_NOOP("Toggle Main Window's Full Screen Mode"),
                          "Ctrl+`", "toggle_fullscreen");
  createMenuWindowsAction(MI_SeeThroughWindow,
                          QT_TR_NOOP("Toggle Main Window's See Through Mode"),
                          "Alt+`", "toggle_seethroughwin_on");
  createMenuHelpAction(MI_About, QT_TR_NOOP("&About OpenToonz..."), "", "info");
  createMenuWindowsAction(MI_StartupPopup, QT_TR_NOOP("&Startup Popup..."),
                          "Alt+S", "opentoonz");
  createMenuWindowsAction(MI_OpenGuidedDrawingControls,
                          QT_TR_NOOP("Guided Drawing Controls"), "",
                          "guided_drawing");

  createMenuAction(MI_OpenCustomPanels, QT_TR_NOOP("&Custom Panels"), files,
                   false);
  createMenuWindowsAction(MI_CustomPanelEditor,
                          QT_TR_NOOP("&Custom Panel Editor..."), "", "");

  menuAct =
      createToggle(MI_DockingCheck, QT_TR_NOOP("&Lock Room Panes"), "",
                   DockingCheckToggleAction ? 1 : 0, MenuWindowsCommandType);
  DockingCheck::instance()->setToggle(menuAct);

  // Menu - Help

  createMenuHelpAction(MI_OpenOnlineManual, QT_TR_NOOP("&Online Manual..."),
                       "F1", "manual");
  createMenuHelpAction(MI_OpenWhatsNew, QT_TR_NOOP("&What's New..."), "",
                       "web");
  createMenuHelpAction(MI_OpenCommunityForum, QT_TR_NOOP("&Community Forum..."),
                       "", "web");
  createMenuHelpAction(MI_OpenReportABug, QT_TR_NOOP("&Report a Bug..."), "",
                       "web");

  // Fill

  createFillAction(
      MI_AutoFillToggle,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Toggle Autofill on Current Palette Color"),
      "Shift+A", "toggle_autofill");

  // Right Click

  createRightClickMenuAction(MI_SavePaletteAs,
                             QT_TR_NOOP("&Save Palette As..."), "", "saveas");
  createRightClickMenuAction(MI_OverwritePalette, QT_TR_NOOP("&Save Palette"),
                             "", "save");
  createRightClickMenuAction(MI_RegeneratePreview,
                             QT_TR_NOOP("&Regenerate Preview"), "");
  createRightClickMenuAction(MI_RegenerateFramePr,
                             QT_TR_NOOP("&Regenerate Frame Preview"), "");
  createRightClickMenuAction(MI_ClonePreview, QT_TR_NOOP("&Clone Preview"), "");
  createRightClickMenuAction(MI_FreezePreview,
                             QT_TR_NOOP("&Freeze//Unfreeze Preview"), "");
  CommandManager::instance()->setToggleTexts(
      MI_FreezePreview, tr("Freeze Preview"), tr("Unfreeze Preview"));
  createRightClickMenuAction(MI_SavePreset, QT_TR_NOOP("&Save As Preset"), "");
  createRightClickMenuAction(MI_PreviewFx, QT_TR_NOOP("Preview Fx"), "");
  createRightClickMenuAction(MI_PasteValues, QT_TR_NOOP("&Paste Color && Name"),
                             "", "paste_color_and_name");
  createRightClickMenuAction(MI_PasteColors, QT_TR_NOOP("Paste Color"), "",
                             "paste_color");
  createRightClickMenuAction(MI_PasteNames, QT_TR_NOOP("Paste Name"), "",
                             "paste_name");
  createRightClickMenuAction(MI_GetColorFromStudioPalette,
                             QT_TR_NOOP("Get Color from Studio Palette"), "");
  createRightClickMenuAction(MI_ToggleLinkToStudioPalette,
                             QT_TR_NOOP("Toggle Link to Studio Palette"), "");
  createRightClickMenuAction(MI_RemoveReferenceToStudioPalette,
                             QT_TR_NOOP("Remove Reference to Studio Palette"),
                             "");
  createRightClickMenuAction(MI_ViewFile, QT_TR_NOOP("&View..."), "",
                             "view_file");
  createRightClickMenuAction(MI_ToggleXSheetToolbar,
                             QT_TR_NOOP("Toggle XSheet Toolbar"), "");
  createRightClickMenuAction(MI_ToggleXsheetBreadcrumbs,
                             QT_TR_NOOP("Toggle Sub-Xsheet Navigation Bar"), "",
                             "toggle_sub_nav");
  createRightClickMenuAction(MI_ToggleXsheetCameraColumn,
                             QT_TR_NOOP("Show/Hide Xsheet Camera Column"), "");
  createRightClickMenuAction(MI_SetKeyframes, QT_TR_NOOP("&Set Key"), "Z",
                             "set_key");
  createRightClickMenuAction(MI_ShiftKeyframesDown,
                             QT_TR_NOOP("&Shift Keys Down"), "",
                             "shift_keys_down");
  createRightClickMenuAction(MI_ShiftKeyframesUp, QT_TR_NOOP("&Shift Keys Up"),
                             "", "shift_keys_up");
  createRightClickMenuAction(MI_PasteNumbers, QT_TR_NOOP("&Paste Numbers"), "",
                             "paste_numbers");
  createRightClickMenuAction(MI_PasteCellContent,
                             QT_TR_NOOP("&Paste Cell Content"), "", "paste");

  createRightClickMenuAction(MI_Histogram, QT_TR_NOOP("&Histogram"), "");
  // MI_ViewerHistogram command is used as a proxy. It will be called when
  // the MI_Histogram is used while the current flip console is in viewer.
  createAction(MI_ViewerHistogram, QT_TR_NOOP("&Viewer Histogram"), "",
               HiddenCommandType);

  createRightClickMenuAction(MI_BlendColors, QT_TR_NOOP("&Blend colors"), "");
  createToggle(MI_OnionSkin, QT_TR_NOOP("Onion Skin Toggle"), "/", false,
               RightClickMenuCommandType, "onionskin_toggle");
  createToggle(MI_ZeroThick, QT_TR_NOOP("Zero Thick Lines"), "Shift+/", false,
               RightClickMenuCommandType, "zero_thick_lines");
  createToggle(MI_CursorOutline, QT_TR_NOOP("Toggle Cursor Size Outline"), "",
               false, RightClickMenuCommandType);
  createRightClickMenuAction(MI_ToggleCurrentTimeIndicator,
                             QT_TR_NOOP("Toggle Current Time Indicator"), "");
  createRightClickMenuAction(MI_DuplicateFile, QT_TR_NOOP("Duplicate"), "",
                             "duplicate");
  createRightClickMenuAction(MI_ShowFolderContents,
                             QT_TR_NOOP("Show Folder Contents"), "",
                             "show_folder_contents");
  createRightClickMenuAction(MI_ConvertFiles, QT_TR_NOOP("Convert..."), "",
                             "convert");
  createRightClickMenuAction(MI_CollectAssets, QT_TR_NOOP("Collect Assets"), "",
                             "collect_assets");
  createRightClickMenuAction(MI_ImportScenes, QT_TR_NOOP("Import Scene"), "",
                             "load_scene");
  createRightClickMenuAction(MI_ExportScenes, QT_TR_NOOP("Export Scene..."), "",
                             "export_scene");

  createRightClickMenuAction(MI_RemoveLevel, QT_TR_NOOP("Remove Level"), "",
                             "remove_level");
  createRightClickMenuAction(MI_AddToBatchRenderList,
                             QT_TR_NOOP("Add As Render Task"), "", "new_scene");
  createRightClickMenuAction(MI_AddToBatchCleanupList,
                             QT_TR_NOOP("Add As Cleanup Task"), "",
                             "cleanup_add");
  createRightClickMenuAction(MI_SelectRowKeyframes,
                             QT_TR_NOOP("Select All Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectColumnKeyframes,
                             QT_TR_NOOP("Select All Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectAllKeyframes,
                             QT_TR_NOOP("Select All Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotBefore,
                             QT_TR_NOOP("Select All Following Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotAfter,
                             QT_TR_NOOP("Select All Previous Keys"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInColumn,
                             QT_TR_NOOP("Select Previous Keys in this Column"),
                             "");
  createRightClickMenuAction(MI_SelectFollowingKeysInColumn,
                             QT_TR_NOOP("Select Following Keys in this Column"),
                             "");
  createRightClickMenuAction(MI_SelectPreviousKeysInRow,
                             QT_TR_NOOP("Select Previous Keys in this Frame"),
                             "");
  createRightClickMenuAction(MI_SelectFollowingKeysInRow,
                             QT_TR_NOOP("Select Following Keys in this Frame"),
                             "");
  createRightClickMenuAction(MI_InvertKeyframeSelection,
                             QT_TR_NOOP("Invert Key Selection"), "");
  createRightClickMenuAction(MI_SetAcceleration, QT_TR_NOOP("Set Acceleration"),
                             "");
  createRightClickMenuAction(MI_SetDeceleration, QT_TR_NOOP("Set Deceleration"),
                             "");
  createRightClickMenuAction(MI_SetConstantSpeed,
                             QT_TR_NOOP("Set Constant Speed"), "");
  createRightClickMenuAction(MI_ResetInterpolation,
                             QT_TR_NOOP("Reset Interpolation"), "");
  createRightClickMenuAction(MI_UseLinearInterpolation,
                             QT_TR_NOOP("Linear Interpolation"), "");
  createRightClickMenuAction(MI_UseSpeedInOutInterpolation,
                             QT_TR_NOOP("Speed In / Speed Out Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseEaseInOutInterpolation,
                             QT_TR_NOOP("Ease In / Ease Out Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseEaseInOutPctInterpolation,
                             QT_TR_NOOP("Ease In / Ease Out (%) Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseExponentialInterpolation,
                             QT_TR_NOOP("Exponential Interpolation"), "");
  createRightClickMenuAction(MI_UseExpressionInterpolation,
                             QT_TR_NOOP("Expression Interpolation"), "");
  createRightClickMenuAction(MI_UseFileInterpolation,
                             QT_TR_NOOP("File Interpolation"), "");
  createRightClickMenuAction(MI_UseConstantInterpolation,
                             QT_TR_NOOP("Constant Interpolation"), "");
  createRightClickMenuAction(MI_FoldColumns, QT_TR_NOOP("Fold Column"), "",
                             "fold_column");
  createRightClickMenuAction(MI_ActivateThisColumnOnly,
                             QT_TR_NOOP("Show This Only"), "");
  createRightClickMenuAction(MI_ActivateSelectedColumns,
                             QT_TR_NOOP("Show Selected"), "");
  createRightClickMenuAction(MI_ActivateAllColumns, QT_TR_NOOP("Show All"), "");
  createRightClickMenuAction(MI_DeactivateSelectedColumns,
                             QT_TR_NOOP("Hide Selected"), "");
  createRightClickMenuAction(MI_DeactivateAllColumns, QT_TR_NOOP("Hide All"),
                             "");
  createRightClickMenuAction(MI_ToggleColumnsActivation,
                             QT_TR_NOOP("Toggle Show/Hide"), "");
  createRightClickMenuAction(MI_EnableThisColumnOnly,
                             QT_TR_NOOP("ON This Only"), "");
  createRightClickMenuAction(MI_EnableSelectedColumns,
                             QT_TR_NOOP("ON Selected"), "");
  createRightClickMenuAction(MI_EnableAllColumns, QT_TR_NOOP("ON All"), "");
  createRightClickMenuAction(MI_DisableAllColumns, QT_TR_NOOP("OFF All"), "");
  createRightClickMenuAction(MI_DisableSelectedColumns,
                             QT_TR_NOOP("OFF Selected"), "");
  createRightClickMenuAction(MI_SwapEnabledColumns, QT_TR_NOOP("Swap ON/OFF"),
                             "");
  createRightClickMenuAction(MI_LockThisColumnOnly,
                             QT_TR_NOOP("Lock This Only"), "Shift+L");
  createRightClickMenuAction(MI_LockSelectedColumns,
                             QT_TR_NOOP("Lock Selected"), "Ctrl+Shift+L");
  createRightClickMenuAction(MI_LockAllColumns, QT_TR_NOOP("Lock All"),
                             "Ctrl+Alt+Shift+L");
  createRightClickMenuAction(MI_UnlockSelectedColumns,
                             QT_TR_NOOP("Unlock Selected"), "Ctrl+Shift+U");
  createRightClickMenuAction(MI_UnlockAllColumns, QT_TR_NOOP("Unlock All"),
                             "Ctrl+Alt+Shift+U");
  createRightClickMenuAction(MI_ToggleColumnLocks,
                             QT_TR_NOOP("Swap Lock/Unlock"), "");
  createRightClickMenuAction(MI_DeactivateUpperColumns,
                             QT_TR_NOOP("Hide Upper Columns"), "");
  createRightClickMenuAction(MI_SeparateColors,
                             QT_TR_NOOP("Separate Colors..."), "",
                             "separate_colors");

  // Tools

  createToolAction(T_Edit, "animate", QT_TR_NOOP("Animate Tool"), "A");
  createToolAction(T_Selection, "selection", QT_TR_NOOP("Selection Tool"), "S");
  createToolAction(T_Brush, "brush", QT_TR_NOOP("Brush Tool"), "B");
  createToolAction(T_Geometric, "geometric", QT_TR_NOOP("Geometric Tool"), "G");
  createToolAction(T_Type, "type", QT_TR_NOOP("Type Tool"), "Y");
  createToolAction(T_Fill, "fill", QT_TR_NOOP("Fill Tool"), "F");
  createToolAction(T_PaintBrush, "paintbrush", QT_TR_NOOP("Paint Brush Tool"),
                   "");
  createToolAction(T_Eraser, "eraser", QT_TR_NOOP("Eraser Tool"), "E");
  createToolAction(T_Tape, "tape", QT_TR_NOOP("Tape Tool"), "T");
  createToolAction(T_StylePicker, "stylepicker",
                   QT_TR_NOOP("Style Picker Tool"), "K");
  createToolAction(T_RGBPicker, "rgbpicker", QT_TR_NOOP("RGB Picker Tool"),
                   "R");
  createToolAction(T_ControlPointEditor, "controlpointeditor",
                   QT_TR_NOOP("Control Point Editor Tool"), "C");
  createToolAction(T_Pinch, "pinch", QT_TR_NOOP("Pinch Tool"), "M");
  createToolAction(T_Pump, "pump", QT_TR_NOOP("Pump Tool"), "");
  createToolAction(T_Magnet, "magnet", QT_TR_NOOP("Magnet Tool"), "");
  createToolAction(T_Bender, "bender", QT_TR_NOOP("Bender Tool"), "");
  createToolAction(T_Iron, "iron", QT_TR_NOOP("Iron Tool"), "");

  createToolAction(T_Cutter, "cutter", QT_TR_NOOP("Cutter Tool"), "");
  createToolAction(T_Skeleton, "skeleton", QT_TR_NOOP("Skeleton Tool"), "V");
  createToolAction(T_Tracker, "radar", QT_TR_NOOP("Tracker Tool"), "");
  createToolAction(T_Hook, "hook", QT_TR_NOOP("Hook Tool"), "O");
  createToolAction(T_Zoom, "zoom", QT_TR_NOOP("Zoom Tool"), "Shift+Space");
  createToolAction(T_Rotate, "rotate", QT_TR_NOOP("Rotate Tool"), "Ctrl+Space");
  createToolAction(T_Hand, "hand", QT_TR_NOOP("Hand Tool"), "Space");
  createToolAction(T_Plastic, "plastic", QT_TR_NOOP("Plastic Tool"), "X");
  createToolAction(T_Ruler, "ruler", QT_TR_NOOP("Ruler Tool"), "");
  createToolAction(T_Finger, "finger", QT_TR_NOOP("Finger Tool"), "");
  createToolAction(T_EditAssistants, "assistant", QT_TR_NOOP("Edit Assistants"),
                   "");

  /*-- Animate tool + mode switching shortcuts --*/
  createAction(MI_EditNextMode, QT_TR_NOOP("Animate Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_EditPosition, QT_TR_NOOP("Animate Tool - Position"), "",
               ToolCommandType, "edit_position");
  createAction(MI_EditRotation, QT_TR_NOOP("Animate Tool - Rotation"), "",
               ToolCommandType, "edit_rotation");
  createAction(MI_EditScale, QT_TR_NOOP("Animate Tool - Scale"), "",
               ToolCommandType, "edit_scale");
  createAction(MI_EditShear, QT_TR_NOOP("Animate Tool - Shear"), "",
               ToolCommandType, "edit_shear");
  createAction(MI_EditCenter, QT_TR_NOOP("Animate Tool - Center"), "",
               ToolCommandType, "edit_center");
  createAction(MI_EditAll, QT_TR_NOOP("Animate Tool - All"), "",
               ToolCommandType, "edit_all");

  /*-- Selection tool + type switching shortcuts --*/
  createAction(MI_SelectionNextType, QT_TR_NOOP("Selection Tool - Next Type"),
               "", ToolCommandType);
  createAction(MI_SelectionRectangular,
               QT_TR_NOOP("Selection Tool - Rectangular"), "", ToolCommandType,
               "selection_rectangular");
  createAction(MI_SelectionFreehand, QT_TR_NOOP("Selection Tool - Freehand"),
               "", ToolCommandType, "selection_freehand");
  createAction(MI_SelectionPolyline, QT_TR_NOOP("Selection Tool - Polyline"),
               "", ToolCommandType, "selection_polyline");

  /*-- Geometric tool + shape switching shortcuts --*/
  createAction(MI_GeometricNextShape, QT_TR_NOOP("Geometric Tool - Next Shape"),
               "", ToolCommandType);
  createAction(MI_GeometricRectangle, QT_TR_NOOP("Geometric Tool - Rectangle"),
               "", ToolCommandType, "geometric_rectangle");
  createAction(MI_GeometricCircle, QT_TR_NOOP("Geometric Tool - Circle"), "",
               ToolCommandType, "geometric_circle");
  createAction(MI_GeometricEllipse, QT_TR_NOOP("Geometric Tool - Ellipse"), "",
               ToolCommandType, "geometric_ellipse");
  createAction(MI_GeometricLine, QT_TR_NOOP("Geometric Tool - Line"), "",
               ToolCommandType, "geometric_line");
  createAction(MI_GeometricPolyline, QT_TR_NOOP("Geometric Tool - Polyline"),
               "", ToolCommandType, "geometric_polyline");
  createAction(MI_GeometricArc, QT_TR_NOOP("Geometric Tool - Arc"), "",
               ToolCommandType, "geometric_arc");
  createAction(MI_GeometricMultiArc, QT_TR_NOOP("Geometric Tool - MultiArc"),
               "", ToolCommandType, "geometric_multiarc");
  createAction(MI_GeometricPolygon, QT_TR_NOOP("Geometric Tool - Polygon"), "",
               ToolCommandType, "geometric_polygon");

  /*-- Type tool + style switching shortcuts --*/
  createAction(MI_TypeNextStyle, QT_TR_NOOP("Type Tool - Next Style"), "",
               ToolCommandType);
  createAction(MI_TypeOblique, QT_TR_NOOP("Type Tool - Oblique"), "",
               ToolCommandType);
  createAction(MI_TypeRegular, QT_TR_NOOP("Type Tool - Regular"), "",
               ToolCommandType);
  createAction(MI_TypeBoldOblique, QT_TR_NOOP("Type Tool - Bold Oblique"), "",
               ToolCommandType);
  createAction(MI_TypeBold, QT_TR_NOOP("Type Tool - Bold"), "",
               ToolCommandType);

  /*-- Paint Brush tool + mode swicthing shortcuts --*/
  createAction(MI_PaintBrushNextMode, QT_TR_NOOP("Paint Brush - Next Mode"), "",
               ToolCommandType);
  createAction(MI_PaintBrushAreas, QT_TR_NOOP("Paint Brush - Areas"), "",
               ToolCommandType, "paintbrush_mode_areas");
  createAction(MI_PaintBrushLines, QT_TR_NOOP("Paint Brush - Lines"), "",
               ToolCommandType, "paintbrush_mode_lines");
  createAction(MI_PaintBrushLinesAndAreas,
               QT_TR_NOOP("Paint Brush - Lines & Areas"), "", ToolCommandType,
               "paintbrush_mode_lines_areas");

  /*-- Fill tool + type/mode switching shortcuts --*/
  createAction(MI_FillNextType, QT_TR_NOOP("Fill Tool - Next Type"), "",
               ToolCommandType);
  createAction(MI_FillNormal, QT_TR_NOOP("Fill Tool - Normal"), "",
               ToolCommandType, "fill_normal");
  createAction(MI_FillRectangular, QT_TR_NOOP("Fill Tool - Rectangular"), "",
               ToolCommandType, "fill_rectangular");
  createAction(MI_FillFreehand, QT_TR_NOOP("Fill Tool - Freehand"), "",
               ToolCommandType, "fill_freehand");
  createAction(MI_FillPolyline, QT_TR_NOOP("Fill Tool - Polyline"), "",
               ToolCommandType, "fill_polyline");
  createAction(MI_FillFreepick, QT_TR_NOOP("Fill Tool - Pick+Freehand"), "",
               ToolCommandType, "fill_freepick");
  createAction(MI_FillNextMode, QT_TR_NOOP("Fill Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_FillAreas, QT_TR_NOOP("Fill Tool - Areas"), "",
               ToolCommandType, "fill_mode_areas");
  createAction(MI_FillLines, QT_TR_NOOP("Fill Tool - Lines"), "",
               ToolCommandType, "fill_mode_lines");
  createAction(MI_FillLinesAndAreas, QT_TR_NOOP("Fill Tool - Lines & Areas"),
               "", ToolCommandType, "fill_mode_lines_areas");

  /*-- Eraser tool + type switching shortcuts --*/
  createAction(MI_EraserNextType, QT_TR_NOOP("Eraser Tool - Next Type"), "",
               ToolCommandType);
  createAction(MI_EraserNormal, QT_TR_NOOP("Eraser Tool - Normal"), "",
               ToolCommandType, "eraser_normal");
  createAction(MI_EraserRectangular, QT_TR_NOOP("Eraser Tool - Rectangular"),
               "", ToolCommandType, "eraser_rectangular");
  createAction(MI_EraserFreehand, QT_TR_NOOP("Eraser Tool - Freehand"), "",
               ToolCommandType, "eraser_freehand");
  createAction(MI_EraserPolyline, QT_TR_NOOP("Eraser Tool - Polyline"), "",
               ToolCommandType, "eraser_polyline");
  createAction(MI_EraserSegment, QT_TR_NOOP("Eraser Tool - Segment"), "",
               ToolCommandType, "eraser_segment");

  /*-- Tape tool + type/mode switching shortcuts --*/
  createAction(MI_TapeNextType, QT_TR_NOOP("Tape Tool - Next Type"), "",
               ToolCommandType);
  createAction(MI_TapeNormal, QT_TR_NOOP("Tape Tool - Normal"), "",
               ToolCommandType, "tape_normal");
  createAction(MI_TapeRectangular, QT_TR_NOOP("Tape Tool - Rectangular"), "",
               ToolCommandType, "tape_rectangular");
  createAction(MI_TapeNextMode, QT_TR_NOOP("Tape Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_TapeEndpointToEndpoint,
               QT_TR_NOOP("Tape Tool - Endpoint to Endpoint"), "",
               ToolCommandType, "tape_end_to_end");
  createAction(MI_TapeEndpointToLine,
               QT_TR_NOOP("Tape Tool - Endpoint to Line"), "", ToolCommandType,
               "tape_end_to_line");
  createAction(MI_TapeLineToLine, QT_TR_NOOP("Tape Tool - Line to Line"), "",
               ToolCommandType, "tape_line_to_line");

  /*-- Style Picker tool + mode switching shortcuts --*/
  createAction(MI_PickStyleNextMode,
               QT_TR_NOOP("Style Picker Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_PickStyleAreas, QT_TR_NOOP("Style Picker Tool - Areas"), "",
               ToolCommandType, "stylepicker_areas");
  createAction(MI_PickStyleLines, QT_TR_NOOP("Style Picker Tool - Lines"), "",
               ToolCommandType, "stylepicker_lines");
  createAction(MI_PickStyleLinesAndAreas,
               QT_TR_NOOP("Style Picker Tool - Lines & Areas"), "",
               ToolCommandType, "stylepicker_lines_areas");

  /*-- RGB Picker tool + type switching shortcuts --*/
  createAction(MI_RGBPickerNextType, QT_TR_NOOP("RGB Picker Tool - Next Type"),
               "", ToolCommandType);
  createAction(MI_RGBPickerNormal, QT_TR_NOOP("RGB Picker Tool - Normal"), "",
               ToolCommandType);
  createAction(MI_RGBPickerRectangular,
               QT_TR_NOOP("RGB Picker Tool - Rectangular"), "",
               ToolCommandType);
  createAction(MI_RGBPickerFreehand, QT_TR_NOOP("RGB Picker Tool - Freehand"),
               "", ToolCommandType);
  createAction(MI_RGBPickerPolyline, QT_TR_NOOP("RGB Picker Tool - Polyline"),
               "", ToolCommandType);

  /*-- Skeleton tool + mode switching shortcuts --*/
  createAction(MI_SkeletonNextMode, QT_TR_NOOP("Skeleton Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_SkeletonBuildSkeleton,
               QT_TR_NOOP("Skeleton Tool - Build Skeleton"), "",
               ToolCommandType);
  createAction(MI_SkeletonAnimate, QT_TR_NOOP("Skeleton Tool - Animate"), "",
               ToolCommandType);
  createAction(MI_SkeletonInverseKinematics,
               QT_TR_NOOP("Skeleton Tool - Inverse Kinematics"), "",
               ToolCommandType);

  /*-- Plastic tool + mode switching shortcuts --*/
  createAction(MI_PlasticNextMode, QT_TR_NOOP("Plastic Tool - Next Mode"), "",
               ToolCommandType);
  createAction(MI_PlasticEditMesh, QT_TR_NOOP("Plastic Tool - Edit Mesh"), "",
               ToolCommandType);
  createAction(MI_PlasticPaintRigid, QT_TR_NOOP("Plastic Tool - Paint Rigid"),
               "", ToolCommandType);
  createAction(MI_PlasticBuildSkeleton,
               QT_TR_NOOP("Plastic Tool - Build Skeleton"), "",
               ToolCommandType);
  createAction(MI_PlasticAnimate, QT_TR_NOOP("Plastic Tool - Animate"), "",
               ToolCommandType);

  // Tool Modifiers

  createToolOptionsAction(MI_SelectNextGuideStroke,
                          QT_TR_NOOP("Select Next Frame Guide Stroke"), "");
  createToolOptionsAction(MI_SelectPrevGuideStroke,
                          QT_TR_NOOP("Select Previous Frame Guide Stroke"), "");
  createToolOptionsAction(
      MI_SelectBothGuideStrokes,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Select Prev && Next Frame Guide Strokes"),
      "");
  createToolOptionsAction(MI_SelectGuideStrokeReset,
                          QT_TR_NOOP("Reset Guide Stroke Selections"), "");
  createToolOptionsAction(MI_TweenGuideStrokes,
                          QT_TR_NOOP("Tween Selected Guide Strokes"), "");
  createToolOptionsAction(MI_TweenGuideStrokeToSelected,
                          QT_TR_NOOP("Tween Guide Strokes to Selected"), "");
  createToolOptionsAction(MI_SelectGuidesAndTweenMode,
                          QT_TR_NOOP("Select Guide Strokes && Tween Mode"), "");
  createToolOptionsAction(MI_FlipNextGuideStroke,
                          QT_TR_NOOP("Flip Next Guide Stroke Direction"), "");
  createToolOptionsAction(MI_FlipPrevGuideStroke,
                          QT_TR_NOOP("Flip Previous Guide Stroke Direction"),
                          "");
  createToolOptionsAction("A_ToolOption_GlobalKey", QT_TR_NOOP("Global Key"),
                          "");

  createToolOptionsAction("A_IncreaseMaxBrushThickness",
                          QT_TR_NOOP("Brush size - Increase max"), "I");
  createToolOptionsAction("A_DecreaseMaxBrushThickness",
                          QT_TR_NOOP("Brush size - Decrease max"), "U");
  createToolOptionsAction("A_IncreaseMinBrushThickness",
                          QT_TR_NOOP("Brush size - Increase min"), "J");
  createToolOptionsAction("A_DecreaseMinBrushThickness",
                          QT_TR_NOOP("Brush size - Decrease min"), "H");
  createToolOptionsAction("A_IncreaseBrushHardness",
                          QT_TR_NOOP("Brush hardness - Increase"), "");
  createToolOptionsAction("A_DecreaseBrushHardness",
                          QT_TR_NOOP("Brush hardness - Decrease"), "");
  createToolOptionsAction("A_ToolOption_SnapSensitivity",
                          QT_TR_NOOP("Snap Sensitivity"), "");
  createToolOptionsAction("A_ToolOption_AutoGroup", QT_TR_NOOP("Auto Group"),
                          "");
  createToolOptionsAction("A_ToolOption_BreakSharpAngles",
                          QT_TR_NOOP("Break sharp angles"), "");
  createToolOptionsAction("A_ToolOption_FrameRange", QT_TR_NOOP("Frame range"),
                          "F6");
  createToolOptionsAction("A_ToolOption_IK", QT_TR_NOOP("Inverse Kinematics"),
                          "");
  createToolOptionsAction("A_ToolOption_Invert", QT_TR_NOOP("Invert"), "");
  createToolOptionsAction("A_ToolOption_Manual", QT_TR_NOOP("Manual"), "");
  createToolOptionsAction("A_ToolOption_OnionSkin", QT_TR_NOOP("Onion skin"),
                          "");
  createToolOptionsAction("A_ToolOption_Orientation", QT_TR_NOOP("Orientation"),
                          "");
  createToolOptionsAction("A_ToolOption_PencilMode", QT_TR_NOOP("Pencil Mode"),
                          "");
  createToolOptionsAction("A_ToolOption_PreserveThickness",
                          QT_TR_NOOP("Preserve Thickness"), "");
  createToolOptionsAction("A_ToolOption_PressureSensitivity",
                          QT_TR_NOOP("Pressure Sensitivity"), "Shift+P");
  createToolOptionsAction("A_ToolOption_SegmentInk", QT_TR_NOOP("Segment Ink"),
                          "F8");
  createToolOptionsAction("A_ToolOption_Selective", QT_TR_NOOP("Selective"),
                          "F7");
  createToolOptionsAction("A_ToolOption_DrawOrder",
                          QT_TR_NOOP("Brush Tool - Draw Order"), "");
  createToolOptionsAction("A_ToolOption_Smooth", QT_TR_NOOP("Smooth"), "");
  createToolOptionsAction("A_ToolOption_Snap", QT_TR_NOOP("Snap"), "");
  createToolOptionsAction("A_ToolOption_AutoSelectDrawing",
                          QT_TR_NOOP("Auto Select Drawing"), "");
  createToolOptionsAction("A_ToolOption_Autofill", QT_TR_NOOP("Auto Fill"), "");
  createToolOptionsAction("A_ToolOption_JoinVectors",
                          QT_TR_NOOP("Join Vectors"), "");
  createToolOptionsAction("A_ToolOption_ShowOnlyActiveSkeleton",
                          QT_TR_NOOP("Show Only Active Skeleton"), "");
  createToolOptionsAction("A_ToolOption_RasterEraser",
                          QT_TR_NOOP("Brush Tool - Eraser (Raster option)"),
                          "");
  createToolOptionsAction("A_ToolOption_LockAlpha",
                          QT_TR_NOOP("Brush Tool - Lock Alpha"), "");
  createToolOptionsAction("A_ToolOption_BrushPreset",
                          QT_TR_NOOP("Brush Preset"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape",
                          QT_TR_NOOP("Geometric Shape"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Rectangle",
                          QT_TR_NOOP("Geometric Shape Rectangle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Circle",
                          QT_TR_NOOP("Geometric Shape Circle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Ellipse",
                          QT_TR_NOOP("Geometric Shape Ellipse"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Line",
                          QT_TR_NOOP("Geometric Shape Line"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polyline",
                          QT_TR_NOOP("Geometric Shape Polyline"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Arc",
                          QT_TR_NOOP("Geometric Shape Arc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:MultiArc",
                          QT_TR_NOOP("Geometric Shape MultiArc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polygon",
                          QT_TR_NOOP("Geometric Shape Polygon"), "");
  createToolOptionsAction("A_ToolOption_GeometricEdge",
                          QT_TR_NOOP("Geometric Edge"), "");
  createToolOptionsAction("A_ToolOption_Mode", QT_TR_NOOP("Mode"), "");
  menuAct = createToolOptionsAction(
      "A_ToolOption_Mode:Areas", QT_TR_NOOP("Mode - Areas"), "", "mode_areas");
  menuAct = createToolOptionsAction(
      "A_ToolOption_Mode:Lines", QT_TR_NOOP("Mode - Lines"), "", "mode_lines");
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Lines & Areas",
                                    QT_TR_NOOP("Mode - Lines && Areas"), "",
                                    "mode_areas_lines");
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Endpoint",
                          QT_TR_NOOP("Mode - Endpoint to Endpoint"), "");
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Line",
                          QT_TR_NOOP("Mode - Endpoint to Line"), "");
  createToolOptionsAction("A_ToolOption_Mode:Line to Line",
                          QT_TR_NOOP("Mode - Line to Line"), "");
  createToolOptionsAction("A_ToolOption_Type", QT_TR_NOOP("Type"), "");

  menuAct =
      createToolOptionsAction("A_ToolOption_Type:Normal",
                              QT_TR_NOOP("Type - Normal"), "", "type_normal");
  menuAct = createToolOptionsAction("A_ToolOption_Type:Rectangular",
                                    QT_TR_NOOP("Type - Rectangular"), "F5",
                                    "type_rectangular");
  menuAct =
      createToolOptionsAction("A_ToolOption_Type:Freehand",
                              QT_TR_NOOP("Type - Freehand"), "", "type_lasso");
  menuAct = createToolOptionsAction("A_ToolOption_Type:Polyline",
                                    QT_TR_NOOP("Type - Polyline"), "",
                                    "type_polyline");
  menuAct = createToolOptionsAction("A_ToolOption_Type:Freepick",
                                    QT_TR_NOOP("Type - Pick+Freehand"), "",
                                    "type_pickerlasso");
  menuAct = createToolOptionsAction("A_ToolOption_Type:Segment",
                                    QT_TR_NOOP("Type - Segment"), "",
                                    "type_erase_segment");

  createToolOptionsAction("A_ToolOption_TypeFont", QT_TR_NOOP("TypeTool Font"),
                          "");
  createToolOptionsAction("A_ToolOption_TypeSize", QT_TR_NOOP("TypeTool Size"),
                          "");
  createToolOptionsAction("A_ToolOption_TypeStyle",
                          QT_TR_NOOP("TypeTool Style"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Oblique",
                          QT_TR_NOOP("TypeTool Style - Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Regular",
                          QT_TR_NOOP("TypeTool Style - Regular"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold Oblique",
                          QT_TR_NOOP("TypeTool Style - Bold Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold",
                          QT_TR_NOOP("TypeTool Style - Bold"), "");

  createToolOptionsAction("A_ToolOption_EditToolActiveAxis",
                          QT_TR_NOOP("Active Axis"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Position",
                          QT_TR_NOOP("Active Axis - Position"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Rotation",
                          QT_TR_NOOP("Active Axis - Rotation"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Scale",
                          QT_TR_NOOP("Active Axis - Scale"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Shear",
                          QT_TR_NOOP("Active Axis - Shear"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Center",
                          QT_TR_NOOP("Active Axis - Center"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:All",
                          QT_TR_NOOP("Active Axis - All"), "");

  createToolOptionsAction("A_ToolOption_SkeletonMode",
                          QT_TR_NOOP("Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Edit Mesh",
                          QT_TR_NOOP("Edit Mesh Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Paint Rigid",
                          QT_TR_NOOP("Paint Rigid Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Build Skeleton",
                          QT_TR_NOOP("Build Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Animate",
                          QT_TR_NOOP("Animate Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Inverse Kinematics",
                          QT_TR_NOOP("Inverse Kinematics Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:None",
                          QT_TR_NOOP("None Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Column",
                          QT_TR_NOOP("Column Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Pegbar",
                          QT_TR_NOOP("Pegbar Pick Mode"), "");
  menuAct = createToolOptionsAction("A_ToolOption_PickScreen",
                                    QT_TR_NOOP("Pick Screen"), "");
  menuAct->setIcon(createQIcon("pickscreen"));
  createToolOptionsAction("A_ToolOption_Meshify", QT_TR_NOOP("Create Mesh"),
                          "");

  menuAct =
      createToolOptionsAction("A_ToolOption_AutopaintLines",
                              QT_TR_NOOP("Fill Tool - Autopaint Lines"), "");
  menuAct->setIcon(createQIcon("toggle_autofill"));

  createToolOptionsAction("A_ToolOption_FlipHorizontal",
                          QT_TR_NOOP("Flip Selection/Object Horizontally"), "");
  createToolOptionsAction("A_ToolOption_FlipVertical",
                          QT_TR_NOOP("Flip Selection/Object Vertically"), "");
  createToolOptionsAction("A_ToolOption_RotateLeft",
                          QT_TR_NOOP("Rotate Selection/Object Left"), "");
  createToolOptionsAction("A_ToolOption_RotateRight",
                          QT_TR_NOOP("Rotate Selection/Object Right"), "");

  // Visualization

  createViewerAction(V_ZoomIn, QT_TR_NOOP("Zoom In"), "+");
  createViewerAction(V_ZoomOut, QT_TR_NOOP("Zoom Out"), "-");
  createViewerAction(V_ViewReset, QT_TR_NOOP("Reset View"), "Alt+0");
  createViewerAction(V_ZoomFit, QT_TR_NOOP("Fit to Window"), "Alt+9");
  createViewerAction(V_ZoomReset, QT_TR_NOOP("Reset Zoom"), "");
  createViewerAction(V_RotateReset, QT_TR_NOOP("Reset Rotation"), "");
  createViewerAction(V_PositionReset, QT_TR_NOOP("Reset Position"), "");

  createViewerAction(V_ActualPixelSize, QT_TR_NOOP("Actual Pixel Size"), "N");
  createViewerAction(V_FlipX, QT_TR_NOOP("Flip Viewer Horizontally"), "");
  createViewerAction(V_FlipY, QT_TR_NOOP("Flip Viewer Vertically"), "");
  createViewerAction(V_ShowHideFullScreen, QT_TR_NOOP("Show//Hide Full Screen"),
                     "Alt+F");
  CommandManager::instance()->setToggleTexts(V_ShowHideFullScreen,
                                             tr("Full Screen Mode"),
                                             tr("Exit Full Screen Mode"));
  createViewerAction(MI_CompareToSnapshot, QT_TR_NOOP("Compare to Snapshot"),
                     "");
  createViewerAction(MI_ZoomInAndFitPanel,
                     QT_TR_NOOP("Zoom In And Fit Floating Panel"),
                     "Ctrl+Alt++");
  createViewerAction(MI_ZoomOutAndFitPanel,
                     QT_TR_NOOP("Zoom Out And Fit Floating Panel"),
                     "Ctrl+Alt+-");

  // Following actions are for adding "Visualization" menu items to the command
  // bar. They are separated from the original actions in order to avoid
  // assigning shortcut keys. They must be triggered only from pressing buttons
  // in the command bar. Assigning shortcut keys and registering as MenuItem
  // will break a logic of ShortcutZoomer. So here we register separate items
  // and bypass the command.
  createVisualizationButtonAction(VB_ViewReset, QT_TR_NOOP("Reset View"),
                                  "reset");
  createVisualizationButtonAction(VB_ZoomFit, QT_TR_NOOP("Fit to Window"),
                                  "fit_to_window");
  createVisualizationButtonAction(VB_ZoomReset, QT_TR_NOOP("Reset Zoom"),
                                  "zoom_reset");
  createVisualizationButtonAction(VB_RotateReset, QT_TR_NOOP("Reset Rotation"),
                                  "rotate_reset");
  createVisualizationButtonAction(VB_PositionReset,
                                  QT_TR_NOOP("Reset Position"));
  createVisualizationButtonAction(
      VB_ActualPixelSize, QT_TR_NOOP("Actual Pixel Size"), "actual_pixel_size");
  createVisualizationButtonAction(
      VB_FlipX, QT_TR_NOOP("Flip Viewer Horizontally"), "fliphoriz");
  createVisualizationButtonAction(
      VB_FlipY, QT_TR_NOOP("Flip Viewer Vertically"), "flipvert");

  // Misc

  menuAct =
      createToggle(MI_TouchGestureControl, QT_TR_NOOP("&Touch Gesture Control"),
                   "", TouchGestureControl ? 1 : 0, MiscCommandType, "touch");
  menuAct->setEnabled(true);
  ;
  createMiscAction(MI_CameraStage, QT_TR_NOOP("&Camera Settings..."), "");
  menuAct =
      createMiscAction(MI_RefreshTree, QT_TR_NOOP("Refresh Folder Tree"), "");
  menuAct->setIconText(tr("Refresh"));
  createMiscAction("A_FxSchematicToggle",
                   QT_TR_NOOP("Toggle FX/Stage schematic"), "");

  // RGBA

  createRGBAAction(MI_RedChannel, QT_TR_NOOP("Red Channel"), "");
  createRGBAAction(MI_GreenChannel, QT_TR_NOOP("Green Channel"), "");
  createRGBAAction(MI_BlueChannel, QT_TR_NOOP("Blue Channel"), "");
  createRGBAAction(MI_MatteChannel, QT_TR_NOOP("Alpha Channel"), "");
  createRGBAAction(MI_RedChannelGreyscale, QT_TR_NOOP("Red Channel Greyscale"),
                   "");
  createRGBAAction(MI_GreenChannelGreyscale,
                   QT_TR_NOOP("Green Channel Greyscale"), "");
  createRGBAAction(MI_BlueChannelGreyscale,
                   QT_TR_NOOP("Blue Channel Greyscale"), "");

  // Stop Motion

#if defined(x64)
  createStopMotionAction(MI_StopMotionExportImageSequence,
                         QT_TR_NOOP("&Export Stop Motion Image Sequence"), "");
  createStopMotionAction(MI_StopMotionCapture,
                         QT_TR_NOOP("Capture Stop Motion Frame"), "");
  createStopMotionAction(MI_StopMotionRaiseOpacity,
                         QT_TR_NOOP("Raise Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionLowerOpacity,
                         QT_TR_NOOP("Lower Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionToggleLiveView,
                         QT_TR_NOOP("Toggle Stop Motion Live View"), "");
#ifdef WITH_CANON
  createStopMotionAction(MI_StopMotionToggleZoom,
                         QT_TR_NOOP("Toggle Stop Motion Zoom"), "");
  createStopMotionAction(MI_StopMotionPickFocusCheck,
                         QT_TR_NOOP("Pick Focus Check Location"), "");
#endif  // WITH_CANON
  createStopMotionAction(MI_StopMotionLowerSubsampling,
                         QT_TR_NOOP("Lower Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionRaiseSubsampling,
                         QT_TR_NOOP("Raise Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionJumpToCamera,
                         QT_TR_NOOP("Go to Stop Motion Insert Frame"), "");
  createStopMotionAction(MI_StopMotionRemoveFrame,
                         QT_TR_NOOP("Remove frame before Stop Motion Camera"),
                         "");
  createStopMotionAction(MI_StopMotionNextFrame,
                         QT_TR_NOOP("Next Frame including Stop Motion Camera"),
                         "");
  createStopMotionAction(MI_StopMotionToggleUseLiveViewImages,
                         QT_TR_NOOP("Show original live view images."), "");
#endif  // x64

  // create cell mark actions
  for (int markId = 0; markId < 12; markId++) {
    std::string cmdId = (std::string)MI_SetCellMark + std::to_string(markId);
    std::string labelStr =
        QT_TR_NOOP("Set Cell Mark ") + std::to_string(markId);
    QAction *action =
        createAction(cmdId.c_str(), labelStr.c_str(), "", CellMarkCommandType);
    action->setData(markId);
  }
}

//-----------------------------------------------------------------------------

void MainWindow::onInkCheckTriggered(bool on) {
  if (!on) return;
  QAction *ink1CheckAction =
      CommandManager::instance()->getAction(MI_Ink1Check);
  if (ink1CheckAction) ink1CheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------

void MainWindow::onInk1CheckTriggered(bool on) {
  if (!on) return;
  QAction *inkCheckAction = CommandManager::instance()->getAction(MI_ICheck);
  if (inkCheckAction) inkCheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------
// delete unused files / folders in the cache
void MainWindow::clearCacheFolder() {
  // currently cache folder is used for following purposes
  // 1. $CACHE/[ProcessID] : for disk swap of image cache.
  //    To be deleted on exit. Remains on crash.
  // 2. $CACHE/ffmpeg : ffmpeg cache.
  //    To be cleared on the end of rendering, on exist and on launch.
  // 3. $CACHE/temp : untitled scene data.
  //    To be deleted on switching or exiting scenes. Remains on crash.

  // So, this function will delete all files / folders in $CACHE
  // except the following items:
  // 1. $CACHE/[Current ProcessID]
  // 2. $CACHE/temp/[Current scene folder] if the current scene is untitled

  TFilePath cacheRoot = ToonzFolder::getCacheRootFolder();
  if (cacheRoot.isEmpty()) cacheRoot = TEnv::getStuffDir() + "cache";

  TFilePathSet filesToBeRemoved;

  TSystem::readDirectory(filesToBeRemoved, cacheRoot, false);

  // keep the imagecache folder
  filesToBeRemoved.remove(cacheRoot + std::to_string(TSystem::getProcessId()));
  // keep the untitled scene data folder
  if (TApp::instance()->getCurrentScene()->getScene()->isUntitled()) {
    filesToBeRemoved.remove(cacheRoot + "temp");
    TFilePathSet untitledData =
        TSystem::readDirectory(cacheRoot + "temp", false);
    untitledData.remove(TApp::instance()
                            ->getCurrentScene()
                            ->getScene()
                            ->getScenePath()
                            .getParentDir());
    filesToBeRemoved.insert(filesToBeRemoved.end(), untitledData.begin(),
                            untitledData.end());
  }

  // return if there is no files/folders to be deleted
  if (filesToBeRemoved.size() == 0) {
    QMessageBox::information(
        this, tr("Clear Cache Folder"),
        tr("There are no unused items in the cache folder."));
    return;
  }

  QString message(tr("Deleting the following items:\n"));
  int count = 0;
  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    QString dirPrefix =
        (TFileStatus(fileToBeRemoved).isDirectory()) ? tr("<DIR> ") : "";
    message +=
        "   " + dirPrefix + (fileToBeRemoved - cacheRoot).getQString() + "\n";
    count++;
    if (count == 5) break;
  }
  if (filesToBeRemoved.size() > 5)
    message +=
        tr("   ... and %1 more items\n").arg(filesToBeRemoved.size() - 5);

  message +=
      tr("\nAre you sure?\n\nN.B. Make sure you are not running another "
         "process of OpenToonz,\nor you may delete necessary files for it.");

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Clear Cache Folder"), message,
      QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel));

  if (ret != QMessageBox::Ok) return;

  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    try {
      if (TFileStatus(fileToBeRemoved).isDirectory())
        TSystem::rmDirTree(fileToBeRemoved);
      else
        TSystem::deleteFile(fileToBeRemoved);
    } catch (TException &e) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()) +
              QString::fromStdWString(e.getMessage()));
    } catch (...) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()));
    }
  }
}

//-----------------------------------------------------------------------------

void MainWindow::onNewMetaLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, META_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

class ReloadStyle final : public MenuItemHandler {
public:
  ReloadStyle() : MenuItemHandler("MI_ReloadStyle") {}
  void execute() override {
    QString currentStyle = Preferences::instance()->getCurrentStyleSheet();
    qApp->setStyleSheet(currentStyle);
  }
} reloadStyle;

void MainWindow::onQuit() { close(); }

//=============================================================================
// RecentFiles
//=============================================================================

RecentFiles::RecentFiles()
    : m_recentScenes(), m_recentSceneProjects(), m_recentLevels() {}

//-----------------------------------------------------------------------------

RecentFiles *RecentFiles::instance() {
  static RecentFiles _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

RecentFiles::~RecentFiles() {}

//-----------------------------------------------------------------------------

void RecentFiles::addFilePath(QString path, FileType fileType,
                              QString projectName) {
  QList<QString> files = (fileType == Scene)   ? m_recentScenes
                         : (fileType == Level) ? m_recentLevels
                                               : m_recentFlipbookImages;
  int i;
  for (i = 0; i < files.size(); i++)
    if (files.at(i) == path) {
      files.removeAt(i);
      if (fileType == Scene) m_recentSceneProjects.removeAt(i);
    }
  files.insert(0, path);
  if (fileType == Scene) m_recentSceneProjects.insert(0, projectName);
  int maxSize = 10;
  if (files.size() > maxSize) {
    files.removeAt(maxSize);
    if (fileType == Scene) m_recentSceneProjects.removeAt(maxSize);
  }

  if (fileType == Scene)
    m_recentScenes = files;
  else if (fileType == Level)
    m_recentLevels = files;
  else
    m_recentFlipbookImages = files;

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::moveFilePath(int fromIndex, int toIndex, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.move(fromIndex, toIndex);
    m_recentSceneProjects.move(fromIndex, toIndex);
  } else if (fileType == Level)
    m_recentLevels.move(fromIndex, toIndex);
  else
    m_recentFlipbookImages.move(fromIndex, toIndex);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::removeFilePath(int index, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.removeAt(index);
    m_recentSceneProjects.removeAt(index);
  } else if (fileType == Level)
    m_recentLevels.removeAt(index);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFilePath(int index, FileType fileType) const {
  return (fileType == Scene)   ? m_recentScenes[index]
         : (fileType == Level) ? m_recentLevels[index]
                               : m_recentFlipbookImages[index];
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFileProject(int index) const {
  if (index >= m_recentScenes.size() || index >= m_recentSceneProjects.size())
    return "-";
  return m_recentSceneProjects[index];
}

QString RecentFiles::getFileProject(QString fileName) const {
  for (int index = 0; index < m_recentScenes.size(); index++)
    if (m_recentScenes[index] == fileName) return m_recentSceneProjects[index];

  return "-";
}

//-----------------------------------------------------------------------------

void RecentFiles::clearRecentFilesList(FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.clear();
    m_recentSceneProjects.clear();
  } else if (fileType == Level)
    m_recentLevels.clear();
  else
    m_recentFlipbookImages.clear();

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::loadRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  int i;

  QList<QVariant> scenes = settings.value(QString("Scenes")).toList();
  if (!scenes.isEmpty()) {
    for (i = 0; i < scenes.size(); i++)
      m_recentScenes.append(scenes.at(i).toString());
  } else {
    QString scene = settings.value(QString("Scenes")).toString();
    if (!scene.isEmpty()) m_recentScenes.append(scene);
  }

  // Load scene's projects info. This is for display purposes only. For
  // backwards compatibility it is stored and maintained separately.
  QList<QVariant> sceneProjects =
      settings.value(QString("SceneProjects")).toList();
  if (!sceneProjects.isEmpty()) {
    for (i = 0; i < sceneProjects.size(); i++)
      m_recentSceneProjects.append(sceneProjects.at(i).toString());
  } else {
    QString sceneProject = settings.value(QString("SceneProjects")).toString();
    if (!sceneProject.isEmpty()) m_recentSceneProjects.append(sceneProject);
  }
  // Should be 1-to-1. If we're short, append projects list with "-".
  while (m_recentSceneProjects.size() < m_recentScenes.size())
    m_recentSceneProjects.append("-");

  QList<QVariant> levels = settings.value(QString("Levels")).toList();
  if (!levels.isEmpty()) {
    for (i = 0; i < levels.size(); i++) {
      QString path = levels.at(i).toString();
#ifdef x64
      if (path.endsWith(".mov") || path.endsWith(".3gp") ||
          path.endsWith(".pct") || path.endsWith(".pict"))
        continue;
#endif
      m_recentLevels.append(path);
    }
  } else {
    QString level = settings.value(QString("Levels")).toString();
    if (!level.isEmpty()) m_recentLevels.append(level);
  }

  QList<QVariant> flipImages =
      settings.value(QString("FlipbookImages")).toList();
  if (!flipImages.isEmpty()) {
    for (i = 0; i < flipImages.size(); i++)
      m_recentFlipbookImages.append(flipImages.at(i).toString());
  } else {
    QString flipImage = settings.value(QString("FlipbookImages")).toString();
    if (!flipImage.isEmpty()) m_recentFlipbookImages.append(flipImage);
  }

  refreshRecentFilesMenu(Scene);
  refreshRecentFilesMenu(Level);
  refreshRecentFilesMenu(Flip);
}

//-----------------------------------------------------------------------------

void RecentFiles::saveRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.setValue(QString("Scenes"), QVariant(m_recentScenes));
  settings.setValue(QString("SceneProjects"), QVariant(m_recentSceneProjects));
  settings.setValue(QString("Levels"), QVariant(m_recentLevels));
  settings.setValue(QString("FlipbookImages"),
                    QVariant(m_recentFlipbookImages));
}

//-----------------------------------------------------------------------------

QList<QString> RecentFiles::getFilesNameList(FileType fileType) {
  QList<QString> files = (fileType == Scene)   ? m_recentScenes
                         : (fileType == Level) ? m_recentLevels
                                               : m_recentFlipbookImages;
  QList<QString> names;
  int i;
  for (i = 0; i < files.size(); i++) {
    TFilePath path(files.at(i).toStdWString());
    QString str, number;
    names.append(number.number(i + 1) + QString(". ") +
                 str.fromStdWString(path.getWideString()));
  }
  return names;
}

//-----------------------------------------------------------------------------

void RecentFiles::refreshRecentFilesMenu(FileType fileType) {
  CommandId id = (fileType == Scene)   ? MI_OpenRecentScene
                 : (fileType == Level) ? MI_OpenRecentLevel
                                       : MI_LoadRecentImage;
  QAction *act = CommandManager::instance()->getAction(id);
  if (!act) return;
  DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
  if (!menu) return;
  QList<QString> names = getFilesNameList(fileType);
  if (names.isEmpty())
    menu->setEnabled(false);
  else {
    CommandId clearActionId = (fileType == Scene)   ? MI_ClearRecentScene
                              : (fileType == Level) ? MI_ClearRecentLevel
                                                    : MI_ClearRecentImage;
    menu->setActions(names);
    menu->addSeparator();
    QAction *clearAction = CommandManager::instance()->getAction(clearActionId);
    assert(clearAction);
    menu->addAction(clearAction);
    if (!menu->isEnabled()) menu->setEnabled(true);
  }
}
