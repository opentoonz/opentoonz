#pragma once

#ifndef TTIO_FFMPEG_INCLUDED
#define TTIO_FFMPEG_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include <QVector>
#include <QStringList>

struct ffmpegFileInfo {
  int m_lx, m_ly, m_frameCount;
  double m_frameRate;
};

class Ffmpeg {
public:
  Ffmpeg();
  ~Ffmpeg();
  void createIntermediateImage(const TImageP &image, int frameIndex);
  void runFfmpeg(QStringList preIArgs, QStringList postIArgs,
                 bool includesInPath, bool includesOutPath,
                 bool overWriteFiles);
  void runFfmpeg(QStringList preIArgs, QStringList postIArgs, TFilePath path);
  QString runFfprobe(QStringList args);
  void cleanUpFiles();
  void addToCleanUp(QString);
  void setFrameRate(double fps);
  void setPath(TFilePath path);
  void saveSoundTrack(TSoundTrack *st);
  bool checkFilesExist();
  static bool checkFfmpeg();
  static bool checkFfprobe();
  static bool checkFormat(std::string format);
  double getFrameRate();
  TDimension getSize();
  int getFrameCount();
  void getFramesFromMovie(int frame = -1);
  TRasterImageP getImage(int frameIndex);
  TFilePath getFfmpegCache();
  ffmpegFileInfo getInfo();
  void disablePrecompute();
  int getGifFrameCount();

private:
  QString m_intermediateFormat, m_ffmpegPath, m_audioPath, m_audioFormat;
  int m_frameCount    = 0, m_lx, m_ly, m_bpp, m_bitsPerSample, m_channelCount,
      m_ffmpegTimeout = 30000, m_frameNumberOffset = -1;
  double m_frameRate  = 24.0;
  bool m_ffmpegExists = false, m_ffprobeExists = false, m_hasSoundTrack = false;
  TFilePath m_path;
  QVector<QString> m_cleanUpList;
  QStringList m_audioArgs;
  TUINT32 m_sampleRate;
  QString cleanPathSymbols();
};

#endif
