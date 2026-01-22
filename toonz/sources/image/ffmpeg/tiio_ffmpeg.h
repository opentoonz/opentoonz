#pragma once

#ifndef TTIO_FFMPEG_INCLUDED
#define TTIO_FFMPEG_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include "tsound.h"
#include "tfilepath.h"
#include "tgeometry.h"

#include <QFile>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QProcess>
#include <limits>

// Struct to hold video file information.
// Note: Zero values in any field may indicate that ffprobe failed to retrieve
// that information.
struct ffmpegFileInfo {
  int m_lx           = 0;
  int m_ly           = 0;
  int m_frameCount   = 0;
  double m_frameRate = 0.0;
};

// RAII helper to safely manage temporary files
// (can be used within functions that create intermediate PNG files)
struct FfmpegTempFileGuard {
  QVector<QString> &cleanupList;
  QStringList addedThisScope;

  explicit FfmpegTempFileGuard(QVector<QString> &list) : cleanupList(list) {}
  ~FfmpegTempFileGuard();

  void add(const QString &path);
  void release();

  FfmpegTempFileGuard(const FfmpegTempFileGuard &)            = delete;
  FfmpegTempFileGuard &operator=(const FfmpegTempFileGuard &) = delete;
};

class Ffmpeg {
public:
  Ffmpeg();
  ~Ffmpeg();

  // Creates optimized intermediate PNG image (used in the old flow)
  void createIntermediateImage(const TImageP &image, int frameIndex);

  // Executes ffmpeg with custom arguments
  void runFfmpeg(const QStringList &preInputArgs,
                 const QStringList &postInputArgs,
                 bool inputPathIncluded  = false,
                 bool outputPathIncluded = false, bool overwrite = true,
                 bool asynchronous = true);

  void runFfmpeg(const QStringList &preInputArgs,
                 const QStringList &postInputArgs, const TFilePath &outputPath);

  // Throws TImageException on ffprobe failure (synchronous use only)
  QString runFfprobe(const QStringList &args) const;

  void cleanUpFiles();
  void addToCleanUp(const QString &path);

  void setFrameRate(double fps);
  void setPath(const TFilePath &path);

  void saveSoundTrack(TSoundTrack *soundTrack);

  bool checkFilesExist() const;

  static bool checkFormat(std::string format);

  // Getters with const
  double getFrameRate() const;
  TDimension getSize() const;
  int getFrameCount() const;
  ffmpegFileInfo getInfo() const;

  void getFramesFromMovie(int frame = -1);
  TRasterImageP getImage(int frameIndex);

  TFilePath getFfmpegCache() const;

  void disablePrecompute();

  int getGifFrameCount();

private:
  QString m_intermediateFormat = "png";
  QString m_audioPath;
  QString m_audioFormat;
  QStringList m_audioArgs;

  double m_frameRate = 0.0;
  int m_lx           = 0;
  int m_ly           = 0;
  int m_bpp          = 0;
  int m_frameCount   = 0;
  int m_startNumber  = std::numeric_limits<int>::max();
  int m_ffmpegTimeoutMs;  // Value comes from constructor (user config)
  int m_sampleRate    = 0;
  int m_channelCount  = 0;
  int m_bitsPerSample = 0;

  bool m_hasSoundTrack = false;

  TFilePath m_path;
  QVector<QString> m_cleanUpList;

  QString cleanPathSymbols() const;
  bool waitFfmpeg(QProcess &process, bool async) const;

  // not allow copy of temporary files
  Ffmpeg(const Ffmpeg &)            = delete;
  Ffmpeg &operator=(const Ffmpeg &) = delete;
};

class TLevelReaderFFmpeg final : public TLevelReader {
public:
  explicit TLevelReaderFFmpeg(const TFilePath &path);
  ~TLevelReaderFFmpeg() override;

  TImageReaderP getFrameReader(TFrameId fid) override;
  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderFFmpeg(f);
  }

private:
  Ffmpeg *m_ffmpegReader = nullptr;
  bool m_framesExtracted = false;
  TDimension m_size;
  int m_frameCount        = -1;
  int m_lx                = 0;
  int m_ly                = 0;
  TImageInfo *m_imageInfo = nullptr;
};

#endif  // TTIO_FFMPEG_INCLUDED
