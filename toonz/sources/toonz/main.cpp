

// Tnz6 includes
#include "audiorecordingpopup.h"
#include "crashhandler.h"
#include "mainwindow.h"
#include "onionskinmaskgui.h"
#include "sceneviewer.h"
#include "ruler.h"
#include "xsheetviewer.h"
#include "flipbook.h"
#include "tapp.h"
#include "iocommand.h"
#include "previewfxmanager.h"
#include "previewer.h"
#include "cleanupsettingspopup.h"
#include "filebrowsermodel.h"
#include "expressionreferencemanager.h"
#include "scriptconsolepanel.h"
#include "thirdparty.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"
#include "tools/cursormanager.h"
#include "tools/rasterselection.h"
#include "tools/strokeselection.h"
#include "../tnztools/selectiontool.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tmessageviewer.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/pluginloader.h"
#include "toonzqt/qtcompat.h"
#include "toonzqt/scriptconsole.h"
#include "toonzqt/tselectionhandle.h"

// TnzStdfx includes
#include "stdfx/shaderfx.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/palettecontroller.h"
#include "toonz/studiopalette.h"
#include "toonz/stylemanager.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tproject.h"
#include "toonz/scriptengine.h"
#include "toonz/stage.h"
#include "toonz/tstageobject.h"
#include "toonz/stagevisitor.h"
#include "toonz/movierenderer.h"
#include "toonz/scenefx.h"
#include "toonz/dpiscale.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"

// TnzSound includes
#include "tnzsound.h"
#include "tsound_io.h"

// TnzImage includes
#include "tnzimage.h"

// TnzBase includes
#include "permissionsmanager.h"
#include "tenv.h"
#include "tproperty.h"
#include "tcli.h"
#include "toutputproperties.h"

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
#include "tfxutil.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "timage.h"
#include "timageinfo.h"
#include "tstroke.h"
#include "tsimplecolorstyles.h"
#include "tpalette.h"
#include "toonz/imagestyles.h"
#include "tvectorbrushstyle.h"
#include "tvectorimage.h"
#include "tfont.h"

#include "kis_tablet_support_win8.h"

#ifdef MACOSX
#include "tipc.h"
#endif

// Qt includes
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QBuffer>
#include <QSplashScreen>
#include <QScreen>
#include <QStyle>
#include <QSurfaceFormat>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGLPixelBuffer>
#include <QGLFormat>
#endif
#include <QTranslator>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMenu>
#include <QPixmap>
#include <QSettings>
#include <QLibraryInfo>
#include <QAudio>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioSink>
#include <QAudioSource>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaDevices>
#else
#include <QAudio>
#include <QAudioDeviceInfo>
#include <QCameraInfo>
#endif
#include <QEventLoop>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QInputDevice>
#include <QPointingDevice>
#endif
#include <QTabletEvent>
#include <QTextBlock>
#include <QTimer>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <future>
#include <thread>

#ifdef _WIN32
#ifndef x64
#include <float.h>
#endif
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

using namespace DVGui;

TEnv::IntVar EnvSoftwareCurrentFontSize("SoftwareCurrentFontSize", 12);

extern ToggleCommandHandler fieldGuideToggle;
extern ToggleCommandHandler safeAreaToggle;
extern ToggleCommandHandler viewCameraToggle;
extern ToggleCommandHandler viewGuideToggle;
extern ToggleCommandHandler viewRulerToggle;
extern TEnv::StringVar EnvSafeAreaName;

// These are the same as the default values. See tenv.cpp and tversion.h
const char* rootVarName     = "TOONZROOT";
const char* systemVarPrefix = "TOONZ";

#ifdef MACOSX
#include "tthread.h"
void postThreadMsg(TThread::Message*) {}
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
static void initToonzEnv(QHash<QString, QString>& argPathValues,
                         bool preferWritablePlatformCache = true) {
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
  QGuiApplication::setDesktopFileName("io.github.OpenToonz");
  QGuiApplication::setWindowIcon(QIcon::fromTheme("io.github.OpenToonz"));
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

  // Setup third party
  ThirdParty::initialize();

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

  TProjectManager* projectManager = TProjectManager::instance();

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
  TFilePath cacheDir;
  if (preferWritablePlatformCache) cacheDir = ToonzFolder::getCacheRootFolder();
  if (cacheDir.isEmpty()) cacheDir = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheDir);
}

//-----------------------------------------------------------------------------

static std::atomic_bool gScriptHadError(false);

static void script_output(int type, const QString& value) {
  if (type == ScriptEngine::ExecutionError || type == ScriptEngine::SyntaxError)
    gScriptHadError.store(true);

  if (type == ScriptEngine::ExecutionError ||
      type == ScriptEngine::SyntaxError ||
      type == ScriptEngine::UndefinedEvaluationResult ||
      type == ScriptEngine::Warning)
    std::cerr << value.toStdString() << std::endl;
  else
    std::cout << value.toStdString() << std::endl;
}

static int run_script(const TFilePath& loadFilePath) {
  if (!TFileStatus(loadFilePath).doesExist()) {
    std::cerr << QObject::tr("Script file %1 does not exists.")
                     .arg(loadFilePath.getQString())
                     .toStdString()
              << std::endl;
    return 1;
  }

  TProjectManager* pm = TProjectManager::instance();
  auto sceneProject   = pm->loadSceneProject(loadFilePath);
  TFilePath oldProjectPath;
  if (!sceneProject) {
    std::cerr << QObject::tr(
                     "It is not possible to load the scene %1 because it does "
                     "not belong to any project.")
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
#if OPENTOONZ_QT_MAJOR >= 6
  QString result = engine.runScriptFile(s);
  if (!result.isEmpty())
    engine.emitOutput(ScriptEngine::EvaluationResult, result);
#else
  QString cmd = QString("run(\"%1\")").arg(s);
  engine.evaluate(cmd);
  engine.wait();
#endif

  if (!oldProjectPath.isEmpty()) pm->setCurrentProjectPath(oldProjectPath);
  return gScriptHadError.load() ? 1 : 0;
}

static void write_gui_smoke_status(const QString& action, const QString& status,
                                   const QStringList& details = QStringList()) {
  QString statusPath = qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  if (statusPath.isEmpty()) return;

  QFileInfo statusInfo(statusPath);
  QDir().mkpath(statusInfo.absolutePath());

  QFile statusFile(statusPath);
  if (!statusFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    std::cerr << "Could not write GUI smoke status file: "
              << statusPath.toStdString() << std::endl;
    return;
  }

  QTextStream out(&statusFile);
  out << "action=" << action << "\n";
  out << "status=" << status << "\n";
  for (const QString& detail : details) out << detail << "\n";
}

static void log_gui_smoke_progress(const char* message) {
  const char* action = std::getenv("OPENTOONZ_GUI_SMOKE_ACTION");
  if (!action || !*action) return;

  std::cerr << "[gui-smoke] " << message << std::endl;
}

static QString gui_smoke_scene_name(const QString& requestedSceneName) {
  QString sceneName = requestedSceneName.trimmed();
  return sceneName.isEmpty() ? QStringLiteral("qt6_gui_smoke") : sceneName;
}

static TFilePath gui_smoke_scene_path(const QString& sceneName) {
  auto project = TProjectManager::instance()->getCurrentProject();
  return project->getScenesPath() +
         TFilePath(sceneName.toStdWString() + L".tnz");
}

static bool add_gui_smoke_frame(TXshSimpleLevel* level, const TFrameId& fid) {
  if (!level) return false;

  if (level->getType() == OVL_XSHLEVEL) {
    TRaster32P raster(32, 32);
    raster->clear();
    level->setFrame(fid, TRasterImageP(raster));
    return true;
  }

  if (level->getType() == PLI_XSHLEVEL) {
    TVectorImageP image = new TVectorImage();
    level->setFrame(fid, image);
    return true;
  }

  return false;
}

static QString gui_smoke_status_value(QString value) {
  value.replace('\n', ' ');
  value.replace('\r', ' ');
  return value;
}

static QString gui_smoke_joined_values(const QStringList& values) {
  QStringList sanitized;
  for (const QString& value : values) {
    sanitized << gui_smoke_status_value(value);
  }
  return sanitized.join('|');
}

static QString gui_smoke_rect_value(const TRect& rect) {
  return QStringLiteral("%1,%2,%3,%4")
      .arg(rect.x0)
      .arg(rect.y0)
      .arg(rect.x1)
      .arg(rect.y1);
}

static QString gui_smoke_rectd_value(const TRectD& rect) {
  if (rect == TConsts::infiniteRectD) return QStringLiteral("infinite");
  return QStringLiteral("%1,%2,%3,%4")
      .arg(rect.x0, 0, 'f', 3)
      .arg(rect.y0, 0, 'f', 3)
      .arg(rect.x1, 0, 'f', 3)
      .arg(rect.y1, 0, 'f', 3);
}

static QString gui_smoke_affine_value(const TAffine& affine) {
  return QStringLiteral("%1,%2,%3,%4,%5,%6")
      .arg(affine.a11, 0, 'f', 6)
      .arg(affine.a12, 0, 'f', 6)
      .arg(affine.a13, 0, 'f', 6)
      .arg(affine.a21, 0, 'f', 6)
      .arg(affine.a22, 0, 'f', 6)
      .arg(affine.a23, 0, 'f', 6);
}

static QString gui_smoke_fx_value(TFx* fx) {
  if (!fx) return QStringLiteral("missing");
  return QString("%1:%2")
      .arg(QString::fromStdString(fx->getFxType()))
      .arg(QString::number(fx->getIdentifier()));
}

static QStringList gui_smoke_fx_bbox_details(const QString& prefix,
                                             const TFxP& fx, double frame,
                                             const TRenderSettings& settings) {
  QStringList details;
  TRasterFx* rasterFx = dynamic_cast<TRasterFx*>(fx.getPointer());
  TRectD bbox;
  const bool bboxOk = rasterFx && rasterFx->getBBox(frame, bbox, settings);
  details << QString("%1Fx=%2").arg(prefix).arg(
                 gui_smoke_fx_value(fx.getPointer()))
          << QString("%1BBoxOk=%2")
                 .arg(prefix)
                 .arg(bboxOk ? QStringLiteral("true") : QStringLiteral("false"))
          << QString("%1BBox=%2")
                 .arg(prefix)
                 .arg(bboxOk ? gui_smoke_rectd_value(bbox)
                             : QStringLiteral("none"));
  return details;
}

static void gui_smoke_pump_events(int msec = 50) {
  QApplication::processEvents(QEventLoop::AllEvents, msec);
  QEventLoop loop;
  QTimer::singleShot(msec, &loop, &QEventLoop::quit);
  loop.exec();
  QApplication::processEvents(QEventLoop::AllEvents, msec);
}

static int gui_smoke_visible_flipbook_count() {
  int count = 0;
  for (QWidget* widget : QApplication::allWidgets()) {
    FlipBook* flipBook = qobject_cast<FlipBook*>(widget);
    if (flipBook && flipBook->isVisible()) ++count;
  }
  return count;
}

static std::vector<FlipBook*> gui_smoke_visible_preview_flipbooks(
    TFx* fx, TXsheet* xsheet) {
  std::vector<FlipBook*> flipbooks;
  for (QWidget* widget : QApplication::allWidgets()) {
    FlipBook* flipbook = qobject_cast<FlipBook*>(widget);
    if (flipbook && flipbook->isVisible() && flipbook->getPreviewedFx() == fx &&
        flipbook->getPreviewXsheet() == xsheet) {
      flipbooks.push_back(flipbook);
    }
  }
  return flipbooks;
}

static QStringList gui_smoke_script_console_view_details() {
  log_gui_smoke_progress("script-console-view-start");
  QStringList details;
  const int beforeFlipbooks = gui_smoke_visible_flipbook_count();

  ScriptConsolePanel panel(TApp::instance()->getMainWindow());
  log_gui_smoke_progress("script-console-view-panel-created");
  ScriptConsole* console = panel.findChild<ScriptConsole*>();
  if (!console) {
    return details << QStringLiteral("scriptConsoleViewProbe=missing-console");
  }

  ScriptEngine* engine = console->getEngine();
  if (!engine) {
    return details << QStringLiteral("scriptConsoleViewProbe=missing-engine");
  }

  bool hadError = false;
  QStringList outputs;
  QObject::connect(
      engine, &ScriptEngine::output, &panel,
      [&](int type, const QString& value) {
        const bool expectedViewError =
            type == ScriptEngine::SyntaxError &&
            value.contains(
                QStringLiteral("expected one argument : an image or a level"));
        if (!expectedViewError && (type == ScriptEngine::ExecutionError ||
                                   type == ScriptEngine::SyntaxError)) {
          hadError = true;
        }
        outputs << QString("%1:%2").arg(type).arg(
            gui_smoke_status_value(value));
      });

  auto executeAndWait = [&](const QString& cmd) {
    panel.executeCommand(cmd);
    QElapsedTimer timer;
    timer.start();
    while (engine->isEvaluating() && timer.elapsed() < 10000)
      gui_smoke_pump_events(50);
    gui_smoke_pump_events(200);
  };

  const TFilePath consoleRunScriptPath =
      ToonzFolder::getLibraryFolder() + "scripts" +
      "qt_script_console_run_child.toonzscript";
  const TFilePath consoleRunScriptFolder = consoleRunScriptPath.getParentDir();
  const QString consoleRunScriptFolderString =
      QString::fromStdWString(consoleRunScriptFolder.getWideString());
  const QString consoleRunScriptPathString =
      QString::fromStdWString(consoleRunScriptPath.getWideString());
  bool consoleRunScriptReady = QDir().mkpath(consoleRunScriptFolderString);
  if (consoleRunScriptReady) {
    QFile consoleRunScript(consoleRunScriptPathString);
    if (consoleRunScript.open(QIODevice::WriteOnly | QIODevice::Truncate |
                              QIODevice::Text)) {
      QTextStream stream(&consoleRunScript);
      stream << "print(\"qt-script-console-run-child\", 4 + 5);\n"
             << "qtScriptConsoleRunChildGlobal = \"child-global\";\n"
             << "\"child-return\";\n";
      consoleRunScript.close();
    } else {
      consoleRunScriptReady = false;
    }
  }

  log_gui_smoke_progress("script-console-view-command-view");
  executeAndWait(
      QStringLiteral("var builder = new ImageBuilder(16, 12, \"Raster\");"
                     "builder.fill(\"#FF0000\");"
                     "var image = builder.image;"
                     "view(image);"
                     "var level = new Level();"
                     "level.setFrame(\"1\", image);"
                     "view(level);"
                     "print(\"qt-script-console-view\", image.type, "
                     "image.width, image.height, level.type, "
                     "level.frameCount);"));
  log_gui_smoke_progress("script-console-view-command-warning");
  executeAndWait(QStringLiteral("warning(\"qt-script-console-warning\");"));
  log_gui_smoke_progress("script-console-view-command-repeat");
  executeAndWait(QStringLiteral("print(\"qt-script-console-repeat\", 1 + 2);"));
  if (consoleRunScriptReady) {
    log_gui_smoke_progress("script-console-view-command-run");
    executeAndWait(QStringLiteral(
        "var runResult = run(\"qt_script_console_run_child.toonzscript\");"
        "print(\"qt-script-console-run\", runResult, "
        "qtScriptConsoleRunChildGlobal);"));
  }
  log_gui_smoke_progress("script-console-view-command-error");
  executeAndWait(QStringLiteral("view();"));
  log_gui_smoke_progress("script-console-view-commands-done");

  console->setFocus();
  console->moveCursor(QTextCursor::End);
  QKeyEvent historyUpEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
  QApplication::sendEvent(console, &historyUpEvent);
  gui_smoke_pump_events(100);
  const QString historyLine = console->document()->lastBlock().text();

  const int afterFlipbooks = gui_smoke_visible_flipbook_count();
  bool sawMarker           = false;
  bool sawWarning          = false;
  bool sawRepeat           = false;
  bool sawRunChild         = false;
  bool sawRun              = false;
  bool sawExpectedError    = false;
  for (const QString& output : outputs) {
    if (output.contains(QStringLiteral("qt-script-console-view"))) {
      sawMarker = true;
    } else if (output.contains(QStringLiteral("qt-script-console-warning"))) {
      sawWarning = true;
    } else if (output.contains(QStringLiteral("qt-script-console-repeat"))) {
      sawRepeat = true;
    } else if (output.contains(QStringLiteral("qt-script-console-run-child"))) {
      sawRunChild = true;
    } else if (output.contains(QStringLiteral("qt-script-console-run"))) {
      sawRun = true;
    } else if (output.contains(QStringLiteral(
                   "expected one argument : an image or a level"))) {
      sawExpectedError = true;
    }
  }
  const bool historyOk = historyLine.contains(QStringLiteral("view();"));

  details
      << QString("scriptConsoleOutput=%1").arg(gui_smoke_joined_values(outputs))
      << QString("scriptConsoleHistoryLine=%1")
             .arg(gui_smoke_status_value(historyLine))
      << QString("scriptConsoleRunScriptReady=%1")
             .arg(consoleRunScriptReady ? QStringLiteral("yes")
                                        : QStringLiteral("no"))
      << QString("visibleFlipbooksBefore=%1").arg(beforeFlipbooks)
      << QString("visibleFlipbooksAfter=%1").arg(afterFlipbooks)
      << QString("visibleFlipbookDelta=%1")
             .arg(afterFlipbooks - beforeFlipbooks)
      << QString("scriptConsoleViewProbe=%1")
             .arg(!hadError && sawMarker && sawWarning && sawRepeat &&
                          sawRunChild && sawRun && sawExpectedError &&
                          afterFlipbooks > beforeFlipbooks && historyOk
                      ? QStringLiteral("ok")
                      : QStringLiteral("error"));
  return details;
}

static SceneViewer* gui_smoke_resolve_scene_viewer() {
  SceneViewer* viewer = TApp::instance()->getActiveViewer();
  if (viewer) return viewer;

  QMainWindow* mainWindow = TApp::instance()->getMainWindow();
  viewer = mainWindow ? mainWindow->findChild<SceneViewer*>() : nullptr;
  if (viewer) TApp::instance()->setActiveViewer(viewer);
  return viewer;
}

static bool gui_smoke_set_view_toggle(const char* commandId,
                                      ToggleCommandHandler& toggle,
                                      bool enabled) {
  QAction* action = CommandManager::instance()->getAction(commandId);
  if (action) action->setChecked(enabled);
  toggle.setStatus(enabled);
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  return action != nullptr && toggle.getStatus() == enabled;
}

static bool gui_smoke_set_field_guide_overlay(bool enabled) {
  return gui_smoke_set_view_toggle("MI_FieldGuide", fieldGuideToggle, enabled);
}

static bool gui_smoke_set_safe_area_overlay(bool enabled) {
  return gui_smoke_set_view_toggle("MI_SafeArea", safeAreaToggle, enabled);
}

static bool gui_smoke_set_view_camera_overlay(bool enabled) {
  return gui_smoke_set_view_toggle("MI_ViewCamera", viewCameraToggle, enabled);
}

static bool gui_smoke_set_view_guide_overlay(bool enabled) {
  return gui_smoke_set_view_toggle("MI_ViewGuide", viewGuideToggle, enabled);
}

static bool gui_smoke_set_view_ruler_overlay(bool enabled) {
  return gui_smoke_set_view_toggle("MI_ViewRuler", viewRulerToggle, enabled);
}

static QImage gui_smoke_grab_viewer_frame(SceneViewer* viewer) {
  if (!viewer) return QImage();

  viewer->GLInvalidateAll();
  viewer->update();
  gui_smoke_pump_events();
  viewer->repaint();
  gui_smoke_pump_events();
  return viewer->grabFramebuffer().convertToFormat(QImage::Format_ARGB32);
}

struct GuiSmokeImageStats {
  int sampleCount          = 0;
  int changedPixels        = 0;
  int redPixels            = 0;
  int changedRedPixels     = 0;
  int greenPixels          = 0;
  int changedGreenPixels   = 0;
  int bluePixels           = 0;
  int changedBluePixels    = 0;
  int grayPixels           = 0;
  int changedGrayPixels    = 0;
  int changedNeutralPixels = 0;
  int onionPixels          = 0;
  int baselineOnionPixels  = 0;
};

struct GuiSmokeWidgetImageStats {
  int sampleCount         = 0;
  int nonBackgroundPixels = 0;
  int changedPixels       = 0;
  int dominantPixelCount  = 0;
  int distinctColors      = 0;
};

struct GuiSmokeGuideLineStats {
  int changedNeutralPixels                = 0;
  int verticalLineColumns                 = 0;
  int horizontalLineRows                  = 0;
  int strongestVerticalLinePixels         = 0;
  int strongestHorizontalLinePixels       = 0;
  int strongestVerticalLineColumn         = -1;
  int strongestHorizontalLineRow          = -1;
  int strongestVerticalLineDashSegments   = 0;
  int strongestHorizontalLineDashSegments = 0;
};

struct GuiSmokeRasterStats {
  int sampleCount  = 0;
  int opaquePixels = 0;
  int redPixels    = 0;
  int greenPixels  = 0;
};

static bool gui_smoke_is_onion_pixel(int red, int green, int blue) {
  return red > 220 && green > 150 && blue > 180 && red > green &&
         blue > green && std::abs(red - blue) < 80;
}

static bool gui_smoke_is_green_overlay_pixel(int red, int green, int blue) {
  return green > 120 && green > red * 2 && green > blue * 2;
}

static bool gui_smoke_is_blue_overlay_pixel(int red, int green, int blue) {
  return blue > 120 && blue > red * 2 && blue > green * 2;
}

static bool gui_smoke_is_gray_overlay_pixel(int red, int green, int blue) {
  const int maxChannel = std::max({red, green, blue});
  const int minChannel = std::min({red, green, blue});
  return minChannel >= 70 && maxChannel <= 170 && maxChannel - minChannel <= 20;
}

static bool gui_smoke_is_neutral_overlay_pixel(int red, int green, int blue) {
  const int maxChannel = std::max({red, green, blue});
  const int minChannel = std::min({red, green, blue});
  return minChannel >= 70 && maxChannel < 252 && maxChannel - minChannel <= 24;
}

static GuiSmokeImageStats gui_smoke_analyze_viewer_frame(
    const QImage& image, const QImage& baseline = QImage()) {
  GuiSmokeImageStats stats;
  if (image.isNull()) return stats;

  const bool compareBaseline = !baseline.isNull() &&
                               baseline.width() == image.width() &&
                               baseline.height() == image.height();
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* scanLine =
        reinterpret_cast<const QRgb*>(image.constScanLine(y));
    const QRgb* baselineLine =
        compareBaseline
            ? reinterpret_cast<const QRgb*>(baseline.constScanLine(y))
            : nullptr;
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = scanLine[x];
      const int red    = qRed(pixel);
      const int green  = qGreen(pixel);
      const int blue   = qBlue(pixel);
      ++stats.sampleCount;

      if (red > 120 && red > green * 2 && red > blue * 2) ++stats.redPixels;
      if (gui_smoke_is_green_overlay_pixel(red, green, blue))
        ++stats.greenPixels;
      if (gui_smoke_is_blue_overlay_pixel(red, green, blue)) ++stats.bluePixels;
      if (gui_smoke_is_gray_overlay_pixel(red, green, blue)) ++stats.grayPixels;
      if (gui_smoke_is_onion_pixel(red, green, blue)) ++stats.onionPixels;

      if (baselineLine) {
        const QRgb oldPixel = baselineLine[x];
        const int oldRed    = qRed(oldPixel);
        const int oldGreen  = qGreen(oldPixel);
        const int oldBlue   = qBlue(oldPixel);
        const int delta = std::abs(red - oldRed) + std::abs(green - oldGreen) +
                          std::abs(blue - oldBlue);
        if (delta > 48) {
          ++stats.changedPixels;
          if (red > 120 && red > green * 2 && red > blue * 2)
            ++stats.changedRedPixels;
          if (gui_smoke_is_green_overlay_pixel(red, green, blue))
            ++stats.changedGreenPixels;
          if (gui_smoke_is_blue_overlay_pixel(red, green, blue))
            ++stats.changedBluePixels;
          if (gui_smoke_is_gray_overlay_pixel(red, green, blue))
            ++stats.changedGrayPixels;
          if (gui_smoke_is_neutral_overlay_pixel(red, green, blue))
            ++stats.changedNeutralPixels;
        }
        if (gui_smoke_is_onion_pixel(oldRed, oldGreen, oldBlue))
          ++stats.baselineOnionPixels;
      }
    }
  }

  return stats;
}

static int gui_smoke_pixel_delta(QRgb a, QRgb b) {
  return std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) +
         std::abs(qBlue(a) - qBlue(b));
}

static bool gui_smoke_is_changed_neutral_pixel(const QImage& image,
                                               const QImage& baseline, int x,
                                               int y) {
  if (image.isNull() || baseline.isNull() || image.size() != baseline.size() ||
      x < 0 || x >= image.width() || y < 0 || y >= image.height()) {
    return false;
  }

  const QRgb pixel = reinterpret_cast<const QRgb*>(image.constScanLine(y))[x];
  const QRgb oldPixel =
      reinterpret_cast<const QRgb*>(baseline.constScanLine(y))[x];
  const int red   = qRed(pixel);
  const int green = qGreen(pixel);
  const int blue  = qBlue(pixel);
  return gui_smoke_pixel_delta(pixel, oldPixel) > 48 &&
         gui_smoke_is_neutral_overlay_pixel(red, green, blue);
}

static int gui_smoke_count_changed_neutral_segments(const QImage& image,
                                                    const QImage& baseline,
                                                    bool vertical,
                                                    int coordinate) {
  if (image.isNull() || baseline.isNull() || image.size() != baseline.size())
    return 0;

  const int limit = vertical ? image.height() : image.width();
  bool inSegment  = false;
  int segments    = 0;
  for (int i = 0; i < limit; ++i) {
    const int x = vertical ? coordinate : i;
    const int y = vertical ? i : coordinate;
    const bool active =
        gui_smoke_is_changed_neutral_pixel(image, baseline, x, y);
    if (active && !inSegment) {
      ++segments;
      inSegment = true;
    } else if (!active) {
      inSegment = false;
    }
  }
  return segments;
}

static GuiSmokeGuideLineStats gui_smoke_analyze_guide_lines(
    const QImage& image, const QImage& baseline) {
  GuiSmokeGuideLineStats stats;
  if (image.isNull() || baseline.isNull() || image.size() != baseline.size())
    return stats;

  std::vector<int> columnCounts(image.width(), 0);
  std::vector<int> rowCounts(image.height(), 0);
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (!gui_smoke_is_changed_neutral_pixel(image, baseline, x, y)) continue;
      ++stats.changedNeutralPixels;
      ++columnCounts[x];
      ++rowCounts[y];
    }
  }

  const int columnThreshold = std::max(24, image.height() / 12);
  const int rowThreshold    = std::max(24, image.width() / 12);
  for (int x = 0; x < static_cast<int>(columnCounts.size()); ++x) {
    if (columnCounts[x] >= columnThreshold) ++stats.verticalLineColumns;
    if (columnCounts[x] > stats.strongestVerticalLinePixels) {
      stats.strongestVerticalLinePixels = columnCounts[x];
      stats.strongestVerticalLineColumn = x;
    }
  }
  for (int y = 0; y < static_cast<int>(rowCounts.size()); ++y) {
    if (rowCounts[y] >= rowThreshold) ++stats.horizontalLineRows;
    if (rowCounts[y] > stats.strongestHorizontalLinePixels) {
      stats.strongestHorizontalLinePixels = rowCounts[y];
      stats.strongestHorizontalLineRow    = y;
    }
  }

  if (stats.strongestVerticalLineColumn >= 0) {
    stats.strongestVerticalLineDashSegments =
        gui_smoke_count_changed_neutral_segments(
            image, baseline, true, stats.strongestVerticalLineColumn);
  }
  if (stats.strongestHorizontalLineRow >= 0) {
    stats.strongestHorizontalLineDashSegments =
        gui_smoke_count_changed_neutral_segments(
            image, baseline, false, stats.strongestHorizontalLineRow);
  }
  return stats;
}

static int gui_smoke_count_changed_neutral_vertical_band(const QImage& image,
                                                         const QImage& baseline,
                                                         int centerX,
                                                         int radius = 1) {
  if (image.isNull() || baseline.isNull() || image.size() != baseline.size())
    return 0;

  int count = 0;
  for (int x = std::max(0, centerX - radius);
       x <= std::min(image.width() - 1, centerX + radius); ++x) {
    for (int y = 0; y < image.height(); ++y) {
      if (gui_smoke_is_changed_neutral_pixel(image, baseline, x, y)) ++count;
    }
  }
  return count;
}

static int gui_smoke_count_changed_neutral_horizontal_band(
    const QImage& image, const QImage& baseline, int centerY, int radius = 1) {
  if (image.isNull() || baseline.isNull() || image.size() != baseline.size())
    return 0;

  int count = 0;
  for (int y = std::max(0, centerY - radius);
       y <= std::min(image.height() - 1, centerY + radius); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (gui_smoke_is_changed_neutral_pixel(image, baseline, x, y)) ++count;
    }
  }
  return count;
}

static GuiSmokeWidgetImageStats gui_smoke_analyze_widget_frame(
    const QImage& image, const QImage& baseline = QImage()) {
  GuiSmokeWidgetImageStats stats;
  if (image.isNull()) return stats;

  QHash<QRgb, int> histogram;
  histogram.reserve(std::min(image.width() * image.height(), 4096));
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* scanLine =
        reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = scanLine[x];
      const QRgb rgb   = qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel));
      histogram[rgb] += 1;
    }
  }

  QRgb background = qRgb(0, 0, 0);
  for (auto it = histogram.cbegin(); it != histogram.cend(); ++it) {
    if (it.value() > stats.dominantPixelCount) {
      background               = it.key();
      stats.dominantPixelCount = it.value();
    }
  }
  stats.distinctColors = histogram.size();

  const bool compareBaseline = !baseline.isNull() &&
                               baseline.width() == image.width() &&
                               baseline.height() == image.height();
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* scanLine =
        reinterpret_cast<const QRgb*>(image.constScanLine(y));
    const QRgb* baselineLine =
        compareBaseline
            ? reinterpret_cast<const QRgb*>(baseline.constScanLine(y))
            : nullptr;
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = scanLine[x];
      ++stats.sampleCount;
      if (gui_smoke_pixel_delta(pixel, background) > 24)
        ++stats.nonBackgroundPixels;
      if (baselineLine && gui_smoke_pixel_delta(pixel, baselineLine[x]) > 36)
        ++stats.changedPixels;
    }
  }

  return stats;
}

static int gui_smoke_hue_distance(int a, int b) {
  if (a < 0 || b < 0) return 360;
  const int distance = std::abs(a - b);
  return std::min(distance, 360 - distance);
}

static int gui_smoke_count_hue_pixels(const QImage& image,
                                      const QColor& targetColor,
                                      int hueTolerance = 24) {
  if (image.isNull()) return 0;
  const int targetHue = targetColor.hsvHue();
  if (targetHue < 0) return 0;

  int matchingPixels = 0;
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* scanLine =
        reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QColor pixel(scanLine[x]);
      if (pixel.alpha() == 0 || pixel.saturation() < 45 || pixel.value() < 35)
        continue;
      if (gui_smoke_hue_distance(pixel.hsvHue(), targetHue) <= hueTolerance)
        ++matchingPixels;
    }
  }
  return matchingPixels;
}

static GuiSmokeRasterStats gui_smoke_analyze_raster_frame(
    TXshSimpleLevel* level, const TFrameId& fid) {
  GuiSmokeRasterStats stats;
  if (!level) return stats;

  TRasterImageP rasterImage = (TRasterImageP)level->getFrame(fid, false);
  TRaster32P raster;
  if (rasterImage) raster = rasterImage->getRaster();
  if (!raster) return stats;

  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    const TPixel32* scanLine = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x) {
      const TPixel32& pixel = scanLine[x];
      ++stats.sampleCount;
      if (pixel.m > 0) ++stats.opaquePixels;
      if (pixel.m > 0 && pixel.r > 120 && pixel.r > pixel.g * 2 &&
          pixel.r > pixel.b * 2) {
        ++stats.redPixels;
      }
      if (pixel.m > 0 && pixel.g > 120 && pixel.g > pixel.r * 2 &&
          pixel.g > pixel.b * 2) {
        ++stats.greenPixels;
      }
    }
  }
  raster->unlock();

  return stats;
}

static GuiSmokeRasterStats gui_smoke_analyze_raster_pixels(
    const TRasterP& inputRaster) {
  GuiSmokeRasterStats stats;
  if (!inputRaster) return stats;

  if (TRaster32P raster = inputRaster) {
    raster->lock();
    for (int y = 0; y < raster->getLy(); ++y) {
      const TPixel32* scanLine = raster->pixels(y);
      for (int x = 0; x < raster->getLx(); ++x) {
        const TPixel32& pixel = scanLine[x];
        ++stats.sampleCount;
        if (pixel.m > 0) ++stats.opaquePixels;
        if (pixel.m > 0 && pixel.r > 120 && pixel.r > pixel.g * 2 &&
            pixel.r > pixel.b * 2)
          ++stats.redPixels;
        if (pixel.m > 0 && pixel.g > 120 && pixel.g > pixel.r * 2 &&
            pixel.g > pixel.b * 2)
          ++stats.greenPixels;
      }
    }
    raster->unlock();
    return stats;
  }

  if (TRaster64P raster = inputRaster) {
    raster->lock();
    for (int y = 0; y < raster->getLy(); ++y) {
      const TPixel64* scanLine = raster->pixels(y);
      for (int x = 0; x < raster->getLx(); ++x) {
        const TPixel64& pixel = scanLine[x];
        ++stats.sampleCount;
        if (pixel.m > 0) ++stats.opaquePixels;
        if (pixel.m > 30840 && pixel.r > 30840 && pixel.r > pixel.g * 2 &&
            pixel.r > pixel.b * 2)
          ++stats.redPixels;
        if (pixel.m > 30840 && pixel.g > 30840 && pixel.g > pixel.r * 2 &&
            pixel.g > pixel.b * 2)
          ++stats.greenPixels;
      }
    }
    raster->unlock();
  }

  return stats;
}

static TRasterP gui_smoke_raster_from_image(const TImageP& image) {
  if (!image) return TRasterP();

  TRasterImageP rasterImage = (TRasterImageP)image;
  if (rasterImage) return rasterImage->getRaster();

  TToonzImageP toonzImage = (TToonzImageP)image;
  return toonzImage ? TRasterP(toonzImage->getRaster()) : TRasterP();
}

static bool gui_smoke_prepare_red_palette(TXshSimpleLevel* level) {
  if (!level) return false;

  TPalette* palette = level->getPalette();
  if (palette) {
    while (palette->getStyleCount() <= 1) {
      palette->addStyle(TPixel32(232, 24, 24, 255));
    }
    palette->setStyle(1, TPixel32(232, 24, 24, 255));
  }

  return palette != nullptr;
}

static QString gui_smoke_capture_output_dir() {
  const QString statusPath =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  return statusPath.isEmpty() ? QDir::tempPath()
                              : QFileInfo(statusPath).absolutePath();
}

static bool gui_smoke_save_capture(const QImage& image, const QString& fileName,
                                   QString* path) {
  const QString outputDir = gui_smoke_capture_output_dir();
  QDir().mkpath(outputDir);
  const QString outputPath = QDir(outputDir).filePath(fileName);
  if (path) *path = outputPath;
  return !image.isNull() && image.save(outputPath);
}

static QImage gui_smoke_grab_widget_frame(QWidget* widget) {
  if (!widget) return QImage();

  widget->update();
  gui_smoke_pump_events();
  widget->repaint();
  gui_smoke_pump_events();
  const QPixmap pixmap = widget->grab();
  return pixmap.isNull()
             ? QImage()
             : pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
}

static bool gui_smoke_widget_high_dpi_ok(QWidget* widget, const QImage& image) {
  if (!widget || image.isNull()) return false;
  const double dpr = std::max(1.0, widget->devicePixelRatioF());
  return image.width() == qRound(widget->width() * dpr) &&
         image.height() == qRound(widget->height() * dpr);
}

static QStringList gui_smoke_viewer_capture_details(
    SceneViewer* viewer, const QImage& before, const QString& capturePrefix,
    const QString& contentName, bool contentOk = true,
    int requiredOnionPixels = 0, int requiredChangedGrayPixels = 0,
    int requiredRedPixels = 1, int requiredChangedNeutralPixels = 0) {
  QStringList details;
  const QImage after = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats stats =
      gui_smoke_analyze_viewer_frame(after, before);

  QString beforePath;
  QString afterPath;
  const bool beforeSaved = gui_smoke_save_capture(
      before, capturePrefix + QStringLiteral("-before.png"), &beforePath);
  const bool afterSaved = gui_smoke_save_capture(
      after, capturePrefix + QStringLiteral("-after.png"), &afterPath);
  const QSize logicalSize      = viewer ? viewer->size() : QSize();
  const int viewerDeviceWidth  = viewer ? viewer->width() : 0;
  const int viewerDeviceHeight = viewer ? viewer->height() : 0;
  const int viewerDevPixRatio  = viewer ? viewer->getDevPixRatio() : 0;
  const double widgetDevicePixelRatio =
      viewer ? viewer->devicePixelRatioF() : 0.0;
  QScreen* viewerScreen = viewer ? viewer->screen() : nullptr;
  const double screenDevicePixelRatio =
      viewerScreen ? viewerScreen->devicePixelRatio() : 0.0;
  const bool logicalToDeviceOk =
      viewer && logicalSize.width() > 0 && logicalSize.height() > 0 &&
      viewerDevPixRatio >= 1 &&
      viewerDeviceWidth == logicalSize.width() * viewerDevPixRatio &&
      viewerDeviceHeight == logicalSize.height() * viewerDevPixRatio;
  const bool framebufferSizeOk = viewer && after.width() == viewerDeviceWidth &&
                                 after.height() == viewerDeviceHeight;
  const bool highDpiOk = logicalToDeviceOk && framebufferSizeOk &&
                         widgetDevicePixelRatio >= 1.0 &&
                         screenDevicePixelRatio >= 1.0;
  const bool onionPixelsOk = requiredOnionPixels <= 0 ||
                             (stats.onionPixels >= requiredOnionPixels &&
                              stats.onionPixels > stats.baselineOnionPixels);
  const bool changedGrayPixelsOk =
      requiredChangedGrayPixels <= 0 ||
      stats.changedGrayPixels >= requiredChangedGrayPixels;
  const bool changedNeutralPixelsOk =
      requiredChangedNeutralPixels <= 0 ||
      stats.changedNeutralPixels >= requiredChangedNeutralPixels;
  const bool redPixelsOk =
      requiredRedPixels <= 0 || stats.redPixels >= requiredRedPixels;
  const bool changedPixelsOk =
      requiredOnionPixels > 0 ? onionPixelsOk : stats.changedPixels > 0;
  const bool ok = contentOk && !before.isNull() && !after.isNull() &&
                  before.size() == after.size() && changedPixelsOk &&
                  redPixelsOk && beforeSaved && afterSaved && highDpiOk &&
                  changedGrayPixelsOk && changedNeutralPixelsOk;

  details
      << QString("viewerRenderContent=%1").arg(contentName)
      << QString("viewerVisible=%1")
             .arg(viewer && viewer->isVisible() ? QStringLiteral("true")
                                                : QStringLiteral("false"))
      << QString("viewerLogicalWidth=%1").arg(logicalSize.width())
      << QString("viewerLogicalHeight=%1").arg(logicalSize.height())
      << QString("viewerWidth=%1").arg(viewerDeviceWidth)
      << QString("viewerHeight=%1").arg(viewerDeviceHeight)
      << QString("viewerDevPixRatio=%1").arg(viewerDevPixRatio)
      << QString("viewerDevicePixelRatio=%1")
             .arg(widgetDevicePixelRatio, 0, 'f', 2)
      << QString("viewerScreenDevicePixelRatio=%1")
             .arg(screenDevicePixelRatio, 0, 'f', 2)
      << QString("framebufferWidth=%1").arg(after.width())
      << QString("framebufferHeight=%1").arg(after.height())
      << QString("viewerLogicalToDeviceProbe=%1")
             .arg(logicalToDeviceOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("viewerFramebufferSizeProbe=%1")
             .arg(framebufferSizeOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("viewerHighDpiProbe=%1")
             .arg(highDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("sampleCount=%1").arg(stats.sampleCount)
      << QString("changedPixels=%1").arg(stats.changedPixels)
      << QString("redPixels=%1").arg(stats.redPixels)
      << QString("changedRedPixels=%1").arg(stats.changedRedPixels)
      << QString("greenPixels=%1").arg(stats.greenPixels)
      << QString("changedGreenPixels=%1").arg(stats.changedGreenPixels)
      << QString("bluePixels=%1").arg(stats.bluePixels)
      << QString("changedBluePixels=%1").arg(stats.changedBluePixels)
      << QString("grayPixels=%1").arg(stats.grayPixels)
      << QString("changedGrayPixels=%1").arg(stats.changedGrayPixels)
      << QString("changedNeutralPixels=%1").arg(stats.changedNeutralPixels)
      << QString("onionPixels=%1").arg(stats.onionPixels)
      << QString("baselineOnionPixels=%1").arg(stats.baselineOnionPixels)
      << QString("beforeCaptureSaved=%1")
             .arg(beforeSaved ? QStringLiteral("true")
                              : QStringLiteral("false"))
      << QString("afterCaptureSaved=%1")
             .arg(afterSaved ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("beforeCapturePath=%1").arg(gui_smoke_status_value(beforePath))
      << QString("afterCapturePath=%1").arg(gui_smoke_status_value(afterPath))
      << QString("viewerRenderProbe=%1")
             .arg(ok ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static bool gui_smoke_add_vector_probe_frame(TXshSimpleLevel* level,
                                             const TFrameId& fid) {
  if (!gui_smoke_prepare_red_palette(level)) return false;

  TVectorImageP image = new TVectorImage();
  image->setPalette(level->getPalette());

  std::vector<TThickPoint> points;
  points.push_back(TThickPoint(-180.0, -110.0, 20.0));
  points.push_back(TThickPoint(0.0, 120.0, 28.0));
  points.push_back(TThickPoint(180.0, -110.0, 20.0));

  TStroke* stroke = new TStroke(points);
  stroke->setStyle(1);
  image->addStroke(stroke);
  image->validateRegions(true);

  level->setFrame(fid, image);
  level->setDirtyFlag(true);
  return true;
}

static bool gui_smoke_add_vector_multi_probe_frame(TXshSimpleLevel* level,
                                                   const TFrameId& fid) {
  if (!gui_smoke_prepare_red_palette(level)) return false;

  TVectorImageP image = new TVectorImage();
  image->setPalette(level->getPalette());

  auto addStroke = [&](const std::vector<TThickPoint>& points) {
    TStroke* stroke = new TStroke(points);
    stroke->setStyle(1);
    image->addStroke(stroke);
  };

  addStroke({TThickPoint(-230.0, -95.0, 18.0), TThickPoint(-165.0, 105.0, 24.0),
             TThickPoint(-75.0, -95.0, 18.0)});
  addStroke({TThickPoint(65.0, -85.0, 18.0), TThickPoint(150.0, 115.0, 24.0),
             TThickPoint(245.0, -85.0, 18.0)});

  image->validateRegions(true);

  level->setFrame(fid, image);
  level->setDirtyFlag(true);
  return true;
}

static void gui_smoke_add_raster_probe_frame(
    TXshSimpleLevel* level, const TFrameId& fid,
    const TDimension& size = TDimension(256, 256),
    const TPointD& dpi     = TPointD(),
    const TPixel32& color  = TPixel32(232, 24, 24, 255)) {
  TRaster32P raster(size.lx, size.ly);
  raster->fill(color);
  TRasterImageP image(raster);
  if (dpi.x > 0.0 && dpi.y > 0.0) image->setDpi(dpi.x, dpi.y);
  level->setFrame(fid, image);
  level->setDirtyFlag(true);
}

static void gui_smoke_add_raster_rect_probe_frame(
    TXshSimpleLevel* level, const TFrameId& fid, const TDimension& size,
    const TRect& rect, const TPixel32& color, const TPointD& dpi = TPointD()) {
  TRaster32P raster(size.lx, size.ly);
  raster->clear();
  raster->lock();
  const int left   = std::max(0, std::min(rect.x0, rect.x1));
  const int right  = std::min(raster->getLx(), std::max(rect.x0, rect.x1));
  const int top    = std::max(0, std::min(rect.y0, rect.y1));
  const int bottom = std::min(raster->getLy(), std::max(rect.y0, rect.y1));
  for (int y = top; y < bottom; ++y) {
    TPixel32* scanLine = raster->pixels(y);
    for (int x = left; x < right; ++x) scanLine[x] = color;
  }
  raster->unlock();

  TRasterImageP image(raster);
  if (dpi.x > 0.0 && dpi.y > 0.0) image->setDpi(dpi.x, dpi.y);
  level->setFrame(fid, image);
  level->setDirtyFlag(true);
}

static bool gui_smoke_add_toonz_raster_rect_probe_frame(
    TXshSimpleLevel* level, const TFrameId& fid, const TDimension& size,
    const TRect& rect, const TPointD& dpi = TPointD()) {
  if (!gui_smoke_prepare_red_palette(level)) return false;

  TRasterCM32P raster(size.lx, size.ly);
  raster->fill(TPixelCM32(0, 0, 255));
  raster->lock();
  const int left   = std::max(0, std::min(rect.x0, rect.x1));
  const int right  = std::min(raster->getLx(), std::max(rect.x0, rect.x1));
  const int top    = std::max(0, std::min(rect.y0, rect.y1));
  const int bottom = std::min(raster->getLy(), std::max(rect.y0, rect.y1));
  for (int y = top; y < bottom; ++y) {
    TPixelCM32* scanLine = raster->pixels(y);
    for (int x = left; x < right; ++x) {
      scanLine[x] = TPixelCM32(0, 1, 255);
    }
  }
  raster->unlock();

  TToonzImageP image(raster, raster->getBounds());
  image->setPalette(level->getPalette());
  if (dpi.x > 0.0 && dpi.y > 0.0) image->setDpi(dpi.x, dpi.y);
  level->setFrame(fid, image);
  level->setDirtyFlag(true);
  return true;
}

static void gui_smoke_add_transparent_raster_probe_frame(TXshSimpleLevel* level,
                                                         const TFrameId& fid,
                                                         const TPixel32& color,
                                                         int x0, int y0, int x1,
                                                         int y1) {
  TRaster32P raster(256, 256);
  raster->clear();
  raster->lock();
  const int left   = std::max(0, std::min(x0, x1));
  const int right  = std::min(raster->getLx(), std::max(x0, x1));
  const int top    = std::max(0, std::min(y0, y1));
  const int bottom = std::min(raster->getLy(), std::max(y0, y1));
  for (int y = top; y < bottom; ++y) {
    TPixel32* scanLine = raster->pixels(y);
    for (int x = left; x < right; ++x) scanLine[x] = color;
  }
  raster->unlock();

  level->setFrame(fid, TRasterImageP(raster));
  level->setDirtyFlag(true);
}

struct GuiSmokeStageOnionCounts final : public Stage::Visitor {
  int m_totalPlayers       = 0;
  int m_onionPlayers       = 0;
  int m_backOnionPlayers   = 0;
  int m_frontOnionPlayers  = 0;
  int m_currentPlayers     = 0;
  int m_currentColumnCount = 0;
  QStringList m_rows;

  GuiSmokeStageOnionCounts(const ImagePainter::VisualSettings& visualSettings)
      : Stage::Visitor(visualSettings) {}

  void onImage(const Stage::Player& player) override {
    ++m_totalPlayers;
    if (player.m_isCurrentColumn) ++m_currentColumnCount;
    if (player.m_onionSkinDistance == 0) ++m_currentPlayers;
    if (player.m_onionSkinDistance != c_noOnionSkin &&
        player.m_onionSkinDistance != 0) {
      ++m_onionPlayers;
      if (player.m_onionSkinDistance < 0)
        ++m_backOnionPlayers;
      else
        ++m_frontOnionPlayers;
      m_rows << QString::number(player.m_frame);
    }
  }

  void onRasterImage(TRasterImage*, const Stage::Player&) override {}
  void enableMask() override {}
  void disableMask() override {}
  void beginMask() override {}
  void endMask() override {}
};

static QStringList gui_smoke_viewer_render_details(
    const QString& requestedSceneName, bool useVectorLevel) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  viewer->resetSceneViewer();
  gui_smoke_pump_events();
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const int levelType          = useVectorLevel ? PLI_XSHLEVEL : OVL_XSHLEVEL;
  const std::wstring levelName = useVectorLevel
                                     ? L"qt6_gui_viewer_render_vector"
                                     : L"qt6_gui_viewer_render_raster";
  TXshLevel* levelXl           = scene->createNewLevel(levelType, levelName);
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error");
    return details;
  }

  const TFrameId fid(1);
  if (useVectorLevel) {
    if (!gui_smoke_add_vector_probe_frame(simpleLevel, fid)) {
      details << QStringLiteral("viewerRenderProbe=vector-frame-error");
      return details;
    }
  } else {
    gui_smoke_add_raster_probe_frame(simpleLevel, fid);
  }

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error");
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QString capturePrefix = useVectorLevel
                                    ? QStringLiteral("viewer-vector-render")
                                    : QStringLiteral("viewer-render");

  details << QString("scene=%1").arg(scenePath.getQString());
  details += gui_smoke_viewer_capture_details(
      viewer, before, capturePrefix,
      useVectorLevel ? QStringLiteral("vector") : QStringLiteral("raster"));

  if (!useVectorLevel) {
    const QImage firstFrame = gui_smoke_grab_viewer_frame(viewer);
    const TFrameId secondFid(2);
    gui_smoke_add_raster_probe_frame(simpleLevel, secondFid,
                                     TDimension(256, 256), TPointD(),
                                     TPixel32(24, 232, 24, 255));
    if (!xsheet->setCell(1, 0, TXshCell(simpleLevel, secondFid))) {
      details << QStringLiteral("viewerStaleFrameProbe=xsheet-error");
      return details;
    }

    TApp::instance()->getCurrentFrame()->setFrame(1);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(100);

    const QImage secondFrame = gui_smoke_grab_viewer_frame(viewer);
    const GuiSmokeImageStats staleFrameStats =
        gui_smoke_analyze_viewer_frame(secondFrame, firstFrame);
    QString staleFramePath;
    const bool staleFrameSaved = gui_smoke_save_capture(
        secondFrame, QStringLiteral("viewer-render-second-frame.png"),
        &staleFramePath);
    const bool staleFrameOk =
        !firstFrame.isNull() && !secondFrame.isNull() &&
        firstFrame.size() == secondFrame.size() && staleFrameSaved &&
        staleFrameStats.changedPixels > 0 && staleFrameStats.greenPixels > 0 &&
        staleFrameStats.changedGreenPixels > 0;

    details << QString("viewerStaleFrameProbe=%1")
                   .arg(staleFrameOk ? QStringLiteral("ok")
                                     : QStringLiteral("error"))
            << QString("viewerStaleFrameChangedPixels=%1")
                   .arg(staleFrameStats.changedPixels)
            << QString("viewerStaleFrameGreenPixels=%1")
                   .arg(staleFrameStats.greenPixels)
            << QString("viewerStaleFrameChangedGreenPixels=%1")
                   .arg(staleFrameStats.changedGreenPixels)
            << QString("viewerStaleFrameCaptureSaved=%1")
                   .arg(staleFrameSaved ? QStringLiteral("true")
                                        : QStringLiteral("false"))
            << QString("viewerStaleFrameCapturePath=%1")
                   .arg(gui_smoke_status_value(staleFramePath));
  }

  return details;
}

class GuiSmokePreviewRenderListener final : public Previewer::Listener {
  TRectD m_previewRect;

public:
  QEventLoop* m_loop    = nullptr;
  int m_startedFrames   = 0;
  int m_completedFrames = 0;
  int m_failedFrames    = 0;
  int m_updateCount     = 0;

  explicit GuiSmokePreviewRenderListener(const TRectD& previewRect)
      : m_previewRect(previewRect) {}

  TRectD getPreviewRect() const override { return m_previewRect; }

  void onRenderStarted(int) override { ++m_startedFrames; }

  void onRenderCompleted(int frame) override {
    ++m_completedFrames;
    if (frame == 0) {
      Previewer* previewer = Previewer::instance(false);
      m_readyAtCompletion  = previewer->isFrameReady(0);
      TRasterP raster      = previewer->getRaster(0, false);
      m_rasterAtCompletion = raster ? raster->getSize() : TDimension();
      m_statsAtCompletion  = gui_smoke_analyze_raster_pixels(raster);
      if (m_loop)
        QMetaObject::invokeMethod(m_loop, "quit", Qt::QueuedConnection);
    }
  }

  bool m_readyAtCompletion = false;
  TDimension m_rasterAtCompletion;
  GuiSmokeRasterStats m_statsAtCompletion;

  void onRenderFailed(int frame) override {
    ++m_failedFrames;
    if (frame == 0 && m_loop)
      QMetaObject::invokeMethod(m_loop, "quit", Qt::QueuedConnection);
  }

  void onPreviewUpdate() override { ++m_updateCount; }
};

struct GuiSmokeFxPreviewRenderProbe {
  unsigned long m_fxId = 0;
  int m_startedFrames  = 0;
  int m_renderedFrames = 0;
  bool m_frameReady    = false;
  TDimension m_rasterSize;
  GuiSmokeRasterStats m_stats;
  QEventLoop* m_loop = nullptr;

  explicit GuiSmokeFxPreviewRenderProbe(unsigned long fxId) : m_fxId(fxId) {}

  bool accepts(unsigned long fxId, const TRenderPort::RenderData& renderData) {
    return fxId == m_fxId &&
           std::find(renderData.m_frames.begin(), renderData.m_frames.end(),
                     0) != renderData.m_frames.end();
  }
};

class GuiSmokeFinalRenderListener final : public MovieRenderer::Listener {
public:
  QEventLoop* m_loop       = nullptr;
  int m_completedFrames    = 0;
  int m_failedFrames       = 0;
  bool m_sequenceCompleted = false;
  TFilePath m_outputPath;
  QString m_error;

  bool onFrameCompleted(int) override {
    ++m_completedFrames;
    return true;
  }

  bool onFrameFailed(int, TException& e) override {
    ++m_failedFrames;
    if (m_error.isEmpty())
      m_error = QString::fromStdString(::to_string(e.getMessage()));
    return true;
  }

  void onSequenceCompleted(const TFilePath& fp) override {
    m_sequenceCompleted = true;
    m_outputPath        = fp;
    if (m_loop) QMetaObject::invokeMethod(m_loop, "quit", Qt::QueuedConnection);
  }
};

class GuiSmokeFinalRenderCapturePort final : public TRenderPort {
public:
  int m_completedRasters = 0;
  int m_failedRasters    = 0;
  TDimension m_rasterSize;
  GuiSmokeRasterStats m_stats;
  QString m_error;

  void onRenderRasterCompleted(const RenderData& renderData) override {
    ++m_completedRasters;
    if (renderData.m_rasA) {
      m_rasterSize = renderData.m_rasA->getSize();
      m_stats      = gui_smoke_analyze_raster_pixels(renderData.m_rasA);
    }
  }

  void onRenderFailure(const RenderData&, TException& e) override {
    ++m_failedRasters;
    if (m_error.isEmpty())
      m_error = QString::fromStdString(::to_string(e.getMessage()));
  }
};

static QStringList gui_smoke_preview_render_output_details(
    const QString& requestedSceneName) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("previewRenderProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));

  TOutputProperties* previewProperties =
      scene->getProperties()->getPreviewProperties();
  previewProperties->setRange(0, 0, 1);
  previewProperties->setThreadIndex(0);
  previewProperties->setMaxTileSizeIndex(1);
  TRenderSettings previewSettings = previewProperties->getRenderSettings();
  previewSettings.m_bpp           = 32;
  previewSettings.m_shrinkX       = 1;
  previewSettings.m_shrinkY       = 1;
  previewSettings.m_applyShrinkToViewer = false;
  previewSettings.m_stereoscopic        = false;
  previewSettings.m_getFullSizeBBox     = false;
  previewSettings.m_cameraBox =
      TRectD(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  previewProperties->setRenderSettings(previewSettings);

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_preview_render_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("previewRenderProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(232, 24, 24, 255), camera->getDpi());

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("previewRenderProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  // The Previewer invalidates cached frames through 300 ms debounce timers
  // hooked to the level/xsheet/scene notifications above. Pump past that
  // window so the deferred invalidation cannot race the render below and
  // clear the freshly cached frame.
  gui_smoke_pump_events(500);

  const TRectD previewRect(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  GuiSmokePreviewRenderListener listener(previewRect);
  Previewer* previewer = Previewer::instance(false);
  previewer->addListener(&listener);
  previewer->clear();
  previewer->update();

  QEventLoop loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  listener.m_loop = &loop;
  QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

  previewer->addFramesToRenderQueue(std::vector<int>{0});
  if (!previewer->isFrameReady(0) && listener.m_failedFrames == 0) {
    timeoutTimer.start(15000);
    loop.exec();
  }
  timeoutTimer.stop();
  gui_smoke_pump_events(100);

  const bool frameReady  = previewer->isFrameReady(0);
  const bool previewBusy = previewer->isBusy();
  TRasterP previewRaster = previewer->getRaster(0, false);
  const GuiSmokeRasterStats previewStats =
      gui_smoke_analyze_raster_pixels(previewRaster);
  const TDimension previewSize =
      previewRaster ? previewRaster->getSize() : TDimension();
  const bool previewOk =
      frameReady && previewRaster && previewSize == TDimension(320, 240) &&
      listener.m_failedFrames == 0 && previewStats.redPixels > 1000;

  previewer->removeListener(&listener);
  previewer->clear(0);
  listener.m_loop = nullptr;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("previewRenderSource=raster-scene-standard-dpi")
      << QString("previewRenderFrameReady=%1")
             .arg(frameReady ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("previewRenderBusy=%1")
             .arg(previewBusy ? QStringLiteral("true")
                              : QStringLiteral("false"))
      << QString("previewRenderStartedFrames=%1").arg(listener.m_startedFrames)
      << QString("previewRenderCompletedFrames=%1")
             .arg(listener.m_completedFrames)
      << QString("previewRenderFailedFrames=%1").arg(listener.m_failedFrames)
      << QString("previewRenderUpdateCount=%1").arg(listener.m_updateCount)
      << QString("previewRenderReadyAtCompletion=%1")
             .arg(listener.m_readyAtCompletion ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("previewRenderSizeAtCompletion=%1x%2")
             .arg(listener.m_rasterAtCompletion.lx)
             .arg(listener.m_rasterAtCompletion.ly)
      << QString("previewRenderRedAtCompletion=%1")
             .arg(listener.m_statsAtCompletion.redPixels)
      << QString("previewRenderRasterWidth=%1").arg(previewSize.lx)
      << QString("previewRenderRasterHeight=%1").arg(previewSize.ly)
      << QString("previewRenderSampleCount=%1").arg(previewStats.sampleCount)
      << QString("previewRenderOpaquePixels=%1").arg(previewStats.opaquePixels)
      << QString("previewRenderRedPixels=%1").arg(previewStats.redPixels)
      << QString("previewRenderProbe=%1")
             .arg(previewOk ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static QStringList gui_smoke_fx_preview_render_output_details(
    const QString& requestedSceneName) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("fxPreviewRenderProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));

  TOutputProperties* previewProperties =
      scene->getProperties()->getPreviewProperties();
  previewProperties->setRange(0, 0, 1);
  previewProperties->setThreadIndex(0);
  previewProperties->setMaxTileSizeIndex(1);
  TRenderSettings previewSettings = previewProperties->getRenderSettings();
  previewSettings.m_bpp           = 32;
  previewSettings.m_shrinkX       = 1;
  previewSettings.m_shrinkY       = 1;
  previewSettings.m_applyShrinkToViewer = false;
  previewSettings.m_stereoscopic        = false;
  previewSettings.m_getFullSizeBBox     = false;
  previewSettings.m_cameraBox =
      TRectD(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  previewProperties->setRenderSettings(previewSettings);

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_fx_preview_render_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("fxPreviewRenderProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(232, 24, 24, 255), camera->getDpi());

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("fxPreviewRenderProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  gui_smoke_pump_events(100);

  TXshColumn* column = xsheet->getColumn(0);
  TFx* columnFx      = column ? column->getFx() : nullptr;
  if (!columnFx) {
    details << QStringLiteral("fxPreviewRenderProbe=column-fx-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFxP previewFx(columnFx);
  const unsigned long previewFxId = previewFx->getIdentifier();
  const std::string cacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(0);

  PreviewFxManager* manager = PreviewFxManager::instance();
  manager->reset(true);
  TImageCache::instance()->remove(cacheId);
  gui_smoke_pump_events(100);

  GuiSmokeFxPreviewRenderProbe probe(previewFxId);
  QEventLoop loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  probe.m_loop = &loop;

  QMetaObject::Connection startedConnection = QObject::connect(
      manager, &PreviewFxManager::startedFrame, QCoreApplication::instance(),
      [&probe](unsigned long fxId, TRenderPort::RenderData renderData) {
        if (probe.accepts(fxId, renderData)) ++probe.m_startedFrames;
      });
  QMetaObject::Connection renderedConnection = QObject::connect(
      manager, &PreviewFxManager::renderedFrame, QCoreApplication::instance(),
      [&probe, &cacheId](unsigned long fxId,
                         TRenderPort::RenderData renderData) {
        if (!probe.accepts(fxId, renderData)) return;

        ++probe.m_renderedFrames;
        TRasterImageP image =
            (TRasterImageP)TImageCache::instance()->get(cacheId, false);
        TRasterP raster    = image ? image->getRaster() : TRasterP();
        probe.m_frameReady = (bool)raster;
        if (raster) {
          probe.m_rasterSize = raster->getSize();
          probe.m_stats      = gui_smoke_analyze_raster_pixels(raster);
        }
        if (probe.m_loop)
          QMetaObject::invokeMethod(probe.m_loop, "quit", Qt::QueuedConnection);
      });
  QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

  FlipBook* flipbook = manager->showNewPreview(previewFx, false);
  gui_smoke_pump_events(100);
  if (!probe.m_frameReady && probe.m_renderedFrames == 0) {
    timeoutTimer.start(15000);
    loop.exec();
  }
  timeoutTimer.stop();
  gui_smoke_pump_events(100);

  if (!probe.m_frameReady) {
    TRasterImageP image =
        (TRasterImageP)TImageCache::instance()->get(cacheId, false);
    TRasterP raster    = image ? image->getRaster() : TRasterP();
    probe.m_frameReady = (bool)raster;
    if (raster) {
      probe.m_rasterSize = raster->getSize();
      probe.m_stats      = gui_smoke_analyze_raster_pixels(raster);
    }
  }

  const bool flipbookOk = flipbook && flipbook->getPreviewedFx() == columnFx &&
                          flipbook->getPreviewXsheet() == xsheet;
  const bool previewOk = flipbookOk && probe.m_frameReady &&
                         probe.m_renderedFrames > 0 &&
                         probe.m_rasterSize == TDimension(320, 240) &&
                         probe.m_stats.redPixels > 1000;

  QObject::disconnect(startedConnection);
  QObject::disconnect(renderedConnection);
  probe.m_loop = nullptr;

  if (flipbook) manager->detach(flipbook);
  TImageCache::instance()->remove(cacheId);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("fxPreviewRenderSource=column-fx-raster-scene")
      << QString("fxPreviewRenderFxId=%1").arg(previewFxId)
      << QString("fxPreviewRenderCacheId=%1")
             .arg(QString::fromStdString(cacheId))
      << QString("fxPreviewRenderFlipbook=%1")
             .arg(flipbook ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewRenderFlipbookAttached=%1")
             .arg(flipbookOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewRenderStartedFrames=%1").arg(probe.m_startedFrames)
      << QString("fxPreviewRenderCompletedFrames=%1")
             .arg(probe.m_renderedFrames)
      << QString("fxPreviewRenderFrameReady=%1")
             .arg(probe.m_frameReady ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("fxPreviewRenderRasterWidth=%1").arg(probe.m_rasterSize.lx)
      << QString("fxPreviewRenderRasterHeight=%1").arg(probe.m_rasterSize.ly)
      << QString("fxPreviewRenderSampleCount=%1").arg(probe.m_stats.sampleCount)
      << QString("fxPreviewRenderOpaquePixels=%1")
             .arg(probe.m_stats.opaquePixels)
      << QString("fxPreviewRenderRedPixels=%1").arg(probe.m_stats.redPixels)
      << QString("fxPreviewRenderProbe=%1")
             .arg(previewOk ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static QStringList gui_smoke_fx_preview_subcamera_render_output_details(
    const QString& requestedSceneName) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("fxPreviewSubcameraProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));

  TCamera* previewCamera = scene->getCurrentPreviewCamera();
  if (previewCamera != camera) {
    previewCamera->setRes(TDimension(320, 240));
    previewCamera->setSize(TDimensionD(previewCamera->getRes().lx / renderDpi,
                                       previewCamera->getRes().ly / renderDpi));
  }
  const TRect subcameraRect(80, 60, 239, 179);
  previewCamera->setInterestRect(subcameraRect);
  const TDimension subcameraSize(subcameraRect.getLx(), subcameraRect.getLy());

  TOutputProperties* previewProperties =
      scene->getProperties()->getPreviewProperties();
  previewProperties->setRange(0, 0, 1);
  previewProperties->setThreadIndex(0);
  previewProperties->setMaxTileSizeIndex(1);
  previewProperties->setSubcameraPreview(true);
  TRenderSettings previewSettings = previewProperties->getRenderSettings();
  previewSettings.m_bpp           = 32;
  previewSettings.m_shrinkX       = 1;
  previewSettings.m_shrinkY       = 1;
  previewSettings.m_applyShrinkToViewer = false;
  previewSettings.m_stereoscopic        = false;
  previewSettings.m_getFullSizeBBox     = false;
  previewSettings.m_cameraBox =
      TRectD(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  previewProperties->setRenderSettings(previewSettings);

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_fx_preview_subcamera_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("fxPreviewSubcameraProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(232, 24, 24, 255), camera->getDpi());

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("fxPreviewSubcameraProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  gui_smoke_pump_events(100);

  TXshColumn* column = xsheet->getColumn(0);
  TFx* columnFx      = column ? column->getFx() : nullptr;
  if (!columnFx) {
    details << QStringLiteral("fxPreviewSubcameraProbe=column-fx-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFxP previewFx(columnFx);
  const unsigned long previewFxId = previewFx->getIdentifier();
  const std::string cacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(0);

  PreviewFxManager* manager = PreviewFxManager::instance();
  manager->reset(true);
  TImageCache::instance()->remove(cacheId);
  gui_smoke_pump_events(100);

  GuiSmokeFxPreviewRenderProbe probe(previewFxId);
  QEventLoop loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  probe.m_loop = &loop;

  QMetaObject::Connection startedConnection = QObject::connect(
      manager, &PreviewFxManager::startedFrame, QCoreApplication::instance(),
      [&probe](unsigned long fxId, TRenderPort::RenderData renderData) {
        if (probe.accepts(fxId, renderData)) ++probe.m_startedFrames;
      });
  QMetaObject::Connection renderedConnection = QObject::connect(
      manager, &PreviewFxManager::renderedFrame, QCoreApplication::instance(),
      [&probe, &cacheId](unsigned long fxId,
                         TRenderPort::RenderData renderData) {
        if (!probe.accepts(fxId, renderData)) return;

        ++probe.m_renderedFrames;
        TRasterImageP image =
            (TRasterImageP)TImageCache::instance()->get(cacheId, false);
        TRasterP raster    = image ? image->getRaster() : TRasterP();
        probe.m_frameReady = (bool)raster;
        if (raster) {
          probe.m_rasterSize = raster->getSize();
          probe.m_stats      = gui_smoke_analyze_raster_pixels(raster);
        }
        if (probe.m_loop)
          QMetaObject::invokeMethod(probe.m_loop, "quit", Qt::QueuedConnection);
      });
  QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

  FlipBook* flipbook = manager->showNewPreview(previewFx, false);
  gui_smoke_pump_events(100);
  if (!probe.m_frameReady && probe.m_renderedFrames == 0) {
    timeoutTimer.start(15000);
    loop.exec();
  }
  timeoutTimer.stop();
  gui_smoke_pump_events(100);

  if (!probe.m_frameReady) {
    TRasterImageP image =
        (TRasterImageP)TImageCache::instance()->get(cacheId, false);
    TRasterP raster    = image ? image->getRaster() : TRasterP();
    probe.m_frameReady = (bool)raster;
    if (raster) {
      probe.m_rasterSize = raster->getSize();
      probe.m_stats      = gui_smoke_analyze_raster_pixels(raster);
    }
  }

  const bool flipbookOk = flipbook && flipbook->getPreviewedFx() == columnFx &&
                          flipbook->getPreviewXsheet() == xsheet;
  const bool subcameraActive = manager->isSubCameraActive(previewFx);
  const bool previewOk = flipbookOk && subcameraActive && probe.m_frameReady &&
                         probe.m_renderedFrames > 0 &&
                         probe.m_rasterSize == subcameraSize &&
                         probe.m_stats.redPixels > 1000;

  QObject::disconnect(startedConnection);
  QObject::disconnect(renderedConnection);
  probe.m_loop = nullptr;

  if (flipbook) manager->detach(flipbook);
  TImageCache::instance()->remove(cacheId);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("fxPreviewSubcameraSource=column-fx-raster-scene")
      << QString("fxPreviewSubcameraFxId=%1").arg(previewFxId)
      << QString("fxPreviewSubcameraCacheId=%1")
             .arg(QString::fromStdString(cacheId))
      << QString("fxPreviewSubcameraRect=%1,%2,%3,%4")
             .arg(subcameraRect.x0)
             .arg(subcameraRect.y0)
             .arg(subcameraRect.x1)
             .arg(subcameraRect.y1)
      << QString("fxPreviewSubcameraExpectedWidth=%1").arg(subcameraSize.lx)
      << QString("fxPreviewSubcameraExpectedHeight=%1").arg(subcameraSize.ly)
      << QString("fxPreviewSubcameraFlipbook=%1")
             .arg(flipbook ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewSubcameraFlipbookAttached=%1")
             .arg(flipbookOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewSubcameraActive=%1")
             .arg(subcameraActive ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("fxPreviewSubcameraStartedFrames=%1")
             .arg(probe.m_startedFrames)
      << QString("fxPreviewSubcameraCompletedFrames=%1")
             .arg(probe.m_renderedFrames)
      << QString("fxPreviewSubcameraFrameReady=%1")
             .arg(probe.m_frameReady ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("fxPreviewSubcameraRasterWidth=%1").arg(probe.m_rasterSize.lx)
      << QString("fxPreviewSubcameraRasterHeight=%1").arg(probe.m_rasterSize.ly)
      << QString("fxPreviewSubcameraSampleCount=%1")
             .arg(probe.m_stats.sampleCount)
      << QString("fxPreviewSubcameraOpaquePixels=%1")
             .arg(probe.m_stats.opaquePixels)
      << QString("fxPreviewSubcameraRedPixels=%1").arg(probe.m_stats.redPixels)
      << QString("fxPreviewSubcameraProbe=%1")
             .arg(previewOk ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static QStringList gui_smoke_fx_preview_flipbook_controls_details(
    const QString& requestedSceneName) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral(
                   "fxPreviewFlipbookControlsProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));

  TOutputProperties* previewProperties =
      scene->getProperties()->getPreviewProperties();
  previewProperties->setRange(0, 1, 1);
  previewProperties->setThreadIndex(0);
  previewProperties->setMaxTileSizeIndex(1);
  previewProperties->setSubcameraPreview(false);
  TRenderSettings previewSettings = previewProperties->getRenderSettings();
  previewSettings.m_bpp           = 32;
  previewSettings.m_shrinkX       = 1;
  previewSettings.m_shrinkY       = 1;
  previewSettings.m_applyShrinkToViewer = false;
  previewSettings.m_stereoscopic        = false;
  previewSettings.m_getFullSizeBBox     = false;
  previewSettings.m_cameraBox =
      TRectD(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  previewProperties->setRenderSettings(previewSettings);

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_fx_preview_flipbook_controls_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("fxPreviewFlipbookControlsProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId firstFid(1);
  const TFrameId secondFid(2);
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, firstFid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(232, 24, 24, 255), camera->getDpi());
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, secondFid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(24, 232, 24, 255), camera->getDpi());

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, firstFid)) ||
      !xsheet->setCell(1, 0, TXshCell(simpleLevel, secondFid))) {
    details << QStringLiteral("fxPreviewFlipbookControlsProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  gui_smoke_pump_events(100);

  TXshColumn* column = xsheet->getColumn(0);
  TFx* columnFx      = column ? column->getFx() : nullptr;
  if (!columnFx) {
    details << QStringLiteral("fxPreviewFlipbookControlsProbe=column-fx-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFxP previewFx(columnFx);
  const unsigned long previewFxId = previewFx->getIdentifier();
  const std::string firstCacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(0);
  const std::string secondCacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(1);

  PreviewFxManager* manager = PreviewFxManager::instance();
  manager->reset(true);
  TImageCache::instance()->remove(firstCacheId);
  TImageCache::instance()->remove(secondCacheId);
  gui_smoke_pump_events(100);

  int startedFrames                         = 0;
  int renderedFrames                        = 0;
  QMetaObject::Connection startedConnection = QObject::connect(
      manager, &PreviewFxManager::startedFrame, QCoreApplication::instance(),
      [&](unsigned long fxId, TRenderPort::RenderData renderData) {
        if (fxId != previewFxId) return;
        for (int frame : renderData.m_frames) {
          if (frame == 0 || frame == 1) ++startedFrames;
        }
      });
  QMetaObject::Connection renderedConnection = QObject::connect(
      manager, &PreviewFxManager::renderedFrame, QCoreApplication::instance(),
      [&](unsigned long fxId, TRenderPort::RenderData renderData) {
        if (fxId != previewFxId) return;
        for (int frame : renderData.m_frames) {
          if (frame == 0 || frame == 1) ++renderedFrames;
        }
      });

  auto analyzeCacheImage = [](const std::string& cacheId, TDimension& size,
                              GuiSmokeRasterStats& stats) {
    TImageP image   = TImageCache::instance()->get(cacheId, false);
    TRasterP raster = gui_smoke_raster_from_image(image);
    size            = raster ? raster->getSize() : TDimension();
    stats           = gui_smoke_analyze_raster_pixels(raster);
    return (bool)raster;
  };

  TDimension firstCacheSize;
  TDimension secondCacheSize;
  GuiSmokeRasterStats firstCacheStats;
  GuiSmokeRasterStats secondCacheStats;
  bool firstFrameReady  = false;
  bool secondFrameReady = false;

  FlipBook* flipbook = manager->showNewPreview(previewFx, false);
  gui_smoke_pump_events(100);
  QElapsedTimer waitTimer;
  waitTimer.start();
  do {
    firstFrameReady =
        analyzeCacheImage(firstCacheId, firstCacheSize, firstCacheStats);
    secondFrameReady =
        analyzeCacheImage(secondCacheId, secondCacheSize, secondCacheStats);
    if (firstFrameReady && secondFrameReady) break;
    gui_smoke_pump_events(100);
  } while (waitTimer.elapsed() < 15000);
  gui_smoke_pump_events(100);

  firstFrameReady =
      analyzeCacheImage(firstCacheId, firstCacheSize, firstCacheStats);
  secondFrameReady =
      analyzeCacheImage(secondCacheId, secondCacheSize, secondCacheStats);

  QObject::disconnect(startedConnection);
  QObject::disconnect(renderedConnection);

  const bool flipbookOk = flipbook && flipbook->getPreviewedFx() == columnFx &&
                          flipbook->getPreviewXsheet() == xsheet;

  TDimension frozenFirstSize;
  TDimension frozenSecondSize;
  GuiSmokeRasterStats frozenFirstStats;
  GuiSmokeRasterStats frozenSecondStats;

  bool frameSwitchOk = false;
  if (flipbook) {
    flipbook->showFrame(2);
    gui_smoke_pump_events(100);
    frameSwitchOk = flipbook->getCurrentFrame() == 2;
  }

  const int beforeCloneCount =
      (int)gui_smoke_visible_preview_flipbooks(columnFx, xsheet).size();
  bool cloneOk        = false;
  int afterCloneCount = beforeCloneCount;
  if (flipbook) {
    flipbook->clonePreview();
    gui_smoke_pump_events(300);
    std::vector<FlipBook*> afterCloneFlipbooks =
        gui_smoke_visible_preview_flipbooks(columnFx, xsheet);
    afterCloneCount = (int)afterCloneFlipbooks.size();
    cloneOk         = afterCloneCount > beforeCloneCount;
    for (FlipBook* candidate : afterCloneFlipbooks) {
      if (candidate && candidate != flipbook) candidate->reset();
    }
    gui_smoke_pump_events(100);
  }

  bool freezeOk          = false;
  bool unfreezeOk        = false;
  bool frozenFirstReady  = false;
  bool frozenSecondReady = false;
  std::string frozenFirstCacheId;
  std::string frozenSecondCacheId;
  if (flipbook) {
    flipbook->showFrame(1);
    gui_smoke_pump_events(100);
    frozenFirstCacheId = "freezed" + std::to_string(flipbook->getPoolIndex()) +
                         ".noext" + std::to_string(0);
    frozenSecondCacheId = "freezed" + std::to_string(flipbook->getPoolIndex()) +
                          ".noext" + std::to_string(1);
    flipbook->freezePreview();
    gui_smoke_pump_events(200);
    freezeOk          = flipbook->isFreezed();
    frozenFirstReady  = analyzeCacheImage(frozenFirstCacheId, frozenFirstSize,
                                          frozenFirstStats);
    frozenSecondReady = analyzeCacheImage(frozenSecondCacheId, frozenSecondSize,
                                          frozenSecondStats);

    flipbook->unfreezePreview();
    gui_smoke_pump_events(300);
    unfreezeOk = !flipbook->isFreezed() &&
                 flipbook->getPreviewedFx() == columnFx &&
                 flipbook->getPreviewXsheet() == xsheet;
  }

  const bool controlsOk =
      flipbookOk && firstFrameReady && secondFrameReady &&
      firstCacheSize == TDimension(320, 240) &&
      secondCacheSize == TDimension(320, 240) &&
      firstCacheStats.redPixels > 1000 && secondCacheStats.greenPixels > 1000 &&
      frameSwitchOk && cloneOk && freezeOk && frozenFirstReady &&
      frozenSecondReady && frozenFirstSize == TDimension(320, 240) &&
      frozenSecondSize == TDimension(320, 240) &&
      frozenFirstStats.redPixels > 1000 &&
      frozenSecondStats.greenPixels > 1000 && unfreezeOk;

  if (flipbook) flipbook->reset();
  manager->reset(true);
  TImageCache::instance()->remove(firstCacheId);
  TImageCache::instance()->remove(secondCacheId);
  if (!frozenFirstCacheId.empty())
    TImageCache::instance()->remove(frozenFirstCacheId);
  if (!frozenSecondCacheId.empty())
    TImageCache::instance()->remove(frozenSecondCacheId);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("fxPreviewFlipbookControlsSource=two-frame-column-fx")
      << QString("fxPreviewFlipbookControlsFxId=%1").arg(previewFxId)
      << QString("fxPreviewFlipbookControlsCacheId0=%1")
             .arg(QString::fromStdString(firstCacheId))
      << QString("fxPreviewFlipbookControlsCacheId1=%1")
             .arg(QString::fromStdString(secondCacheId))
      << QString("fxPreviewFlipbookControlsFlipbook=%1")
             .arg(flipbook ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsFlipbookAttached=%1")
             .arg(flipbookOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsStartedFrames=%1").arg(startedFrames)
      << QString("fxPreviewFlipbookControlsCompletedFrames=%1")
             .arg(renderedFrames)
      << QString("fxPreviewFlipbookControlsFrame0Ready=%1")
             .arg(firstFrameReady ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsFrame1Ready=%1")
             .arg(secondFrameReady ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsFrame0RasterWidth=%1")
             .arg(firstCacheSize.lx)
      << QString("fxPreviewFlipbookControlsFrame0RasterHeight=%1")
             .arg(firstCacheSize.ly)
      << QString("fxPreviewFlipbookControlsFrame1RasterWidth=%1")
             .arg(secondCacheSize.lx)
      << QString("fxPreviewFlipbookControlsFrame1RasterHeight=%1")
             .arg(secondCacheSize.ly)
      << QString("fxPreviewFlipbookControlsFrame0RedPixels=%1")
             .arg(firstCacheStats.redPixels)
      << QString("fxPreviewFlipbookControlsFrame1GreenPixels=%1")
             .arg(secondCacheStats.greenPixels)
      << QString("fxPreviewFlipbookControlsFrameSwitch=%1")
             .arg(frameSwitchOk ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsVisibleBeforeClone=%1")
             .arg(beforeCloneCount)
      << QString("fxPreviewFlipbookControlsVisibleAfterClone=%1")
             .arg(afterCloneCount)
      << QString("fxPreviewFlipbookControlsClone=%1")
             .arg(cloneOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsFreeze=%1")
             .arg(freezeOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsFrozenFrame0RedPixels=%1")
             .arg(frozenFirstStats.redPixels)
      << QString("fxPreviewFlipbookControlsFrozenFrame1GreenPixels=%1")
             .arg(frozenSecondStats.greenPixels)
      << QString("fxPreviewFlipbookControlsUnfreeze=%1")
             .arg(unfreezeOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewFlipbookControlsProbe=%1")
             .arg(controlsOk ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static QFileInfoList gui_smoke_png_output_entries(const QString& outputDir,
                                                  const QString& prefix) {
  QDir dir(outputDir);
  return dir.entryInfoList(QStringList() << (prefix + QStringLiteral("*.png")),
                           QDir::Files, QDir::Name);
}

static QStringList gui_smoke_fx_preview_save_previewed_frames_details(
    const QString& requestedSceneName, bool subcameraPreview = false) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral(
                   "fxPreviewSavePreviewedFramesProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));

  const TRect subcameraRect(80, 60, 239, 179);
  if (subcameraPreview) {
    TCamera* previewCamera = scene->getCurrentPreviewCamera();
    if (previewCamera != camera) {
      previewCamera->setRes(TDimension(320, 240));
      previewCamera->setSize(
          TDimensionD(previewCamera->getRes().lx / renderDpi,
                      previewCamera->getRes().ly / renderDpi));
    }
    previewCamera->setInterestRect(subcameraRect);
  }
  const TDimension expectedPreviewSize =
      subcameraPreview
          ? TDimension(subcameraRect.getLx(), subcameraRect.getLy())
          : TDimension(320, 240);

  TOutputProperties* previewProperties =
      scene->getProperties()->getPreviewProperties();
  previewProperties->setRange(0, 1, 1);
  previewProperties->setThreadIndex(0);
  previewProperties->setMaxTileSizeIndex(1);
  previewProperties->setSubcameraPreview(subcameraPreview);
  TRenderSettings previewSettings = previewProperties->getRenderSettings();
  previewSettings.m_bpp           = 32;
  previewSettings.m_shrinkX       = 1;
  previewSettings.m_shrinkY       = 1;
  previewSettings.m_applyShrinkToViewer = false;
  previewSettings.m_stereoscopic        = false;
  previewSettings.m_getFullSizeBBox     = false;
  previewSettings.m_cameraBox =
      TRectD(TPointD(-160.0, -120.0), TDimensionD(320.0, 240.0));
  previewProperties->setRenderSettings(previewSettings);

  TOutputProperties* outputProperties =
      scene->getProperties()->getOutputProperties();
  TRenderSettings outputSettings = outputProperties->getRenderSettings();
  outputSettings.m_bpp           = 32;
  outputProperties->setRenderSettings(outputSettings);

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL,
      subcameraPreview
          ? L"qt6_gui_fx_preview_subcamera_save_previewed_frames_raster"
          : L"qt6_gui_fx_preview_save_previewed_frames_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("fxPreviewSavePreviewedFramesProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId firstFid(1);
  const TFrameId secondFid(2);
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, firstFid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(232, 24, 24, 255), camera->getDpi());
  gui_smoke_add_raster_rect_probe_frame(
      simpleLevel, secondFid, TDimension(320, 240), TRect(0, 0, 320, 240),
      TPixel32(24, 232, 24, 255), camera->getDpi());

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, firstFid)) ||
      !xsheet->setCell(1, 0, TXshCell(simpleLevel, secondFid))) {
    details << QStringLiteral("fxPreviewSavePreviewedFramesProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  gui_smoke_pump_events(100);

  TXshColumn* column = xsheet->getColumn(0);
  TFx* columnFx      = column ? column->getFx() : nullptr;
  if (!columnFx) {
    details << QStringLiteral(
                   "fxPreviewSavePreviewedFramesProbe=column-fx-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFxP previewFx(columnFx);
  const unsigned long previewFxId = previewFx->getIdentifier();
  const std::string firstCacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(0);
  const std::string secondCacheId =
      std::to_string(previewFxId) + ".noext" + std::to_string(1);

  PreviewFxManager* manager = PreviewFxManager::instance();
  manager->reset(true);
  TImageCache::instance()->remove(firstCacheId);
  TImageCache::instance()->remove(secondCacheId);
  gui_smoke_pump_events(100);

  auto analyzeCacheImage = [](const std::string& cacheId, TDimension& size,
                              GuiSmokeRasterStats& stats) {
    TImageP image   = TImageCache::instance()->get(cacheId, false);
    TRasterP raster = gui_smoke_raster_from_image(image);
    size            = raster ? raster->getSize() : TDimension();
    stats           = gui_smoke_analyze_raster_pixels(raster);
    return (bool)raster;
  };

  TDimension firstCacheSize;
  TDimension secondCacheSize;
  GuiSmokeRasterStats firstCacheStats;
  GuiSmokeRasterStats secondCacheStats;
  bool firstFrameReady  = false;
  bool secondFrameReady = false;

  FlipBook* flipbook = manager->showNewPreview(previewFx, false);
  gui_smoke_pump_events(100);
  QElapsedTimer waitTimer;
  waitTimer.start();
  do {
    firstFrameReady =
        analyzeCacheImage(firstCacheId, firstCacheSize, firstCacheStats);
    secondFrameReady =
        analyzeCacheImage(secondCacheId, secondCacheSize, secondCacheStats);
    if (firstFrameReady && secondFrameReady) break;
    gui_smoke_pump_events(100);
  } while (waitTimer.elapsed() < 15000);
  gui_smoke_pump_events(100);

  firstFrameReady =
      analyzeCacheImage(firstCacheId, firstCacheSize, firstCacheStats);
  secondFrameReady =
      analyzeCacheImage(secondCacheId, secondCacheSize, secondCacheStats);

  const bool flipbookOk = flipbook && flipbook->getPreviewedFx() == columnFx &&
                          flipbook->getPreviewXsheet() == xsheet;
  const bool subcameraActive = manager->isSubCameraActive(previewFx);

  QString statusPath = qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  const QString outputPrefix =
      subcameraPreview
          ? QStringLiteral("fx-preview-subcamera-save-previewed-frames")
          : QStringLiteral("fx-preview-save-previewed-frames");
  QString outputDir =
      statusPath.isEmpty()
          ? QDir(QDir::tempPath()).filePath(outputPrefix)
          : QFileInfo(statusPath).absoluteDir().filePath(outputPrefix);
  QDir().mkpath(outputDir);
  QDir saveDir(outputDir);
  for (const QFileInfo& entry :
       gui_smoke_png_output_entries(outputDir, outputPrefix)) {
    QFile::remove(entry.absoluteFilePath());
  }

  const QString requestedOutputPath =
      saveDir.filePath(outputPrefix + QStringLiteral("..png"));
  const bool saveStarted =
      flipbook && firstFrameReady && secondFrameReady &&
      flipbook->doSaveImages(TFilePath(requestedOutputPath.toStdWString()),
                             false);

  QFileInfoList outputEntries;
  QImage firstOutputImage;
  QImage secondOutputImage;
  GuiSmokeImageStats firstOutputStats;
  GuiSmokeImageStats secondOutputStats;
  bool outputOk = false;
  waitTimer.restart();
  do {
    gui_smoke_pump_events(100);
    outputEntries = gui_smoke_png_output_entries(outputDir, outputPrefix);
    if (outputEntries.size() >= 2) {
      firstOutputImage = QImage(outputEntries.at(0).absoluteFilePath())
                             .convertToFormat(QImage::Format_ARGB32);
      secondOutputImage = QImage(outputEntries.at(1).absoluteFilePath())
                              .convertToFormat(QImage::Format_ARGB32);
      firstOutputStats  = gui_smoke_analyze_viewer_frame(firstOutputImage);
      secondOutputStats = gui_smoke_analyze_viewer_frame(secondOutputImage);
      outputOk = !firstOutputImage.isNull() && !secondOutputImage.isNull() &&
                 firstOutputImage.width() == expectedPreviewSize.lx &&
                 firstOutputImage.height() == expectedPreviewSize.ly &&
                 secondOutputImage.width() == expectedPreviewSize.lx &&
                 secondOutputImage.height() == expectedPreviewSize.ly &&
                 firstOutputStats.redPixels > 1000 &&
                 secondOutputStats.greenPixels > 1000;
      if (outputOk) break;
    }
  } while (waitTimer.elapsed() < 15000);
  gui_smoke_pump_events(100);

  outputEntries = gui_smoke_png_output_entries(outputDir, outputPrefix);
  if (outputEntries.size() >= 2) {
    firstOutputImage = QImage(outputEntries.at(0).absoluteFilePath())
                           .convertToFormat(QImage::Format_ARGB32);
    secondOutputImage = QImage(outputEntries.at(1).absoluteFilePath())
                            .convertToFormat(QImage::Format_ARGB32);
    firstOutputStats  = gui_smoke_analyze_viewer_frame(firstOutputImage);
    secondOutputStats = gui_smoke_analyze_viewer_frame(secondOutputImage);
    outputOk = !firstOutputImage.isNull() && !secondOutputImage.isNull() &&
               firstOutputImage.width() == expectedPreviewSize.lx &&
               firstOutputImage.height() == expectedPreviewSize.ly &&
               secondOutputImage.width() == expectedPreviewSize.lx &&
               secondOutputImage.height() == expectedPreviewSize.ly &&
               firstOutputStats.redPixels > 1000 &&
               secondOutputStats.greenPixels > 1000;
  }

  const QFileInfo firstOutputInfo =
      outputEntries.size() > 0 ? outputEntries.at(0) : QFileInfo();
  const QFileInfo secondOutputInfo =
      outputEntries.size() > 1 ? outputEntries.at(1) : QFileInfo();
  const bool saveOk =
      flipbookOk && (!subcameraPreview || subcameraActive) && firstFrameReady &&
      secondFrameReady && firstCacheSize == expectedPreviewSize &&
      secondCacheSize == expectedPreviewSize &&
      firstCacheStats.redPixels > 1000 && secondCacheStats.greenPixels > 1000 &&
      saveStarted && outputEntries.size() == 2 && outputOk;

  if (flipbook) flipbook->reset();
  manager->reset(true);
  TImageCache::instance()->remove(firstCacheId);
  TImageCache::instance()->remove(secondCacheId);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("fxPreviewSavePreviewedFramesSource=%1")
             .arg(subcameraPreview
                      ? QStringLiteral("two-frame-subcamera-column-fx")
                      : QStringLiteral("two-frame-column-fx"))
      << QString("fxPreviewSavePreviewedFramesFxId=%1").arg(previewFxId)
      << QString("fxPreviewSavePreviewedFramesSubcamera=%1")
             .arg(subcameraPreview ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesSubcameraActive=%1")
             .arg(subcameraActive ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesExpectedWidth=%1")
             .arg(expectedPreviewSize.lx)
      << QString("fxPreviewSavePreviewedFramesExpectedHeight=%1")
             .arg(expectedPreviewSize.ly)
      << QString("fxPreviewSavePreviewedFramesFlipbook=%1")
             .arg(flipbook ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesFlipbookAttached=%1")
             .arg(flipbookOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesFrame0Ready=%1")
             .arg(firstFrameReady ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesFrame1Ready=%1")
             .arg(secondFrameReady ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesFrame0RedPixels=%1")
             .arg(firstCacheStats.redPixels)
      << QString("fxPreviewSavePreviewedFramesFrame1GreenPixels=%1")
             .arg(secondCacheStats.greenPixels)
      << QString("fxPreviewSavePreviewedFramesStarted=%1")
             .arg(saveStarted ? QStringLiteral("true")
                              : QStringLiteral("false"))
      << QString("fxPreviewSavePreviewedFramesRequestedOutputPath=%1")
             .arg(gui_smoke_status_value(requestedOutputPath))
      << QString("fxPreviewSavePreviewedFramesOutputDir=%1")
             .arg(gui_smoke_status_value(outputDir))
      << QString("fxPreviewSavePreviewedFramesOutputCount=%1")
             .arg(outputEntries.size())
      << QString("fxPreviewSavePreviewedFramesOutput0Path=%1")
             .arg(gui_smoke_status_value(firstOutputInfo.absoluteFilePath()))
      << QString("fxPreviewSavePreviewedFramesOutput1Path=%1")
             .arg(gui_smoke_status_value(secondOutputInfo.absoluteFilePath()))
      << QString("fxPreviewSavePreviewedFramesOutput0Bytes=%1")
             .arg(firstOutputInfo.size())
      << QString("fxPreviewSavePreviewedFramesOutput1Bytes=%1")
             .arg(secondOutputInfo.size())
      << QString("fxPreviewSavePreviewedFramesOutput0Width=%1")
             .arg(firstOutputImage.width())
      << QString("fxPreviewSavePreviewedFramesOutput0Height=%1")
             .arg(firstOutputImage.height())
      << QString("fxPreviewSavePreviewedFramesOutput1Width=%1")
             .arg(secondOutputImage.width())
      << QString("fxPreviewSavePreviewedFramesOutput1Height=%1")
             .arg(secondOutputImage.height())
      << QString("fxPreviewSavePreviewedFramesOutput0RedPixels=%1")
             .arg(firstOutputStats.redPixels)
      << QString("fxPreviewSavePreviewedFramesOutput1GreenPixels=%1")
             .arg(secondOutputStats.greenPixels)
      << QString("fxPreviewSavePreviewedFramesOutputProbe=%1")
             .arg(outputOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("fxPreviewSavePreviewedFramesProbe=%1")
             .arg(saveOk ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static QFileInfoList gui_smoke_final_render_output_entries(
    const QString& outputDir) {
  QDir dir(outputDir);
  return dir.entryInfoList(
      QStringList() << QStringLiteral("final-render-output*.png"), QDir::Files,
      QDir::Name);
}

static QStringList gui_smoke_final_render_output_details(
    const QString& requestedSceneName, bool opaqueBackground = false,
    bool sequenceOutput = false, bool compositeOutput = false,
    bool vectorOutput = false, bool fxOutput = false,
    bool toonzRasterOutput = false) {
  QStringList details;
  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("finalRenderProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TCamera* camera = scene->getCurrentCamera();
  camera->setRes(TDimension(320, 240));
  const double renderDpi = Stage::standardDpi;
  camera->setSize(TDimensionD(camera->getRes().lx / renderDpi,
                              camera->getRes().ly / renderDpi));
  if (opaqueBackground)
    scene->getProperties()->setBgColor(TPixel32(24, 24, 232, 255));

  int levelType          = OVL_XSHLEVEL;
  std::wstring levelName = L"qt6_gui_final_render_raster";
  if (vectorOutput) {
    levelType = PLI_XSHLEVEL;
    levelName = L"qt6_gui_final_render_vector";
  } else if (toonzRasterOutput) {
    levelType = TZP_XSHLEVEL;
    levelName = L"qt6_gui_final_render_toonz_raster";
  }
  TXshLevel* levelXl           = scene->createNewLevel(levelType, levelName);
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("finalRenderProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  TXshLevel* compositeLevelXl           = nullptr;
  TXshSimpleLevel* compositeSimpleLevel = nullptr;
  if (compositeOutput) {
    compositeLevelXl = scene->createNewLevel(
        OVL_XSHLEVEL, L"qt6_gui_final_render_composite_raster");
    compositeSimpleLevel =
        compositeLevelXl ? compositeLevelXl->getSimpleLevel() : nullptr;
    if (!compositeSimpleLevel) {
      details << QStringLiteral("finalRenderProbe=level-error")
              << QString("scene=%1").arg(scenePath.getQString());
      return details;
    }
  }

  const TFrameId fid(1);
  const TFrameId secondFid(2);
  if (vectorOutput) {
    if (!gui_smoke_add_vector_probe_frame(simpleLevel, fid)) {
      details << QStringLiteral("finalRenderProbe=vector-frame-error")
              << QString("scene=%1").arg(scenePath.getQString());
      return details;
    }
  } else if (toonzRasterOutput) {
    if (!gui_smoke_add_toonz_raster_rect_probe_frame(
            simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 320, 240),
            camera->getDpi())) {
      details << QStringLiteral("finalRenderProbe=toonz-raster-frame-error")
              << QString("scene=%1").arg(scenePath.getQString());
      return details;
    }
  } else if (fxOutput) {
    gui_smoke_add_raster_rect_probe_frame(
        simpleLevel, fid, TDimension(320, 240), TRect(95, 70, 225, 170),
        TPixel32(232, 24, 24, 255), camera->getDpi());
  } else if (compositeOutput) {
    gui_smoke_add_raster_rect_probe_frame(
        simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 150, 240),
        TPixel32(232, 24, 24, 255), camera->getDpi());
    gui_smoke_add_raster_rect_probe_frame(
        compositeSimpleLevel, fid, TDimension(320, 240),
        TRect(170, 0, 320, 240), TPixel32(24, 232, 24, 255), camera->getDpi());
  } else if (sequenceOutput) {
    gui_smoke_add_raster_rect_probe_frame(
        simpleLevel, fid, TDimension(320, 240), TRect(0, 0, 320, 240),
        TPixel32(232, 24, 24, 255), camera->getDpi());
    gui_smoke_add_raster_rect_probe_frame(
        simpleLevel, secondFid, TDimension(320, 240), TRect(0, 0, 320, 240),
        TPixel32(24, 232, 24, 255), camera->getDpi());
  } else if (opaqueBackground) {
    gui_smoke_add_raster_rect_probe_frame(
        simpleLevel, fid, TDimension(320, 240), TRect(70, 50, 250, 190),
        TPixel32(232, 24, 24, 255), camera->getDpi());
  } else {
    gui_smoke_add_raster_probe_frame(simpleLevel, fid, TDimension(320, 240),
                                     camera->getDpi());
  }
  const GuiSmokeRasterStats sourceStats =
      vectorOutput || toonzRasterOutput
          ? GuiSmokeRasterStats()
          : gui_smoke_analyze_raster_frame(simpleLevel, fid);
  TPointD levelDpi      = simpleLevel->getDpi(fid);
  TImageInfo* frameInfo = simpleLevel->getFrameInfo(fid, false);
  TRasterImageP sourceImage =
      vectorOutput ? TRasterImageP()
                   : (TRasterImageP)simpleLevel->getFrame(fid, false);
  TRect sourceSavebox;
  TRectD sourceBBox;
  TDimension sourceRasterSize;
  if (sourceImage) {
    sourceSavebox = sourceImage->getSavebox();
    sourceBBox    = sourceImage->getBBox();
    if (sourceImage->getRaster())
      sourceRasterSize = sourceImage->getRaster()->getSize();
  }
  const TAffine cameraDpiAffine   = getDpiAffine(camera);
  const TAffine levelDpiAffine    = getDpiAffine(simpleLevel, fid, true);
  const TAffine combinedDpiAffine = cameraDpiAffine.inv() * levelDpiAffine;

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("finalRenderProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  if (sequenceOutput &&
      !xsheet->setCell(1, 0, TXshCell(simpleLevel, secondFid))) {
    details << QStringLiteral("finalRenderProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  if (compositeOutput &&
      !xsheet->setCell(0, 1, TXshCell(compositeSimpleLevel, fid))) {
    details << QStringLiteral("finalRenderProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  const QString outputDir = gui_smoke_capture_output_dir();
  QDir().mkpath(outputDir);
  QDir renderDir(outputDir);
  const QFileInfoList staleEntries = renderDir.entryInfoList(
      QStringList() << QStringLiteral("final-render-output*.png"), QDir::Files,
      QDir::Name);
  for (const QFileInfo& entry : staleEntries)
    QFile::remove(entry.absoluteFilePath());

  const QString requestedOutputPath =
      renderDir.filePath(QStringLiteral("final-render-output..png"));
  const TFilePath renderPath(requestedOutputPath.toStdWString());

  TOutputProperties* outputProperties =
      scene->getProperties()->getOutputProperties();
  outputProperties->setPath(renderPath);
  outputProperties->setRange(0, sequenceOutput ? 1 : 0, 1);
  outputProperties->setThreadIndex(0);
  outputProperties->setMaxTileSizeIndex(1);
  outputProperties->setMultimediaRendering(0);

  TRenderSettings renderSettings = outputProperties->getRenderSettings();
  renderSettings.m_shrinkX       = 1;
  renderSettings.m_shrinkY       = 1;
  renderSettings.m_stereoscopic  = false;
  outputProperties->setRenderSettings(renderSettings);

  const int xsheetColumnCount = xsheet->getColumnCount();
  TXshColumn* column0         = xsheet->getColumn(0);
  TFx* columnFx               = column0 ? column0->getFx() : nullptr;
  FxDag* fxDag                = xsheet->getFxDag();
  TFxSet* terminalFxs         = fxDag ? fxDag->getTerminalFxs() : nullptr;
  const int terminalFxCount   = terminalFxs ? terminalFxs->getFxCount() : -1;
  TFx* terminalFx0    = terminalFxCount > 0 ? terminalFxs->getFx(0) : nullptr;
  TOutputFx* outputFx = fxDag ? fxDag->getOutputFx(0) : nullptr;
  TFx* outputInputFx  = outputFx && outputFx->getInputPortCount() > 0
                            ? outputFx->getInputPort(0)->getFx()
                            : nullptr;
  TRasterFx* rasterColumnFx = dynamic_cast<TRasterFx*>(columnFx);
  TRectD columnBBox;
  const bool columnBBoxOk =
      rasterColumnFx &&
      rasterColumnFx->getBBox(0.0, columnBBox, renderSettings);
  TAffine columnPlacement;
  const bool columnPlacementOk =
      getColumnPlacement(columnPlacement, xsheet, 0.0, 0, false);
  const int whichLevels = outputProperties->getWhichLevels();
  const TFxP columnRoot = columnFx ? TFxP(columnFx) : TFxP();
  QStringList sceneFxBBoxDetails;
  sceneFxBBoxDetails += gui_smoke_fx_bbox_details(
      QStringLiteral("finalRenderUnifiedXsheetDefault"),
      buildSceneFx(scene, 0.0, xsheet, TFxP(), BSFX_DEFAULT_TR, false,
                   whichLevels, renderSettings.m_shrinkX),
      0.0, renderSettings);
  sceneFxBBoxDetails += gui_smoke_fx_bbox_details(
      QStringLiteral("finalRenderColumnNoTr"),
      buildSceneFx(scene, 0.0, xsheet, columnRoot, BSFX_NO_TR, false,
                   whichLevels, renderSettings.m_shrinkX),
      0.0, renderSettings);
  sceneFxBBoxDetails += gui_smoke_fx_bbox_details(
      QStringLiteral("finalRenderColumnColumnTr"),
      buildSceneFx(scene, 0.0, xsheet, columnRoot, BSFX_COLUMN_TR, false,
                   whichLevels, renderSettings.m_shrinkX),
      0.0, renderSettings);
  sceneFxBBoxDetails += gui_smoke_fx_bbox_details(
      QStringLiteral("finalRenderColumnCameraColumnTr"),
      buildSceneFx(scene, 0.0, xsheet, columnRoot,
                   BSFX_Transforms_Enum(BSFX_CAMERA_TR | BSFX_COLUMN_TR), false,
                   whichLevels, renderSettings.m_shrinkX),
      0.0, renderSettings);
  sceneFxBBoxDetails += gui_smoke_fx_bbox_details(
      QStringLiteral("finalRenderColumnDefault"),
      buildSceneFx(scene, 0.0, xsheet, columnRoot, BSFX_DEFAULT_TR, false,
                   whichLevels, renderSettings.m_shrinkX),
      0.0, renderSettings);

  TFxPair fxPair;
  TFxP baseSceneFx = buildSceneFx(scene, 0.0, renderSettings.m_shrinkX, false);
  fxPair.m_frameA =
      fxOutput ? TFxUtil::makeBlur(baseSceneFx, 8.0) : baseSceneFx;
  fxPair.m_frameB = TRasterFxP();
  if (!fxPair.m_frameA) {
    details << QStringLiteral("finalRenderProbe=build-fx-error")
            << QString("scene=%1").arg(scenePath.getQString())
            << QString("requestedOutputPath=%1")
                   .arg(gui_smoke_status_value(requestedOutputPath));
    return details;
  }
  TFxPair secondFxPair;
  if (sequenceOutput) {
    secondFxPair.m_frameA =
        buildSceneFx(scene, 1.0, renderSettings.m_shrinkX, false);
    secondFxPair.m_frameB = TRasterFxP();
    if (!secondFxPair.m_frameA) {
      details << QStringLiteral("finalRenderProbe=build-fx-error")
              << QString("scene=%1").arg(scenePath.getQString())
              << QString("requestedOutputPath=%1")
                     .arg(gui_smoke_status_value(requestedOutputPath));
      return details;
    }
  }
  TRectD rootBBox;
  const bool rootBBoxOk =
      fxPair.m_frameA->getBBox(0.0, rootBBox, renderSettings);
  TRenderSettings fullSizeBBoxSettings(renderSettings);
  fullSizeBBoxSettings.m_getFullSizeBBox = true;
  TRectD rootFullSizeBBox;
  const bool rootFullSizeBBoxOk =
      fxPair.m_frameA->getBBox(0.0, rootFullSizeBBox, fullSizeBBoxSettings);

  GuiSmokeFinalRenderListener listener;
  QEventLoop loop;
  listener.m_loop = &loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

  GuiSmokeFinalRenderCapturePort capturePort;
  const TDimension cameraRes = camera->getRes();
  capturePort.setRenderArea(TRectD(-0.5 * cameraRes.lx, -0.5 * cameraRes.ly,
                                   0.5 * cameraRes.lx, 0.5 * cameraRes.ly));

  MovieRenderer renderer(scene, renderPath, 1, false);
  renderer.setRenderSettings(renderSettings);
  TPointD dpi = camera->getDpi();
  renderer.setDpi(dpi.x, dpi.y);
  renderer.enablePrecomputing(false);
  renderer.addListener(&listener);
  renderer.getTRenderer()->addPort(&capturePort);
  renderer.addFrame(0.0, fxPair);
  if (sequenceOutput) renderer.addFrame(1.0, secondFxPair);
  timeoutTimer.start(15000);
  renderer.start();
  loop.exec();
  listener.m_loop = nullptr;
  timeoutTimer.stop();
  renderer.getTRenderer()->removePort(&capturePort);

  gui_smoke_pump_events(100);

  const QFileInfoList outputEntries =
      gui_smoke_final_render_output_entries(outputDir);
  const QString outputPath       = outputEntries.isEmpty()
                                       ? QString()
                                       : outputEntries.first().absoluteFilePath();
  const QString secondOutputPath = outputEntries.size() > 1
                                       ? outputEntries.at(1).absoluteFilePath()
                                       : QString();
  const QFileInfo outputInfo(outputPath);
  const QFileInfo secondOutputInfo(secondOutputPath);
  const QImage outputImage =
      outputPath.isEmpty()
          ? QImage()
          : QImage(outputPath).convertToFormat(QImage::Format_ARGB32);
  const QImage secondOutputImage =
      secondOutputPath.isEmpty()
          ? QImage()
          : QImage(secondOutputPath).convertToFormat(QImage::Format_ARGB32);
  const GuiSmokeImageStats stats = gui_smoke_analyze_viewer_frame(outputImage);
  const GuiSmokeImageStats secondStats =
      gui_smoke_analyze_viewer_frame(secondOutputImage);
  const bool firstColorPixelsOk =
      compositeOutput    ? stats.redPixels > 1000 && stats.greenPixels > 1000
      : opaqueBackground ? stats.redPixels > 1000 && stats.bluePixels > 1000
                         : stats.redPixels > 1000;
  const bool secondOutputOk =
      !sequenceOutput ||
      (secondOutputInfo.exists() && secondOutputInfo.size() > 0 &&
       !secondOutputImage.isNull() && secondOutputImage.width() == 320 &&
       secondOutputImage.height() == 240 && secondStats.greenPixels > 1000);
  const bool outputCountOk = outputEntries.size() == (sequenceOutput ? 2 : 1);
  const bool outputOk =
      outputCountOk && outputInfo.exists() && outputInfo.size() > 0 &&
      !outputImage.isNull() && outputImage.width() == 320 &&
      outputImage.height() == 240 && firstColorPixelsOk && secondOutputOk;
  const int expectedFrames = sequenceOutput ? 2 : 1;
  const bool renderOk      = listener.m_sequenceCompleted &&
                        listener.m_completedFrames == expectedFrames &&
                        listener.m_failedFrames == 0 && outputOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("finalRenderFxSource=%1")
             .arg(compositeOutput
                      ? QStringLiteral("two-column-raster-composite")
                  : vectorOutput ? QStringLiteral("vector-scene-standard-dpi")
                  : toonzRasterOutput
                      ? QStringLiteral("toonz-raster-scene-standard-dpi")
                  : fxOutput ? QStringLiteral("blurred-partial-raster-fx")
                  : opaqueBackground
                      ? QStringLiteral("partial-raster-opaque-background")
                      : QStringLiteral("raster-scene-standard-dpi"))
      << QString("finalRenderFxApplied=%1")
             .arg(fxOutput ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("finalRenderFxRoot=%1")
             .arg(gui_smoke_fx_value(fxPair.m_frameA.getPointer()))
      << QString("finalRenderOpaqueBackground=%1")
             .arg(opaqueBackground ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("finalRenderSequenceOutput=%1")
             .arg(sequenceOutput ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("finalRenderCompositeOutput=%1")
             .arg(compositeOutput ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("finalRenderVectorOutput=%1")
             .arg(vectorOutput ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("finalRenderToonzRasterOutput=%1")
             .arg(toonzRasterOutput ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("finalRenderExpectedFrames=%1").arg(expectedFrames)
      << QString("finalRenderCameraRes=%1x%2")
             .arg(cameraRes.lx)
             .arg(cameraRes.ly)
      << QString("finalRenderCameraDpi=%1x%2")
             .arg(dpi.x, 0, 'f', 2)
             .arg(dpi.y, 0, 'f', 2)
      << QString("finalRenderLevelDpi=%1x%2")
             .arg(levelDpi.x, 0, 'f', 2)
             .arg(levelDpi.y, 0, 'f', 2)
      << QString("finalRenderFrameInfoPresent=%1")
             .arg(frameInfo ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("finalRenderFrameInfoValid=%1")
             .arg(frameInfo && frameInfo->m_valid ? QStringLiteral("true")
                                                  : QStringLiteral("false"))
      << QString("finalRenderFrameInfoSize=%1x%2")
             .arg(frameInfo ? frameInfo->m_lx : -1)
             .arg(frameInfo ? frameInfo->m_ly : -1)
      << QString("finalRenderFrameInfoSavebox=%1,%2,%3,%4")
             .arg(frameInfo ? frameInfo->m_x0 : 0)
             .arg(frameInfo ? frameInfo->m_y0 : 0)
             .arg(frameInfo ? frameInfo->m_x1 : -1)
             .arg(frameInfo ? frameInfo->m_y1 : -1)
      << QString("finalRenderFrameInfoDpi=%1x%2")
             .arg(frameInfo ? frameInfo->m_dpix : 0.0, 0, 'f', 2)
             .arg(frameInfo ? frameInfo->m_dpiy : 0.0, 0, 'f', 2)
      << QString("finalRenderSourceRasterSize=%1x%2")
             .arg(sourceRasterSize.lx)
             .arg(sourceRasterSize.ly)
      << QString("finalRenderSourceSavebox=%1")
             .arg(sourceImage ? gui_smoke_rect_value(sourceSavebox)
                              : QStringLiteral("missing"))
      << QString("finalRenderSourceBBox=%1")
             .arg(sourceImage ? gui_smoke_rectd_value(sourceBBox)
                              : QStringLiteral("missing"))
      << QString("finalRenderCameraDpiAffine=%1")
             .arg(gui_smoke_affine_value(cameraDpiAffine))
      << QString("finalRenderLevelDpiAffine=%1")
             .arg(gui_smoke_affine_value(levelDpiAffine))
      << QString("finalRenderCombinedDpiAffine=%1")
             .arg(gui_smoke_affine_value(combinedDpiAffine))
      << QString("finalRenderWhichLevels=%1").arg(whichLevels)
      << QString("finalRenderXsheetColumnCount=%1").arg(xsheetColumnCount)
      << QString("finalRenderColumn0Present=%1")
             .arg(column0 ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("finalRenderColumn0Empty=%1")
             .arg(column0 && column0->isEmpty() ? QStringLiteral("true")
                                                : QStringLiteral("false"))
      << QString("finalRenderColumn0PreviewVisible=%1")
             .arg(column0 && column0->isPreviewVisible()
                      ? QStringLiteral("true")
                      : QStringLiteral("false"))
      << QString("finalRenderColumn0Rendered=%1")
             .arg(column0 && column0->isRendered() ? QStringLiteral("true")
                                                   : QStringLiteral("false"))
      << QString("finalRenderColumn0Type=%1")
             .arg(column0 ? static_cast<int>(column0->getColumnType()) : -1)
      << QString("finalRenderColumnFx=%1").arg(gui_smoke_fx_value(columnFx))
      << QString("finalRenderColumnFxOutputConnections=%1")
             .arg(columnFx ? columnFx->getOutputConnectionCount() : -1)
      << QString("finalRenderColumnBBoxOk=%1")
             .arg(columnBBoxOk ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("finalRenderColumnBBox=%1")
             .arg(columnBBoxOk ? gui_smoke_rectd_value(columnBBox)
                               : QStringLiteral("none"))
      << QString("finalRenderColumnPlacementOk=%1")
             .arg(columnPlacementOk ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("finalRenderColumnPlacement=%1")
             .arg(columnPlacementOk ? gui_smoke_affine_value(columnPlacement)
                                    : QStringLiteral("none"))
      << QString("finalRenderTerminalFxCount=%1").arg(terminalFxCount)
      << QString("finalRenderTerminalFx0=%1")
             .arg(gui_smoke_fx_value(terminalFx0))
      << QString("finalRenderTerminalContainsColumnFx=%1")
             .arg(terminalFxs && terminalFxs->containsFx(columnFx)
                      ? QStringLiteral("true")
                      : QStringLiteral("false"))
      << QString("finalRenderOutputFx=%1").arg(gui_smoke_fx_value(outputFx))
      << QString("finalRenderOutputInputFx=%1")
             .arg(gui_smoke_fx_value(outputInputFx))
      << QString("finalRenderOutputInputIsXsheetFx=%1")
             .arg(fxDag && outputInputFx == fxDag->getXsheetFx()
                      ? QStringLiteral("true")
                      : QStringLiteral("false"))
      << sceneFxBBoxDetails
      << QString("finalRenderRootBBoxOk=%1")
             .arg(rootBBoxOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("finalRenderRootBBox=%1").arg(gui_smoke_rectd_value(rootBBox))
      << QString("finalRenderRootFullSizeBBoxOk=%1")
             .arg(rootFullSizeBBoxOk ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("finalRenderRootFullSizeBBox=%1")
             .arg(gui_smoke_rectd_value(rootFullSizeBBox))
      << QString("finalRenderSourceSampleCount=%1").arg(sourceStats.sampleCount)
      << QString("finalRenderSourceOpaquePixels=%1")
             .arg(sourceStats.opaquePixels)
      << QString("finalRenderSourceRedPixels=%1").arg(sourceStats.redPixels)
      << QString("requestedOutputPath=%1")
             .arg(gui_smoke_status_value(requestedOutputPath))
      << QString("finalRenderCapturedRasters=%1")
             .arg(capturePort.m_completedRasters)
      << QString("finalRenderCapturedFailures=%1")
             .arg(capturePort.m_failedRasters)
      << QString("finalRenderCapturedError=%1")
             .arg(gui_smoke_status_value(capturePort.m_error))
      << QString("finalRenderCapturedWidth=%1").arg(capturePort.m_rasterSize.lx)
      << QString("finalRenderCapturedHeight=%1")
             .arg(capturePort.m_rasterSize.ly)
      << QString("finalRenderCapturedSampleCount=%1")
             .arg(capturePort.m_stats.sampleCount)
      << QString("finalRenderCapturedOpaquePixels=%1")
             .arg(capturePort.m_stats.opaquePixels)
      << QString("finalRenderCapturedRedPixels=%1")
             .arg(capturePort.m_stats.redPixels)
      << QString("finalRenderSequenceCompleted=%1")
             .arg(listener.m_sequenceCompleted ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("finalRenderCompletedFrames=%1")
             .arg(listener.m_completedFrames)
      << QString("finalRenderFailedFrames=%1").arg(listener.m_failedFrames)
      << QString("finalRenderError=%1")
             .arg(gui_smoke_status_value(listener.m_error))
      << QString("finalRenderReportedPath=%1")
             .arg(gui_smoke_status_value(listener.m_outputPath.getQString()))
      << QString("finalRenderOutputPath=%1")
             .arg(gui_smoke_status_value(outputPath))
      << QString("finalRenderOutputFrameCount=%1").arg(outputEntries.size())
      << QString("finalRenderOutputExists=%1")
             .arg(outputInfo.exists() ? QStringLiteral("true")
                                      : QStringLiteral("false"))
      << QString("finalRenderOutputBytes=%1").arg(outputInfo.size())
      << QString("finalRenderOutputWidth=%1").arg(outputImage.width())
      << QString("finalRenderOutputHeight=%1").arg(outputImage.height())
      << QString("finalRenderOutputSampleCount=%1").arg(stats.sampleCount)
      << QString("finalRenderOutputRedPixels=%1").arg(stats.redPixels)
      << QString("finalRenderOutputGreenPixels=%1").arg(stats.greenPixels)
      << QString("finalRenderOutputBluePixels=%1").arg(stats.bluePixels)
      << QString("finalRenderSecondOutputPath=%1")
             .arg(gui_smoke_status_value(secondOutputPath))
      << QString("finalRenderSecondOutputExists=%1")
             .arg(secondOutputInfo.exists() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("finalRenderSecondOutputBytes=%1").arg(secondOutputInfo.size())
      << QString("finalRenderSecondOutputWidth=%1")
             .arg(secondOutputImage.width())
      << QString("finalRenderSecondOutputHeight=%1")
             .arg(secondOutputImage.height())
      << QString("finalRenderSecondOutputSampleCount=%1")
             .arg(secondStats.sampleCount)
      << QString("finalRenderSecondOutputRedPixels=%1")
             .arg(secondStats.redPixels)
      << QString("finalRenderSecondOutputGreenPixels=%1")
             .arg(secondStats.greenPixels)
      << QString("finalRenderSecondOutputBluePixels=%1")
             .arg(secondStats.bluePixels)
      << QString("finalRenderOutputProbe=%1")
             .arg(outputOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("finalRenderProbe=%1")
             .arg(renderOk ? QStringLiteral("ok") : QStringLiteral("error"));
  return details;
}

static QStringList gui_smoke_viewer_zoom_pan_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("viewerTransformProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("viewerTransformProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_zoom_pan_raster");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("viewerTransformProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("viewerTransformProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before      = gui_smoke_grab_viewer_frame(viewer);
  const TAffine beforeAff  = viewer->getViewMatrix();
  const double beforeScale = std::sqrt(std::abs(beforeAff.det()));
  const int devPixRatio    = std::max(1, viewer->getDevPixRatio());
  const double zoomFactor  = 1.35;
  const QPointF panDelta(48.0 * devPixRatio, -32.0 * devPixRatio);

  viewer->setViewMatrix(TTranslation(panDelta.x(), -panDelta.y()) *
                            TScale(zoomFactor) * beforeAff,
                        SceneViewer::SCENE_VIEWMODE);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const TAffine afterAff  = viewer->getViewMatrix();
  const double afterScale = std::sqrt(std::abs(afterAff.det()));
  const bool transformOk  = afterAff != beforeAff && afterScale > beforeScale &&
                           std::abs(afterAff.a13 - beforeAff.a13) > 0.5 &&
                           std::abs(afterAff.a23 - beforeAff.a23) > 0.5;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("viewerTransformContent=raster")
          << QString("viewerZoomFactor=%1").arg(zoomFactor, 0, 'f', 2)
          << QString("viewerPanLogicalX=%1").arg(panDelta.x() / devPixRatio)
          << QString("viewerPanLogicalY=%1").arg(panDelta.y() / devPixRatio)
          << QString("viewerPanDeviceX=%1").arg(panDelta.x())
          << QString("viewerPanDeviceY=%1").arg(panDelta.y())
          << QString("viewerScaleBefore=%1").arg(beforeScale, 0, 'f', 4)
          << QString("viewerScaleAfter=%1").arg(afterScale, 0, 'f', 4)
          << QString("viewerTransformProbe=%1")
                 .arg(transformOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-zoom-pan"),
      QStringLiteral("raster-zoom-pan"), transformOk);

  return details;
}

static QStringList gui_smoke_viewer_onion_skin_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* onionLevelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_back");
  TXshLevel* currentLevelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_current");
  TXshSimpleLevel* onionLevel =
      onionLevelXl ? onionLevelXl->getSimpleLevel() : nullptr;
  TXshSimpleLevel* currentLevel =
      currentLevelXl ? currentLevelXl->getSimpleLevel() : nullptr;
  if (!onionLevel || !currentLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId onionFid(1);
  const TFrameId currentFid(2);
  gui_smoke_add_transparent_raster_probe_frame(
      onionLevel, onionFid, TPixel32(24, 192, 232, 255), 34, 34, 122, 122);
  gui_smoke_add_transparent_raster_probe_frame(
      currentLevel, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(onionLevel, onionFid)) ||
      !xsheet->setCell(1, 0, TXshCell(currentLevel, currentFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(1);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(currentLevelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  mask.clear();
  mask.enable(true);
  mask.setIsWholeScene(true);
  mask.setMos(-1, true);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  const bool onionOk = activeMask.isEnabled() && activeMask.isMos(-1) &&
                       activeMask.getMosCount() == 1;
  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell onionCell   = xsheet->getCell(0, 0);
  const TXshCell currentCell = xsheet->getCell(1, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=raster")
      << QStringLiteral("onionSkinCurrentRow=1")
      << QStringLiteral("onionSkinBackOffset=-1")
      << QString("onionSkinWholeScene=%1")
             .arg(activeMask.isWholeScene() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFrameMode=%1")
             .arg(TApp::instance()->getCurrentFrame()->isEditingScene()
                      ? QStringLiteral("scene")
                      : QStringLiteral("level"))
      << QString("onionSkinCurrentColumn=%1").arg(currentColumn)
      << QString("onionSkinCurrentCellEmpty=%1")
             .arg(currentCell.isEmpty() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
      << QString("onionSkinBackCellEmpty=%1")
             .arg(onionCell.isEmpty() ? QStringLiteral("true")
                                      : QStringLiteral("false"))
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin"),
      QStringLiteral("raster-onion-skin"), onionOk, 1);

  return details;
}

static QStringList gui_smoke_viewer_camera_overlay_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("cameraOverlayProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("cameraOverlayProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool overlayDisabled = gui_smoke_set_view_camera_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const bool overlayEnabled = gui_smoke_set_view_camera_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  const bool overlayOk = overlayDisabled && overlayEnabled;
  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("cameraOverlayDisabled=%1")
                 .arg(overlayDisabled ? QStringLiteral("true")
                                      : QStringLiteral("false"))
          << QString("cameraOverlayEnabled=%1")
                 .arg(overlayEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
          << QString("cameraOverlayProbe=%1")
                 .arg(overlayOk ? QStringLiteral("ok")
                                : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-camera-overlay"),
      QStringLiteral("camera-overlay"), overlayOk);

  return details;
}

static QStringList gui_smoke_viewer_safe_area_field_guide_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("safeAreaProbe=no-viewer")
            << QStringLiteral("fieldGuideProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("safeAreaProbe=scene-folder-error")
            << QStringLiteral("fieldGuideProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const bool safeAreaEnabled   = gui_smoke_set_safe_area_overlay(true);
  const bool fieldGuideEnabled = gui_smoke_set_field_guide_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const bool safeAreaOk   = safeAreaDisabled && safeAreaEnabled;
  const bool fieldGuideOk = fieldGuideDisabled && fieldGuideEnabled;
  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("cameraOverlayDisabled=%1")
                 .arg(cameraDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
          << QString("safeAreaDisabled=%1")
                 .arg(safeAreaDisabled ? QStringLiteral("true")
                                       : QStringLiteral("false"))
          << QString("safeAreaEnabled=%1")
                 .arg(safeAreaEnabled ? QStringLiteral("true")
                                      : QStringLiteral("false"))
          << QString("fieldGuideDisabled=%1")
                 .arg(fieldGuideDisabled ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("fieldGuideEnabled=%1")
                 .arg(fieldGuideEnabled ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("safeAreaProbe=%1")
                 .arg(safeAreaOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"))
          << QString("fieldGuideProbe=%1")
                 .arg(fieldGuideOk ? QStringLiteral("ok")
                                   : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-safe-area-field-guide"),
      QStringLiteral("safe-area-field-guide"), safeAreaOk && fieldGuideOk, 0,
      1);

  return details;
}

static QStringList gui_smoke_viewer_safe_area_presets_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("safeAreaPresetProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("safeAreaPresetProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const QString originalSafeAreaName = QString::fromStdString(EnvSafeAreaName);
  const QString defaultSafeAreaName  = QStringLiteral("PR_safe");
  const QString customSafeAreaName   = QStringLiteral("150MT_FR_PR_safe");

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool rulerDisabled      = gui_smoke_set_view_ruler_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage disabledFrame = gui_smoke_grab_viewer_frame(viewer);

  EnvSafeAreaName = defaultSafeAreaName.toStdString();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  const bool safeAreaEnabled = gui_smoke_set_safe_area_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage defaultFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats defaultStats =
      gui_smoke_analyze_viewer_frame(defaultFrame, disabledFrame);
  const bool defaultPresetStored =
      QString::fromStdString(EnvSafeAreaName) == defaultSafeAreaName;

  EnvSafeAreaName = customSafeAreaName.toStdString();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage customFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats customStats =
      gui_smoke_analyze_viewer_frame(customFrame, disabledFrame);
  const GuiSmokeImageStats changedPresetStats =
      gui_smoke_analyze_viewer_frame(customFrame, defaultFrame);
  const bool customPresetStored =
      QString::fromStdString(EnvSafeAreaName) == customSafeAreaName;

  QString disabledCapturePath;
  const bool disabledCaptureSaved = gui_smoke_save_capture(
      disabledFrame, QStringLiteral("viewer-safe-area-presets-disabled.png"),
      &disabledCapturePath);

  const bool defaultVisible =
      defaultStats.redPixels > 1000 && defaultStats.changedRedPixels > 1000;
  const bool customVisible =
      customStats.redPixels > 100 && customStats.greenPixels > 100 &&
      customStats.bluePixels > 100 && customStats.changedGreenPixels > 100 &&
      customStats.changedBluePixels > 100;
  const bool presetsDiffer = changedPresetStats.changedPixels > 1000 &&
                             changedPresetStats.changedGreenPixels > 100 &&
                             changedPresetStats.changedBluePixels > 100;
  const bool presetsOk = cameraDisabled && fieldGuideDisabled &&
                         rulerDisabled && guideDisabled && safeAreaDisabled &&
                         safeAreaEnabled && defaultPresetStored &&
                         customPresetStored && defaultVisible &&
                         customVisible && presetsDiffer && disabledCaptureSaved;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("viewRulerDisabled=%1")
             .arg(rulerDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("viewGuideDisabled=%1")
             .arg(guideDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("safeAreaEnabled=%1")
             .arg(safeAreaEnabled ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("safeAreaNameOriginal=%1")
             .arg(gui_smoke_status_value(originalSafeAreaName))
      << QString("safeAreaNameDefault=%1")
             .arg(gui_smoke_status_value(defaultSafeAreaName))
      << QString("safeAreaNameCustom=%1")
             .arg(gui_smoke_status_value(customSafeAreaName))
      << QString("storedSafeAreaName=%1")
             .arg(gui_smoke_status_value(
                 QString::fromStdString(EnvSafeAreaName)))
      << QString("defaultSafeAreaRedPixels=%1").arg(defaultStats.redPixels)
      << QString("defaultSafeAreaChangedRedPixels=%1")
             .arg(defaultStats.changedRedPixels)
      << QString("customSafeAreaRedPixels=%1").arg(customStats.redPixels)
      << QString("customSafeAreaGreenPixels=%1").arg(customStats.greenPixels)
      << QString("customSafeAreaBluePixels=%1").arg(customStats.bluePixels)
      << QString("customSafeAreaChangedRedPixels=%1")
             .arg(customStats.changedRedPixels)
      << QString("customSafeAreaChangedGreenPixels=%1")
             .arg(customStats.changedGreenPixels)
      << QString("customSafeAreaChangedBluePixels=%1")
             .arg(customStats.changedBluePixels)
      << QString("safeAreaPresetChangedPixels=%1")
             .arg(changedPresetStats.changedPixels)
      << QString("safeAreaPresetChangedRedPixels=%1")
             .arg(changedPresetStats.changedRedPixels)
      << QString("safeAreaPresetChangedGreenPixels=%1")
             .arg(changedPresetStats.changedGreenPixels)
      << QString("safeAreaPresetChangedBluePixels=%1")
             .arg(changedPresetStats.changedBluePixels)
      << QString("safeAreaPresetDisabledCaptureSaved=%1")
             .arg(disabledCaptureSaved ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("safeAreaPresetDisabledCapturePath=%1")
             .arg(gui_smoke_status_value(disabledCapturePath))
      << QString("safeAreaPresetProbe=%1")
             .arg(presetsOk ? QStringLiteral("ok") : QStringLiteral("error"));

  details += gui_smoke_viewer_capture_details(
      viewer, defaultFrame, QStringLiteral("viewer-safe-area-presets"),
      QStringLiteral("safe-area-presets"), presetsOk, 0, 0, 1);

  EnvSafeAreaName = originalSafeAreaName.toStdString();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  return details;
}

static bool gui_smoke_write_custom_safe_area_file(const QString& path,
                                                  const QString& presetName) {
  QFileInfo info(path);
  QDir().mkpath(info.absolutePath());

  QSettings settings(path, QSettings::IniFormat);
  settings.clear();
  settings.beginGroup(QStringLiteral("SafeArea0"));
  settings.setValue(QStringLiteral("name"), presetName);
  settings.beginGroup(QStringLiteral("area"));

  QList<QVariant> redArea;
  redArea << 92.0 << 80.0 << 255 << 0 << 0;
  settings.setValue(QStringLiteral("0"), redArea);

  QList<QVariant> greenArea;
  greenArea << 76.0 << 66.0 << 0 << 255 << 0;
  settings.setValue(QStringLiteral("1"), greenArea);

  QList<QVariant> blueArea;
  blueArea << 58.0 << 50.0 << 0 << 0 << 255;
  settings.setValue(QStringLiteral("2"), blueArea);

  settings.endGroup();
  settings.endGroup();
  settings.sync();
  return settings.status() == QSettings::NoError && QFileInfo(path).exists();
}

static bool gui_smoke_restore_safe_area_file(
    const QString& path, bool hadOriginal, const QByteArray& originalContent) {
  if (!hadOriginal) return QFile::remove(path) || !QFileInfo(path).exists();

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  const bool ok = file.write(originalContent) == originalContent.size();
  file.close();
  return ok;
}

static QStringList gui_smoke_viewer_safe_area_custom_file_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("safeAreaCustomFileProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("safeAreaCustomFileProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const QString originalSafeAreaName = QString::fromStdString(EnvSafeAreaName);
  const QString customSafeAreaName =
      QStringLiteral("codex_custom_safe_area_probe");
  const QString safeAreaFilePath =
      toQString(TEnv::getConfigDir() + TFilePath("safearea.ini"));

  QFile originalFile(safeAreaFilePath);
  const bool hadOriginal = originalFile.exists();
  QByteArray originalContent;
  if (hadOriginal && originalFile.open(QIODevice::ReadOnly)) {
    originalContent = originalFile.readAll();
    originalFile.close();
  }

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool rulerDisabled      = gui_smoke_set_view_ruler_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage disabledFrame = gui_smoke_grab_viewer_frame(viewer);

  const bool customFileWritten = gui_smoke_write_custom_safe_area_file(
      safeAreaFilePath, customSafeAreaName);
  EnvSafeAreaName = customSafeAreaName.toStdString();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  const bool safeAreaEnabled = gui_smoke_set_safe_area_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage customFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats customStats =
      gui_smoke_analyze_viewer_frame(customFrame, disabledFrame);
  const bool customPresetStored =
      QString::fromStdString(EnvSafeAreaName) == customSafeAreaName;

  QString disabledCapturePath;
  const bool disabledCaptureSaved = gui_smoke_save_capture(
      disabledFrame,
      QStringLiteral("viewer-safe-area-custom-file-disabled.png"),
      &disabledCapturePath);

  const bool customVisible =
      customStats.redPixels > 100 && customStats.greenPixels > 100 &&
      customStats.bluePixels > 100 && customStats.changedRedPixels > 100 &&
      customStats.changedGreenPixels > 100 &&
      customStats.changedBluePixels > 100;
  const bool customFileOk =
      cameraDisabled && fieldGuideDisabled && rulerDisabled && guideDisabled &&
      safeAreaDisabled && safeAreaEnabled && customFileWritten &&
      customPresetStored && customVisible && disabledCaptureSaved;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("viewRulerDisabled=%1")
             .arg(rulerDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("viewGuideDisabled=%1")
             .arg(guideDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("safeAreaEnabled=%1")
             .arg(safeAreaEnabled ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("safeAreaNameOriginal=%1")
             .arg(gui_smoke_status_value(originalSafeAreaName))
      << QString("customSafeAreaName=%1")
             .arg(gui_smoke_status_value(customSafeAreaName))
      << QString("storedSafeAreaName=%1")
             .arg(gui_smoke_status_value(
                 QString::fromStdString(EnvSafeAreaName)))
      << QString("customSafeAreaFilePath=%1")
             .arg(gui_smoke_status_value(safeAreaFilePath))
      << QString("customSafeAreaFileWritten=%1")
             .arg(customFileWritten ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("customSafeAreaRedPixels=%1").arg(customStats.redPixels)
      << QString("customSafeAreaGreenPixels=%1").arg(customStats.greenPixels)
      << QString("customSafeAreaBluePixels=%1").arg(customStats.bluePixels)
      << QString("customSafeAreaChangedRedPixels=%1")
             .arg(customStats.changedRedPixels)
      << QString("customSafeAreaChangedGreenPixels=%1")
             .arg(customStats.changedGreenPixels)
      << QString("customSafeAreaChangedBluePixels=%1")
             .arg(customStats.changedBluePixels)
      << QString("safeAreaCustomFileDisabledCaptureSaved=%1")
             .arg(disabledCaptureSaved ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("safeAreaCustomFileDisabledCapturePath=%1")
             .arg(gui_smoke_status_value(disabledCapturePath))
      << QString("safeAreaCustomFileProbe=%1")
             .arg(customFileOk ? QStringLiteral("ok")
                               : QStringLiteral("error"));

  details += gui_smoke_viewer_capture_details(
      viewer, disabledFrame, QStringLiteral("viewer-safe-area-custom-file"),
      QStringLiteral("safe-area-custom-file"), customFileOk, 0, 0, 1);

  EnvSafeAreaName                 = originalSafeAreaName.toStdString();
  const bool safeAreaFileRestored = gui_smoke_restore_safe_area_file(
      safeAreaFilePath, hadOriginal, originalContent);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  details << QString("safeAreaCustomFileRestored=%1")
                 .arg(safeAreaFileRestored ? QStringLiteral("true")
                                           : QStringLiteral("false"));

  return details;
}

static QStringList gui_smoke_viewer_field_guide_settings_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("fieldGuideSettingsProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("fieldGuideSettingsProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  TSceneProperties* properties = scene->getProperties();
  const int initialSize        = properties->getFieldGuideSize();
  const double initialAspect   = properties->getFieldGuideAspectRatio();
  const int firstSize          = 10;
  const double firstAspect     = 1.3333;
  const int secondSize         = 24;
  const double secondAspect    = 2.4;

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool rulerDisabled      = gui_smoke_set_view_ruler_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage disabledFrame = gui_smoke_grab_viewer_frame(viewer);

  properties->setFieldGuideSize(firstSize);
  properties->setFieldGuideAspectRatio(firstAspect);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  const bool fieldGuideEnabled = gui_smoke_set_field_guide_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage firstFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats firstStats =
      gui_smoke_analyze_viewer_frame(firstFrame, disabledFrame);
  const bool firstStored =
      properties->getFieldGuideSize() == firstSize &&
      std::abs(properties->getFieldGuideAspectRatio() - firstAspect) < 0.001;

  properties->setFieldGuideSize(secondSize);
  properties->setFieldGuideAspectRatio(secondAspect);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage secondFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats secondStats =
      gui_smoke_analyze_viewer_frame(secondFrame, disabledFrame);
  const GuiSmokeImageStats changedSettingsStats =
      gui_smoke_analyze_viewer_frame(secondFrame, firstFrame);

  QString disabledCapturePath;
  QString firstCapturePath;
  const bool disabledCaptureSaved = gui_smoke_save_capture(
      disabledFrame, QStringLiteral("viewer-field-guide-settings-disabled.png"),
      &disabledCapturePath);
  const bool firstCaptureSaved = gui_smoke_save_capture(
      firstFrame, QStringLiteral("viewer-field-guide-settings-first.png"),
      &firstCapturePath);

  const bool secondStored =
      properties->getFieldGuideSize() == secondSize &&
      std::abs(properties->getFieldGuideAspectRatio() - secondAspect) < 0.001;
  const bool firstVisible =
      firstStats.changedGrayPixels > 1000 && firstStats.grayPixels > 1000;
  const bool secondVisible =
      secondStats.changedGrayPixels > 1000 && secondStats.grayPixels > 1000;
  const bool settingsDiffer = changedSettingsStats.changedGrayPixels > 1000;
  const bool settingsOk = cameraDisabled && safeAreaDisabled && rulerDisabled &&
                          guideDisabled && fieldGuideDisabled &&
                          fieldGuideEnabled && firstStored && secondStored &&
                          firstVisible && secondVisible && settingsDiffer &&
                          disabledCaptureSaved && firstCaptureSaved;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("viewRulerDisabled=%1")
             .arg(rulerDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("viewGuideDisabled=%1")
             .arg(guideDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("fieldGuideEnabled=%1")
             .arg(fieldGuideEnabled ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("initialFieldGuideSize=%1").arg(initialSize)
      << QString("initialFieldGuideAspectRatio=%1")
             .arg(initialAspect, 0, 'f', 4)
      << QString("firstFieldGuideSize=%1").arg(firstSize)
      << QString("firstFieldGuideAspectRatio=%1").arg(firstAspect, 0, 'f', 4)
      << QString("secondFieldGuideSize=%1").arg(secondSize)
      << QString("secondFieldGuideAspectRatio=%1").arg(secondAspect, 0, 'f', 4)
      << QString("storedFieldGuideSize=%1").arg(properties->getFieldGuideSize())
      << QString("storedFieldGuideAspectRatio=%1")
             .arg(properties->getFieldGuideAspectRatio(), 0, 'f', 4)
      << QString("firstFieldGuideGrayPixels=%1").arg(firstStats.grayPixels)
      << QString("firstFieldGuideChangedGrayPixels=%1")
             .arg(firstStats.changedGrayPixels)
      << QString("secondFieldGuideGrayPixels=%1").arg(secondStats.grayPixels)
      << QString("secondFieldGuideChangedGrayPixels=%1")
             .arg(secondStats.changedGrayPixels)
      << QString("fieldGuideSettingsChangedPixels=%1")
             .arg(changedSettingsStats.changedPixels)
      << QString("fieldGuideSettingsChangedGrayPixels=%1")
             .arg(changedSettingsStats.changedGrayPixels)
      << QString("fieldGuideSettingsDisabledCaptureSaved=%1")
             .arg(disabledCaptureSaved ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("fieldGuideSettingsDisabledCapturePath=%1")
             .arg(gui_smoke_status_value(disabledCapturePath))
      << QString("fieldGuideSettingsFirstCaptureSaved=%1")
             .arg(firstCaptureSaved ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("fieldGuideSettingsFirstCapturePath=%1")
             .arg(gui_smoke_status_value(firstCapturePath))
      << QString("fieldGuideSettingsProbe=%1")
             .arg(settingsOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, disabledFrame, QStringLiteral("viewer-field-guide-settings"),
      QStringLiteral("field-guide-settings"), settingsOk, 0, 1000, 0);

  return details;
}

static int gui_smoke_visible_ruler_count() {
  QMainWindow* mainWindow = TApp::instance()->getMainWindow();
  if (!mainWindow) return 0;

  int visibleCount           = 0;
  const QList<Ruler*> rulers = mainWindow->findChildren<Ruler*>();
  for (Ruler* ruler : rulers) {
    if (ruler && ruler->isVisible()) ++visibleCount;
  }
  return visibleCount;
}

static int gui_smoke_ruler_count() {
  QMainWindow* mainWindow = TApp::instance()->getMainWindow();
  return mainWindow ? mainWindow->findChildren<Ruler*>().size() : 0;
}

static bool gui_smoke_send_widget_mouse_event(QWidget* widget,
                                              QEvent::Type type,
                                              const QPointF& localPos,
                                              Qt::MouseButton button,
                                              Qt::MouseButtons buttons,
                                              Qt::KeyboardModifiers modifiers);
static bool gui_smoke_near(double a, double b, double epsilon);
static QPointF gui_smoke_world_to_viewer_device(SceneViewer* viewer,
                                                const TPointD& worldPos);

static void gui_smoke_resolve_visible_rulers(Ruler** horizontalRuler,
                                             Ruler** verticalRuler) {
  if (horizontalRuler) *horizontalRuler = nullptr;
  if (verticalRuler) *verticalRuler = nullptr;

  QMainWindow* mainWindow = TApp::instance()->getMainWindow();
  if (!mainWindow) return;

  const QList<Ruler*> rulers = mainWindow->findChildren<Ruler*>();
  for (Ruler* ruler : rulers) {
    if (!ruler || !ruler->isVisible()) continue;
    if (horizontalRuler && !*horizontalRuler && ruler->height() <= 20 &&
        ruler->width() > ruler->height()) {
      *horizontalRuler = ruler;
    } else if (verticalRuler && !*verticalRuler && ruler->width() <= 20 &&
               ruler->height() > ruler->width()) {
      *verticalRuler = ruler;
    }
  }
}

static XsheetViewer* gui_smoke_resolve_xsheet_viewer() {
  QMainWindow* mainWindow = TApp::instance()->getMainWindow();
  if (!mainWindow) return nullptr;

  const QList<XsheetViewer*> viewers =
      mainWindow->findChildren<XsheetViewer*>();
  for (XsheetViewer* viewer : viewers) {
    if (viewer && viewer->isVisible() && viewer->width() > 0 &&
        viewer->height() > 0) {
      return viewer;
    }
  }
  return viewers.isEmpty() ? nullptr : viewers.front();
}

static XsheetGUI::RowArea* gui_smoke_resolve_xsheet_row_area(
    XsheetViewer* viewer) {
  if (!viewer) return nullptr;
  return viewer->findChild<XsheetGUI::RowArea*>();
}

static QPoint gui_smoke_xsheet_row_area_point(XsheetViewer* viewer, int row,
                                              PredefinedRect which,
                                              bool useHalfFrameAdjustment) {
  if (!viewer || !viewer->orientation()) return QPoint();

  QPoint topLeft = viewer->positionToXY(CellPosition(row, -1));
  if (!viewer->orientation()->isVerticalTimeline())
    topLeft.setY(0);
  else
    topLeft.setX(0);

  const QPoint frameAdjustment = viewer->getFrameZoomAdjustment();
  QRect rect                   = viewer->orientation()->rect(which);
  if (useHalfFrameAdjustment)
    rect.translate(-frameAdjustment / 2);
  else
    rect.adjust(0, 0, -frameAdjustment.x(), -frameAdjustment.y());

  return topLeft + rect.center();
}

static bool gui_smoke_replay_row_area_click(QWidget* rowArea,
                                            const QPointF& point) {
  if (!rowArea || !rowArea->rect().contains(point.toPoint())) return false;
  rowArea->setFocus(Qt::OtherFocusReason);
  bool delivered = gui_smoke_send_widget_mouse_event(
      rowArea, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton,
      Qt::NoModifier);
  delivered = gui_smoke_send_widget_mouse_event(
                  rowArea, QEvent::MouseButtonRelease, point, Qt::LeftButton,
                  Qt::NoButton, Qt::NoModifier) &&
              delivered;
  return delivered;
}

static bool gui_smoke_replay_row_area_double_click(QWidget* rowArea,
                                                   const QPointF& point) {
  if (!rowArea || !rowArea->rect().contains(point.toPoint())) return false;
  rowArea->setFocus(Qt::OtherFocusReason);
  bool delivered = gui_smoke_send_widget_mouse_event(
      rowArea, QEvent::MouseButtonDblClick, point, Qt::LeftButton,
      Qt::LeftButton, Qt::NoModifier);
  delivered = gui_smoke_send_widget_mouse_event(
                  rowArea, QEvent::MouseButtonRelease, point, Qt::LeftButton,
                  Qt::NoButton, Qt::NoModifier) &&
              delivered;
  return delivered;
}

static bool gui_smoke_replay_row_area_drag(QWidget* rowArea,
                                           const std::vector<QPointF>& points) {
  if (!rowArea || points.size() < 2) return false;
  for (const QPointF& point : points) {
    if (!rowArea->rect().contains(point.toPoint())) return false;
  }

  rowArea->setFocus(Qt::OtherFocusReason);
  bool delivered = gui_smoke_send_widget_mouse_event(
      rowArea, QEvent::MouseButtonPress, points.front(), Qt::LeftButton,
      Qt::LeftButton, Qt::NoModifier);
  for (size_t i = 1; i < points.size(); ++i) {
    delivered = gui_smoke_send_widget_mouse_event(
                    rowArea, QEvent::MouseMove, points[i], Qt::NoButton,
                    Qt::LeftButton, Qt::NoModifier) &&
                delivered;
  }
  delivered = gui_smoke_send_widget_mouse_event(
                  rowArea, QEvent::MouseButtonRelease, points.back(),
                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier) &&
              delivered;
  return delivered;
}

static bool gui_smoke_set_shift_trace_enabled(bool enabled) {
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  QAction* action = CommandManager::instance()->getAction("MI_ShiftTrace");
  if (!onionHandle || !action) return false;

  action->setChecked(enabled);
  if (enabled) OnioniSkinMaskGUI::resetShiftTraceFrameOffset();

  OnionSkinMask mask = onionHandle->getOnionSkinMask();
  mask.setShiftTraceStatus(enabled ? OnionSkinMask::ENABLED
                                   : OnionSkinMask::DISABLED);
  if (!enabled) {
    mask.setShiftTraceGhostFrameOffset(0, 0);
    mask.setShiftTraceGhostFrameOffset(1, 0);
  }
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  return action->isChecked() == enabled;
}

static double gui_smoke_clamp_widget_coordinate(double value, int extent) {
  if (extent <= 1) return 0.0;
  const double minValue = 1.0;
  const double maxValue = std::max(minValue, static_cast<double>(extent - 2));
  return std::min(std::max(value, minValue), maxValue);
}

static QPointF gui_smoke_ruler_drag_point(Ruler* ruler, bool vertical,
                                          double offset) {
  if (!ruler) return QPointF();
  if (vertical) {
    return QPointF(
        gui_smoke_clamp_widget_coordinate(ruler->width() * 0.5, ruler->width()),
        gui_smoke_clamp_widget_coordinate(ruler->height() * 0.5 + offset,
                                          ruler->height()));
  }
  return QPointF(gui_smoke_clamp_widget_coordinate(
                     ruler->width() * 0.5 + offset, ruler->width()),
                 gui_smoke_clamp_widget_coordinate(ruler->height() * 0.5,
                                                   ruler->height()));
}

static bool gui_smoke_replay_ruler_drag(Ruler* ruler, const QPointF& start,
                                        const QPointF& end,
                                        bool allowOutsideEnd = false) {
  if (!ruler || !ruler->rect().contains(start.toPoint()) ||
      (!allowOutsideEnd && !ruler->rect().contains(end.toPoint()))) {
    return false;
  }

  bool delivered = gui_smoke_send_widget_mouse_event(
      ruler, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton,
      Qt::NoModifier);
  delivered = gui_smoke_send_widget_mouse_event(ruler, QEvent::MouseMove, end,
                                                Qt::NoButton, Qt::LeftButton,
                                                Qt::NoModifier) &&
              delivered;
  delivered = gui_smoke_send_widget_mouse_event(
                  ruler, QEvent::MouseButtonRelease, end, Qt::LeftButton,
                  Qt::NoButton, Qt::NoModifier) &&
              delivered;
  return delivered;
}

static bool gui_smoke_delete_ruler_guide(Ruler* ruler, const QPointF& point) {
  if (!ruler || !ruler->rect().contains(point.toPoint())) return false;

  bool delivered = gui_smoke_send_widget_mouse_event(
      ruler, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton,
      Qt::NoModifier);
  delivered = gui_smoke_send_widget_mouse_event(
                  ruler, QEvent::MouseButtonRelease, point, Qt::RightButton,
                  Qt::NoButton, Qt::NoModifier) &&
              delivered;
  return delivered;
}

static QStringList gui_smoke_viewer_onion_skin_row_area_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinUiProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinUiProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_row_area");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinUiProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId backFid(1);
  const TFrameId currentFid(2);
  const TFrameId frontFid(3);
  gui_smoke_add_transparent_raster_probe_frame(
      level, backFid, TPixel32(24, 192, 232, 255), 34, 34, 122, 122);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);
  gui_smoke_add_transparent_raster_probe_frame(
      level, frontFid, TPixel32(232, 210, 24, 255), 234, 34, 322, 122);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, backFid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, frontFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinUiProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(1);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    xsheetViewer->scrollTo(1, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=row-area-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinUiProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-rowarea"),
        QStringLiteral("row-area-onion-skin"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore, QStringLiteral("viewer-onion-skin-rowarea-xsheet-before.png"),
      &rowBeforePath);

  const QPoint mosPoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 0, PredefinedRect::ONION_DOT_AREA, false);
  const QPoint fosPoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 2, PredefinedRect::ONION_FIXED_DOT_AREA, false);
  const QPoint togglePoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 1, PredefinedRect::ONION, true);
  const bool pointsInRowArea = rowArea->rect().contains(mosPoint) &&
                               rowArea->rect().contains(fosPoint) &&
                               rowArea->rect().contains(togglePoint);

  const bool mosDelivered =
      gui_smoke_replay_row_area_click(rowArea, QPointF(mosPoint));
  const bool fosDelivered =
      gui_smoke_replay_row_area_click(rowArea, QPointF(fosPoint));
  gui_smoke_pump_events(100);

  const OnionSkinMask afterMarkersMask = onionHandle->getOnionSkinMask();
  const bool markersOk =
      afterMarkersMask.isEnabled() && afterMarkersMask.isMos(-1) &&
      afterMarkersMask.isFos(2) && afterMarkersMask.getMosCount() == 1 &&
      afterMarkersMask.getFosCount() == 1;

  const bool disableDelivered =
      gui_smoke_replay_row_area_double_click(rowArea, QPointF(togglePoint));
  gui_smoke_pump_events(100);
  const OnionSkinMask afterDisableMask = onionHandle->getOnionSkinMask();
  const bool disabledOk                = !afterDisableMask.isEnabled() &&
                          afterDisableMask.isMos(-1) &&
                          afterDisableMask.isFos(2);

  const bool enableDelivered =
      gui_smoke_replay_row_area_double_click(rowArea, QPointF(togglePoint));
  gui_smoke_pump_events(120);
  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  const bool enabledOk = activeMask.isEnabled() && activeMask.isMos(-1) &&
                         activeMask.isFos(2) && activeMask.getMosCount() == 1 &&
                         activeMask.getFosCount() == 1;

  viewer->GLInvalidateAll();
  if (xsheetViewer) xsheetViewer->updateRows();
  gui_smoke_pump_events(150);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter, QStringLiteral("viewer-onion-skin-rowarea-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const bool rowAreaOk    = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0;

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell backCell    = xsheet->getCell(0, 0);
  const TXshCell currentCell = xsheet->getCell(1, 0);
  const TXshCell frontCell   = xsheet->getCell(2, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);

  const bool onionOk =
      markersOk && disabledOk && enabledOk && stageCounts.m_onionPlayers >= 2 &&
      stageCounts.m_backOnionPlayers >= 1 &&
      stageCounts.m_frontOnionPlayers >= 1 && currentFrame == 1 &&
      currentColumn == 0 && !backCell.isEmpty() && !currentCell.isEmpty() &&
      !frontCell.isEmpty();
  const bool uiOk = onionOk && pointsInRowArea && mosDelivered &&
                    fosDelivered && disableDelivered && enableDelivered &&
                    rowAreaOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=row-area-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetViewerVisible=%1")
             .arg(xsheetViewer->isVisible() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaVisible=%1")
             .arg(rowArea->isVisible() ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1").arg(rowStats.distinctColors)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QStringLiteral("onionSkinBackOffset=-1")
      << QStringLiteral("onionSkinFixedRow=2")
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("onionSkinEnabledAfterMarkers=%1")
             .arg(afterMarkersMask.isEnabled() ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("onionSkinEnabledAfterDisable=%1")
             .arg(afterDisableMask.isEnabled() ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("onionSkinEnabledAfterEnable=%1")
             .arg(activeMask.isEnabled() ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("rowAreaPointsInBounds=%1")
             .arg(pointsInRowArea ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("rowAreaMosPoint=%1,%2").arg(mosPoint.x()).arg(mosPoint.y())
      << QString("rowAreaFosPoint=%1,%2").arg(fosPoint.x()).arg(fosPoint.y())
      << QString("rowAreaTogglePoint=%1,%2")
             .arg(togglePoint.x())
             .arg(togglePoint.y())
      << QString("rowAreaMosEventDelivered=%1")
             .arg(mosDelivered ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("rowAreaFosEventDelivered=%1")
             .arg(fosDelivered ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("rowAreaDisableEventDelivered=%1")
             .arg(disableDelivered ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("rowAreaEnableEventDelivered=%1")
             .arg(enableDelivered ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinUiProbe=%1")
             .arg(uiOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-rowarea"),
      QStringLiteral("row-area-onion-skin"), uiOk, 1);

  return details;
}

struct GuiSmokeOnionRowAreaOrientationPass {
  QString label;
  QString orientationName;
  bool verticalTimeline = false;
  bool pointsInRowArea  = false;
  bool mosDelivered     = false;
  bool fosDelivered     = false;
  bool disableDelivered = false;
  bool enableDelivered  = false;
  bool markersOk        = false;
  bool disabledOk       = false;
  bool enabledOk        = false;
  bool rowBeforeSaved   = false;
  bool rowAfterSaved    = false;
  bool rowHighDpiOk     = false;
  bool rowAreaOk        = false;
  bool uiOk             = false;
  int rowImageWidth     = 0;
  int rowImageHeight    = 0;
  QPoint mosPoint;
  QPoint fosPoint;
  QPoint togglePoint;
  QString rowBeforePath;
  QString rowAfterPath;
  GuiSmokeWidgetImageStats rowStats;
  OnionSkinMask afterMarkersMask;
  OnionSkinMask afterDisableMask;
  OnionSkinMask activeMask;
};

static GuiSmokeOnionRowAreaOrientationPass
gui_smoke_run_onion_row_area_orientation_pass(
    const QString& label, SceneViewer* viewer, XsheetViewer* xsheetViewer,
    XsheetGUI::RowArea* rowArea, TOnionSkinMaskHandle* onionHandle) {
  GuiSmokeOnionRowAreaOrientationPass result;
  result.label = label;
  if (!viewer || !xsheetViewer || !rowArea || !onionHandle ||
      !xsheetViewer->orientation()) {
    return result;
  }

  result.orientationName  = xsheetViewer->orientation()->name();
  result.verticalTimeline = xsheetViewer->orientation()->isVerticalTimeline();

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  viewer->GLInvalidateAll();
  xsheetViewer->updateAreeSize();
  xsheetViewer->updateRows();
  rowArea->update();
  gui_smoke_pump_events(150);

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  result.rowBeforeSaved  = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-orientations-%1-xsheet-before.png")
          .arg(label),
      &result.rowBeforePath);

  result.mosPoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 0, PredefinedRect::ONION_DOT_AREA, false);
  result.fosPoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 2, PredefinedRect::ONION_FIXED_DOT_AREA, false);
  result.togglePoint = gui_smoke_xsheet_row_area_point(
      xsheetViewer, 1, PredefinedRect::ONION, true);
  result.pointsInRowArea = rowArea->rect().contains(result.mosPoint) &&
                           rowArea->rect().contains(result.fosPoint) &&
                           rowArea->rect().contains(result.togglePoint);

  result.mosDelivered =
      gui_smoke_replay_row_area_click(rowArea, QPointF(result.mosPoint));
  result.fosDelivered =
      gui_smoke_replay_row_area_click(rowArea, QPointF(result.fosPoint));
  gui_smoke_pump_events(100);

  result.afterMarkersMask = onionHandle->getOnionSkinMask();
  result.markersOk        = result.afterMarkersMask.isEnabled() &&
                     result.afterMarkersMask.isMos(-1) &&
                     result.afterMarkersMask.isFos(2) &&
                     result.afterMarkersMask.getMosCount() == 1 &&
                     result.afterMarkersMask.getFosCount() == 1;

  result.disableDelivered = gui_smoke_replay_row_area_double_click(
      rowArea, QPointF(result.togglePoint));
  gui_smoke_pump_events(100);
  result.afterDisableMask = onionHandle->getOnionSkinMask();
  result.disabledOk       = !result.afterDisableMask.isEnabled() &&
                      result.afterDisableMask.isMos(-1) &&
                      result.afterDisableMask.isFos(2);

  result.enableDelivered = gui_smoke_replay_row_area_double_click(
      rowArea, QPointF(result.togglePoint));
  gui_smoke_pump_events(120);
  result.activeMask = onionHandle->getOnionSkinMask();
  result.enabledOk =
      result.activeMask.isEnabled() && result.activeMask.isMos(-1) &&
      result.activeMask.isFos(2) && result.activeMask.getMosCount() == 1 &&
      result.activeMask.getFosCount() == 1;

  viewer->GLInvalidateAll();
  xsheetViewer->updateRows();
  rowArea->update();
  gui_smoke_pump_events(150);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  result.rowImageWidth  = rowAfter.width();
  result.rowImageHeight = rowAfter.height();
  result.rowAfterSaved  = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-orientations-%1-xsheet-after.png")
          .arg(label),
      &result.rowAfterPath);
  result.rowStats     = gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  result.rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  result.rowAreaOk    = result.rowBeforeSaved && result.rowAfterSaved &&
                     result.rowHighDpiOk && result.rowStats.changedPixels > 0 &&
                     result.rowStats.nonBackgroundPixels > 0;
  result.uiOk = result.markersOk && result.disabledOk && result.enabledOk &&
                result.pointsInRowArea && result.mosDelivered &&
                result.fosDelivered && result.disableDelivered &&
                result.enableDelivered && result.rowAreaOk;
  return result;
}

static void gui_smoke_append_onion_row_area_orientation_pass_details(
    QStringList& details, const QString& prefix,
    const GuiSmokeOnionRowAreaOrientationPass& pass) {
  details
      << QString("%1Orientation=%2").arg(prefix, pass.orientationName)
      << QString("%1VerticalTimeline=%2")
             .arg(prefix, pass.verticalTimeline ? QStringLiteral("true")
                                                : QStringLiteral("false"))
      << QString("%1RowAreaChangedPixels=%2")
             .arg(prefix)
             .arg(pass.rowStats.changedPixels)
      << QString("%1RowAreaImageWidth=%2").arg(prefix).arg(pass.rowImageWidth)
      << QString("%1RowAreaImageHeight=%2").arg(prefix).arg(pass.rowImageHeight)
      << QString("%1RowAreaNonBackgroundPixels=%2")
             .arg(prefix)
             .arg(pass.rowStats.nonBackgroundPixels)
      << QString("%1RowAreaDistinctColors=%2")
             .arg(prefix)
             .arg(pass.rowStats.distinctColors)
      << QString("%1RowAreaHighDpiProbe=%2")
             .arg(prefix, pass.rowHighDpiOk ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
      << QString("%1RowAreaBeforeCaptureSaved=%2")
             .arg(prefix, pass.rowBeforeSaved ? QStringLiteral("true")
                                              : QStringLiteral("false"))
      << QString("%1RowAreaAfterCaptureSaved=%2")
             .arg(prefix, pass.rowAfterSaved ? QStringLiteral("true")
                                             : QStringLiteral("false"))
      << QString("%1RowAreaBeforeCapturePath=%2")
             .arg(prefix, gui_smoke_status_value(pass.rowBeforePath))
      << QString("%1RowAreaAfterCapturePath=%2")
             .arg(prefix, gui_smoke_status_value(pass.rowAfterPath))
      << QString("%1RowAreaProbe=%2")
             .arg(prefix, pass.rowAreaOk ? QStringLiteral("ok")
                                         : QStringLiteral("error"))
      << QString("%1PointsInBounds=%2")
             .arg(prefix, pass.pointsInRowArea ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("%1MosPoint=%2,%3")
             .arg(prefix)
             .arg(pass.mosPoint.x())
             .arg(pass.mosPoint.y())
      << QString("%1FosPoint=%2,%3")
             .arg(prefix)
             .arg(pass.fosPoint.x())
             .arg(pass.fosPoint.y())
      << QString("%1TogglePoint=%2,%3")
             .arg(prefix)
             .arg(pass.togglePoint.x())
             .arg(pass.togglePoint.y())
      << QString("%1MosEventDelivered=%2")
             .arg(prefix, pass.mosDelivered ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("%1FosEventDelivered=%2")
             .arg(prefix, pass.fosDelivered ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("%1DisableEventDelivered=%2")
             .arg(prefix, pass.disableDelivered ? QStringLiteral("true")
                                                : QStringLiteral("false"))
      << QString("%1EnableEventDelivered=%2")
             .arg(prefix, pass.enableDelivered ? QStringLiteral("true")
                                               : QStringLiteral("false"))
      << QString("%1EnabledAfterMarkers=%2")
             .arg(prefix, pass.afterMarkersMask.isEnabled()
                              ? QStringLiteral("true")
                              : QStringLiteral("false"))
      << QString("%1EnabledAfterDisable=%2")
             .arg(prefix, pass.afterDisableMask.isEnabled()
                              ? QStringLiteral("true")
                              : QStringLiteral("false"))
      << QString("%1EnabledAfterEnable=%2")
             .arg(prefix, pass.activeMask.isEnabled() ? QStringLiteral("true")
                                                      : QStringLiteral("false"))
      << QString("%1UiProbe=%2")
             .arg(prefix,
                  pass.uiOk ? QStringLiteral("ok") : QStringLiteral("error"));
}

static QStringList gui_smoke_viewer_onion_skin_orientations_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinOrientationProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinOrientationProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_orientations");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinOrientationProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId backFid(1);
  const TFrameId currentFid(2);
  const TFrameId frontFid(3);
  gui_smoke_add_transparent_raster_probe_frame(
      level, backFid, TPixel32(24, 192, 232, 255), 34, 34, 122, 122);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);
  gui_smoke_add_transparent_raster_probe_frame(
      level, frontFid, TPixel32(232, 210, 24, 255), 168, 34, 246, 112);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, backFid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, frontFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinOrientationProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(1);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    if (!xsheetViewer->orientation()->isVerticalTimeline()) {
      xsheetViewer->flipOrientation();
      gui_smoke_pump_events(150);
    }
    xsheetViewer->scrollTo(1, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=orientation-row-area-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinOrientationProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-orientations"),
        QStringLiteral("orientation-row-area-onion-skin"), false, 1);
    return details;
  }

  const GuiSmokeOnionRowAreaOrientationPass verticalPass =
      gui_smoke_run_onion_row_area_orientation_pass(QStringLiteral("vertical"),
                                                    viewer, xsheetViewer,
                                                    rowArea, onionHandle);

  xsheetViewer->flipOrientation();
  gui_smoke_pump_events(150);
  xsheetViewer->scrollTo(1, 0);
  xsheetViewer->updateAreeSize();
  xsheetViewer->updateRows();
  rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  gui_smoke_pump_events(150);

  const GuiSmokeOnionRowAreaOrientationPass horizontalPass =
      gui_smoke_run_onion_row_area_orientation_pass(
          QStringLiteral("horizontal"), viewer, xsheetViewer, rowArea,
          onionHandle);

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell backCell        = xsheet->getCell(0, 0);
  const TXshCell currentCell     = xsheet->getCell(1, 0);
  const TXshCell frontCell       = xsheet->getCell(2, 0);
  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);

  const bool orientationFlipOk =
      verticalPass.verticalTimeline && !horizontalPass.verticalTimeline &&
      !verticalPass.orientationName.isEmpty() &&
      !horizontalPass.orientationName.isEmpty() &&
      verticalPass.orientationName != horizontalPass.orientationName;
  const bool onionOk =
      verticalPass.enabledOk && horizontalPass.enabledOk &&
      stageCounts.m_onionPlayers >= 2 && stageCounts.m_backOnionPlayers >= 1 &&
      stageCounts.m_frontOnionPlayers >= 1 && currentFrame == 1 &&
      currentColumn == 0 && !backCell.isEmpty() && !currentCell.isEmpty() &&
      !frontCell.isEmpty();
  const bool rowAreaOk = verticalPass.rowAreaOk && horizontalPass.rowAreaOk;
  const bool rowHighDpiOk =
      verticalPass.rowHighDpiOk && horizontalPass.rowHighDpiOk;
  const bool orientationOk =
      onionOk && orientationFlipOk && verticalPass.uiOk && horizontalPass.uiOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=orientation-row-area-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetViewerVisible=%1")
             .arg(xsheetViewer->isVisible() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaVisible=%1")
             .arg(rowArea && rowArea->isVisible() ? QStringLiteral("true")
                                                  : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea ? rowArea->width() : 0)
      << QString("xsheetRowAreaHeight=%1").arg(rowArea ? rowArea->height() : 0)
      << QString("xsheetRowAreaImageWidth=%1").arg(horizontalPass.rowImageWidth)
      << QString("xsheetRowAreaImageHeight=%1")
             .arg(horizontalPass.rowImageHeight)
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1")
             .arg(verticalPass.rowStats.changedPixels +
                  horizontalPass.rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(verticalPass.rowStats.nonBackgroundPixels +
                  horizontalPass.rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1")
             .arg(std::max(verticalPass.rowStats.distinctColors,
                           horizontalPass.rowStats.distinctColors))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("orientationBefore=%1").arg(verticalPass.orientationName)
      << QString("orientationAfter=%1").arg(horizontalPass.orientationName)
      << QString("orientationFlipProbe=%1")
             .arg(orientationFlipOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QStringLiteral("onionSkinBackOffset=-1")
      << QStringLiteral("onionSkinFixedRow=2")
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinUiProbe=%1")
             .arg(orientationOk ? QStringLiteral("ok")
                                : QStringLiteral("error"))
      << QString("onionSkinOrientationProbe=%1")
             .arg(orientationOk ? QStringLiteral("ok")
                                : QStringLiteral("error"));
  gui_smoke_append_onion_row_area_orientation_pass_details(
      details, QStringLiteral("vertical"), verticalPass);
  gui_smoke_append_onion_row_area_orientation_pass_details(
      details, QStringLiteral("horizontal"), horizontalPass);
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-orientations"),
      QStringLiteral("orientation-row-area-onion-skin"), orientationOk, 1);

  return details;
}

static QStringList gui_smoke_viewer_onion_skin_row_area_drag_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinDragProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinDragProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_onion_row_area_drag");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinDragProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId back2Fid(1);
  const TFrameId back1Fid(2);
  const TFrameId currentFid(3);
  const TFrameId front1Fid(4);
  const TFrameId front2Fid(5);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back2Fid, TPixel32(24, 192, 232, 255), 22, 28, 110, 116);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back1Fid, TPixel32(48, 122, 232, 255), 82, 94, 170, 182);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front1Fid, TPixel32(232, 210, 24, 255), 234, 34, 322, 122);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front2Fid, TPixel32(56, 208, 92, 255), 260, 150, 348, 238);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, back2Fid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, back1Fid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(3, 0, TXshCell(level, front1Fid)) ||
      !xsheet->setCell(4, 0, TXshCell(level, front2Fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinDragProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(2);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    xsheetViewer->scrollTo(2, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=row-area-raster-drag")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinDragProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-rowarea-drag"),
        QStringLiteral("row-area-onion-skin-drag"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-rowarea-drag-xsheet-before.png"),
      &rowBeforePath);

  const std::vector<QPointF> dragPoints = {
      QPointF(gui_smoke_xsheet_row_area_point(
          xsheetViewer, 0, PredefinedRect::ONION_DOT_AREA, false)),
      QPointF(gui_smoke_xsheet_row_area_point(
          xsheetViewer, 1, PredefinedRect::ONION_DOT_AREA, false)),
      QPointF(gui_smoke_xsheet_row_area_point(
          xsheetViewer, 3, PredefinedRect::ONION_DOT_AREA, false)),
      QPointF(gui_smoke_xsheet_row_area_point(
          xsheetViewer, 4, PredefinedRect::ONION_DOT_AREA, false))};
  bool pointsInRowArea = true;
  QStringList rowAreaDragPointValues;
  for (const QPointF& point : dragPoints) {
    pointsInRowArea =
        pointsInRowArea && rowArea->rect().contains(point.toPoint());
    rowAreaDragPointValues << QString("%1,%2")
                                  .arg(std::round(point.x()))
                                  .arg(std::round(point.y()));
  }

  const bool dragDelivered =
      gui_smoke_replay_row_area_drag(rowArea, dragPoints);
  gui_smoke_pump_events(150);

  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  int mosRangeStart              = 0;
  int mosRangeEnd                = -1;
  const bool hasMosRange = activeMask.getMosRange(mosRangeStart, mosRangeEnd);
  const bool maskRangeOk =
      activeMask.isEnabled() && activeMask.getMosCount() == 4 &&
      activeMask.getFosCount() == 0 && activeMask.isMos(-2) &&
      activeMask.isMos(-1) && activeMask.isMos(1) && activeMask.isMos(2) &&
      hasMosRange && mosRangeStart == -2 && mosRangeEnd == 2;

  viewer->GLInvalidateAll();
  if (xsheetViewer) xsheetViewer->updateRows();
  gui_smoke_pump_events(180);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-rowarea-drag-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const bool rowAreaOk    = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0;

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell back2Cell   = xsheet->getCell(0, 0);
  const TXshCell back1Cell   = xsheet->getCell(1, 0);
  const TXshCell currentCell = xsheet->getCell(2, 0);
  const TXshCell front1Cell  = xsheet->getCell(3, 0);
  const TXshCell front2Cell  = xsheet->getCell(4, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);
  const bool onionRowsOk = onionRows.size() == 4 && onionRows[0] == 0 &&
                           onionRows[1] == 1 && onionRows[2] == 3 &&
                           onionRows[3] == 4;

  const bool onionOk =
      maskRangeOk && onionRowsOk && stageCounts.m_onionPlayers >= 4 &&
      stageCounts.m_backOnionPlayers >= 2 &&
      stageCounts.m_frontOnionPlayers >= 2 && currentFrame == 2 &&
      currentColumn == 0 && !back2Cell.isEmpty() && !back1Cell.isEmpty() &&
      !currentCell.isEmpty() && !front1Cell.isEmpty() && !front2Cell.isEmpty();
  const bool dragOk = onionOk && pointsInRowArea && dragDelivered && rowAreaOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=row-area-raster-drag")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetViewerVisible=%1")
             .arg(xsheetViewer->isVisible() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaVisible=%1")
             .arg(rowArea->isVisible() ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1").arg(rowStats.distinctColors)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QStringLiteral("onionSkinExpectedRows=0,1,3,4")
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosRange=%1,%2").arg(mosRangeStart).arg(mosRangeEnd)
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("onionSkinEnabledAfterDrag=%1")
             .arg(activeMask.isEnabled() ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("rowAreaPointsInBounds=%1")
             .arg(pointsInRowArea ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("rowAreaDragPoints=%1").arg(rowAreaDragPointValues.join('|'))
      << QString("rowAreaDragEventDelivered=%1")
             .arg(dragDelivered ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinDragProbe=%1")
             .arg(dragOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-rowarea-drag"),
      QStringLiteral("row-area-onion-skin-drag"), dragOk, 1);

  return details;
}

static QStringList gui_smoke_viewer_onion_skin_fixed_marker_drag_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinFixedDragProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinFixedDragProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_onion_fixed_marker_drag");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinFixedDragProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId back3Fid(1);
  const TFrameId back2Fid(2);
  const TFrameId back1Fid(3);
  const TFrameId currentFid(4);
  const TFrameId front1Fid(5);
  const TFrameId front2Fid(6);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back3Fid, TPixel32(24, 192, 232, 255), 18, 30, 98, 110);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back2Fid, TPixel32(48, 122, 232, 255), 76, 88, 156, 168);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back1Fid, TPixel32(132, 82, 232, 255), 188, 44, 268, 124);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front1Fid, TPixel32(232, 210, 24, 255), 242, 40, 322, 120);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front2Fid, TPixel32(56, 208, 92, 255), 260, 154, 340, 234);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, back3Fid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, back2Fid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, back1Fid)) ||
      !xsheet->setCell(3, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(4, 0, TXshCell(level, front1Fid)) ||
      !xsheet->setCell(5, 0, TXshCell(level, front2Fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinFixedDragProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(3);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    xsheetViewer->scrollTo(3, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=fixed-marker-drag-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinFixedDragProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-fixed-marker-drag"),
        QStringLiteral("fixed-marker-onion-skin-drag"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-fixed-marker-drag-xsheet-before.png"),
      &rowBeforePath);

  auto fixedPoint = [&](int row) {
    return QPointF(gui_smoke_xsheet_row_area_point(
        xsheetViewer, row, PredefinedRect::ONION_FIXED_DOT_AREA, false));
  };
  auto pointsInBounds = [&](const std::vector<QPointF>& points) {
    for (const QPointF& point : points) {
      if (!rowArea->rect().contains(point.toPoint())) return false;
    }
    return true;
  };
  auto joinedPoints = [](const std::vector<QPointF>& points) {
    QStringList values;
    for (const QPointF& point : points) {
      values << QString("%1,%2")
                    .arg(std::round(point.x()))
                    .arg(std::round(point.y()));
    }
    return values.join('|');
  };

  const std::vector<QPointF> addBackPoints = {fixedPoint(0), fixedPoint(1),
                                              fixedPoint(2)};
  const bool addBackInBounds               = pointsInBounds(addBackPoints);
  const bool addBackDelivered =
      gui_smoke_replay_row_area_drag(rowArea, addBackPoints);
  gui_smoke_pump_events(120);
  const OnionSkinMask afterAddBackMask = onionHandle->getOnionSkinMask();
  const bool addBackOk = addBackDelivered && afterAddBackMask.isEnabled() &&
                         afterAddBackMask.getMosCount() == 0 &&
                         afterAddBackMask.getFosCount() == 3 &&
                         afterAddBackMask.isFos(0) &&
                         afterAddBackMask.isFos(1) && afterAddBackMask.isFos(2);

  const std::vector<QPointF> removeBackPoints = {fixedPoint(1), fixedPoint(2)};
  const bool removeBackInBounds = pointsInBounds(removeBackPoints);
  const bool removeBackDelivered =
      gui_smoke_replay_row_area_drag(rowArea, removeBackPoints);
  gui_smoke_pump_events(120);
  const OnionSkinMask afterRemoveBackMask = onionHandle->getOnionSkinMask();
  const bool removeBackOk =
      removeBackDelivered && afterRemoveBackMask.isEnabled() &&
      afterRemoveBackMask.getMosCount() == 0 &&
      afterRemoveBackMask.getFosCount() == 1 && afterRemoveBackMask.isFos(0) &&
      !afterRemoveBackMask.isFos(1) && !afterRemoveBackMask.isFos(2);

  const std::vector<QPointF> addFrontPoints = {fixedPoint(5), fixedPoint(4)};
  const bool addFrontInBounds               = pointsInBounds(addFrontPoints);
  const bool addFrontDelivered =
      gui_smoke_replay_row_area_drag(rowArea, addFrontPoints);
  gui_smoke_pump_events(120);
  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  const bool finalFixedOk =
      addFrontDelivered && activeMask.isEnabled() &&
      activeMask.getMosCount() == 0 && activeMask.getFosCount() == 3 &&
      activeMask.isFos(0) && activeMask.isFos(4) && activeMask.isFos(5) &&
      !activeMask.isFos(1) && !activeMask.isFos(2) && !activeMask.isFos(3);

  viewer->GLInvalidateAll();
  xsheetViewer->updateRows();
  gui_smoke_pump_events(180);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-fixed-marker-drag-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const bool rowAreaOk    = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0;

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell currentCell = xsheet->getCell(3, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);
  const bool onionRowsOk =
      onionRows.size() == 3 &&
      std::find(onionRows.begin(), onionRows.end(), 0) != onionRows.end() &&
      std::find(onionRows.begin(), onionRows.end(), 4) != onionRows.end() &&
      std::find(onionRows.begin(), onionRows.end(), 5) != onionRows.end();

  const bool onionOk =
      finalFixedOk && onionRowsOk && stageCounts.m_onionPlayers >= 3 &&
      stageCounts.m_backOnionPlayers >= 1 &&
      stageCounts.m_frontOnionPlayers >= 2 && currentFrame == 3 &&
      currentColumn == 0 && !currentCell.isEmpty();
  const bool fixedDragOk = addBackInBounds && removeBackInBounds &&
                           addFrontInBounds && addBackOk && removeBackOk &&
                           onionOk && rowAreaOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=fixed-marker-drag-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetViewerVisible=%1")
             .arg(xsheetViewer->isVisible() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaVisible=%1")
             .arg(rowArea->isVisible() ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1").arg(rowStats.distinctColors)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QStringLiteral("onionSkinExpectedRows=0,4,5")
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCountAfterAddBack=%1")
             .arg(afterAddBackMask.getFosCount())
      << QString("onionSkinFosCountAfterRemoveBack=%1")
             .arg(afterRemoveBackMask.getFosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("onionSkinEnabledAfterFixedDrag=%1")
             .arg(activeMask.isEnabled() ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("rowAreaAddFixedPointsInBounds=%1")
             .arg(addBackInBounds ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("rowAreaRemoveFixedPointsInBounds=%1")
             .arg(removeBackInBounds ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("rowAreaAddFrontFixedPointsInBounds=%1")
             .arg(addFrontInBounds ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("rowAreaAddFixedPoints=%1").arg(joinedPoints(addBackPoints))
      << QString("rowAreaRemoveFixedPoints=%1")
             .arg(joinedPoints(removeBackPoints))
      << QString("rowAreaAddFrontFixedPoints=%1")
             .arg(joinedPoints(addFrontPoints))
      << QString("rowAreaAddFixedDragDelivered=%1")
             .arg(addBackDelivered ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("rowAreaRemoveFixedDragDelivered=%1")
             .arg(removeBackDelivered ? QStringLiteral("true")
                                      : QStringLiteral("false"))
      << QString("rowAreaAddFrontFixedDragDelivered=%1")
             .arg(addFrontDelivered ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinAddFixedProbe=%1")
             .arg(addBackOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinRemoveFixedProbe=%1")
             .arg(removeBackOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinFixedDragProbe=%1")
             .arg(fixedDragOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-fixed-marker-drag"),
      QStringLiteral("fixed-marker-onion-skin-drag"), fixedDragOk, 1);

  return details;
}

static QString gui_smoke_shift_trace_offsets(OnionSkinMask& mask) {
  return QString("%1,%2")
      .arg(mask.getShiftTraceGhostFrameOffset(0))
      .arg(mask.getShiftTraceGhostFrameOffset(1));
}

static QStringList gui_smoke_viewer_onion_skin_shift_trace_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinShiftTraceProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinShiftTraceProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);
  gui_smoke_set_shift_trace_enabled(false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  mask.setShiftTraceStatus(OnionSkinMask::DISABLED);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_shift_trace");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinShiftTraceProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId back2Fid(1);
  const TFrameId back1Fid(2);
  const TFrameId currentFid(3);
  const TFrameId front1Fid(4);
  const TFrameId front2Fid(5);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back2Fid, TPixel32(24, 192, 232, 255), 20, 26, 110, 116);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back1Fid, TPixel32(48, 122, 232, 255), 82, 94, 172, 184);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 224, 224);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front1Fid, TPixel32(232, 210, 24, 255), 226, 38, 316, 128);
  gui_smoke_add_transparent_raster_probe_frame(
      level, front2Fid, TPixel32(56, 208, 92, 255), 258, 154, 348, 244);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, back2Fid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, back1Fid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(3, 0, TXshCell(level, front1Fid)) ||
      !xsheet->setCell(4, 0, TXshCell(level, front2Fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinShiftTraceProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(2);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    if (!xsheetViewer->orientation()->isVerticalTimeline()) {
      xsheetViewer->flipOrientation();
      gui_smoke_pump_events(150);
    }
    xsheetViewer->scrollTo(2, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const bool shiftTraceActionOk = gui_smoke_set_shift_trace_enabled(true);
  viewer->GLInvalidateAll();
  if (xsheetViewer) xsheetViewer->updateRows();
  gui_smoke_pump_events(150);

  const QImage before = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=shift-trace-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinShiftTraceProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-shift-trace"),
        QStringLiteral("shift-trace-onion-skin"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-shift-trace-xsheet-before.png"),
      &rowBeforePath);

  auto shiftPoint = [&](int row) {
    return QPointF(gui_smoke_xsheet_row_area_point(
        xsheetViewer, row, PredefinedRect::SHIFTTRACE_DOT_AREA, false));
  };
  auto pointsInBounds = [&](const std::vector<QPointF>& points) {
    for (const QPointF& point : points) {
      if (!rowArea->rect().contains(point.toPoint())) return false;
    }
    return true;
  };
  auto joinedPoints = [](const std::vector<QPointF>& points) {
    QStringList values;
    for (const QPointF& point : points) {
      values << QString("%1,%2")
                    .arg(std::round(point.x()))
                    .arg(std::round(point.y()));
    }
    return values.join('|');
  };

  const std::vector<QPointF> points = {shiftPoint(0), shiftPoint(1),
                                       shiftPoint(2), shiftPoint(3),
                                       shiftPoint(4)};
  const bool allPointsInBounds      = pointsInBounds(points);

  OnionSkinMask initialMask = onionHandle->getOnionSkinMask();
  const bool initialOffsetsOk =
      initialMask.isShiftTraceEnabled() &&
      initialMask.getShiftTraceGhostFrameOffset(0) == -1 &&
      initialMask.getShiftTraceGhostFrameOffset(1) == 1;

  const bool moveBackDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[0]);
  gui_smoke_pump_events(100);
  OnionSkinMask afterMoveBackMask = onionHandle->getOnionSkinMask();
  const bool moveBackOk =
      moveBackDelivered &&
      afterMoveBackMask.getShiftTraceGhostFrameOffset(0) == -2 &&
      afterMoveBackMask.getShiftTraceGhostFrameOffset(1) == 1;

  const bool moveFrontDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[4]);
  gui_smoke_pump_events(100);
  OnionSkinMask afterMoveFrontMask = onionHandle->getOnionSkinMask();
  const bool moveFrontOk =
      moveFrontDelivered &&
      afterMoveFrontMask.getShiftTraceGhostFrameOffset(0) == -2 &&
      afterMoveFrontMask.getShiftTraceGhostFrameOffset(1) == 2;

  const bool resetDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[2]);
  gui_smoke_pump_events(100);
  OnionSkinMask afterResetMask = onionHandle->getOnionSkinMask();
  const bool resetOk           = resetDelivered &&
                       afterResetMask.getShiftTraceGhostFrameOffset(0) == -1 &&
                       afterResetMask.getShiftTraceGhostFrameOffset(1) == 1;

  const bool hideBackDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[1]);
  gui_smoke_pump_events(100);
  OnionSkinMask afterHideBackMask = onionHandle->getOnionSkinMask();
  const bool hideBackOk =
      hideBackDelivered &&
      afterHideBackMask.getShiftTraceGhostFrameOffset(0) == 0 &&
      afterHideBackMask.getShiftTraceGhostFrameOffset(1) == 1;

  const bool hideFrontDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[3]);
  gui_smoke_pump_events(100);
  OnionSkinMask afterHideFrontMask = onionHandle->getOnionSkinMask();
  const bool hideFrontOk =
      hideFrontDelivered &&
      afterHideFrontMask.getShiftTraceGhostFrameOffset(0) == 0 &&
      afterHideFrontMask.getShiftTraceGhostFrameOffset(1) == 0;

  const bool finalMoveBackDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[0]);
  gui_smoke_pump_events(100);
  const bool finalMoveFrontDelivered =
      gui_smoke_replay_row_area_click(rowArea, points[4]);
  gui_smoke_pump_events(150);
  OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  const bool finalOffsetsOk =
      finalMoveBackDelivered && finalMoveFrontDelivered &&
      activeMask.isShiftTraceEnabled() &&
      activeMask.getShiftTraceStatus() == OnionSkinMask::ENABLED &&
      activeMask.getShiftTraceGhostFrameOffset(0) == -2 &&
      activeMask.getShiftTraceGhostFrameOffset(1) == 2;

  viewer->GLInvalidateAll();
  xsheetViewer->updateRows();
  rowArea->update();
  gui_smoke_pump_events(180);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-shift-trace-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const bool rowAreaOk    = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0;

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell currentCell = xsheet->getCell(2, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  const bool onionOk = finalOffsetsOk && stageCounts.m_onionPlayers >= 2 &&
                       stageCounts.m_backOnionPlayers >= 1 &&
                       stageCounts.m_frontOnionPlayers >= 1 &&
                       currentFrame == 2 && currentColumn == 0 &&
                       !currentCell.isEmpty();
  const bool shiftTraceOk = shiftTraceActionOk && allPointsInBounds &&
                            initialOffsetsOk && moveBackOk && moveFrontOk &&
                            resetOk && hideBackOk && hideFrontOk &&
                            finalOffsetsOk && onionOk && rowAreaOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=shift-trace-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetViewerVisible=%1")
             .arg(xsheetViewer->isVisible() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaVisible=%1")
             .arg(rowArea->isVisible() ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1").arg(rowStats.distinctColors)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("shiftTraceActionEnabled=%1")
             .arg(shiftTraceActionOk ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("shiftTraceInitialOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(initialMask))
      << QString("shiftTraceAfterMoveBackOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(afterMoveBackMask))
      << QString("shiftTraceAfterMoveFrontOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(afterMoveFrontMask))
      << QString("shiftTraceAfterResetOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(afterResetMask))
      << QString("shiftTraceAfterHideBackOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(afterHideBackMask))
      << QString("shiftTraceAfterHideFrontOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(afterHideFrontMask))
      << QString("shiftTraceFinalOffsets=%1")
             .arg(gui_smoke_shift_trace_offsets(activeMask))
      << QString("shiftTracePointsInBounds=%1")
             .arg(allPointsInBounds ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("shiftTracePoints=%1").arg(joinedPoints(points))
      << QString("shiftTraceMoveBackEventDelivered=%1")
             .arg(moveBackDelivered ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("shiftTraceMoveFrontEventDelivered=%1")
             .arg(moveFrontDelivered ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("shiftTraceResetEventDelivered=%1")
             .arg(resetDelivered ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("shiftTraceHideBackEventDelivered=%1")
             .arg(hideBackDelivered ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("shiftTraceHideFrontEventDelivered=%1")
             .arg(hideFrontDelivered ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("shiftTraceFinalMoveBackEventDelivered=%1")
             .arg(finalMoveBackDelivered ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("shiftTraceFinalMoveFrontEventDelivered=%1")
             .arg(finalMoveFrontDelivered ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QString("onionSkinShiftTraceStatus=%1")
             .arg(activeMask.getShiftTraceStatus())
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinShiftTraceProbe=%1")
             .arg(shiftTraceOk ? QStringLiteral("ok")
                               : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-shift-trace"),
      QStringLiteral("shift-trace-onion-skin"), shiftTraceOk, 1);

  return details;
}

static QAction* gui_smoke_find_menu_action(QMenu* menu,
                                           const QString& actionText) {
  if (!menu) return nullptr;
  const QList<QAction*> actions = menu->actions();
  for (QAction* action : actions) {
    if (action && action->text() == actionText) return action;
  }
  return nullptr;
}

static QString gui_smoke_join_menu_action_texts(QMenu* menu) {
  QStringList values;
  if (!menu) return QString();
  const QList<QAction*> actions = menu->actions();
  for (QAction* action : actions) {
    if (action && !action->isSeparator())
      values << action->text().replace(QStringLiteral(","),
                                       QStringLiteral(";"));
  }
  return values.join('|');
}

static bool gui_smoke_trigger_onion_menu_action(QWidget* parent,
                                                const QString& actionText,
                                                QStringList* menuTexts) {
  QMenu menu(parent);
  OnioniSkinMaskGUI::addOnionSkinCommand(&menu, false);
  if (menuTexts) *menuTexts << gui_smoke_join_menu_action_texts(&menu);
  QAction* action = gui_smoke_find_menu_action(&menu, actionText);
  if (!action || !action->isEnabled()) return false;
  action->trigger();
  gui_smoke_pump_events(80);
  return true;
}

static QStringList gui_smoke_viewer_onion_skin_context_menu_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinMenuProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinMenuProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  Preferences::instance()->setValue(onionSkinEnabled, true, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_context_menu");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinMenuProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId back3Fid(1);
  const TFrameId back2Fid(2);
  const TFrameId back1Fid(3);
  const TFrameId currentFid(4);
  const TFrameId frontFid(5);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back3Fid, TPixel32(24, 192, 232, 255), 22, 28, 110, 116);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back2Fid, TPixel32(48, 122, 232, 255), 82, 94, 170, 182);
  gui_smoke_add_transparent_raster_probe_frame(
      level, back1Fid, TPixel32(132, 82, 232, 255), 214, 42, 302, 130);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222, 222);
  gui_smoke_add_transparent_raster_probe_frame(
      level, frontFid, TPixel32(56, 208, 92, 255), 260, 150, 348, 238);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(level, back3Fid)) ||
      !xsheet->setCell(1, 0, TXshCell(level, back2Fid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, back1Fid)) ||
      !xsheet->setCell(3, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(4, 0, TXshCell(level, frontFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinMenuProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(3);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    xsheetViewer->scrollTo(3, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=context-menu-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinMenuProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-context-menu"),
        QStringLiteral("onion-skin-context-menu"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-context-menu-xsheet-before.png"),
      &rowBeforePath);

  QStringList menuStates;
  QMenu initialMenu(rowArea);
  OnioniSkinMaskGUI::addOnionSkinCommand(&initialMenu, false);
  const QString initialMenuLabels =
      gui_smoke_join_menu_action_texts(&initialMenu);
  const bool initialMenuOk =
      gui_smoke_find_menu_action(
          &initialMenu, QStringLiteral("Activate Onion Skin")) != nullptr;

  const bool activateDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Activate Onion Skin"), &menuStates);
  const OnionSkinMask afterActivateMask = onionHandle->getOnionSkinMask();
  const bool activateOk =
      activateDelivered && afterActivateMask.isEnabled() &&
      !afterActivateMask.isWholeScene() &&
      afterActivateMask.getMosCount() == 3 &&
      afterActivateMask.getFosCount() == 0 && afterActivateMask.isMos(-1) &&
      afterActivateMask.isMos(-2) && afterActivateMask.isMos(-3);

  const bool extendDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Extend Onion Skin To Scene"), &menuStates);
  const OnionSkinMask afterExtendMask = onionHandle->getOnionSkinMask();
  const bool extendOk = extendDelivered && afterExtendMask.isWholeScene();

  const bool limitDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Limit Onion Skin To Level"), &menuStates);
  const OnionSkinMask afterLimitMask = onionHandle->getOnionSkinMask();
  const bool limitOk = limitDelivered && !afterLimitMask.isWholeScene();

  const bool deactivateDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Deactivate Onion Skin"), &menuStates);
  const OnionSkinMask afterDeactivateMask = onionHandle->getOnionSkinMask();
  const bool deactivateOk                 = deactivateDelivered &&
                            !afterDeactivateMask.isEnabled() &&
                            afterDeactivateMask.getMosCount() == 3 &&
                            afterDeactivateMask.getFosCount() == 0;

  const bool reactivateDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Activate Onion Skin"), &menuStates);
  const OnionSkinMask afterReactivateMask = onionHandle->getOnionSkinMask();
  const bool reactivateOk                 = reactivateDelivered &&
                            afterReactivateMask.isEnabled() &&
                            afterReactivateMask.getMosCount() == 3 &&
                            afterReactivateMask.getFosCount() == 0;

  OnionSkinMask combinedMask = afterReactivateMask;
  combinedMask.setFos(4, true);
  onionHandle->setOnionSkinMask(combinedMask);
  onionHandle->notifyOnionSkinMaskChanged();
  gui_smoke_pump_events(80);

  const bool clearFosDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Clear All Fixed Onion Skin Markers"),
      &menuStates);
  const OnionSkinMask afterClearFosMask = onionHandle->getOnionSkinMask();
  const bool clearFosOk = clearFosDelivered && afterClearFosMask.isEnabled() &&
                          afterClearFosMask.getMosCount() == 3 &&
                          afterClearFosMask.getFosCount() == 0;

  combinedMask = afterClearFosMask;
  combinedMask.setFos(4, true);
  onionHandle->setOnionSkinMask(combinedMask);
  onionHandle->notifyOnionSkinMaskChanged();
  gui_smoke_pump_events(80);

  const bool clearMosDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Clear All Relative Onion Skin Markers"),
      &menuStates);
  const OnionSkinMask afterClearMosMask = onionHandle->getOnionSkinMask();
  const bool clearMosOk = clearMosDelivered && afterClearMosMask.isEnabled() &&
                          afterClearMosMask.getMosCount() == 0 &&
                          afterClearMosMask.getFosCount() == 1 &&
                          afterClearMosMask.isFos(4);

  combinedMask = afterClearMosMask;
  combinedMask.setMos(-1, true);
  combinedMask.setMos(-2, true);
  combinedMask.setMos(-3, true);
  onionHandle->setOnionSkinMask(combinedMask);
  onionHandle->notifyOnionSkinMaskChanged();
  gui_smoke_pump_events(80);

  const bool clearAllDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Clear All Onion Skin Markers"), &menuStates);
  const OnionSkinMask afterClearAllMask = onionHandle->getOnionSkinMask();
  const bool clearAllOk = clearAllDelivered && afterClearAllMask.isEnabled() &&
                          afterClearAllMask.getMosCount() == 0 &&
                          afterClearAllMask.getFosCount() == 0;

  const bool finalActivateDelivered = gui_smoke_trigger_onion_menu_action(
      rowArea, QStringLiteral("Activate Onion Skin"), &menuStates);
  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();
  int mosRangeStart              = 0;
  int mosRangeEnd                = -1;
  const bool hasMosRange = activeMask.getMosRange(mosRangeStart, mosRangeEnd);
  const bool finalMaskOk =
      finalActivateDelivered && activeMask.isEnabled() &&
      !activeMask.isWholeScene() && activeMask.getMosCount() == 3 &&
      activeMask.getFosCount() == 0 && activeMask.isMos(-1) &&
      activeMask.isMos(-2) && activeMask.isMos(-3) && hasMosRange &&
      mosRangeStart == -3 && mosRangeEnd == -1;

  viewer->GLInvalidateAll();
  xsheetViewer->updateRows();
  gui_smoke_pump_events(180);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-context-menu-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const bool rowAreaOk    = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0;

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell currentCell = xsheet->getCell(3, 0);

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);
  const bool onionRowsOk = onionRows.size() == 3 && onionRows[0] == 0 &&
                           onionRows[1] == 1 && onionRows[2] == 2;

  const bool onionOk =
      finalMaskOk && onionRowsOk && stageCounts.m_onionPlayers >= 3 &&
      stageCounts.m_backOnionPlayers >= 3 &&
      stageCounts.m_frontOnionPlayers == 0 && currentFrame == 3 &&
      currentColumn == 0 && !currentCell.isEmpty();
  const bool menuOk = initialMenuOk && activateOk && extendOk && limitOk &&
                      deactivateOk && reactivateOk && clearFosOk &&
                      clearMosOk && clearAllOk && onionOk && rowAreaOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=context-menu-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaDistinctColors=%1").arg(rowStats.distinctColors)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinInitialMenuActions=%1")
             .arg(gui_smoke_status_value(initialMenuLabels))
      << QString("onionSkinMenuStates=%1")
             .arg(gui_smoke_status_value(menuStates.join('/')))
      << QString("onionSkinActivateCommand=%1")
             .arg(activateOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinExtendCommand=%1")
             .arg(extendOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinLimitCommand=%1")
             .arg(limitOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinDeactivateCommand=%1")
             .arg(deactivateOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinReactivateCommand=%1")
             .arg(reactivateOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinClearFixedCommand=%1")
             .arg(clearFosOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinClearRelativeCommand=%1")
             .arg(clearMosOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinClearAllCommand=%1")
             .arg(clearAllOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosRange=%1,%2").arg(mosRangeStart).arg(mosRangeEnd)
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("onionSkinWholeScene=%1")
             .arg(activeMask.isWholeScene() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinMenuProbe=%1")
             .arg(menuOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-context-menu"),
      QStringLiteral("onion-skin-context-menu"), menuOk, 1);

  return details;
}

static QStringList gui_smoke_viewer_onion_skin_custom_colors_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer               = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle* onionHandle = TApp::instance()->getCurrentOnionSkin();
  if (!viewer || !onionHandle) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("onionSkinProbe=no-viewer")
            << QStringLiteral("onionSkinCustomColorProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("onionSkinProbe=scene-folder-error")
            << QStringLiteral("onionSkinCustomColorProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const QColor backOnionCustomColor(44, 92, 224);
  const QColor frontOnionCustomColor(232, 184, 28);
  Preferences::instance()->setValue(onionSkinEnabled, true, false);
  Preferences::instance()->setValue(backOnionColor, backOnionCustomColor,
                                    false);
  Preferences::instance()->setValue(frontOnionColor, frontOnionCustomColor,
                                    false);
  Preferences::instance()->setValue(onionInksOnly, false, false);

  OnionSkinMask mask;
  mask.clear();
  mask.enable(false);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_onion_custom_colors");
  TXshSimpleLevel* level = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!level) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("onionSkinProbe=level-error")
            << QStringLiteral("onionSkinCustomColorProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId backFid(1);
  const TFrameId currentFid(2);
  const TFrameId frontFid(3);
  gui_smoke_add_transparent_raster_probe_frame(
      level, backFid, TPixel32(0, 0, 0, 255), 34, 34, 134, 134);
  gui_smoke_add_transparent_raster_probe_frame(
      level, currentFid, TPixel32(232, 24, 24, 255), 146, 146, 246, 246);
  gui_smoke_add_transparent_raster_probe_frame(
      level, frontFid, TPixel32(0, 0, 0, 255), 168, 34, 246, 112);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(1, 0, TXshCell(level, backFid)) ||
      !xsheet->setCell(2, 0, TXshCell(level, currentFid)) ||
      !xsheet->setCell(3, 0, TXshCell(level, frontFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QStringLiteral("onionSkinCustomColorProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(2);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  XsheetViewer* xsheetViewer = gui_smoke_resolve_xsheet_viewer();
  if (xsheetViewer) {
    xsheetViewer->scrollTo(2, 0);
    xsheetViewer->updateAreeSize();
    xsheetViewer->updateRows();
  }
  gui_smoke_pump_events(150);

  XsheetGUI::RowArea* rowArea = gui_smoke_resolve_xsheet_row_area(xsheetViewer);
  const QImage before         = gui_smoke_grab_viewer_frame(viewer);
  if (!xsheetViewer || !rowArea) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QStringLiteral("onionSkinContent=custom-color-raster")
            << QString("xsheetViewerFound=%1")
                   .arg(xsheetViewer ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("xsheetRowAreaFound=%1")
                   .arg(rowArea ? QStringLiteral("true")
                                : QStringLiteral("false"))
            << QStringLiteral("onionSkinProbe=no-row-area")
            << QStringLiteral("onionSkinCustomColorProbe=no-row-area");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-onion-skin-custom-colors"),
        QStringLiteral("onion-skin-custom-colors"), false, 1);
    return details;
  }

  const QImage rowBefore = gui_smoke_grab_widget_frame(rowArea);
  QString rowBeforePath;
  const bool rowBeforeSaved = gui_smoke_save_capture(
      rowBefore,
      QStringLiteral("viewer-onion-skin-custom-colors-xsheet-before.png"),
      &rowBeforePath);

  mask.clear();
  mask.enable(true);
  mask.setMos(-1, true);
  mask.setMos(1, true);
  onionHandle->setOnionSkinMask(mask);
  onionHandle->notifyOnionSkinMaskChanged();

  viewer->GLInvalidateAll();
  xsheetViewer->updateRows();
  gui_smoke_pump_events(180);

  const QImage rowAfter = gui_smoke_grab_widget_frame(rowArea);
  QString rowAfterPath;
  const bool rowAfterSaved = gui_smoke_save_capture(
      rowAfter,
      QStringLiteral("viewer-onion-skin-custom-colors-xsheet-after.png"),
      &rowAfterPath);
  const GuiSmokeWidgetImageStats rowStats =
      gui_smoke_analyze_widget_frame(rowAfter, rowBefore);
  const bool rowHighDpiOk = gui_smoke_widget_high_dpi_ok(rowArea, rowAfter);
  const int rowBackColorPixels =
      gui_smoke_count_hue_pixels(rowAfter, backOnionCustomColor);
  const int rowFrontColorPixels =
      gui_smoke_count_hue_pixels(rowAfter, frontOnionCustomColor);

  const int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();
  const int currentColumn =
      TApp::instance()->getCurrentColumn()->getColumnIndex();
  const TXshCell currentCell     = xsheet->getCell(2, 0);
  const OnionSkinMask activeMask = onionHandle->getOnionSkinMask();

  ImagePainter::VisualSettings visualSettings;
  visualSettings.m_sceneProperties = scene->getProperties();
  GuiSmokeStageOnionCounts stageCounts(visualSettings);
  Stage::VisitArgs args;
  args.m_scene          = scene;
  args.m_xsh            = xsheet;
  args.m_row            = currentFrame;
  args.m_col            = currentColumn;
  args.m_osm            = &activeMask;
  args.m_currentFrameId = currentCell.getFrameId();
  Stage::visit(stageCounts, args);

  std::vector<int> onionRows;
  activeMask.getAll(currentFrame, onionRows);
  QStringList onionRowValues;
  for (int row : onionRows) onionRowValues << QString::number(row);

  const bool onionOk =
      activeMask.isEnabled() && activeMask.getMosCount() == 2 &&
      activeMask.getFosCount() == 0 && activeMask.isMos(-1) &&
      activeMask.isMos(1) && onionRows.size() == 2 && onionRows[0] == 1 &&
      onionRows[1] == 3 && stageCounts.m_backOnionPlayers >= 1 &&
      stageCounts.m_frontOnionPlayers >= 1 && currentFrame == 2 &&
      currentColumn == 0 && !currentCell.isEmpty();
  const bool rowAreaOk = rowBeforeSaved && rowAfterSaved && rowHighDpiOk &&
                         rowStats.changedPixels > 0 &&
                         rowStats.nonBackgroundPixels > 0 &&
                         rowBackColorPixels > 4 && rowFrontColorPixels > 4;

  const TPixel32 storedFrontColor =
      Preferences::instance()->getColorValue(frontOnionColor);
  const TPixel32 storedBackColor =
      Preferences::instance()->getColorValue(backOnionColor);
  const bool preferenceColorOk =
      storedBackColor.r == backOnionCustomColor.red() &&
      storedBackColor.g == backOnionCustomColor.green() &&
      storedBackColor.b == backOnionCustomColor.blue() &&
      storedFrontColor.r == frontOnionCustomColor.red() &&
      storedFrontColor.g == frontOnionCustomColor.green() &&
      storedFrontColor.b == frontOnionCustomColor.blue();

  QImage afterPreview = gui_smoke_grab_viewer_frame(viewer);
  const int viewerBackColorPixels =
      gui_smoke_count_hue_pixels(afterPreview, backOnionCustomColor);
  const int viewerFrontColorPixels =
      gui_smoke_count_hue_pixels(afterPreview, frontOnionCustomColor);
  const bool viewerColorOk =
      viewerBackColorPixels > 100 && viewerFrontColorPixels > 100;
  const bool customColorOk =
      preferenceColorOk && rowAreaOk && viewerColorOk && onionOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("onionSkinContent=custom-color-raster")
      << QString("xsheetViewerFound=%1")
             .arg(xsheetViewer ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("xsheetRowAreaFound=%1")
             .arg(rowArea ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("onionSkinBackColor=%1,%2,%3")
             .arg(backOnionCustomColor.red())
             .arg(backOnionCustomColor.green())
             .arg(backOnionCustomColor.blue())
      << QString("onionSkinFrontColor=%1,%2,%3")
             .arg(frontOnionCustomColor.red())
             .arg(frontOnionCustomColor.green())
             .arg(frontOnionCustomColor.blue())
      << QString("onionSkinStoredBackColor=%1,%2,%3")
             .arg(storedBackColor.r)
             .arg(storedBackColor.g)
             .arg(storedBackColor.b)
      << QString("onionSkinStoredFrontColor=%1,%2,%3")
             .arg(storedFrontColor.r)
             .arg(storedFrontColor.g)
             .arg(storedFrontColor.b)
      << QString("xsheetRowAreaWidth=%1").arg(rowArea->width())
      << QString("xsheetRowAreaHeight=%1").arg(rowArea->height())
      << QString("xsheetRowAreaImageWidth=%1").arg(rowAfter.width())
      << QString("xsheetRowAreaImageHeight=%1").arg(rowAfter.height())
      << QString("xsheetRowAreaHighDpiProbe=%1")
             .arg(rowHighDpiOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("xsheetRowAreaChangedPixels=%1").arg(rowStats.changedPixels)
      << QString("xsheetRowAreaNonBackgroundPixels=%1")
             .arg(rowStats.nonBackgroundPixels)
      << QString("xsheetRowAreaBackColorPixels=%1").arg(rowBackColorPixels)
      << QString("xsheetRowAreaFrontColorPixels=%1").arg(rowFrontColorPixels)
      << QString("xsheetRowAreaBeforeCaptureSaved=%1")
             .arg(rowBeforeSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("xsheetRowAreaAfterCaptureSaved=%1")
             .arg(rowAfterSaved ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("xsheetRowAreaBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(rowBeforePath))
      << QString("xsheetRowAreaAfterCapturePath=%1")
             .arg(gui_smoke_status_value(rowAfterPath))
      << QString("xsheetRowAreaProbe=%1")
             .arg(rowAreaOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("viewerBackColorPixels=%1").arg(viewerBackColorPixels)
      << QString("viewerFrontColorPixels=%1").arg(viewerFrontColorPixels)
      << QString("onionSkinCurrentRow=%1").arg(currentFrame)
      << QString("onionSkinRows=%1").arg(onionRowValues.join(','))
      << QString("onionSkinMosCount=%1").arg(activeMask.getMosCount())
      << QString("onionSkinFosCount=%1").arg(activeMask.getFosCount())
      << QString("stagePlayerCount=%1").arg(stageCounts.m_totalPlayers)
      << QString("stageCurrentPlayerCount=%1").arg(stageCounts.m_currentPlayers)
      << QString("stageCurrentColumnPlayerCount=%1")
             .arg(stageCounts.m_currentColumnCount)
      << QString("stageOnionPlayerCount=%1").arg(stageCounts.m_onionPlayers)
      << QString("stageBackOnionPlayerCount=%1")
             .arg(stageCounts.m_backOnionPlayers)
      << QString("stageFrontOnionPlayerCount=%1")
             .arg(stageCounts.m_frontOnionPlayers)
      << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
      << QString("onionSkinPreferenceColorProbe=%1")
             .arg(preferenceColorOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("onionSkinViewerColorProbe=%1")
             .arg(viewerColorOk ? QStringLiteral("ok")
                                : QStringLiteral("error"))
      << QString("onionSkinProbe=%1")
             .arg(onionOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("onionSkinCustomColorProbe=%1")
             .arg(customColorOk ? QStringLiteral("ok")
                                : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin-custom-colors"),
      QStringLiteral("onion-skin-custom-colors"), customColorOk);

  return details;
}

static QStringList gui_smoke_viewer_ruler_guide_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerGuideProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerGuideProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties* properties = scene->getProperties();
  properties->getHGuides().clear();
  properties->getVGuides().clear();
  properties->getHGuides().push_back(0.0);
  properties->getVGuides().push_back(0.0);

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool rulerDisabled      = gui_smoke_set_view_ruler_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  const bool rulerEnabled = gui_smoke_set_view_ruler_overlay(true);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const bool guideEnabled = gui_smoke_set_view_guide_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const int rulerCount        = gui_smoke_ruler_count();
  const int visibleRulerCount = gui_smoke_visible_ruler_count();
  const int hGuideCount       = properties->getHGuides().size();
  const int vGuideCount       = properties->getVGuides().size();
  const bool guideOk = guideDisabled && guideEnabled && rulerDisabled &&
                       rulerEnabled && hGuideCount > 0 && vGuideCount > 0 &&
                       rulerCount >= 2 && visibleRulerCount >= 2;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("cameraOverlayDisabled=%1")
                 .arg(cameraDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
          << QString("safeAreaDisabled=%1")
                 .arg(safeAreaDisabled ? QStringLiteral("true")
                                       : QStringLiteral("false"))
          << QString("fieldGuideDisabled=%1")
                 .arg(fieldGuideDisabled ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("viewGuideDisabled=%1")
                 .arg(guideDisabled ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("viewGuideEnabled=%1")
                 .arg(guideEnabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("viewRulerDisabled=%1")
                 .arg(rulerDisabled ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("viewRulerEnabled=%1")
                 .arg(rulerEnabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("hGuideCount=%1").arg(hGuideCount)
          << QString("vGuideCount=%1").arg(vGuideCount)
          << QString("rulerWidgetCount=%1").arg(rulerCount)
          << QString("visibleRulerWidgetCount=%1").arg(visibleRulerCount)
          << QString("rulerGuideProbe=%1")
                 .arg(guideOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-guide"),
      QStringLiteral("ruler-guide"), guideOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_ruler_guide_events_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerGuideEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerGuideEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties* properties = scene->getProperties();
  properties->getHGuides().clear();
  properties->getVGuides().clear();

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideEnabled       = gui_smoke_set_view_guide_overlay(true);
  const bool rulerEnabled       = gui_smoke_set_view_ruler_overlay(true);

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  Ruler* horizontalRuler = nullptr;
  Ruler* verticalRuler   = nullptr;
  gui_smoke_resolve_visible_rulers(&horizontalRuler, &verticalRuler);
  if (!horizontalRuler || !verticalRuler) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QString("viewGuideEnabled=%1")
                   .arg(guideEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("viewRulerEnabled=%1")
                   .arg(rulerEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
            << QString("visibleRulerWidgetCount=%1")
                   .arg(gui_smoke_visible_ruler_count())
            << QStringLiteral("rulerGuideEventProbe=no-visible-rulers");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-ruler-guide-events"),
        QStringLiteral("ruler-guide-events"), false, 0, 0, 0, 1);
    return details;
  }

  const int horizontalBefore = horizontalRuler->getGuideCount();
  const int verticalBefore   = verticalRuler->getGuideCount();
  const QPointF horizontalStart =
      gui_smoke_ruler_drag_point(horizontalRuler, false, -90.0);
  const QPointF horizontalEnd =
      gui_smoke_ruler_drag_point(horizontalRuler, false, 110.0);
  const QPointF verticalStart =
      gui_smoke_ruler_drag_point(verticalRuler, true, -80.0);
  const QPointF verticalEnd =
      gui_smoke_ruler_drag_point(verticalRuler, true, 120.0);
  const double horizontalStartValue =
      horizontalRuler->posToValue(horizontalStart.toPoint()) *
      viewer->getDevPixRatio();
  const double horizontalEndValue =
      horizontalRuler->posToValue(horizontalEnd.toPoint()) *
      viewer->getDevPixRatio();
  const double verticalStartValue =
      verticalRuler->posToValue(verticalStart.toPoint()) *
      viewer->getDevPixRatio();
  const double verticalEndValue =
      verticalRuler->posToValue(verticalEnd.toPoint()) *
      viewer->getDevPixRatio();

  const bool horizontalDragDelivered = gui_smoke_replay_ruler_drag(
      horizontalRuler, horizontalStart, horizontalEnd);
  const bool verticalDragDelivered =
      gui_smoke_replay_ruler_drag(verticalRuler, verticalStart, verticalEnd);
  gui_smoke_pump_events(100);

  const int horizontalAfterMove = horizontalRuler->getGuideCount();
  const int verticalAfterMove   = verticalRuler->getGuideCount();
  const double horizontalGuideAfterMove =
      horizontalAfterMove > 0
          ? horizontalRuler->getGuide(horizontalAfterMove - 1)
          : 0.0;
  const double verticalGuideAfterMove =
      verticalAfterMove > 0 ? verticalRuler->getGuide(verticalAfterMove - 1)
                            : 0.0;

  const bool horizontalMoved =
      horizontalAfterMove == horizontalBefore + 1 &&
      !gui_smoke_near(horizontalStartValue, horizontalGuideAfterMove, 0.01) &&
      gui_smoke_near(horizontalEndValue, horizontalGuideAfterMove, 0.01);
  const bool verticalMoved =
      verticalAfterMove == verticalBefore + 1 &&
      !gui_smoke_near(verticalStartValue, verticalGuideAfterMove, 0.01) &&
      gui_smoke_near(verticalEndValue, verticalGuideAfterMove, 0.01);

  const bool horizontalDeleteDelivered =
      gui_smoke_delete_ruler_guide(horizontalRuler, horizontalEnd);
  gui_smoke_pump_events(100);

  const int horizontalAfterDelete = horizontalRuler->getGuideCount();
  const int verticalAfterDelete   = verticalRuler->getGuideCount();
  const int hGuideCount           = properties->getHGuides().size();
  const int vGuideCount           = properties->getVGuides().size();
  const bool horizontalDeleted    = horizontalAfterDelete == horizontalBefore;
  const bool verticalPreserved    = verticalAfterDelete == verticalAfterMove;
  const bool eventOk              = cameraDisabled && safeAreaDisabled &&
                       fieldGuideDisabled && guideEnabled && rulerEnabled &&
                       horizontalDragDelivered && verticalDragDelivered &&
                       horizontalDeleteDelivered && horizontalMoved &&
                       verticalMoved && horizontalDeleted &&
                       verticalPreserved && (hGuideCount + vGuideCount) == 1;

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("viewGuideEnabled=%1")
             .arg(guideEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("viewRulerEnabled=%1")
             .arg(rulerEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
      << QString("visibleRulerWidgetCount=%1")
             .arg(gui_smoke_visible_ruler_count())
      << QString("horizontalRulerSize=%1x%2")
             .arg(horizontalRuler->width())
             .arg(horizontalRuler->height())
      << QString("verticalRulerSize=%1x%2")
             .arg(verticalRuler->width())
             .arg(verticalRuler->height())
      << QString("horizontalGuideCountBefore=%1").arg(horizontalBefore)
      << QString("horizontalGuideCountAfterMove=%1").arg(horizontalAfterMove)
      << QString("horizontalGuideCountAfterDelete=%1")
             .arg(horizontalAfterDelete)
      << QString("verticalGuideCountBefore=%1").arg(verticalBefore)
      << QString("verticalGuideCountAfterMove=%1").arg(verticalAfterMove)
      << QString("verticalGuideCountAfterDelete=%1").arg(verticalAfterDelete)
      << QString("hGuideCount=%1").arg(hGuideCount)
      << QString("vGuideCount=%1").arg(vGuideCount)
      << QString("horizontalGuideStart=%1").arg(horizontalStartValue, 0, 'f', 4)
      << QString("horizontalGuideAfterMove=%1")
             .arg(horizontalGuideAfterMove, 0, 'f', 4)
      << QString("horizontalGuideEnd=%1").arg(horizontalEndValue, 0, 'f', 4)
      << QString("verticalGuideStart=%1").arg(verticalStartValue, 0, 'f', 4)
      << QString("verticalGuideAfterMove=%1")
             .arg(verticalGuideAfterMove, 0, 'f', 4)
      << QString("verticalGuideEnd=%1").arg(verticalEndValue, 0, 'f', 4)
      << QString("horizontalDragDelivered=%1")
             .arg(horizontalDragDelivered ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("verticalDragDelivered=%1")
             .arg(verticalDragDelivered ? QStringLiteral("true")
                                        : QStringLiteral("false"))
      << QString("horizontalDeleteDelivered=%1")
             .arg(horizontalDeleteDelivered ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("rulerGuideEventProbe=%1")
             .arg(eventOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-guide-events"),
      QStringLiteral("ruler-guide-events"), eventOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_ruler_guide_variants_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerGuideVariantProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerGuideVariantProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties* properties = scene->getProperties();
  properties->getHGuides().clear();
  properties->getVGuides().clear();

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideEnabled       = gui_smoke_set_view_guide_overlay(true);
  const bool rulerEnabled       = gui_smoke_set_view_ruler_overlay(true);

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  Ruler* horizontalRuler = nullptr;
  Ruler* verticalRuler   = nullptr;
  gui_smoke_resolve_visible_rulers(&horizontalRuler, &verticalRuler);
  if (!horizontalRuler || !verticalRuler) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QString("viewGuideEnabled=%1")
                   .arg(guideEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("viewRulerEnabled=%1")
                   .arg(rulerEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
            << QString("visibleRulerWidgetCount=%1")
                   .arg(gui_smoke_visible_ruler_count())
            << QStringLiteral("rulerGuideVariantProbe=no-visible-rulers");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-ruler-guide-variants"),
        QStringLiteral("ruler-guide-variants"), false, 0, 0, 0, 1);
    return details;
  }

  const int horizontalBefore = horizontalRuler->getGuideCount();
  const int verticalBefore   = verticalRuler->getGuideCount();
  const QPointF horizontalStart =
      gui_smoke_ruler_drag_point(horizontalRuler, false, -120.0);
  const QPointF horizontalEnd =
      gui_smoke_ruler_drag_point(horizontalRuler, false, 120.0);
  const QPointF horizontalHideEnd(horizontalEnd.x(), -8.0);
  const QPointF verticalStart =
      gui_smoke_ruler_drag_point(verticalRuler, true, -120.0);
  const QPointF verticalEnd =
      gui_smoke_ruler_drag_point(verticalRuler, true, 120.0);
  const QPointF verticalHideEnd(-8.0, verticalEnd.y());
  const QPointF horizontalFinalStart =
      gui_smoke_ruler_drag_point(horizontalRuler, false, -40.0);
  const QPointF horizontalFinalEnd =
      gui_smoke_ruler_drag_point(horizontalRuler, false, 40.0);
  const QPointF verticalFinalStart =
      gui_smoke_ruler_drag_point(verticalRuler, true, -40.0);
  const QPointF verticalFinalEnd =
      gui_smoke_ruler_drag_point(verticalRuler, true, 40.0);

  const bool horizontalCreateDelivered = gui_smoke_replay_ruler_drag(
      horizontalRuler, horizontalStart, horizontalEnd);
  gui_smoke_pump_events(80);
  const int horizontalAfterCreate    = horizontalRuler->getGuideCount();
  const bool horizontalHideDelivered = gui_smoke_replay_ruler_drag(
      horizontalRuler, horizontalEnd, horizontalHideEnd, true);
  gui_smoke_pump_events(80);
  const int horizontalAfterHide = horizontalRuler->getGuideCount();

  const bool verticalCreateDelivered =
      gui_smoke_replay_ruler_drag(verticalRuler, verticalStart, verticalEnd);
  gui_smoke_pump_events(80);
  const int verticalAfterCreate    = verticalRuler->getGuideCount();
  const bool verticalHideDelivered = gui_smoke_replay_ruler_drag(
      verticalRuler, verticalEnd, verticalHideEnd, true);
  gui_smoke_pump_events(80);
  const int verticalAfterHide = verticalRuler->getGuideCount();

  const bool horizontalFinalDelivered = gui_smoke_replay_ruler_drag(
      horizontalRuler, horizontalFinalStart, horizontalFinalEnd);
  const bool verticalFinalDelivered = gui_smoke_replay_ruler_drag(
      verticalRuler, verticalFinalStart, verticalFinalEnd);
  gui_smoke_pump_events(120);

  const int horizontalFinal   = horizontalRuler->getGuideCount();
  const int verticalFinal     = verticalRuler->getGuideCount();
  const int hGuideCount       = properties->getHGuides().size();
  const int vGuideCount       = properties->getVGuides().size();
  const bool horizontalHidden = horizontalAfterCreate == horizontalBefore + 1 &&
                                horizontalAfterHide == horizontalBefore;
  const bool verticalHidden = verticalAfterCreate == verticalBefore + 1 &&
                              verticalAfterHide == verticalBefore;
  const bool finalGuidesVisible = horizontalFinal == horizontalBefore + 1 &&
                                  verticalFinal == verticalBefore + 1 &&
                                  hGuideCount == horizontalFinal &&
                                  vGuideCount == verticalFinal;
  const bool variantOk =
      cameraDisabled && safeAreaDisabled && fieldGuideDisabled &&
      guideEnabled && rulerEnabled && horizontalCreateDelivered &&
      horizontalHideDelivered && verticalCreateDelivered &&
      verticalHideDelivered && horizontalFinalDelivered &&
      verticalFinalDelivered && horizontalHidden && verticalHidden &&
      finalGuidesVisible && gui_smoke_visible_ruler_count() >= 2;

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("viewGuideEnabled=%1")
             .arg(guideEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("viewRulerEnabled=%1")
             .arg(rulerEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
      << QString("visibleRulerWidgetCount=%1")
             .arg(gui_smoke_visible_ruler_count())
      << QString("horizontalGuideCountBefore=%1").arg(horizontalBefore)
      << QString("horizontalGuideCountAfterCreate=%1")
             .arg(horizontalAfterCreate)
      << QString("horizontalGuideCountAfterHide=%1").arg(horizontalAfterHide)
      << QString("horizontalGuideCountFinal=%1").arg(horizontalFinal)
      << QString("verticalGuideCountBefore=%1").arg(verticalBefore)
      << QString("verticalGuideCountAfterCreate=%1").arg(verticalAfterCreate)
      << QString("verticalGuideCountAfterHide=%1").arg(verticalAfterHide)
      << QString("verticalGuideCountFinal=%1").arg(verticalFinal)
      << QString("hGuideCount=%1").arg(hGuideCount)
      << QString("vGuideCount=%1").arg(vGuideCount)
      << QString("horizontalCreateDelivered=%1")
             .arg(horizontalCreateDelivered ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("horizontalHideDelivered=%1")
             .arg(horizontalHideDelivered ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("verticalCreateDelivered=%1")
             .arg(verticalCreateDelivered ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("verticalHideDelivered=%1")
             .arg(verticalHideDelivered ? QStringLiteral("true")
                                        : QStringLiteral("false"))
      << QString("horizontalFinalDelivered=%1")
             .arg(horizontalFinalDelivered ? QStringLiteral("true")
                                           : QStringLiteral("false"))
      << QString("verticalFinalDelivered=%1")
             .arg(verticalFinalDelivered ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("horizontalHideEnd=%1,%2")
             .arg(horizontalHideEnd.x(), 0, 'f', 2)
             .arg(horizontalHideEnd.y(), 0, 'f', 2)
      << QString("verticalHideEnd=%1,%2")
             .arg(verticalHideEnd.x(), 0, 'f', 2)
             .arg(verticalHideEnd.y(), 0, 'f', 2)
      << QString("rulerGuideVariantProbe=%1")
             .arg(variantOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-guide-variants"),
      QStringLiteral("ruler-guide-variants"), variantOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_ruler_guide_lines_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerGuideLineProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerGuideLineProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties* properties            = scene->getProperties();
  const std::vector<double> firstHGuides  = {-96.0, 132.0};
  const std::vector<double> firstVGuides  = {-88.0, 116.0};
  const std::vector<double> secondHGuides = {-156.0, 48.0};
  const std::vector<double> secondVGuides = {-136.0, 56.0};

  auto applyGuides = [&](const std::vector<double>& hGuides,
                         const std::vector<double>& vGuides) {
    properties->getHGuides().clear();
    properties->getVGuides().clear();
    for (double guide : hGuides) properties->getHGuides().push_back(guide);
    for (double guide : vGuides) properties->getVGuides().push_back(guide);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  };

  auto joinedGuides = [](const std::vector<double>& guides) {
    QStringList values;
    for (double guide : guides) values << QString::number(guide, 'f', 2);
    return values.join(',');
  };

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool rulerEnabled       = gui_smoke_set_view_ruler_overlay(true);
  applyGuides(firstHGuides, firstVGuides);

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  Ruler* horizontalRuler = nullptr;
  Ruler* verticalRuler   = nullptr;
  gui_smoke_resolve_visible_rulers(&horizontalRuler, &verticalRuler);
  const QImage guideDisabledFrame = gui_smoke_grab_viewer_frame(viewer);
  if (!horizontalRuler || !verticalRuler) {
    details << QString("scene=%1").arg(scenePath.getQString())
            << QString("viewGuideDisabled=%1")
                   .arg(guideDisabled ? QStringLiteral("true")
                                      : QStringLiteral("false"))
            << QString("viewRulerEnabled=%1")
                   .arg(rulerEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
            << QString("visibleRulerWidgetCount=%1")
                   .arg(gui_smoke_visible_ruler_count())
            << QStringLiteral("rulerGuideLineProbe=no-visible-rulers");
    details += gui_smoke_viewer_capture_details(
        viewer, guideDisabledFrame, QStringLiteral("viewer-ruler-guide-lines"),
        QStringLiteral("ruler-guide-lines"), false, 0, 0, 0, 1);
    return details;
  }

  const bool guideEnabled = gui_smoke_set_view_guide_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);
  const QImage firstFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeGuideLineStats firstStats =
      gui_smoke_analyze_guide_lines(firstFrame, guideDisabledFrame);

  auto verticalBandPixels = [&](const QImage& image, const QImage& baseline,
                                const std::vector<double>& guides) {
    int pixels = 0;
    for (double guide : guides) {
      const QPointF devicePoint =
          gui_smoke_world_to_viewer_device(viewer, TPointD(guide, 0.0));
      pixels += gui_smoke_count_changed_neutral_vertical_band(
          image, baseline, qRound(devicePoint.x()), 2);
    }
    return pixels;
  };
  auto horizontalBandPixels = [&](const QImage& image, const QImage& baseline,
                                  const std::vector<double>& guides) {
    int pixels = 0;
    for (double guide : guides) {
      const QPointF devicePoint =
          gui_smoke_world_to_viewer_device(viewer, TPointD(0.0, guide));
      pixels += gui_smoke_count_changed_neutral_horizontal_band(
          image, baseline, qRound(devicePoint.y()), 2);
    }
    return pixels;
  };

  const int firstVerticalGuideBandPixels =
      verticalBandPixels(firstFrame, guideDisabledFrame, firstHGuides);
  const int firstHorizontalGuideBandPixels =
      horizontalBandPixels(firstFrame, guideDisabledFrame, firstVGuides);

  applyGuides(secondHGuides, secondVGuides);
  horizontalRuler->update();
  verticalRuler->update();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);
  const QImage secondFrame = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeGuideLineStats secondStats =
      gui_smoke_analyze_guide_lines(secondFrame, guideDisabledFrame);
  const GuiSmokeGuideLineStats movedStats =
      gui_smoke_analyze_guide_lines(secondFrame, firstFrame);
  const int secondVerticalGuideBandPixels =
      verticalBandPixels(secondFrame, guideDisabledFrame, secondHGuides);
  const int secondHorizontalGuideBandPixels =
      horizontalBandPixels(secondFrame, guideDisabledFrame, secondVGuides);

  QString guideDisabledPath;
  const bool guideDisabledSaved = gui_smoke_save_capture(
      guideDisabledFrame,
      QStringLiteral("viewer-ruler-guide-lines-guide-disabled.png"),
      &guideDisabledPath);

  const bool firstGuideLinesOk =
      firstStats.changedNeutralPixels > 1000 &&
      firstStats.verticalLineColumns >= 2 &&
      firstStats.horizontalLineRows >= 2 &&
      firstStats.strongestVerticalLinePixels > 100 &&
      firstStats.strongestHorizontalLinePixels > 100 &&
      firstStats.strongestVerticalLineDashSegments >= 4 &&
      firstStats.strongestHorizontalLineDashSegments >= 4 &&
      firstVerticalGuideBandPixels > 100 &&
      firstHorizontalGuideBandPixels > 100;
  const bool secondGuideLinesOk =
      secondStats.changedNeutralPixels > 1000 &&
      secondStats.verticalLineColumns >= 2 &&
      secondStats.horizontalLineRows >= 2 &&
      secondStats.strongestVerticalLinePixels > 100 &&
      secondStats.strongestHorizontalLinePixels > 100 &&
      secondStats.strongestVerticalLineDashSegments >= 4 &&
      secondStats.strongestHorizontalLineDashSegments >= 4 &&
      secondVerticalGuideBandPixels > 100 &&
      secondHorizontalGuideBandPixels > 100;
  const bool movedGuideLinesOk = movedStats.changedNeutralPixels > 1000 &&
                                 movedStats.verticalLineColumns >= 2 &&
                                 movedStats.horizontalLineRows >= 2;
  const int hGuideCount = properties->getHGuides().size();
  const int vGuideCount = properties->getVGuides().size();
  const bool lineOk =
      cameraDisabled && safeAreaDisabled && fieldGuideDisabled &&
      guideDisabled && guideEnabled && rulerEnabled && guideDisabledSaved &&
      hGuideCount == static_cast<int>(secondHGuides.size()) &&
      vGuideCount == static_cast<int>(secondVGuides.size()) &&
      gui_smoke_visible_ruler_count() >= 2 && firstGuideLinesOk &&
      secondGuideLinesOk && movedGuideLinesOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("cameraOverlayDisabled=%1")
                 .arg(cameraDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
          << QString("safeAreaDisabled=%1")
                 .arg(safeAreaDisabled ? QStringLiteral("true")
                                       : QStringLiteral("false"))
          << QString("fieldGuideDisabled=%1")
                 .arg(fieldGuideDisabled ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("viewGuideDisabled=%1")
                 .arg(guideDisabled ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("viewGuideEnabled=%1")
                 .arg(guideEnabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("viewRulerEnabled=%1")
                 .arg(rulerEnabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
          << QString("visibleRulerWidgetCount=%1")
                 .arg(gui_smoke_visible_ruler_count())
          << QString("firstHGuides=%1").arg(joinedGuides(firstHGuides))
          << QString("firstVGuides=%1").arg(joinedGuides(firstVGuides))
          << QString("secondHGuides=%1").arg(joinedGuides(secondHGuides))
          << QString("secondVGuides=%1").arg(joinedGuides(secondVGuides))
          << QString("hGuideCount=%1").arg(hGuideCount)
          << QString("vGuideCount=%1").arg(vGuideCount)
          << QString("firstGuideLineChangedNeutralPixels=%1")
                 .arg(firstStats.changedNeutralPixels)
          << QString("firstVerticalGuideLineColumns=%1")
                 .arg(firstStats.verticalLineColumns)
          << QString("firstHorizontalGuideLineRows=%1")
                 .arg(firstStats.horizontalLineRows)
          << QString("firstStrongestVerticalGuideLinePixels=%1")
                 .arg(firstStats.strongestVerticalLinePixels)
          << QString("firstStrongestHorizontalGuideLinePixels=%1")
                 .arg(firstStats.strongestHorizontalLinePixels)
          << QString("firstVerticalGuideLineDashSegments=%1")
                 .arg(firstStats.strongestVerticalLineDashSegments)
          << QString("firstHorizontalGuideLineDashSegments=%1")
                 .arg(firstStats.strongestHorizontalLineDashSegments)
          << QString("firstVerticalGuideBandPixels=%1")
                 .arg(firstVerticalGuideBandPixels)
          << QString("firstHorizontalGuideBandPixels=%1")
                 .arg(firstHorizontalGuideBandPixels)
          << QString("secondGuideLineChangedNeutralPixels=%1")
                 .arg(secondStats.changedNeutralPixels)
          << QString("secondVerticalGuideLineColumns=%1")
                 .arg(secondStats.verticalLineColumns)
          << QString("secondHorizontalGuideLineRows=%1")
                 .arg(secondStats.horizontalLineRows)
          << QString("secondStrongestVerticalGuideLinePixels=%1")
                 .arg(secondStats.strongestVerticalLinePixels)
          << QString("secondStrongestHorizontalGuideLinePixels=%1")
                 .arg(secondStats.strongestHorizontalLinePixels)
          << QString("secondVerticalGuideLineDashSegments=%1")
                 .arg(secondStats.strongestVerticalLineDashSegments)
          << QString("secondHorizontalGuideLineDashSegments=%1")
                 .arg(secondStats.strongestHorizontalLineDashSegments)
          << QString("secondVerticalGuideBandPixels=%1")
                 .arg(secondVerticalGuideBandPixels)
          << QString("secondHorizontalGuideBandPixels=%1")
                 .arg(secondHorizontalGuideBandPixels)
          << QString("movedGuideLineChangedNeutralPixels=%1")
                 .arg(movedStats.changedNeutralPixels)
          << QString("movedVerticalGuideLineColumns=%1")
                 .arg(movedStats.verticalLineColumns)
          << QString("movedHorizontalGuideLineRows=%1")
                 .arg(movedStats.horizontalLineRows)
          << QString("guideDisabledCaptureSaved=%1")
                 .arg(guideDisabledSaved ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("guideDisabledCapturePath=%1")
                 .arg(gui_smoke_status_value(guideDisabledPath))
          << QString("rulerGuideLineProbe=%1")
                 .arg(lineOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, firstFrame, QStringLiteral("viewer-ruler-guide-lines"),
      QStringLiteral("ruler-guide-lines"), lineOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_ruler_guide_styles_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerStyleProbe=no-viewer")
            << QStringLiteral("rulerWidgetHighDpiProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerStyleProbe=scene-folder-error")
            << QStringLiteral("rulerWidgetHighDpiProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties* properties = scene->getProperties();
  properties->getHGuides().clear();
  properties->getVGuides().clear();
  properties->getHGuides().push_back(0.0);
  properties->getVGuides().push_back(0.0);

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled     = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled   = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideDisabled      = gui_smoke_set_view_guide_overlay(false);
  const bool rulerEnabled       = gui_smoke_set_view_ruler_overlay(true);

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  Ruler* horizontalRuler = nullptr;
  Ruler* verticalRuler   = nullptr;
  gui_smoke_resolve_visible_rulers(&horizontalRuler, &verticalRuler);
  if (!horizontalRuler || !verticalRuler) {
    const QImage before = gui_smoke_grab_viewer_frame(viewer);
    details << QString("scene=%1").arg(scenePath.getQString())
            << QString("viewGuideDisabled=%1")
                   .arg(guideDisabled ? QStringLiteral("true")
                                      : QStringLiteral("false"))
            << QString("viewRulerEnabled=%1")
                   .arg(rulerEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
            << QString("visibleRulerWidgetCount=%1")
                   .arg(gui_smoke_visible_ruler_count())
            << QStringLiteral("rulerStyleProbe=no-visible-rulers")
            << QStringLiteral("rulerWidgetHighDpiProbe=no-visible-rulers");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-ruler-guide-styles"),
        QStringLiteral("ruler-guide-styles"), false, 0, 0, 0, 1);
    return details;
  }

  const QImage before           = gui_smoke_grab_viewer_frame(viewer);
  const QImage horizontalBefore = gui_smoke_grab_widget_frame(horizontalRuler);
  const QImage verticalBefore   = gui_smoke_grab_widget_frame(verticalRuler);
  const GuiSmokeWidgetImageStats horizontalBeforeStats =
      gui_smoke_analyze_widget_frame(horizontalBefore);
  const GuiSmokeWidgetImageStats verticalBeforeStats =
      gui_smoke_analyze_widget_frame(verticalBefore);

  const QColor customBackground(21, 94, 117);
  const QColor customScale(250, 204, 21);
  const QColor customBorder(239, 68, 68);
  const QColor customHandle(34, 197, 94);
  const QColor customHandleDrag(249, 115, 22);
  const QString customStyle = QStringLiteral(
                                  "Ruler {"
                                  " qproperty-ParentBGColor: %1;"
                                  " qproperty-ScaleColor: %2;"
                                  " qproperty-BorderColor: %3;"
                                  " qproperty-HandleColor: %4;"
                                  " qproperty-HandleDragColor: %5;"
                                  " }")
                                  .arg(customBackground.name())
                                  .arg(customScale.name())
                                  .arg(customBorder.name())
                                  .arg(customHandle.name())
                                  .arg(customHandleDrag.name());
  horizontalRuler->setStyleSheet(customStyle);
  verticalRuler->setStyleSheet(customStyle);
  horizontalRuler->ensurePolished();
  verticalRuler->ensurePolished();
  horizontalRuler->style()->unpolish(horizontalRuler);
  horizontalRuler->style()->polish(horizontalRuler);
  verticalRuler->style()->unpolish(verticalRuler);
  verticalRuler->style()->polish(verticalRuler);

  const bool guideEnabled = gui_smoke_set_view_guide_overlay(true);
  viewer->GLInvalidateAll();
  horizontalRuler->update();
  verticalRuler->update();
  gui_smoke_pump_events(150);

  const QImage horizontalAfter = gui_smoke_grab_widget_frame(horizontalRuler);
  const QImage verticalAfter   = gui_smoke_grab_widget_frame(verticalRuler);
  const GuiSmokeWidgetImageStats horizontalAfterStats =
      gui_smoke_analyze_widget_frame(horizontalAfter, horizontalBefore);
  const GuiSmokeWidgetImageStats verticalAfterStats =
      gui_smoke_analyze_widget_frame(verticalAfter, verticalBefore);

  QString horizontalBeforePath;
  QString horizontalAfterPath;
  QString verticalBeforePath;
  QString verticalAfterPath;
  const bool horizontalBeforeSaved = gui_smoke_save_capture(
      horizontalBefore,
      QStringLiteral("viewer-ruler-guide-styles-horizontal-before.png"),
      &horizontalBeforePath);
  const bool horizontalAfterSaved = gui_smoke_save_capture(
      horizontalAfter,
      QStringLiteral("viewer-ruler-guide-styles-horizontal-after.png"),
      &horizontalAfterPath);
  const bool verticalBeforeSaved = gui_smoke_save_capture(
      verticalBefore,
      QStringLiteral("viewer-ruler-guide-styles-vertical-before.png"),
      &verticalBeforePath);
  const bool verticalAfterSaved = gui_smoke_save_capture(
      verticalAfter,
      QStringLiteral("viewer-ruler-guide-styles-vertical-after.png"),
      &verticalAfterPath);

  const int horizontalBackgroundBefore =
      gui_smoke_count_hue_pixels(horizontalBefore, customBackground);
  const int horizontalBackgroundAfter =
      gui_smoke_count_hue_pixels(horizontalAfter, customBackground);
  const int verticalBackgroundBefore =
      gui_smoke_count_hue_pixels(verticalBefore, customBackground);
  const int verticalBackgroundAfter =
      gui_smoke_count_hue_pixels(verticalAfter, customBackground);
  const int horizontalHandleBefore =
      gui_smoke_count_hue_pixels(horizontalBefore, customHandle);
  const int horizontalHandleAfter =
      gui_smoke_count_hue_pixels(horizontalAfter, customHandle);
  const int verticalHandleBefore =
      gui_smoke_count_hue_pixels(verticalBefore, customHandle);
  const int verticalHandleAfter =
      gui_smoke_count_hue_pixels(verticalAfter, customHandle);
  const int horizontalScaleAfter =
      gui_smoke_count_hue_pixels(horizontalAfter, customScale);
  const int verticalScaleAfter =
      gui_smoke_count_hue_pixels(verticalAfter, customScale);

  const bool horizontalHighDpiOk =
      gui_smoke_widget_high_dpi_ok(horizontalRuler, horizontalBefore) &&
      gui_smoke_widget_high_dpi_ok(horizontalRuler, horizontalAfter);
  const bool verticalHighDpiOk =
      gui_smoke_widget_high_dpi_ok(verticalRuler, verticalBefore) &&
      gui_smoke_widget_high_dpi_ok(verticalRuler, verticalAfter);
  const bool rulerHighDpiOk = horizontalHighDpiOk && verticalHighDpiOk;
  const bool savedOk        = horizontalBeforeSaved && horizontalAfterSaved &&
                       verticalBeforeSaved && verticalAfterSaved;
  const bool stylePixelsOk =
      horizontalAfterStats.changedPixels > 1000 &&
      verticalAfterStats.changedPixels > 1000 &&
      horizontalBackgroundAfter > horizontalBackgroundBefore + 1000 &&
      verticalBackgroundAfter > verticalBackgroundBefore + 1000 &&
      horizontalHandleAfter > horizontalHandleBefore + 20 &&
      verticalHandleAfter > verticalHandleBefore + 20 &&
      horizontalScaleAfter > 20 && verticalScaleAfter > 20;
  const int hGuideCount = properties->getHGuides().size();
  const int vGuideCount = properties->getVGuides().size();
  const bool overlayOk  = cameraDisabled && safeAreaDisabled &&
                         fieldGuideDisabled && guideDisabled && guideEnabled &&
                         rulerEnabled && hGuideCount > 0 && vGuideCount > 0 &&
                         gui_smoke_visible_ruler_count() >= 2;
  const bool styleOk = overlayOk && stylePixelsOk && rulerHighDpiOk && savedOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("rulerStyleContent=qss-qproperty")
      << QString("cameraOverlayDisabled=%1")
             .arg(cameraDisabled ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("safeAreaDisabled=%1")
             .arg(safeAreaDisabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
      << QString("fieldGuideDisabled=%1")
             .arg(fieldGuideDisabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("viewGuideDisabled=%1")
             .arg(guideDisabled ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("viewGuideEnabled=%1")
             .arg(guideEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("viewRulerEnabled=%1")
             .arg(rulerEnabled ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("hGuideCount=%1").arg(hGuideCount)
      << QString("vGuideCount=%1").arg(vGuideCount)
      << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
      << QString("visibleRulerWidgetCount=%1")
             .arg(gui_smoke_visible_ruler_count())
      << QString("horizontalRulerLogicalSize=%1x%2")
             .arg(horizontalRuler->width())
             .arg(horizontalRuler->height())
      << QString("verticalRulerLogicalSize=%1x%2")
             .arg(verticalRuler->width())
             .arg(verticalRuler->height())
      << QString("horizontalRulerCaptureSizeBefore=%1x%2")
             .arg(horizontalBefore.width())
             .arg(horizontalBefore.height())
      << QString("horizontalRulerCaptureSizeAfter=%1x%2")
             .arg(horizontalAfter.width())
             .arg(horizontalAfter.height())
      << QString("verticalRulerCaptureSizeBefore=%1x%2")
             .arg(verticalBefore.width())
             .arg(verticalBefore.height())
      << QString("verticalRulerCaptureSizeAfter=%1x%2")
             .arg(verticalAfter.width())
             .arg(verticalAfter.height())
      << QString("rulerWidgetDevicePixelRatio=%1")
             .arg(horizontalRuler->devicePixelRatioF(), 0, 'f', 2)
      << QString("rulerStyleBackgroundColor=%1").arg(customBackground.name())
      << QString("rulerStyleScaleColor=%1").arg(customScale.name())
      << QString("rulerStyleHandleColor=%1").arg(customHandle.name())
      << QString("horizontalRulerStyleBackgroundPixels=%1->%2")
             .arg(horizontalBackgroundBefore)
             .arg(horizontalBackgroundAfter)
      << QString("verticalRulerStyleBackgroundPixels=%1->%2")
             .arg(verticalBackgroundBefore)
             .arg(verticalBackgroundAfter)
      << QString("horizontalRulerStyleHandlePixels=%1->%2")
             .arg(horizontalHandleBefore)
             .arg(horizontalHandleAfter)
      << QString("verticalRulerStyleHandlePixels=%1->%2")
             .arg(verticalHandleBefore)
             .arg(verticalHandleAfter)
      << QString("horizontalRulerStyleScalePixels=%1").arg(horizontalScaleAfter)
      << QString("verticalRulerStyleScalePixels=%1").arg(verticalScaleAfter)
      << QString("horizontalRulerChangedPixels=%1")
             .arg(horizontalAfterStats.changedPixels)
      << QString("verticalRulerChangedPixels=%1")
             .arg(verticalAfterStats.changedPixels)
      << QString("horizontalRulerDistinctColors=%1->%2")
             .arg(horizontalBeforeStats.distinctColors)
             .arg(horizontalAfterStats.distinctColors)
      << QString("verticalRulerDistinctColors=%1->%2")
             .arg(verticalBeforeStats.distinctColors)
             .arg(verticalAfterStats.distinctColors)
      << QString("horizontalRulerBeforeCaptureSaved=%1")
             .arg(horizontalBeforeSaved ? QStringLiteral("true")
                                        : QStringLiteral("false"))
      << QString("horizontalRulerAfterCaptureSaved=%1")
             .arg(horizontalAfterSaved ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("verticalRulerBeforeCaptureSaved=%1")
             .arg(verticalBeforeSaved ? QStringLiteral("true")
                                      : QStringLiteral("false"))
      << QString("verticalRulerAfterCaptureSaved=%1")
             .arg(verticalAfterSaved ? QStringLiteral("true")
                                     : QStringLiteral("false"))
      << QString("horizontalRulerBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(horizontalBeforePath))
      << QString("horizontalRulerAfterCapturePath=%1")
             .arg(gui_smoke_status_value(horizontalAfterPath))
      << QString("verticalRulerBeforeCapturePath=%1")
             .arg(gui_smoke_status_value(verticalBeforePath))
      << QString("verticalRulerAfterCapturePath=%1")
             .arg(gui_smoke_status_value(verticalAfterPath))
      << QString("rulerWidgetHighDpiProbe=%1")
             .arg(rulerHighDpiOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"))
      << QString("rulerStyleProbe=%1")
             .arg(styleOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-guide-styles"),
      QStringLiteral("ruler-guide-styles"), styleOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_ruler_ticks_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("rulerTickProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("rulerTickProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_ruler_ticks");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("rulerTickProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("rulerTickProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  const bool guideDisabled = gui_smoke_set_view_guide_overlay(false);
  const bool rulerEnabled  = gui_smoke_set_view_ruler_overlay(true);

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  Ruler* horizontalRuler = nullptr;
  Ruler* verticalRuler   = nullptr;
  gui_smoke_resolve_visible_rulers(&horizontalRuler, &verticalRuler);
  if (!horizontalRuler || !verticalRuler) {
    const QImage before = gui_smoke_grab_viewer_frame(viewer);
    details << QString("scene=%1").arg(scenePath.getQString())
            << QString("viewGuideDisabled=%1")
                   .arg(guideDisabled ? QStringLiteral("true")
                                      : QStringLiteral("false"))
            << QString("viewRulerEnabled=%1")
                   .arg(rulerEnabled ? QStringLiteral("true")
                                     : QStringLiteral("false"))
            << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
            << QString("visibleRulerWidgetCount=%1")
                   .arg(gui_smoke_visible_ruler_count())
            << QStringLiteral("rulerTickProbe=no-visible-rulers")
            << QStringLiteral("rulerWidgetHighDpiProbe=no-visible-rulers");
    details += gui_smoke_viewer_capture_details(
        viewer, before, QStringLiteral("viewer-ruler-ticks"),
        QStringLiteral("raster-ruler-ticks"), false);
    return details;
  }

  const QImage before           = gui_smoke_grab_viewer_frame(viewer);
  const QImage horizontalBefore = gui_smoke_grab_widget_frame(horizontalRuler);
  const QImage verticalBefore   = gui_smoke_grab_widget_frame(verticalRuler);
  const GuiSmokeWidgetImageStats horizontalBeforeStats =
      gui_smoke_analyze_widget_frame(horizontalBefore);
  const GuiSmokeWidgetImageStats verticalBeforeStats =
      gui_smoke_analyze_widget_frame(verticalBefore);
  const double horizontalUnitBefore = horizontalRuler->getUnit();
  const double verticalUnitBefore   = verticalRuler->getUnit();
  const double horizontalPanBefore  = horizontalRuler->getPan();
  const double verticalPanBefore    = verticalRuler->getPan();

  const TAffine beforeAff  = viewer->getViewMatrix();
  const double beforeScale = std::sqrt(std::abs(beforeAff.det()));
  const int devPixRatio    = std::max(1, viewer->getDevPixRatio());
  const double zoomFactor  = 1.45;
  const QPointF panDelta(64.0 * devPixRatio, -48.0 * devPixRatio);
  viewer->setViewMatrix(TTranslation(panDelta.x(), -panDelta.y()) *
                            TScale(zoomFactor) * beforeAff,
                        SceneViewer::SCENE_VIEWMODE);
  viewer->GLInvalidateAll();
  horizontalRuler->update();
  verticalRuler->update();
  gui_smoke_pump_events(150);

  const QImage horizontalAfter = gui_smoke_grab_widget_frame(horizontalRuler);
  const QImage verticalAfter   = gui_smoke_grab_widget_frame(verticalRuler);
  const GuiSmokeWidgetImageStats horizontalAfterStats =
      gui_smoke_analyze_widget_frame(horizontalAfter, horizontalBefore);
  const GuiSmokeWidgetImageStats verticalAfterStats =
      gui_smoke_analyze_widget_frame(verticalAfter, verticalBefore);
  const double horizontalUnitAfter = horizontalRuler->getUnit();
  const double verticalUnitAfter   = verticalRuler->getUnit();
  const double horizontalPanAfter  = horizontalRuler->getPan();
  const double verticalPanAfter    = verticalRuler->getPan();
  const TAffine afterAff           = viewer->getViewMatrix();
  const double afterScale          = std::sqrt(std::abs(afterAff.det()));

  QString horizontalBeforePath;
  QString horizontalAfterPath;
  QString verticalBeforePath;
  QString verticalAfterPath;
  const bool horizontalBeforeSaved = gui_smoke_save_capture(
      horizontalBefore,
      QStringLiteral("viewer-ruler-ticks-horizontal-before.png"),
      &horizontalBeforePath);
  const bool horizontalAfterSaved = gui_smoke_save_capture(
      horizontalAfter,
      QStringLiteral("viewer-ruler-ticks-horizontal-after.png"),
      &horizontalAfterPath);
  const bool verticalBeforeSaved = gui_smoke_save_capture(
      verticalBefore, QStringLiteral("viewer-ruler-ticks-vertical-before.png"),
      &verticalBeforePath);
  const bool verticalAfterSaved = gui_smoke_save_capture(
      verticalAfter, QStringLiteral("viewer-ruler-ticks-vertical-after.png"),
      &verticalAfterPath);

  const bool horizontalHighDpiOk =
      gui_smoke_widget_high_dpi_ok(horizontalRuler, horizontalBefore) &&
      gui_smoke_widget_high_dpi_ok(horizontalRuler, horizontalAfter);
  const bool verticalHighDpiOk =
      gui_smoke_widget_high_dpi_ok(verticalRuler, verticalBefore) &&
      gui_smoke_widget_high_dpi_ok(verticalRuler, verticalAfter);
  const bool rulerHighDpiOk = horizontalHighDpiOk && verticalHighDpiOk;
  const bool rulerPixelsOk  = horizontalBeforeStats.nonBackgroundPixels > 20 &&
                             horizontalAfterStats.nonBackgroundPixels > 20 &&
                             verticalBeforeStats.nonBackgroundPixels > 20 &&
                             verticalAfterStats.nonBackgroundPixels > 20 &&
                             horizontalAfterStats.changedPixels > 20 &&
                             verticalAfterStats.changedPixels > 20;
  const bool rulerTransformOk =
      afterAff != beforeAff && afterScale > beforeScale &&
      !gui_smoke_near(horizontalUnitBefore, horizontalUnitAfter, 0.01) &&
      !gui_smoke_near(verticalUnitBefore, verticalUnitAfter, 0.01) &&
      std::abs(horizontalPanAfter - horizontalPanBefore) > 0.5 &&
      std::abs(verticalPanAfter - verticalPanBefore) > 0.5;
  const bool savedOk = horizontalBeforeSaved && horizontalAfterSaved &&
                       verticalBeforeSaved && verticalAfterSaved;
  const bool tickOk = guideDisabled && rulerEnabled && rulerPixelsOk &&
                      rulerTransformOk && rulerHighDpiOk && savedOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("rulerTickContent=raster-transform")
          << QString("viewGuideDisabled=%1")
                 .arg(guideDisabled ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("viewRulerEnabled=%1")
                 .arg(rulerEnabled ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("rulerWidgetCount=%1").arg(gui_smoke_ruler_count())
          << QString("visibleRulerWidgetCount=%1")
                 .arg(gui_smoke_visible_ruler_count())
          << QString("horizontalRulerLogicalSize=%1x%2")
                 .arg(horizontalRuler->width())
                 .arg(horizontalRuler->height())
          << QString("verticalRulerLogicalSize=%1x%2")
                 .arg(verticalRuler->width())
                 .arg(verticalRuler->height())
          << QString("horizontalRulerCaptureSizeBefore=%1x%2")
                 .arg(horizontalBefore.width())
                 .arg(horizontalBefore.height())
          << QString("horizontalRulerCaptureSizeAfter=%1x%2")
                 .arg(horizontalAfter.width())
                 .arg(horizontalAfter.height())
          << QString("verticalRulerCaptureSizeBefore=%1x%2")
                 .arg(verticalBefore.width())
                 .arg(verticalBefore.height())
          << QString("verticalRulerCaptureSizeAfter=%1x%2")
                 .arg(verticalAfter.width())
                 .arg(verticalAfter.height())
          << QString("rulerWidgetDevicePixelRatio=%1")
                 .arg(horizontalRuler->devicePixelRatioF(), 0, 'f', 2)
          << QString("horizontalRulerUnit=%1->%2")
                 .arg(horizontalUnitBefore, 0, 'f', 4)
                 .arg(horizontalUnitAfter, 0, 'f', 4)
          << QString("verticalRulerUnit=%1->%2")
                 .arg(verticalUnitBefore, 0, 'f', 4)
                 .arg(verticalUnitAfter, 0, 'f', 4)
          << QString("horizontalRulerPan=%1->%2")
                 .arg(horizontalPanBefore, 0, 'f', 4)
                 .arg(horizontalPanAfter, 0, 'f', 4)
          << QString("verticalRulerPan=%1->%2")
                 .arg(verticalPanBefore, 0, 'f', 4)
                 .arg(verticalPanAfter, 0, 'f', 4)
          << QString("horizontalRulerTickPixelsBefore=%1")
                 .arg(horizontalBeforeStats.nonBackgroundPixels)
          << QString("horizontalRulerTickPixelsAfter=%1")
                 .arg(horizontalAfterStats.nonBackgroundPixels)
          << QString("horizontalRulerChangedPixels=%1")
                 .arg(horizontalAfterStats.changedPixels)
          << QString("horizontalRulerDistinctColors=%1->%2")
                 .arg(horizontalBeforeStats.distinctColors)
                 .arg(horizontalAfterStats.distinctColors)
          << QString("verticalRulerTickPixelsBefore=%1")
                 .arg(verticalBeforeStats.nonBackgroundPixels)
          << QString("verticalRulerTickPixelsAfter=%1")
                 .arg(verticalAfterStats.nonBackgroundPixels)
          << QString("verticalRulerChangedPixels=%1")
                 .arg(verticalAfterStats.changedPixels)
          << QString("verticalRulerDistinctColors=%1->%2")
                 .arg(verticalBeforeStats.distinctColors)
                 .arg(verticalAfterStats.distinctColors)
          << QString("horizontalRulerBeforeCaptureSaved=%1")
                 .arg(horizontalBeforeSaved ? QStringLiteral("true")
                                            : QStringLiteral("false"))
          << QString("horizontalRulerAfterCaptureSaved=%1")
                 .arg(horizontalAfterSaved ? QStringLiteral("true")
                                           : QStringLiteral("false"))
          << QString("verticalRulerBeforeCaptureSaved=%1")
                 .arg(verticalBeforeSaved ? QStringLiteral("true")
                                          : QStringLiteral("false"))
          << QString("verticalRulerAfterCaptureSaved=%1")
                 .arg(verticalAfterSaved ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("horizontalRulerBeforeCapturePath=%1")
                 .arg(gui_smoke_status_value(horizontalBeforePath))
          << QString("horizontalRulerAfterCapturePath=%1")
                 .arg(gui_smoke_status_value(horizontalAfterPath))
          << QString("verticalRulerBeforeCapturePath=%1")
                 .arg(gui_smoke_status_value(verticalBeforePath))
          << QString("verticalRulerAfterCapturePath=%1")
                 .arg(gui_smoke_status_value(verticalAfterPath))
          << QString("rulerWidgetHighDpiProbe=%1")
                 .arg(rulerHighDpiOk ? QStringLiteral("ok")
                                     : QStringLiteral("error"))
          << QString("rulerTransformProbe=%1")
                 .arg(rulerTransformOk ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
          << QString("rulerTickProbe=%1")
                 .arg(tickOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-ticks"),
      QStringLiteral("raster-ruler-ticks"), tickOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_overlay_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolOverlayProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolOverlayProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolOverlayProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolOverlayProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolOverlayProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Hand);
  TTool* baselineTool = toolHandle->getTool();
  if (!baselineTool || baselineTool->getName() != T_Hand) {
    details << QStringLiteral("viewerRenderProbe=baseline-tool-error")
            << QStringLiteral("toolOverlayProbe=baseline-tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  baselineTool->setViewer(viewer);
  baselineTool->updateMatrix();
  baselineTool->updateEnabled();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolOverlayProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const TStageObjectId currentObjectId =
      TApp::instance()->getCurrentObject()->getObjectId();
  const bool objectOk =
      currentObjectId == objectId && currentObjectId.isColumn();
  const bool toolOk = tool->isEnabled() && objectOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(currentObjectId.toString()))
          << QString("toolOverlayProbe=%1")
                 .arg(toolOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-overlay"),
      QStringLiteral("animate-tool-overlay"), toolOk);

  return details;
}

static TMouseEvent gui_smoke_tool_mouse_event(Qt::MouseButton button,
                                              Qt::MouseButtons buttons) {
  TMouseEvent event;
  event.m_pressure = 1.0;
  event.m_button   = button;
  event.m_buttons  = buttons;
  return event;
}

static QStringList gui_smoke_viewer_animate_tool_drag_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolTransformProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolTransformProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_drag");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("toolTransformProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolTransformProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolTransformProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const double beforeX          = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY          = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine beforePlacement = xsheet->getPlacement(objectId, 0);

  const TPointD dragStart(0.0, 0.0);
  const TPointD dragMid(72.0, -36.0);
  const TPointD dragEnd(144.0, -72.0);
  const TMouseEvent downEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::LeftButton);
  const TMouseEvent dragEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::LeftButton);
  const TMouseEvent upEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::NoButton);

  toolHandle->setToolBusy(true);
  tool->leftButtonDown(dragStart, downEvent);
  gui_smoke_pump_events(30);
  tool->leftButtonDrag(dragMid, dragEvent);
  gui_smoke_pump_events(30);
  tool->leftButtonDrag(dragEnd, dragEvent);
  gui_smoke_pump_events(30);
  tool->leftButtonUp(dragEnd, upEvent);
  toolHandle->setToolBusy(false);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const double afterX          = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY          = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement = xsheet->getPlacement(objectId, 0);
  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk = objectOk && beforePlacement != afterPlacement &&
                           (std::abs(afterX - beforeX) > 0.0001 ||
                            std::abs(afterY - beforeY) > 0.0001);

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(objectId.toString()))
          << QString("stageObjectXBefore=%1").arg(beforeX, 0, 'f', 4)
          << QString("stageObjectYBefore=%1").arg(beforeY, 0, 'f', 4)
          << QString("stageObjectXAfter=%1").arg(afterX, 0, 'f', 4)
          << QString("stageObjectYAfter=%1").arg(afterY, 0, 'f', 4)
          << QString("toolDragStart=%1,%2")
                 .arg(dragStart.x, 0, 'f', 1)
                 .arg(dragStart.y, 0, 'f', 1)
          << QString("toolDragEnd=%1,%2")
                 .arg(dragEnd.x, 0, 'f', 1)
                 .arg(dragEnd.y, 0, 'f', 1)
          << QString("toolTransformProbe=%1")
                 .arg(transformOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-drag"),
      QStringLiteral("animate-tool-drag"), transformOk);

  return details;
}

enum class GuiSmokeRasterBrushInput {
  DirectTool,
  MouseEvents,
  TabletEvents,
  SystemMouseEvents
};

static QString gui_smoke_raster_brush_input_name(
    GuiSmokeRasterBrushInput input) {
  switch (input) {
  case GuiSmokeRasterBrushInput::DirectTool:
    return QStringLiteral("direct-tool");
  case GuiSmokeRasterBrushInput::MouseEvents:
    return QStringLiteral("qt-mouse-events");
  case GuiSmokeRasterBrushInput::TabletEvents:
    return QStringLiteral("qt-tablet-events");
  case GuiSmokeRasterBrushInput::SystemMouseEvents:
    return QStringLiteral("system-mouse-events");
  }
  return QStringLiteral("unknown");
}

static std::vector<TPointD> gui_smoke_tool_stroke_points() {
  return {TPointD(-190.0, -90.0), TPointD(-110.0, 55.0), TPointD(0.0, 130.0),
          TPointD(110.0, 55.0), TPointD(190.0, -90.0)};
}

static void gui_smoke_replay_tool_stroke(ToolHandle* toolHandle, TTool* tool) {
  const TMouseEvent downEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::LeftButton);
  const TMouseEvent dragEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::LeftButton);
  const TMouseEvent upEvent =
      gui_smoke_tool_mouse_event(Qt::LeftButton, Qt::NoButton);
  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();

  toolHandle->setToolBusy(true);
  tool->setCanUndo(false);
  tool->leftButtonDown(strokePoints.front(), downEvent);
  for (size_t i = 1; i + 1 < strokePoints.size(); ++i) {
    gui_smoke_pump_events(10);
    tool->leftButtonDrag(strokePoints[i], dragEvent);
  }
  gui_smoke_pump_events(10);
  tool->leftButtonUp(strokePoints.back(), upEvent);
  tool->setCanUndo(true);
  toolHandle->setToolBusy(false);
  gui_smoke_pump_events(100);
}

static QPointF gui_smoke_world_to_viewer_local(SceneViewer* viewer,
                                               const TPointD& worldPos) {
  if (!viewer) return QPointF();

  const TPointD viewPos    = viewer->getViewMatrix() * worldPos;
  const double devPixRatio = std::max(1, viewer->getDevPixRatio());
  return QPointF((viewer->width() / 2.0 + viewPos.x) / devPixRatio,
                 (viewer->height() / 2.0 - viewPos.y) / devPixRatio);
}

static QPointF gui_smoke_world_to_viewer_device(SceneViewer* viewer,
                                                const TPointD& worldPos) {
  if (!viewer) return QPointF();

  const TPointD viewPos = viewer->getViewMatrix() * worldPos;
  return QPointF(viewer->width() / 2.0 + viewPos.x,
                 viewer->height() / 2.0 - viewPos.y);
}

static QString gui_smoke_joined_screen_points(
    const std::vector<QPointF>& points) {
  QStringList values;
  for (const QPointF& point : points) {
    values << QString("%1,%2")
                  .arg(std::round(point.x()))
                  .arg(std::round(point.y()));
  }
  return values.join('|');
}

static std::vector<QPointF> gui_smoke_tool_screen_points(SceneViewer* viewer) {
  std::vector<QPointF> globalPoints;
  if (!viewer) return globalPoints;

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  globalPoints.reserve(strokePoints.size());
  for (const TPointD& point : strokePoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) {
      globalPoints.clear();
      return globalPoints;
    }
    globalPoints.push_back(viewer->mapToGlobal(localPoint.toPoint()));
  }
  return globalPoints;
}

static int gui_smoke_external_input_timeout_ms() {
  bool ok           = false;
  const int timeout = qEnvironmentVariableIntValue(
      "OPENTOONZ_GUI_SMOKE_EXTERNAL_INPUT_TIMEOUT_MS", &ok);
  return ok && timeout > 0 ? timeout : 30000;
}

class GuiSmokeSystemMouseEventProbe final : public QObject {
  SceneViewer* m_viewer = nullptr;

public:
  int appEventCount         = 0;
  int viewerEventCount      = 0;
  int spontaneousEventCount = 0;
  int pressCount            = 0;
  int moveCount             = 0;
  int releaseCount          = 0;
  QString lastLocalPoint    = QStringLiteral("none");
  QString lastGlobalPoint   = QStringLiteral("none");
  QString lastTarget        = QStringLiteral("none");

  explicit GuiSmokeSystemMouseEventProbe(SceneViewer* viewer)
      : m_viewer(viewer) {}

  bool eventFilter(QObject* watched, QEvent* event) override {
    if (!event) return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
      ++pressCount;
      break;
    case QEvent::MouseMove:
      ++moveCount;
      break;
    case QEvent::MouseButtonRelease:
      ++releaseCount;
      break;
    case QEvent::MouseButtonDblClick:
      break;
    default:
      return QObject::eventFilter(watched, event);
    }

    ++appEventCount;
    if (event->spontaneous()) ++spontaneousEventCount;

    QWidget* widget = qobject_cast<QWidget*>(watched);
    if (widget && m_viewer &&
        (widget == m_viewer || m_viewer->isAncestorOf(widget))) {
      ++viewerEventCount;
    }

    QMouseEvent* mouseEvent   = static_cast<QMouseEvent*>(event);
    const QPointF localPoint  = QtCompat::mouseEventPositionF(mouseEvent);
    const QPointF globalPoint = QtCompat::mouseEventGlobalPosition(mouseEvent);
    lastLocalPoint            = QString("%1,%2")
                         .arg(localPoint.x(), 0, 'f', 2)
                         .arg(localPoint.y(), 0, 'f', 2);
    lastGlobalPoint = QString("%1,%2")
                          .arg(globalPoint.x(), 0, 'f', 2)
                          .arg(globalPoint.y(), 0, 'f', 2);
    lastTarget = watched && watched->metaObject()
                     ? QString::fromLatin1(watched->metaObject()->className())
                     : QStringLiteral("unknown");

    return QObject::eventFilter(watched, event);
  }

  QStringList details() const {
    return QStringList()
           << QString("systemMouseAppEventCount=%1").arg(appEventCount)
           << QString("systemMouseViewerEventCount=%1").arg(viewerEventCount)
           << QString("systemMouseSpontaneousEventCount=%1")
                  .arg(spontaneousEventCount)
           << QString("systemMousePressCount=%1").arg(pressCount)
           << QString("systemMouseMoveCount=%1").arg(moveCount)
           << QString("systemMouseReleaseCount=%1").arg(releaseCount)
           << QString("systemMouseLastLocalPoint=%1").arg(lastLocalPoint)
           << QString("systemMouseLastGlobalPoint=%1").arg(lastGlobalPoint)
           << QString("systemMouseLastTarget=%1")
                  .arg(gui_smoke_status_value(lastTarget));
  }
};

static bool gui_smoke_send_widget_mouse_event(
    QWidget* widget, QEvent::Type type, const QPointF& localPos,
    Qt::MouseButton button, Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  if (!widget) return false;

  const QPointF globalPos = widget->mapToGlobal(localPos.toPoint());
  QMouseEvent event       = QtCompat::makeMouseEvent(type, localPos, globalPos,
                                                     button, buttons, modifiers);
  const bool delivered    = QApplication::sendEvent(widget, &event);
  gui_smoke_pump_events(25);
  return delivered;
}

static bool gui_smoke_send_viewer_mouse_event(
    SceneViewer* viewer, QEvent::Type type, const QPointF& localPos,
    Qt::MouseButton button, Qt::MouseButtons buttons,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  return gui_smoke_send_widget_mouse_event(viewer, type, localPos, button,
                                           buttons, modifiers);
}

static bool gui_smoke_replay_viewer_mouse_stroke(SceneViewer* viewer) {
  if (!viewer) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  std::vector<QPointF> localPoints;
  localPoints.reserve(strokePoints.size());
  for (const TPointD& point : strokePoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseButtonPress, localPoints.front(), Qt::LeftButton,
      Qt::LeftButton);
  for (size_t i = 1; i + 1 < localPoints.size(); ++i) {
    delivered = gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                                  localPoints[i], Qt::NoButton,
                                                  Qt::LeftButton) &&
                delivered;
  }
  delivered = gui_smoke_send_viewer_mouse_event(
                  viewer, QEvent::MouseButtonRelease, localPoints.back(),
                  Qt::LeftButton, Qt::NoButton) &&
              delivered;
  gui_smoke_pump_events(100);
  return delivered;
}

static bool gui_smoke_replay_viewer_mouse_drag(
    SceneViewer* viewer, const std::vector<TPointD>& worldPoints,
    std::vector<QPointF>* localPointsOut = nullptr,
    Qt::KeyboardModifiers modifiers      = Qt::NoModifier) {
  if (!viewer || worldPoints.size() < 2) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  std::vector<QPointF> localPoints;
  localPoints.reserve(worldPoints.size());
  for (const TPointD& point : worldPoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseButtonPress, localPoints.front(), Qt::LeftButton,
      Qt::LeftButton, modifiers);
  for (size_t i = 1; i < localPoints.size(); ++i) {
    delivered = gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                                  localPoints[i], Qt::NoButton,
                                                  Qt::LeftButton, modifiers) &&
                delivered;
  }
  delivered = gui_smoke_send_viewer_mouse_event(
                  viewer, QEvent::MouseButtonRelease, localPoints.back(),
                  Qt::LeftButton, Qt::NoButton, modifiers) &&
              delivered;
  gui_smoke_pump_events(100);

  if (localPointsOut) *localPointsOut = localPoints;
  return delivered;
}

static bool gui_smoke_replay_viewer_mouse_polyline(
    SceneViewer* viewer, const std::vector<TPointD>& worldPoints,
    std::vector<QPointF>* localPointsOut = nullptr) {
  if (!viewer || worldPoints.size() < 3) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  std::vector<QPointF> localPoints;
  localPoints.reserve(worldPoints.size());
  for (const TPointD& point : worldPoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = true;
  for (const QPointF& localPoint : localPoints) {
    delivered = gui_smoke_send_viewer_mouse_event(
                    viewer, QEvent::MouseButtonPress, localPoint,
                    Qt::LeftButton, Qt::LeftButton) &&
                delivered;
    delivered = gui_smoke_send_viewer_mouse_event(
                    viewer, QEvent::MouseButtonRelease, localPoint,
                    Qt::LeftButton, Qt::NoButton) &&
                delivered;
  }

  const QPointF closePoint = localPoints.back();
  delivered                = gui_smoke_send_viewer_mouse_event(
                  viewer, QEvent::MouseButtonDblClick, closePoint,
                  Qt::LeftButton, Qt::LeftButton) &&
              delivered;
  delivered = gui_smoke_send_viewer_mouse_event(
                  viewer, QEvent::MouseButtonRelease, closePoint,
                  Qt::LeftButton, Qt::NoButton) &&
              delivered;
  gui_smoke_pump_events(120);

  if (localPointsOut) *localPointsOut = localPoints;
  return delivered;
}

static QString gui_smoke_joined_local_points(
    const std::vector<QPointF>& points) {
  QStringList values;
  for (const QPointF& point : points) {
    values
        << QString("%1,%2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f', 1);
  }
  return values.join('|');
}

static QString gui_smoke_rect_details(const TRectD& rect) {
  if (rect.isEmpty()) return QStringLiteral("empty");
  return QString("%1,%2,%3,%4")
      .arg(rect.x0, 0, 'f', 4)
      .arg(rect.y0, 0, 'f', 4)
      .arg(rect.x1, 0, 'f', 4)
      .arg(rect.y1, 0, 'f', 4);
}

static bool gui_smoke_near(double a, double b, double epsilon = 0.0001) {
  return std::abs(a - b) <= epsilon;
}

static void gui_smoke_reset_stage_object_position(TStageObject* stageObject,
                                                  double frame, double x,
                                                  double y) {
  if (!stageObject) return;
  stageObject->getParam(TStageObject::T_X)->setValue(frame, x);
  stageObject->getParam(TStageObject::T_Y)->setValue(frame, y);
}

static TEnumProperty* gui_smoke_find_tool_enum_property(
    TTool* tool, const std::string& propertyName,
    const std::string& propertyId) {
  if (!tool) return nullptr;

  auto findProperty = [&](TPropertyGroup* properties) -> TEnumProperty* {
    if (!properties) return nullptr;

    if (TProperty* property = properties->getProperty(propertyName)) {
      if (TEnumProperty* enumProperty =
              dynamic_cast<TEnumProperty*>(property)) {
        return enumProperty;
      }
    }

    for (int i = 0; i < properties->getPropertyCount(); ++i) {
      TProperty* property = properties->getProperty(i);
      if (!property) continue;
      if (property->getName() != propertyName &&
          property->getId() != propertyId)
        continue;
      if (TEnumProperty* enumProperty =
              dynamic_cast<TEnumProperty*>(property)) {
        return enumProperty;
      }
    }
    return nullptr;
  };

  if (TEnumProperty* property =
          findProperty(tool->getProperties(tool->getTargetType()))) {
    return property;
  }
  if (TEnumProperty* property = findProperty(tool->getProperties(0)))
    return property;
  if (TEnumProperty* property = findProperty(tool->getProperties(1)))
    return property;
  return nullptr;
}

static bool gui_smoke_set_tool_enum_property(
    TTool* tool, const std::string& propertyName, const std::string& propertyId,
    const std::wstring& value, QString* actualValueOut = nullptr) {
  TEnumProperty* property =
      gui_smoke_find_tool_enum_property(tool, propertyName, propertyId);
  if (!property) {
    if (actualValueOut) *actualValueOut = QStringLiteral("<missing>");
    return false;
  }

  property->setValue(value);
  tool->onPropertyChanged(property->getName());
  const QString actualValue = QString::fromStdWString(property->getValue());
  if (actualValueOut) *actualValueOut = actualValue;
  return property->getValue() == value;
}

static bool gui_smoke_tool_cursor_pixmap_ok(int cursorId) {
  const QCursor cursor   = getToolCursor(cursorId);
  const int baseCursorId = cursorId & 0xFF;
  if (baseCursorId == ToolCursor::CURSOR_NONE)
    return cursor.shape() == Qt::BlankCursor;
  if (baseCursorId == ToolCursor::ForbiddenCursor)
    return cursor.shape() == Qt::ForbiddenCursor;
  return !cursor.pixmap().isNull();
}

struct GuiSmokeCursorArtworkSignature {
  int width               = 0;
  int height              = 0;
  int hotX                = 0;
  int hotY                = 0;
  int alphaPixels         = 0;
  int opaquePixels        = 0;
  int colorPixels         = 0;
  double devicePixelRatio = 1.0;
  QString hash;
  bool ok = false;
};

static GuiSmokeCursorArtworkSignature gui_smoke_cursor_artwork_signature(
    int cursorId) {
  GuiSmokeCursorArtworkSignature signature;
  const QCursor cursor = getToolCursor(cursorId);
  const QPixmap pixmap = cursor.pixmap();
  if (pixmap.isNull()) return signature;

  const QPoint hotSpot = cursor.hotSpot();
  const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  signature.width    = image.width();
  signature.height   = image.height();
  signature.hotX     = hotSpot.x();
  signature.hotY     = hotSpot.y();
  signature.devicePixelRatio = pixmap.devicePixelRatio();

  quint64 hash = 1469598103934665603ull;
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = line[x];
      const int alpha  = qAlpha(pixel);
      if (alpha > 0) {
        ++signature.alphaPixels;
        if (alpha == 255) ++signature.opaquePixels;
        if (qRed(pixel) != 0 || qGreen(pixel) != 0 || qBlue(pixel) != 0)
          ++signature.colorPixels;
      }
      hash ^= static_cast<quint64>(pixel);
      hash *= 1099511628211ull;
    }
  }

  signature.hash = QString::number(hash, 16);
  signature.ok   = signature.width > 0 && signature.height > 0 &&
                 signature.alphaPixels > 0 && !signature.hash.isEmpty();
  return signature;
}

static QString gui_smoke_cursor_artwork_summary(
    const GuiSmokeCursorArtworkSignature& signature) {
  if (!signature.ok) return QStringLiteral("missing");
  return QString("%1x%2@%3,hot=%4,%5,alpha=%6,opaque=%7,color=%8,hash=%9")
      .arg(signature.width)
      .arg(signature.height)
      .arg(signature.devicePixelRatio, 0, 'f', 2)
      .arg(signature.hotX)
      .arg(signature.hotY)
      .arg(signature.alphaPixels)
      .arg(signature.opaquePixels)
      .arg(signature.colorPixels)
      .arg(signature.hash);
}

struct GuiSmokeToolCursorProbe {
  QString label;
  QString activeAxis;
  QPointF localPoint;
  bool axisOk               = false;
  bool inBounds             = false;
  bool normalEventDelivered = false;
  bool altEventDelivered    = false;
  bool resetEventDelivered  = false;
  int normalCursor          = ToolCursor::CURSOR_NONE;
  int altCursor             = ToolCursor::CURSOR_NONE;
  int expectedCursor        = ToolCursor::CURSOR_NONE;
  bool expectedCursorOk     = false;
  bool preciseCursorOk      = false;
  bool normalPixmapOk       = false;
  bool altPixmapOk          = false;
  GuiSmokeCursorArtworkSignature normalArtwork;
  GuiSmokeCursorArtworkSignature altArtwork;

  bool ok() const {
    return axisOk && inBounds && normalEventDelivered && altEventDelivered &&
           resetEventDelivered && expectedCursorOk && preciseCursorOk &&
           normalPixmapOk && altPixmapOk && normalArtwork.ok && altArtwork.ok;
  }
};

static GuiSmokeToolCursorProbe gui_smoke_probe_animate_tool_cursor(
    SceneViewer* viewer, TTool* tool, const QString& label,
    const std::wstring& activeAxis, int expectedCursor,
    const TPointD& worldPos) {
  GuiSmokeToolCursorProbe probe;
  probe.label          = label;
  probe.expectedCursor = expectedCursor;
  probe.axisOk         = gui_smoke_set_tool_enum_property(
      tool, "Active Axis", "EditToolActiveAxis", activeAxis, &probe.activeAxis);
  tool->updateMatrix();
  if (viewer) {
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(60);
    probe.localPoint = gui_smoke_world_to_viewer_local(viewer, worldPos);
    probe.inBounds   = viewer->rect().contains(probe.localPoint.toPoint());
  }

  if (!viewer || !probe.inBounds) return probe;

  probe.normalEventDelivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseMove, probe.localPoint, Qt::NoButton, Qt::NoButton,
      Qt::NoModifier);
  probe.normalCursor   = tool->getCursorId();
  probe.normalPixmapOk = gui_smoke_tool_cursor_pixmap_ok(probe.normalCursor);
  probe.normalArtwork  = gui_smoke_cursor_artwork_signature(probe.normalCursor);

  probe.altEventDelivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseMove, probe.localPoint, Qt::NoButton, Qt::NoButton,
      Qt::AltModifier);
  probe.altCursor   = tool->getCursorId();
  probe.altPixmapOk = gui_smoke_tool_cursor_pixmap_ok(probe.altCursor);
  probe.altArtwork  = gui_smoke_cursor_artwork_signature(probe.altCursor);

  probe.resetEventDelivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseMove, probe.localPoint, Qt::NoButton, Qt::NoButton,
      Qt::NoModifier);

  probe.expectedCursorOk = probe.normalCursor == expectedCursor;
  probe.preciseCursorOk =
      (probe.altCursor & ToolCursor::Ex_Precise) != 0 &&
      (probe.altCursor & ~ToolCursor::Ex_Precise) == probe.normalCursor;
  return probe;
}

static void gui_smoke_append_tool_cursor_probe_details(
    QStringList& details, const GuiSmokeToolCursorProbe& probe) {
  const QString prefix = QStringLiteral("animateCursor%1").arg(probe.label);
  details
      << QString("%1Axis=%2")
             .arg(prefix)
             .arg(gui_smoke_status_value(probe.activeAxis))
      << QString("%1ExpectedCursor=%2").arg(prefix).arg(probe.expectedCursor)
      << QString("%1NormalCursor=%2").arg(prefix).arg(probe.normalCursor)
      << QString("%1AltCursor=%2").arg(prefix).arg(probe.altCursor)
      << QString("%1LocalPoint=%2,%3")
             .arg(prefix)
             .arg(probe.localPoint.x(), 0, 'f', 2)
             .arg(probe.localPoint.y(), 0, 'f', 2)
      << QString("%1AxisOk=%2")
             .arg(prefix)
             .arg(probe.axisOk ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("%1InBounds=%2")
             .arg(prefix)
             .arg(probe.inBounds ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("%1NormalEventDelivered=%2")
             .arg(prefix)
             .arg(probe.normalEventDelivered ? QStringLiteral("true")
                                             : QStringLiteral("false"))
      << QString("%1AltEventDelivered=%2")
             .arg(prefix)
             .arg(probe.altEventDelivered ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("%1ResetEventDelivered=%2")
             .arg(prefix)
             .arg(probe.resetEventDelivered ? QStringLiteral("true")
                                            : QStringLiteral("false"))
      << QString("%1ExpectedCursorOk=%2")
             .arg(prefix)
             .arg(probe.expectedCursorOk ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("%1PreciseCursorOk=%2")
             .arg(prefix)
             .arg(probe.preciseCursorOk ? QStringLiteral("true")
                                        : QStringLiteral("false"))
      << QString("%1NormalPixmapOk=%2")
             .arg(prefix)
             .arg(probe.normalPixmapOk ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("%1AltPixmapOk=%2")
             .arg(prefix)
             .arg(probe.altPixmapOk ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("%1NormalArtwork=%2")
             .arg(prefix)
             .arg(gui_smoke_cursor_artwork_summary(probe.normalArtwork))
      << QString("%1AltArtwork=%2")
             .arg(prefix)
             .arg(gui_smoke_cursor_artwork_summary(probe.altArtwork))
      << QString("%1NormalArtworkOk=%2")
             .arg(prefix)
             .arg(probe.normalArtwork.ok ? QStringLiteral("true")
                                         : QStringLiteral("false"))
      << QString("%1AltArtworkOk=%2")
             .arg(prefix)
             .arg(probe.altArtwork.ok ? QStringLiteral("true")
                                      : QStringLiteral("false"))
      << QString("%1Probe=%2")
             .arg(prefix)
             .arg(probe.ok() ? QStringLiteral("ok") : QStringLiteral("error"));
}

struct GuiSmokeStageObjectBaseline {
  double angle  = 0.0;
  double x      = 0.0;
  double y      = 0.0;
  double scaleX = 0.0;
  double scaleY = 0.0;
  double scale  = 0.0;
  double shearX = 0.0;
  double shearY = 0.0;
  TPointD center;
};

static GuiSmokeStageObjectBaseline gui_smoke_capture_stage_object_baseline(
    TXsheet* xsheet, const TStageObjectId& objectId, TStageObject* stageObject,
    int frame) {
  GuiSmokeStageObjectBaseline baseline;
  if (!xsheet || !stageObject) return baseline;

  baseline.angle  = stageObject->getParam(TStageObject::T_Angle, frame);
  baseline.x      = stageObject->getParam(TStageObject::T_X, frame);
  baseline.y      = stageObject->getParam(TStageObject::T_Y, frame);
  baseline.scaleX = stageObject->getParam(TStageObject::T_ScaleX, frame);
  baseline.scaleY = stageObject->getParam(TStageObject::T_ScaleY, frame);
  baseline.scale  = stageObject->getParam(TStageObject::T_Scale, frame);
  baseline.shearX = stageObject->getParam(TStageObject::T_ShearX, frame);
  baseline.shearY = stageObject->getParam(TStageObject::T_ShearY, frame);
  baseline.center = xsheet->getCenter(objectId, frame);
  return baseline;
}

static void gui_smoke_restore_stage_object_baseline(
    TXsheet* xsheet, const TStageObjectId& objectId, TStageObject* stageObject,
    int frame, const GuiSmokeStageObjectBaseline& baseline) {
  if (!xsheet || !stageObject) return;

  stageObject->getParam(TStageObject::T_Angle)->setValue(frame, baseline.angle);
  stageObject->getParam(TStageObject::T_X)->setValue(frame, baseline.x);
  stageObject->getParam(TStageObject::T_Y)->setValue(frame, baseline.y);
  stageObject->getParam(TStageObject::T_ScaleX)
      ->setValue(frame, baseline.scaleX);
  stageObject->getParam(TStageObject::T_ScaleY)
      ->setValue(frame, baseline.scaleY);
  stageObject->getParam(TStageObject::T_Scale)->setValue(frame, baseline.scale);
  stageObject->getParam(TStageObject::T_ShearX)
      ->setValue(frame, baseline.shearX);
  stageObject->getParam(TStageObject::T_ShearY)
      ->setValue(frame, baseline.shearY);
  xsheet->setCenter(objectId, frame, baseline.center);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

enum class GuiSmokeAnimateAxisDragKind {
  Position,
  Rotation,
  Scale,
  Shear,
  Center
};

struct GuiSmokeAnimateAxisDragProbe {
  QString label;
  QString activeAxis;
  QString beforeValue;
  QString afterValue;
  QString deltaValue;
  QString localPoints;
  bool axisOk               = false;
  bool mouseEventsDelivered = false;
  bool transformOk          = false;

  bool ok() const { return axisOk && mouseEventsDelivered && transformOk; }
};

static GuiSmokeAnimateAxisDragProbe gui_smoke_probe_animate_axis_drag(
    SceneViewer* viewer, TTool* tool, TXsheet* xsheet,
    const TStageObjectId& objectId, TStageObject* stageObject, int frame,
    const QString& label, const std::wstring& activeAxis,
    GuiSmokeAnimateAxisDragKind kind, const std::vector<TPointD>& worldPoints) {
  GuiSmokeAnimateAxisDragProbe probe;
  probe.label  = label;
  probe.axisOk = gui_smoke_set_tool_enum_property(
      tool, "Active Axis", "EditToolActiveAxis", activeAxis, &probe.activeAxis);
  tool->updateMatrix();
  if (viewer) {
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(60);
  }

  const double beforeX = stageObject->getParam(TStageObject::T_X, frame);
  const double beforeY = stageObject->getParam(TStageObject::T_Y, frame);
  const double beforeAngle =
      stageObject->getParam(TStageObject::T_Angle, frame);
  const double beforeScale =
      stageObject->getParam(TStageObject::T_Scale, frame);
  const double beforeShearX =
      stageObject->getParam(TStageObject::T_ShearX, frame);
  const double beforeShearY =
      stageObject->getParam(TStageObject::T_ShearY, frame);
  const TPointD beforeCenter = xsheet->getCenter(objectId, frame);

  std::vector<QPointF> localPoints;
  probe.mouseEventsDelivered =
      gui_smoke_replay_viewer_mouse_drag(viewer, worldPoints, &localPoints);
  probe.localPoints = gui_smoke_joined_local_points(localPoints);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  if (viewer) {
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(90);
  }
  tool->updateMatrix();

  const double afterX     = stageObject->getParam(TStageObject::T_X, frame);
  const double afterY     = stageObject->getParam(TStageObject::T_Y, frame);
  const double afterAngle = stageObject->getParam(TStageObject::T_Angle, frame);
  const double afterScale = stageObject->getParam(TStageObject::T_Scale, frame);
  const double afterShearX =
      stageObject->getParam(TStageObject::T_ShearX, frame);
  const double afterShearY =
      stageObject->getParam(TStageObject::T_ShearY, frame);
  const TPointD afterCenter = xsheet->getCenter(objectId, frame);

  switch (kind) {
  case GuiSmokeAnimateAxisDragKind::Position: {
    const double deltaX = afterX - beforeX;
    const double deltaY = afterY - beforeY;
    probe.beforeValue =
        QString("%1,%2").arg(beforeX, 0, 'f', 4).arg(beforeY, 0, 'f', 4);
    probe.afterValue =
        QString("%1,%2").arg(afterX, 0, 'f', 4).arg(afterY, 0, 'f', 4);
    probe.deltaValue =
        QString("%1,%2").arg(deltaX, 0, 'f', 4).arg(deltaY, 0, 'f', 4);
    probe.transformOk = std::abs(deltaX) > 0.0001 && std::abs(deltaY) > 0.0001;
    break;
  }
  case GuiSmokeAnimateAxisDragKind::Rotation: {
    const double deltaAngle = afterAngle - beforeAngle;
    probe.beforeValue       = QString("%1").arg(beforeAngle, 0, 'f', 4);
    probe.afterValue        = QString("%1").arg(afterAngle, 0, 'f', 4);
    probe.deltaValue        = QString("%1").arg(deltaAngle, 0, 'f', 4);
    probe.transformOk       = std::abs(deltaAngle) > 0.001;
    break;
  }
  case GuiSmokeAnimateAxisDragKind::Scale: {
    const double deltaScale = afterScale - beforeScale;
    probe.beforeValue       = QString("%1").arg(beforeScale, 0, 'f', 4);
    probe.afterValue        = QString("%1").arg(afterScale, 0, 'f', 4);
    probe.deltaValue        = QString("%1").arg(deltaScale, 0, 'f', 4);
    probe.transformOk       = std::abs(deltaScale) > 0.001;
    break;
  }
  case GuiSmokeAnimateAxisDragKind::Shear: {
    const double deltaShearX = afterShearX - beforeShearX;
    const double deltaShearY = afterShearY - beforeShearY;
    probe.beforeValue        = QString("%1,%2")
                            .arg(beforeShearX, 0, 'f', 4)
                            .arg(beforeShearY, 0, 'f', 4);
    probe.afterValue = QString("%1,%2")
                           .arg(afterShearX, 0, 'f', 4)
                           .arg(afterShearY, 0, 'f', 4);
    probe.deltaValue = QString("%1,%2")
                           .arg(deltaShearX, 0, 'f', 4)
                           .arg(deltaShearY, 0, 'f', 4);
    probe.transformOk =
        std::abs(deltaShearX) > 0.0001 && std::abs(deltaShearY) > 0.0001;
    break;
  }
  case GuiSmokeAnimateAxisDragKind::Center: {
    const TPointD deltaCenter = afterCenter - beforeCenter;
    probe.beforeValue         = QString("%1,%2")
                            .arg(beforeCenter.x, 0, 'f', 4)
                            .arg(beforeCenter.y, 0, 'f', 4);
    probe.afterValue = QString("%1,%2")
                           .arg(afterCenter.x, 0, 'f', 4)
                           .arg(afterCenter.y, 0, 'f', 4);
    probe.deltaValue = QString("%1,%2")
                           .arg(deltaCenter.x, 0, 'f', 4)
                           .arg(deltaCenter.y, 0, 'f', 4);
    probe.transformOk = norm(deltaCenter) > 0.0001;
    break;
  }
  }

  return probe;
}

static void gui_smoke_append_animate_axis_drag_probe_details(
    QStringList& details, const GuiSmokeAnimateAxisDragProbe& probe) {
  const QString prefix = QStringLiteral("animateAxis%1").arg(probe.label);
  details << QString("%1Axis=%2")
                 .arg(prefix)
                 .arg(gui_smoke_status_value(probe.activeAxis))
          << QString("%1Before=%2")
                 .arg(prefix)
                 .arg(gui_smoke_status_value(probe.beforeValue))
          << QString("%1After=%2")
                 .arg(prefix)
                 .arg(gui_smoke_status_value(probe.afterValue))
          << QString("%1Delta=%2")
                 .arg(prefix)
                 .arg(gui_smoke_status_value(probe.deltaValue))
          << QString("%1LocalPoints=%2")
                 .arg(prefix)
                 .arg(gui_smoke_status_value(probe.localPoints))
          << QString("%1AxisOk=%2")
                 .arg(prefix)
                 .arg(probe.axisOk ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("%1MouseEventsDelivered=%2")
                 .arg(prefix)
                 .arg(probe.mouseEventsDelivered ? QStringLiteral("true")
                                                 : QStringLiteral("false"))
          << QString("%1TransformOk=%2")
                 .arg(prefix)
                 .arg(probe.transformOk ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("%1Probe=%2")
                 .arg(prefix)
                 .arg(probe.ok() ? QStringLiteral("ok")
                                 : QStringLiteral("error"));
}

static QStringList gui_smoke_viewer_animate_tool_mouse_event_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolTransformProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolTransformProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_mouse_events");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("toolTransformProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolTransformProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolTransformProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const double beforeX          = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY          = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine beforePlacement = xsheet->getPlacement(objectId, 0);

  const std::vector<TPointD> dragPoints = {
      TPointD(0.0, 0.0), TPointD(72.0, -36.0), TPointD(144.0, -72.0)};
  std::vector<QPointF> localDragPoints;
  const bool mouseEventsDelivered =
      gui_smoke_replay_viewer_mouse_drag(viewer, dragPoints, &localDragPoints);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const double afterX          = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY          = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement = xsheet->getPlacement(objectId, 0);
  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk = mouseEventsDelivered && objectOk &&
                           beforePlacement != afterPlacement &&
                           (std::abs(afterX - beforeX) > 0.0001 ||
                            std::abs(afterY - beforeY) > 0.0001);

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(objectId.toString()))
          << QString("stageObjectXBefore=%1").arg(beforeX, 0, 'f', 4)
          << QString("stageObjectYBefore=%1").arg(beforeY, 0, 'f', 4)
          << QString("stageObjectXAfter=%1").arg(afterX, 0, 'f', 4)
          << QString("stageObjectYAfter=%1").arg(afterY, 0, 'f', 4)
          << QString("toolDragStart=%1,%2")
                 .arg(dragPoints.front().x, 0, 'f', 1)
                 .arg(dragPoints.front().y, 0, 'f', 1)
          << QString("toolDragEnd=%1,%2")
                 .arg(dragPoints.back().x, 0, 'f', 1)
                 .arg(dragPoints.back().y, 0, 'f', 1)
          << QString("toolDragLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(localDragPoints))
          << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
          << QString("toolTransformProbe=%1")
                 .arg(transformOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-mouse-events"),
      QStringLiteral("animate-tool-mouse-events"), transformOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_undo_redo_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolTransformProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer")
            << QStringLiteral("undoRedoProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolTransformProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QStringLiteral("undoRedoProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_undo_redo");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QStringLiteral("undoRedoProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QStringLiteral("undoRedoProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("toolTransformProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QStringLiteral("undoRedoProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QStringLiteral("undoRedoProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolTransformProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QStringLiteral("undoRedoProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolTransformProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QStringLiteral("undoRedoProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const double beforeX          = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY          = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine beforePlacement = xsheet->getPlacement(objectId, 0);

  TUndoManager* undoManager = TUndoManager::manager();
  undoManager->reset();
  const int historyCountBefore = undoManager->getHistoryCount();
  const int historyIndexBefore = undoManager->getCurrentHistoryIndex();

  const std::vector<TPointD> dragPoints = {
      TPointD(0.0, 0.0), TPointD(72.0, -36.0), TPointD(144.0, -72.0)};
  std::vector<QPointF> localDragPoints;
  const bool mouseEventsDelivered =
      gui_smoke_replay_viewer_mouse_drag(viewer, dragPoints, &localDragPoints);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const double afterX             = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY             = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement    = xsheet->getPlacement(objectId, 0);
  const int historyCountAfterDrag = undoManager->getHistoryCount();
  const int historyIndexAfterDrag = undoManager->getCurrentHistoryIndex();

  const bool undoResult = undoManager->undo();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);
  const double undoX              = stageObject->getParam(TStageObject::T_X, 0);
  const double undoY              = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine undoPlacement     = xsheet->getPlacement(objectId, 0);
  const int historyIndexAfterUndo = undoManager->getCurrentHistoryIndex();

  const bool redoResult = undoManager->redo();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const double redoX              = stageObject->getParam(TStageObject::T_X, 0);
  const double redoY              = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine redoPlacement     = xsheet->getPlacement(objectId, 0);
  const int historyIndexAfterRedo = undoManager->getCurrentHistoryIndex();

  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk = mouseEventsDelivered && objectOk &&
                           beforePlacement != afterPlacement &&
                           (std::abs(afterX - beforeX) > 0.0001 ||
                            std::abs(afterY - beforeY) > 0.0001);
  const bool undoRestored = gui_smoke_near(undoX, beforeX) &&
                            gui_smoke_near(undoY, beforeY) &&
                            undoPlacement == beforePlacement;
  const bool redoRestored = gui_smoke_near(redoX, afterX) &&
                            gui_smoke_near(redoY, afterY) &&
                            redoPlacement == afterPlacement;
  const bool undoRedoOk =
      transformOk && historyCountBefore == 0 && historyIndexBefore == 0 &&
      historyCountAfterDrag > historyCountBefore &&
      historyIndexAfterDrag == historyCountAfterDrag && undoResult &&
      historyIndexAfterUndo < historyIndexAfterDrag && undoRestored &&
      redoResult && historyIndexAfterRedo == historyIndexAfterDrag &&
      redoRestored;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("stageObject=%1")
             .arg(QString::fromStdString(objectId.toString()))
      << QString("stageObjectXBefore=%1").arg(beforeX, 0, 'f', 4)
      << QString("stageObjectYBefore=%1").arg(beforeY, 0, 'f', 4)
      << QString("stageObjectXAfter=%1").arg(afterX, 0, 'f', 4)
      << QString("stageObjectYAfter=%1").arg(afterY, 0, 'f', 4)
      << QString("stageObjectXUndo=%1").arg(undoX, 0, 'f', 4)
      << QString("stageObjectYUndo=%1").arg(undoY, 0, 'f', 4)
      << QString("stageObjectXRedo=%1").arg(redoX, 0, 'f', 4)
      << QString("stageObjectYRedo=%1").arg(redoY, 0, 'f', 4)
      << QString("undoResult=%1")
             .arg(undoResult ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("redoResult=%1")
             .arg(redoResult ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("undoRestored=%1")
             .arg(undoRestored ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("redoRestored=%1")
             .arg(redoRestored ? QStringLiteral("true")
                               : QStringLiteral("false"))
      << QString("undoHistoryCountBefore=%1").arg(historyCountBefore)
      << QString("undoHistoryCountAfterDrag=%1").arg(historyCountAfterDrag)
      << QString("undoHistoryIndexBefore=%1").arg(historyIndexBefore)
      << QString("undoHistoryIndexAfterDrag=%1").arg(historyIndexAfterDrag)
      << QString("undoHistoryIndexAfterUndo=%1").arg(historyIndexAfterUndo)
      << QString("undoHistoryIndexAfterRedo=%1").arg(historyIndexAfterRedo)
      << QString("toolDragStart=%1,%2")
             .arg(dragPoints.front().x, 0, 'f', 1)
             .arg(dragPoints.front().y, 0, 'f', 1)
      << QString("toolDragEnd=%1,%2")
             .arg(dragPoints.back().x, 0, 'f', 1)
             .arg(dragPoints.back().y, 0, 'f', 1)
      << QString("toolDragLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(localDragPoints))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("toolTransformProbe=%1")
             .arg(transformOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("undoRedoProbe=%1")
             .arg(undoRedoOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-undo-redo"),
      QStringLiteral("animate-tool-undo-redo"), undoRedoOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_modifiers_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolTransformProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer")
            << QStringLiteral("modifierProbe=no-viewer")
            << QStringLiteral("cursorPreciseProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolTransformProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QStringLiteral("modifierProbe=scene-folder-error")
            << QStringLiteral("cursorPreciseProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_modifiers");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QStringLiteral("modifierProbe=level-error")
            << QStringLiteral("cursorPreciseProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QStringLiteral("modifierProbe=xsheet-error")
            << QStringLiteral("cursorPreciseProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("toolTransformProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QStringLiteral("modifierProbe=stage-object-error")
            << QStringLiteral("cursorPreciseProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QStringLiteral("modifierProbe=tool-handle-error")
            << QStringLiteral("cursorPreciseProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolTransformProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QStringLiteral("modifierProbe=tool-error")
            << QStringLiteral("cursorPreciseProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolTransformProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QStringLiteral("modifierProbe=tool-disabled")
            << QStringLiteral("cursorPreciseProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const double frame   = 0.0;
  const double beforeX = stageObject->getParam(TStageObject::T_X, frame);
  const double beforeY = stageObject->getParam(TStageObject::T_Y, frame);
  const TAffine beforePlacement         = xsheet->getPlacement(objectId, frame);
  const std::vector<TPointD> dragPoints = {
      TPointD(0.0, 0.0), TPointD(72.0, -36.0), TPointD(144.0, -72.0)};

  bool cursorEventsDelivered = false;
  int normalCursor           = tool->getCursorId();
  int altCursor              = normalCursor;
  const QPointF cursorLocalPoint =
      gui_smoke_world_to_viewer_local(viewer, dragPoints.front());
  if (viewer->rect().contains(cursorLocalPoint.toPoint())) {
    cursorEventsDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, cursorLocalPoint, Qt::NoButton, Qt::NoButton,
        Qt::NoModifier);
    normalCursor          = tool->getCursorId();
    cursorEventsDelivered = gui_smoke_send_viewer_mouse_event(
                                viewer, QEvent::MouseMove, cursorLocalPoint,
                                Qt::NoButton, Qt::NoButton, Qt::AltModifier) &&
                            cursorEventsDelivered;
    altCursor             = tool->getCursorId();
    cursorEventsDelivered = gui_smoke_send_viewer_mouse_event(
                                viewer, QEvent::MouseMove, cursorLocalPoint,
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier) &&
                            cursorEventsDelivered;
  }
  const bool cursorPreciseBefore = (normalCursor & ToolCursor::Ex_Precise) != 0;
  const bool cursorPreciseAfter  = (altCursor & ToolCursor::Ex_Precise) != 0;
  const bool cursorPreciseOk =
      cursorEventsDelivered && !cursorPreciseBefore && cursorPreciseAfter;

  std::vector<QPointF> normalLocalDragPoints;
  const bool normalMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, dragPoints, &normalLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);
  const double normalX = stageObject->getParam(TStageObject::T_X, frame);
  const double normalY = stageObject->getParam(TStageObject::T_Y, frame);
  const TAffine normalPlacement = xsheet->getPlacement(objectId, frame);
  const double normalDeltaX     = normalX - beforeX;
  const double normalDeltaY     = normalY - beforeY;

  auto resetObject = [&]() {
    gui_smoke_reset_stage_object_position(stageObject, frame, beforeX, beforeY);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifySceneChanged();
    tool->updateMatrix();
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(80);
  };

  resetObject();
  std::vector<QPointF> altLocalDragPoints;
  const bool altMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, dragPoints, &altLocalDragPoints, Qt::AltModifier);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);
  const double altX          = stageObject->getParam(TStageObject::T_X, frame);
  const double altY          = stageObject->getParam(TStageObject::T_Y, frame);
  const TAffine altPlacement = xsheet->getPlacement(objectId, frame);
  const double altDeltaX     = altX - beforeX;
  const double altDeltaY     = altY - beforeY;

  resetObject();
  std::vector<QPointF> shiftLocalDragPoints;
  const bool shiftMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, dragPoints, &shiftLocalDragPoints, Qt::ShiftModifier);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const double shiftX = stageObject->getParam(TStageObject::T_X, frame);
  const double shiftY = stageObject->getParam(TStageObject::T_Y, frame);
  const TAffine shiftPlacement = xsheet->getPlacement(objectId, frame);
  const double shiftDeltaX     = shiftX - beforeX;
  const double shiftDeltaY     = shiftY - beforeY;

  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool normalTransformOk = normalMouseEventsDelivered && objectOk &&
                                 beforePlacement != normalPlacement &&
                                 std::abs(normalDeltaX) > 0.0001 &&
                                 std::abs(normalDeltaY) > 0.0001;
  const bool altPrecisionOk =
      altMouseEventsDelivered && beforePlacement != altPlacement &&
      std::abs(altDeltaX) > 0.0001 && std::abs(altDeltaY) > 0.0001 &&
      gui_smoke_near(altDeltaX, normalDeltaX * 0.1, 0.005) &&
      gui_smoke_near(altDeltaY, normalDeltaY * 0.1, 0.005);
  const bool shiftConstraintOk =
      shiftMouseEventsDelivered && beforePlacement != shiftPlacement &&
      std::abs(shiftDeltaX) > 0.0001 &&
      gui_smoke_near(shiftDeltaX, normalDeltaX, 0.005) &&
      gui_smoke_near(shiftY, beforeY, 0.005) &&
      gui_smoke_near(shiftDeltaY, 0.0, 0.005);
  const bool mouseEventsDelivered =
      cursorEventsDelivered && normalMouseEventsDelivered &&
      altMouseEventsDelivered && shiftMouseEventsDelivered;
  const bool modifierOk = normalTransformOk && altPrecisionOk &&
                          shiftConstraintOk && cursorPreciseOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(objectId.toString()))
          << QString("stageObjectXBefore=%1").arg(beforeX, 0, 'f', 4)
          << QString("stageObjectYBefore=%1").arg(beforeY, 0, 'f', 4)
          << QString("normalStageObjectXAfter=%1").arg(normalX, 0, 'f', 4)
          << QString("normalStageObjectYAfter=%1").arg(normalY, 0, 'f', 4)
          << QString("altStageObjectXAfter=%1").arg(altX, 0, 'f', 4)
          << QString("altStageObjectYAfter=%1").arg(altY, 0, 'f', 4)
          << QString("shiftStageObjectXAfter=%1").arg(shiftX, 0, 'f', 4)
          << QString("shiftStageObjectYAfter=%1").arg(shiftY, 0, 'f', 4)
          << QString("normalDeltaX=%1").arg(normalDeltaX, 0, 'f', 4)
          << QString("normalDeltaY=%1").arg(normalDeltaY, 0, 'f', 4)
          << QString("altDeltaX=%1").arg(altDeltaX, 0, 'f', 4)
          << QString("altDeltaY=%1").arg(altDeltaY, 0, 'f', 4)
          << QString("shiftDeltaX=%1").arg(shiftDeltaX, 0, 'f', 4)
          << QString("shiftDeltaY=%1").arg(shiftDeltaY, 0, 'f', 4)
          << QString("normalToAltExpectedRatio=%1").arg(0.1, 0, 'f', 1)
          << QString("normalCursor=%1").arg(normalCursor)
          << QString("altCursor=%1").arg(altCursor)
          << QString("cursorPreciseBefore=%1")
                 .arg(cursorPreciseBefore ? QStringLiteral("true")
                                          : QStringLiteral("false"))
          << QString("cursorPreciseAfter=%1")
                 .arg(cursorPreciseAfter ? QStringLiteral("true")
                                         : QStringLiteral("false"))
          << QString("toolDragStart=%1,%2")
                 .arg(dragPoints.front().x, 0, 'f', 1)
                 .arg(dragPoints.front().y, 0, 'f', 1)
          << QString("toolDragEnd=%1,%2")
                 .arg(dragPoints.back().x, 0, 'f', 1)
                 .arg(dragPoints.back().y, 0, 'f', 1)
          << QString("toolDragLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(normalLocalDragPoints))
          << QString("altToolDragLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(altLocalDragPoints))
          << QString("shiftToolDragLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(shiftLocalDragPoints))
          << QString("normalMouseEventProbe=%1")
                 .arg(normalMouseEventsDelivered ? QStringLiteral("ok")
                                                 : QStringLiteral("error"))
          << QString("altMouseEventProbe=%1")
                 .arg(altMouseEventsDelivered ? QStringLiteral("ok")
                                              : QStringLiteral("error"))
          << QString("shiftMouseEventProbe=%1")
                 .arg(shiftMouseEventsDelivered ? QStringLiteral("ok")
                                                : QStringLiteral("error"))
          << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
          << QString("toolTransformProbe=%1")
                 .arg(normalTransformOk ? QStringLiteral("ok")
                                        : QStringLiteral("error"))
          << QString("altPrecisionProbe=%1")
                 .arg(altPrecisionOk ? QStringLiteral("ok")
                                     : QStringLiteral("error"))
          << QString("shiftConstraintProbe=%1")
                 .arg(shiftConstraintOk ? QStringLiteral("ok")
                                        : QStringLiteral("error"))
          << QString("cursorPreciseProbe=%1")
                 .arg(cursorPreciseOk ? QStringLiteral("ok")
                                      : QStringLiteral("error"))
          << QString("modifierProbe=%1")
                 .arg(modifierOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-modifiers"),
      QStringLiteral("animate-tool-modifiers"), modifierOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_handles_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("handleHitTestProbe=no-viewer")
            << QStringLiteral("handleHoverProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("handleHitTestProbe=scene-folder-error")
            << QStringLiteral("handleHoverProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_handles");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("handleHitTestProbe=level-error")
            << QStringLiteral("handleHoverProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("handleHitTestProbe=xsheet-error")
            << QStringLiteral("handleHoverProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("handleHitTestProbe=stage-object-error")
            << QStringLiteral("handleHoverProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const int frame = 0;
  TApp::instance()->getCurrentFrame()->setFrame(frame);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("handleHitTestProbe=tool-handle-error")
            << QStringLiteral("handleHoverProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("handleHitTestProbe=tool-error")
            << QStringLiteral("handleHoverProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("handleHitTestProbe=tool-disabled")
            << QStringLiteral("handleHoverProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString activeAxisValue;
  const bool activeAxisOk = gui_smoke_set_tool_enum_property(
      tool, "Active Axis", "EditToolActiveAxis", L"All", &activeAxisValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const GuiSmokeStageObjectBaseline baseline =
      gui_smoke_capture_stage_object_baseline(xsheet, objectId, stageObject,
                                              frame);
  const double handleUnit = tool->getPixelSize() *
                            Preferences::instance()->getAnimateToolHandleSize();
  const TPointD centerHandle(0.0, 0.0);
  const TPointD rotationHandle(0.0, handleUnit * 30.0);
  const TPointD rotationEnd(handleUnit * 30.0, handleUnit * 30.0);
  const TPointD scaleHandle(-handleUnit * 30.0, -handleUnit * 30.0);
  const TPointD scaleEnd(-handleUnit * 45.0, -handleUnit * 45.0);
  const TPointD centerEnd(handleUnit * 24.0, handleUnit * 12.0);

  const QPointF rotationLocalPoint =
      gui_smoke_world_to_viewer_local(viewer, rotationHandle);
  bool hoverDelivered = false;
  QImage hoverFrame;
  GuiSmokeImageStats hoverStats;
  bool hoverCaptureSaved = false;
  QString hoverCapturePath;
  if (viewer->rect().contains(rotationLocalPoint.toPoint())) {
    hoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, rotationLocalPoint, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    hoverFrame = gui_smoke_grab_viewer_frame(viewer);
    hoverStats = gui_smoke_analyze_viewer_frame(hoverFrame, before);

    const QString statusPath =
        qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
    const QString outputDir = statusPath.isEmpty()
                                  ? QDir::tempPath()
                                  : QFileInfo(statusPath).absolutePath();
    QDir().mkpath(outputDir);
    hoverCapturePath = QDir(outputDir).filePath(
        QStringLiteral("viewer-animate-tool-handles-hover.png"));
    hoverCaptureSaved = hoverFrame.save(hoverCapturePath);
  }
  const bool hoverOk = hoverDelivered && !hoverFrame.isNull() &&
                       hoverStats.changedPixels > 0 && hoverCaptureSaved;

  std::vector<QPointF> rotationLocalDragPoints;
  const bool rotationMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, {rotationHandle, rotationEnd}, &rotationLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const double rotationAngleAfter =
      stageObject->getParam(TStageObject::T_Angle, frame);
  const bool rotationHitOk =
      rotationMouseEventsDelivered &&
      std::abs(rotationAngleAfter - baseline.angle) > 0.001;

  gui_smoke_restore_stage_object_baseline(xsheet, objectId, stageObject, frame,
                                          baseline);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  std::vector<QPointF> scaleLocalDragPoints;
  const bool scaleMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, {scaleHandle, scaleEnd}, &scaleLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const double scaleAfter = stageObject->getParam(TStageObject::T_Scale, frame);
  const bool scaleHitOk   = scaleMouseEventsDelivered &&
                          std::abs(scaleAfter - baseline.scale) > 0.001;

  gui_smoke_restore_stage_object_baseline(xsheet, objectId, stageObject, frame,
                                          baseline);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  std::vector<QPointF> centerLocalDragPoints;
  const bool centerMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, {centerHandle, centerEnd}, &centerLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const TPointD centerAfter = xsheet->getCenter(objectId, frame);
  const bool centerHitOk    = centerMouseEventsDelivered &&
                           norm(centerAfter - baseline.center) > 0.0001;

  const bool mouseEventsDelivered =
      hoverDelivered && rotationMouseEventsDelivered &&
      scaleMouseEventsDelivered && centerMouseEventsDelivered;
  const bool handleHitTestOk =
      activeAxisOk && hoverOk && rotationHitOk && scaleHitOk && centerHitOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("activeAxis=%1").arg(gui_smoke_status_value(activeAxisValue))
      << QString("handleUnit=%1").arg(handleUnit, 0, 'f', 4)
      << QString("stageObject=%1")
             .arg(QString::fromStdString(objectId.toString()))
      << QString("angleBefore=%1").arg(baseline.angle, 0, 'f', 4)
      << QString("angleAfterRotationHandle=%1")
             .arg(rotationAngleAfter, 0, 'f', 4)
      << QString("scaleBefore=%1").arg(baseline.scale, 0, 'f', 4)
      << QString("scaleAfterScaleHandle=%1").arg(scaleAfter, 0, 'f', 4)
      << QString("centerBefore=%1,%2")
             .arg(baseline.center.x, 0, 'f', 4)
             .arg(baseline.center.y, 0, 'f', 4)
      << QString("centerAfterCenterHandle=%1,%2")
             .arg(centerAfter.x, 0, 'f', 4)
             .arg(centerAfter.y, 0, 'f', 4)
      << QString("rotationHandleWorld=%1,%2")
             .arg(rotationHandle.x, 0, 'f', 4)
             .arg(rotationHandle.y, 0, 'f', 4)
      << QString("scaleHandleWorld=%1,%2")
             .arg(scaleHandle.x, 0, 'f', 4)
             .arg(scaleHandle.y, 0, 'f', 4)
      << QString("centerHandleWorld=%1,%2")
             .arg(centerHandle.x, 0, 'f', 4)
             .arg(centerHandle.y, 0, 'f', 4)
      << QString("rotationHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(rotationLocalDragPoints))
      << QString("scaleHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(scaleLocalDragPoints))
      << QString("centerHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(centerLocalDragPoints))
      << QString("hoverChangedPixels=%1").arg(hoverStats.changedPixels)
      << QString("hoverCaptureSaved=%1")
             .arg(hoverCaptureSaved ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("hoverCapturePath=%1")
             .arg(gui_smoke_status_value(hoverCapturePath))
      << QString("activeAxisProbe=%1")
             .arg(activeAxisOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("rotationHandleProbe=%1")
             .arg(rotationHitOk ? QStringLiteral("ok")
                                : QStringLiteral("error"))
      << QString("scaleHandleProbe=%1")
             .arg(scaleHitOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("centerHandleProbe=%1")
             .arg(centerHitOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("handleHoverProbe=%1")
             .arg(hoverOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("handleHitTestProbe=%1")
             .arg(handleHitTestOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-handles"),
      QStringLiteral("animate-tool-handles"), handleHitTestOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_handle_variants_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("handleVariantProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("handleVariantProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_handle_variants");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("handleVariantProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("handleVariantProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("handleVariantProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const int frame = 0;
  TApp::instance()->getCurrentFrame()->setFrame(frame);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("handleVariantProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("handleVariantProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("handleVariantProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString activeAxisValue;
  const bool activeAxisOk = gui_smoke_set_tool_enum_property(
      tool, "Active Axis", "EditToolActiveAxis", L"All", &activeAxisValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const GuiSmokeStageObjectBaseline baseline =
      gui_smoke_capture_stage_object_baseline(xsheet, objectId, stageObject,
                                              frame);
  const double handleUnit = tool->getPixelSize() *
                            Preferences::instance()->getAnimateToolHandleSize();

  auto restoreObject = [&]() {
    gui_smoke_restore_stage_object_baseline(xsheet, objectId, stageObject,
                                            frame, baseline);
    tool->updateMatrix();
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(80);
  };

  const double translationBeforeX =
      stageObject->getParam(TStageObject::T_X, frame);
  const double translationBeforeY =
      stageObject->getParam(TStageObject::T_Y, frame);
  std::vector<QPointF> translationLocalDragPoints;
  const bool translationMouseEventsDelivered =
      gui_smoke_replay_viewer_mouse_drag(
          viewer,
          {TPointD(handleUnit * 70.0, handleUnit * 70.0),
           TPointD(handleUnit * 100.0, handleUnit * 35.0)},
          &translationLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const double translationAfterX =
      stageObject->getParam(TStageObject::T_X, frame);
  const double translationAfterY =
      stageObject->getParam(TStageObject::T_Y, frame);
  const bool translationOk =
      translationMouseEventsDelivered &&
      std::abs(translationAfterX - translationBeforeX) > 0.0001 &&
      std::abs(translationAfterY - translationBeforeY) > 0.0001;

  restoreObject();
  const double scaleXBefore =
      stageObject->getParam(TStageObject::T_ScaleX, frame);
  const double scaleYBefore =
      stageObject->getParam(TStageObject::T_ScaleY, frame);
  const TPointD scaleXYHandle(-handleUnit * 20.0, -handleUnit * 20.0);
  const TPointD scaleXYEnd(-handleUnit * 42.0, -handleUnit * 28.0);
  std::vector<QPointF> scaleXYLocalDragPoints;
  const bool scaleXYMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, {scaleXYHandle, scaleXYEnd}, &scaleXYLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const double scaleXAfter =
      stageObject->getParam(TStageObject::T_ScaleX, frame);
  const double scaleYAfter =
      stageObject->getParam(TStageObject::T_ScaleY, frame);
  const bool scaleXYOk = scaleXYMouseEventsDelivered &&
                         std::abs(scaleXAfter - scaleXBefore) > 0.001 &&
                         std::abs(scaleYAfter - scaleYBefore) > 0.001 &&
                         std::abs((scaleXAfter - scaleXBefore) -
                                  (scaleYAfter - scaleYBefore)) > 0.0001;

  restoreObject();
  const double shearXBefore =
      stageObject->getParam(TStageObject::T_ShearX, frame);
  const double shearYBefore =
      stageObject->getParam(TStageObject::T_ShearY, frame);
  const TPointD shearHandle(handleUnit * 30.0, -handleUnit * 30.0);
  const TPointD shearEnd(handleUnit * 55.0, -handleUnit * 12.0);
  std::vector<QPointF> shearLocalDragPoints;
  const bool shearMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, {shearHandle, shearEnd}, &shearLocalDragPoints);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const double shearXAfter =
      stageObject->getParam(TStageObject::T_ShearX, frame);
  const double shearYAfter =
      stageObject->getParam(TStageObject::T_ShearY, frame);
  const bool shearOk = shearMouseEventsDelivered &&
                       std::abs(shearXAfter - shearXBefore) > 0.0001 &&
                       std::abs(shearYAfter - shearYBefore) > 0.0001;

  const bool mouseEventsDelivered = translationMouseEventsDelivered &&
                                    scaleXYMouseEventsDelivered &&
                                    shearMouseEventsDelivered;
  const bool handleVariantOk =
      activeAxisOk && translationOk && scaleXYOk && shearOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("activeAxis=%1").arg(gui_smoke_status_value(activeAxisValue))
      << QString("handleUnit=%1").arg(handleUnit, 0, 'f', 4)
      << QString("stageObject=%1")
             .arg(QString::fromStdString(objectId.toString()))
      << QString("translationBefore=%1,%2")
             .arg(translationBeforeX, 0, 'f', 4)
             .arg(translationBeforeY, 0, 'f', 4)
      << QString("translationAfter=%1,%2")
             .arg(translationAfterX, 0, 'f', 4)
             .arg(translationAfterY, 0, 'f', 4)
      << QString("scaleXBefore=%1").arg(scaleXBefore, 0, 'f', 4)
      << QString("scaleYBefore=%1").arg(scaleYBefore, 0, 'f', 4)
      << QString("scaleXAfterScaleXYHandle=%1").arg(scaleXAfter, 0, 'f', 4)
      << QString("scaleYAfterScaleXYHandle=%1").arg(scaleYAfter, 0, 'f', 4)
      << QString("shearXBefore=%1").arg(shearXBefore, 0, 'f', 4)
      << QString("shearYBefore=%1").arg(shearYBefore, 0, 'f', 4)
      << QString("shearXAfterShearHandle=%1").arg(shearXAfter, 0, 'f', 4)
      << QString("shearYAfterShearHandle=%1").arg(shearYAfter, 0, 'f', 4)
      << QString("translationLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(translationLocalDragPoints))
      << QString("scaleXYHandleWorld=%1,%2")
             .arg(scaleXYHandle.x, 0, 'f', 4)
             .arg(scaleXYHandle.y, 0, 'f', 4)
      << QString("scaleXYHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(scaleXYLocalDragPoints))
      << QString("shearHandleWorld=%1,%2")
             .arg(shearHandle.x, 0, 'f', 4)
             .arg(shearHandle.y, 0, 'f', 4)
      << QString("shearHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(shearLocalDragPoints))
      << QString("activeAxisProbe=%1")
             .arg(activeAxisOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("translationFallbackProbe=%1")
             .arg(translationOk ? QStringLiteral("ok")
                                : QStringLiteral("error"))
      << QString("scaleXYHandleProbe=%1")
             .arg(scaleXYOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("shearHandleProbe=%1")
             .arg(shearOk ? QStringLiteral("ok") : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("handleVariantProbe=%1")
             .arg(handleVariantOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-handle-variants"),
      QStringLiteral("animate-tool-handle-variants"), handleVariantOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_axis_drags_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("axisDragProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("axisDragProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_axis_drags");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("axisDragProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("axisDragProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("axisDragProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const int frame = 0;
  TApp::instance()->getCurrentFrame()->setFrame(frame);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("axisDragProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("axisDragProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("axisDragProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const GuiSmokeStageObjectBaseline baseline =
      gui_smoke_capture_stage_object_baseline(xsheet, objectId, stageObject,
                                              frame);
  const double handleUnit = tool->getPixelSize() *
                            Preferences::instance()->getAnimateToolHandleSize();

  auto restoreObject = [&]() {
    gui_smoke_restore_stage_object_baseline(xsheet, objectId, stageObject,
                                            frame, baseline);
    tool->updateMatrix();
    viewer->GLInvalidateAll();
    gui_smoke_pump_events(80);
  };

  const GuiSmokeAnimateAxisDragProbe positionProbe =
      gui_smoke_probe_animate_axis_drag(
          viewer, tool, xsheet, objectId, stageObject, frame,
          QStringLiteral("Position"), L"Position",
          GuiSmokeAnimateAxisDragKind::Position,
          {TPointD(0.0, 0.0), TPointD(72.0, -36.0), TPointD(144.0, -72.0)});

  restoreObject();
  const GuiSmokeAnimateAxisDragProbe rotationProbe =
      gui_smoke_probe_animate_axis_drag(
          viewer, tool, xsheet, objectId, stageObject, frame,
          QStringLiteral("Rotation"), L"Rotation",
          GuiSmokeAnimateAxisDragKind::Rotation,
          {TPointD(0.0, handleUnit * 30.0),
           TPointD(handleUnit * 30.0, handleUnit * 30.0)});

  restoreObject();
  const GuiSmokeAnimateAxisDragProbe scaleProbe =
      gui_smoke_probe_animate_axis_drag(
          viewer, tool, xsheet, objectId, stageObject, frame,
          QStringLiteral("Scale"), L"Scale", GuiSmokeAnimateAxisDragKind::Scale,
          {TPointD(-handleUnit * 30.0, -handleUnit * 30.0),
           TPointD(-handleUnit * 45.0, -handleUnit * 45.0)});

  restoreObject();
  const GuiSmokeAnimateAxisDragProbe shearProbe =
      gui_smoke_probe_animate_axis_drag(
          viewer, tool, xsheet, objectId, stageObject, frame,
          QStringLiteral("Shear"), L"Shear", GuiSmokeAnimateAxisDragKind::Shear,
          {TPointD(handleUnit * 30.0, handleUnit * 30.0),
           TPointD(handleUnit * 55.0, handleUnit * 12.0)});

  restoreObject();
  const GuiSmokeAnimateAxisDragProbe centerProbe =
      gui_smoke_probe_animate_axis_drag(
          viewer, tool, xsheet, objectId, stageObject, frame,
          QStringLiteral("Center"), L"Center",
          GuiSmokeAnimateAxisDragKind::Center,
          {TPointD(0.0, 0.0), TPointD(handleUnit * 24.0, handleUnit * 12.0)});

  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool mouseEventsDelivered =
      positionProbe.mouseEventsDelivered &&
      rotationProbe.mouseEventsDelivered && scaleProbe.mouseEventsDelivered &&
      shearProbe.mouseEventsDelivered && centerProbe.mouseEventsDelivered;
  const bool axisDragOk = objectOk && positionProbe.ok() &&
                          rotationProbe.ok() && scaleProbe.ok() &&
                          shearProbe.ok() && centerProbe.ok();

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(objectId.toString()))
          << QString("handleUnit=%1").arg(handleUnit, 0, 'f', 4);
  gui_smoke_append_animate_axis_drag_probe_details(details, positionProbe);
  gui_smoke_append_animate_axis_drag_probe_details(details, rotationProbe);
  gui_smoke_append_animate_axis_drag_probe_details(details, scaleProbe);
  gui_smoke_append_animate_axis_drag_probe_details(details, shearProbe);
  gui_smoke_append_animate_axis_drag_probe_details(details, centerProbe);
  details << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
          << QString("axisDragProbe=%1")
                 .arg(axisDragOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-axis-drags"),
      QStringLiteral("animate-tool-axis-drags"), axisDragOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_cursors_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("animateCursorProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("animateCursorProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_cursors");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("animateCursorProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("animateCursorProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject* stageObject     = xsheet->getStageObject(objectId);
  if (!stageObject) {
    details << QStringLiteral("viewerRenderProbe=stage-object-error")
            << QStringLiteral("animateCursorProbe=stage-object-error")
            << QStringLiteral("mouseEventProbe=stage-object-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("animateCursorProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Hand);
  TTool* baselineTool = toolHandle->getTool();
  if (!baselineTool || baselineTool->getName() != T_Hand) {
    details << QStringLiteral("viewerRenderProbe=baseline-tool-error")
            << QStringLiteral("animateCursorProbe=baseline-tool-error")
            << QStringLiteral("mouseEventProbe=baseline-tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  baselineTool->setViewer(viewer);
  baselineTool->updateMatrix();
  baselineTool->updateEnabled();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  toolHandle->setTool(T_Edit);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Edit) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("animateCursorProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("animateCursorProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TPointD probeWorldPos(0.0, 0.0);
  const GuiSmokeToolCursorProbe positionProbe =
      gui_smoke_probe_animate_tool_cursor(
          viewer, tool, QStringLiteral("Position"), L"Position",
          ToolCursor::MoveCursor, probeWorldPos);
  const GuiSmokeToolCursorProbe rotationProbe =
      gui_smoke_probe_animate_tool_cursor(
          viewer, tool, QStringLiteral("Rotation"), L"Rotation",
          ToolCursor::RotCursor, probeWorldPos);
  const GuiSmokeToolCursorProbe scaleProbe =
      gui_smoke_probe_animate_tool_cursor(
          viewer, tool, QStringLiteral("Scale"), L"Scale",
          ToolCursor::ScaleGlobalCursor, probeWorldPos);
  const GuiSmokeToolCursorProbe shearProbe =
      gui_smoke_probe_animate_tool_cursor(viewer, tool, QStringLiteral("Shear"),
                                          L"Shear", ToolCursor::ScaleCursor,
                                          probeWorldPos);
  const GuiSmokeToolCursorProbe centerProbe =
      gui_smoke_probe_animate_tool_cursor(
          viewer, tool, QStringLiteral("Center"), L"Center",
          ToolCursor::MoveCursor, probeWorldPos);

  const bool mouseEventsDelivered =
      positionProbe.normalEventDelivered && positionProbe.altEventDelivered &&
      positionProbe.resetEventDelivered && rotationProbe.normalEventDelivered &&
      rotationProbe.altEventDelivered && rotationProbe.resetEventDelivered &&
      scaleProbe.normalEventDelivered && scaleProbe.altEventDelivered &&
      scaleProbe.resetEventDelivered && shearProbe.normalEventDelivered &&
      shearProbe.altEventDelivered && shearProbe.resetEventDelivered &&
      centerProbe.normalEventDelivered && centerProbe.altEventDelivered &&
      centerProbe.resetEventDelivered;
  const bool animateCursorOk = positionProbe.ok() && rotationProbe.ok() &&
                               scaleProbe.ok() && shearProbe.ok() &&
                               centerProbe.ok();

  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("stageObject=%1")
                 .arg(QString::fromStdString(objectId.toString()));
  gui_smoke_append_tool_cursor_probe_details(details, positionProbe);
  gui_smoke_append_tool_cursor_probe_details(details, rotationProbe);
  gui_smoke_append_tool_cursor_probe_details(details, scaleProbe);
  gui_smoke_append_tool_cursor_probe_details(details, shearProbe);
  gui_smoke_append_tool_cursor_probe_details(details, centerProbe);
  details << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
          << QString("animateCursorProbe=%1")
                 .arg(animateCursorOk ? QStringLiteral("ok")
                                      : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-cursors"),
      QStringLiteral("animate-tool-cursors"), animateCursorOk);

  return details;
}

static QStringList gui_smoke_viewer_selection_tool_vector_handles_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionRectProbe=no-viewer")
            << QStringLiteral("selectionCursorProbe=no-viewer")
            << QStringLiteral("selectionHandleProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionRectProbe=scene-folder-error")
            << QStringLiteral("selectionCursorProbe=scene-folder-error")
            << QStringLiteral("selectionHandleProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      PLI_XSHLEVEL, L"qt6_gui_viewer_selection_tool_vector_handles");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionRectProbe=level-error")
            << QStringLiteral("selectionCursorProbe=level-error")
            << QStringLiteral("selectionHandleProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  if (!gui_smoke_add_vector_probe_frame(simpleLevel, fid)) {
    details << QStringLiteral("viewerRenderProbe=vector-frame-error")
            << QStringLiteral("selectionToolProbe=vector-frame-error")
            << QStringLiteral("selectionRectProbe=vector-frame-error")
            << QStringLiteral("selectionCursorProbe=vector-frame-error")
            << QStringLiteral("selectionHandleProbe=vector-frame-error")
            << QStringLiteral("mouseEventProbe=vector-frame-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionRectProbe=xsheet-error")
            << QStringLiteral("selectionCursorProbe=xsheet-error")
            << QStringLiteral("selectionHandleProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionRectProbe=tool-handle-error")
            << QStringLiteral("selectionCursorProbe=tool-handle-error")
            << QStringLiteral("selectionHandleProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionRectProbe=tool-error")
            << QStringLiteral("selectionCursorProbe=tool-error")
            << QStringLiteral("selectionHandleProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionRectProbe=tool-disabled")
            << QStringLiteral("selectionCursorProbe=tool-disabled")
            << QStringLiteral("selectionHandleProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  const bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  QString selectionTypeValue;
  const bool selectionTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Rectangular", &selectionTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  TVectorImageP image = (TVectorImageP)simpleLevel->getFrame(fid, false);
  TRectD strokeBBoxBefore;
  if (image && image->getStrokeCount() > 0) {
    strokeBBoxBefore = image->getStroke(0)->getBBox();
  }

  std::vector<QPointF> selectionRectLocalPoints;
  bool selectionRectMouseEventsDelivered = false;
  if (!strokeBBoxBefore.isEmpty()) {
    const TPointD selectStart(strokeBBoxBefore.x0 - 80.0,
                              strokeBBoxBefore.y0 - 80.0);
    const TPointD selectEnd(strokeBBoxBefore.x1 + 80.0,
                            strokeBBoxBefore.y1 + 80.0);
    selectionRectMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {selectStart, selectEnd}, &selectionRectLocalPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  StrokeSelection* selection =
      dynamic_cast<StrokeSelection*>(tool->getSelection());
  const int selectionCount =
      selection ? static_cast<int>(selection->getSelection().size()) : -1;
  const bool stroke0Selected = selection && selection->isSelected(0);
  const bool selectionRectOk = selectionRectMouseEventsDelivered &&
                               selectionCount == 1 && stroke0Selected;

  image = (TVectorImageP)simpleLevel->getFrame(fid, false);
  TRectD selectionBBoxBeforeScale;
  if (image && image->getStrokeCount() > 0) {
    selectionBBoxBeforeScale = image->getStroke(0)->getBBox();
  }

  const TPointD scaleHandle = selectionBBoxBeforeScale.getP11();
  const TPointD scaleEnd(selectionBBoxBeforeScale.x1 + 70.0,
                         selectionBBoxBeforeScale.y1 + 50.0);
  const QPointF scaleHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, scaleHandle);
  bool scaleHoverDelivered  = false;
  int scaleCursorId         = ToolCursor::CURSOR_NONE;
  bool scaleCursorArtworkOk = false;
  GuiSmokeCursorArtworkSignature scaleCursorArtwork;
  if (selectionRectOk && !selectionBBoxBeforeScale.isEmpty() &&
      viewer->rect().contains(scaleHandleLocal.toPoint())) {
    scaleHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, scaleHandleLocal, Qt::NoButton, Qt::NoButton,
        Qt::NoModifier);
    scaleCursorId        = tool->getCursorId();
    scaleCursorArtworkOk = gui_smoke_tool_cursor_pixmap_ok(scaleCursorId);
    scaleCursorArtwork   = gui_smoke_cursor_artwork_signature(scaleCursorId);
  }
  const bool selectionCursorOk = scaleHoverDelivered && scaleCursorArtworkOk &&
                                 scaleCursorArtwork.ok &&
                                 (scaleCursorId == ToolCursor::ScaleCursor ||
                                  scaleCursorId == ToolCursor::ScaleInvCursor);

  std::vector<QPointF> scaleLocalDragPoints;
  bool scaleMouseEventsDelivered = false;
  if (selectionRectOk && !selectionBBoxBeforeScale.isEmpty()) {
    scaleMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {scaleHandle, scaleEnd}, &scaleLocalDragPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  image = (TVectorImageP)simpleLevel->getFrame(fid, false);
  TRectD selectionBBoxAfterScale;
  if (image && image->getStrokeCount() > 0) {
    image->validateRegions(true);
    selectionBBoxAfterScale = image->getStroke(0)->getBBox();
  }
  simpleLevel->setDirtyFlag(true);

  const double bboxWidthDelta =
      selectionBBoxAfterScale.getLx() - selectionBBoxBeforeScale.getLx();
  const double bboxHeightDelta =
      selectionBBoxAfterScale.getLy() - selectionBBoxBeforeScale.getLy();
  const bool selectionHandleOk =
      scaleMouseEventsDelivered && !selectionBBoxBeforeScale.isEmpty() &&
      !selectionBBoxAfterScale.isEmpty() &&
      (std::abs(bboxWidthDelta) > 0.5 || std::abs(bboxHeightDelta) > 0.5);
  const bool selectionToolOk =
      selectionModeOk && selectionTypeOk && tool->isEnabled();
  const bool mouseEventsDelivered = selectionRectMouseEventsDelivered &&
                                    scaleHoverDelivered &&
                                    scaleMouseEventsDelivered;
  const bool selectionOk = selectionToolOk && selectionRectOk &&
                           selectionCursorOk && selectionHandleOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("selectionMode=%1")
             .arg(gui_smoke_status_value(selectionModeValue))
      << QString("selectionType=%1")
             .arg(gui_smoke_status_value(selectionTypeValue))
      << QString("strokeBBoxBefore=%1")
             .arg(gui_smoke_rect_details(strokeBBoxBefore))
      << QString("selectionCount=%1").arg(selectionCount)
      << QString("selectionStroke0Selected=%1")
             .arg(stroke0Selected ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("selectionBBoxBeforeScale=%1")
             .arg(gui_smoke_rect_details(selectionBBoxBeforeScale))
      << QString("selectionBBoxAfterScale=%1")
             .arg(gui_smoke_rect_details(selectionBBoxAfterScale))
      << QString("selectionBBoxWidthDelta=%1").arg(bboxWidthDelta, 0, 'f', 4)
      << QString("selectionBBoxHeightDelta=%1").arg(bboxHeightDelta, 0, 'f', 4)
      << QString("selectionRectLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(selectionRectLocalPoints))
      << QString("selectionScaleHandleWorld=%1,%2")
             .arg(scaleHandle.x, 0, 'f', 4)
             .arg(scaleHandle.y, 0, 'f', 4)
      << QString("selectionScaleHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(scaleLocalDragPoints))
      << QString("selectionScaleCursorId=%1").arg(scaleCursorId)
      << QString("selectionScaleCursorArtwork=%1")
             .arg(scaleCursorArtworkOk ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("selectionScaleCursorArtworkSignature=%1")
             .arg(gui_smoke_cursor_artwork_summary(scaleCursorArtwork))
      << QString("selectionRectMouseEvents=%1")
             .arg(selectionRectMouseEventsDelivered ? QStringLiteral("ok")
                                                    : QStringLiteral("error"))
      << QString("selectionScaleHoverEvent=%1")
             .arg(scaleHoverDelivered ? QStringLiteral("ok")
                                      : QStringLiteral("error"))
      << QString("selectionScaleMouseEvents=%1")
             .arg(scaleMouseEventsDelivered ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
      << QString("selectionToolProbe=%1")
             .arg(selectionToolOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionRectProbe=%1")
             .arg(selectionRectOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionCursorProbe=%1")
             .arg(selectionCursorOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("selectionHandleProbe=%1")
             .arg(selectionHandleOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-selection-tool-vector-handles"),
      QStringLiteral("selection-tool-vector-handles"), selectionOk);

  return details;
}

static QStringList
gui_smoke_viewer_selection_tool_vector_handle_variants_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionRectProbe=no-viewer")
            << QStringLiteral("selectionVariantCursorProbe=no-viewer")
            << QStringLiteral("selectionVariantHandleProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionRectProbe=scene-folder-error")
            << QStringLiteral("selectionVariantCursorProbe=scene-folder-error")
            << QStringLiteral("selectionVariantHandleProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      PLI_XSHLEVEL, L"qt6_gui_viewer_selection_tool_vector_handle_variants");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionRectProbe=level-error")
            << QStringLiteral("selectionVariantCursorProbe=level-error")
            << QStringLiteral("selectionVariantHandleProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  if (!gui_smoke_add_vector_probe_frame(simpleLevel, fid)) {
    details << QStringLiteral("viewerRenderProbe=vector-frame-error")
            << QStringLiteral("selectionToolProbe=vector-frame-error")
            << QStringLiteral("selectionRectProbe=vector-frame-error")
            << QStringLiteral("selectionVariantCursorProbe=vector-frame-error")
            << QStringLiteral("selectionVariantHandleProbe=vector-frame-error")
            << QStringLiteral("mouseEventProbe=vector-frame-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionRectProbe=xsheet-error")
            << QStringLiteral("selectionVariantCursorProbe=xsheet-error")
            << QStringLiteral("selectionVariantHandleProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionRectProbe=tool-handle-error")
            << QStringLiteral("selectionVariantCursorProbe=tool-handle-error")
            << QStringLiteral("selectionVariantHandleProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionRectProbe=tool-error")
            << QStringLiteral("selectionVariantCursorProbe=tool-error")
            << QStringLiteral("selectionVariantHandleProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionRectProbe=tool-disabled")
            << QStringLiteral("selectionVariantCursorProbe=tool-disabled")
            << QStringLiteral("selectionVariantHandleProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  const bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  QString selectionTypeValue;
  const bool selectionTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Rectangular", &selectionTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  auto strokeBBox = [&]() {
    TVectorImageP image = (TVectorImageP)simpleLevel->getFrame(fid, false);
    if (!image || image->getStrokeCount() <= 0) return TRectD();
    image->validateRegions(true);
    return image->getStroke(0)->getBBox();
  };

  const TRectD strokeBBoxBefore = strokeBBox();
  std::vector<QPointF> selectionRectLocalPoints;
  bool selectionRectMouseEventsDelivered = false;
  if (!strokeBBoxBefore.isEmpty()) {
    const TPointD selectStart(strokeBBoxBefore.x0 - 80.0,
                              strokeBBoxBefore.y0 - 80.0);
    const TPointD selectEnd(strokeBBoxBefore.x1 + 80.0,
                            strokeBBoxBefore.y1 + 80.0);
    selectionRectMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {selectStart, selectEnd}, &selectionRectLocalPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  StrokeSelection* selection =
      dynamic_cast<StrokeSelection*>(tool->getSelection());
  const int selectionCount =
      selection ? static_cast<int>(selection->getSelection().size()) : -1;
  const bool stroke0Selected = selection && selection->isSelected(0);
  const bool selectionRectOk = selectionRectMouseEventsDelivered &&
                               selectionCount == 1 && stroke0Selected;

  const TRectD bboxBeforeHorizontalScale = strokeBBox();
  const double horizontalMidY =
      (bboxBeforeHorizontalScale.y0 + bboxBeforeHorizontalScale.y1) * 0.5;
  const TPointD horizontalScaleHandle(bboxBeforeHorizontalScale.x1,
                                      horizontalMidY);
  const TPointD horizontalScaleEnd(bboxBeforeHorizontalScale.x1 + 55.0,
                                   horizontalMidY);
  const QPointF horizontalScaleHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, horizontalScaleHandle);
  bool horizontalScaleHoverDelivered  = false;
  int horizontalScaleCursorId         = ToolCursor::CURSOR_NONE;
  bool horizontalScaleCursorArtworkOk = false;
  if (selectionRectOk && !bboxBeforeHorizontalScale.isEmpty() &&
      viewer->rect().contains(horizontalScaleHandleLocal.toPoint())) {
    horizontalScaleHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, horizontalScaleHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    horizontalScaleCursorId = tool->getCursorId();
    horizontalScaleCursorArtworkOk =
        gui_smoke_tool_cursor_pixmap_ok(horizontalScaleCursorId);
  }
  const bool horizontalScaleCursorOk =
      horizontalScaleHoverDelivered && horizontalScaleCursorArtworkOk &&
      horizontalScaleCursorId == ToolCursor::ScaleHCursor;

  std::vector<QPointF> horizontalScaleLocalDragPoints;
  bool horizontalScaleMouseEventsDelivered = false;
  if (selectionRectOk && !bboxBeforeHorizontalScale.isEmpty()) {
    horizontalScaleMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {horizontalScaleHandle, horizontalScaleEnd},
        &horizontalScaleLocalDragPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  const TRectD bboxAfterHorizontalScale = strokeBBox();
  const double horizontalWidthDelta =
      bboxAfterHorizontalScale.getLx() - bboxBeforeHorizontalScale.getLx();
  const bool horizontalScaleOk = horizontalScaleMouseEventsDelivered &&
                                 !bboxBeforeHorizontalScale.isEmpty() &&
                                 !bboxAfterHorizontalScale.isEmpty() &&
                                 std::abs(horizontalWidthDelta) > 0.5;

  const TRectD bboxBeforeVerticalScale = bboxAfterHorizontalScale;
  const double verticalMidX =
      (bboxBeforeVerticalScale.x0 + bboxBeforeVerticalScale.x1) * 0.5;
  const TPointD verticalScaleHandle(verticalMidX, bboxBeforeVerticalScale.y1);
  const TPointD verticalScaleEnd(verticalMidX,
                                 bboxBeforeVerticalScale.y1 + 45.0);
  const QPointF verticalScaleHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, verticalScaleHandle);
  bool verticalScaleHoverDelivered  = false;
  int verticalScaleCursorId         = ToolCursor::CURSOR_NONE;
  bool verticalScaleCursorArtworkOk = false;
  if (selectionRectOk && !bboxBeforeVerticalScale.isEmpty() &&
      viewer->rect().contains(verticalScaleHandleLocal.toPoint())) {
    verticalScaleHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, verticalScaleHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    verticalScaleCursorId = tool->getCursorId();
    verticalScaleCursorArtworkOk =
        gui_smoke_tool_cursor_pixmap_ok(verticalScaleCursorId);
  }
  const bool verticalScaleCursorOk =
      verticalScaleHoverDelivered && verticalScaleCursorArtworkOk &&
      verticalScaleCursorId == ToolCursor::ScaleVCursor;

  std::vector<QPointF> verticalScaleLocalDragPoints;
  bool verticalScaleMouseEventsDelivered = false;
  if (selectionRectOk && !bboxBeforeVerticalScale.isEmpty()) {
    verticalScaleMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {verticalScaleHandle, verticalScaleEnd},
        &verticalScaleLocalDragPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  const TRectD bboxAfterVerticalScale = strokeBBox();
  const double verticalHeightDelta =
      bboxAfterVerticalScale.getLy() - bboxBeforeVerticalScale.getLy();
  const bool verticalScaleOk =
      verticalScaleMouseEventsDelivered && !bboxBeforeVerticalScale.isEmpty() &&
      !bboxAfterVerticalScale.isEmpty() && std::abs(verticalHeightDelta) > 0.5;

  const TRectD bboxBeforeRotation = bboxAfterVerticalScale;
  const double pixelSize          = tool->getPixelSize();
  const TPointD rotationHandle(bboxBeforeRotation.x1 + 15.0 * pixelSize,
                               bboxBeforeRotation.y1 + 15.0 * pixelSize);
  const TPointD rotationEnd(rotationHandle.x - 55.0, rotationHandle.y + 35.0);
  const QPointF rotationHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, rotationHandle);
  bool rotationHoverDelivered  = false;
  int rotationCursorId         = ToolCursor::CURSOR_NONE;
  bool rotationCursorArtworkOk = false;
  if (selectionRectOk && !bboxBeforeRotation.isEmpty() &&
      viewer->rect().contains(rotationHandleLocal.toPoint())) {
    rotationHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, rotationHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    rotationCursorId        = tool->getCursorId();
    rotationCursorArtworkOk = gui_smoke_tool_cursor_pixmap_ok(rotationCursorId);
  }
  const bool rotationCursorOk = rotationHoverDelivered &&
                                rotationCursorArtworkOk &&
                                rotationCursorId == ToolCursor::RotCursor;

  std::vector<QPointF> rotationLocalDragPoints;
  bool rotationMouseEventsDelivered = false;
  if (selectionRectOk && !bboxBeforeRotation.isEmpty()) {
    rotationMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {rotationHandle, rotationEnd}, &rotationLocalDragPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(180);

  const TRectD bboxAfterRotation = strokeBBox();
  const bool rotationBBoxChanged =
      !bboxBeforeRotation.isEmpty() && !bboxAfterRotation.isEmpty() &&
      (std::abs(bboxAfterRotation.x0 - bboxBeforeRotation.x0) > 0.5 ||
       std::abs(bboxAfterRotation.y0 - bboxBeforeRotation.y0) > 0.5 ||
       std::abs(bboxAfterRotation.x1 - bboxBeforeRotation.x1) > 0.5 ||
       std::abs(bboxAfterRotation.y1 - bboxBeforeRotation.y1) > 0.5);
  const bool rotationOk = rotationMouseEventsDelivered && rotationBBoxChanged;

  simpleLevel->setDirtyFlag(true);

  const bool selectionToolOk =
      selectionModeOk && selectionTypeOk && tool->isEnabled();
  const bool selectionVariantCursorOk =
      horizontalScaleCursorOk && verticalScaleCursorOk && rotationCursorOk;
  const bool selectionVariantHandleOk =
      horizontalScaleOk && verticalScaleOk && rotationOk;
  const bool mouseEventsDelivered =
      selectionRectMouseEventsDelivered && horizontalScaleHoverDelivered &&
      horizontalScaleMouseEventsDelivered && verticalScaleHoverDelivered &&
      verticalScaleMouseEventsDelivered && rotationHoverDelivered &&
      rotationMouseEventsDelivered;
  const bool selectionOk = selectionToolOk && selectionRectOk &&
                           selectionVariantCursorOk && selectionVariantHandleOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("selectionMode=%1")
             .arg(gui_smoke_status_value(selectionModeValue))
      << QString("selectionType=%1")
             .arg(gui_smoke_status_value(selectionTypeValue))
      << QString("strokeBBoxBefore=%1")
             .arg(gui_smoke_rect_details(strokeBBoxBefore))
      << QString("selectionCount=%1").arg(selectionCount)
      << QString("selectionStroke0Selected=%1")
             .arg(stroke0Selected ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("selectionRectLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(selectionRectLocalPoints))
      << QString("selectionBBoxBeforeHorizontalScale=%1")
             .arg(gui_smoke_rect_details(bboxBeforeHorizontalScale))
      << QString("selectionBBoxAfterHorizontalScale=%1")
             .arg(gui_smoke_rect_details(bboxAfterHorizontalScale))
      << QString("selectionHorizontalWidthDelta=%1")
             .arg(horizontalWidthDelta, 0, 'f', 4)
      << QString("selectionHorizontalScaleCursorId=%1")
             .arg(horizontalScaleCursorId)
      << QString("selectionHorizontalScaleCursorArtwork=%1")
             .arg(horizontalScaleCursorArtworkOk ? QStringLiteral("ok")
                                                 : QStringLiteral("error"))
      << QString("selectionHorizontalScaleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(horizontalScaleLocalDragPoints))
      << QString("selectionBBoxBeforeVerticalScale=%1")
             .arg(gui_smoke_rect_details(bboxBeforeVerticalScale))
      << QString("selectionBBoxAfterVerticalScale=%1")
             .arg(gui_smoke_rect_details(bboxAfterVerticalScale))
      << QString("selectionVerticalHeightDelta=%1")
             .arg(verticalHeightDelta, 0, 'f', 4)
      << QString("selectionVerticalScaleCursorId=%1").arg(verticalScaleCursorId)
      << QString("selectionVerticalScaleCursorArtwork=%1")
             .arg(verticalScaleCursorArtworkOk ? QStringLiteral("ok")
                                               : QStringLiteral("error"))
      << QString("selectionVerticalScaleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(verticalScaleLocalDragPoints))
      << QString("selectionBBoxBeforeRotation=%1")
             .arg(gui_smoke_rect_details(bboxBeforeRotation))
      << QString("selectionBBoxAfterRotation=%1")
             .arg(gui_smoke_rect_details(bboxAfterRotation))
      << QString("selectionRotationCursorId=%1").arg(rotationCursorId)
      << QString("selectionRotationCursorArtwork=%1")
             .arg(rotationCursorArtworkOk ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
      << QString("selectionRotationLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(rotationLocalDragPoints))
      << QString("selectionRectMouseEvents=%1")
             .arg(selectionRectMouseEventsDelivered ? QStringLiteral("ok")
                                                    : QStringLiteral("error"))
      << QString("selectionHorizontalScaleHoverEvent=%1")
             .arg(horizontalScaleHoverDelivered ? QStringLiteral("ok")
                                                : QStringLiteral("error"))
      << QString("selectionHorizontalScaleMouseEvents=%1")
             .arg(horizontalScaleMouseEventsDelivered ? QStringLiteral("ok")
                                                      : QStringLiteral("error"))
      << QString("selectionVerticalScaleHoverEvent=%1")
             .arg(verticalScaleHoverDelivered ? QStringLiteral("ok")
                                              : QStringLiteral("error"))
      << QString("selectionVerticalScaleMouseEvents=%1")
             .arg(verticalScaleMouseEventsDelivered ? QStringLiteral("ok")
                                                    : QStringLiteral("error"))
      << QString("selectionRotationHoverEvent=%1")
             .arg(rotationHoverDelivered ? QStringLiteral("ok")
                                         : QStringLiteral("error"))
      << QString("selectionRotationMouseEvents=%1")
             .arg(rotationMouseEventsDelivered ? QStringLiteral("ok")
                                               : QStringLiteral("error"))
      << QString("selectionToolProbe=%1")
             .arg(selectionToolOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionRectProbe=%1")
             .arg(selectionRectOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionVariantCursorProbe=%1")
             .arg(selectionVariantCursorOk ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
      << QString("selectionVariantHandleProbe=%1")
             .arg(selectionVariantHandleOk ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before,
      QStringLiteral("viewer-selection-tool-vector-handle-variants"),
      QStringLiteral("selection-tool-vector-handle-variants"), selectionOk);

  return details;
}

static QStringList
gui_smoke_viewer_selection_tool_vector_center_thickness_deform_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionRectProbe=no-viewer")
            << QStringLiteral("selectionAdvancedCursorProbe=no-viewer")
            << QStringLiteral("selectionAdvancedHandleProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionRectProbe=scene-folder-error")
            << QStringLiteral("selectionAdvancedCursorProbe=scene-folder-error")
            << QStringLiteral("selectionAdvancedHandleProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      PLI_XSHLEVEL, L"qt6_gui_viewer_selection_tool_vector_center_thickness");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionRectProbe=level-error")
            << QStringLiteral("selectionAdvancedCursorProbe=level-error")
            << QStringLiteral("selectionAdvancedHandleProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  if (!gui_smoke_add_vector_probe_frame(simpleLevel, fid)) {
    details << QStringLiteral("viewerRenderProbe=vector-frame-error")
            << QStringLiteral("selectionToolProbe=vector-frame-error")
            << QStringLiteral("selectionRectProbe=vector-frame-error")
            << QStringLiteral("selectionAdvancedCursorProbe=vector-frame-error")
            << QStringLiteral("selectionAdvancedHandleProbe=vector-frame-error")
            << QStringLiteral("mouseEventProbe=vector-frame-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionRectProbe=xsheet-error")
            << QStringLiteral("selectionAdvancedCursorProbe=xsheet-error")
            << QStringLiteral("selectionAdvancedHandleProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionRectProbe=tool-handle-error")
            << QStringLiteral("selectionAdvancedCursorProbe=tool-handle-error")
            << QStringLiteral("selectionAdvancedHandleProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionRectProbe=tool-error")
            << QStringLiteral("selectionAdvancedCursorProbe=tool-error")
            << QStringLiteral("selectionAdvancedHandleProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(tool);
  if (!selectionTool) {
    details << QStringLiteral("viewerRenderProbe=tool-cast-error")
            << QStringLiteral("selectionToolProbe=tool-cast-error")
            << QStringLiteral("selectionRectProbe=tool-cast-error")
            << QStringLiteral("selectionAdvancedCursorProbe=tool-cast-error")
            << QStringLiteral("selectionAdvancedHandleProbe=tool-cast-error")
            << QStringLiteral("mouseEventProbe=tool-cast-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionRectProbe=tool-disabled")
            << QStringLiteral("selectionAdvancedCursorProbe=tool-disabled")
            << QStringLiteral("selectionAdvancedHandleProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  const bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  QString selectionTypeValue;
  const bool selectionTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Rectangular", &selectionTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  auto vectorImage = [&]() {
    return (TVectorImageP)simpleLevel->getFrame(fid, false);
  };
  auto strokeBBox = [&]() {
    TVectorImageP image = vectorImage();
    if (!image || image->getStrokeCount() <= 0) return TRectD();
    image->validateRegions(true);
    return image->getStroke(0)->getBBox();
  };
  auto averageThickness = [&]() {
    TVectorImageP image = vectorImage();
    if (!image || image->getStrokeCount() <= 0) return -1.0;
    const TStroke* stroke = image->getStroke(0);
    if (!stroke || stroke->getControlPointCount() <= 0) return -1.0;
    double total = 0.0;
    for (int i = 0; i < stroke->getControlPointCount(); ++i)
      total += stroke->getControlPoint(i).thick;
    return total / stroke->getControlPointCount();
  };

  const TRectD strokeBBoxBefore = strokeBBox();
  std::vector<QPointF> selectionRectLocalPoints;
  bool selectionRectMouseEventsDelivered = false;
  if (!strokeBBoxBefore.isEmpty()) {
    const TPointD selectStart(strokeBBoxBefore.x0 - 80.0,
                              strokeBBoxBefore.y0 - 80.0);
    const TPointD selectEnd(strokeBBoxBefore.x1 + 80.0,
                            strokeBBoxBefore.y1 + 80.0);
    selectionRectMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {selectStart, selectEnd}, &selectionRectLocalPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  StrokeSelection* selection =
      dynamic_cast<StrokeSelection*>(tool->getSelection());
  const int selectionCount =
      selection ? static_cast<int>(selection->getSelection().size()) : -1;
  const bool stroke0Selected = selection && selection->isSelected(0);
  const bool selectionRectOk = selectionRectMouseEventsDelivered &&
                               selectionCount == 1 && stroke0Selected;

  const TPointD centerBefore = selectionTool->getCenter();
  const TPointD centerEnd(centerBefore.x + 45.0, centerBefore.y - 35.0);
  const QPointF centerHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, centerBefore);
  bool centerHoverDelivered  = false;
  int centerCursorId         = ToolCursor::CURSOR_NONE;
  bool centerCursorArtworkOk = false;
  if (selectionRectOk && viewer->rect().contains(centerHandleLocal.toPoint())) {
    centerHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, centerHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    centerCursorId        = tool->getCursorId();
    centerCursorArtworkOk = gui_smoke_tool_cursor_pixmap_ok(centerCursorId);
  }
  const bool centerCursorOk = centerHoverDelivered && centerCursorArtworkOk &&
                              centerCursorId == ToolCursor::PointingHandCursor;

  std::vector<QPointF> centerLocalDragPoints;
  bool centerMouseEventsDelivered = false;
  if (selectionRectOk) {
    centerMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {centerBefore, centerEnd}, &centerLocalDragPoints);
  }
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const TPointD centerAfter = selectionTool->getCenter();
  const bool centerMoveOk =
      centerMouseEventsDelivered && norm(centerAfter - centerBefore) > 0.5;

  const TRectD bboxBeforeThickness = strokeBBox();
  const double thicknessBefore     = averageThickness();
  const double pixelSize           = tool->getPixelSize();
  const TPointD thicknessHandle    = bboxBeforeThickness.getP10() -
                                  TPointD(14.0 * pixelSize, 15.0 * pixelSize);
  const TPointD thicknessEnd(thicknessHandle.x, thicknessHandle.y + 45.0);
  const QPointF thicknessHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, thicknessHandle);
  bool thicknessHoverDelivered  = false;
  int thicknessCursorId         = ToolCursor::CURSOR_NONE;
  bool thicknessCursorArtworkOk = false;
  if (selectionRectOk && !bboxBeforeThickness.isEmpty() &&
      viewer->rect().contains(thicknessHandleLocal.toPoint())) {
    thicknessHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, thicknessHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::NoModifier);
    thicknessCursorId = tool->getCursorId();
    thicknessCursorArtworkOk =
        gui_smoke_tool_cursor_pixmap_ok(thicknessCursorId);
  }
  const bool thicknessCursorOk = thicknessHoverDelivered &&
                                 thicknessCursorArtworkOk &&
                                 thicknessCursorId == ToolCursor::PumpCursor;

  std::vector<QPointF> thicknessLocalDragPoints;
  bool thicknessMouseEventsDelivered = false;
  if (selectionRectOk && !bboxBeforeThickness.isEmpty()) {
    thicknessMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {thicknessHandle, thicknessEnd}, &thicknessLocalDragPoints);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(180);
  const double thicknessAfter = averageThickness();
  const double thicknessDelta = thicknessAfter - thicknessBefore;
  const bool thicknessOk      = thicknessMouseEventsDelivered &&
                           thicknessBefore >= 0.0 && thicknessAfter >= 0.0 &&
                           std::abs(thicknessDelta) > 0.5;

  const TRectD bboxBeforeDeform = strokeBBox();
  const TPointD deformHandle    = bboxBeforeDeform.getP11();
  const TPointD deformEnd(deformHandle.x + 42.0, deformHandle.y - 32.0);
  const QPointF deformHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, deformHandle);
  bool deformHoverDelivered  = false;
  int deformCursorId         = ToolCursor::CURSOR_NONE;
  bool deformCursorArtworkOk = false;
  if (selectionRectOk && !bboxBeforeDeform.isEmpty() &&
      viewer->rect().contains(deformHandleLocal.toPoint())) {
    deformHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, deformHandleLocal, Qt::NoButton,
        Qt::NoButton, Qt::ControlModifier);
    deformCursorId        = tool->getCursorId();
    deformCursorArtworkOk = gui_smoke_tool_cursor_pixmap_ok(deformCursorId);
  }
  const bool deformCursorOk = deformHoverDelivered && deformCursorArtworkOk &&
                              deformCursorId == ToolCursor::DistortCursor;

  std::vector<QPointF> deformLocalDragPoints;
  bool deformMouseEventsDelivered = false;
  if (selectionRectOk && !bboxBeforeDeform.isEmpty()) {
    deformMouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
        viewer, {deformHandle, deformEnd}, &deformLocalDragPoints,
        Qt::ControlModifier);
  }
  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(180);

  const TRectD bboxAfterDeform = strokeBBox();
  const bool deformBBoxChanged =
      !bboxBeforeDeform.isEmpty() && !bboxAfterDeform.isEmpty() &&
      (std::abs(bboxAfterDeform.x0 - bboxBeforeDeform.x0) > 0.5 ||
       std::abs(bboxAfterDeform.y0 - bboxBeforeDeform.y0) > 0.5 ||
       std::abs(bboxAfterDeform.x1 - bboxBeforeDeform.x1) > 0.5 ||
       std::abs(bboxAfterDeform.y1 - bboxBeforeDeform.y1) > 0.5);
  const bool deformOk = deformMouseEventsDelivered && deformBBoxChanged;

  simpleLevel->setDirtyFlag(true);

  const bool selectionToolOk =
      selectionModeOk && selectionTypeOk && tool->isEnabled();
  const bool selectionAdvancedCursorOk =
      centerCursorOk && thicknessCursorOk && deformCursorOk;
  const bool selectionAdvancedHandleOk =
      centerMoveOk && thicknessOk && deformOk;
  const bool mouseEventsDelivered =
      selectionRectMouseEventsDelivered && centerHoverDelivered &&
      centerMouseEventsDelivered && thicknessHoverDelivered &&
      thicknessMouseEventsDelivered && deformHoverDelivered &&
      deformMouseEventsDelivered;
  const bool selectionOk = selectionToolOk && selectionRectOk &&
                           selectionAdvancedCursorOk &&
                           selectionAdvancedHandleOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("selectionMode=%1")
             .arg(gui_smoke_status_value(selectionModeValue))
      << QString("selectionType=%1")
             .arg(gui_smoke_status_value(selectionTypeValue))
      << QString("strokeBBoxBefore=%1")
             .arg(gui_smoke_rect_details(strokeBBoxBefore))
      << QString("selectionCount=%1").arg(selectionCount)
      << QString("selectionStroke0Selected=%1")
             .arg(stroke0Selected ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("selectionRectLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(selectionRectLocalPoints))
      << QString("selectionCenterBefore=%1,%2")
             .arg(centerBefore.x, 0, 'f', 4)
             .arg(centerBefore.y, 0, 'f', 4)
      << QString("selectionCenterAfter=%1,%2")
             .arg(centerAfter.x, 0, 'f', 4)
             .arg(centerAfter.y, 0, 'f', 4)
      << QString("selectionCenterDelta=%1,%2")
             .arg(centerAfter.x - centerBefore.x, 0, 'f', 4)
             .arg(centerAfter.y - centerBefore.y, 0, 'f', 4)
      << QString("selectionCenterCursorId=%1").arg(centerCursorId)
      << QString("selectionCenterCursorArtwork=%1")
             .arg(centerCursorArtworkOk ? QStringLiteral("ok")
                                        : QStringLiteral("error"))
      << QString("selectionCenterLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(centerLocalDragPoints))
      << QString("selectionBBoxBeforeThickness=%1")
             .arg(gui_smoke_rect_details(bboxBeforeThickness))
      << QString("selectionThicknessBefore=%1").arg(thicknessBefore, 0, 'f', 4)
      << QString("selectionThicknessAfter=%1").arg(thicknessAfter, 0, 'f', 4)
      << QString("selectionThicknessDelta=%1").arg(thicknessDelta, 0, 'f', 4)
      << QString("selectionThicknessCursorId=%1").arg(thicknessCursorId)
      << QString("selectionThicknessCursorArtwork=%1")
             .arg(thicknessCursorArtworkOk ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
      << QString("selectionThicknessLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(thicknessLocalDragPoints))
      << QString("selectionBBoxBeforeDeform=%1")
             .arg(gui_smoke_rect_details(bboxBeforeDeform))
      << QString("selectionBBoxAfterDeform=%1")
             .arg(gui_smoke_rect_details(bboxAfterDeform))
      << QString("selectionDeformCursorId=%1").arg(deformCursorId)
      << QString("selectionDeformCursorArtwork=%1")
             .arg(deformCursorArtworkOk ? QStringLiteral("ok")
                                        : QStringLiteral("error"))
      << QString("selectionDeformLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(deformLocalDragPoints))
      << QString("selectionRectMouseEvents=%1")
             .arg(selectionRectMouseEventsDelivered ? QStringLiteral("ok")
                                                    : QStringLiteral("error"))
      << QString("selectionCenterHoverEvent=%1")
             .arg(centerHoverDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("selectionCenterMouseEvents=%1")
             .arg(centerMouseEventsDelivered ? QStringLiteral("ok")
                                             : QStringLiteral("error"))
      << QString("selectionThicknessHoverEvent=%1")
             .arg(thicknessHoverDelivered ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
      << QString("selectionThicknessMouseEvents=%1")
             .arg(thicknessMouseEventsDelivered ? QStringLiteral("ok")
                                                : QStringLiteral("error"))
      << QString("selectionDeformHoverEvent=%1")
             .arg(deformHoverDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("selectionDeformMouseEvents=%1")
             .arg(deformMouseEventsDelivered ? QStringLiteral("ok")
                                             : QStringLiteral("error"))
      << QString("selectionToolProbe=%1")
             .arg(selectionToolOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionRectProbe=%1")
             .arg(selectionRectOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionAdvancedCursorProbe=%1")
             .arg(selectionAdvancedCursorOk ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
      << QString("selectionAdvancedHandleProbe=%1")
             .arg(selectionAdvancedHandleOk ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before,
      QStringLiteral("viewer-selection-tool-vector-center-thickness-deform"),
      QStringLiteral("selection-tool-vector-center-thickness-deform"),
      selectionOk);

  return details;
}

static QStringList gui_smoke_viewer_selection_tool_vector_mode_variants_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionFreehandProbe=no-viewer")
            << QStringLiteral("selectionPolylineProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionFreehandProbe=scene-folder-error")
            << QStringLiteral("selectionPolylineProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      PLI_XSHLEVEL, L"qt6_gui_viewer_selection_tool_vector_mode_variants");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionFreehandProbe=level-error")
            << QStringLiteral("selectionPolylineProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  if (!gui_smoke_add_vector_multi_probe_frame(simpleLevel, fid)) {
    details << QStringLiteral("viewerRenderProbe=vector-frame-error")
            << QStringLiteral("selectionToolProbe=vector-frame-error")
            << QStringLiteral("selectionFreehandProbe=vector-frame-error")
            << QStringLiteral("selectionPolylineProbe=vector-frame-error")
            << QStringLiteral("mouseEventProbe=vector-frame-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionFreehandProbe=xsheet-error")
            << QStringLiteral("selectionPolylineProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionFreehandProbe=tool-handle-error")
            << QStringLiteral("selectionPolylineProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionFreehandProbe=tool-error")
            << QStringLiteral("selectionPolylineProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(tool);
  if (!selectionTool) {
    details << QStringLiteral("viewerRenderProbe=tool-cast-error")
            << QStringLiteral("selectionToolProbe=tool-cast-error")
            << QStringLiteral("selectionFreehandProbe=tool-cast-error")
            << QStringLiteral("selectionPolylineProbe=tool-cast-error")
            << QStringLiteral("mouseEventProbe=tool-cast-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionFreehandProbe=tool-disabled")
            << QStringLiteral("selectionPolylineProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  const bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  QString freehandTypeValue;
  const bool freehandTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Freehand", &freehandTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  TVectorImageP image = (TVectorImageP)simpleLevel->getFrame(fid, false);
  const int vectorStrokeCount = image ? image->getStrokeCount() : -1;

  StrokeSelection* selection =
      dynamic_cast<StrokeSelection*>(tool->getSelection());
  const bool selectionEmptyBefore = selection ? selection->isEmpty() : true;

  const std::vector<TPointD> freehandPoints = {
      TPointD(-270.0, -135.0), TPointD(-260.0, 145.0), TPointD(-40.0, 145.0),
      TPointD(-40.0, -135.0), TPointD(-270.0, -135.0)};
  std::vector<QPointF> freehandLocalPoints;
  const bool freehandMouseEventsDelivered =
      freehandTypeOk ? gui_smoke_replay_viewer_mouse_drag(
                           viewer, freehandPoints, &freehandLocalPoints)
                     : false;

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  selection = dynamic_cast<StrokeSelection*>(tool->getSelection());
  const int freehandCount =
      selection ? static_cast<int>(selection->getSelection().size()) : -1;
  const bool freehandStroke0Selected = selection && selection->isSelected(0);
  const bool freehandStroke1Selected = selection && selection->isSelected(1);
  const int freehandBBoxCount        = selectionTool->getBBoxsCount();
  const TRectD freehandBBox =
      freehandBBoxCount > 0 ? selectionTool->getBBox().getBox() : TRectD();
  const bool selectionFreehandOk =
      freehandTypeOk && freehandMouseEventsDelivered && selection &&
      selectionEmptyBefore && freehandCount == 1 && freehandStroke0Selected &&
      !freehandStroke1Selected && freehandBBoxCount > 0 &&
      !freehandBBox.isEmpty();

  bool selectionClearedBeforePolyline = false;
  if (selection) {
    selection->selectNone();
    selectionTool->onSelectionChanged();
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
    selectionClearedBeforePolyline = selection->isEmpty();
  }

  QString polylineTypeValue;
  const bool polylineTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Polyline", &polylineTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);

  const std::vector<TPointD> polylinePoints = {
      TPointD(35.0, -125.0), TPointD(275.0, -125.0), TPointD(275.0, 150.0),
      TPointD(35.0, 150.0)};
  std::vector<QPointF> polylineLocalPoints;
  const bool polylineMouseEventsDelivered =
      polylineTypeOk ? gui_smoke_replay_viewer_mouse_polyline(
                           viewer, polylinePoints, &polylineLocalPoints)
                     : false;

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  selection = dynamic_cast<StrokeSelection*>(tool->getSelection());
  const int polylineCount =
      selection ? static_cast<int>(selection->getSelection().size()) : -1;
  const bool polylineStroke0Selected = selection && selection->isSelected(0);
  const bool polylineStroke1Selected = selection && selection->isSelected(1);
  const int polylineBBoxCount        = selectionTool->getBBoxsCount();
  const TRectD polylineBBox =
      polylineBBoxCount > 0 ? selectionTool->getBBox().getBox() : TRectD();
  const bool bboxChangedFromFreehand =
      !freehandBBox.isEmpty() && !polylineBBox.isEmpty() &&
      (std::abs(polylineBBox.x0 - freehandBBox.x0) > 1.0 ||
       std::abs(polylineBBox.x1 - freehandBBox.x1) > 1.0 ||
       std::abs(polylineBBox.y0 - freehandBBox.y0) > 1.0 ||
       std::abs(polylineBBox.y1 - freehandBBox.y1) > 1.0);
  const bool selectionPolylineOk =
      polylineTypeOk && polylineMouseEventsDelivered && selection &&
      selectionClearedBeforePolyline && polylineCount == 1 &&
      !polylineStroke0Selected && polylineStroke1Selected &&
      polylineBBoxCount > 0 && !polylineBBox.isEmpty() &&
      bboxChangedFromFreehand;

  simpleLevel->setDirtyFlag(true);
  const bool selectionToolOk =
      selectionModeOk && freehandTypeOk && polylineTypeOk && tool->isEnabled();
  const bool mouseEventsDelivered =
      freehandMouseEventsDelivered && polylineMouseEventsDelivered;
  const bool selectionOk =
      selectionToolOk && selectionFreehandOk && selectionPolylineOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("selectionMode=%1")
                 .arg(gui_smoke_status_value(selectionModeValue))
          << QString("selectionFreehandType=%1")
                 .arg(gui_smoke_status_value(freehandTypeValue))
          << QString("selectionPolylineType=%1")
                 .arg(gui_smoke_status_value(polylineTypeValue))
          << QString("selectionEmptyBefore=%1")
                 .arg(selectionEmptyBefore ? QStringLiteral("true")
                                           : QStringLiteral("false"))
          << QString("vectorStrokeCount=%1").arg(vectorStrokeCount)
          << QString("selectionFreehandCount=%1").arg(freehandCount)
          << QString("selectionFreehandStroke0Selected=%1")
                 .arg(freehandStroke0Selected ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionFreehandStroke1Selected=%1")
                 .arg(freehandStroke1Selected ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionFreehandBBox=%1")
                 .arg(gui_smoke_rect_details(freehandBBox))
          << QString("selectionFreehandBBoxCount=%1").arg(freehandBBoxCount)
          << QString("selectionClearedBeforePolyline=%1")
                 .arg(selectionClearedBeforePolyline ? QStringLiteral("true")
                                                     : QStringLiteral("false"))
          << QString("selectionPolylineCount=%1").arg(polylineCount)
          << QString("selectionPolylineStroke0Selected=%1")
                 .arg(polylineStroke0Selected ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionPolylineStroke1Selected=%1")
                 .arg(polylineStroke1Selected ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionPolylineBBox=%1")
                 .arg(gui_smoke_rect_details(polylineBBox))
          << QString("selectionPolylineBBoxCount=%1").arg(polylineBBoxCount)
          << QString("selectionBBoxChangedFromFreehand=%1")
                 .arg(bboxChangedFromFreehand ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionFreehandLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(freehandLocalPoints))
          << QString("selectionPolylineLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(polylineLocalPoints))
          << QString("selectionFreehandMouseEvents=%1")
                 .arg(freehandMouseEventsDelivered ? QStringLiteral("ok")
                                                   : QStringLiteral("error"))
          << QString("selectionPolylineMouseEvents=%1")
                 .arg(polylineMouseEventsDelivered ? QStringLiteral("ok")
                                                   : QStringLiteral("error"))
          << QString("selectionToolProbe=%1")
                 .arg(selectionToolOk ? QStringLiteral("ok")
                                      : QStringLiteral("error"))
          << QString("selectionFreehandProbe=%1")
                 .arg(selectionFreehandOk ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
          << QString("selectionPolylineProbe=%1")
                 .arg(selectionPolylineOk ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
          << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before,
      QStringLiteral("viewer-selection-tool-vector-mode-variants"),
      QStringLiteral("selection-tool-vector-mode-variants"), selectionOk);

  return details;
}

static QStringList gui_smoke_viewer_selection_tool_raster_handles_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionRectProbe=no-viewer")
            << QStringLiteral("selectionRasterCursorProbe=no-viewer")
            << QStringLiteral("selectionRasterHandleProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionRectProbe=scene-folder-error")
            << QStringLiteral("selectionRasterCursorProbe=scene-folder-error")
            << QStringLiteral("selectionRasterHandleProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_selection_tool_raster_handles");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionRectProbe=level-error")
            << QStringLiteral("selectionRasterCursorProbe=level-error")
            << QStringLiteral("selectionRasterHandleProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionRectProbe=xsheet-error")
            << QStringLiteral("selectionRasterCursorProbe=xsheet-error")
            << QStringLiteral("selectionRasterHandleProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentPalette()->setPalette(simpleLevel->getPalette(),
                                                    1);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->setPalette(simpleLevel->getPalette(), 1);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionRectProbe=tool-handle-error")
            << QStringLiteral("selectionRasterCursorProbe=tool-handle-error")
            << QStringLiteral("selectionRasterHandleProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionRectProbe=tool-error")
            << QStringLiteral("selectionRasterCursorProbe=tool-error")
            << QStringLiteral("selectionRasterHandleProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(tool);
  if (!selectionTool) {
    details << QStringLiteral("viewerRenderProbe=tool-cast-error")
            << QStringLiteral("selectionToolProbe=tool-cast-error")
            << QStringLiteral("selectionRectProbe=tool-cast-error")
            << QStringLiteral("selectionRasterCursorProbe=tool-cast-error")
            << QStringLiteral("selectionRasterHandleProbe=tool-cast-error")
            << QStringLiteral("mouseEventProbe=tool-cast-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionRectProbe=tool-disabled")
            << QStringLiteral("selectionRasterCursorProbe=tool-disabled")
            << QStringLiteral("selectionRasterHandleProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  if (!selectionModeOk && selectionModeValue == QStringLiteral("<missing>")) {
    selectionModeOk    = true;
    selectionModeValue = QStringLiteral("not-applicable");
  }
  QString selectionTypeValue;
  const bool selectionTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Rectangular", &selectionTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeRasterStats rasterBefore =
      gui_smoke_analyze_raster_frame(simpleLevel, fid);

  RasterSelection* selection =
      dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyBefore = selection ? selection->isEmpty() : true;
  const bool selectionFloatingBefore =
      selection ? selection->isFloating() : false;

  const TPointD selectStart(-90.0, -85.0);
  const TPointD selectEnd(95.0, 90.0);
  std::vector<QPointF> selectionRectLocalPoints;
  const bool selectionRectMouseEventsDelivered =
      gui_smoke_replay_viewer_mouse_drag(viewer, {selectStart, selectEnd},
                                         &selectionRectLocalPoints);

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  selection = dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyAfterRect = selection ? selection->isEmpty() : true;
  const bool selectionFloatingAfterRect =
      selection ? selection->isFloating() : false;
  const TRectD selectionBBoxAfterRect =
      selection ? selection->getSelectionBbox() : TRectD();
  const int bboxCountAfterRect = selectionTool->getBBoxsCount();
  const bool selectionRectOk =
      selectionRectMouseEventsDelivered && selection && selectionEmptyBefore &&
      !selectionEmptyAfterRect && !selectionFloatingAfterRect &&
      !selectionBBoxAfterRect.isEmpty() && bboxCountAfterRect > 0;

  DragSelectionTool::FourPoints bboxBeforeScale =
      bboxCountAfterRect > 0 ? selectionTool->getBBox()
                             : DragSelectionTool::FourPoints();
  const bool bboxBeforeScaleEmpty = bboxBeforeScale.isEmpty();
  const TPointD scaleHandle       = bboxBeforeScale.getP11();
  const TPointD scaleEnd(scaleHandle.x + 55.0, scaleHandle.y + 45.0);
  const QPointF scaleHandleLocal =
      gui_smoke_world_to_viewer_local(viewer, scaleHandle);
  bool scaleHoverDelivered  = false;
  int scaleCursorId         = ToolCursor::CURSOR_NONE;
  bool scaleCursorArtworkOk = false;
  GuiSmokeCursorArtworkSignature scaleCursorArtwork;
  if (selectionRectOk && !bboxBeforeScaleEmpty &&
      viewer->rect().contains(scaleHandleLocal.toPoint())) {
    scaleHoverDelivered = gui_smoke_send_viewer_mouse_event(
        viewer, QEvent::MouseMove, scaleHandleLocal, Qt::NoButton, Qt::NoButton,
        Qt::NoModifier);
    scaleCursorId        = tool->getCursorId();
    scaleCursorArtworkOk = gui_smoke_tool_cursor_pixmap_ok(scaleCursorId);
    scaleCursorArtwork   = gui_smoke_cursor_artwork_signature(scaleCursorId);
  }
  const bool selectionCursorOk = scaleHoverDelivered && scaleCursorArtworkOk &&
                                 scaleCursorArtwork.ok &&
                                 (scaleCursorId == ToolCursor::ScaleCursor ||
                                  scaleCursorId == ToolCursor::ScaleInvCursor);

  std::vector<QPointF> scaleLocalDragPoints;
  const bool scaleMouseEventsDelivered =
      selectionRectOk && !bboxBeforeScaleEmpty
          ? gui_smoke_replay_viewer_mouse_drag(viewer, {scaleHandle, scaleEnd},
                                               &scaleLocalDragPoints)
          : false;

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  selection = dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyAfterScale = selection ? selection->isEmpty() : true;
  const bool selectionFloatingAfterScale =
      selection ? selection->isFloating() : false;
  const TRectD selectionBBoxAfterScale =
      selection ? selection->getSelectionBbox() : TRectD();
  const double bboxWidthDelta =
      selectionBBoxAfterScale.getLx() - selectionBBoxAfterRect.getLx();
  const double bboxHeightDelta =
      selectionBBoxAfterScale.getLy() - selectionBBoxAfterRect.getLy();
  const bool selectionHandleOk =
      scaleMouseEventsDelivered && !selectionEmptyAfterScale &&
      selectionFloatingAfterScale && !selectionBBoxAfterScale.isEmpty() &&
      (std::abs(bboxWidthDelta) > 0.5 || std::abs(bboxHeightDelta) > 0.5);

  simpleLevel->setDirtyFlag(true);
  const GuiSmokeRasterStats rasterAfter =
      gui_smoke_analyze_raster_frame(simpleLevel, fid);
  const bool selectionToolOk =
      selectionModeOk && selectionTypeOk && tool->isEnabled();
  const bool mouseEventsDelivered = selectionRectMouseEventsDelivered &&
                                    scaleHoverDelivered &&
                                    scaleMouseEventsDelivered;
  const bool selectionOk = selectionToolOk && selectionRectOk &&
                           selectionCursorOk && selectionHandleOk;

  details
      << QString("scene=%1").arg(scenePath.getQString())
      << QStringLiteral("toolInputPath=qt-mouse-events")
      << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
      << QString("toolTargetType=%1").arg(tool->getTargetType())
      << QString("toolEnabled=%1")
             .arg(tool->isEnabled() ? QStringLiteral("true")
                                    : QStringLiteral("false"))
      << QString("toolDisabledReason=%1")
             .arg(gui_smoke_status_value(disabledReason))
      << QString("selectionMode=%1")
             .arg(gui_smoke_status_value(selectionModeValue))
      << QString("selectionType=%1")
             .arg(gui_smoke_status_value(selectionTypeValue))
      << QString("selectionEmptyBefore=%1")
             .arg(selectionEmptyBefore ? QStringLiteral("true")
                                       : QStringLiteral("false"))
      << QString("selectionFloatingBefore=%1")
             .arg(selectionFloatingBefore ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("selectionEmptyAfterRect=%1")
             .arg(selectionEmptyAfterRect ? QStringLiteral("true")
                                          : QStringLiteral("false"))
      << QString("selectionFloatingAfterRect=%1")
             .arg(selectionFloatingAfterRect ? QStringLiteral("true")
                                             : QStringLiteral("false"))
      << QString("selectionBBoxAfterRect=%1")
             .arg(gui_smoke_rect_details(selectionBBoxAfterRect))
      << QString("selectionBBoxAfterScale=%1")
             .arg(gui_smoke_rect_details(selectionBBoxAfterScale))
      << QString("selectionBBoxWidthDelta=%1").arg(bboxWidthDelta, 0, 'f', 4)
      << QString("selectionBBoxHeightDelta=%1").arg(bboxHeightDelta, 0, 'f', 4)
      << QString("selectionEmptyAfterScale=%1")
             .arg(selectionEmptyAfterScale ? QStringLiteral("true")
                                           : QStringLiteral("false"))
      << QString("selectionFloatingAfterScale=%1")
             .arg(selectionFloatingAfterScale ? QStringLiteral("true")
                                              : QStringLiteral("false"))
      << QString("selectionBBoxCountAfterRect=%1").arg(bboxCountAfterRect)
      << QString("selectionRectLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(selectionRectLocalPoints))
      << QString("selectionScaleHandleWorld=%1,%2")
             .arg(scaleHandle.x, 0, 'f', 4)
             .arg(scaleHandle.y, 0, 'f', 4)
      << QString("selectionScaleHandleLocalPoints=%1")
             .arg(gui_smoke_joined_local_points(scaleLocalDragPoints))
      << QString("selectionScaleCursorId=%1").arg(scaleCursorId)
      << QString("selectionScaleCursorArtwork=%1")
             .arg(scaleCursorArtworkOk ? QStringLiteral("ok")
                                       : QStringLiteral("error"))
      << QString("selectionScaleCursorArtworkSignature=%1")
             .arg(gui_smoke_cursor_artwork_summary(scaleCursorArtwork))
      << QString("rasterPixelsBefore=%1").arg(rasterBefore.opaquePixels)
      << QString("rasterPixelsAfter=%1").arg(rasterAfter.opaquePixels)
      << QString("rasterRedPixelsBefore=%1").arg(rasterBefore.redPixels)
      << QString("rasterRedPixelsAfter=%1").arg(rasterAfter.redPixels)
      << QString("selectionRectMouseEvents=%1")
             .arg(selectionRectMouseEventsDelivered ? QStringLiteral("ok")
                                                    : QStringLiteral("error"))
      << QString("selectionScaleHoverEvent=%1")
             .arg(scaleHoverDelivered ? QStringLiteral("ok")
                                      : QStringLiteral("error"))
      << QString("selectionScaleMouseEvents=%1")
             .arg(scaleMouseEventsDelivered ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
      << QString("selectionToolProbe=%1")
             .arg(selectionToolOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionRectProbe=%1")
             .arg(selectionRectOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
      << QString("selectionRasterCursorProbe=%1")
             .arg(selectionCursorOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("selectionRasterHandleProbe=%1")
             .arg(selectionHandleOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
      << QString("mouseEventProbe=%1")
             .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                       : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-selection-tool-raster-handles"),
      QStringLiteral("selection-tool-raster-handles"), selectionOk);

  return details;
}

static QStringList gui_smoke_viewer_selection_tool_raster_mode_variants_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("selectionToolProbe=no-viewer")
            << QStringLiteral("selectionFreehandProbe=no-viewer")
            << QStringLiteral("selectionPolylineProbe=no-viewer")
            << QStringLiteral("mouseEventProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("selectionToolProbe=scene-folder-error")
            << QStringLiteral("selectionFreehandProbe=scene-folder-error")
            << QStringLiteral("selectionPolylineProbe=scene-folder-error")
            << QStringLiteral("mouseEventProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel* levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_selection_tool_raster_mode_variants");
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("selectionToolProbe=level-error")
            << QStringLiteral("selectionFreehandProbe=level-error")
            << QStringLiteral("selectionPolylineProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("selectionToolProbe=xsheet-error")
            << QStringLiteral("selectionFreehandProbe=xsheet-error")
            << QStringLiteral("selectionPolylineProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(objectId);
  TApp::instance()->getCurrentObject()->notifyObjectIdSwitched();
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentPalette()->setPalette(simpleLevel->getPalette(),
                                                    1);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->setPalette(simpleLevel->getPalette(), 1);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("selectionToolProbe=tool-handle-error")
            << QStringLiteral("selectionFreehandProbe=tool-handle-error")
            << QStringLiteral("selectionPolylineProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Selection);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Selection) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("selectionToolProbe=tool-error")
            << QStringLiteral("selectionFreehandProbe=tool-error")
            << QStringLiteral("selectionPolylineProbe=tool-error")
            << QStringLiteral("mouseEventProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  SelectionTool* selectionTool = dynamic_cast<SelectionTool*>(tool);
  if (!selectionTool) {
    details << QStringLiteral("viewerRenderProbe=tool-cast-error")
            << QStringLiteral("selectionToolProbe=tool-cast-error")
            << QStringLiteral("selectionFreehandProbe=tool-cast-error")
            << QStringLiteral("selectionPolylineProbe=tool-cast-error")
            << QStringLiteral("mouseEventProbe=tool-cast-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("selectionToolProbe=tool-disabled")
            << QStringLiteral("selectionFreehandProbe=tool-disabled")
            << QStringLiteral("selectionPolylineProbe=tool-disabled")
            << QStringLiteral("mouseEventProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  QString selectionModeValue;
  bool selectionModeOk = gui_smoke_set_tool_enum_property(
      tool, "Mode:", "SelectionMode", L"Standard", &selectionModeValue);
  if (!selectionModeOk && selectionModeValue == QStringLiteral("<missing>")) {
    selectionModeOk    = true;
    selectionModeValue = QStringLiteral("not-applicable");
  }

  QString freehandTypeValue;
  const bool freehandTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Freehand", &freehandTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  RasterSelection* selection =
      dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyBefore = selection ? selection->isEmpty() : true;

  const std::vector<TPointD> freehandPoints = {
      TPointD(-150.0, -85.0), TPointD(-130.0, -130.0), TPointD(-70.0, -150.0),
      TPointD(-15.0, -110.0), TPointD(-5.0, -35.0),    TPointD(-45.0, 55.0),
      TPointD(-110.0, 70.0),  TPointD(-160.0, 15.0),   TPointD(-150.0, -85.0)};
  std::vector<QPointF> freehandLocalPoints;
  const bool freehandMouseEventsDelivered =
      freehandTypeOk ? gui_smoke_replay_viewer_mouse_drag(
                           viewer, freehandPoints, &freehandLocalPoints)
                     : false;

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  selection = dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyAfterFreehand =
      selection ? selection->isEmpty() : true;
  const bool selectionFloatingAfterFreehand =
      selection ? selection->isFloating() : false;
  const TRectD selectionBBoxAfterFreehand =
      selection ? selection->getSelectionBbox() : TRectD();
  const int bboxCountAfterFreehand = selectionTool->getBBoxsCount();
  const bool selectionFreehandOk =
      freehandTypeOk && freehandMouseEventsDelivered && selection &&
      selectionEmptyBefore && !selectionEmptyAfterFreehand &&
      !selectionFloatingAfterFreehand &&
      !selectionBBoxAfterFreehand.isEmpty() && bboxCountAfterFreehand > 0;

  QString polylineTypeValue;
  const bool polylineTypeOk = gui_smoke_set_tool_enum_property(
      tool, "Type:", "Type", L"Polyline", &polylineTypeValue);
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);

  const std::vector<TPointD> polylinePoints = {
      TPointD(35.0, -95.0), TPointD(155.0, -95.0), TPointD(165.0, 55.0),
      TPointD(55.0, 105.0)};
  std::vector<QPointF> polylineLocalPoints;
  const bool polylineMouseEventsDelivered =
      polylineTypeOk ? gui_smoke_replay_viewer_mouse_polyline(
                           viewer, polylinePoints, &polylineLocalPoints)
                     : false;

  TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  tool->updateMatrix();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(150);

  selection = dynamic_cast<RasterSelection*>(tool->getSelection());
  const bool selectionEmptyAfterPolyline =
      selection ? selection->isEmpty() : true;
  const bool selectionFloatingAfterPolyline =
      selection ? selection->isFloating() : false;
  const TRectD selectionBBoxAfterPolyline =
      selection ? selection->getSelectionBbox() : TRectD();
  const int bboxCountAfterPolyline = selectionTool->getBBoxsCount();
  const bool bboxChangedFromFreehand =
      !selectionBBoxAfterFreehand.isEmpty() &&
      !selectionBBoxAfterPolyline.isEmpty() &&
      (std::abs(selectionBBoxAfterPolyline.x0 - selectionBBoxAfterFreehand.x0) >
           1.0 ||
       std::abs(selectionBBoxAfterPolyline.x1 - selectionBBoxAfterFreehand.x1) >
           1.0 ||
       std::abs(selectionBBoxAfterPolyline.y0 - selectionBBoxAfterFreehand.y0) >
           1.0 ||
       std::abs(selectionBBoxAfterPolyline.y1 - selectionBBoxAfterFreehand.y1) >
           1.0);
  const bool selectionPolylineOk =
      polylineTypeOk && polylineMouseEventsDelivered && selection &&
      !selectionEmptyAfterPolyline && !selectionFloatingAfterPolyline &&
      !selectionBBoxAfterPolyline.isEmpty() && bboxCountAfterPolyline > 0 &&
      bboxChangedFromFreehand;

  simpleLevel->setDirtyFlag(true);
  const bool selectionToolOk =
      selectionModeOk && freehandTypeOk && polylineTypeOk && tool->isEnabled();
  const bool mouseEventsDelivered =
      freehandMouseEventsDelivered && polylineMouseEventsDelivered;
  const bool selectionOk =
      selectionToolOk && selectionFreehandOk && selectionPolylineOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("selectionMode=%1")
                 .arg(gui_smoke_status_value(selectionModeValue))
          << QString("selectionFreehandType=%1")
                 .arg(gui_smoke_status_value(freehandTypeValue))
          << QString("selectionPolylineType=%1")
                 .arg(gui_smoke_status_value(polylineTypeValue))
          << QString("selectionEmptyBefore=%1")
                 .arg(selectionEmptyBefore ? QStringLiteral("true")
                                           : QStringLiteral("false"))
          << QString("selectionEmptyAfterFreehand=%1")
                 .arg(selectionEmptyAfterFreehand ? QStringLiteral("true")
                                                  : QStringLiteral("false"))
          << QString("selectionFloatingAfterFreehand=%1")
                 .arg(selectionFloatingAfterFreehand ? QStringLiteral("true")
                                                     : QStringLiteral("false"))
          << QString("selectionBBoxAfterFreehand=%1")
                 .arg(gui_smoke_rect_details(selectionBBoxAfterFreehand))
          << QString("selectionBBoxCountAfterFreehand=%1")
                 .arg(bboxCountAfterFreehand)
          << QString("selectionEmptyAfterPolyline=%1")
                 .arg(selectionEmptyAfterPolyline ? QStringLiteral("true")
                                                  : QStringLiteral("false"))
          << QString("selectionFloatingAfterPolyline=%1")
                 .arg(selectionFloatingAfterPolyline ? QStringLiteral("true")
                                                     : QStringLiteral("false"))
          << QString("selectionBBoxAfterPolyline=%1")
                 .arg(gui_smoke_rect_details(selectionBBoxAfterPolyline))
          << QString("selectionBBoxCountAfterPolyline=%1")
                 .arg(bboxCountAfterPolyline)
          << QString("selectionBBoxChangedFromFreehand=%1")
                 .arg(bboxChangedFromFreehand ? QStringLiteral("true")
                                              : QStringLiteral("false"))
          << QString("selectionFreehandLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(freehandLocalPoints))
          << QString("selectionPolylineLocalPoints=%1")
                 .arg(gui_smoke_joined_local_points(polylineLocalPoints))
          << QString("selectionFreehandMouseEvents=%1")
                 .arg(freehandMouseEventsDelivered ? QStringLiteral("ok")
                                                   : QStringLiteral("error"))
          << QString("selectionPolylineMouseEvents=%1")
                 .arg(polylineMouseEventsDelivered ? QStringLiteral("ok")
                                                   : QStringLiteral("error"))
          << QString("selectionToolProbe=%1")
                 .arg(selectionToolOk ? QStringLiteral("ok")
                                      : QStringLiteral("error"))
          << QString("selectionFreehandProbe=%1")
                 .arg(selectionFreehandOk ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
          << QString("selectionPolylineProbe=%1")
                 .arg(selectionPolylineOk ? QStringLiteral("ok")
                                          : QStringLiteral("error"))
          << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before,
      QStringLiteral("viewer-selection-tool-raster-mode-variants"),
      QStringLiteral("selection-tool-raster-mode-variants"), selectionOk);

  return details;
}

static bool gui_smoke_send_viewer_tablet_event(
    SceneViewer* viewer, QEvent::Type type, const QPointF& localPos,
    double pressure, float xTilt, float yTilt, Qt::MouseButton button,
    Qt::MouseButtons buttons) {
  if (!viewer) return false;

  const QPointF globalPos = viewer->mapToGlobal(localPos.toPoint());
  QTabletEvent event = QtCompat::makeTabletEvent(
      type, localPos, globalPos, QtCompat::TabletStylus, QtCompat::TabletPen,
      pressure, xTilt, yTilt, 0.0, 0.0, 0, Qt::NoModifier, 0x746e7a36, button,
      buttons);
  const bool delivered = QApplication::sendEvent(viewer, &event);
  gui_smoke_pump_events(30);
  return delivered;
}

static bool gui_smoke_replay_viewer_tablet_stroke(SceneViewer* viewer) {
  if (!viewer) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  std::vector<QPointF> localPoints;
  localPoints.reserve(strokePoints.size());
  for (const TPointD& point : strokePoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = gui_smoke_send_viewer_tablet_event(
      viewer, QEvent::TabletPress, localPoints.front(), 0.35, -18.0f, 12.0f,
      Qt::LeftButton, Qt::LeftButton);
  for (size_t i = 1; i + 1 < localPoints.size(); ++i) {
    delivered = gui_smoke_send_viewer_tablet_event(
                    viewer, QEvent::TabletMove, localPoints[i], 0.85, -18.0f,
                    12.0f, Qt::NoButton, Qt::LeftButton) &&
                delivered;
  }
  delivered = gui_smoke_send_viewer_tablet_event(
                  viewer, QEvent::TabletRelease, localPoints.back(), 0.0,
                  -18.0f, 12.0f, Qt::LeftButton, Qt::NoButton) &&
              delivered;
  gui_smoke_pump_events(120);
  return delivered;
}

static QStringList gui_smoke_viewer_vector_brush_details(
    const QString& requestedSceneName) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolInputProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolInputProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  const std::wstring levelName = L"qt6_gui_viewer_vector_brush";
  TXshLevel* levelXl           = scene->createNewLevel(PLI_XSHLEVEL, levelName);
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel || !gui_smoke_prepare_red_palette(simpleLevel)) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolInputProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  TVectorImageP image = new TVectorImage();
  image->setPalette(simpleLevel->getPalette());
  simpleLevel->setFrame(fid, image);
  simpleLevel->setDirtyFlag(true);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolInputProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->setPalette(simpleLevel->getPalette(), 1);
  TApp::instance()->setCurrentLevelStyleIndex(1, true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolInputProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Brush);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Brush) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolInputProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolInputProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TVectorImageP beforeImage = (TVectorImageP)simpleLevel->getFrame(fid, false);
  const int strokesBefore   = beforeImage ? beforeImage->getStrokeCount() : -1;

  if (tool->preLeftButtonDown()) {
    tool = toolHandle->getTool();
    if (!tool) {
      details << QStringLiteral("viewerRenderProbe=tool-switch-error")
              << QStringLiteral("toolInputProbe=tool-switch-error")
              << QString("scene=%1").arg(scenePath.getQString());
      return details;
    }
    tool->setViewer(viewer);
    tool->updateMatrix();
  }

  gui_smoke_replay_tool_stroke(toolHandle, tool);

  TVectorImageP afterImage = (TVectorImageP)simpleLevel->getFrame(fid, false);
  const int strokesAfter   = afterImage ? afterImage->getStrokeCount() : -1;
  if (afterImage) afterImage->validateRegions(true);
  simpleLevel->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  const bool toolOk = strokesBefore >= 0 && strokesAfter > strokesBefore;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("strokesBefore=%1").arg(strokesBefore)
          << QString("strokesAfter=%1").arg(strokesAfter)
          << QString("toolInputProbe=%1")
                 .arg(toolOk ? QStringLiteral("ok") : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-vector-brush"),
      QStringLiteral("vector-brush"), toolOk);

  return details;
}

static QStringList gui_smoke_viewer_raster_brush_details(
    const QString& requestedSceneName,
    GuiSmokeRasterBrushInput input = GuiSmokeRasterBrushInput::DirectTool,
    const QString& action          = QString()) {
  QStringList details;
  SceneViewer* viewer = gui_smoke_resolve_scene_viewer();
  if (!viewer) {
    details << QStringLiteral("viewerRenderProbe=no-viewer")
            << QStringLiteral("toolInputProbe=no-viewer")
            << QStringLiteral("rasterInputProbe=no-viewer");
    return details;
  }

  QString sceneName   = gui_smoke_scene_name(requestedSceneName);
  TFilePath scenePath = gui_smoke_scene_path(sceneName);
  if (!TSystem::touchParentDir(scenePath)) {
    details << QStringLiteral("viewerRenderProbe=scene-folder-error")
            << QStringLiteral("toolInputProbe=scene-folder-error")
            << QStringLiteral("rasterInputProbe=scene-folder-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  IoCmd::newScene();
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  const std::wstring levelName = L"qt6_gui_viewer_raster_brush";
  TXshLevel* levelXl           = scene->createNewLevel(OVL_XSHLEVEL, levelName);
  TXshSimpleLevel* simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel || !gui_smoke_prepare_red_palette(simpleLevel)) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolInputProbe=level-error")
            << QStringLiteral("rasterInputProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  TRaster32P raster(512, 512);
  raster->clear();
  TRasterImageP image(raster);
  image->setPalette(simpleLevel->getPalette());
  simpleLevel->setFrame(fid, image);
  simpleLevel->setDirtyFlag(true);

  TXsheet* xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolInputProbe=xsheet-error")
            << QStringLiteral("rasterInputProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(
      TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentPalette()->setPalette(simpleLevel->getPalette(),
                                                    1);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->setPalette(simpleLevel->getPalette(), 1);
  TApp::instance()->setCurrentLevelStyleIndex(1, true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeRasterStats rasterBefore =
      gui_smoke_analyze_raster_frame(simpleLevel, fid);

  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolInputProbe=tool-handle-error")
            << QStringLiteral("rasterInputProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Brush);
  TTool* tool = toolHandle->getTool();
  if (!tool || tool->getName() != T_Brush) {
    details << QStringLiteral("viewerRenderProbe=tool-error")
            << QStringLiteral("toolInputProbe=tool-error")
            << QStringLiteral("rasterInputProbe=tool-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  tool->setViewer(viewer);
  tool->updateMatrix();
  const QString disabledReason = tool->updateEnabled();
  if (!tool->isEnabled()) {
    details << QStringLiteral("viewerRenderProbe=tool-disabled")
            << QStringLiteral("toolInputProbe=tool-disabled")
            << QStringLiteral("rasterInputProbe=tool-disabled")
            << QString("toolDisabledReason=%1")
                   .arg(gui_smoke_status_value(disabledReason))
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const bool useDirectTool   = input == GuiSmokeRasterBrushInput::DirectTool;
  const bool useMouseEvents  = input == GuiSmokeRasterBrushInput::MouseEvents;
  const bool useTabletEvents = input == GuiSmokeRasterBrushInput::TabletEvents;
  const bool useSystemMouseEvents =
      input == GuiSmokeRasterBrushInput::SystemMouseEvents;

  if (useDirectTool && tool->preLeftButtonDown()) {
    tool = toolHandle->getTool();
    if (!tool) {
      details << QStringLiteral("viewerRenderProbe=tool-switch-error")
              << QStringLiteral("toolInputProbe=tool-switch-error")
              << QStringLiteral("rasterInputProbe=tool-switch-error")
              << QString("scene=%1").arg(scenePath.getQString());
      return details;
    }
    tool->setViewer(viewer);
    tool->updateMatrix();
  }

  bool systemMouseEventsDelivered = true;
  GuiSmokeSystemMouseEventProbe systemMouseProbe(viewer);
  if (useSystemMouseEvents) {
    systemMouseEventsDelivered = false;
    const std::vector<QPointF> globalPoints =
        gui_smoke_tool_screen_points(viewer);
    if (!globalPoints.empty()) {
      const QString smokeAction =
          action.isEmpty() ? QStringLiteral("viewer-raster-brush-system-events")
                           : action;
      write_gui_smoke_status(
          smokeAction, QStringLiteral("ready"),
          QStringList()
              << QString("scene=%1").arg(scenePath.getQString())
              << QString("toolInputPath=%1")
                     .arg(gui_smoke_raster_brush_input_name(input))
              << QString("toolName=%1")
                     .arg(QString::fromStdString(tool->getName()))
              << QString("toolTargetType=%1").arg(tool->getTargetType())
              << QString("toolEnabled=%1")
                     .arg(tool->isEnabled() ? QStringLiteral("true")
                                            : QStringLiteral("false"))
              << QString("rasterPixelsBefore=%1").arg(rasterBefore.opaquePixels)
              << QString("rasterRedPixelsBefore=%1").arg(rasterBefore.redPixels)
              << QString("systemMousePointCount=%1").arg(globalPoints.size())
              << QString("systemMousePoints=%1")
                     .arg(gui_smoke_joined_screen_points(globalPoints))
              << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle()));

      qApp->installEventFilter(&systemMouseProbe);
      QElapsedTimer timer;
      timer.start();
      while (timer.elapsed() < gui_smoke_external_input_timeout_ms()) {
        gui_smoke_pump_events(50);
        const GuiSmokeRasterStats currentStats =
            gui_smoke_analyze_raster_frame(simpleLevel, fid);
        if (currentStats.opaquePixels > rasterBefore.opaquePixels &&
            currentStats.redPixels > rasterBefore.redPixels) {
          systemMouseEventsDelivered = true;
          break;
        }
      }
      qApp->removeEventFilter(&systemMouseProbe);
    }
  }

  const bool mouseEventsDelivered =
      useMouseEvents ? gui_smoke_replay_viewer_mouse_stroke(viewer) : true;
  const bool tabletEventsDelivered =
      useTabletEvents ? gui_smoke_replay_viewer_tablet_stroke(viewer) : true;
  if (useDirectTool) gui_smoke_replay_tool_stroke(toolHandle, tool);

  simpleLevel->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  const GuiSmokeRasterStats rasterAfter =
      gui_smoke_analyze_raster_frame(simpleLevel, fid);
  const bool rasterOk = mouseEventsDelivered && tabletEventsDelivered &&
                        systemMouseEventsDelivered &&
                        rasterAfter.opaquePixels > rasterBefore.opaquePixels &&
                        rasterAfter.redPixels > rasterBefore.redPixels;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QString("toolInputPath=%1")
                 .arg(gui_smoke_raster_brush_input_name(input))
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("rasterPixelsBefore=%1").arg(rasterBefore.opaquePixels)
          << QString("rasterPixelsAfter=%1").arg(rasterAfter.opaquePixels)
          << QString("rasterRedPixelsBefore=%1").arg(rasterBefore.redPixels)
          << QString("rasterRedPixelsAfter=%1").arg(rasterAfter.redPixels)
          << QString("toolInputProbe=%1")
                 .arg(rasterOk ? QStringLiteral("ok") : QStringLiteral("error"))
          << QString("rasterInputProbe=%1")
                 .arg(rasterOk ? QStringLiteral("ok") : QStringLiteral("error"))
          << QString("mouseEventProbe=%1")
                 .arg(mouseEventsDelivered ? QStringLiteral("ok")
                                           : QStringLiteral("error"))
          << QString("tabletEventProbe=%1")
                 .arg(tabletEventsDelivered ? QStringLiteral("ok")
                                            : QStringLiteral("error"))
          << QString("systemMouseEventProbe=%1")
                 .arg(systemMouseEventsDelivered ? QStringLiteral("ok")
                                                 : QStringLiteral("error"))
          << QStringLiteral("tabletPressurePress=0.35")
          << QStringLiteral("tabletPressureDrag=0.85")
          << QStringLiteral("tabletPressureRelease=0")
          << QStringLiteral("tabletTiltX=-18")
          << QStringLiteral("tabletTiltY=12");
  if (useSystemMouseEvents) details += systemMouseProbe.details();
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-raster-brush"),
      QStringLiteral("raster-brush"), rasterOk);

  return details;
}

static QStringList gui_smoke_media_device_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QList<QAudioDevice> audioInputs  = QMediaDevices::audioInputs();
  const QList<QAudioDevice> audioOutputs = QMediaDevices::audioOutputs();
  const QList<QCameraDevice> videoInputs = QMediaDevices::videoInputs();
  QStringList audioInputNames;
  QStringList audioOutputNames;
  QStringList videoInputNames;

  for (const QAudioDevice& device : audioInputs) {
    audioInputNames << device.description();
  }
  for (const QAudioDevice& device : audioOutputs) {
    audioOutputNames << device.description();
  }
  for (const QCameraDevice& device : videoInputs) {
    videoInputNames << device.description();
  }

  const QAudioDevice defaultAudioInput  = QMediaDevices::defaultAudioInput();
  const QAudioDevice defaultAudioOutput = QMediaDevices::defaultAudioOutput();
  const QCameraDevice defaultVideoInput = QMediaDevices::defaultVideoInput();

  details
      << QStringLiteral("qtMultimediaApi=Qt6")
      << QString("audioInputCount=%1").arg(audioInputs.size())
      << QString("audioOutputCount=%1").arg(audioOutputs.size())
      << QString("videoInputCount=%1").arg(videoInputs.size())
      << QString("defaultAudioInput=%1")
             .arg(defaultAudioInput.isNull()
                      ? QString()
                      : gui_smoke_status_value(defaultAudioInput.description()))
      << QString("defaultAudioOutput=%1")
             .arg(
                 defaultAudioOutput.isNull()
                     ? QString()
                     : gui_smoke_status_value(defaultAudioOutput.description()))
      << QString("defaultVideoInput=%1")
             .arg(defaultVideoInput.isNull()
                      ? QString()
                      : gui_smoke_status_value(defaultVideoInput.description()))
      << QString("audioInputs=%1").arg(gui_smoke_joined_values(audioInputNames))
      << QString("audioOutputs=%1")
             .arg(gui_smoke_joined_values(audioOutputNames))
      << QString("videoInputs=%1")
             .arg(gui_smoke_joined_values(videoInputNames));
#else
  const QList<QAudioDeviceInfo> audioInputs =
      QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
  const QList<QAudioDeviceInfo> audioOutputs =
      QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
  const QList<QCameraInfo> videoInputs = QCameraInfo::availableCameras();
  QStringList audioInputNames;
  QStringList audioOutputNames;
  QStringList videoInputNames;

  for (const QAudioDeviceInfo& device : audioInputs) {
    audioInputNames << device.deviceName();
  }
  for (const QAudioDeviceInfo& device : audioOutputs) {
    audioOutputNames << device.deviceName();
  }
  for (const QCameraInfo& device : videoInputs) {
    videoInputNames << device.description();
  }

  const QAudioDeviceInfo defaultAudioInput =
      QAudioDeviceInfo::defaultInputDevice();
  const QAudioDeviceInfo defaultAudioOutput =
      QAudioDeviceInfo::defaultOutputDevice();
  const QCameraInfo defaultVideoInput = QCameraInfo::defaultCamera();

  details
      << QStringLiteral("qtMultimediaApi=Qt5")
      << QString("audioInputCount=%1").arg(audioInputs.size())
      << QString("audioOutputCount=%1").arg(audioOutputs.size())
      << QString("videoInputCount=%1").arg(videoInputs.size())
      << QString("defaultAudioInput=%1")
             .arg(defaultAudioInput.isNull()
                      ? QString()
                      : gui_smoke_status_value(defaultAudioInput.deviceName()))
      << QString("defaultAudioOutput=%1")
             .arg(defaultAudioOutput.isNull()
                      ? QString()
                      : gui_smoke_status_value(defaultAudioOutput.deviceName()))
      << QString("defaultVideoInput=%1")
             .arg(defaultVideoInput.isNull()
                      ? QString()
                      : gui_smoke_status_value(defaultVideoInput.description()))
      << QString("audioInputs=%1").arg(gui_smoke_joined_values(audioInputNames))
      << QString("audioOutputs=%1")
             .arg(gui_smoke_joined_values(audioOutputNames))
      << QString("videoInputs=%1")
             .arg(gui_smoke_joined_values(videoInputNames));
#endif

  return details;
}

static QString gui_smoke_audio_error_name(QAudio::Error error) {
  switch (error) {
  case QAudio::NoError:
    return QStringLiteral("NoError");
  case QAudio::OpenError:
    return QStringLiteral("OpenError");
  case QAudio::IOError:
    return QStringLiteral("IOError");
  case QAudio::FatalError:
    return QStringLiteral("FatalError");
  default:
    return QStringLiteral("UnknownError");
  }
}

static QString gui_smoke_audio_state_name(QAudio::State state) {
  switch (state) {
  case QAudio::ActiveState:
    return QStringLiteral("ActiveState");
  case QAudio::SuspendedState:
    return QStringLiteral("SuspendedState");
  case QAudio::StoppedState:
    return QStringLiteral("StoppedState");
  case QAudio::IdleState:
    return QStringLiteral("IdleState");
  default:
    return QStringLiteral("UnknownState");
  }
}

static QStringList gui_smoke_audio_output_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QAudioDevice output = QMediaDevices::defaultAudioOutput();
  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("defaultAudioOutput=%1")
                 .arg(output.isNull()
                          ? QString()
                          : gui_smoke_status_value(output.description()))
          << QString("audioOutputAvailable=%1")
                 .arg(output.isNull() ? QStringLiteral("false")
                                      : QStringLiteral("true"));

  if (output.isNull()) {
    details << QStringLiteral("audioOutputProbe=no-device");
    return details;
  }

  const QAudioFormat format = output.preferredFormat();
  const int sampleRate      = format.sampleRate();
  const int bytesPerFrame   = format.bytesPerFrame();
  const int durationMs      = 250;
  details << QString("sampleRate=%1").arg(sampleRate)
          << QString("channelCount=%1").arg(format.channelCount())
          << QString("bytesPerFrame=%1").arg(bytesPerFrame)
          << QString("sampleFormat=%1").arg(format.sampleFormat());

  if (!format.isValid() || sampleRate <= 0 || bytesPerFrame <= 0) {
    details << QStringLiteral("audioOutputProbe=invalid-format");
    return details;
  }

  QByteArray samples;
  samples.resize((sampleRate * durationMs / 1000) * bytesPerFrame);
  samples.fill('\0');

  QBuffer buffer(&samples);
  if (!buffer.open(QIODevice::ReadOnly)) {
    details << QStringLiteral("audioOutputProbe=buffer-open-failed");
    return details;
  }

  QAudioSink sink(output, format);
  sink.setVolume(0.0);
  sink.start(&buffer);

  QEventLoop loop;
  QTimer::singleShot(durationMs + 100, &loop, &QEventLoop::quit);
  loop.exec();

  const QAudio::Error error = sink.error();
  details
      << QString("audioState=%1").arg(gui_smoke_audio_state_name(sink.state()))
      << QString("audioError=%1").arg(gui_smoke_audio_error_name(error))
      << QString("processedUSecs=%1").arg(sink.processedUSecs())
      << QString("elapsedUSecs=%1").arg(sink.elapsedUSecs())
      << QString("bytesProvided=%1").arg(samples.size())
      << QString("audioOutputProbe=%1")
             .arg(error == QAudio::NoError ? QStringLiteral("ok")
                                           : QStringLiteral("error"));
  sink.stop();
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("audioOutputProbe=unsupported");
#endif

  return details;
}

static QStringList gui_smoke_audio_playback_wav_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QAudioDevice output = QMediaDevices::defaultAudioOutput();
  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("defaultAudioOutput=%1")
                 .arg(output.isNull()
                          ? QString()
                          : gui_smoke_status_value(output.description()))
          << QString("audioOutputAvailable=%1")
                 .arg(output.isNull() ? QStringLiteral("false")
                                      : QStringLiteral("true"));

  if (output.isNull()) {
    details << QStringLiteral("audioPlaybackWavProbe=no-device");
    return details;
  }

  QAudioFormat format;
  format.setSampleRate(44100);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);

  const bool targetFormatSupported = output.isFormatSupported(format);
  details << QString("targetFormatSupported=%1")
                 .arg(targetFormatSupported ? QStringLiteral("true")
                                            : QStringLiteral("false"));
  if (!targetFormatSupported) {
    details << QStringLiteral("audioPlaybackWavProbe=unsupported-format");
    return details;
  }

  const int sampleRate    = format.sampleRate();
  const int bytesPerFrame = format.bytesPerFrame();
  const int durationMs    = 3000;
  const int sampleCount   = sampleRate * durationMs / 1000;
  details << QString("sampleRate=%1").arg(sampleRate)
          << QString("channelCount=%1").arg(format.channelCount())
          << QString("bytesPerFrame=%1").arg(bytesPerFrame)
          << QString("sampleFormat=%1").arg(format.sampleFormat())
          << QString("sourceDurationMs=%1").arg(durationMs)
          << QString("sourceSampleCount=%1").arg(sampleCount);

  if (!format.isValid() || sampleRate <= 0 || bytesPerFrame <= 0 ||
      sampleCount <= 0) {
    details << QStringLiteral("audioPlaybackWavProbe=invalid-format");
    return details;
  }

  QByteArray samples;
  samples.resize(sampleCount * bytesPerFrame);
  qint16* pcm            = reinterpret_cast<qint16*>(samples.data());
  constexpr double pi    = 3.14159265358979323846;
  const double amplitude = 32767.0 * 0.10;
  const double frequency = 440.0;
  for (int sample = 0; sample < sampleCount; ++sample) {
    pcm[sample] = static_cast<qint16>(
        amplitude * std::sin(2.0 * pi * frequency * sample / sampleRate));
  }

  const QString statusPath =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  const QString outputDir = statusPath.isEmpty()
                                ? QDir::tempPath()
                                : QFileInfo(statusPath).absolutePath();
  QDir().mkpath(outputDir);
  const QString playbackPath =
      QDir(outputDir).filePath(QStringLiteral("audio-playback-wav.wav"));
  QFile::remove(playbackPath);

  AudioWriterWAV writer(format);
  if (!writer.start(playbackPath, false)) {
    details
        << QString("playbackPath=%1").arg(gui_smoke_status_value(playbackPath))
        << QStringLiteral("audioPlaybackWavProbe=writer-start-failed");
    return details;
  }
  const qint64 bytesWritten = writer.write(samples.constData(), samples.size());
  const bool writerStopped  = writer.stop();

  QFileInfo playbackInfo(playbackPath);
  TSoundTrackP loadedTrack;
  const bool wavLoadOk =
      TSoundTrackReader::load(TFilePath(playbackPath), loadedTrack);

  const bool outputInstalled = TSoundOutputDevice::installed();
  bool playbackOpenOk        = false;
  bool playbackStarted       = false;
  bool playbackStopped       = false;
  QString playbackException;

  if (outputInstalled && wavLoadOk && loadedTrack) {
    try {
      TSoundOutputDevice player;
      player.setVolume(0.10);
      playbackOpenOk = player.open(loadedTrack);
      player.play(loadedTrack, 0, loadedTrack->getSampleCount() - 1, false,
                  false);

      for (int i = 0; i < 20; ++i) {
        if (player.isPlaying()) {
          playbackStarted = true;
          break;
        }
        QEventLoop waitLoop;
        QTimer::singleShot(50, &waitLoop, &QEventLoop::quit);
        waitLoop.exec();
      }

      player.stop();
      QEventLoop stopLoop;
      QTimer::singleShot(100, &stopLoop, &QEventLoop::quit);
      stopLoop.exec();
      playbackStopped = !player.isPlaying();
    } catch (TSoundDeviceException& e) {
      playbackException = QString::fromStdWString(e.getMessage());
    } catch (TException& e) {
      playbackException = QString::fromStdWString(e.getMessage());
    } catch (...) {
      playbackException = QStringLiteral("unknown");
    }
  }

  const qint64 fileSize = playbackInfo.size();
  details
      << QString("playbackPath=%1").arg(gui_smoke_status_value(playbackPath))
      << QString("writerStopped=%1")
             .arg(writerStopped ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("bytesWritten=%1").arg(bytesWritten)
      << QString("wavFileSize=%1").arg(fileSize)
      << QString("wavLoadOk=%1")
             .arg(wavLoadOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("wavSampleRate=%1")
             .arg(loadedTrack ? loadedTrack->getSampleRate() : 0)
      << QString("wavChannelCount=%1")
             .arg(loadedTrack ? loadedTrack->getChannelCount() : 0)
      << QString("wavBitPerSample=%1")
             .arg(loadedTrack ? loadedTrack->getBitPerSample() : 0)
      << QString("wavSampleCount=%1")
             .arg(loadedTrack ? loadedTrack->getSampleCount() : 0)
      << QString("wavDurationMs=%1")
             .arg(loadedTrack ? loadedTrack->getDuration() * 1000.0 : 0.0, 0,
                  'f', 2)
      << QString("audioOutputInstalled=%1")
             .arg(outputInstalled ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("playbackOpenOk=%1")
             .arg(playbackOpenOk ? QStringLiteral("true")
                                 : QStringLiteral("false"))
      << QString("playbackStarted=%1")
             .arg(playbackStarted ? QStringLiteral("true")
                                  : QStringLiteral("false"))
      << QString("playbackStoppedAfterStop=%1")
             .arg(playbackStopped ? QStringLiteral("true")
                                  : QStringLiteral("false"));
  if (!playbackException.isEmpty()) {
    details << QString("playbackException=%1")
                   .arg(gui_smoke_status_value(playbackException));
  }
  details << QString("audioPlaybackWavProbe=%1")
                 .arg(writerStopped && bytesWritten == samples.size() &&
                              fileSize > 44 && wavLoadOk && loadedTrack &&
                              loadedTrack->getSampleCount() == sampleCount &&
                              outputInstalled && playbackOpenOk &&
                              playbackStarted && playbackStopped
                          ? QStringLiteral("ok")
                          : QStringLiteral("error"));
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("audioPlaybackWavProbe=unsupported");
#endif

  return details;
}

static QStringList gui_smoke_audio_input_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QAudioDevice input = QMediaDevices::defaultAudioInput();
  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("defaultAudioInput=%1")
                 .arg(input.isNull()
                          ? QString()
                          : gui_smoke_status_value(input.description()))
          << QString("audioInputAvailable=%1")
                 .arg(input.isNull() ? QStringLiteral("false")
                                     : QStringLiteral("true"));

  if (input.isNull()) {
    details << QStringLiteral("audioInputProbe=no-device");
    return details;
  }

  const QAudioFormat format = input.preferredFormat();
  const int sampleRate      = format.sampleRate();
  const int bytesPerFrame   = format.bytesPerFrame();
  const int durationMs      = 500;
  details << QString("sampleRate=%1").arg(sampleRate)
          << QString("channelCount=%1").arg(format.channelCount())
          << QString("bytesPerFrame=%1").arg(bytesPerFrame)
          << QString("sampleFormat=%1").arg(format.sampleFormat());

  if (!format.isValid() || sampleRate <= 0 || bytesPerFrame <= 0) {
    details << QStringLiteral("audioInputProbe=invalid-format");
    return details;
  }

  QAudioSource source(input, format);
  QIODevice* capture = source.start();
  if (!capture) {
    details << QString("audioError=%1")
                   .arg(gui_smoke_audio_error_name(source.error()))
            << QStringLiteral("audioInputProbe=start-failed");
    return details;
  }

  qint64 bytesCaptured = 0;
  QEventLoop loop;
  QObject::connect(capture, &QIODevice::readyRead,
                   [&]() { bytesCaptured += capture->readAll().size(); });
  QObject::connect(&source, &QAudioSource::stateChanged,
                   [&](QAudio::State state) {
                     if (state == QAudio::StoppedState) loop.quit();
                   });
  QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
  loop.exec();

  bytesCaptured += capture->readAll().size();
  const QAudio::Error error = source.error();
  details << QString("audioState=%1")
                 .arg(gui_smoke_audio_state_name(source.state()))
          << QString("audioError=%1").arg(gui_smoke_audio_error_name(error))
          << QString("processedUSecs=%1").arg(source.processedUSecs())
          << QString("elapsedUSecs=%1").arg(source.elapsedUSecs())
          << QString("bytesCaptured=%1").arg(bytesCaptured)
          << QString("audioInputProbe=%1")
                 .arg(error == QAudio::NoError && bytesCaptured > 0
                          ? QStringLiteral("ok")
                          : QStringLiteral("error"));
  source.stop();
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("audioInputProbe=unsupported");
#endif

  return details;
}

static QStringList gui_smoke_audio_input_details_bounded() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  std::packaged_task<QStringList()> task(gui_smoke_audio_input_details);
  std::future<QStringList> future = task.get_future();
  std::thread(std::move(task)).detach();
  if (future.wait_for(std::chrono::seconds(8)) == std::future_status::ready) {
    return future.get();
  }
  return QStringList() << QStringLiteral("qtMultimediaApi=Qt6")
                       << QStringLiteral("audioInputProbe=timeout");
#else
  return gui_smoke_audio_input_details();
#endif
}

static QStringList gui_smoke_audio_recording_wav_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QAudioDevice input = QMediaDevices::defaultAudioInput();
  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("defaultAudioInput=%1")
                 .arg(input.isNull()
                          ? QString()
                          : gui_smoke_status_value(input.description()))
          << QString("audioInputAvailable=%1")
                 .arg(input.isNull() ? QStringLiteral("false")
                                     : QStringLiteral("true"));

  if (input.isNull()) {
    details << QStringLiteral("audioRecordingWavProbe=no-device");
    return details;
  }

  QAudioFormat format;
  format.setSampleRate(44100);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);
  if (!input.isFormatSupported(format)) format = input.preferredFormat();

  const int sampleRate    = format.sampleRate();
  const int bytesPerFrame = format.bytesPerFrame();
  const int durationMs    = 750;
  details << QString("sampleRate=%1").arg(sampleRate)
          << QString("channelCount=%1").arg(format.channelCount())
          << QString("bytesPerFrame=%1").arg(bytesPerFrame)
          << QString("sampleFormat=%1").arg(format.sampleFormat());

  if (!format.isValid() || sampleRate <= 0 || bytesPerFrame <= 0) {
    details << QStringLiteral("audioRecordingWavProbe=invalid-format");
    return details;
  }

  const QString statusPath =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  const QString outputDir = statusPath.isEmpty()
                                ? QDir::tempPath()
                                : QFileInfo(statusPath).absolutePath();
  QDir().mkpath(outputDir);
  const QString recordingPath =
      QDir(outputDir).filePath(QStringLiteral("audio-recording-wav.wav"));
  QFile::remove(recordingPath);

  AudioWriterWAV writer(format);
  if (!writer.start(recordingPath, false)) {
    details << QString("recordingPath=%1")
                   .arg(gui_smoke_status_value(recordingPath))
            << QStringLiteral("audioRecordingWavProbe=writer-start-failed");
    return details;
  }

  QAudioSource source(input, format);
  source.start(&writer);
  QEventLoop loop;
  QObject::connect(&source, &QAudioSource::stateChanged,
                   [&](QAudio::State state) {
                     if (state == QAudio::StoppedState) loop.quit();
                   });
  QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
  loop.exec();

  const QAudio::Error error = source.error();
  source.stop();
  const bool writerStopped = writer.stop();

  QFileInfo recordingInfo(recordingPath);
  const qint64 fileSize      = recordingInfo.size();
  const qint64 bytesRecorded = fileSize > 44 ? fileSize - 44 : 0;
  TSoundTrackP loadedTrack;
  const bool wavLoadOk =
      TSoundTrackReader::load(TFilePath(recordingPath), loadedTrack);

  details
      << QString("audioState=%1")
             .arg(gui_smoke_audio_state_name(source.state()))
      << QString("audioError=%1").arg(gui_smoke_audio_error_name(error))
      << QString("processedUSecs=%1").arg(source.processedUSecs())
      << QString("elapsedUSecs=%1").arg(source.elapsedUSecs())
      << QString("recordingPath=%1").arg(gui_smoke_status_value(recordingPath))
      << QString("writerStopped=%1")
             .arg(writerStopped ? QStringLiteral("true")
                                : QStringLiteral("false"))
      << QString("wavFileSize=%1").arg(fileSize)
      << QString("bytesRecorded=%1").arg(bytesRecorded)
      << QString("wavLoadOk=%1")
             .arg(wavLoadOk ? QStringLiteral("true") : QStringLiteral("false"))
      << QString("wavSampleRate=%1")
             .arg(loadedTrack ? loadedTrack->getSampleRate() : 0)
      << QString("wavChannelCount=%1")
             .arg(loadedTrack ? loadedTrack->getChannelCount() : 0)
      << QString("wavBitPerSample=%1")
             .arg(loadedTrack ? loadedTrack->getBitPerSample() : 0)
      << QString("wavSampleCount=%1")
             .arg(loadedTrack ? loadedTrack->getSampleCount() : 0)
      << QString("wavDurationMs=%1")
             .arg(loadedTrack ? loadedTrack->getDuration() * 1000.0 : 0.0, 0,
                  'f', 2)
      << QString("audioRecordingWavProbe=%1")
             .arg(error == QAudio::NoError && writerStopped && wavLoadOk &&
                          bytesRecorded > 0 && loadedTrack &&
                          loadedTrack->getSampleCount() > 0
                      ? QStringLiteral("ok")
                      : QStringLiteral("error"));
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("audioRecordingWavProbe=unsupported");
#endif

  return details;
}

static QStringList gui_smoke_audio_recording_wav_details_bounded() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  std::packaged_task<QStringList()> task(gui_smoke_audio_recording_wav_details);
  std::future<QStringList> future = task.get_future();
  std::thread(std::move(task)).detach();
  if (future.wait_for(std::chrono::seconds(8)) == std::future_status::ready) {
    return future.get();
  }
  return QStringList() << QStringLiteral("qtMultimediaApi=Qt6")
                       << QStringLiteral("audioRecordingWavProbe=timeout");
#else
  return gui_smoke_audio_recording_wav_details();
#endif
}

static QStringList gui_smoke_camera_format_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QList<QCameraDevice> videoInputs = QMediaDevices::videoInputs();
  const QCameraDevice defaultVideoInput  = QMediaDevices::defaultVideoInput();
  const QList<QCameraFormat> formats     = defaultVideoInput.isNull()
                                               ? QList<QCameraFormat>()
                                               : defaultVideoInput.videoFormats();
  QStringList formatValues;
  bool hasFormatSummary = false;
  int minWidth          = 0;
  int minHeight         = 0;
  int maxWidth          = 0;
  int maxHeight         = 0;
  float minFrameRate    = 0.0f;
  float maxFrameRate    = 0.0f;

  for (const QCameraFormat& format : formats) {
    const QSize resolution         = format.resolution();
    const float formatMinFrameRate = format.minFrameRate();
    const float formatMaxFrameRate = format.maxFrameRate();

    formatValues << QString("%1x%2@%3-%4fps:%5")
                        .arg(resolution.width())
                        .arg(resolution.height())
                        .arg(formatMinFrameRate, 0, 'f', 2)
                        .arg(formatMaxFrameRate, 0, 'f', 2)
                        .arg(static_cast<int>(format.pixelFormat()));

    if (!hasFormatSummary) {
      minWidth         = resolution.width();
      minHeight        = resolution.height();
      maxWidth         = resolution.width();
      maxHeight        = resolution.height();
      minFrameRate     = formatMinFrameRate;
      maxFrameRate     = formatMaxFrameRate;
      hasFormatSummary = true;
      continue;
    }

    minWidth     = std::min(minWidth, resolution.width());
    minHeight    = std::min(minHeight, resolution.height());
    maxWidth     = std::max(maxWidth, resolution.width());
    maxHeight    = std::max(maxHeight, resolution.height());
    minFrameRate = std::min(minFrameRate, formatMinFrameRate);
    maxFrameRate = std::max(maxFrameRate, formatMaxFrameRate);
  }

  const QString probe =
      videoInputs.isEmpty()
          ? QStringLiteral("no-camera")
          : (defaultVideoInput.isNull()
                 ? QStringLiteral("no-default-camera")
                 : (formats.isEmpty() ? QStringLiteral("no-formats")
                                      : QStringLiteral("ok")));

  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("videoInputCount=%1").arg(videoInputs.size())
          << QString("defaultVideoInput=%1")
                 .arg(defaultVideoInput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultVideoInput.description()))
          << QString("defaultVideoInputAvailable=%1")
                 .arg(defaultVideoInput.isNull() ? QStringLiteral("false")
                                                 : QStringLiteral("true"))
          << QString("defaultVideoFormatCount=%1").arg(formats.size())
          << QString("defaultVideoMinWidth=%1").arg(minWidth)
          << QString("defaultVideoMinHeight=%1").arg(minHeight)
          << QString("defaultVideoMaxWidth=%1").arg(maxWidth)
          << QString("defaultVideoMaxHeight=%1").arg(maxHeight)
          << QString("defaultVideoMinFrameRate=%1").arg(minFrameRate, 0, 'f', 2)
          << QString("defaultVideoMaxFrameRate=%1").arg(maxFrameRate, 0, 'f', 2)
          << QString("defaultVideoFormats=%1")
                 .arg(gui_smoke_joined_values(formatValues))
          << QString("cameraFormatProbe=%1").arg(probe);
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("cameraFormatProbe=unsupported");
#endif

  return details;
}

static void run_gui_smoke_hook(const QString& action,
                               const QString& requestedSceneName,
                               const TFilePath& loadedScenePath) {
  if (action.isEmpty()) return;

  try {
    if (action == "startup") {
      write_gui_smoke_status(
          action, "ok",
          QStringList()
              << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle()));
      return;
    }

    if (action == "open-scene") {
      ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
      write_gui_smoke_status(
          action, "ok",
          QStringList()
              << QString("scene=%1").arg(scene->getScenePath().getQString())
              << QString("requested=%1").arg(loadedScenePath.getQString())
              << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle()));
      return;
    }

    if (action == "high-dpi") {
      QWidget* mainWindow = TApp::instance()->getMainWindow();
      QScreen* screen =
          mainWindow ? mainWindow->screen() : QApplication::primaryScreen();
      if (!screen) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to resolve screen"));
        return;
      }

      const QRect geometry = screen->geometry();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      const QString highDpiMode = QStringLiteral("qt6-always-on");
#else
      const QString highDpiMode = QStringLiteral("qt5-attribute");
#endif

      write_gui_smoke_status(
          action, "ok",
          QStringList() << QString("windowDevicePixelRatio=%1")
                               .arg(mainWindow ? mainWindow->devicePixelRatioF()
                                               : 0.0,
                                    0, 'f', 2)
                        << QString("screenDevicePixelRatio=%1")
                               .arg(screen->devicePixelRatio(), 0, 'f', 2)
                        << QString("logicalDpiX=%1")
                               .arg(screen->logicalDotsPerInchX(), 0, 'f', 2)
                        << QString("logicalDpiY=%1")
                               .arg(screen->logicalDotsPerInchY(), 0, 'f', 2)
                        << QString("physicalDpiX=%1")
                               .arg(screen->physicalDotsPerInchX(), 0, 'f', 2)
                        << QString("physicalDpiY=%1")
                               .arg(screen->physicalDotsPerInchY(), 0, 'f', 2)
                        << QString("screenGeometry=%1x%2+%3+%4")
                               .arg(geometry.width())
                               .arg(geometry.height())
                               .arg(geometry.x())
                               .arg(geometry.y())
                        << QString("screenName=%1").arg(screen->name())
                        << QString("highDpiMode=%1").arg(highDpiMode)
                        << QString("window=%1")
                               .arg(mainWindow ? mainWindow->windowTitle()
                                               : QString()));
      return;
    }

    if (action == "media-devices") {
      QStringList details = gui_smoke_media_device_details();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      write_gui_smoke_status(action, "ok", details);
      return;
    }

    if (action == "audio-input") {
      QStringList details = gui_smoke_audio_input_details_bounded();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(QStringLiteral("audioInputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-recording-wav") {
      QStringList details = gui_smoke_audio_recording_wav_details_bounded();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("audioRecordingWavProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-playback-wav") {
      QStringList details = gui_smoke_audio_playback_wav_details();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("audioPlaybackWavProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "camera-formats") {
      QStringList details = gui_smoke_camera_format_details();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("cameraFormatProbe=ok")) ||
          details.contains(QStringLiteral("cameraFormatProbe=no-camera"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-output") {
      QStringList details = gui_smoke_audio_output_details();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(QStringLiteral("audioOutputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "script-console-view") {
      QStringList details = gui_smoke_script_console_view_details();
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("scriptConsoleViewProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "preview-render-output") {
      QStringList details =
          gui_smoke_preview_render_output_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(QStringLiteral("previewRenderProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "fx-preview-render-output") {
      QStringList details =
          gui_smoke_fx_preview_render_output_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("fxPreviewRenderProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "fx-preview-subcamera-render-output") {
      QStringList details =
          gui_smoke_fx_preview_subcamera_render_output_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("fxPreviewSubcameraProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "fx-preview-flipbook-controls") {
      QStringList details =
          gui_smoke_fx_preview_flipbook_controls_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("fxPreviewFlipbookControlsProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "fx-preview-save-previewed-frames") {
      QStringList details = gui_smoke_fx_preview_save_previewed_frames_details(
          requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(
          QStringLiteral("fxPreviewSavePreviewedFramesProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "fx-preview-subcamera-save-previewed-frames") {
      QStringList details = gui_smoke_fx_preview_save_previewed_frames_details(
          requestedSceneName, true);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(
          QStringLiteral("fxPreviewSavePreviewedFramesProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "final-render-output" ||
        action == "final-render-background-output" ||
        action == "final-render-sequence-output" ||
        action == "final-render-composite-output" ||
        action == "final-render-vector-output" ||
        action == "final-render-toonz-raster-output" ||
        action == "final-render-fx-output") {
      const bool opaqueBackground = action == "final-render-background-output";
      const bool sequenceOutput   = action == "final-render-sequence-output";
      const bool compositeOutput  = action == "final-render-composite-output";
      const bool vectorOutput     = action == "final-render-vector-output";
      const bool fxOutput         = action == "final-render-fx-output";
      const bool toonzRasterOutput =
          action == "final-render-toonz-raster-output";
      QStringList details = gui_smoke_final_render_output_details(
          requestedSceneName, opaqueBackground, sequenceOutput, compositeOutput,
          vectorOutput, fxOutput, toonzRasterOutput);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("finalRenderProbe=ok")) &&
          details.contains(QStringLiteral("finalRenderOutputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-render" || action == "viewer-vector-render") {
      QStringList details = gui_smoke_viewer_render_details(
          requestedSceneName, action == "viewer-vector-render");
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-zoom-pan") {
      QStringList details =
          gui_smoke_viewer_zoom_pan_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("viewerTransformProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin") {
      QStringList details =
          gui_smoke_viewer_onion_skin_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-rowarea") {
      QStringList details =
          gui_smoke_viewer_onion_skin_row_area_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinUiProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-rowarea-drag") {
      QStringList details =
          gui_smoke_viewer_onion_skin_row_area_drag_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinDragProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-fixed-marker-drag") {
      QStringList details =
          gui_smoke_viewer_onion_skin_fixed_marker_drag_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinFixedDragProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-shift-trace") {
      QStringList details =
          gui_smoke_viewer_onion_skin_shift_trace_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinShiftTraceProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-context-menu") {
      QStringList details =
          gui_smoke_viewer_onion_skin_context_menu_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinMenuProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-custom-colors") {
      QStringList details =
          gui_smoke_viewer_onion_skin_custom_colors_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinCustomColorProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin-orientations") {
      QStringList details =
          gui_smoke_viewer_onion_skin_orientations_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinProbe=ok")) &&
          details.contains(QStringLiteral("onionSkinOrientationProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-camera-overlay") {
      QStringList details =
          gui_smoke_viewer_camera_overlay_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("cameraOverlayProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-safe-area-field-guide") {
      QStringList details =
          gui_smoke_viewer_safe_area_field_guide_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("safeAreaProbe=ok")) &&
          details.contains(QStringLiteral("fieldGuideProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-safe-area-presets") {
      QStringList details =
          gui_smoke_viewer_safe_area_presets_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("safeAreaPresetProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-safe-area-custom-file") {
      QStringList details =
          gui_smoke_viewer_safe_area_custom_file_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("safeAreaCustomFileProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-field-guide-settings") {
      QStringList details =
          gui_smoke_viewer_field_guide_settings_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("fieldGuideSettingsProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerGuideProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide-events") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_events_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerGuideEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide-variants") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_variants_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerGuideVariantProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide-lines") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_lines_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerGuideLineProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide-styles") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_styles_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerStyleProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-ticks") {
      QStringList details =
          gui_smoke_viewer_ruler_ticks_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("rulerTickProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-overlay") {
      QStringList details =
          gui_smoke_viewer_animate_tool_overlay_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolOverlayProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-drag") {
      QStringList details =
          gui_smoke_viewer_animate_tool_drag_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolTransformProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-mouse-events") {
      QStringList details =
          gui_smoke_viewer_animate_tool_mouse_event_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolTransformProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-undo-redo") {
      QStringList details =
          gui_smoke_viewer_animate_tool_undo_redo_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolTransformProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("undoRedoProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-modifiers") {
      QStringList details =
          gui_smoke_viewer_animate_tool_modifiers_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolTransformProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("modifierProbe=ok")) &&
          details.contains(QStringLiteral("cursorPreciseProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-handles") {
      QStringList details =
          gui_smoke_viewer_animate_tool_handles_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("handleHoverProbe=ok")) &&
          details.contains(QStringLiteral("handleHitTestProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-handle-variants") {
      QStringList details =
          gui_smoke_viewer_animate_tool_handle_variants_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("handleVariantProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-axis-drags") {
      QStringList details =
          gui_smoke_viewer_animate_tool_axis_drags_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("axisDragProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-cursors") {
      QStringList details =
          gui_smoke_viewer_animate_tool_cursors_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("animateCursorProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-vector-handles") {
      QStringList details =
          gui_smoke_viewer_selection_tool_vector_handles_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionRectProbe=ok")) &&
          details.contains(QStringLiteral("selectionCursorProbe=ok")) &&
          details.contains(QStringLiteral("selectionHandleProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-vector-handle-variants") {
      QStringList details =
          gui_smoke_viewer_selection_tool_vector_handle_variants_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionRectProbe=ok")) &&
          details.contains(QStringLiteral("selectionVariantCursorProbe=ok")) &&
          details.contains(QStringLiteral("selectionVariantHandleProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-vector-center-thickness-deform") {
      QStringList details =
          gui_smoke_viewer_selection_tool_vector_center_thickness_deform_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionRectProbe=ok")) &&
          details.contains(QStringLiteral("selectionAdvancedCursorProbe=ok")) &&
          details.contains(QStringLiteral("selectionAdvancedHandleProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-vector-mode-variants") {
      QStringList details =
          gui_smoke_viewer_selection_tool_vector_mode_variants_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionFreehandProbe=ok")) &&
          details.contains(QStringLiteral("selectionPolylineProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-raster-handles") {
      QStringList details =
          gui_smoke_viewer_selection_tool_raster_handles_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionRectProbe=ok")) &&
          details.contains(QStringLiteral("selectionRasterCursorProbe=ok")) &&
          details.contains(QStringLiteral("selectionRasterHandleProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-selection-tool-raster-mode-variants") {
      QStringList details =
          gui_smoke_viewer_selection_tool_raster_mode_variants_details(
              requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("selectionToolProbe=ok")) &&
          details.contains(QStringLiteral("selectionFreehandProbe=ok")) &&
          details.contains(QStringLiteral("selectionPolylineProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-vector-brush") {
      QStringList details =
          gui_smoke_viewer_vector_brush_details(requestedSceneName);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolInputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-raster-brush" ||
        action == "viewer-raster-brush-mouse-events" ||
        action == "viewer-raster-brush-tablet-events" ||
        action == "viewer-raster-brush-system-events") {
      GuiSmokeRasterBrushInput input = GuiSmokeRasterBrushInput::DirectTool;
      if (action == "viewer-raster-brush-mouse-events")
        input = GuiSmokeRasterBrushInput::MouseEvents;
      else if (action == "viewer-raster-brush-tablet-events")
        input = GuiSmokeRasterBrushInput::TabletEvents;
      else if (action == "viewer-raster-brush-system-events")
        input = GuiSmokeRasterBrushInput::SystemMouseEvents;

      QStringList details = gui_smoke_viewer_raster_brush_details(
          requestedSceneName, input, action);
      details << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
          details.contains(QStringLiteral("toolInputProbe=ok")) &&
          details.contains(QStringLiteral("rasterInputProbe=ok")) &&
          details.contains(QStringLiteral("mouseEventProbe=ok")) &&
          details.contains(QStringLiteral("tabletEventProbe=ok")) &&
          details.contains(QStringLiteral("systemMouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "create-scene") {
      QString sceneName   = gui_smoke_scene_name(requestedSceneName);
      TFilePath scenePath = gui_smoke_scene_path(sceneName);
      if (!TSystem::touchParentDir(scenePath)) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to create scene folder")
                          << QString("scene=%1").arg(scenePath.getQString()));
        return;
      }

      IoCmd::newScene();
      ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
      scene->setScenePath(scenePath);
      bool saved = IoCmd::saveScene(scenePath, IoCmd::SILENTLY_OVERWRITE);
      if (saved) {
        TApp::instance()->getCurrentScene()->notifySceneSwitched();
        TApp::instance()->getCurrentScene()->notifyNameSceneChange();
      }

      write_gui_smoke_status(
          action, saved ? "ok" : "error",
          QStringList()
              << QString("scene=%1").arg(scenePath.getQString())
              << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle()));
      return;
    }

    if (action == "xsheet-scrub") {
      QString sceneName   = gui_smoke_scene_name(requestedSceneName);
      TFilePath scenePath = gui_smoke_scene_path(sceneName);
      if (!TSystem::touchParentDir(scenePath)) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to create scene folder")
                          << QString("scene=%1").arg(scenePath.getQString()));
        return;
      }

      IoCmd::newScene();
      ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
      scene->setScenePath(scenePath);

      TXshLevel* rasterXl =
          scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_raster");
      TXshLevel* vectorXl =
          scene->createNewLevel(PLI_XSHLEVEL, L"qt6_gui_vector");
      TXshSimpleLevel* rasterLevel =
          rasterXl ? rasterXl->getSimpleLevel() : nullptr;
      TXshSimpleLevel* vectorLevel =
          vectorXl ? vectorXl->getSimpleLevel() : nullptr;
      const TFrameId fid(1);

      if (!add_gui_smoke_frame(rasterLevel, fid) ||
          !add_gui_smoke_frame(vectorLevel, fid)) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to create smoke frames"));
        return;
      }

      TXsheet* xsheet = scene->getXsheet();
      const bool rasterCellOk =
          xsheet->setCell(0, 0, TXshCell(rasterLevel, fid)) &&
          xsheet->setCell(1, 0, TXshCell(rasterLevel, fid));
      const bool vectorCellOk =
          xsheet->setCell(2, 1, TXshCell(vectorLevel, fid));
      if (!rasterCellOk || !vectorCellOk) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to populate xsheet"));
        return;
      }

      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->notifySceneChanged();

      const bool saved = IoCmd::saveScene(scenePath, IoCmd::SILENTLY_OVERWRITE);
      if (saved) {
        TApp::instance()->getCurrentScene()->notifySceneSwitched();
        TApp::instance()->getCurrentScene()->notifyNameSceneChange();
      }

      TApp::instance()->getCurrentFrame()->setFrame(0);
      TApp::instance()->getCurrentFrame()->setFrame(2);
      TApp::instance()->getCurrentColumn()->setColumnIndex(1);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

      write_gui_smoke_status(
          action, saved ? "ok" : "error",
          QStringList()
              << QString("scene=%1").arg(scenePath.getQString())
              << QString("frame=%1")
                     .arg(TApp::instance()->getCurrentFrame()->getFrame())
              << QString("column=%1")
                     .arg(
                         TApp::instance()->getCurrentColumn()->getColumnIndex())
              << QString("frameCount=%1").arg(scene->getFrameCount())
              << QString("columnCount=%1").arg(xsheet->getColumnCount())
              << QString("rasterFrames=%1").arg(rasterLevel->getFrameCount())
              << QString("vectorFrames=%1").arg(vectorLevel->getFrameCount())
              << QString("window=%1")
                     .arg(TApp::instance()->getMainWindow()->windowTitle()));
      return;
    }

    write_gui_smoke_status(
        action, "error",
        QStringList() << QString("message=unsupported GUI smoke action"));
  } catch (const TException& e) {
    write_gui_smoke_status(
        action, "error",
        QStringList() << QString("message=%1")
                             .arg(QString::fromStdWString(e.getMessage())));
  } catch (const std::exception& e) {
    write_gui_smoke_status(
        action, "error", QStringList() << QString("message=%1").arg(e.what()));
  } catch (...) {
    write_gui_smoke_status(
        action, "error", QStringList() << QString("message=unknown exception"));
  }
}

//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  // Enable standard input/output on Windows Platform for debug
  if (::AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen("CON", "r", stdin);
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
    atexit([]() { ::FreeConsole(); });
  }
#endif

  // Install signal handlers to catch crashes
  CrashHandler::install();

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

    const std::map<std::string, std::string>& spm = TEnv::getSystemPathMap();
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

#if OPENTOONZ_QT_MAJOR >= 6
  const bool runScriptWithQApplication =
      qEnvironmentVariableIsSet("OPENTOONZ_SCRIPT_USE_QAPPLICATION");
  if (loadFilePath.getType() == "toonzscript" && !runScriptWithQApplication) {
    try {
      QCoreApplication a(argc, argv);
      TSystem::hasMainLoop(true);
      TMessageRepository::instance();
      ThirdParty::initialize();
      initToonzEnv(argumentPathValues, false);
      TThread::init();
      return run_script(loadFilePath);
    } catch (const TException& e) {
      std::cerr << QString::fromStdWString(e.getMessage()).toStdString()
                << std::endl;
      return 1;
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
      return 1;
    } catch (...) {
      std::cerr << "Unknown script-mode initialization error" << std::endl;
      return 1;
    }
  }
#endif

  // Enables high-DPI scaling in Qt 5. Qt 6 always enables this behavior and
  // deprecates the application attribute.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
  log_gui_smoke_progress("before-qapplication");

  QApplication a(argc, argv);
  log_gui_smoke_progress("qapplication-created");

#ifdef MACOSX
  // This workaround is to avoid missing left button problem on Qt5.6.0.
  // To invalidate m_rightButtonClicked in Qt/qnsview.mm, sending
  // NSLeftButtonDown event before NSLeftMouseDragged event propagated to
  // QApplication. See more details in ../mousedragfilter/mousedragfilter.mm.

#include "mousedragfilter.h"

  class OSXMouseDragFilter final : public QAbstractNativeEventFilter {
    bool leftButtonPressed = false;

  public:
    bool nativeEventFilter(const QByteArray& eventType, void* message,
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                           long*) override {
#else
                           qintptr*) override {
#endif
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

// Enable smooth icons on high-DPI monitors in Qt 5. Qt 6 always enables this.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
a.setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if defined(_WIN32)
// Compress tablet events with application attributes instead of implementing
// the delay-timer by ourselves
a.setAttribute(Qt::AA_CompressHighFrequencyEvents);
a.setAttribute(Qt::AA_CompressTabletEvents);
#endif

#ifdef _WIN32
// BUG_WORKAROUND: #20230627
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

const bool isRunScript = (loadFilePath.getType() == "toonzscript");
const QString scriptWorkingDirectory = qEnvironmentVariable(
    "OPENTOONZ_SCRIPT_WORKING_DIRECTORY", QDir::currentPath());

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

// Set show icons in menus flag (use iconVisibleInMenu to disable selectively)
QApplication::instance()->setAttribute(Qt::AA_DontShowIconsInMenus, false);

TEnv::setApplicationFileName(argv[0]);

#if OPENTOONZ_QT_MAJOR >= 6
if (isRunScript) {
  try {
    TSystem::hasMainLoop(true);
    TMessageRepository::instance();
    TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
        &toonzRunOutOfContMemHandler);
    initToonzEnv(argumentPathValues, false);
    log_gui_smoke_progress("toonz-env-initialized");
    IconGenerator::setFilmstripIconSize(Preferences::instance()->getIconSize());
    TThread::init();
    log_gui_smoke_progress("threads-initialized");
    QDir::setCurrent(scriptWorkingDirectory);
    return run_script(loadFilePath);
  } catch (const TException& e) {
    std::cerr << QString::fromStdWString(e.getMessage()).toStdString()
              << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown QApplication script-mode initialization error"
              << std::endl;
    return 1;
  }
}
#endif

  // splash screen (override with local file if present)
  QString exeDir          = QCoreApplication::applicationDirPath();
  QString localSplashPath = QDir(exeDir).filePath("splash.svg");

  QPixmap splashPixmap;

  if (QFileInfo(localSplashPath).exists() &&
      QFileInfo(localSplashPath).isFile()) {
    splashPixmap = QIcon(localSplashPath).pixmap(QSize(610, 344));
    if (splashPixmap.isNull()) {
      // fallback if loading fails
      splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
    }
  } else {
    splashPixmap = QIcon(":Resources/splash.svg").pixmap(QSize(610, 344));
  }
#ifdef _WIN32
QFont font("Segoe UI", -1);
#else
  QFont font("Helvetica", -1);
#endif
font.setPixelSize(13);
font.setWeight(QFont::Normal);
a.setFont(font);

QString offsetStr("\n\n\n\n\n\n\n\n");

TSystem::hasMainLoop(true);

TMessageRepository::instance();

QSplashScreen splash(splashPixmap);
if (!isRunScript) splash.show();
a.processEvents();

splash.showMessage(offsetStr + "Initializing OpenGL format...", Qt::AlignCenter,
                   Qt::white);
a.processEvents();

// OpenGL
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QGLFormat fmt;
fmt.setAlpha(true);
fmt.setStencil(true);
QGLFormat::setDefaultFormat(fmt);
#else
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  fmt.setAlphaBufferSize(8);
  fmt.setStencilBufferSize(8);
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
#if defined(MACOSX) || defined(LINUX) || defined(FREEBSD)
  fmt.setVersion(2, 1);
#endif
  QSurfaceFormat::setDefaultFormat(fmt);
#endif

#ifndef __HAIKU__
glutInit(&argc, argv);
#endif

splash.showMessage(offsetStr + "Initializing Toonz environment ...",
                   Qt::AlignCenter, Qt::white);
a.processEvents();

  // Install run out of contiguous memory callback
  TBigMemoryManager::instance()->setRunOutOfContiguousMemoryHandler(
      &toonzRunOutOfContMemHandler);

  // Toonz environment
  initToonzEnv(argumentPathValues);
  log_gui_smoke_progress("toonz-env-initialized");

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
log_gui_smoke_progress("threads-initialized");

TProjectManager* projectManager = TProjectManager::instance();
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
      } catch (TException& e) {
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
(void)translator.load("toonz", languagePathString);

// La installo
a.installTranslator(&translator);

// Carico la traduzione contenuta in toonzqt.qm (se e' presente)
QTranslator translator2;
(void)translator2.load("toonzqt", languagePathString);
a.installTranslator(&translator2);

// Carico la traduzione contenuta in tnzcore.qm (se e' presente)
QTranslator tnzcoreTranslator;
(void)tnzcoreTranslator.load("tnzcore", languagePathString);
qApp->installTranslator(&tnzcoreTranslator);

// Carico la traduzione contenuta in toonzlib.qm (se e' presente)
QTranslator toonzlibTranslator;
(void)toonzlibTranslator.load("toonzlib", languagePathString);
qApp->installTranslator(&toonzlibTranslator);

// Carico la traduzione contenuta in colorfx.qm (se e' presente)
QTranslator colorfxTranslator;
(void)colorfxTranslator.load("colorfx", languagePathString);
qApp->installTranslator(&colorfxTranslator);

// Carico la traduzione contenuta in tools.qm
QTranslator toolTranslator;
(void)toolTranslator.load("tnztools", languagePathString);
qApp->installTranslator(&toolTranslator);

// load translation for file writers properties
QTranslator imageTranslator;
(void)imageTranslator.load("image", languagePathString);
qApp->installTranslator(&imageTranslator);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
const QString qtTranslationsPath =
    QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
  const QString qtTranslationsPath =
      QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
QTranslator qtTranslator;
(void)qtTranslator.load("qt_" + QLocale::system().name(), qtTranslationsPath);
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

// Initialize ThemeManager before TApp
auto& themeManager = ThemeManager::getInstance();
themeManager.initialize();

TTool::setApplication(TApp::instance());
TApp::instance()->init();
log_gui_smoke_progress("tapp-initialized");

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
log_gui_smoke_progress("main-window-constructing");
MainWindow w(argumentLayoutFileName);
log_gui_smoke_progress("main-window-constructed");
CrashHandler::attachParentWindow(&w);
CrashHandler::reportProjectInfo(true);

if (isRunScript) {
  QDir::setCurrent(scriptWorkingDirectory);
  return run_script(loadFilePath);
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

// Parse inital stylesheet in ThemeManager
themeManager.parseCustomPropertiesFromStylesheet(currentStyle);

// w.setWindowTitle(QString::fromStdString(TEnv::getApplicationFullName()));
w.changeWindowTitle();
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
if (Preferences::instance()->isLatestVersionCheckEnabled()) w.checkForUpdates();

w.show();
log_gui_smoke_progress("main-window-shown");

QString guiSmokeAction = qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_ACTION");
if (!guiSmokeAction.isEmpty()) {
  log_gui_smoke_progress("hook-starting");
  if (!loadFilePath.isEmpty() && TFileStatus(loadFilePath).doesExist())
    IoCmd::loadScene(loadFilePath);
  QString guiSmokeSceneName =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_SCENE_NAME");
  TFilePath guiSmokeLoadedScenePath = loadFilePath;
  run_gui_smoke_hook(guiSmokeAction, guiSmokeSceneName,
                     guiSmokeLoadedScenePath);
  log_gui_smoke_progress("hook-finished");
} else {
  // Show floating panels only after the main window has been shown
  w.startupFloatingPanels();

  CommandManager::instance()->execute(T_Hand);
  if (!loadFilePath.isEmpty()) {
    splash.showMessage(
        QString("Loading file '") + loadFilePath.getQString() + "'...",
        Qt::AlignCenter, Qt::white);
    if (TFileStatus(loadFilePath).doesExist()) IoCmd::loadScene(loadFilePath);
  }
}

QFont* myFont;
QString fontName  = Preferences::instance()->getInterfaceFont();
QString fontStyle = Preferences::instance()->getInterfaceFontStyle();

TFontManager* fontMgr = TFontManager::instance();
std::vector<std::wstring> typefaces;
bool isBold = false, isItalic = false, hasKerning = false;
try {
  fontMgr->loadFontNames();
  fontMgr->setFamily(fontName.toStdWString());
  fontMgr->getAllTypefaces(typefaces);
  isBold     = fontMgr->isBold(fontName, fontStyle);
  isItalic   = fontMgr->isItalic(fontName, fontStyle);
  hasKerning = fontMgr->hasKerning();
} catch (TFontCreationError&) {
  // Do nothing. A default font should load
}

myFont = new QFont(fontName);
myFont->setPixelSize(EnvSoftwareCurrentFontSize);
myFont->setBold(isBold);
myFont->setItalic(isItalic);
myFont->setKerning(hasKerning);

a.setFont(*myFont);

QAction* action = CommandManager::instance()->getAction("MI_OpenTMessage");
if (action)
  QObject::connect(TMessageRepository::instance(), SIGNAL(openMessageCenter()),
                   action, SLOT(trigger()));

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
  KisTabletSupportWin8* penFilter = new KisTabletSupportWin8();
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

return ret;
}
