

#include "tnzsound.h"
#include "tsio.h"
// #include "tpluginmanager.h"
#include "tsound_io.h"
#include "tfiletype.h"

// static TPluginInfo info("soundIOPlugin");

void cribbioNonCapiscoNulla() {}

void mammaChePaura() {}

void initSoundIo() {
  TSoundTrackReader::define("wav", TSoundTrackReaderWav::create);
  TSoundTrackWriter::define("wav", TSoundTrackWriterWav::create);
  TFileType::declare("wav", TFileType::AUDIO_LEVEL);

  TSoundTrackReader::define("aiff", TSoundTrackReaderAiff::create);
  TSoundTrackWriter::define("aiff", TSoundTrackWriterAiff::create);
  TFileType::declare("aiff", TFileType::AUDIO_LEVEL);

  TSoundTrackReader::define("raw", TSoundTrackReaderRaw::create);
  TSoundTrackWriter::define("raw", TSoundTrackWriterRaw::create);
  TFileType::declare("raw", TFileType::AUDIO_LEVEL);

  // return &info;
}
