#include <memory>

#include "tmachine.h"
#include "tsio_ffmpeg.h"
#include "tsystem.h"
#include "tfilepath_io.h"
#include "tsound_t.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "thirdparty.h"

#include <QDir>
#include <QProcess>

//==============================================================================

TSoundTrackReaderFFmpeg::TSoundTrackReaderFFmpeg(const TFilePath &fp)
    : TSoundTrackReader(fp) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundTrackReaderFFmpeg::load() {
  QProcess ffmpeg;
  QByteArray rawAudio;

  // Pipe the audio through ffmpeg
  ThirdParty::runFFmpegAudio(ffmpeg, m_path.getQString(), "-");
  if (!ThirdParty::readFFmpegAudio(ffmpeg, rawAudio)) return nullptr;

  long sampleCount = rawAudio.size() / 4;

  TSoundTrack *track = new TSoundTrackStereo16(44100, 2, sampleCount);
  memcpy((char *)track->getRawData(), rawAudio.constData(), sampleCount * 4);
  return track;
}
