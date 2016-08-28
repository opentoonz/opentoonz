#pragma once

#ifndef TTIO_GIF_INCLUDED
#define TTIO_GIF_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tiio_ffmpeg.h"
#include <QVector>
#include <QStringList>
#include <QtGui/QImage>
//#include "tthreadmessage.h"

//===========================================================
//
//  TLevelWriterGif
//
//===========================================================

class TLevelWriterGif : public TLevelWriter {
public:
  TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterGif();
  // FfmpegBridge* ffmpeg;
  void setFrameRate(double fps);

  TImageWriterP getFrameWriter(TFrameId fid) override;
  void save(const TImageP &image, int frameIndex);

  void saveSoundTrack(TSoundTrack *st);

  static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
    return new TLevelWriterGif(path, winfo);
  }

private:
  Ffmpeg *ffmpegWriter;
  int m_frameCount, m_lx, m_ly;
  // double m_fps;
  int m_scale;
  int m_padding      = 0;
  bool m_looping     = false;
  bool m_palette     = false;
  bool m_trim        = false;
  bool m_transparent = false;
  bool m_dithered    = false;
  int m_left = 0, m_right = 0, m_top = 0, m_bottom = 0;
  std::vector<QImage *> m_images;
  // std::vector<QImage> m_imagesResized;
  bool m_firstPass = true;
  bool checkImageMagick();
  bool checkDithering();
  QString m_thresholdsPath;
};

//===========================================================
//
//  TLevelReaderGif
//
//===========================================================

class TLevelReaderGif final : public TLevelReader {
public:
  TLevelReaderGif(const TFilePath &path);
  ~TLevelReaderGif();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderGif(f);
  }

  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();
  // TThread::Mutex m_mutex;
  // void *m_decompressedBuffer;
private:
  Ffmpeg *ffmpegReader;
  TDimension m_size;
  int m_frameCount, m_lx, m_ly;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class GifWriterProperties : public TPropertyGroup {
public:
  TIntProperty m_scale;
  TIntProperty m_padding;
  TBoolProperty m_looping;
  TBoolProperty m_palette;
  TBoolProperty m_trim;
  TBoolProperty m_transparent;
  TBoolProperty m_dithered;
  GifWriterProperties();
};

//===========================================================================

// Tiio::Reader *makeGifReader();
// Tiio::Writer *makeGifWriter();

}  // namespace

#endif
