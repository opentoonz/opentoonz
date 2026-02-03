#pragma once

#ifndef TIIO_AVI_H
#define TIIO_AVI_H

#ifdef _WIN32
#include <windows.h>
#include <vfw.h>
#endif

#include "tlevel_io.h"
#include "tthreadmessage.h"

#include <QCoreApplication>

class TAviCodecCompressor;
class VDVideoDecompressor;

//===========================================================
//
//  TLevelWriterAvi
//
//===========================================================

class TLevelWriterAvi final : public TLevelWriter {
public:
  TLevelWriterAvi(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterAvi();
  TImageWriterP getFrameWriter(TFrameId fid) override;

  void saveSoundTrack(TSoundTrack *st) override;
  void save(const TImageP &image, int frameIndex);
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterAvi(f, winfo);
  }

private:
  TThread::Mutex m_mutex;
#ifdef _WIN32
  PAVIFILE m_aviFile;
  PAVISTREAM m_videoStream;
  PAVISTREAM m_audioStream;
  TSoundTrack *m_st;
  HIC m_hic;
  BITMAPINFO *m_bitmapinfo;
  BITMAPINFO *m_outputFmt;
  AVISTREAMINFO m_audioStreamInfo;
  void *m_buffer;
  bool m_initDone;
  int IOError;
  int m_bpp;
  long m_maxDataSize;
  std::list<int> m_delayedFrames;
  int m_firstframe;
#endif

  void doSaveSoundTrack();
  void searchForCodec();
#ifdef _WIN32
  int compressFrame(BITMAPINFOHEADER *outHeader, void **bufferOut,
                    int frameIndex, DWORD flagsIn, DWORD &flagsOut);
#endif
  void createBitmap(int lx, int ly);
};

//===========================================================
//
//  TLevelWriterAvi
//
//===========================================================

class TLevelReaderAvi final : public TLevelReader {
public:
  TLevelReaderAvi(const TFilePath &path);
  ~TLevelReaderAvi();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderAvi(f);
  }
  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();
  TThread::Mutex m_mutex;
  void *m_decompressedBuffer;

#ifdef _WIN32
private:
  PAVISTREAM m_videoStream;
  BITMAPINFO *m_srcBitmapInfo, *m_dstBitmapInfo;
  HIC m_hic;
  int IOError, m_prevFrame;

  int readFrameFromStream(void *bufferOut, DWORD &bufferSize,
                          int frameIndex) const;
  DWORD decompressFrame(void *srcBuffer, int srcSize, void *dstBuffer,
                        int currentFrame, int desiredFrame);
  HIC findCandidateDecompressor();
#endif
};

//===========================================================
//
//  Tiio::AviWriterProperties
//
//===========================================================

namespace Tiio {
class AviWriterProperties final : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(AviWriterProperties)
public:
  AviWriterProperties();
  TEnumProperty m_codec;
  static TEnumProperty m_defaultCodec;

  void updateTranslation() override;
};
}

#endif  // TIIO_AVI_H
