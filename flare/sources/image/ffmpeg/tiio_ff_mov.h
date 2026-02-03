#pragma once

#ifndef TTIO_FFMPEG_MOV_INCLUDED
#define TTIO_FFMPEG_MOV_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tiio_ffmpeg.h"

#include <QCoreApplication>

//===========================================================
//
//  TLevelWriterFFMov
//
//===========================================================

class TLevelWriterFFMov : public TLevelWriter {
public:
  TLevelWriterFFMov(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterFFMov();
  void setFrameRate(double fps) override;

  TImageWriterP getFrameWriter(TFrameId fid) override;
  void save(const TImageP &image, int frameIndex);

  void saveSoundTrack(TSoundTrack *st) override;

  static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
    return new TLevelWriterFFMov(path, winfo);
  }

private:
  Ffmpeg *ffmpegWriter;
  int m_lx, m_ly;
  int m_scale;
  int m_vidQuality;
};

//===========================================================
//
//  TLevelReaderFFMov
//
//===========================================================

class TLevelReaderFFMov final : public TLevelReader {
public:
  TLevelReaderFFMov(const TFilePath &path);
  ~TLevelReaderFFMov();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderFFMov(f);
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

class FFMovWriterProperties : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(FFMovWriterProperties)
public:
  TIntProperty m_vidQuality;
  TIntProperty m_scale;
  FFMovWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

}  // namespace Tiio

#endif
