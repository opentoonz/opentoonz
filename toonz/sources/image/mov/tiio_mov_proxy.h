#pragma once

#ifndef TIIO_MOV_PROXY_H
#define TIIO_MOV_PROXY_H

#if defined(x64) || defined(__LP64__) || defined(LINUX) || defined(FREEBSD)  || defined(HAIKU) || (defined(_WIN32) && defined(__GNUC__))

// Qt includes
#include <QString>
#include <QLocalSocket>

#include "tlevel_io.h"

//---------------------------------------------------------------------

// QuickTime check
bool IsQuickTimeInstalled();

//---------------------------------------------------------------------

//******************************************************************************
//    TLevelWriterMov Proxy - delegates to a background 32-bit process
//******************************************************************************

class TLevelWriterMov final : public TLevelWriter {
  unsigned int m_id;

public:
  TLevelWriterMov(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterMov();

  void setFrameRate(double fps);

  TImageWriterP getFrameWriter(TFrameId fid);
  void save(const TImageP &img, int frameIndex);

  void saveSoundTrack(TSoundTrack *st);

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriterMov(f, winfo);
  };

  QLocalSocket *socket();
};

//******************************************************************************
//    TLevelReaderMov Proxy
//******************************************************************************

class TLevelReaderMov final : public TLevelReader {
  unsigned int m_id;
  int m_lx, m_ly;

public:
  TLevelReaderMov(const TFilePath &path);
  ~TLevelReaderMov();
  TImageReaderP getFrameReader(TFrameId fid);
  TLevelP loadInfo();

  const TImageInfo *getImageInfo(TFrameId fid) { return m_info; }
  const TImageInfo *getImageInfo() { return m_info; }

  void enableRandomAccessRead(bool enable);
  void load(const TRasterP &rasP, int frameIndex, const TPoint &pos,
            int shrinkX = 1, int shrinkY = 1);
  TDimension getSize() const { return TDimension(m_lx, m_ly); }
  TRect getBBox() const { return TRect(0, 0, m_lx - 1, m_ly - 1); }

public:
  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderMov(f);
  }
};

//===========================================================================

namespace Tiio {
class MovWriterProperties final : public TPropertyGroup {
public:
  MovWriterProperties();
};
}

//===========================================================================

#endif  // x64

#endif  // TIIO_MOV_PROXY_H
