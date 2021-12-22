#pragma once

#ifndef TIIO_3GP_PROXY_H
#define TIIO_3GP_PROXY_H

#if defined(x64) || defined(__LP64__) || defined(LINUX) || defined(FREEBSD) || (defined(_WIN32) && defined(__GNUC__))

// Qt includes
#include <QString>
#include <QLocalSocket>

#include "tlevel_io.h"

//---------------------------------------------------------------------

// QuickTime check
bool IsQuickTimeInstalled();

//---------------------------------------------------------------------

//******************************************************************************
//    TLevelWriter3gp Proxy - delegates to a background 32-bit process
//******************************************************************************

class TLevelWriter3gp final : public TLevelWriter {
  unsigned int m_id;

public:
  TLevelWriter3gp(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriter3gp();

  void setFrameRate(double fps);

  TImageWriterP getFrameWriter(TFrameId fid);
  void save(const TImageP &img, int frameIndex);

  void saveSoundTrack(TSoundTrack *st);

public:
  static TLevelWriter *create(const TFilePath &f, TPropertyGroup *winfo) {
    return new TLevelWriter3gp(f, winfo);
  };

  QLocalSocket *socket();
};

//******************************************************************************
//    TLevelReader3gp Proxy
//******************************************************************************

class TLevelReader3gp final : public TLevelReader {
  unsigned int m_id;
  int m_lx, m_ly;

public:
  TLevelReader3gp(const TFilePath &path);
  ~TLevelReader3gp();
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
    return new TLevelReader3gp(f);
  }
};

//===========================================================================

#endif  // x64

#endif  // TIIO_3GP_PROXY_H
