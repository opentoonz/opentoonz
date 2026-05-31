

// Tnz6 includes
#include "audiorecordingpopup.h"
#include "crashhandler.h"
#include "mainwindow.h"
#include "sceneviewer.h"
#include "ruler.h"
#include "flipbook.h"
#include "tapp.h"
#include "iocommand.h"
#include "previewfxmanager.h"
#include "cleanupsettingspopup.h"
#include "filebrowsermodel.h"
#include "expressionreferencemanager.h"
#include "thirdparty.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"

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
#include "trasterimage.h"
#include "timage.h"
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
#include <QMouseEvent>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QInputDevice>
#include <QPointingDevice>
#endif
#include <QTabletEvent>
#include <QTimer>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>

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

// These are the same as the default values. See tenv.cpp and tversion.h
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
static void initToonzEnv(QHash<QString, QString> &argPathValues,
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
  TFilePath cacheDir;
  if (preferWritablePlatformCache) cacheDir = ToonzFolder::getCacheRootFolder();
  if (cacheDir.isEmpty()) cacheDir = TEnv::getStuffDir() + "cache";
  TImageCache::instance()->setRootDir(cacheDir);
}

//-----------------------------------------------------------------------------

static std::atomic_bool gScriptHadError(false);

static void script_output(int type, const QString &value) {
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

static int run_script(const TFilePath &loadFilePath) {
  if (!TFileStatus(loadFilePath).doesExist()) {
    std::cerr << QObject::tr("Script file %1 does not exists.")
                     .arg(loadFilePath.getQString())
                     .toStdString()
              << std::endl;
    return 1;
  }

  TProjectManager *pm = TProjectManager::instance();
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

static void write_gui_smoke_status(const QString &action, const QString &status,
                                   const QStringList &details = QStringList()) {
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
  for (const QString &detail : details) out << detail << "\n";
}

static void log_gui_smoke_progress(const char *message) {
  const char *action = std::getenv("OPENTOONZ_GUI_SMOKE_ACTION");
  if (!action || !*action) return;

  std::cerr << "[gui-smoke] " << message << std::endl;
}

static QString gui_smoke_scene_name(const QString &requestedSceneName) {
  QString sceneName = requestedSceneName.trimmed();
  return sceneName.isEmpty() ? QStringLiteral("qt6_gui_smoke") : sceneName;
}

static TFilePath gui_smoke_scene_path(const QString &sceneName) {
  auto project = TProjectManager::instance()->getCurrentProject();
  return project->getScenesPath() +
         TFilePath(sceneName.toStdWString() + L".tnz");
}

static bool add_gui_smoke_frame(TXshSimpleLevel *level, const TFrameId &fid) {
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

static QString gui_smoke_joined_values(const QStringList &values) {
  QStringList sanitized;
  for (const QString &value : values) {
    sanitized << gui_smoke_status_value(value);
  }
  return sanitized.join('|');
}

static void gui_smoke_pump_events(int msec = 50) {
  QApplication::processEvents(QEventLoop::AllEvents, msec);
  QEventLoop loop;
  QTimer::singleShot(msec, &loop, &QEventLoop::quit);
  loop.exec();
  QApplication::processEvents(QEventLoop::AllEvents, msec);
}

static SceneViewer *gui_smoke_resolve_scene_viewer() {
  SceneViewer *viewer = TApp::instance()->getActiveViewer();
  if (viewer) return viewer;

  QMainWindow *mainWindow = TApp::instance()->getMainWindow();
  viewer = mainWindow ? mainWindow->findChild<SceneViewer *>() : nullptr;
  if (viewer) TApp::instance()->setActiveViewer(viewer);
  return viewer;
}

static bool gui_smoke_set_view_toggle(const char *commandId,
                                      ToggleCommandHandler &toggle,
                                      bool enabled) {
  QAction *action = CommandManager::instance()->getAction(commandId);
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

static QImage gui_smoke_grab_viewer_frame(SceneViewer *viewer) {
  if (!viewer) return QImage();

  viewer->GLInvalidateAll();
  viewer->update();
  gui_smoke_pump_events();
  viewer->repaint();
  gui_smoke_pump_events();
  return viewer->grabFramebuffer().convertToFormat(QImage::Format_ARGB32);
}

struct GuiSmokeImageStats {
  int sampleCount         = 0;
  int changedPixels       = 0;
  int redPixels           = 0;
  int changedRedPixels    = 0;
  int grayPixels          = 0;
  int changedGrayPixels   = 0;
  int changedNeutralPixels = 0;
  int onionPixels         = 0;
  int baselineOnionPixels = 0;
};

struct GuiSmokeRasterStats {
  int sampleCount  = 0;
  int opaquePixels = 0;
  int redPixels    = 0;
};

static bool gui_smoke_is_onion_pixel(int red, int green, int blue) {
  return red > 220 && green > 150 && blue > 180 && red > green &&
         blue > green && std::abs(red - blue) < 80;
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
    const QImage &image, const QImage &baseline = QImage()) {
  GuiSmokeImageStats stats;
  if (image.isNull()) return stats;

  const bool compareBaseline = !baseline.isNull() &&
                               baseline.width() == image.width() &&
                               baseline.height() == image.height();
  for (int y = 0; y < image.height(); ++y) {
    const QRgb *scanLine = reinterpret_cast<const QRgb *>(image.constScanLine(y));
    const QRgb *baselineLine =
        compareBaseline
            ? reinterpret_cast<const QRgb *>(baseline.constScanLine(y))
            : nullptr;
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = scanLine[x];
      const int red    = qRed(pixel);
      const int green  = qGreen(pixel);
      const int blue   = qBlue(pixel);
      ++stats.sampleCount;

      if (red > 120 && red > green * 2 && red > blue * 2) ++stats.redPixels;
      if (gui_smoke_is_gray_overlay_pixel(red, green, blue))
        ++stats.grayPixels;
      if (gui_smoke_is_onion_pixel(red, green, blue)) ++stats.onionPixels;

      if (baselineLine) {
        const QRgb oldPixel = baselineLine[x];
        const int oldRed    = qRed(oldPixel);
        const int oldGreen  = qGreen(oldPixel);
        const int oldBlue   = qBlue(oldPixel);
        const int delta = std::abs(red - oldRed) +
                          std::abs(green - oldGreen) +
                          std::abs(blue - oldBlue);
        if (delta > 48) {
          ++stats.changedPixels;
          if (red > 120 && red > green * 2 && red > blue * 2)
            ++stats.changedRedPixels;
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

static GuiSmokeRasterStats gui_smoke_analyze_raster_frame(
    TXshSimpleLevel *level, const TFrameId &fid) {
  GuiSmokeRasterStats stats;
  if (!level) return stats;

  TRasterImageP rasterImage = (TRasterImageP)level->getFrame(fid, false);
  TRaster32P raster;
  if (rasterImage) raster = rasterImage->getRaster();
  if (!raster) return stats;

  raster->lock();
  for (int y = 0; y < raster->getLy(); ++y) {
    const TPixel32 *scanLine = raster->pixels(y);
    for (int x = 0; x < raster->getLx(); ++x) {
      const TPixel32 &pixel = scanLine[x];
      ++stats.sampleCount;
      if (pixel.m > 0) ++stats.opaquePixels;
      if (pixel.m > 0 && pixel.r > 120 && pixel.r > pixel.g * 2 &&
          pixel.r > pixel.b * 2) {
        ++stats.redPixels;
      }
    }
  }
  raster->unlock();

  return stats;
}

static bool gui_smoke_prepare_red_palette(TXshSimpleLevel *level) {
  if (!level) return false;

  TPalette *palette = level->getPalette();
  if (palette) {
    while (palette->getStyleCount() <= 1) {
      palette->addStyle(TPixel32(232, 24, 24, 255));
    }
    palette->setStyle(1, TPixel32(232, 24, 24, 255));
  }

  return palette != nullptr;
}

static QStringList gui_smoke_viewer_capture_details(
    SceneViewer *viewer, const QImage &before, const QString &capturePrefix,
    const QString &contentName, bool contentOk = true,
    int requiredOnionPixels = 0, int requiredChangedGrayPixels = 0,
    int requiredRedPixels = 1, int requiredChangedNeutralPixels = 0) {
  QStringList details;
  const QImage after = gui_smoke_grab_viewer_frame(viewer);
  const GuiSmokeImageStats stats =
      gui_smoke_analyze_viewer_frame(after, before);

  const QString statusPath =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  const QString outputDir =
      statusPath.isEmpty() ? QDir::tempPath()
                           : QFileInfo(statusPath).absolutePath();
  QDir().mkpath(outputDir);
  const QString beforePath =
      QDir(outputDir).filePath(capturePrefix + QStringLiteral("-before.png"));
  const QString afterPath =
      QDir(outputDir).filePath(capturePrefix + QStringLiteral("-after.png"));
  const bool beforeSaved = before.isNull() ? false : before.save(beforePath);
  const bool afterSaved  = after.isNull() ? false : after.save(afterPath);
  const QSize logicalSize = viewer ? viewer->size() : QSize();
  const int viewerDeviceWidth = viewer ? viewer->width() : 0;
  const int viewerDeviceHeight = viewer ? viewer->height() : 0;
  const int viewerDevPixRatio = viewer ? viewer->getDevPixRatio() : 0;
  const double widgetDevicePixelRatio =
      viewer ? viewer->devicePixelRatioF() : 0.0;
  QScreen *viewerScreen = viewer ? viewer->screen() : nullptr;
  const double screenDevicePixelRatio =
      viewerScreen ? viewerScreen->devicePixelRatio() : 0.0;
  const bool logicalToDeviceOk =
      viewer && logicalSize.width() > 0 && logicalSize.height() > 0 &&
      viewerDevPixRatio >= 1 &&
      viewerDeviceWidth == logicalSize.width() * viewerDevPixRatio &&
      viewerDeviceHeight == logicalSize.height() * viewerDevPixRatio;
  const bool framebufferSizeOk =
      viewer && after.width() == viewerDeviceWidth &&
      after.height() == viewerDeviceHeight;
  const bool highDpiOk = logicalToDeviceOk && framebufferSizeOk &&
                         widgetDevicePixelRatio >= 1.0 &&
                         screenDevicePixelRatio >= 1.0;
  const bool onionPixelsOk =
      requiredOnionPixels <= 0 ||
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

  details << QString("viewerRenderContent=%1").arg(contentName)
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
                 .arg(highDpiOk ? QStringLiteral("ok")
                                : QStringLiteral("error"))
          << QString("sampleCount=%1").arg(stats.sampleCount)
          << QString("changedPixels=%1").arg(stats.changedPixels)
          << QString("redPixels=%1").arg(stats.redPixels)
          << QString("changedRedPixels=%1").arg(stats.changedRedPixels)
          << QString("grayPixels=%1").arg(stats.grayPixels)
          << QString("changedGrayPixels=%1").arg(stats.changedGrayPixels)
          << QString("changedNeutralPixels=%1")
                 .arg(stats.changedNeutralPixels)
          << QString("onionPixels=%1").arg(stats.onionPixels)
          << QString("baselineOnionPixels=%1")
                 .arg(stats.baselineOnionPixels)
          << QString("beforeCaptureSaved=%1")
                 .arg(beforeSaved ? QStringLiteral("true")
                                  : QStringLiteral("false"))
          << QString("afterCaptureSaved=%1")
                 .arg(afterSaved ? QStringLiteral("true")
                                 : QStringLiteral("false"))
          << QString("beforeCapturePath=%1")
                 .arg(gui_smoke_status_value(beforePath))
          << QString("afterCapturePath=%1")
                 .arg(gui_smoke_status_value(afterPath))
          << QString("viewerRenderProbe=%1")
                 .arg(ok ? QStringLiteral("ok") : QStringLiteral("error"));

  return details;
}

static bool gui_smoke_add_vector_probe_frame(TXshSimpleLevel *level,
                                             const TFrameId &fid) {
  if (!gui_smoke_prepare_red_palette(level)) return false;

  TVectorImageP image = new TVectorImage();
  image->setPalette(level->getPalette());

  std::vector<TThickPoint> points;
  points.push_back(TThickPoint(-180.0, -110.0, 20.0));
  points.push_back(TThickPoint(0.0, 120.0, 28.0));
  points.push_back(TThickPoint(180.0, -110.0, 20.0));

  TStroke *stroke = new TStroke(points);
  stroke->setStyle(1);
  image->addStroke(stroke);
  image->validateRegions(true);

  level->setFrame(fid, image);
  level->setDirtyFlag(true);
  return true;
}

static void gui_smoke_add_raster_probe_frame(TXshSimpleLevel *level,
                                             const TFrameId &fid) {
  TRaster32P raster(256, 256);
  raster->fill(TPixel32(232, 24, 24, 255));
  level->setFrame(fid, TRasterImageP(raster));
  level->setDirtyFlag(true);
}

static void gui_smoke_add_transparent_raster_probe_frame(
    TXshSimpleLevel *level, const TFrameId &fid, const TPixel32 &color,
    int x0, int y0, int x1, int y1) {
  TRaster32P raster(256, 256);
  raster->clear();
  raster->lock();
  const int left   = std::max(0, std::min(x0, x1));
  const int right  = std::min(raster->getLx(), std::max(x0, x1));
  const int top    = std::max(0, std::min(y0, y1));
  const int bottom = std::min(raster->getLy(), std::max(y0, y1));
  for (int y = top; y < bottom; ++y) {
    TPixel32 *scanLine = raster->pixels(y);
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

  GuiSmokeStageOnionCounts(const ImagePainter::VisualSettings &visualSettings)
      : Stage::Visitor(visualSettings) {}

  void onImage(const Stage::Player &player) override {
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

  void onRasterImage(TRasterImage *, const Stage::Player &) override {}
  void enableMask() override {}
  void disableMask() override {}
  void beginMask() override {}
  void endMask() override {}
};

static QStringList gui_smoke_viewer_render_details(
    const QString &requestedSceneName, bool useVectorLevel) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  viewer->resetSceneViewer();
  gui_smoke_pump_events();
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const int levelType = useVectorLevel ? PLI_XSHLEVEL : OVL_XSHLEVEL;
  const std::wstring levelName =
      useVectorLevel ? L"qt6_gui_viewer_render_vector"
                     : L"qt6_gui_viewer_render_raster";
  TXshLevel *levelXl = scene->createNewLevel(levelType, levelName);
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
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
  const QString capturePrefix =
      useVectorLevel ? QStringLiteral("viewer-vector-render")
                     : QStringLiteral("viewer-render");

  details << QString("scene=%1").arg(scenePath.getQString());
  details += gui_smoke_viewer_capture_details(
      viewer, before, capturePrefix,
      useVectorLevel ? QStringLiteral("vector") : QStringLiteral("raster"));

  return details;
}

static QStringList gui_smoke_viewer_zoom_pan_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_zoom_pan_raster");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("viewerTransformProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet *xsheet = scene->getXsheet();
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
  const QImage before = gui_smoke_grab_viewer_frame(viewer);
  const TAffine beforeAff = viewer->getViewMatrix();
  const double beforeScale = std::sqrt(std::abs(beforeAff.det()));
  const int devPixRatio = std::max(1, viewer->getDevPixRatio());
  const double zoomFactor = 1.35;
  const QPointF panDelta(48.0 * devPixRatio, -32.0 * devPixRatio);

  viewer->setViewMatrix(TTranslation(panDelta.x(), -panDelta.y()) *
                            TScale(zoomFactor) * beforeAff,
                        SceneViewer::SCENE_VIEWMODE);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const TAffine afterAff = viewer->getViewMatrix();
  const double afterScale = std::sqrt(std::abs(afterAff.det()));
  const bool transformOk =
      afterAff != beforeAff && afterScale > beforeScale &&
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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
  TOnionSkinMaskHandle *onionHandle =
      TApp::instance()->getCurrentOnionSkin();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *onionLevelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_back");
  TXshLevel *currentLevelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_onion_current");
  TXshSimpleLevel *onionLevel =
      onionLevelXl ? onionLevelXl->getSimpleLevel() : nullptr;
  TXshSimpleLevel *currentLevel =
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
      currentLevel, currentFid, TPixel32(232, 24, 24, 255), 134, 134, 222,
      222);

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(onionLevel, onionFid)) ||
      !xsheet->setCell(1, 0, TXshCell(currentLevel, currentFid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("onionSkinProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(1);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::ColumnId(0));
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
  const TXshCell onionCell  = xsheet->getCell(0, 0);
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

  details << QString("scene=%1").arg(scenePath.getQString())
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
          << QString("stageCurrentPlayerCount=%1")
                 .arg(stageCounts.m_currentPlayers)
          << QString("stageCurrentColumnPlayerCount=%1")
                 .arg(stageCounts.m_currentColumnCount)
          << QString("stageOnionPlayerCount=%1")
                 .arg(stageCounts.m_onionPlayers)
          << QString("stageBackOnionPlayerCount=%1")
                 .arg(stageCounts.m_backOnionPlayers)
          << QString("stageFrontOnionPlayerCount=%1")
                 .arg(stageCounts.m_frontOnionPlayers)
          << QString("stageOnionRows=%1").arg(stageCounts.m_rows.join(','))
          << QString("onionSkinProbe=%1")
                 .arg(onionOk ? QStringLiteral("ok")
                              : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-onion-skin"),
      QStringLiteral("raster-onion-skin"), onionOk, 1);

  return details;
}

static QStringList gui_smoke_viewer_camera_overlay_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  const bool safeAreaEnabled = gui_smoke_set_safe_area_overlay(true);
  const bool fieldGuideEnabled = gui_smoke_set_field_guide_overlay(true);
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const bool safeAreaOk = safeAreaDisabled && safeAreaEnabled;
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

static int gui_smoke_visible_ruler_count() {
  QMainWindow *mainWindow = TApp::instance()->getMainWindow();
  if (!mainWindow) return 0;

  int visibleCount = 0;
  const QList<Ruler *> rulers = mainWindow->findChildren<Ruler *>();
  for (Ruler *ruler : rulers) {
    if (ruler && ruler->isVisible()) ++visibleCount;
  }
  return visibleCount;
}

static int gui_smoke_ruler_count() {
  QMainWindow *mainWindow = TApp::instance()->getMainWindow();
  return mainWindow ? mainWindow->findChildren<Ruler *>().size() : 0;
}

static QStringList gui_smoke_viewer_ruler_guide_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::NoneId);

  TSceneProperties *properties = scene->getProperties();
  properties->getHGuides().clear();
  properties->getVGuides().clear();
  properties->getHGuides().push_back(0.0);
  properties->getVGuides().push_back(0.0);

  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();

  const bool cameraDisabled = gui_smoke_set_view_camera_overlay(false);
  const bool safeAreaDisabled = gui_smoke_set_safe_area_overlay(false);
  const bool fieldGuideDisabled = gui_smoke_set_field_guide_overlay(false);
  const bool guideDisabled = gui_smoke_set_view_guide_overlay(false);
  const bool rulerDisabled = gui_smoke_set_view_ruler_overlay(false);
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

  const int rulerCount = gui_smoke_ruler_count();
  const int visibleRulerCount = gui_smoke_visible_ruler_count();
  const int hGuideCount = properties->getHGuides().size();
  const int vGuideCount = properties->getVGuides().size();
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
                 .arg(guideOk ? QStringLiteral("ok")
                              : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-ruler-guide"),
      QStringLiteral("ruler-guide"), guideOk, 0, 0, 0, 1);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_overlay_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolOverlayProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet *xsheet = scene->getXsheet();
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolOverlayProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Hand);
  TTool *baselineTool = toolHandle->getTool();
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
  TTool *tool = toolHandle->getTool();
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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl =
      scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_drag");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject *stageObject     = xsheet->getStageObject(objectId);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool *tool = toolHandle->getTool();
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

  const double beforeX = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY = stageObject->getParam(TStageObject::T_Y, 0);
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

  const double afterX = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement = xsheet->getPlacement(objectId, 0);
  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk =
      objectOk && beforePlacement != afterPlacement &&
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
  return {TPointD(-190.0, -90.0), TPointD(-110.0, 55.0),
          TPointD(0.0, 130.0), TPointD(110.0, 55.0),
          TPointD(190.0, -90.0)};
}

static void gui_smoke_replay_tool_stroke(ToolHandle *toolHandle, TTool *tool) {
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

static QPointF gui_smoke_world_to_viewer_local(SceneViewer *viewer,
                                               const TPointD &worldPos) {
  if (!viewer) return QPointF();

  const TPointD viewPos = viewer->getViewMatrix() * worldPos;
  const double devPixRatio = std::max(1, viewer->getDevPixRatio());
  return QPointF((viewer->width() / 2.0 + viewPos.x) / devPixRatio,
                 (viewer->height() / 2.0 - viewPos.y) / devPixRatio);
}

static QString gui_smoke_joined_screen_points(
    const std::vector<QPointF> &points) {
  QStringList values;
  for (const QPointF &point : points) {
    values << QString("%1,%2")
                  .arg(std::round(point.x()))
                  .arg(std::round(point.y()));
  }
  return values.join('|');
}

static std::vector<QPointF> gui_smoke_tool_screen_points(SceneViewer *viewer) {
  std::vector<QPointF> globalPoints;
  if (!viewer) return globalPoints;

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  globalPoints.reserve(strokePoints.size());
  for (const TPointD &point : strokePoints) {
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

static bool gui_smoke_send_viewer_mouse_event(SceneViewer *viewer,
                                              QEvent::Type type,
                                              const QPointF &localPos,
                                              Qt::MouseButton button,
                                              Qt::MouseButtons buttons,
                                              Qt::KeyboardModifiers modifiers =
                                                  Qt::NoModifier) {
  if (!viewer) return false;

  const QPointF globalPos = viewer->mapToGlobal(localPos.toPoint());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  QMouseEvent event(type, localPos, localPos, globalPos, button, buttons,
                    modifiers);
#else
  QMouseEvent event(type, localPos, globalPos, button, buttons, modifiers);
#endif
  const bool delivered = QApplication::sendEvent(viewer, &event);
  gui_smoke_pump_events(25);
  return delivered;
}

static bool gui_smoke_replay_viewer_mouse_stroke(SceneViewer *viewer) {
  if (!viewer) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  std::vector<QPointF> localPoints;
  localPoints.reserve(strokePoints.size());
  for (const TPointD &point : strokePoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseButtonPress, localPoints.front(), Qt::LeftButton,
      Qt::LeftButton);
  for (size_t i = 1; i + 1 < localPoints.size(); ++i) {
    delivered = gui_smoke_send_viewer_mouse_event(
                    viewer, QEvent::MouseMove, localPoints[i],
                    Qt::NoButton, Qt::LeftButton) &&
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
    SceneViewer *viewer, const std::vector<TPointD> &worldPoints,
    std::vector<QPointF> *localPointsOut = nullptr,
    Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  if (!viewer || worldPoints.size() < 2) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  std::vector<QPointF> localPoints;
  localPoints.reserve(worldPoints.size());
  for (const TPointD &point : worldPoints) {
    const QPointF localPoint = gui_smoke_world_to_viewer_local(viewer, point);
    if (!viewer->rect().contains(localPoint.toPoint())) return false;
    localPoints.push_back(localPoint);
  }

  bool delivered = gui_smoke_send_viewer_mouse_event(
      viewer, QEvent::MouseButtonPress, localPoints.front(), Qt::LeftButton,
      Qt::LeftButton, modifiers);
  for (size_t i = 1; i + 1 < localPoints.size(); ++i) {
    delivered = gui_smoke_send_viewer_mouse_event(
                    viewer, QEvent::MouseMove, localPoints[i],
                    Qt::NoButton, Qt::LeftButton, modifiers) &&
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

static QString gui_smoke_joined_local_points(
    const std::vector<QPointF> &points) {
  QStringList values;
  for (const QPointF &point : points) {
    values << QString("%1,%2").arg(point.x(), 0, 'f', 1).arg(point.y(), 0, 'f',
                                                             1);
  }
  return values.join('|');
}

static bool gui_smoke_near(double a, double b, double epsilon = 0.0001) {
  return std::abs(a - b) <= epsilon;
}

static void gui_smoke_reset_stage_object_position(TStageObject *stageObject,
                                                  double frame, double x,
                                                  double y) {
  if (!stageObject) return;
  stageObject->getParam(TStageObject::T_X)->setValue(frame, x);
  stageObject->getParam(TStageObject::T_Y)->setValue(frame, y);
}

static TEnumProperty *gui_smoke_find_tool_enum_property(
    TTool *tool, const std::string &propertyName, const std::string &propertyId) {
  if (!tool) return nullptr;
  TPropertyGroup *properties = tool->getProperties(tool->getTargetType());
  if (!properties) return nullptr;

  if (TProperty *property = properties->getProperty(propertyName)) {
    if (TEnumProperty *enumProperty =
            dynamic_cast<TEnumProperty *>(property)) {
      return enumProperty;
    }
  }

  for (int i = 0; i < properties->getPropertyCount(); ++i) {
    TProperty *property = properties->getProperty(i);
    if (!property) continue;
    if (property->getName() != propertyName && property->getId() != propertyId)
      continue;
    if (TEnumProperty *enumProperty =
            dynamic_cast<TEnumProperty *>(property)) {
      return enumProperty;
    }
  }
  return nullptr;
}

static bool gui_smoke_set_tool_enum_property(
    TTool *tool, const std::string &propertyName, const std::string &propertyId,
    const std::wstring &value, QString *actualValueOut = nullptr) {
  TEnumProperty *property =
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

struct GuiSmokeStageObjectBaseline {
  double angle = 0.0;
  double x = 0.0;
  double y = 0.0;
  double scaleX = 0.0;
  double scaleY = 0.0;
  double scale = 0.0;
  double shearX = 0.0;
  double shearY = 0.0;
  TPointD center;
};

static GuiSmokeStageObjectBaseline gui_smoke_capture_stage_object_baseline(
    TXsheet *xsheet, const TStageObjectId &objectId, TStageObject *stageObject,
    int frame) {
  GuiSmokeStageObjectBaseline baseline;
  if (!xsheet || !stageObject) return baseline;

  baseline.angle =
      stageObject->getParam(TStageObject::T_Angle, frame);
  baseline.x = stageObject->getParam(TStageObject::T_X, frame);
  baseline.y = stageObject->getParam(TStageObject::T_Y, frame);
  baseline.scaleX =
      stageObject->getParam(TStageObject::T_ScaleX, frame);
  baseline.scaleY =
      stageObject->getParam(TStageObject::T_ScaleY, frame);
  baseline.scale =
      stageObject->getParam(TStageObject::T_Scale, frame);
  baseline.shearX =
      stageObject->getParam(TStageObject::T_ShearX, frame);
  baseline.shearY =
      stageObject->getParam(TStageObject::T_ShearY, frame);
  baseline.center = xsheet->getCenter(objectId, frame);
  return baseline;
}

static void gui_smoke_restore_stage_object_baseline(
    TXsheet *xsheet, const TStageObjectId &objectId, TStageObject *stageObject,
    int frame, const GuiSmokeStageObjectBaseline &baseline) {
  if (!xsheet || !stageObject) return;

  stageObject->getParam(TStageObject::T_Angle)->setValue(frame, baseline.angle);
  stageObject->getParam(TStageObject::T_X)->setValue(frame, baseline.x);
  stageObject->getParam(TStageObject::T_Y)->setValue(frame, baseline.y);
  stageObject->getParam(TStageObject::T_ScaleX)->setValue(frame,
                                                          baseline.scaleX);
  stageObject->getParam(TStageObject::T_ScaleY)->setValue(frame,
                                                          baseline.scaleY);
  stageObject->getParam(TStageObject::T_Scale)->setValue(frame,
                                                         baseline.scale);
  stageObject->getParam(TStageObject::T_ShearX)->setValue(frame,
                                                          baseline.shearX);
  stageObject->getParam(TStageObject::T_ShearY)->setValue(frame,
                                                          baseline.shearY);
  xsheet->setCenter(objectId, frame, baseline.center);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

static QStringList gui_smoke_viewer_animate_tool_mouse_event_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_mouse_events");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
  if (!simpleLevel) {
    details << QStringLiteral("viewerRenderProbe=level-error")
            << QStringLiteral("toolTransformProbe=level-error")
            << QStringLiteral("mouseEventProbe=level-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TFrameId fid(1);
  gui_smoke_add_raster_probe_frame(simpleLevel, fid);

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject *stageObject     = xsheet->getStageObject(objectId);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolTransformProbe=tool-handle-error")
            << QStringLiteral("mouseEventProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Edit);
  TTool *tool = toolHandle->getTool();
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

  const double beforeX = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine beforePlacement = xsheet->getPlacement(objectId, 0);

  const std::vector<TPointD> dragPoints = {TPointD(0.0, 0.0),
                                           TPointD(72.0, -36.0),
                                           TPointD(144.0, -72.0)};
  std::vector<QPointF> localDragPoints;
  const bool mouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, dragPoints, &localDragPoints);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const double afterX = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement = xsheet->getPlacement(objectId, 0);
  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk =
      mouseEventsDelivered && objectOk && beforePlacement != afterPlacement &&
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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_undo_redo");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolTransformProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QStringLiteral("undoRedoProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject *stageObject     = xsheet->getStageObject(objectId);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
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
  TTool *tool = toolHandle->getTool();
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

  const double beforeX = stageObject->getParam(TStageObject::T_X, 0);
  const double beforeY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine beforePlacement = xsheet->getPlacement(objectId, 0);

  TUndoManager *undoManager = TUndoManager::manager();
  undoManager->reset();
  const int historyCountBefore = undoManager->getHistoryCount();
  const int historyIndexBefore = undoManager->getCurrentHistoryIndex();

  const std::vector<TPointD> dragPoints = {TPointD(0.0, 0.0),
                                           TPointD(72.0, -36.0),
                                           TPointD(144.0, -72.0)};
  std::vector<QPointF> localDragPoints;
  const bool mouseEventsDelivered = gui_smoke_replay_viewer_mouse_drag(
      viewer, dragPoints, &localDragPoints);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);

  const double afterX = stageObject->getParam(TStageObject::T_X, 0);
  const double afterY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine afterPlacement = xsheet->getPlacement(objectId, 0);
  const int historyCountAfterDrag = undoManager->getHistoryCount();
  const int historyIndexAfterDrag = undoManager->getCurrentHistoryIndex();

  const bool undoResult = undoManager->undo();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(80);
  const double undoX = stageObject->getParam(TStageObject::T_X, 0);
  const double undoY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine undoPlacement = xsheet->getPlacement(objectId, 0);
  const int historyIndexAfterUndo = undoManager->getCurrentHistoryIndex();

  const bool redoResult = undoManager->redo();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(120);
  const double redoX = stageObject->getParam(TStageObject::T_X, 0);
  const double redoY = stageObject->getParam(TStageObject::T_Y, 0);
  const TAffine redoPlacement = xsheet->getPlacement(objectId, 0);
  const int historyIndexAfterRedo = undoManager->getCurrentHistoryIndex();

  const bool objectOk =
      TApp::instance()->getCurrentObject()->getObjectId() == objectId;
  const bool transformOk =
      mouseEventsDelivered && objectOk && beforePlacement != afterPlacement &&
      (std::abs(afterX - beforeX) > 0.0001 ||
       std::abs(afterY - beforeY) > 0.0001);
  const bool undoRestored =
      gui_smoke_near(undoX, beforeX) && gui_smoke_near(undoY, beforeY) &&
      undoPlacement == beforePlacement;
  const bool redoRestored =
      gui_smoke_near(redoX, afterX) && gui_smoke_near(redoY, afterY) &&
      redoPlacement == afterPlacement;
  const bool undoRedoOk =
      transformOk && historyCountBefore == 0 && historyIndexBefore == 0 &&
      historyCountAfterDrag > historyCountBefore &&
      historyIndexAfterDrag == historyCountAfterDrag && undoResult &&
      historyIndexAfterUndo < historyIndexAfterDrag && undoRestored &&
      redoResult && historyIndexAfterRedo == historyIndexAfterDrag &&
      redoRestored;

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
          << QString("stageObjectXUndo=%1").arg(undoX, 0, 'f', 4)
          << QString("stageObjectYUndo=%1").arg(undoY, 0, 'f', 4)
          << QString("stageObjectXRedo=%1").arg(redoX, 0, 'f', 4)
          << QString("stageObjectYRedo=%1").arg(redoY, 0, 'f', 4)
          << QString("undoResult=%1")
                 .arg(undoResult ? QStringLiteral("true")
                                 : QStringLiteral("false"))
          << QString("redoResult=%1")
                 .arg(redoResult ? QStringLiteral("true")
                                 : QStringLiteral("false"))
          << QString("undoRestored=%1")
                 .arg(undoRestored ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("redoRestored=%1")
                 .arg(redoRestored ? QStringLiteral("true")
                                   : QStringLiteral("false"))
          << QString("undoHistoryCountBefore=%1").arg(historyCountBefore)
          << QString("undoHistoryCountAfterDrag=%1").arg(historyCountAfterDrag)
          << QString("undoHistoryIndexBefore=%1").arg(historyIndexBefore)
          << QString("undoHistoryIndexAfterDrag=%1")
                 .arg(historyIndexAfterDrag)
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
                 .arg(transformOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
          << QString("undoRedoProbe=%1")
                 .arg(undoRedoOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"));
  details += gui_smoke_viewer_capture_details(
      viewer, before, QStringLiteral("viewer-animate-tool-undo-redo"),
      QStringLiteral("animate-tool-undo-redo"), undoRedoOk);

  return details;
}

static QStringList gui_smoke_viewer_animate_tool_modifiers_details(
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_modifiers");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
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
  TStageObject *stageObject     = xsheet->getStageObject(objectId);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
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
  TTool *tool = toolHandle->getTool();
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
  const TAffine beforePlacement = xsheet->getPlacement(objectId, frame);
  const std::vector<TPointD> dragPoints = {TPointD(0.0, 0.0),
                                           TPointD(72.0, -36.0),
                                           TPointD(144.0, -72.0)};

  bool cursorEventsDelivered = false;
  int normalCursor           = tool->getCursorId();
  int altCursor              = normalCursor;
  const QPointF cursorLocalPoint =
      gui_smoke_world_to_viewer_local(viewer, dragPoints.front());
  if (viewer->rect().contains(cursorLocalPoint.toPoint())) {
    cursorEventsDelivered =
        gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                          cursorLocalPoint, Qt::NoButton,
                                          Qt::NoButton, Qt::NoModifier);
    normalCursor = tool->getCursorId();
    cursorEventsDelivered =
        gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                          cursorLocalPoint, Qt::NoButton,
                                          Qt::NoButton, Qt::AltModifier) &&
        cursorEventsDelivered;
    altCursor = tool->getCursorId();
    cursorEventsDelivered =
        gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                          cursorLocalPoint, Qt::NoButton,
                                          Qt::NoButton, Qt::NoModifier) &&
        cursorEventsDelivered;
  }
  const bool cursorPreciseBefore =
      (normalCursor & ToolCursor::Ex_Precise) != 0;
  const bool cursorPreciseAfter = (altCursor & ToolCursor::Ex_Precise) != 0;
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
  const double altX = stageObject->getParam(TStageObject::T_X, frame);
  const double altY = stageObject->getParam(TStageObject::T_Y, frame);
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
  const bool normalTransformOk =
      normalMouseEventsDelivered && objectOk &&
      beforePlacement != normalPlacement && std::abs(normalDeltaX) > 0.0001 &&
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
  const bool modifierOk =
      normalTransformOk && altPrecisionOk && shiftConstraintOk &&
      cursorPreciseOk;

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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  TXshLevel *levelXl = scene->createNewLevel(
      OVL_XSHLEVEL, L"qt6_gui_viewer_animate_tool_handles");
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("handleHitTestProbe=xsheet-error")
            << QStringLiteral("handleHoverProbe=xsheet-error")
            << QStringLiteral("mouseEventProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  const TStageObjectId objectId = TStageObjectId::ColumnId(0);
  TStageObject *stageObject     = xsheet->getStageObject(objectId);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
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
  TTool *tool = toolHandle->getTool();
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
  const double handleUnit =
      tool->getPixelSize() *
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
    hoverDelivered =
        gui_smoke_send_viewer_mouse_event(viewer, QEvent::MouseMove,
                                          rotationLocalPoint, Qt::NoButton,
                                          Qt::NoButton, Qt::NoModifier);
    hoverFrame = gui_smoke_grab_viewer_frame(viewer);
    hoverStats = gui_smoke_analyze_viewer_frame(hoverFrame, before);

    const QString statusPath =
        qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
    const QString outputDir =
        statusPath.isEmpty() ? QDir::tempPath()
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
  const double scaleAfter =
      stageObject->getParam(TStageObject::T_Scale, frame);
  const bool scaleHitOk = scaleMouseEventsDelivered &&
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
  const bool centerHitOk =
      centerMouseEventsDelivered && norm(centerAfter - baseline.center) > 0.0001;

  const bool mouseEventsDelivered = hoverDelivered &&
                                    rotationMouseEventsDelivered &&
                                    scaleMouseEventsDelivered &&
                                    centerMouseEventsDelivered;
  const bool handleHitTestOk =
      activeAxisOk && hoverOk && rotationHitOk && scaleHitOk && centerHitOk;

  details << QString("scene=%1").arg(scenePath.getQString())
          << QStringLiteral("toolInputPath=qt-mouse-events")
          << QString("toolName=%1").arg(QString::fromStdString(tool->getName()))
          << QString("toolTargetType=%1").arg(tool->getTargetType())
          << QString("toolEnabled=%1")
                 .arg(tool->isEnabled() ? QStringLiteral("true")
                                        : QStringLiteral("false"))
          << QString("toolDisabledReason=%1")
                 .arg(gui_smoke_status_value(disabledReason))
          << QString("activeAxis=%1")
                 .arg(gui_smoke_status_value(activeAxisValue))
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
                 .arg(activeAxisOk ? QStringLiteral("ok")
                                   : QStringLiteral("error"))
          << QString("rotationHandleProbe=%1")
                 .arg(rotationHitOk ? QStringLiteral("ok")
                                    : QStringLiteral("error"))
          << QString("scaleHandleProbe=%1")
                 .arg(scaleHitOk ? QStringLiteral("ok")
                                 : QStringLiteral("error"))
          << QString("centerHandleProbe=%1")
                 .arg(centerHitOk ? QStringLiteral("ok")
                                  : QStringLiteral("error"))
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

static bool gui_smoke_send_viewer_tablet_event(SceneViewer *viewer,
                                               QEvent::Type type,
                                               const QPointF &localPos,
                                               double pressure, float xTilt,
                                               float yTilt,
                                               Qt::MouseButton button,
                                               Qt::MouseButtons buttons) {
  if (!viewer) return false;

  const QPointF globalPos = viewer->mapToGlobal(localPos.toPoint());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  static const QPointingDevice *smokeTabletDevice = [] {
    QInputDevice::Capabilities capabilities =
        QInputDevice::Capability::Position | QInputDevice::Capability::Pressure |
        QInputDevice::Capability::XTilt | QInputDevice::Capability::YTilt;
    return new QPointingDevice(
        QStringLiteral("OpenToonz GUI smoke stylus"), 0x746e7a36,
        QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
        capabilities, 1, 1);
  }();
  QTabletEvent event(type, smokeTabletDevice, localPos, globalPos, pressure,
                     xTilt, yTilt, 0.0f, 0.0, 0.0f, Qt::NoModifier, button,
                     buttons);
#else
  QTabletEvent event(type, localPos, globalPos, QTabletEvent::Stylus,
                     QTabletEvent::Pen, pressure, xTilt, yTilt, 0.0, 0.0, 0,
                     Qt::NoModifier, 0x746e7a36, button, buttons);
#endif
  const bool delivered = QApplication::sendEvent(viewer, &event);
  gui_smoke_pump_events(30);
  return delivered;
}

static bool gui_smoke_replay_viewer_tablet_stroke(SceneViewer *viewer) {
  if (!viewer) return false;

  viewer->setFocus(Qt::OtherFocusReason);
  gui_smoke_pump_events();

  const std::vector<TPointD> strokePoints = gui_smoke_tool_stroke_points();
  std::vector<QPointF> localPoints;
  localPoints.reserve(strokePoints.size());
  for (const TPointD &point : strokePoints) {
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
    const QString &requestedSceneName) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  const std::wstring levelName = L"qt6_gui_viewer_vector_brush";
  TXshLevel *levelXl = scene->createNewLevel(PLI_XSHLEVEL, levelName);
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolInputProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getPaletteController()->getCurrentLevelPalette()->setPalette(
      simpleLevel->getPalette(), 1);
  TApp::instance()->setCurrentLevelStyleIndex(1, true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  viewer->resetSceneViewer();
  viewer->fitToCamera();
  viewer->GLInvalidateAll();
  gui_smoke_pump_events(100);
  const QImage before = gui_smoke_grab_viewer_frame(viewer);

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolInputProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  toolHandle->onImageChanged(TImage::VECTOR);
  toolHandle->setTool(T_Brush);
  TTool *tool = toolHandle->getTool();
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
  const int strokesBefore = beforeImage ? beforeImage->getStrokeCount() : -1;

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
  const int strokesAfter = afterImage ? afterImage->getStrokeCount() : -1;
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
    const QString &requestedSceneName,
    GuiSmokeRasterBrushInput input = GuiSmokeRasterBrushInput::DirectTool,
    const QString &action = QString()) {
  QStringList details;
  SceneViewer *viewer = gui_smoke_resolve_scene_viewer();
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
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->setScenePath(scenePath);
  TApp::instance()->getCurrentScene()->notifySceneSwitched();
  TApp::instance()->getCurrentScene()->notifyNameSceneChange();

  const std::wstring levelName = L"qt6_gui_viewer_raster_brush";
  TXshLevel *levelXl = scene->createNewLevel(OVL_XSHLEVEL, levelName);
  TXshSimpleLevel *simpleLevel = levelXl ? levelXl->getSimpleLevel() : nullptr;
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

  TXsheet *xsheet = scene->getXsheet();
  if (!xsheet->setCell(0, 0, TXshCell(simpleLevel, fid))) {
    details << QStringLiteral("viewerRenderProbe=xsheet-error")
            << QStringLiteral("toolInputProbe=xsheet-error")
            << QStringLiteral("rasterInputProbe=xsheet-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }

  TApp::instance()->getCurrentFrame()->setFrame(0);
  TApp::instance()->getCurrentColumn()->setColumnIndex(0);
  TApp::instance()->getCurrentObject()->setObjectId(TStageObjectId::ColumnId(0));
  TApp::instance()->getCurrentLevel()->setLevel(levelXl);
  TApp::instance()->getCurrentPalette()->setPalette(simpleLevel->getPalette(),
                                                    1);
  TApp::instance()->getPaletteController()->getCurrentLevelPalette()->setPalette(
      simpleLevel->getPalette(), 1);
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

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (!toolHandle) {
    details << QStringLiteral("viewerRenderProbe=tool-handle-error")
            << QStringLiteral("toolInputProbe=tool-handle-error")
            << QStringLiteral("rasterInputProbe=tool-handle-error")
            << QString("scene=%1").arg(scenePath.getQString());
    return details;
  }
  toolHandle->onImageChanged(TImage::RASTER);
  toolHandle->setTool(T_Brush);
  TTool *tool = toolHandle->getTool();
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

  const bool useDirectTool = input == GuiSmokeRasterBrushInput::DirectTool;
  const bool useMouseEvents = input == GuiSmokeRasterBrushInput::MouseEvents;
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
              << QString("rasterPixelsBefore=%1")
                     .arg(rasterBefore.opaquePixels)
              << QString("rasterRedPixelsBefore=%1")
                     .arg(rasterBefore.redPixels)
              << QString("systemMousePointCount=%1").arg(globalPoints.size())
              << QString("systemMousePoints=%1")
                     .arg(gui_smoke_joined_screen_points(globalPoints))
              << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle()));

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
  const bool rasterOk =
      mouseEventsDelivered && tabletEventsDelivered &&
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

  for (const QAudioDevice &device : audioInputs) {
    audioInputNames << device.description();
  }
  for (const QAudioDevice &device : audioOutputs) {
    audioOutputNames << device.description();
  }
  for (const QCameraDevice &device : videoInputs) {
    videoInputNames << device.description();
  }

  const QAudioDevice defaultAudioInput  = QMediaDevices::defaultAudioInput();
  const QAudioDevice defaultAudioOutput = QMediaDevices::defaultAudioOutput();
  const QCameraDevice defaultVideoInput = QMediaDevices::defaultVideoInput();

  details << QStringLiteral("qtMultimediaApi=Qt6")
          << QString("audioInputCount=%1").arg(audioInputs.size())
          << QString("audioOutputCount=%1").arg(audioOutputs.size())
          << QString("videoInputCount=%1").arg(videoInputs.size())
          << QString("defaultAudioInput=%1")
                 .arg(defaultAudioInput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultAudioInput.description()))
          << QString("defaultAudioOutput=%1")
                 .arg(defaultAudioOutput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultAudioOutput.description()))
          << QString("defaultVideoInput=%1")
                 .arg(defaultVideoInput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultVideoInput.description()))
          << QString("audioInputs=%1").arg(gui_smoke_joined_values(audioInputNames))
          << QString("audioOutputs=%1")
                 .arg(gui_smoke_joined_values(audioOutputNames))
          << QString("videoInputs=%1").arg(gui_smoke_joined_values(videoInputNames));
#else
  const QList<QAudioDeviceInfo> audioInputs =
      QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
  const QList<QAudioDeviceInfo> audioOutputs =
      QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
  const QList<QCameraInfo> videoInputs = QCameraInfo::availableCameras();
  QStringList audioInputNames;
  QStringList audioOutputNames;
  QStringList videoInputNames;

  for (const QAudioDeviceInfo &device : audioInputs) {
    audioInputNames << device.deviceName();
  }
  for (const QAudioDeviceInfo &device : audioOutputs) {
    audioOutputNames << device.deviceName();
  }
  for (const QCameraInfo &device : videoInputs) {
    videoInputNames << device.description();
  }

  const QAudioDeviceInfo defaultAudioInput =
      QAudioDeviceInfo::defaultInputDevice();
  const QAudioDeviceInfo defaultAudioOutput =
      QAudioDeviceInfo::defaultOutputDevice();
  const QCameraInfo defaultVideoInput = QCameraInfo::defaultCamera();

  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QString("audioInputCount=%1").arg(audioInputs.size())
          << QString("audioOutputCount=%1").arg(audioOutputs.size())
          << QString("videoInputCount=%1").arg(videoInputs.size())
          << QString("defaultAudioInput=%1")
                 .arg(defaultAudioInput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultAudioInput.deviceName()))
          << QString("defaultAudioOutput=%1")
                 .arg(defaultAudioOutput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultAudioOutput.deviceName()))
          << QString("defaultVideoInput=%1")
                 .arg(defaultVideoInput.isNull()
                          ? QString()
                          : gui_smoke_status_value(
                                defaultVideoInput.description()))
          << QString("audioInputs=%1").arg(gui_smoke_joined_values(audioInputNames))
          << QString("audioOutputs=%1")
                 .arg(gui_smoke_joined_values(audioOutputNames))
          << QString("videoInputs=%1").arg(gui_smoke_joined_values(videoInputNames));
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
  const int sampleRate     = format.sampleRate();
  const int bytesPerFrame  = format.bytesPerFrame();
  const int durationMs     = 250;
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
  buffer.open(QIODevice::ReadOnly);

  QAudioSink sink(output, format);
  sink.setVolume(0.0);
  sink.start(&buffer);

  QEventLoop loop;
  QTimer::singleShot(durationMs + 100, &loop, &QEventLoop::quit);
  loop.exec();

  const QAudio::Error error = sink.error();
  details << QString("audioState=%1").arg(gui_smoke_audio_state_name(sink.state()))
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
  qint16 *pcm = reinterpret_cast<qint16 *>(samples.data());
  constexpr double pi = 3.14159265358979323846;
  const double amplitude = 32767.0 * 0.10;
  const double frequency = 440.0;
  for (int sample = 0; sample < sampleCount; ++sample) {
    pcm[sample] = static_cast<qint16>(
        amplitude * std::sin(2.0 * pi * frequency * sample / sampleRate));
  }

  const QString statusPath =
      qEnvironmentVariable("OPENTOONZ_GUI_SMOKE_STATUS_FILE");
  const QString outputDir =
      statusPath.isEmpty() ? QDir::tempPath()
                           : QFileInfo(statusPath).absolutePath();
  QDir().mkpath(outputDir);
  const QString playbackPath =
      QDir(outputDir).filePath(QStringLiteral("audio-playback-wav.wav"));
  QFile::remove(playbackPath);

  AudioWriterWAV writer(format);
  if (!writer.start(playbackPath, false)) {
    details << QString("playbackPath=%1")
                   .arg(gui_smoke_status_value(playbackPath))
            << QStringLiteral("audioPlaybackWavProbe=writer-start-failed");
    return details;
  }
  const qint64 bytesWritten = writer.write(samples.constData(), samples.size());
  const bool writerStopped = writer.stop();

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
    } catch (TSoundDeviceException &e) {
      playbackException = QString::fromStdWString(e.getMessage());
    } catch (TException &e) {
      playbackException = QString::fromStdWString(e.getMessage());
    } catch (...) {
      playbackException = QStringLiteral("unknown");
    }
  }

  const qint64 fileSize = playbackInfo.size();
  details << QString("playbackPath=%1")
                 .arg(gui_smoke_status_value(playbackPath))
          << QString("writerStopped=%1")
                 .arg(writerStopped ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("bytesWritten=%1").arg(bytesWritten)
          << QString("wavFileSize=%1").arg(fileSize)
          << QString("wavLoadOk=%1")
                 .arg(wavLoadOk ? QStringLiteral("true")
                                : QStringLiteral("false"))
          << QString("wavSampleRate=%1")
                 .arg(loadedTrack ? loadedTrack->getSampleRate() : 0)
          << QString("wavChannelCount=%1")
                 .arg(loadedTrack ? loadedTrack->getChannelCount() : 0)
          << QString("wavBitPerSample=%1")
                 .arg(loadedTrack ? loadedTrack->getBitPerSample() : 0)
          << QString("wavSampleCount=%1")
                 .arg(loadedTrack ? loadedTrack->getSampleCount() : 0)
          << QString("wavDurationMs=%1")
                 .arg(loadedTrack ? loadedTrack->getDuration() * 1000.0 : 0.0,
                      0, 'f', 2)
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
  const int sampleRate     = format.sampleRate();
  const int bytesPerFrame  = format.bytesPerFrame();
  const int durationMs     = 500;
  details << QString("sampleRate=%1").arg(sampleRate)
          << QString("channelCount=%1").arg(format.channelCount())
          << QString("bytesPerFrame=%1").arg(bytesPerFrame)
          << QString("sampleFormat=%1").arg(format.sampleFormat());

  if (!format.isValid() || sampleRate <= 0 || bytesPerFrame <= 0) {
    details << QStringLiteral("audioInputProbe=invalid-format");
    return details;
  }

  QAudioSource source(input, format);
  QIODevice *capture = source.start();
  if (!capture) {
    details << QString("audioError=%1")
                   .arg(gui_smoke_audio_error_name(source.error()))
            << QStringLiteral("audioInputProbe=start-failed");
    return details;
  }

  qint64 bytesCaptured = 0;
  QEventLoop loop;
  QObject::connect(capture, &QIODevice::readyRead, [&]() {
    bytesCaptured += capture->readAll().size();
  });
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
  const QString outputDir =
      statusPath.isEmpty() ? QDir::tempPath()
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
  const qint64 fileSize     = recordingInfo.size();
  const qint64 bytesRecorded = fileSize > 44 ? fileSize - 44 : 0;
  TSoundTrackP loadedTrack;
  const bool wavLoadOk =
      TSoundTrackReader::load(TFilePath(recordingPath), loadedTrack);

  details << QString("audioState=%1")
                 .arg(gui_smoke_audio_state_name(source.state()))
          << QString("audioError=%1").arg(gui_smoke_audio_error_name(error))
          << QString("processedUSecs=%1").arg(source.processedUSecs())
          << QString("elapsedUSecs=%1").arg(source.elapsedUSecs())
          << QString("recordingPath=%1")
                 .arg(gui_smoke_status_value(recordingPath))
          << QString("writerStopped=%1")
                 .arg(writerStopped ? QStringLiteral("true")
                                    : QStringLiteral("false"))
          << QString("wavFileSize=%1").arg(fileSize)
          << QString("bytesRecorded=%1").arg(bytesRecorded)
          << QString("wavLoadOk=%1")
                 .arg(wavLoadOk ? QStringLiteral("true")
                                : QStringLiteral("false"))
          << QString("wavSampleRate=%1")
                 .arg(loadedTrack ? loadedTrack->getSampleRate() : 0)
          << QString("wavChannelCount=%1")
                 .arg(loadedTrack ? loadedTrack->getChannelCount() : 0)
          << QString("wavBitPerSample=%1")
                 .arg(loadedTrack ? loadedTrack->getBitPerSample() : 0)
          << QString("wavSampleCount=%1")
                 .arg(loadedTrack ? loadedTrack->getSampleCount() : 0)
          << QString("wavDurationMs=%1")
                 .arg(loadedTrack ? loadedTrack->getDuration() * 1000.0 : 0.0,
                      0, 'f', 2)
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

static QStringList gui_smoke_camera_format_details() {
  QStringList details;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QList<QCameraDevice> videoInputs = QMediaDevices::videoInputs();
  const QCameraDevice defaultVideoInput = QMediaDevices::defaultVideoInput();
  const QList<QCameraFormat> formats =
      defaultVideoInput.isNull() ? QList<QCameraFormat>()
                                 : defaultVideoInput.videoFormats();
  QStringList formatValues;
  bool hasFormatSummary = false;
  int minWidth          = 0;
  int minHeight         = 0;
  int maxWidth          = 0;
  int maxHeight         = 0;
  float minFrameRate    = 0.0f;
  float maxFrameRate    = 0.0f;

  for (const QCameraFormat &format : formats) {
    const QSize resolution = format.resolution();
    const float formatMinFrameRate = format.minFrameRate();
    const float formatMaxFrameRate = format.maxFrameRate();

    formatValues << QString("%1x%2@%3-%4fps:%5")
                        .arg(resolution.width())
                        .arg(resolution.height())
                        .arg(formatMinFrameRate, 0, 'f', 2)
                        .arg(formatMaxFrameRate, 0, 'f', 2)
                        .arg(static_cast<int>(format.pixelFormat()));

    if (!hasFormatSummary) {
      minWidth          = resolution.width();
      minHeight         = resolution.height();
      maxWidth          = resolution.width();
      maxHeight         = resolution.height();
      minFrameRate      = formatMinFrameRate;
      maxFrameRate      = formatMaxFrameRate;
      hasFormatSummary  = true;
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
          << QString("defaultVideoMinFrameRate=%1")
                 .arg(minFrameRate, 0, 'f', 2)
          << QString("defaultVideoMaxFrameRate=%1")
                 .arg(maxFrameRate, 0, 'f', 2)
          << QString("defaultVideoFormats=%1")
                 .arg(gui_smoke_joined_values(formatValues))
          << QString("cameraFormatProbe=%1").arg(probe);
#else
  details << QStringLiteral("qtMultimediaApi=Qt5")
          << QStringLiteral("cameraFormatProbe=unsupported");
#endif

  return details;
}

static void run_gui_smoke_hook(const QString &action,
                               const QString &requestedSceneName,
                               const TFilePath &loadedScenePath) {
  if (action.isEmpty()) return;

  try {
    if (action == "startup") {
      write_gui_smoke_status(
          action, "ok",
          QStringList()
              << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle()));
      return;
    }

    if (action == "open-scene") {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      write_gui_smoke_status(
          action, "ok",
          QStringList() << QString("scene=%1").arg(scene->getScenePath().getQString())
                        << QString("requested=%1").arg(loadedScenePath.getQString())
                        << QString("window=%1").arg(TApp::instance()
                                                        ->getMainWindow()
                                                        ->windowTitle()));
      return;
    }

    if (action == "high-dpi") {
      QWidget *mainWindow = TApp::instance()->getMainWindow();
      QScreen *screen =
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
          QStringList()
              << QString("windowDevicePixelRatio=%1")
                     .arg(mainWindow ? mainWindow->devicePixelRatioF() : 0.0,
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
                     .arg(mainWindow ? mainWindow->windowTitle() : QString()));
      return;
    }

    if (action == "media-devices") {
      QStringList details = gui_smoke_media_device_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      write_gui_smoke_status(action, "ok", details);
      return;
    }

    if (action == "audio-input") {
      QStringList details = gui_smoke_audio_input_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("audioInputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-recording-wav") {
      QStringList details = gui_smoke_audio_recording_wav_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("audioRecordingWavProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-playback-wav") {
      QStringList details = gui_smoke_audio_playback_wav_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok =
          details.contains(QStringLiteral("audioPlaybackWavProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "camera-formats") {
      QStringList details = gui_smoke_camera_format_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("cameraFormatProbe=ok")) ||
                      details.contains(
                          QStringLiteral("cameraFormatProbe=no-camera"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "audio-output") {
      QStringList details = gui_smoke_audio_output_details();
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("audioOutputProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-render" || action == "viewer-vector-render") {
      QStringList details =
          gui_smoke_viewer_render_details(requestedSceneName,
                                          action == "viewer-vector-render");
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-zoom-pan") {
      QStringList details =
          gui_smoke_viewer_zoom_pan_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(
                          QStringLiteral("viewerTransformProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-onion-skin") {
      QStringList details =
          gui_smoke_viewer_onion_skin_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("onionSkinProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-camera-overlay") {
      QStringList details =
          gui_smoke_viewer_camera_overlay_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(
                          QStringLiteral("cameraOverlayProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-safe-area-field-guide") {
      QStringList details =
          gui_smoke_viewer_safe_area_field_guide_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("safeAreaProbe=ok")) &&
                      details.contains(QStringLiteral("fieldGuideProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-ruler-guide") {
      QStringList details =
          gui_smoke_viewer_ruler_guide_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("rulerGuideProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-overlay") {
      QStringList details =
          gui_smoke_viewer_animate_tool_overlay_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("toolOverlayProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-drag") {
      QStringList details =
          gui_smoke_viewer_animate_tool_drag_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("toolTransformProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-mouse-events") {
      QStringList details =
          gui_smoke_viewer_animate_tool_mouse_event_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(
                          QStringLiteral("toolTransformProbe=ok")) &&
                      details.contains(QStringLiteral("mouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-undo-redo") {
      QStringList details =
          gui_smoke_viewer_animate_tool_undo_redo_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(
                          QStringLiteral("toolTransformProbe=ok")) &&
                      details.contains(QStringLiteral("mouseEventProbe=ok")) &&
                      details.contains(QStringLiteral("undoRedoProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-modifiers") {
      QStringList details =
          gui_smoke_viewer_animate_tool_modifiers_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(
                          QStringLiteral("toolTransformProbe=ok")) &&
                      details.contains(QStringLiteral("mouseEventProbe=ok")) &&
                      details.contains(QStringLiteral("modifierProbe=ok")) &&
                      details.contains(
                          QStringLiteral("cursorPreciseProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-animate-tool-handles") {
      QStringList details =
          gui_smoke_viewer_animate_tool_handles_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("mouseEventProbe=ok")) &&
                      details.contains(
                          QStringLiteral("handleHoverProbe=ok")) &&
                      details.contains(
                          QStringLiteral("handleHitTestProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "viewer-vector-brush") {
      QStringList details =
          gui_smoke_viewer_vector_brush_details(requestedSceneName);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
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

      QStringList details =
          gui_smoke_viewer_raster_brush_details(requestedSceneName, input,
                                                action);
      details << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle());
      const bool ok = details.contains(QStringLiteral("viewerRenderProbe=ok")) &&
                      details.contains(QStringLiteral("toolInputProbe=ok")) &&
                      details.contains(QStringLiteral("rasterInputProbe=ok")) &&
                      details.contains(QStringLiteral("mouseEventProbe=ok")) &&
                      details.contains(QStringLiteral("tabletEventProbe=ok")) &&
                      details.contains(
                          QStringLiteral("systemMouseEventProbe=ok"));
      write_gui_smoke_status(action, ok ? "ok" : "error", details);
      return;
    }

    if (action == "create-scene") {
      QString sceneName     = gui_smoke_scene_name(requestedSceneName);
      TFilePath scenePath   = gui_smoke_scene_path(sceneName);
      if (!TSystem::touchParentDir(scenePath)) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to create scene folder")
                          << QString("scene=%1").arg(scenePath.getQString()));
        return;
      }

      IoCmd::newScene();
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      scene->setScenePath(scenePath);
      bool saved = IoCmd::saveScene(scenePath, IoCmd::SILENTLY_OVERWRITE);
      if (saved) {
        TApp::instance()->getCurrentScene()->notifySceneSwitched();
        TApp::instance()->getCurrentScene()->notifyNameSceneChange();
      }

      write_gui_smoke_status(
          action, saved ? "ok" : "error",
          QStringList() << QString("scene=%1").arg(scenePath.getQString())
                        << QString("window=%1").arg(TApp::instance()
                                                        ->getMainWindow()
                                                        ->windowTitle()));
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
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      scene->setScenePath(scenePath);

      TXshLevel *rasterXl =
          scene->createNewLevel(OVL_XSHLEVEL, L"qt6_gui_raster");
      TXshLevel *vectorXl =
          scene->createNewLevel(PLI_XSHLEVEL, L"qt6_gui_vector");
      TXshSimpleLevel *rasterLevel =
          rasterXl ? rasterXl->getSimpleLevel() : nullptr;
      TXshSimpleLevel *vectorLevel =
          vectorXl ? vectorXl->getSimpleLevel() : nullptr;
      const TFrameId fid(1);

      if (!add_gui_smoke_frame(rasterLevel, fid) ||
          !add_gui_smoke_frame(vectorLevel, fid)) {
        write_gui_smoke_status(
            action, "error",
            QStringList() << QString("message=failed to create smoke frames"));
        return;
      }

      TXsheet *xsheet = scene->getXsheet();
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
                     .arg(TApp::instance()->getCurrentColumn()->getColumnIndex())
              << QString("frameCount=%1").arg(scene->getFrameCount())
              << QString("columnCount=%1").arg(xsheet->getColumnCount())
              << QString("rasterFrames=%1").arg(rasterLevel->getFrameCount())
              << QString("vectorFrames=%1").arg(vectorLevel->getFrameCount())
              << QString("window=%1").arg(TApp::instance()
                                              ->getMainWindow()
                                              ->windowTitle()));
      return;
    }

    write_gui_smoke_status(
        action, "error",
        QStringList() << QString("message=unsupported GUI smoke action"));
  } catch (const TException &e) {
    write_gui_smoke_status(
        action, "error",
        QStringList() << QString("message=%1").arg(
            QString::fromStdWString(e.getMessage())));
  } catch (const std::exception &e) {
    write_gui_smoke_status(action, "error",
                           QStringList() << QString("message=%1").arg(e.what()));
  } catch (...) {
    write_gui_smoke_status(action, "error",
                           QStringList() << QString("message=unknown exception"));
  }
}

//-----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
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
    } catch (const TException &e) {
      std::cerr << QString::fromStdWString(e.getMessage()).toStdString()
                << std::endl;
      return 1;
    } catch (const std::exception &e) {
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
    bool nativeEventFilter(const QByteArray &eventType, void *message,
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                           long *) Q_DECL_OVERRIDE {
#else
                           qintptr *) Q_DECL_OVERRIDE {
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
  const QString scriptWorkingDirectory = QDir::currentPath();

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

// splash screen (override with local file if present)
QString exeDir = QCoreApplication::applicationDirPath();
QString localSplashPath = QDir(exeDir).filePath("splash.svg");

QPixmap splashPixmap;

if (QFileInfo(localSplashPath).exists() && QFileInfo(localSplashPath).isFile()) {
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

  splash.showMessage(offsetStr + "Initializing QGLFormat...", Qt::AlignCenter,
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

  // Setup third party
  ThirdParty::initialize();

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

#if OPENTOONZ_QT_MAJOR >= 6
  if (isRunScript) {
    QDir::setCurrent(scriptWorkingDirectory);
    return run_script(loadFilePath);
  }
#endif

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
  (void)qtTranslator.load("qt_" + QLocale::system().name(),
                          qtTranslationsPath);
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
  auto &themeManager = ThemeManager::getInstance();
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
  if (Preferences::instance()->isLatestVersionCheckEnabled())
    w.checkForUpdates();

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

  return ret;
}
