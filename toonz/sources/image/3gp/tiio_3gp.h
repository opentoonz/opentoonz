#pragma once

#ifndef TIIO_3GP_H
#define TIIO_3_H

#if defined(x64) || (defined(__GNUC__) && defined(_WIN32))
#include "tiio_3gp_proxy.h"
#else

#include <windows.h>

namespace QuickTime {
#define list List
#define map Map
#define iterator Iterator
#define float_t Float_t
#define int_fast8_t QT_int_fast8_t
#define int_fast16_t QT_int_fast16_t
#define uint_fast16_t QT_uint_fast16_t

#include "QTML.h"
#include "Movies.h"
#include "Script.h"
#include "FixMath.h"
#include "Sound.h"

#include "QuickTimeComponents.h"
#include "tquicktime.h"

#undef list
#undef map
#undef iterator
#undef float_t
#undef QT_int_fast8_t
#undef QT_int_fast16_t
#undef QT_uint_fast16_t
}

#include "tlevel_io.h"
#include "tthreadmessage.h"

using namespace QuickTime;

class TImageWriter3gp;
class TImageReader3gp;

bool IsQuickTimeInstalled();

class TLevelWriter3gp final : public TLevelWriter {
public:
  TLevelWriter3gp(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriter3gp();
  TImageWriterP getFrameWriter(TFrameId fid) override;

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
  short m_refNum;
  UCHAR m_cancelled;
  TSoundTrack *m_st;

  PixelXRGB *buf;
  int buf_lx;
  int buf_ly;
  TThread::Mutex m_mutex;

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriter3gp(f, winfo);
  };
  void saveSoundTrack(TSoundTrack *st) override;
};

class TLevelReader3gp final : public TLevelReader {
public:
  TLevelReader3gp(const TFilePath &path);
  ~TLevelReader3gp();
  TImageReaderP getFrameReader(TFrameId fid) override;
  // friend class TImageReaderMov;
  TLevelP loadInfo() override;

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
  std::vector<TimeValue> currentTimes;
  int m_lx, m_ly;

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReader3gp(f);
  };
  TThread::Mutex m_mutex;
};

//------------------------------------------------------------------------------

#endif  //! x64

#endif  // TIIO_MOV_H
