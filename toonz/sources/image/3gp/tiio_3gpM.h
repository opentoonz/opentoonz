#pragma once

#ifndef TIIO_3GP_H
#define TIIO_3GP_H

#ifdef __LP64__
#include "tiio_3gp_proxy.h"
#else

#include <Carbon/Carbon.h>
#include <QuickTime/Movies.h>
#include <QuickTime/ImageCompression.h>
#include <QuickTime/QuickTimeComponents.h>

#include "tquicktime.h"
#include "tthreadmessage.h"

#include "tlevel_io.h"
#include "tthread.h"

class TImageWriter3gp;
class TImageReader3gp;

bool IsQuickTimeInstalled();

class TLevelWriter3gp : public TLevelWriter {
public:
  TLevelWriter3gp(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriter3gp();
  TImageWriterP getFrameWriter(TFrameId fid);

  bool m_initDone;
  int m_IOError;
  Handle m_dataRef;
  Handle m_hMovieData;
  Handle m_soundDataRef;
  Handle m_hSoundMovieData;
  MovieExportComponent m_myExporter;

  void save(const TImageP &img, int frameIndex);

private:
  Movie m_movie;
  Track m_videoTrack;
  Media m_videoMedia;
  GWorldPtr m_gworld;
  PixMapHandle m_pixmap;
  Handle m_compressedData;
  short m_refNum;
  UCHAR m_cancelled;

  PixelXRGB *buf;
  int buf_lx;
  int buf_ly;
  TThread::Mutex m_mutex;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriter3gp(f, winfo);
  };
  void saveSoundTrack(TSoundTrack *st);
};

class TLevelReader3gp : public TLevelReader {
public:
  TLevelReader3gp(const TFilePath &path);
  ~TLevelReader3gp();
  TImageReaderP getFrameReader(TFrameId fid);
  // friend class TImageReader3gp;
  TLevelP loadInfo();

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
  vector<TimeValue> currentTimes;
  int m_lx, m_ly;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReader3gp(f);
  };
  TThread::Mutex m_mutex;
};

//------------------------------------------------------------------------------

/*
class TWriterInfo3gp : public TWriterInfo {
friend class TLevelWriter3gp;
public:
  static TWriterInfo *create(const string &ext);
  ~TWriterInfo3gp();
  TWriterInfo *clone() const;
private:
  TWriterInfo3gp();


  TWriterInfo3gp(const TWriterInfo3gp&);

  TWriterInfo3gp&operator=(const TWriterInfo3gp&);   // not implemented

  typedef map<int, CodecQ> CodecTable;
  typedef map<int, CodecType> QualityTable;
  CodecTable m_codecTable;
  QualityTable m_qualityTable;
};
*/

#endif  //!__LP64__

#endif  // TIIO_3gp_H
