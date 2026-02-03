#pragma once

#ifndef TTIO_APNG_INCLUDED
#define TTIO_APNG_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tiio_ffmpeg.h"
#include <QCoreApplication>

//===========================================================
//
//  TLevelWriterAPng
//
//===========================================================

class TLevelWriterAPng : public TLevelWriter {
public:
  TLevelWriterAPng(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterAPng();
  void setFrameRate(double fps) override;

  TImageWriterP getFrameWriter(TFrameId fid) override;
  void save(const TImageP &image, int frameIndex);

  void saveSoundTrack(TSoundTrack *st) override;

  static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
    return new TLevelWriterAPng(path, winfo);
  }

private:
  Ffmpeg *ffmpegWriter;
  int m_lx, m_ly;
  int m_scale;
  bool m_looping;
  bool m_extPng;
};

//===========================================================
//
//  TLevelReaderAPng
//
//===========================================================

class TLevelReaderAPng final : public TLevelReader {
public:
  TLevelReaderAPng(const TFilePath &path);
  ~TLevelReaderAPng();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderAPng(f);
  }

  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();
private:
  Ffmpeg *ffmpegReader;
  bool ffmpegFramesCreated = false;
  TDimension m_size;
  int m_frameCount, m_lx, m_ly;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class APngWriterProperties : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(APngWriterProperties)
public:
  TIntProperty m_scale;
  TBoolProperty m_looping;
  TBoolProperty m_extPng;
  APngWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

}  // namespace Tiio

#endif
