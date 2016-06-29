#pragma once

#ifndef TSIO_AIFF_INCLUDED
#define TSIO_AIFF_INCLUDED

#include "tsound_io.h"

//==========================================================
/*!
The class TSoundTrackReaderAiff reads audio files having
.aiff extension
*/

class TSoundTrackReaderAiff final : public TSoundTrackReader {
public:
  TSoundTrackReaderAiff(const TFilePath &fp);
  ~TSoundTrackReaderAiff() {}

  /*!
Loads the .aiff audio file whose path has been specified in the constructor.
It returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader able to read .aiff audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderAiff(fp);
  }
};

//==========================================================
/*!
The class TSoundTrackWriterAiff writes audio file having
.aiff extension
*/
class TSoundTrackWriterAiff final : public TSoundTrackWriter {
public:
  TSoundTrackWriterAiff(const TFilePath &fp);
  ~TSoundTrackWriterAiff() {}

  /*!
Saves the informations of the soundtrack in .aiff audio file
whose path has been specified in the constructor.
*/
  bool save(const TSoundTrackP &) override;

  /*!
Returns a soundtrack writer able to write .aiff audio files
*/
  static TSoundTrackWriter *create(const TFilePath &fp) {
    return new TSoundTrackWriterAiff(fp);
  }
};

#endif
