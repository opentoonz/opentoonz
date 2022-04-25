

// Tnz6 includes
#include "mainwindow.h"
#include "flipbook.h"
#include "tapp.h"
#include "iocommand.h"
#include "previewfxmanager.h"
#include "cleanupsettingspopup.h"
#include "filebrowsermodel.h"
#include "expressionreferencemanager.h"

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
#include "toonz/tproject.h"
#include "toonz/scriptengine.h"

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
#include "tfont.h"

#include "kis_tablet_support_win8.h"

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
#include <QHash>

#ifdef _WIN32
#ifndef x64
#include <float.h>
#endif
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

using namespace DVGui;

TEnv::IntVar EnvSoftwareCurrentFontSize("SoftwareCurrentFontSize", 12);

const char *rootVarName     = "TOONZROOT";
const char *systemVarPrefix = "TOONZ";

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
      CRITICAL,
      msg + "\n" +
          QObject::tr("Installing %1 again could fix the problem.")
              .arg(QString::fromStdString(TEnv::getApplicationFullName())));
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
static void initToonzEnv(QHash<QString, QString> &argPathValues) {
  StudioPalette::enable(true);
  TEnv::setRootVarName(rootVarName);
  TEnv::setSystemVarPrefix(systemVarPrefix);

  QHash<QString, QString>::const_iterator i = argPathValues.constBegin();
  while (i != argPathValues.constEnd()) {
    if (!TEnv::setArgPathValue(i.key().toStdString(), i.value().toStdString()))
      DVGui::error(
          QObject::tr("The qualifier %1 is not a valid key name. Skipping.")
              .arg(i.key()));
    ++i;
  }

  QCoreApplication::setOrganizationName("OpenToonz");
  QCoreApplication::setOrganizationDomain("");
  QCoreApplication::setApplicationName(
      QString::fromStdString(TEnv::getApplicationName()));

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
  TFilePath cacheDir = ToonzFolder::getCacheRootFolder();
  if (cacheDir.isEmpty()) cacheDir = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheDir);
}

//-----------------------------------------------------------------------------

static void script_output(int type, const QString &value) {
  if (type == ScriptEngine::ExecutionError ||
      type == ScriptEngine::SyntaxError ||
      type == ScriptEngine::UndefinedEvaluationResult ||
      type == ScriptEngine::Warning)
    std::cerr << value.toStdString() << std::endl;
  else
    std::cout << value.toStdString() << std::endl;
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

  // parsing arguments and qualifiers
  TFilePath loadFilePath;
  QString argumentLayoutFileName = "";
  QHash<QString, QString> argumentPathValues;
  if (argc > 1) {
    TCli::Usage usage(argv[0]);
    TCli::UsageLine usageLine;
    TCli::FilePathArgument loadFileArg(
        "filePath", "Source scene file to open or script file to run");
    TCli::StringQualifier layoutFileQual(
        "-layout filename",
        "Custom layout file to be used, it should be saved in "
        "$TOONZPROFILES\\layouts\\personal\\[CurrentLayoutName].[UserName]\\. "
        "layouts.txt is used by default.");
    usageLine = usageLine + layoutFileQual;

    // system path qualifiers
    std::map<QString, std::unique_ptr<TCli::QualifierT<TFilePath>>>
        systemPathQualMap;
    QString qualKey  = QString("%1ROOT").arg(systemVarPrefix);
    QString qualName = QString("-%1 folderpath").arg(qualKey);
    QString qualHelp =
        QString(
            "%1 path. It will automatically set other system paths to %1 "
            "unless individually specified with other qualifiers.")
            .arg(qualKey);
    systemPathQualMap[qualKey].reset(new TCli::QualifierT<TFilePath>(
        qualName.toStdString(), qualHelp.toStdString()));
    usageLine = usageLine + *systemPathQualMap[qualKey];

    const std::map<std::string, std::string> &spm = TEnv::getSystemPathMap();
    for (auto itr = spm.begin(); itr != spm.end(); ++itr) {
      qualKey = QString("%1%2")
                    .arg(systemVarPrefix)
                    .arg(QString::fromStdString((*itr).first));
      qualName = QString("-%1 folderpath").arg(qualKey);
      qualHelp = QString("%1 path.").arg(qualKey);
      systemPathQualMap[qualKey].reset(new TCli::QualifierT<TFilePath>(
          qualName.toStdString(), qualHelp.toStdString()));
      usageLine = usageLine + *systemPathQualMap[qualKey];
    }
    usage.add(usageLine);
    usage.add(usageLine + loadFileArg);

    if (!usage.parse(argc, argv)) exit(1);

    loadFilePath = loadFileArg.getValue();
    if (layoutFileQual.isSelected())
      argumentLayoutFileName =
          QString::fromStdString(layoutFileQual.getValue());
    for (auto q_itr = systemPathQualMap.begin();
         q_itr != systemPathQualMap.end(); ++q_itr) {
      if (q_itr->second->isSelected())
        argumentPathValues.insert(q_itr->first,
                                  q_itr->second->getValue().getQString());
    }

    argc = 1;
  }

// Enables high-DPI scaling. This attribute must be set before QApplication is
// constructed. Available from Qt 5.6.
#if QT_VERSION >= 0x050600
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QApplication a(argc, argv);

#ifdef MACOSX
  // This workaround is to avoid missing left button problem on Qt5.6.0.
  // To invalidate m_rightButtonClicked in Qt/qnsview.mm, sending
  // NSLeftButtonDown event before NSLeftMouseDragged event propagated to
  // QApplication. See more details in ../mousedragfilter/mousedragfilter.mm.

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
  // angle,	force Qt to use desktop OpenGL
  // FIXME: This options should be called before constructing the application.
  // Thus, ANGLE seems to be enabled as of now.
  a.setAttribute(Qt::AA_UseDesktopOpenGL, true);
#endif

  // Some Qt objects are destroyed badly without a living qApp. So, we must
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
#if defined(_WIN32) && QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  // Compress tablet events with application attributes instead of implementing
  // the delay-timer by ourselves
  a.setAttribute(Qt::AA_CompressHighFrequencyEvents);
  a.setAttribute(Qt::AA_CompressTabletEvents);
#endif

#ifdef _WIN32
  // This attribute is set to make menubar icon to be always (16 x devPixRatio).
  // Without this attribute the menu bar icon size becomes the same as tool bar
  // when Windows scale is in 125%. Currently hiding the menu bar icon is done
  // by setting transparent pixmap only in menu bar icon size. So the size must
  // be different between for menu bar and for tool bar.
  a.setAttribute(Qt::AA_Use96Dpi);
#endif

  // Set the app's locale for numeric stuff to standard C. This is important for
  // atof() and similar
  // calls that are locale-dependent.
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

  // Set icon theme search paths
  QStringList themeSearchPathsList = {":/icons"};
  QIcon::setThemeSearchPaths(themeSearchPathsList);
  // qDebug() << "All icon theme search paths:" << QIcon::themeSearchPaths();

  // Set show icons in menus flag (use iconVisibleInMenu to disable selectively)
  QApplication::instance()->setAttribute(Qt::AA_DontShowIconsInMenus, false);

  TEnv::setApplicationFileName(argv[0]);

  // splash screen
  QPixmap splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));

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

  bool isRunScript = (loadFilePath.getType() == "toonzscript");

  QSplashScreen splash(splashPixmap);
  if (!isRunScript) splash.show();
  a.processEvents();

  splash.showMessage(offsetStr + "Initializing QGLFormat...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // OpenGL
  QGLFormat fmt;
  fmt.setAlpha(true);
  fmt.setStencil(true);
  QGLFormat::setDefaultFormat(fmt);

  glutInit(&argc, argv);

  splash.showMessage(offsetStr + "Initializing Toonz environment ...",
                     Qt::AlignCenter, Qt::white);
  a.processEvents();

  // Install run out of contiguous memory callback
  TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
      &toonzRunOutOfContMemHandler);

  // Toonz environment
  initToonzEnv(argumentPathValues);

  // prepare for 30bit display
  if (Preferences::instance()->is30bitDisplayEnabled()) {
    QSurfaceFormat sFmt = QSurfaceFormat::defaultFormat();
    sFmt.setRedBufferSize(10);
    sFmt.setGreenBufferSize(10);
    sFmt.setBlueBufferSize(10);
    sFmt.setAlphaBufferSize(2);
    QSurfaceFormat::setDefaultFormat(sFmt);
  }

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
  translator.load("toonz", languagePathString);

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

  // Force to have left-to-right layout direction in any language environment.
  // This function has to be called after installTranslator().
  a.setLayoutDirection(Qt::LeftToRight);

  splash.showMessage(offsetStr + "Loading styles ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Set default start icon theme
  QIcon::setThemeName(Preferences::instance()->getIconTheme() ? "dark"
                                                              : "light");
  // qDebug() << "Icon theme name:" << QIcon::themeName();

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

  if (isRunScript) {
    // load script
    if (TFileStatus(loadFilePath).doesExist()) {
      // find project for this script file
      TProjectManager *pm    = TProjectManager::instance();
      TProjectP sceneProject = pm->loadSceneProject(loadFilePath);
      TFilePath oldProjectPath;
      if (!sceneProject) {
        std::cerr << QObject::tr(
                         "It is not possible to load the scene %1 because it "
                         "does not "
                         "belong to any project.")
                         .arg(loadFilePath.getQString())
                         .toStdString()
                  << std::endl;
        return 1;
      }
      if (sceneProject && !sceneProject->isCurrent()) {
        oldProjectPath = pm->getCurrentProjectPath();
        pm->setCurrentProjectPath(sceneProject->getProjectPath());
      }
      ScriptEngine engine;
      QObject::connect(&engine, &ScriptEngine::output, script_output);
      QString s = QString::fromStdWString(loadFilePath.getWideString())
                      .replace("\\", "\\\\")
                      .replace("\"", "\\\"");
      QString cmd = QString("run(\"%1\")").arg(s);
      engine.evaluate(cmd);
      engine.wait();
      if (!oldProjectPath.isEmpty()) pm->setCurrentProjectPath(oldProjectPath);
      return 1;
    } else {
      std::cerr << QObject::tr("Script file %1 does not exists.")
                       .arg(loadFilePath.getQString())
                       .toStdString()
                << std::endl;
      return 1;
    }
  }

#ifdef _WIN32
  // http://doc.qt.io/qt-5/windows-issues.html#fullscreen-opengl-based-windows
  if (w.windowHandle())
    QWindowsWindowFunctions::setHasBorderInFullScreen(w.windowHandle(), true);
#endif

    // Qt have started to support Windows Ink from 5.12.
    // Unlike WinTab API used in Qt 5.9 the tablet behaviors are different and
    // are (at least, for OT) problematic. The customized Qt5.15.2 are made with
    // cherry-picking the WinTab feature to be officially introduced from 6.0.
    // See https://github.com/shun-iwasawa/qt5/releases/tag/v5.15.2_wintab for
    // details. The following feature can only be used with the customized Qt,
    // with WITH_WINTAB build option, and in Windows-x64 build.

#ifdef WITH_WINTAB
  bool useQtNativeWinInk = Preferences::instance()->isQtNativeWinInkEnabled();
  QWindowsWindowFunctions::setWinTabEnabled(!useQtNativeWinInk);
#endif

  splash.showMessage(offsetStr + "Loading style sheet ...", Qt::AlignCenter,
                     Qt::white);
  a.processEvents();

  // Carico lo styleSheet
  QString currentStyle = Preferences::instance()->getCurrentStyleSheet();
  a.setStyleSheet(currentStyle);

  w.setWindowTitle(QString::fromStdString(TEnv::getApplicationFullName()));
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
  if (settings.contains("MainWindowGeometry"))
    w.restoreGeometry(settings.value("MainWindowGeometry").toByteArray());
  else  // maximize window on the first launch
    w.setWindowState(w.windowState() | Qt::WindowMaximized);

  ExpressionReferenceManager::instance()->init();

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
  if (!loadFilePath.isEmpty()) {
    splash.showMessage(
        QString("Loading file '") + loadFilePath.getQString() + "'...",
        Qt::AlignCenter, Qt::white);
    loadFilePath = loadFilePath.withType("tnz");
    if (TFileStatus(loadFilePath).doesExist()) IoCmd::loadScene(loadFilePath);
  }

  QFont *myFont;
  QString fontName  = Preferences::instance()->getInterfaceFont();
  QString fontStyle = Preferences::instance()->getInterfaceFontStyle();

  TFontManager *fontMgr = TFontManager::instance();
  std::vector<std::wstring> typefaces;
  bool isBold = false, isItalic = false, hasKerning = false;
  try {
    fontMgr->loadFontNames();
    fontMgr->setFamily(fontName.toStdWString());
    fontMgr->getAllTypefaces(typefaces);
    isBold     = fontMgr->isBold(fontName, fontStyle);
    isItalic   = fontMgr->isItalic(fontName, fontStyle);
    hasKerning = fontMgr->hasKerning();
  } catch (TFontCreationError &) {
    // Do nothing. A default font should load
  }

  myFont = new QFont(fontName);
  myFont->setPixelSize(EnvSoftwareCurrentFontSize);
  myFont->setBold(isBold);
  myFont->setItalic(isItalic);
  myFont->setKerning(hasKerning);

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

#ifdef _WIN32
  if (Preferences::instance()->isWinInkEnabled()) {
    KisTabletSupportWin8 *penFilter = new KisTabletSupportWin8();
    if (penFilter->init()) {
      a.installNativeEventFilter(penFilter);
    } else {
      delete penFilter;
    }
  }
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
