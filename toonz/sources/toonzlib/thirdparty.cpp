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
#include <QRegularExpression>
#include <QDir>

namespace ThirdParty {

//-----------------------------------------------------------------------------

void initialize() {
  // Auto detect FFmpeg
  if (!ThirdParty::checkFFmpeg()) {
    QString path = ThirdParty::autodetectFFmpeg();
    if (!path.isEmpty()) ThirdParty::setFFmpegDir(path);
  }

  // Auto detect Rhubarb
  if (!ThirdParty::checkRhubarb()) {
    QString path = ThirdParty::autodetectRhubarb();
    if (!path.isEmpty()) ThirdParty::setRhubarbDir(path);
  }
}

//=============================================================================
// FFmpeg interface
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define FFMPEG_EXE "/ffmpeg.exe"
#define FFPROBE_EXE "/ffprobe.exe"
#else
#define FFMPEG_EXE "/ffmpeg"
#define FFPROBE_EXE "/ffprobe"
#endif

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
  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if both executables exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + FFMPEG_EXE)) &&
         TSystem::doesExistFileOrLevel(TFilePath(dir + FFPROBE_EXE));
}

//-----------------------------------------------------------------------------

bool checkFFmpeg() {
  // Path in preferences
  return findFFmpeg(Preferences::instance()->getFfmpegPath());
}

//-----------------------------------------------------------------------------

QString autodetectFFmpegViaPath() {
  QString command;
  QStringList arguments;

#ifdef _WIN32
  command = "where";
  arguments << "ffmpeg";
#else
  command = "which";
  arguments << "ffmpeg";
#endif

  QProcess process;
  process.start(command, arguments);

  if (!process.waitForFinished(3000)) {
    if (process.state() == QProcess::Running) {
      process.terminate();
      process.waitForFinished(500);
    }
    return "";
  }

  if (process.exitCode() != 0) {
    return "";
  }

  QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

  QStringList paths =
      output.split(QRegularExpression("[\\n\\r]+"), Qt::SkipEmptyParts);

  if (!paths.isEmpty()) {
    QString fullPath = paths.first().trimmed();

    QString pathDir = QFileInfo(fullPath).dir().path();

    if (findFFmpeg(pathDir)) {
      return pathDir;
    }
  }

  return "";
}

QString autodetectFFmpeg() {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (findFFmpeg(dir)) return dir;

  QString pathDir = autodetectFFmpegViaPath();
  if (!pathDir.isEmpty()) return pathDir;

  if (findFFmpeg(".")) return ".";
  if (findFFmpeg("./ffmpeg")) return "./ffmpeg";
  if (findFFmpeg("./ffmpeg/bin")) return "./ffmpeg/bin";
  if (findFFmpeg("./FFmpeg")) return "./FFmpeg";
  if (findFFmpeg("./FFmpeg/bin")) return "./FFmpeg/bin";

#ifndef _WIN32
  if (findFFmpeg("/usr/local/bin")) return "/usr/local/bin";
  if (findFFmpeg("/usr/bin")) return "/usr/bin";
  if (findFFmpeg("/bin")) return "/bin";
#endif

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
  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + FFMPEG_EXE, arguments);
}

//-----------------------------------------------------------------------------

void runFFprobe(QProcess &process, const QStringList &arguments) {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + FFPROBE_EXE, arguments);
}

//-----------------------------------------------------------------------------

void runFFmpegAudio(QProcess &process, QString srcPath, QString dstPath,
                    int samplerate, int bpp, int channels) {
  QStringList args;
  args << "-y";
  args << "-i";
  args << srcPath;
  args << "-f";
  switch (bpp) {
  case 8:  // unsigned
    args << "u8";
    break;
  case 16:
    args << "s16le";
    break;
  case 24:
    args << "s24le";
    break;
  case 32:  // floating-point
    args << "f32le";
    break;
  default:
    return;
  }
  args << "-ac";
  args << QString::number(channels);
  args << "-ar";
  args << QString::number(samplerate);
  args << dstPath;

  ThirdParty::runFFmpeg(process, args);
}

//-----------------------------------------------------------------------------

bool readFFmpegAudio(QProcess &process, QByteArray &rawData) {
  if (!process.waitForStarted()) return false;
  if (!process.waitForFinished(30000)) return false;
  bool success = (process.exitCode() == 0);

  if (success) rawData = process.readAllStandardOutput();
  process.close();

  return success;
}

//=============================================================================
// Rhubarb interface
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define RHUBARB_EXE "/rhubarb.exe"
#else
#define RHUBARB_EXE "/rhubarb"
#endif

//-----------------------------------------------------------------------------

bool findRhubarb(QString dir) {
  // Rhubarb executable

  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if executable exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + RHUBARB_EXE));
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

#ifndef _WIN32
  if (findRhubarb("/usr/local/bin")) return "/usr/local/bin";
  if (findRhubarb("/usr/bin")) return "/usr/bin";
  if (findRhubarb("/bin")) return "/bin";
#endif

  return "";
}

//-----------------------------------------------------------------------------

void runRhubarb(QProcess &process, const QStringList &arguments) {
  QString dir = Preferences::instance()->getRhubarbPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + RHUBARB_EXE, arguments);
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
