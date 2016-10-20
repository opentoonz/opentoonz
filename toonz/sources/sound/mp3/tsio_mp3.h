#pragma once

#ifndef TSIO_MP3_INCLUDED
#define TSIO_MP3_INCLUDED

#include "tsound_io.h"

//==========================================================
/*!
The class TSoundTrackReaderMp3 reads audio files having
.mp3 extension
*/
class TSoundTrackReaderMp3 final : public TSoundTrackReader {
public:
  TSoundTrackReaderMp3(const TFilePath &fp);
  ~TSoundTrackReaderMp3() {}

  /*!
Loads the .mp3 audio file whose path has been specified in the constructor.
It returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader able to read .mp3 audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderMp3(fp);
  }
};

class FfmpegAudio {
public:
  TFilePath getRawAudio(TFilePath path);
  static bool checkFfmpeg();

private:
  TFilePath getFfmpegCache();
  void runFfmpeg(QStringList args);
};

#endif
