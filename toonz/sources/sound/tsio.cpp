

#include "tnzsound.h"
#include "tsio.h"
// #include "tpluginmanager.h"
#include "tsound_io.h"
#include "tfiletype.h"

// static TPluginInfo info("soundIOPlugin");

void initSoundIo() {
  TSoundTrackReader::define("wav", TSoundTrackReaderWav::create);
  TSoundTrackWriter::define("wav", TSoundTrackWriterWav::create);
  TFileType::declare("wav", TFileType::AUDIO_LEVEL);

  TSoundTrackReader::define("aiff", TSoundTrackReaderAiff::create);
  TSoundTrackWriter::define("aiff", TSoundTrackWriterAiff::create);
  TFileType::declare("aiff", TFileType::AUDIO_LEVEL);

  TSoundTrackReader::define("aif", TSoundTrackReaderAiff::create);
  TSoundTrackWriter::define("aif", TSoundTrackWriterAiff::create);
  TFileType::declare("aif", TFileType::AUDIO_LEVEL);

  TSoundTrackReader::define("raw", TSoundTrackReaderRaw::create);
  TSoundTrackWriter::define("raw", TSoundTrackWriterRaw::create);
  TFileType::declare("raw", TFileType::AUDIO_LEVEL);

  if (FfmpegAudio::checkFfmpeg()) {
    TSoundTrackReader::define("mp3", TSoundTrackReaderMp3::create);
    // TSoundTrackWriter::define("mp3", TSoundTrackWriterMp3::create);
    TFileType::declare("mp3", TFileType::AUDIO_LEVEL);
  }
  // return &info;
}
