#include "thirdparty.h"

// TnzLib includes
#include "toonz/preferences.h"

// TnzCore includes
#include "tsystem.h"
#include "tmsgcore.h"

// QT includes
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

namespace ThirdParty {

//-----------------------------------------------------------------------------

void getFFmpegVideoSupported(QStringList &exts) {
  exts.append("gif");
  exts.append("mp4");
  exts.append("webm");
}

//-----------------------------------------------------------------------------

void getFFmpegAudioSupported(QStringList &exts) {
  exts.append("mp3");
  exts.append("ogg");
  exts.append("flac");
}

//-----------------------------------------------------------------------------

bool findFFmpeg(QString dir) {
  // FFmpeg executables
#ifdef _WIN32
  QString ffmpeg_exe = "/ffmpeg.exe";
  QString probe_exe  = "/ffprobe.exe";
#else
  QString ffmpeg_exe = "/ffmpeg";
  QString probe_exe  = "/ffprobe";
#endif

  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if both executables exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + ffmpeg_exe)) &&
         TSystem::doesExistFileOrLevel(TFilePath(dir + probe_exe));
}

//-----------------------------------------------------------------------------

bool checkFFmpeg() {
  // Path in preferences
  return findFFmpeg(Preferences::instance()->getFfmpegPath());
}

//-----------------------------------------------------------------------------

QString autodetectFFmpeg() {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (findFFmpeg(dir)) return dir;

  if (findFFmpeg(".")) return ".";
  if (findFFmpeg("./ffmpeg")) return "./ffmpeg";
  if (findFFmpeg("./ffmpeg/bin")) return "./ffmpeg/bin";
  if (findFFmpeg("./FFmpeg")) return "./FFmpeg";
  if (findFFmpeg("./FFmpeg/bin")) return "./FFmpeg/bin";

  return "";
}

//-----------------------------------------------------------------------------

QString getFFmpegDir() {
  return Preferences::instance()->getStringValue(ffmpegPath);
}

//-----------------------------------------------------------------------------

void setFFmpegDir(const QString &dir) {
  QString path = Preferences::instance()->getFfmpegPath();
  if (path != dir) Preferences::instance()->setValue(ffmpegPath, dir);
}

//-----------------------------------------------------------------------------

int getFFmpegTimeout() {
  return Preferences::instance()->getIntValue(ffmpegTimeout);
}

//-----------------------------------------------------------------------------

void setFFmpegTimeout(int secs) {
  int timeout = Preferences::instance()->getIntValue(ffmpegTimeout);
  if (secs != timeout) Preferences::instance()->setValue(ffmpegTimeout, secs);
}

//-----------------------------------------------------------------------------

void runFFmpeg(QProcess &process, const QStringList &arguments) {
#ifdef _WIN32
  QString ffmpeg_exe = "/ffmpeg.exe";
#else
  QString ffmpeg_exe = "/ffmpeg";
#endif

  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + ffmpeg_exe, arguments);
}

//-----------------------------------------------------------------------------

void runFFprobe(QProcess &process, const QStringList &arguments) {
#ifdef _WIN32
  QString ffprobe_exe = "/ffprobe.exe";
#else
  QString ffprobe_exe = "/ffprobe";
#endif

  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + ffprobe_exe, arguments);
}

//-----------------------------------------------------------------------------

bool findRhubarb(QString dir) {
  // Rhubarb executable
#ifdef _WIN32
  QString rhubarb_exe = "/rhubarb.exe";
#else
  QString rhubarb_exe = "/rhubarb";
#endif

  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if executable exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + rhubarb_exe));
}

//-----------------------------------------------------------------------------

bool checkRhubarb() {
  // Path in preferences
  return findRhubarb(Preferences::instance()->getRhubarbPath());
}

//-----------------------------------------------------------------------------

QString autodetectRhubarb() {
  QString dir = Preferences::instance()->getRhubarbPath();
  if (findRhubarb(dir)) return dir;

  if (findRhubarb(".")) return ".";
  if (findRhubarb("./rhubarb")) return "./rhubarb";
  if (findRhubarb("./rhubarb/bin")) return "./rhubarb/bin";
  if (findRhubarb("./Rhubarb-Lip-Sync")) return "./Rhubarb-Lip-Sync";

  return "";
}

//-----------------------------------------------------------------------------

void runRhubarb(QProcess &process, const QStringList &arguments) {
#ifdef _WIN32
  QString rhubarb_exe = "/rhubarb.exe";
#else
  QString rhubarb_exe = "/rhubarb";
#endif

  QString dir = Preferences::instance()->getRhubarbPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + rhubarb_exe, arguments);
}

//-----------------------------------------------------------------------------

QString getRhubarbDir() {
  return Preferences::instance()->getStringValue(rhubarbPath);
}

//-----------------------------------------------------------------------------

void setRhubarbDir(const QString &dir) {
  QString path = Preferences::instance()->getRhubarbPath();
  if (path != dir) Preferences::instance()->setValue(rhubarbPath, dir);
}

//-----------------------------------------------------------------------------

int getRhubarbTimeout() {
  return Preferences::instance()->getIntValue(rhubarbTimeout);
}

//-----------------------------------------------------------------------------

void setRhubarbTimeout(int secs) {
  int timeout = Preferences ::instance()->getIntValue(rhubarbTimeout);
  if (secs != timeout) Preferences::instance()->setValue(rhubarbTimeout, secs);
}

//-----------------------------------------------------------------------------

int waitAsyncProcess(const QProcess &process, int timeout) {
  QEventLoop eloop;
  QTimer timer;
  timer.connect(&timer, &QTimer::timeout, &eloop, [&eloop] { eloop.exit(-2); });
  QMetaObject::Connection con1 = process.connect(
      &process, &QProcess::errorOccurred, &eloop, [&eloop] { eloop.exit(-1); });
  QMetaObject::Connection con2 = process.connect(
      &process,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      &eloop, &QEventLoop::quit);
  if (timeout >= 0) timer.start(timeout);

  int result = eloop.exec();
  process.disconnect(con1);
  process.disconnect(con2);

  return result;
}

//-----------------------------------------------------------------------------

}  // namespace ThirdParty
