#pragma once

#ifndef TSIO_RAW_INCLUDED
#define TSIO_RAW_INCLUDED

#include "tsound_io.h"

//==========================================================

/*!
The class TSoundTrackReaderRaw reads audio files having
.raw extension (this kind of file contains only the sample)
*/
class TSoundTrackReaderRaw final : public TSoundTrackReader {
public:
  TSoundTrackReaderRaw(const TFilePath &fp);
  ~TSoundTrackReaderRaw() {}

  /*!
Loads the .raw audio file whose path has been specified in the constructor.
It returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader able to read .raw audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderRaw(fp);
  }
};

//==========================================================

/*!
The class TSoundTrackWriterRaw writes audio file having
.raw extension (this kind of file contains only the sample)
*/
class TSoundTrackWriterRaw final : public TSoundTrackWriter {
public:
  TSoundTrackWriterRaw(const TFilePath &fp);
  ~TSoundTrackWriterRaw() {}

  /*!
Saves the informations of the soundtrack in .raw audio file
whose path has been specified in the constructor.
*/
  bool save(const TSoundTrackP &) override;

  /*!
Returns a soundtrack writer able to write .raw audio files
*/
  static TSoundTrackWriter *create(const TFilePath &fp) {
    return new TSoundTrackWriterRaw(fp);
  }
};

#endif
