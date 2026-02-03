#pragma once

#ifndef TSIO_FFMPEG_INCLUDED
#define TSIO_FFMPEG_INCLUDED

#include "tsound_io.h"

//==========================================================
/*!
The class TSoundTrackReaderFFmpeg reads audio files
*/
class TSoundTrackReaderFFmpeg final : public TSoundTrackReader {
public:
  TSoundTrackReaderFFmpeg(const TFilePath &fp);
  ~TSoundTrackReaderFFmpeg() {}

  /*!
Loads audio file whose path has been specified in the constructor.
It returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader able to read audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderFFmpeg(fp);
  }
};

#endif
