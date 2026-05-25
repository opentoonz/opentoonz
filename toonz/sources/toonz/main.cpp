

// Tnz6 includes
#include "audiorecordingpopup.h"
#include "crashhandler.h"
#include "mainwindow.h"
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
#include "toonz/studiopalette.h"
#include "toonz/stylemanager.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tproject.h"
#include "toonz/scriptengine.h"

// TnzSound includes
#include "tnzsound.h"
#include "tsound_io.h"

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
#include "trasterimage.h"
#include "tsimplecolorstyles.h"
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
  if (loadFilePath.getType() == "toonzscript") {
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

  bool isRunScript = (loadFilePath.getType() == "toonzscript");

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
  if (isRunScript) return run_script(loadFilePath);
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
