#include <cstdio>

#include <QCoreApplication>
#include <QProcess>

#include "thirdparty.h"
#include "toonz/preferences.h"

namespace {

int fail(const char *message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

void stopIfStarted(QProcess &process) {
  if (process.state() == QProcess::NotRunning) return;
  process.kill();
  process.waitForFinished();
}

}  // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  Preferences *preferences = Preferences::instance();
  const QString oldFFmpeg  = preferences->getFfmpegPath();
  const QString oldRhubarb = preferences->getRhubarbPath();

  preferences->setValue(ffmpegPath, "", false);
  preferences->setValue(rhubarbPath, "", false);

  QProcess ffmpeg;
  ThirdParty::runFFmpeg(ffmpeg, {"-version"});
  ffmpeg.waitForStarted(100);
  stopIfStarted(ffmpeg);

  QProcess ffprobe;
  ThirdParty::runFFprobe(ffprobe, {"-version"});
  ffprobe.waitForStarted(100);
  stopIfStarted(ffprobe);

  QProcess rhubarb;
  ThirdParty::runRhubarb(rhubarb, {"--version"});
  rhubarb.waitForStarted(100);
  stopIfStarted(rhubarb);

  preferences->setValue(ffmpegPath, oldFFmpeg, false);
  preferences->setValue(rhubarbPath, oldRhubarb, false);

  if (ffmpeg.program().isEmpty() || ffprobe.program().isEmpty() ||
      rhubarb.program().isEmpty())
    return fail("third-party tool program path was not resolved");

  return 0;
}
