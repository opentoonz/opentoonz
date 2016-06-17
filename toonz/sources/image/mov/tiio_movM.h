#pragma once

#ifndef TIIO_MOV_H
#define TIIO_MOV_H

#ifdef __LP64__
#include "tiio_mov_proxy.h"
#else

//#include <Carbon/Carbon.h>
//#include <Movies.h>
//#include <ImageCompression.h>
//#include "QuickTimeComponents.h"

#include <QuickTime/QuickTime.h>

#include "tquicktime.h"
#include "tlevel_io.h"
#include "tthread.h"
#include "tthreadmessage.h"

bool IsQuickTimeInstalled();

class TLevelWriterMov : public TLevelWriter {
  std::vector<std::pair<int, TimeValue>> m_savedFrames;

public:
  TLevelWriterMov(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterMov();
  TImageWriterP getFrameWriter(TFrameId fid);

  int m_IOError;

  void save(const TImageP &img, int frameIndex);

private:
  Movie m_movie;
  Track m_videoTrack;
  Media m_videoMedia;
  Track m_soundTrack;
  Media m_soundMedia;

  GWorldPtr m_gworld;
  PixMapHandle m_pixmap;
  // Handle       m_compressedData;
  short m_refNum;

  PixelXRGB *buf;
  int buf_lx;
  int buf_ly;
  int m_firstFrame;
  TThread::Mutex m_mutex;
  ComponentInstance m_ci;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterMov(f, winfo);
  };
  void saveSoundTrack(TSoundTrack *st);
};

class TLevelReaderMov : public TLevelReader {
  bool m_readAsToonzOutput;

public:
  TLevelReaderMov(const TFilePath &path);
  ~TLevelReaderMov();
  TImageReaderP getFrameReader(TFrameId fid);
  // friend class TImageReaderMov;
  TLevelP loadInfo();

  void enableRandomAccessRead(bool enable) { m_readAsToonzOutput = enable; }
  void load(const TRasterP &rasP, int frameIndex, const TPoint &pos,
            int shrinkX = 1, int shrinkY = 1);
  TDimension getSize() const { return TDimension(m_lx, m_ly); }
  TRect getBBox() const { return TRect(0, 0, m_lx - 1, m_ly - 1); }

  int m_IOError;

private:
  short m_refNum;
  Movie m_movie;
  short m_resId;
  Track m_track;
  long m_depth;
  map<int, TimeValue> currentTimes;
  int m_lx, m_ly;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderMov(f);
  };
  TThread::Mutex m_mutex;

  TLevelP loadToonzOutputFormatInfo();
};

//------------------------------------------------------------------------------
/*
class TWriterInfoMov : public TWriterInfo {
friend class TLevelWriterMov;
public:
  static TWriterInfo *create(const string &ext);
  ~TWriterInfoMov();
  TWriterInfo *clone() const;
private:
  TWriterInfoMov();


  TWriterInfoMov(const TWriterInfoMov&);

  TWriterInfoMov&operator=(const TWriterInfoMov&);   // not implemented

  typedef map<int, CodecQ> CodecTable;
  typedef map<int, CodecType> QualityTable;
  CodecTable m_codecTable;
  QualityTable m_qualityTable;
};
*/

namespace Tiio {

class MovWriterProperties : public TPropertyGroup {
  //  std::map<wstring, CodecType> m_codecMap;
  //  std::map<wstring, CodecQ> m_qualityMap;

public:
  MovWriterProperties();
  //	TPropertyGroup* clone() const;
  //  TEnumProperty m_codec;
  //  TEnumProperty m_quality;
  //	CodecType getCurrentCodec()const;
  //  wstring getCurrentNameFromCodec(CodecType &cCodec)const;
  //  wstring getCurrentQualityFromCodeQ(CodecQ &cCodeQ)const;
  //	CodecQ getCurrentQuality()const;
};
}

#endif  //__LP64__

#endif  // TIIO_MOV_H
