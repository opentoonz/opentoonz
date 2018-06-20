

// Tnz6 includes
#include "mainwindow.h"
#include "flipbook.h"
#include "tapp.h"
#include "iocommand.h"
#include "previewfxmanager.h"
#include "cleanupsettingspopup.h"
#include "filebrowsermodel.h"

#ifdef LINETEST
#include "licensegui.h"
#include "licensecontroller.h"
#endif

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tmessageviewer.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/pluginloader.h"

// TnzStdfx includes
#include "stdfx/shaderfx.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/studiopalette.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshsimplelevel.h"

// TnzSound includes
#include "tnzsound.h"

// TnzImage includes
#include "tnzimage.h"

// TnzBase includes
#include "permissionsmanager.h"
#include "tenv.h"
#include "tcli.h"

// TnzCore includes
#include "tsystem.h"
#include "tthread.h"
#include "tthreadmessage.h"
#include "tundo.h"
#include "tconvert.h"
#include "tiio_std.h"
#include "timagecache.h"
#include "tofflinegl.h"
#include "tpluginmanager.h"
#include "tsimplecolorstyles.h"
#include "toonz/imagestyles.h"
#include "tvectorbrushstyle.h"

#ifdef MACOSX
#include "tipc.h"
#endif

// Qt includes
#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QSplashScreen>
#include <QGLPixelBuffer>
#include <QTranslator>
#include <QFileInfo>
#include <QSettings>
#include <QLibraryInfo>

using namespace DVGui;
#if defined LINETEST
const char *applicationName    = "Toonz LineTest";
const char *applicationVersion = "6.4";
const char *dllRelativePath    = "./linetest.app/Contents/Frameworks";
TEnv::StringVar EnvSoftwareCurrentFont("SoftwareCurrentFont", "MS Sans Serif");
TEnv::IntVar EnvSoftwareCurrentFontSize("SoftwareCurrentFontSize", 12);
const char *applicationFullName = "LineTest 6.4 Beta";
const char *rootVarName         = "LINETESTROOT";
const char *systemVarPrefix     = "LINETEST";
#else
const char *applicationName     = "OpenToonz";
const char *applicationVersion  = "1.2";
const char *applicationRevision = "1";
const char *dllRelativePath     = "./toonz6.app/Contents/Frameworks";
#endif

TEnv::IntVar EnvSoftwareCurrentFontSize("SoftwareCurrentFontSize", 12);

const char *applicationFullName = "OpenToonz 1.2.1";  // next will be 1.3 (not 1.3.0)
const char *rootVarName         = "TOONZROOT";
const char *systemVarPrefix     = "TOONZ";

#ifdef MACOSX
#include "tthread.h"
void postThreadMsg(TThread::Message *) {}
void qt_mac_set_menubar_merge(bool enable);
#endif

// Modifica per toonz (non servono questo tipo di licenze)
#define NO_LICENSE
//-----------------------------------------------------------------------------

static void fatalError(QString msg) {
  DVGui::MsgBoxInPopup(
      CRITICAL, msg + "\n" +
                    QObject::tr("Installing %1 again could fix the problem.")
                        .arg(applicationFullName));
  exit(0);
}
//-----------------------------------------------------------------------------

static void lastWarningError(QString msg) {
  DVGui::error(msg);
  // exit(0);
}
//-----------------------------------------------------------------------------

static void toonzRunOutOfContMemHandler(unsigned long size) {
#ifdef _WIN32
  static bool firstTime = true;
  if (firstTime) {
    MessageBox(NULL, (LPCWSTR)L"Run out of contiguous physical memory: please save all and restart Toonz!",
				   (LPCWSTR)L"Warning", MB_OK | MB_SYSTEMMODAL);
    firstTime = false;
  }
#endif
}

//-----------------------------------------------------------------------------

// todo.. da mettere in qualche .h
DV_IMPORT_API void initStdFx();
DV_IMPORT_API void initColorFx();

//-----------------------------------------------------------------------------

//! Inizializzaza l'Environment di Toonz
/*! In particolare imposta la projectRoot e
    la stuffDir, controlla se la directory di outputs esiste (e provvede a
    crearla in caso contrario) verifica inoltre che stuffDir esista.
*/
static void initToonzEnv() {
  StudioPalette::enable(true);

  TEnv::setApplication(applicationName, applicationVersion,
                       applicationRevision);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);
  TEnv::setDllRelativeDir(TFilePath(dllRelativePath));

  QCoreApplication::setOrganizationName("OpenToonz");
  QCoreApplication::setOrganizationDomain("");
  QString fullApplicationNameQStr =
      QString(applicationName) + " " + applicationVersion;
  QCoreApplication::setApplicationName(fullApplicationNameQStr);

  /*-- TOONZROOTのPathの確認 --*/
  // controllo se la xxxroot e' definita e corrisponde ad un folder esistente

  /*-- ENGLISH: Confirm TOONZROOT Path
        Check if the xxxroot is defined and corresponds to an existing folder
  --*/

  TFilePath stuffDir = TEnv::getStuffDir();
  if (stuffDir == TFilePath())
    fatalError("Undefined or empty: \"" + toQString(TEnv::getRootVarPath()) +
               "\"");
  else if (!TFileStatus(stuffDir).isDirectory())
    fatalError("Folder \"" + toQString(stuffDir) +
               "\" not found or not readable");

  Tiio::defineStd();
  initImageIo();
  initSoundIo();
  initStdFx();
  initColorFx();

  // TPluginManager::instance()->loadStandardPlugins();

  TFilePath library = ToonzFolder::getLibraryFolder();

  TRasterImagePatternStrokeStyle::setRootDir(library);
  TVectorImagePatternStrokeStyle::setRootDir(library);
  TVectorBrushStyle::setRootDir(library);

  CustomStyleManager::setRootPath(library);

  // sembra indispensabile nella lettura dei .tab 2.2:
  TPalette::setRootDir(library);
  TImageStyle::setLibraryDir(library);

  // TProjectManager::instance()->enableTabMode(true);

  TProjectManager *projectManager = TProjectManager::instance();

  /*--
   * TOONZPROJECTSのパスセットを取得する。（TOONZPROJECTSはセミコロンで区切って複数設定可能）
   * --*/
  TFilePathSet projectsRoots = ToonzFolder::getProjectsFolders();
  TFilePathSet::iterator it;
  for (it = projectsRoots.begin(); it != projectsRoots.end(); ++it)
    projectManager->addProjectsRoot(*it);

  /*-- もしまだ無ければ、TOONZROOT/sandboxにsandboxプロジェクトを作る --*/
  projectManager->createSandboxIfNeeded();

  /*
TProjectP project = projectManager->getCurrentProject();
Non dovrebbe servire per Tab:
project->setFolder(TProject::Drawings, TFilePath("$scenepath"));
project->setFolder(TProject::Extras, TFilePath("$scenepath"));
project->setUseScenePath(TProject::Drawings, false);
project->setUseScenePath(TProject::Extras, false);
*/
  // Imposto la rootDir per ImageCache

  /*-- TOONZCACHEROOTの設定  --*/
  TFilePath cacheDir               = ToonzFolder::getCacheRootFolder();
  if (cacheDir.isEmpty()) cacheDir = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheDir);
}

//-----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
  //  Enable standard input/output on Windows Platform for debug
  BOOL consoleAttached = ::AttachConsole(ATTACH_PARENT_PROCESS);
  if (consoleAttached) {
    freopen("CON", "r", stdin);
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
  }
#endif

  /*-- "-layout [レイアウト設定ファイル名]"
   * で、必要なモジュールのPageだけのレイアウトで起動することを可能にする --*/
  QString argumentLayoutFileName = "";
  TFilePath loadScenePath;
  if (argc > 1) {
    for (int a = 1; a < argc; a++) {
      if (QString(argv[a]) == "-layout") {
        argumentLayoutFileName = QString(argv[a + 1]);
        a++;
      } else
        loadScenePath = TFilePath(argv[a]);
    }
  }

// Enables high-DPI scaling. This attribute must be set before QApplication is
// constructed. Available from Qt 5.6.
#if QT_VERSION >= 0x050600
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QApplication a(argc, argv);

#ifdef MACOSX
// This workaround is to avoid missing left button problem on Qt5.6.0.
// To invalidate m_rightButtonClicked in Qt/qnsview.mm, sending NSLeftButtonDown
// event
// before NSLeftMouseDragged event propagated to QApplication.
// See more details in ../mousedragfilter/mousedragfilter.mm.

#include "mousedragfilter.h"

  class OSXMouseDragFilter final : public QAbstractNativeEventFilter {
    bool leftButtonPressed = false;

  public:
    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           long *) Q_DECL_OVERRIDE {
      if (IsLeftMouseDown(message)) {
        leftButtonPressed = true;
      }
      if (IsLeftMouseUp(message)) {
        leftButtonPressed = false;
      }

      if (eventType == "mac_generic_NSEvent") {
        if (IsLeftMouseDragged(message) && !leftButtonPressed) {
          std::cout << "force mouse press event" << std::endl;
          SendLeftMousePressEvent();
          return true;
        }
      }
      return false;
    }
  };

  a.installNativeEventFilter(new OSXMouseDragFilter);
#endif

#ifdef Q_OS_WIN
  //	Since currently OpenToonz does not work with OpenGL of software or
  // angle,
  //	force Qt to use desktop OpenGL
  a.setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

  // Some Qt objects are destroyed badly withouth a living qApp. So, we must
  // enforce a way to either
  // postpone the application destruction until the very end, OR ensure that
  // sensible objects are
  // destroyed before.

  // Using a static QApplication only worked on Windows, and in any case C++
  // respects the statics destruction
  // order ONLY within the same library. On MAC, it made the app crash on exit
  // o_o. So, nope.

  std::unique_ptr<QObject> mainScope(new QObject(
      &a));  // A QObject destroyed before the qApp is therefore explicitly
  mainScope->setObjectName("mainScope");  // provided. It can be accessed by
                                          // looking in the qApp's children.

#ifdef _WIN32
#ifndef x64
  // Store the floating point control word. It will be re-set before Toonz
  // initialization
  // has ended.
  unsigned int fpWord = 0;
  _controlfp_s(&fpWord, 0, 0);
#endif
#endif

#ifdef _WIN32
  // At least on windows, Qt's 4.5.2 native windows feature tend to create
  // weird flickering effects when dragging panel separators.
  a.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

  // Enable to render smooth icons on high dpi monitors
  a.setAttribute(Qt::AA_UseHighDpiPixmaps);

  // Set the app's locale for numeric stuff to standard C. This is important for
  // atof() and similar
  // calls that are locale-dependant.
  setlocale(LC_NUMERIC, "C");

// Set current directory to the bundle/application path - this is needed to have
// correct relative paths
#ifdef MACOSX
  {
    QDir appDir(QApplication::applicationDirPath());
    appDir.cdUp(), appDir.cdUp(), appDir.cdUp();

    bool ret = QDir::setCurrent(appDir.absolutePath());
    assert(ret);
  }
#endif

  // splash screen
  QPixmap splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
  splashPixmap.setDevicePixelRatio(QApplication::desktop()->devicePixelRatio());
// QPixmap splashPixmap(":Resources/splash.png");
#ifdef _WIN32
  QFont font("Segoe UI", -1);
#else
  QFont font("Helvetica", -1);
#endif
  font.setPixelSize(13);
  font.setWeight(50);
  a.setFont(font);

  QString offsetStr("\n\n\n\n\n\n\n\n");

  TSystem::hasMainLoop(true);

  TMessageRepository::instance();

  QSplashScreen splash(splashPixmap);
  splash.show();
  a.processEvents();

  splash.showMessage(offsetStr + "Initializing QGLFormat...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // OpenGL
  QGLFormat fmt;
  fmt.setAlpha(true);
  fmt.setStencil(true);
  QGLFormat::setDefaultFormat(fmt);

// seems this function should be called at all systems
// pheraps in some GLUT-implementations initalization is mere formality
#if defined(LINUX) || (defined(_WIN32) && defined(__GNUC__))
  glutInit(&argc, argv);
#endif

  splash.showMessage(offsetStr + "Initializing Toonz environment ...",
                     Qt::AlignCenter, Qt::white);
  a.processEvents();

  // Install run out of contiguous memory callback
  TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
      &toonzRunOutOfContMemHandler);

  // Toonz environment
  initToonzEnv();

  // Initialize thread components
  TThread::init();

  TProjectManager *projectManager = TProjectManager::instance();
  if (Preferences::instance()->isSVNEnabled()) {
    // Read Version Control repositories and add it to project manager as
    // "special" svn project root
    VersionControl::instance()->init();
    QList<SVNRepository> repositories =
        VersionControl::instance()->getRepositories();
    int count = repositories.size();
    for (int i = 0; i < count; i++) {
      SVNRepository r = repositories.at(i);

      TFilePath localPath(r.m_localPath.toStdWString());
      if (!TFileStatus(localPath).doesExist()) {
        try {
          TSystem::mkDir(localPath);
        } catch (TException &e) {
          fatalError(QString::fromStdWString(e.getMessage()));
        }
      }
      projectManager->addSVNProjectsRoot(localPath);
    }
  }

#if defined(MACOSX) && defined(__LP64__)

  // Load the shared memory settings
  int shmmax = Preferences::instance()->getShmMax();
  int shmseg = Preferences::instance()->getShmSeg();
  int shmall = Preferences::instance()->getShmAll();
  int shmmni = Preferences::instance()->getShmMni();

  if (shmall <
      0)  // Make sure that at least 100 MB of shared memory are available
    shmall = (tipc::shm_maxSharedPages() < (100 << 8)) ? (100 << 8) : -1;

  tipc::shm_set(shmmax, shmseg, shmall, shmmni);

#endif

  // DVDirModel must be instantiated after Version Control initialization...
  FolderListenerManager::instance()->addListener(DvDirModel::instance());

  splash.showMessage(offsetStr + "Loading Translator ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Carico la traduzione contenuta in toonz.qm (se ï¿½ presente)
  QString languagePathString =
      QString::fromStdString(::to_string(TEnv::getConfigDir() + "loc"));
#ifndef WIN32
  // the merge of menu on osx can cause problems with different languages with
  // the Preferences menu
  // qt_mac_set_menubar_merge(false);
  languagePathString += "/" + Preferences::instance()->getCurrentLanguage();
#else
  languagePathString += "\\" + Preferences::instance()->getCurrentLanguage();
#endif
  QTranslator translator;
#ifdef LINETEST
  translator.load("linetest", languagePathString);
#else
  translator.load("toonz", languagePathString);
#endif

  // La installo
  a.installTranslator(&translator);

  // Carico la traduzione contenuta in toonzqt.qm (se e' presente)
  QTranslator translator2;
  translator2.load("toonzqt", languagePathString);
  a.installTranslator(&translator2);

  // Carico la traduzione contenuta in tnzcore.qm (se e' presente)
  QTranslator tnzcoreTranslator;
  tnzcoreTranslator.load("tnzcore", languagePathString);
  qApp->installTranslator(&tnzcoreTranslator);

  // Carico la traduzione contenuta in toonzlib.qm (se e' presente)
  QTranslator toonzlibTranslator;
  toonzlibTranslator.load("toonzlib", languagePathString);
  qApp->installTranslator(&toonzlibTranslator);

  // Carico la traduzione contenuta in colorfx.qm (se e' presente)
  QTranslator colorfxTranslator;
  colorfxTranslator.load("colorfx", languagePathString);
  qApp->installTranslator(&colorfxTranslator);

  // Carico la traduzione contenuta in tools.qm
  QTranslator toolTranslator;
  toolTranslator.load("tnztools", languagePathString);
  qApp->installTranslator(&toolTranslator);

  // load translation for file writers properties
  QTranslator imageTranslator;
  imageTranslator.load("image", languagePathString);
  qApp->installTranslator(&imageTranslator);

  QTranslator qtTranslator;
  qtTranslator.load("qt_" + QLocale::system().name(),
                    QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  a.installTranslator(&qtTranslator);

  // Aggiorno la traduzione delle properties di tutti i tools
  TTool::updateToolsPropertiesTranslation();
  // Apply translation to file writers properties
  Tiio::updateFileWritersPropertiesTranslation();

  splash.showMessage(offsetStr + "Loading styles ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // stile
  QApplication::setStyle("windows");

  IconGenerator::setFilmstripIconSize(Preferences::instance()->getIconSize());

  splash.showMessage(offsetStr + "Loading shaders ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  loadShaderInterfaces(ToonzFolder::getLibraryFolder() + TFilePath("shaders"));

  splash.showMessage(offsetStr + "Initializing OpenToonz ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  TTool::setApplication(TApp::instance());
  TApp::instance()->init();

  splash.showMessage(offsetStr + "Loading Plugins...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();
  /* poll the thread ends:
   絶対に必要なわけではないが PluginLoader は中で setup
   ハンドラが常に固有のスレッドで呼ばれるよう main thread queue の blocking
   をしているので
   processEvents を行う必要がある
*/
  while (!PluginLoader::load_entries("")) {
    a.processEvents();
  }

  splash.showMessage(offsetStr + "Creating main window ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  /*-- Layoutファイル名をMainWindowのctorに渡す --*/
  MainWindow w(argumentLayoutFileName);

  splash.showMessage(offsetStr + "Loading style sheet ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Carico lo styleSheet
  QString currentStyle = Preferences::instance()->getCurrentStyleSheetPath();
  a.setStyleSheet(currentStyle);

  TApp::instance()->setMainWindow(&w);
  w.setWindowTitle(applicationFullName);
  if (TEnv::getIsPortable()) {
    splash.showMessage(offsetStr + "Starting OpenToonz Portable ...",
                       Qt::AlignCenter, Qt::white);
  } else {
    splash.showMessage(offsetStr + "Starting main window ...", Qt::AlignCenter,
                       Qt::white);
  }
  a.processEvents();

  TFilePath fp = ToonzFolder::getModuleFile("mainwindow.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  w.restoreGeometry(settings.value("MainWindowGeometry").toByteArray());

#ifndef MACOSX
  // Workaround for the maximized window case: Qt delivers two resize events,
  // one in the normal geometry, before
  // maximizing (why!?), the second afterwards - all inside the following show()
  // call. This makes troublesome for
  // the docking system to correctly restore the saved geometry. Fortunately,
  // MainWindow::showEvent(..) gets called
  // just between the two, so we can disable the currentRoom layout right before
  // showing and re-enable it after
  // the normal resize has happened.
  if (w.isMaximized()) w.getCurrentRoom()->layout()->setEnabled(false);
#endif

  QRect splashGeometry = splash.geometry();
  splash.finish(&w);

  a.setQuitOnLastWindowClosed(false);
  // a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
  if (Preferences::instance()->isLatestVersionCheckEnabled())
    w.checkForUpdates();

  w.show();

  // Show floating panels only after the main window has been shown
  w.startupFloatingPanels();

  CommandManager::instance()->execute(T_Hand);
  if (!loadScenePath.isEmpty()) {
    splash.showMessage(
        QString("Loading file '") + loadScenePath.getQString() + "'...",
        Qt::AlignCenter, Qt::white);

    loadScenePath = loadScenePath.withType("tnz");
    if (TFileStatus(loadScenePath).doesExist()) IoCmd::loadScene(loadScenePath);
  }

  QFont *myFont;
  std::string fontName =
      Preferences::instance()->getInterfaceFont().toStdString();
  std::string isBold =
      Preferences::instance()->getInterfaceFontWeight() ? "Yes" : "No";
  myFont = new QFont(QString::fromStdString(fontName));
  myFont->setPixelSize(EnvSoftwareCurrentFontSize);
  if (strcmp(isBold.c_str(), "Yes") == 0)
    myFont->setBold(true);
  else
    myFont->setBold(false);
  a.setFont(*myFont);

  QAction *action = CommandManager::instance()->getAction("MI_OpenTMessage");
  if (action)
    QObject::connect(TMessageRepository::instance(),
                     SIGNAL(openMessageCenter()), action, SLOT(trigger()));

  QObject::connect(TUndoManager::manager(), SIGNAL(somethingChanged()),
                   TApp::instance()->getCurrentScene(), SLOT(setDirtyFlag()));

#ifdef _WIN32
#ifndef x64
  // On 32-bit architecture, there could be cases in which initialization could
  // alter the
  // FPU floating point control word. I've seen this happen when loading some
  // AVI coded (VFAPI),
  // where 80-bit internal precision was used instead of the standard 64-bit
  // (much faster and
  // sufficient - especially considering that x86 truncates to 64-bit
  // representation anyway).
  // IN ANY CASE, revert to the original control word.
  // In the x64 case these precision changes simply should not take place up to
  // _controlfp_s
  // documentation.
  _controlfp_s(0, fpWord, -1);
#endif
#endif

  a.installEventFilter(TApp::instance());

  int ret = a.exec();

  TUndoManager::manager()->reset();
  PreviewFxManager::instance()->reset();

#ifdef _WIN32
  if (consoleAttached) {
    ::FreeConsole();
  }
#endif

  return ret;
}
