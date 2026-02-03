

#include "tnzsound.h"
// #include "tpluginmanager.h"
#include "tsound_io.h"
#include "tfiletype.h"
#include "thirdparty.h"

#include "wav/tsio_wav.h"
#include "aiff/tsio_aiff.h"
#include "raw/tsio_raw.h"
#include "ffmpeg/tsio_ffmpeg.h"

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

  if (ThirdParty::checkFFmpeg()) {
    TSoundTrackReader::define("mp3", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("mp3", TFileType::AUDIO_LEVEL);
    TSoundTrackReader::define("ogg", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("ogg", TFileType::AUDIO_LEVEL);
    TSoundTrackReader::define("flac", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("flac", TFileType::AUDIO_LEVEL);
    TSoundTrackReader::define("m4a", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("m4a", TFileType::AUDIO_LEVEL);
    TSoundTrackReader::define("aac", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("aac", TFileType::AUDIO_LEVEL);
    TSoundTrackReader::define("ffaudio", TSoundTrackReaderFFmpeg::create);
    TFileType::declare("ffaudio", TFileType::AUDIO_LEVEL);
  }
  // return &info;
}
