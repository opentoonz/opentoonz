#pragma once

#ifndef TSIO_MP3_INCLUDED
#define TSIO_MP3_INCLUDED

#include "tsound_io.h"
#include <memory>

//==========================================================
/*!
The class TSoundTrackReaderMp3 reads audio files with
the .mp3 extension
*/
class TSoundTrackReaderMp3 final : public TSoundTrackReader {
public:
  TSoundTrackReaderMp3(const TFilePath &fp);
  ~TSoundTrackReaderMp3() = default;

  /*!
Loads the .mp3 audio file whose path was specified in the constructor.
Returns a TSoundTrackP created from the audio file
*/
  TSoundTrackP load() override;

  /*!
Returns a soundtrack reader capable of reading .mp3 audio files
*/
  static TSoundTrackReader *create(const TFilePath &fp) {
    return new TSoundTrackReaderMp3(fp);
  }
};

class FfmpegAudio {
public:
  TFilePath getRawAudio(TFilePath path);

  // Optional: add explicit constructor and destructor
  FfmpegAudio()  = default;
  ~FfmpegAudio() = default;

  // Disable copy and assignment to avoid issues
  FfmpegAudio(const FfmpegAudio &)            = delete;
  FfmpegAudio &operator=(const FfmpegAudio &) = delete;

  // Allow move if necessary
  FfmpegAudio(FfmpegAudio &&)            = default;
  FfmpegAudio &operator=(FfmpegAudio &&) = default;

private:
  TFilePath getFfmpegCache();
  void runFfmpeg(QStringList args);
};

#endif
