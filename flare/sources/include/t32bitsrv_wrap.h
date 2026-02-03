#pragma once

#ifndef T32BITSRV_WRAP
#define T32BITSRV_WRAP

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tcommon.h"
#include "traster.h"
#include "tipc.h"

// Qt includes
#include <QCoreApplication>
#include <QDir>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

/*
  This file contains the platform-specific command lines and server names to
  address 64-bit Toonz's background 32-bit process (used to deal with QuickTime,
  to say one).
*/

namespace t32bitsrv {

//*************************************************************************************
//  Platform-specific Server Command Lines
//*************************************************************************************

static QString srvName() {
  static QString name(tipc::applicationSpecificServerName("t32bitsrv"));
  return name;
}

#ifdef _WIN32
static QString srvCmdlinePrg() {
  static QString cmd("srv/t32bitsrv.exe");
  return cmd;
}
#else
static QString srvCmdlinePrg() {
  return "\"" + QCoreApplication::applicationDirPath() + "/t32bitsrv";
}
#endif

static QStringList srvCmdlineArgs() {
  static QStringList args = {srvName()};
  return args;
}

//*************************************************************************************
//  Buffer data exchanger
//*************************************************************************************

class DVAPI BufferExchanger final : public tipc::ShMemReader,
                                    public tipc::ShMemWriter {
  UCHAR *m_buf;
  UCHAR *m_data;

public:
  BufferExchanger(UCHAR *buf) : m_buf(buf), m_data(buf) {}
  ~BufferExchanger() {}

  UCHAR *buffer() const { return m_buf; }

  int read(const char *srcBuf, int len) override;
  int write(char *dstBuf, int len) override;
};

//*************************************************************************************
//  Raster data exchanger
//*************************************************************************************

template <typename PIXEL>
class DVAPI RasterExchanger final : public tipc::ShMemReader,
                                    public tipc::ShMemWriter {
  typedef PIXEL pix_type;

  TRasterPT<PIXEL> m_ras;
  PIXEL *m_pix;

public:
  RasterExchanger(TRasterP ras) : m_ras(ras) {
    m_ras->lock();
    m_pix = m_ras->pixels(0);
  }
  ~RasterExchanger() { m_ras->unlock(); }

  TRasterP raster() const { return m_ras; }

  int read(const char *srcBuf, int len) override;
  int write(char *dstBuf, int len) override;
};

}  // namespace t32bitsrv

#endif  // T32BITSRV_WRAP
