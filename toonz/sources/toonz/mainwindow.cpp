

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

// TnzTools includes
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/updatechecker.h"
#include "toonzqt/paletteviewer.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/stage2.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "timagecache.h"
#include "tthread.h"

// Couldn't place it...
#ifdef LINETEST
#include "tnzcamera.h"
#endif

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

TEnv::IntVar ViewCameraToggleAction("ViewCameraToggleAction", 1);
TEnv::IntVar ViewTableToggleAction("ViewTableToggleAction", 1);
TEnv::IntVar FieldGuideToggleAction("FieldGuideToggleAction", 0);
TEnv::IntVar ViewBBoxToggleAction("ViewBBoxToggleAction1", 1);
TEnv::IntVar EditInPlaceToggleAction("EditInPlaceToggleAction", 0);
#ifdef LINETEST
TEnv::IntVar CapturePanelFieldGuideToggleAction(
    "CapturePanelFieldGuideToggleAction", 0);
#endif
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

#if QT_VERSION >= 0x050500
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WindowFlags flags)
#else
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WFlags flags)
#endif
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

  /*-- FillAreas,FillLinesに直接切り替えるコマンド --*/
  setCommandHandler(MI_FillAreas, this, &MainWindow::toggleFillAreas);
  setCommandHandler(MI_FillLines, this, &MainWindow::toggleFillLines);
  /*-- StylepickerAreas,StylepickerLinesに直接切り替えるコマンド --*/
  setCommandHandler(MI_PickStyleAreas, this, &MainWindow::togglePickStyleAreas);
  setCommandHandler(MI_PickStyleLines, this, &MainWindow::togglePickStyleLines);

  setCommandHandler(MI_About, this, &MainWindow::onAbout);
  setCommandHandler(MI_MaximizePanel, this, &MainWindow::maximizePanel);
  setCommandHandler(MI_FullScreenWindow, this, &MainWindow::fullScreenWindow);
  setCommandHandler("MI_NewVectorLevel", this,
                    &MainWindow::onNewVectorLevelButtonPressed);
  setCommandHandler("MI_NewToonzRasterLevel", this,
                    &MainWindow::onNewToonzRasterLevelButtonPressed);
  setCommandHandler("MI_NewRasterLevel", this,
                    &MainWindow::onNewRasterLevelButtonPressed);
  // remove ffmpegCache if still exists from crashed exit
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
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

  QString name = QString::fromStdWString(scene->getSceneName());

  /*--- レイアウトファイル名を頭に表示させる ---*/
  if (!m_layoutName.isEmpty()) name.prepend(m_layoutName + " : ");

  if (name.isEmpty()) name = tr("Untitled");

  name += " : " + QString::fromStdString(TEnv::getApplicationFullName());

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
      int pos = (argumentLayoutFileName.indexOf("_layout") == -1)
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
      cvp->setVisiblePartsFlag(CVPARTS_None);
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
    if (cvp) cvp->setVisiblePartsFlag(CVPARTS_TOOLBAR | CVPARTS_TOOLOPTIONS);
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
  bool ret = TUndoManager::manager()->undo();
  if (!ret) DVGui::error(QObject::tr("No more Undo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onRedo() {
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
  pixmap.setDevicePixelRatio(QApplication::desktop()->devicePixelRatio());
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

void MainWindow::autofillToggle() {
  TPaletteHandle *h = TApp::instance()->getCurrentPalette();
  h->toggleAutopaint();
}

void MainWindow::resetRoomsLayout() {
  if (!m_saveSettingsOnQuit) return;

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
      QObject::tr("The rooms will be reset the next time you run Toonz."));
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
  if (isFullScreen())
    setWindowState(Qt::WindowMaximized);
  else
    setWindowState(Qt::WindowFullScreen);
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
#ifdef LINETEST
  else if (cm->getAction(MI_CapturePanelFieldGuide) == action)
    CapturePanelFieldGuideToggleAction = isChecked;
#endif
  else if (cm->getAction(MI_RasterizePli) == action) {
    if (!QGLPixelBuffer::hasOpenGLPbuffers()) isChecked = 0;
    RasterizePliToggleAction                            = isChecked;
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
      Preferences::instance()->enableLatestVersionCheck(false);
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
#ifdef LINETEST
  if (TnzCamera::instance()->isCameraConnected())
    TnzCamera::instance()->cameraDisconnect();
#endif

  event->accept();
  TThread::shutdown();
  qApp->quit();
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createAction(const char *id, const QString &name,
                                  const QString &defaultShortcut,
                                  CommandType type) {
  QAction *action = new DVAction(name, this);
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
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRightClickMenuAction(
    const char *id, const QString &name, const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, RightClickMenuCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuFileAction(const char *id, const QString &name,
                                          const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuFileCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuEditAction(const char *id, const QString &name,
                                          const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuEditCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuScanCleanupAction(
    const char *id, const QString &name, const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuScanCleanupCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuLevelAction(const char *id, const QString &name,
                                           const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuLevelCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuXsheetAction(const char *id, const QString &name,
                                            const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuXsheetCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuCellsAction(const char *id, const QString &name,
                                           const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuCellsCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuViewAction(const char *id, const QString &name,
                                          const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuViewCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuWindowsAction(const char *id,
                                             const QString &name,
                                             const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, MenuWindowsCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createPlaybackAction(const char *id, const QString &name,
                                          const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, PlaybackCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRGBAAction(const char *id, const QString &name,
                                      const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, RGBACommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createFillAction(const char *id, const QString &name,
                                      const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, FillCommandType);
}
//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuAction(const char *id, const QString &name,
                                      QList<QString> list) {
  QMenu *menu     = new DVMenuAction(name, this, list);
  QAction *action = menu->menuAction();
  CommandManager::instance()->define(id, MenuCommandType, "", action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createViewerAction(const char *id, const QString &name,
                                        const QString &defaultShortcut) {
  return createAction(id, name, defaultShortcut, ZoomCommandType);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMiscAction(const char *id, const QString &name,
                                      const char *defaultShortcut) {
  QAction *action = new DVAction(name, this);
  CommandManager::instance()->define(id, MiscCommandType, defaultShortcut,
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolOptionsAction(const char *id,
                                             const QString &name,
                                             const QString &defaultShortcut) {
  QAction *action = new DVAction(name, this);
  addAction(action);
  CommandManager::instance()->define(id, ToolModifierCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToggle(const char *id, const QString &name,
                                  const QString &defaultShortcut,
                                  bool startStatus, CommandType type) {
  QAction *action = createAction(id, name, defaultShortcut, type);
  action->setCheckable(true);
  if (startStatus == true) action->trigger();
  bool ret =
      connect(action, SIGNAL(changed()), this, SLOT(onMenuCheckboxChanged()));
  assert(ret);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolAction(const char *id, const char *iconName,
                                      const QString &name,
                                      const QString &defaultShortcut) {
  QString normalResource = QString(":Resources/") + iconName + ".svg";
  QString overResource   = QString(":Resources/") + iconName + "_rollover.svg";
  QIcon icon;
  icon.addFile(normalResource, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(overResource, QSize(), QIcon::Normal, QIcon::On);
  icon.addFile(overResource, QSize(), QIcon::Active);
  QAction *action = new DVAction(icon, name, this);
  action->setCheckable(true);
  action->setActionGroup(m_toolsActionGroup);

  // When the viewer is maximized (not fullscreen) the toolbar is hided and the
  // action are disabled,
  // so the tool shortcuts don't work.
  // Adding tool actions to the main window solve this problem!
  addAction(action);

  CommandManager::instance()->define(id, ToolCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

void MainWindow::defineActions() {
  createMenuFileAction(MI_NewScene, tr("&New Scene"), "Ctrl+N");
  createMenuFileAction(MI_LoadScene, tr("&Load Scene..."), "Ctrl+L");
  createMenuFileAction(MI_SaveScene, tr("&Save Scene"), "Ctrl+Shift+S");
  createMenuFileAction(MI_SaveSceneAs, tr("&Save Scene As..."), "");
  createMenuFileAction(MI_SaveAll, tr("&Save All"), "Ctrl+S");
  createMenuFileAction(MI_RevertScene, tr("&Revert Scene"), "");

  QAction *act = CommandManager::instance()->getAction(MI_RevertScene);
  if (act) act->setEnabled(false);

  QList<QString> files;

  createMenuFileAction(MI_LoadFolder, tr("&Load Folder..."), "");
  createMenuFileAction(MI_LoadSubSceneFile, tr("&Load As Sub-xsheet..."), "");
  createMenuAction(MI_OpenRecentScene, tr("&Open Recent Scene File"), files);
  createMenuAction(MI_OpenRecentLevel, tr("&Open Recent Level File"), files);
  createMenuFileAction(MI_ClearRecentScene, tr("&Clear Recent Scene File List"),
                       "");
  createMenuFileAction(MI_ClearRecentLevel, tr("&Clear Recent level File List"),
                       "");
  createMenuFileAction(MI_NewLevel, tr("&New Level..."), "Alt+N");

  QAction *newVectorLevelAction =
      createMenuFileAction(MI_NewVectorLevel, tr("&New Vector Level"), "");
  newVectorLevelAction->setIconText(tr("New Vector Level"));
  newVectorLevelAction->setIcon(QIcon(":Resources/new_vector_level.svg"));
  QAction *newToonzRasterLevelAction = createMenuFileAction(
      MI_NewToonzRasterLevel, tr("&New Toonz Raster Level"), "");
  newToonzRasterLevelAction->setIconText(tr("New Toonz Raster Level"));
  newToonzRasterLevelAction->setIcon(
      QIcon(":Resources/new_toonz_raster_level.svg"));
  QAction *newRasterLevelAction =
      createMenuFileAction(MI_NewRasterLevel, tr("&New Raster Level"), "");
  newRasterLevelAction->setIconText(tr("New Raster Level"));
  newRasterLevelAction->setIcon(QIcon(":Resources/new_raster_level.svg"));
  QAction *loadLevelAction =
      createMenuFileAction(MI_LoadLevel, tr("&Load Level..."), "");
  loadLevelAction->setIcon(QIcon(":Resources/load_level.svg"));
  createMenuFileAction(MI_SaveLevel, tr("&Save Level"), "");
  createMenuFileAction(MI_SaveAllLevels, tr("&Save All Levels"), "");
  createMenuFileAction(MI_SaveLevelAs, tr("&Save Level As..."), "");
  createMenuFileAction(MI_ExportLevel, tr("&Export Level..."), "");
  createMenuFileAction(MI_ConvertFileWithInput, tr("&Convert File..."), "");
  createRightClickMenuAction(MI_SavePaletteAs, tr("&Save Palette As..."), "");
  createRightClickMenuAction(MI_OverwritePalette, tr("&Save Palette"), "");
  createMenuFileAction(MI_LoadColorModel, tr("&Load Color Model..."), "");
  createMenuFileAction(MI_ImportMagpieFile, tr("&Import Magpie File..."), "");
  createMenuFileAction(MI_NewProject, tr("&New Project..."), "");
  createMenuFileAction(MI_ProjectSettings, tr("&Project Settings..."), "");
  createMenuFileAction(MI_SaveDefaultSettings, tr("&Save Default Settings"),
                       "");
  createMenuFileAction(MI_OutputSettings, tr("&Output Settings..."), "Ctrl+O");
  createMenuFileAction(MI_PreviewSettings, tr("&Preview Settings..."), "");
  createMenuFileAction(MI_Render, tr("&Render"), "Ctrl+Shift+R");
  createMenuFileAction(MI_FastRender, tr("&Fast Render to MP4"), "Alt+R");
  createMenuFileAction(MI_Preview, tr("&Preview"), "Ctrl+R");
  createMenuFileAction(MI_SoundTrack, tr("&Export Soundtrack"), "");
  createRightClickMenuAction(MI_SavePreviewedFrames,
                             tr("&Save Previewed Frames"), "");
  createRightClickMenuAction(MI_RegeneratePreview, tr("&Regenerate Preview"),
                             "");
  createRightClickMenuAction(MI_RegenerateFramePr,
                             tr("&Regenerate Frame Preview"), "");
  createRightClickMenuAction(MI_ClonePreview, tr("&Clone Preview"), "");
  createRightClickMenuAction(MI_FreezePreview, tr("&Freeze//Unfreeze Preview"),
                             "");
  CommandManager::instance()->setToggleTexts(
      MI_FreezePreview, tr("Freeze Preview"), tr("Unfreeze Preview"));
  // createAction(MI_SavePreview,         "&Save Preview",		"");
  createRightClickMenuAction(MI_SavePreset, tr("&Save As Preset"), "");
  QAction *preferencesAction =
      createMenuFileAction(MI_Preferences, tr("&Preferences..."), "Ctrl+U");
  preferencesAction->setIcon(QIcon(":Resources/preferences.svg"));
  createMenuFileAction(MI_ShortcutPopup, tr("&Configure Shortcuts..."), "");
  createMenuFileAction(MI_PrintXsheet, tr("&Print Xsheet"), "");
  createMenuFileAction("MI_RunScript", tr("Run Script..."), "");
  createMenuFileAction("MI_OpenScriptConsole", tr("Open Script Console..."),
                       "");
  createMenuFileAction(MI_Print, tr("&Print Current Frame..."), "Ctrl+P");
  createMenuFileAction(MI_Quit, tr("&Quit"), "Ctrl+Q");
#ifndef NDEBUG
  createMenuFileAction("MI_ReloadStyle", tr("Reload qss"), "");
#endif
  createMenuAction(MI_LoadRecentImage, tr("&Load Recent Image Files"), files);
  createMenuFileAction(MI_ClearRecentImage,
                       tr("&Clear Recent Flipbook Image List"), "");

  createRightClickMenuAction(MI_PreviewFx, tr("Preview Fx"), "");

  createMenuEditAction(MI_SelectAll, tr("&Select All"), "Ctrl+A");
  createMenuEditAction(MI_InvertSelection, tr("&Invert Selection"), "");
  QAction *undoAction = createMenuEditAction(MI_Undo, tr("&Undo"), "Ctrl+Z");
  undoAction->setIcon(QIcon(":Resources/undo.svg"));
  QAction *redoAction = createMenuEditAction(MI_Redo, tr("&Redo"), "Ctrl+Y");
  redoAction->setIcon(QIcon(":Resources/redo.svg"));
  createMenuEditAction(MI_Cut, tr("&Cut"), "Ctrl+X");
  createMenuEditAction(MI_Copy, tr("&Copy"), "Ctrl+C");
  createMenuEditAction(MI_Paste, tr("&Paste Insert"), "Ctrl+V");
  createMenuEditAction(MI_PasteAbove, tr("&Paste Insert Above/After"),
                       "Ctrl+Shift+V");
  // createMenuEditAction(MI_PasteNew,     tr("&Paste New"),  "");
  createMenuCellsAction(MI_MergeFrames, tr("&Merge"), "");
  createMenuEditAction(MI_PasteInto, tr("&Paste Into"), "");
  createRightClickMenuAction(MI_PasteValues, tr("&Paste Color && Name"), "");
  createRightClickMenuAction(MI_PasteColors, tr("Paste Color"), "");
  createRightClickMenuAction(MI_PasteNames, tr("Paste Name"), "");
  createRightClickMenuAction(MI_GetColorFromStudioPalette,
                             tr("Get Color from Studio Palette"), "");
  createRightClickMenuAction(MI_ToggleLinkToStudioPalette,
                             tr("Toggle Link to Studio Palette"), "");
  createRightClickMenuAction(MI_RemoveReferenceToStudioPalette,
                             tr("Remove Reference to Studio Palette"), "");
  createMenuEditAction(MI_Clear, tr("&Delete"), "Del");
  createMenuEditAction(MI_Insert, tr("&Insert"), "Ins");
  createMenuEditAction(MI_InsertAbove, tr("&Insert Above/After"), "Shift+Ins");
  createMenuEditAction(MI_Group, tr("&Group"), "Ctrl+G");
  createMenuEditAction(MI_Ungroup, tr("&Ungroup"), "Ctrl+Shift+G");
  createMenuEditAction(MI_BringToFront, tr("&Bring to Front"), "Ctrl+]");
  createMenuEditAction(MI_BringForward, tr("&Bring Forward"), "]");
  createMenuEditAction(MI_SendBack, tr("&Send to Back"), "Ctrl+[");
  createMenuEditAction(MI_SendBackward, tr("&Send Backward"), "[");
  createMenuEditAction(MI_EnterGroup, tr("&Enter Group"), "");
  createMenuEditAction(MI_ExitGroup, tr("&Exit Group"), "");
  createMenuEditAction(MI_RemoveEndpoints, tr("&Remove Vector Overflow"), "");
  QAction *touchToggle =
      createToggle(MI_TouchGestureControl, tr("&Touch Gesture Control"), "",
                   TouchGestureControl ? 1 : 0, MenuEditCommandType);
  touchToggle->setEnabled(true);
  touchToggle->setIcon(QIcon(":Resources/touch.svg"));

  createMenuScanCleanupAction(MI_DefineScanner, tr("&Define Scanner..."), "");
  createMenuScanCleanupAction(MI_ScanSettings, tr("&Scan Settings..."), "");
  createMenuScanCleanupAction(MI_Scan, tr("&Scan"), "");
  createMenuScanCleanupAction(MI_Autocenter, tr("&Autocenter..."), "");

  QAction *toggle = createToggle(MI_SetScanCropbox, tr("&Set Cropbox"), "", 0,
                                 MenuScanCleanupCommandType);
  if (toggle) {
    SetScanCropboxCheck::instance()->setToggle(toggle);
    QString scannerType = QSettings().value("CurrentScannerType").toString();
    if (scannerType == "TWAIN") toggle->setDisabled(true);
    toggle = createMenuScanCleanupAction(MI_ResetScanCropbox,
                                         tr("&Reset Cropbox"), "");
    if (scannerType == "TWAIN") toggle->setDisabled(true);
  }

  createMenuScanCleanupAction(MI_CleanupSettings, tr("&Cleanup Settings..."),
                              "");

  toggle = createToggle(MI_CleanupPreview, tr("&Preview Cleanup"), "", 0,
                        MenuScanCleanupCommandType);
  CleanupPreviewCheck::instance()->setToggle(toggle);
  toggle = createToggle(MI_CameraTest, tr("&Camera Test"), "", 0,
                        MenuScanCleanupCommandType);
  CameraTestCheck::instance()->setToggle(toggle);

  createToggle(MI_OpacityCheck, tr("&Opacity Check"), "Alt+1", false,
               MenuScanCleanupCommandType);

  createMenuScanCleanupAction(MI_Cleanup, tr("&Cleanup"), "");

  createMenuScanCleanupAction(MI_PencilTest, tr("&Camera Capture..."), "");

  createMenuLevelAction(MI_AddFrames, tr("&Add Frames..."), "");
  createMenuLevelAction(MI_Renumber, tr("&Renumber..."), "");
  createMenuLevelAction(MI_ReplaceLevel, tr("&Replace Level..."), "");
  createMenuLevelAction(MI_RevertToCleanedUp, tr("&Revert to Cleaned Up"), "");
  createMenuLevelAction(MI_RevertToLastSaved, tr("&Reload"), "");
  createMenuLevelAction(MI_ExposeResource, tr("&Expose in Xsheet"), "");
  createMenuLevelAction(MI_EditLevel, tr("&Display in Level Strip"), "");
  createMenuLevelAction(MI_LevelSettings, tr("&Level Settings..."), "");
  createMenuLevelAction(MI_AdjustLevels, tr("Adjust Levels..."), "");
  createMenuLevelAction(MI_AdjustThickness, tr("Adjust Thickness..."), "");
  createMenuLevelAction(MI_Antialias, tr("&Antialias..."), "");
  createMenuLevelAction(MI_Binarize, tr("&Binarize..."), "");
  createMenuLevelAction(MI_BrightnessAndContrast,
                        tr("&Brightness and Contrast..."), "");
  createMenuLevelAction(MI_LinesFade, tr("&Color Fade..."), "");
#ifdef LINETEST
  createMenuLevelAction(MI_Capture, tr("&Capture"), "Space");
#endif
  QAction *action =
      createMenuLevelAction(MI_CanvasSize, tr("&Canvas Size..."), "");
  if (action) action->setDisabled(true);
  action->setIcon(QIcon(":Resources/canvas.svg"));
  createMenuLevelAction(MI_FileInfo, tr("&Info..."), "");
  createRightClickMenuAction(MI_ViewFile, tr("&View..."), "");
  createMenuLevelAction(MI_RemoveUnused, tr("&Remove All Unused Levels"), "");
  createMenuLevelAction(MI_ReplaceParentDirectory,
                        tr("&Replace Parent Directory..."), "");

  createMenuXsheetAction(MI_SceneSettings, tr("&Scene Settings..."), "");
  createMenuXsheetAction(MI_CameraSettings, tr("&Camera Settings..."), "");
  createMiscAction(MI_CameraStage, tr("&Camera Settings..."), "");

  QAction *openChildAction =
      createMenuXsheetAction(MI_OpenChild, tr("&Open Sub-xsheet"), "");
  openChildAction->setIconText("Open Sub-XSheet");
  openChildAction->setIcon(createQIconOnOffPNG("sub_enter"));

  QAction *closeChildAction =
      createMenuXsheetAction(MI_CloseChild, tr("&Close Sub-xsheet"), "");
  closeChildAction->setIconText("Close Sub-XSheet");
  closeChildAction->setIcon(createQIconOnOffPNG("sub_leave"));

  createMenuXsheetAction(MI_ExplodeChild, tr("Explode Sub-xsheet"), "");

  QAction *collapseAction =
      createMenuXsheetAction(MI_Collapse, tr("Collapse"), "");
  collapseAction->setIconText("Collapse");
  collapseAction->setIcon(createQIconOnOffPNG("collapse"));

  toggle = createToggle(MI_ToggleEditInPlace, tr("&Toggle Edit In Place"), "",
                        EditInPlaceToggleAction ? 1 : 0, MenuViewCommandType);
  toggle->setIconText(tr("Toggle Edit in Place"));
  toggle->setIcon(QIcon(":Resources/edit_in_place.svg"));

  createMenuXsheetAction(MI_SaveSubxsheetAs, tr("&Save Sub-xsheet As..."), "");
  createMenuXsheetAction(MI_Resequence, tr("Resequence"), "");
  QAction *cloneAction =
      createMenuXsheetAction(MI_CloneChild, tr("Clone Sub-xsheet"), "");
  cloneAction->setIcon(QIcon(":Resources/clone.svg"));
  createMenuXsheetAction(MI_ApplyMatchLines, tr("&Apply Match Lines..."), "");
  createMenuXsheetAction(MI_MergeCmapped, tr("&Merge Tlv Levels..."), "");
  createMenuXsheetAction(MI_DeleteMatchLines, tr("&Delete Match Lines"), "");
  createMenuXsheetAction(MI_DeleteInk, tr("&Delete Lines..."), "");
  QAction *mergeLevelsAction =
      createMenuXsheetAction(MI_MergeColumns, tr("&Merge Levels"), "");
  mergeLevelsAction->setIcon(QIcon(":Resources/merge.svg"));
  createMenuXsheetAction(MI_InsertFx, tr("&New FX..."), "Ctrl+F");
  QAction *newOutputAction =
      createMenuXsheetAction(MI_NewOutputFx, tr("&New Output"), "Ctrl+F");
  newOutputAction->setIcon(createQIconOnOff("output", false));

  createRightClickMenuAction(MI_FxParamEditor, tr("&Edit FX..."), "Ctrl+K");

  createMenuXsheetAction(MI_InsertSceneFrame, tr("Insert Frame"), "");
  createMenuXsheetAction(MI_RemoveSceneFrame, tr("Remove Frame"), "");
  createMenuXsheetAction(MI_InsertGlobalKeyframe, tr("Insert Multiple Keys"),
                         "");
  createMenuXsheetAction(MI_RemoveGlobalKeyframe, tr("Remove Multiple Keys"),
                         "");
  createMenuXsheetAction(MI_NewNoteLevel, tr("New Note Level"), "");
  createMenuXsheetAction(MI_RemoveEmptyColumns, tr("Remove Empty Columns"), "");
  createMenuXsheetAction(MI_LipSyncPopup, tr("&Apply Lip Sync Data to Column"),
                         "Alt+L");
  createRightClickMenuAction(MI_ToggleXSheetToolbar,
                             tr("Toggle XSheet Toolbar"), "");
  createMenuCellsAction(MI_Reverse, tr("&Reverse"), "");
  createMenuCellsAction(MI_Swing, tr("&Swing"), "");
  createMenuCellsAction(MI_Random, tr("&Random"), "");
  createMenuCellsAction(MI_Increment, tr("&Autoexpose"), "");

  QAction *repeatAction = createMenuCellsAction(MI_Dup, tr("&Repeat..."), "");
  repeatAction->setIconText("Repeat");
  repeatAction->setIcon(createQIconOnOffPNG("repeat_icon"));

  createMenuCellsAction(MI_ResetStep, tr("&Reset Step"), "");
  createMenuCellsAction(MI_IncreaseStep, tr("&Increase Step"), "'");
  createMenuCellsAction(MI_DecreaseStep, tr("&Decrease Step"), ";");
  createMenuCellsAction(MI_Step2, tr("&Step 2"), "");
  createMenuCellsAction(MI_Step3, tr("&Step 3"), "");
  createMenuCellsAction(MI_Step4, tr("&Step 4"), "");
  createMenuCellsAction(MI_Each2, tr("&Each 2"), "");
  createMenuCellsAction(MI_Each3, tr("&Each 3"), "");
  createMenuCellsAction(MI_Each4, tr("&Each 4"), "");
  createMenuCellsAction(MI_Rollup, tr("&Roll Up"), "");
  createMenuCellsAction(MI_Rolldown, tr("&Roll Down"), "");
  QAction *timeStretchAction =
      createMenuCellsAction(MI_TimeStretch, tr("&Time Stretch..."), "");
  timeStretchAction->setIcon(QIcon(":Resources/timestretch.svg"));
  createMenuCellsAction(MI_Duplicate, tr("&Duplicate Drawing  "), "D");
  createMenuCellsAction(MI_Autorenumber, tr("&Autorenumber"), "");
  createMenuCellsAction(MI_CloneLevel, tr("&Clone"), "");
  createMenuCellsAction(MI_DrawingSubForward,
                        tr("Drawing Substitution Forward"), "W");
  createMenuCellsAction(MI_DrawingSubBackward,
                        tr("Drawing Substitution Backward"), "Q");
  createMenuCellsAction(MI_DrawingSubGroupForward,
                        tr("Similar Drawing Substitution Forward"), "Alt+W");
  createMenuCellsAction(MI_DrawingSubGroupBackward,
                        tr("Similar Drawing Substitution Backward"), "Alt+Q");
  QAction *reframeOnesAction =
      createMenuCellsAction(MI_Reframe1, tr("1's"), "");
  reframeOnesAction->setIconText("1's");

  QAction *reframeTwosAction =
      createMenuCellsAction(MI_Reframe2, tr("2's"), "");
  reframeTwosAction->setIconText("2's");

  QAction *reframeThreesAction =
      createMenuCellsAction(MI_Reframe3, tr("3's"), "");
  reframeThreesAction->setIconText("3's");

  createMenuCellsAction(MI_Reframe4, tr("4's"), "");

  createMenuCellsAction(MI_ReframeWithEmptyInbetweens,
                        tr("Reframe with Empty Inbetweens..."), "");
  createMenuCellsAction(MI_AutoInputCellNumber, tr("Auto Input Cell Number..."),
                        "");
  createMenuCellsAction(MI_FillEmptyCell, tr("&Fill In Empty Cells"), "");

  createRightClickMenuAction(MI_SetKeyframes, tr("&Set Key"), "Z");
  createRightClickMenuAction(MI_PasteNumbers, tr("&Paste Numbers"), "");

  createToggle(MI_ViewCamera, tr("&Camera Box"), "",
               ViewCameraToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewTable, tr("&Table"), "", ViewTableToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_FieldGuide, tr("&Field Guide"), "Shift+G",
               FieldGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewBBox, tr("&Raster Bounding Box"), "",
               ViewBBoxToggleAction ? 1 : 0, MenuViewCommandType);
#ifdef LINETEST
  createToggle(MI_CapturePanelFieldGuide, tr("&Field Guide in Capture Window"),
               "", CapturePanelFieldGuideToggleAction ? 1 : 0,
               MenuViewCommandType);
#endif
  createToggle(MI_SafeArea, tr("&Safe Area"), "", SafeAreaToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_ViewColorcard, tr("&Camera BG Color"), "",
               ViewColorcardToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewGuide, tr("&Guide"), "", ViewGuideToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_ViewRuler, tr("&Ruler"), "", ViewRulerToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_TCheck, tr("&Transparency Check  "), "",
               TCheckToggleAction ? 1 : 0, MenuViewCommandType);
  QAction *inkCheckAction =
      createToggle(MI_ICheck, tr("&Ink Check"), "", ICheckToggleAction ? 1 : 0,
                   MenuViewCommandType);
  QAction *ink1CheckAction =
      createToggle(MI_Ink1Check, tr("&Ink#1 Check"), "",
                   Ink1CheckToggleAction ? 1 : 0, MenuViewCommandType);
  /*-- Ink Check と Ink1Checkを排他的にする --*/
  connect(inkCheckAction, SIGNAL(triggered(bool)), this,
          SLOT(onInkCheckTriggered(bool)));
  connect(ink1CheckAction, SIGNAL(triggered(bool)), this,
          SLOT(onInk1CheckTriggered(bool)));

  createToggle(MI_PCheck, tr("&Paint Check"), "", PCheckToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_IOnly, tr("Inks &Only"), "", IOnlyToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_GCheck, tr("&Fill Check"), "", GCheckToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_BCheck, tr("&Black BG Check"), "", BCheckToggleAction ? 1 : 0,
               MenuViewCommandType);
  createToggle(MI_ACheck, tr("&Gap Check"), "", ACheckToggleAction ? 1 : 0,
               MenuViewCommandType);
  QAction *shiftTraceAction = createToggle(MI_ShiftTrace, tr("Shift and Trace"),
                                           "", false, MenuViewCommandType);
  shiftTraceAction->setIcon(QIcon(":Resources/shift_and_trace.svg"));
  shiftTraceAction = createToggle(MI_EditShift, tr("Edit Shift"), "", false,
                                  MenuViewCommandType);
  shiftTraceAction->setIcon(QIcon(":Resources/shift_and_trace_edit.svg"));
  createToggle(MI_NoShift, tr("No Shift"), "", false, MenuViewCommandType);
  CommandManager::instance()->enable(MI_EditShift, false);
  CommandManager::instance()->enable(MI_NoShift, false);
  shiftTraceAction =
      createAction(MI_ResetShift, tr("Reset Shift"), "", MenuViewCommandType);
  shiftTraceAction->setIcon(QIcon(":Resources/shift_and_trace_reset.svg"));

  if (QGLPixelBuffer::hasOpenGLPbuffers())
    createToggle(MI_RasterizePli, tr("&Visualize Vector As Raster"), "",
                 RasterizePliToggleAction ? 1 : 0, MenuViewCommandType);
  else
    RasterizePliToggleAction = 0;

  createRightClickMenuAction(MI_Histogram, tr("&Histogram"), "");

  // createToolOptionsAction("A_ToolOption_Link", tr("Link"), "");
  createToggle(MI_Link, tr("Link Flipbooks"), "", LinkToggleAction ? 1 : 0,
               MenuViewCommandType);

  createPlaybackAction(MI_Play, tr("Play"), "P");
  createPlaybackAction(MI_Loop, tr("Loop"), "L");
  createPlaybackAction(MI_Pause, tr("Pause"), "");
  createPlaybackAction(MI_FirstFrame, tr("First Frame"), "Alt+,");
  createPlaybackAction(MI_LastFrame, tr("Last Frame"), "Alt+.");
  createPlaybackAction(MI_PrevFrame, tr("Previous Frame"), "Shift+,");
  createPlaybackAction(MI_NextFrame, tr("Next Frame"), "Shift+.");

  createAction(MI_NextDrawing, tr("Next Drawing"), ".", PlaybackCommandType);
  createAction(MI_PrevDrawing, tr("Prev Drawing"), ",", PlaybackCommandType);
  createAction(MI_NextStep, tr("Next Step"), "", PlaybackCommandType);
  createAction(MI_PrevStep, tr("Prev Step"), "", PlaybackCommandType);

  createRGBAAction(MI_RedChannel, tr("Red Channel"), "");
  createRGBAAction(MI_GreenChannel, tr("Green Channel"), "");
  createRGBAAction(MI_BlueChannel, tr("Blue Channel"), "");
  createRGBAAction(MI_MatteChannel, tr("Alpha Channel"), "");
  createRGBAAction(MI_RedChannelGreyscale, tr("Red Channel Greyscale"), "");
  createRGBAAction(MI_GreenChannelGreyscale, tr("Green Channel Greyscale"), "");
  createRGBAAction(MI_BlueChannelGreyscale, tr("Blue Channel Greyscale"), "");
  /*-- Viewer下部のCompareToSnapshotボタンのトグル --*/
  createViewerAction(MI_CompareToSnapshot, tr("Compare to Snapshot"), "");

  createFillAction(MI_AutoFillToggle,
                   tr("Toggle Autofill on Current Palette Color"), "Shift+A");

  toggle =
      createToggle(MI_DockingCheck, tr("&Lock Room Panes"), "",
                   DockingCheckToggleAction ? 1 : 0, MenuWindowsCommandType);
  DockingCheck::instance()->setToggle(toggle);

// createRightClickMenuAction(MI_OpenCurrentScene,   tr("&Current Scene"),
// "");
#ifdef LINETEST
  createMenuWindowsAction(MI_OpenExport, tr("&Export"), "");
#endif
  createMenuWindowsAction(MI_OpenFileBrowser, tr("&File Browser"), "");
  createMenuWindowsAction(MI_OpenFileViewer, tr("&Flipbook"), "");
  createMenuWindowsAction(MI_OpenFunctionEditor, tr("&Function Editor"), "");
  createMenuWindowsAction(MI_OpenFilmStrip, tr("&Level Strip"), "");
  createMenuWindowsAction(MI_OpenPalette, tr("&Palette"), "");
  QAction *pltGizmoAction =
      createRightClickMenuAction(MI_OpenPltGizmo, tr("&Palette Gizmo"), "");
  pltGizmoAction->setIcon(QIcon(":Resources/palettegizmo.svg"));
  createRightClickMenuAction(MI_EraseUnusedStyles, tr("&Delete Unused Styles"),
                             "");
  createMenuWindowsAction(MI_OpenTasks, tr("&Tasks"), "");
  createMenuWindowsAction(MI_OpenBatchServers, tr("&Batch Servers"), "");
  createMenuWindowsAction(MI_OpenTMessage, tr("&Message Center"), "");
  createMenuWindowsAction(MI_OpenColorModel, tr("&Color Model"), "");
  createMenuWindowsAction(MI_OpenStudioPalette, tr("&Studio Palette"), "");
  createMenuWindowsAction(MI_OpenSchematic, tr("&Schematic"), "");
  createMenuWindowsAction(MI_OpenCleanupSettings, tr("&Cleanup Settings"), "");

  createMenuWindowsAction(MI_OpenFileBrowser2, tr("&Scene Cast"), "");
  createMenuWindowsAction(MI_OpenStyleControl, tr("&Style Editor"), "");
  createMenuWindowsAction(MI_OpenToolbar, tr("&Toolbar"), "");
  createMenuWindowsAction(MI_OpenToolOptionBar, tr("&Tool Option Bar"), "");
  createMenuWindowsAction(MI_OpenCommandToolbar, tr("&Command Bar"), "");
  createMenuWindowsAction(MI_OpenLevelView, tr("&Viewer"), "");
#ifdef LINETEST
  createMenuWindowsAction(MI_OpenLineTestCapture, tr("&LineTest Capture"), "");
  createMenuWindowsAction(MI_OpenLineTestView, tr("&LineTest Viewer"), "");
#endif
  createMenuWindowsAction(MI_OpenXshView, tr("&Xsheet"), "");
  createMenuWindowsAction(MI_OpenTimelineView, tr("&Timeline"), "");
  //  createAction(MI_TestAnimation,     "Test Animation",   "Ctrl+Return");
  //  createAction(MI_Export,            "Export",           "Ctrl+E");

  createMenuWindowsAction(MI_OpenComboViewer, tr("&ComboViewer"), "");
  createMenuWindowsAction(MI_OpenHistoryPanel, tr("&History"), "Ctrl+H");
  createMenuWindowsAction(MI_AudioRecording, tr("Record Audio"), "Alt+A");
  createMenuWindowsAction(MI_ResetRoomLayout, tr("&Reset to Default Rooms"),
                          "");
  createMenuWindowsAction(MI_MaximizePanel, tr("Toggle Maximize Panel"), "`");
  createMenuWindowsAction(MI_FullScreenWindow,
                          tr("Toggle Main Window's Full Screen Mode"),
                          "Ctrl+`");
  createMenuWindowsAction(MI_About, tr("&About OpenToonz..."), "");
  createMenuWindowsAction(MI_StartupPopup, tr("&Startup Popup..."), "Alt+S");

  createRightClickMenuAction(MI_BlendColors, tr("&Blend colors"), "");

  createToggle(MI_OnionSkin, tr("Onion Skin Toggle"), "/", false,
               RightClickMenuCommandType);
  createToggle(MI_ZeroThick, tr("Zero Thick Lines"), "Shift+/", false,
               RightClickMenuCommandType);
  createToggle(MI_CursorOutline, tr("Toggle Cursor Size Outline"), "", false,
               RightClickMenuCommandType);

  createRightClickMenuAction(MI_ToggleCurrentTimeIndicator,
                             tr("Toggle Current Time Indicator"), "");

  // createRightClickMenuAction(MI_LoadSubSceneFile,     tr("Load As
  // Sub-xsheet"),   "");
  // createRightClickMenuAction(MI_LoadResourceFile,     tr("Load"),
  // "");
  createRightClickMenuAction(MI_DuplicateFile, tr("Duplicate"), "");
  createRightClickMenuAction(MI_ShowFolderContents, tr("Show Folder Contents"),
                             "");
  createRightClickMenuAction(MI_ConvertFiles, tr("Convert..."), "");
  createRightClickMenuAction(MI_CollectAssets, tr("Collect Assets"), "");
  createRightClickMenuAction(MI_ImportScenes, tr("Import Scene"), "");
  createRightClickMenuAction(MI_ExportScenes, tr("Export Scene..."), "");

  // createRightClickMenuAction(MI_PremultiplyFile,      tr("Premultiply"),
  // "");
  createMenuLevelAction(MI_ConvertToVectors, tr("Convert to Vectors..."), "");
  createMenuLevelAction(MI_ConvertToToonzRaster, tr("Vectors to Toonz Raster"),
                        "");
  createMenuLevelAction(MI_ConvertVectorToVector,
                        tr("Replace Vectors with Simplified Vectors"), "");
  createMenuLevelAction(MI_Tracking, tr("Tracking..."), "");
  createRightClickMenuAction(MI_RemoveLevel, tr("Remove Level"), "");
  createRightClickMenuAction(MI_AddToBatchRenderList, tr("Add As Render Task"),
                             "");
  createRightClickMenuAction(MI_AddToBatchCleanupList,
                             tr("Add As Cleanup Task"), "");

  createRightClickMenuAction(MI_SelectRowKeyframes,
                             tr("Select All Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectColumnKeyframes,
                             tr("Select All Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectAllKeyframes, tr("Select All Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotBefore,
                             tr("Select All Following Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotAfter,
                             tr("Select All Previous Keys"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInColumn,
                             tr("Select Previous Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectFollowingKeysInColumn,
                             tr("Select Following Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInRow,
                             tr("Select Previous Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectFollowingKeysInRow,
                             tr("Select Following Keys in this Frame"), "");
  createRightClickMenuAction(MI_InvertKeyframeSelection,
                             tr("Invert Key Selection"), "");

  createRightClickMenuAction(MI_SetAcceleration, tr("Set Acceleration"), "");
  createRightClickMenuAction(MI_SetDeceleration, tr("Set Deceleration"), "");
  createRightClickMenuAction(MI_SetConstantSpeed, tr("Set Constant Speed"), "");
  createRightClickMenuAction(MI_ResetInterpolation, tr("Reset Interpolation"),
                             "");

  createRightClickMenuAction(MI_UseLinearInterpolation,
                             tr("Linear Interpolation"), "");
  createRightClickMenuAction(MI_UseSpeedInOutInterpolation,
                             tr("Speed In / Speed Out Interpolation"), "");
  createRightClickMenuAction(MI_UseEaseInOutInterpolation,
                             tr("Ease In / Ease Out Interpolation"), "");
  createRightClickMenuAction(MI_UseEaseInOutPctInterpolation,
                             tr("Ease In / Ease Out (%) Interpolation"), "");
  createRightClickMenuAction(MI_UseExponentialInterpolation,
                             tr("Exponential Interpolation"), "");
  createRightClickMenuAction(MI_UseExpressionInterpolation,
                             tr("Expression Interpolation"), "");
  createRightClickMenuAction(MI_UseFileInterpolation, tr("File Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseConstantInterpolation,
                             tr("Constant Interpolation"), "");

  createRightClickMenuAction(MI_FoldColumns, tr("Fold Column"), "");

  createRightClickMenuAction(MI_ActivateThisColumnOnly, tr("Show This Only"),
                             "");
  createRightClickMenuAction(MI_ActivateSelectedColumns, tr("Show Selected"),
                             "");
  createRightClickMenuAction(MI_ActivateAllColumns, tr("Show All"), "");
  createRightClickMenuAction(MI_DeactivateSelectedColumns, tr("Hide Selected"),
                             "");
  createRightClickMenuAction(MI_DeactivateAllColumns, tr("Hide All"), "");
  createRightClickMenuAction(MI_ToggleColumnsActivation, tr("Toggle Show/Hide"),
                             "");
  createRightClickMenuAction(MI_EnableThisColumnOnly, tr("ON This Only"), "");
  createRightClickMenuAction(MI_EnableSelectedColumns, tr("ON Selected"), "");
  createRightClickMenuAction(MI_EnableAllColumns, tr("ON All"), "");
  createRightClickMenuAction(MI_DisableAllColumns, tr("OFF All"), "");
  createRightClickMenuAction(MI_DisableSelectedColumns, tr("OFF Selected"), "");
  createRightClickMenuAction(MI_SwapEnabledColumns, tr("Swap ON/OFF"), "");
  createRightClickMenuAction(MI_LockThisColumnOnly, tr("Lock This Only"),
                             "Shift+L");
  createRightClickMenuAction(MI_LockSelectedColumns, tr("Lock Selected"),
                             "Ctrl+Shift+L");
  createRightClickMenuAction(MI_LockAllColumns, tr("Lock All"),
                             "Ctrl+Alt+Shift+L");
  createRightClickMenuAction(MI_UnlockSelectedColumns, tr("Unlock Selected"),
                             "Ctrl+Shift+U");
  createRightClickMenuAction(MI_UnlockAllColumns, tr("Unlock All"),
                             "Ctrl+Alt+Shift+U");
  createRightClickMenuAction(MI_ToggleColumnLocks, tr("Swap Lock/Unlock"), "");
  /*-- カレントカラムの右側のカラムを全て非表示にするコマンド --*/
  createRightClickMenuAction(MI_DeactivateUpperColumns,
                             tr("Hide Upper Columns"), "");

  createRightClickMenuAction(MI_SeparateColors, tr("Separate Colors..."), "");

  createToolAction(T_Edit, "edit", tr("Animate Tool"), "A");
  createToolAction(T_Selection, "selection", tr("Selection Tool"), "S");
  createToolAction(T_Brush, "brush", tr("Brush Tool"), "B");
  createToolAction(T_Geometric, "geometric", tr("Geometric Tool"), "G");
  createToolAction(T_Type, "type", tr("Type Tool"), "Y");
  createToolAction(T_Fill, "fill", tr("Fill Tool"), "F");
  createToolAction(T_PaintBrush, "paintbrush", tr("Paint Brush Tool"), "");
  createToolAction(T_Eraser, "eraser", tr("Eraser Tool"), "E");
  createToolAction(T_Tape, "tape", tr("Tape Tool"), "T");
  createToolAction(T_StylePicker, "stylepicker", tr("Style Picker Tool"), "K");
  createToolAction(T_RGBPicker, "RGBpicker", tr("RGB Picker Tool"), "R");
  createToolAction(T_ControlPointEditor, "controlpointeditor",
                   tr("Control Point Editor Tool"), "C");
  createToolAction(T_Pinch, "pinch", tr("Pinch Tool"), "M");
  createToolAction(T_Pump, "pump", tr("Pump Tool"), "");
  createToolAction(T_Magnet, "magnet", tr("Magnet Tool"), "");
  createToolAction(T_Bender, "bender", tr("Bender Tool"), "");
  createToolAction(T_Iron, "iron", tr("Iron Tool"), "");
  createToolAction(T_Cutter, "cutter", tr("Cutter Tool"), "");
  createToolAction(T_Skeleton, "skeleton", tr("Skeleton Tool"), "V");
  createToolAction(T_Tracker, "tracker", tr("Tracker Tool"), "");
  createToolAction(T_Hook, "hook", tr("Hook Tool"), "O");
  createToolAction(T_Zoom, "zoom", tr("Zoom Tool"), "Shift+Space");
  createToolAction(T_Rotate, "rotate", tr("Rotate Tool"), "Ctrl+Space");
  createToolAction(T_Hand, "hand", tr("Hand Tool"), "Space");
  createToolAction(T_Plastic, "plastic", tr("Plastic Tool"), "X");
  createToolAction(T_Ruler, "ruler", tr("Ruler Tool"), "");
  createToolAction(T_Finger, "finger", tr("Finger Tool"), "");

  createViewerAction(V_ZoomIn, tr("Zoom In"), "+");
  createViewerAction(V_ZoomOut, tr("Zoom Out"), "-");
  createViewerAction(V_ZoomReset, tr("Reset View"), "Alt+0");
  createViewerAction(V_ZoomFit, tr("Fit to Window"), "Alt+9");
  createViewerAction(V_ActualPixelSize, tr("Actual Pixel Size"), "N");
  createViewerAction(V_FlipX, tr("Flip Viewer Horizontally"), "");
  createViewerAction(V_FlipY, tr("Flip Viewer Vertically"), "");
  createViewerAction(V_ShowHideFullScreen, tr("Show//Hide Full Screen"),
                     "Alt+F");
  CommandManager::instance()->setToggleTexts(V_ShowHideFullScreen,
                                             tr("Full Screen Mode"),
                                             tr("Exit Full Screen Mode"));

  QAction *refreshAct =
      createMiscAction(MI_RefreshTree, tr("Refresh Folder Tree"), "");
  refreshAct->setIconText(tr("Refresh"));
  refreshAct->setIcon(createQIcon("refresh"));

  createToolOptionsAction("A_ToolOption_GlobalKey", tr("Global Key"), "");

  createToolOptionsAction("A_IncreaseMaxBrushThickness",
                          tr("Brush size - Increase max"), "I");
  createToolOptionsAction("A_DecreaseMaxBrushThickness",
                          tr("Brush size - Decrease max"), "U");
  createToolOptionsAction("A_IncreaseMinBrushThickness",
                          tr("Brush size - Increase min"), "J");
  createToolOptionsAction("A_DecreaseMinBrushThickness",
                          tr("Brush size - Decrease min"), "H");
  createToolOptionsAction("A_IncreaseBrushHardness",
                          tr("Brush hardness - Increase"), "");
  createToolOptionsAction("A_DecreaseBrushHardness",
                          tr("Brush hardness - Decrease"), "");
  createToolOptionsAction("A_ToolOption_SnapSensitivity", tr("SnapSensitivity"),
                          "");
  createToolOptionsAction("A_ToolOption_AutoGroup", tr("Auto Group"), "");
  createToolOptionsAction("A_ToolOption_BreakSharpAngles",
                          tr("Break sharp angles"), "");
  createToolOptionsAction("A_ToolOption_FrameRange", tr("Frame range"), "F6");
  createToolOptionsAction("A_ToolOption_IK", tr("Inverse kinematics"), "");
  createToolOptionsAction("A_ToolOption_Invert", tr("Invert"), "");
  createToolOptionsAction("A_ToolOption_Manual", tr("Manual"), "");
  createToolOptionsAction("A_ToolOption_OnionSkin", tr("Onion skin"), "");
  createToolOptionsAction("A_ToolOption_Orientation", tr("Orientation"), "");
  createToolOptionsAction("A_ToolOption_PencilMode", tr("Pencil Mode"), "");
  createToolOptionsAction("A_ToolOption_PreserveThickness",
                          tr("Preserve Thickness"), "");
  createToolOptionsAction("A_ToolOption_PressureSensitivity",
                          tr("Pressure Sensitivity"), "Shift+P");
  createToolOptionsAction("A_ToolOption_SegmentInk", tr("Segment Ink"), "F8");
  createToolOptionsAction("A_ToolOption_Selective", tr("Selective"), "F7");
  createToolOptionsAction("A_ToolOption_DrawOrder",
                          tr("Brush Tool - Draw Order"), "");
  createToolOptionsAction("A_ToolOption_Smooth", tr("Smooth"), "");
  createToolOptionsAction("A_ToolOption_Snap", tr("Snap"), "");
  createToolOptionsAction("A_ToolOption_AutoSelectDrawing",
                          tr("Auto Select Drawing"), "");
  createToolOptionsAction("A_ToolOption_Autofill", tr("Auto Fill"), "");
  createToolOptionsAction("A_ToolOption_JoinVectors", tr("Join Vectors"), "");
  createToolOptionsAction("A_ToolOption_ShowOnlyActiveSkeleton",
                          tr("Show Only Active Skeleton"), "");

  // Option Menu
  createToolOptionsAction("A_ToolOption_BrushPreset", tr("Brush Preset"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape", tr("Geometric Shape"),
                          "");
  createToolOptionsAction("A_ToolOption_GeometricEdge", tr("Geometric Edge"),
                          "");
  createToolOptionsAction("A_ToolOption_Mode", tr("Mode"), "");
  createToolOptionsAction("A_ToolOption_Mode:Areas", tr("Mode - Areas"), "");
  createToolOptionsAction("A_ToolOption_Mode:Lines", tr("Mode - Lines"), "");
  createToolOptionsAction("A_ToolOption_Mode:Lines & Areas",
                          tr("Mode - Lines & Areas"), "");
  createToolOptionsAction("A_ToolOption_Type", tr("Type"), "");
  createToolOptionsAction("A_ToolOption_Type:Normal", tr("Type - Normal"), "");
  createToolOptionsAction("A_ToolOption_Type:Rectangular",
                          tr("Type - Rectangular"), "F5");
  createToolOptionsAction("A_ToolOption_Type:Freehand", tr("Type - Freehand"),
                          "");
  createToolOptionsAction("A_ToolOption_Type:Polyline", tr("Type - Polyline"),
                          "");
  createToolOptionsAction("A_ToolOption_TypeFont", tr("TypeTool Font"), "");
  createToolOptionsAction("A_ToolOption_TypeSize", tr("TypeTool Size"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle", tr("TypeTool Style"), "");

  createToolOptionsAction("A_ToolOption_EditToolActiveAxis", tr("Active Axis"),
                          "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Position",
                          tr("Active Axis - Position"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Rotation",
                          tr("Active Axis - Rotation"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Scale",
                          tr("Active Axis - Scale"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Shear",
                          tr("Active Axis - Shear"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Center",
                          tr("Active Axis - Center"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:All",
                          tr("Active Axis - All"), "");

  createToolOptionsAction("A_ToolOption_SkeletonMode:Build Skeleton",
                          tr("Build Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Animate",
                          tr("Animate Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Inverse Kinematics",
                          tr("Inverse Kinematics Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:None", tr("None Pick Mode"),
                          "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Column",
                          tr("Column Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Pegbar",
                          tr("Pegbar Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_PickScreen", tr("Pick Screen"), "");
  createToolOptionsAction("A_ToolOption_Meshify", tr("Create Mesh"), "");

  createToolOptionsAction("A_ToolOption_AutopaintLines",
                          tr("Fill Tool - Autopaint Lines"), "");

  /*-- FillAreas, FillLinesにキー1つで切り替えるためのコマンド --*/
  createAction(MI_FillAreas, tr("Fill Tool - Areas"), "", ToolCommandType);
  createAction(MI_FillLines, tr("Fill Tool - Lines"), "", ToolCommandType);

  /*-- Style picker Area, Style picker Lineににキー1つで切り替えるためのコマンド
   * --*/
  createAction(MI_PickStyleAreas, tr("Style Picker Tool - Areas"), "",
               ToolCommandType);
  createAction(MI_PickStyleLines, tr("Style Picker Tool - Lines"), "",
               ToolCommandType);

  createMiscAction("A_FxSchematicToggle", tr("Toggle FX/Stage schematic"), "");
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

//---------------------------------------------------------------------------------------
/*-- FillAreas, FillLinesにキー1つで切り替えるためのコマンド --*/
void MainWindow::toggleFillAreas() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void MainWindow::toggleFillLines() {
  CommandManager::instance()->getAction(T_Fill)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

//---------------------------------------------------------------------------------------
/*-- Style picker Area, Style picker Lineににキー1つで切り替えるためのコマンド
 * --*/
void MainWindow::togglePickStyleAreas() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Areas")->trigger();
}

void MainWindow::togglePickStyleLines() {
  CommandManager::instance()->getAction(T_StylePicker)->trigger();
  CommandManager::instance()->getAction("A_ToolOption_Mode:Lines")->trigger();
}

//-----------------------------------------------------------------------------

void MainWindow::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

class ReloadStyle final : public MenuItemHandler {
public:
  ReloadStyle() : MenuItemHandler("MI_ReloadStyle") {}
  void execute() override {
    QString currentStyle = Preferences::instance()->getCurrentStyleSheetPath();
    QFile file(currentStyle);
    file.open(QFile::ReadOnly);
    QString styleSheet = QString(file.readAll());
    qApp->setStyleSheet(styleSheet);
    file.close();
  }
} reloadStyle;

void MainWindow::onQuit() { close(); }

//=============================================================================
// RecentFiles
//=============================================================================

RecentFiles::RecentFiles() : m_recentScenes(), m_recentLevels() {}

//-----------------------------------------------------------------------------

RecentFiles *RecentFiles::instance() {
  static RecentFiles _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

RecentFiles::~RecentFiles() {}

//-----------------------------------------------------------------------------

void RecentFiles::addFilePath(QString path, FileType fileType) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
                                                 : m_recentFlipbookImages;
  int i;
  for (i = 0; i < files.size(); i++)
    if (files.at(i) == path) files.removeAt(i);
  files.insert(0, path);
  int maxSize = 10;
  if (files.size() > maxSize) files.removeAt(maxSize);

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
  if (fileType == Scene)
    m_recentScenes.move(fromIndex, toIndex);
  else if (fileType == Level)
    m_recentLevels.move(fromIndex, toIndex);
  else
    m_recentFlipbookImages.move(fromIndex, toIndex);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::removeFilePath(int index, FileType fileType) {
  if (fileType == Scene)
    m_recentScenes.removeAt(index);
  else if (fileType == Level)
    m_recentLevels.removeAt(index);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFilePath(int index, FileType fileType) const {
  return (fileType == Scene)
             ? m_recentScenes[index]
             : (fileType == Level) ? m_recentLevels[index]
                                   : m_recentFlipbookImages[index];
}

//-----------------------------------------------------------------------------

void RecentFiles::clearRecentFilesList(FileType fileType) {
  if (fileType == Scene)
    m_recentScenes.clear();
  else if (fileType == Level)
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
  settings.setValue(QString("Levels"), QVariant(m_recentLevels));
  settings.setValue(QString("FlipbookImages"),
                    QVariant(m_recentFlipbookImages));
}

//-----------------------------------------------------------------------------

QList<QString> RecentFiles::getFilesNameList(FileType fileType) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
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
  CommandId id = (fileType == Scene) ? MI_OpenRecentScene
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
    CommandId clearActionId =
        (fileType == Scene) ? MI_ClearRecentScene : (fileType == Level)
                                                        ? MI_ClearRecentLevel
                                                        : MI_ClearRecentImage;
    menu->setActions(names);
    menu->addSeparator();
    QAction *clearAction = CommandManager::instance()->getAction(clearActionId);
    assert(clearAction);
    menu->addAction(clearAction);
    if (!menu->isEnabled()) menu->setEnabled(true);
  }
}
