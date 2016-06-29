#pragma once

#ifndef TSIO_WAV_INCLUDED
#define TSIO_WAV_INCLUDED

#include "tsound_io.h"

//==========================================================
/*!
The class TSoundTrackReaderWav reads audio files having
.wav extension
*/
class TSoundTrackReaderWav final : public TSoundTrackReader {
public:
  TSoundTrackReaderWav(const TFilePath &fp);
  ~TSoundTrackReaderWav() {}

  /*!
Loads the .wav audio file whose path has been specified in the constructor.
It returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader able to read .wav audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderWav(fp);
  }
};

//==========================================================
/*!
The class TSoundTrackWriterWav writes audio file having
.wav extension
*/

class TSoundTrackWriterWav final : public TSoundTrackWriter {
public:
  TSoundTrackWriterWav(const TFilePath &fp);
  ~TSoundTrackWriterWav() {}

  /*!
Saves the informations of the soundtrack in .wav audio file
whose path has been specified in the constructor.
*/
  bool save(const TSoundTrackP &) override;

  /*!
Returns a soundtrack writer able to write .wav audio files
*/
  static TSoundTrackWriter *create(const TFilePath &fp) {
    return new TSoundTrackWriterWav(fp);
  }
};

#endif
