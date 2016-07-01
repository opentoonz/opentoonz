#pragma once

#ifndef TCACHEDLEVEL_INCLUDED
#define TCACHEDLEVEL_INCLUDED

/*
   PER ORA SI PUO' USARE LA CACHE SOLO CON WINDOWS
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include "tfilepath.h"
#include "traster.h"
#include "tthread.h"

#undef DVAPI
#undef DVVAR
#ifdef TRASTERIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// forward declaration

class TRawDataCodec;
class TRasterCodec;
class TCachePersist;

//=============================================================================

/*
class TCache {
public:

  TCache();
  virtual ~TCache();

  bool isCached(int frame) const  = 0;
  bool isCached(int starFrame, int endFrame) const  = 0; // estremi compresi

  virtual void putRaster(int frame, const TRasterP &ras) = 0;

  virtual void getRaster(int frame, TRaster32P  &ras) const
    { getRaster(frame, TRasterP(ras)); }
  virtual void getRaster(int frame, TRaster64P  &ras) const
    { getRaster(frame, TRasterP(ras)); }
  virtual void getRaster(int frame, TRasterYUV422P &ras) const
    { getRaster(frame, TRasterP(ras)); }

  void invalidateAll() = 0;
  void invalidate(int starFrame, int endFrame) = 0; // estremi compresi

protected:
  virtual void getRaster(int frame, TRasterP &ras) const = 0;
};


class TRamCache final : public TCache {
public:
  TRamCache();

};


class TDiskCache final : public TCache {
public:
  TDiskCache();
};


void TDiskCache::putRaster(int frame, const TRsterP &ras)
{
}


void TDiskCache::getRaster(



class TRAMUncompressedCache final : public TRamCache {
};

class TRAMLzoCache final : public TRamCache {
};


class TDiskUncompressedCache final : public TDiskCache {
};


class TDiskYUV422Cache final : public TDiskCache {
public:
  TDiskYUV422Cache();
  ~TDiskYUV422Cache();

  void putRaster(int frame, const TRasterP &ras);
  void getRaster(int frame, TRasterYUV422P &ras) const;

};



void TRAMUncompressedCache::getRaster(int frame, TRasterP &ras)
{
TRaster32P ras32(ras.getSize());
getRaster(frame, ras32);
convert(ras, ras32);
}

void TRAMUncompressedCache::getRaster(int frame, TRaster32P  &ras) const
{
//....
}

*/

//=============================================================================
//=============================================================================
//=============================================================================

class DVAPI TRasterCache {
public:
  TRasterCache(TCachePersist *cp);
  virtual ~TRasterCache();

  void setMode(const TDimension &size, int bpp);
  void getMode(TDimension &size, int &bpp) const;

  TRasterP getRaster(int frame) const;
  bool getBuffer(int frame, UCHAR *&buffer, int &wrap, int &bpp,
                 TDimension &rasDim) const;

  bool getRaster(int frame, TRaster32P &ras) const;

  void putRaster(int frame, const TRasterP &ras);

  UCHAR *getRawData(int frame, TINT32 &size, int &lx, int &ly) const;

  bool isFrameCached(int frame) const;

  void invalidate();
  void invalidate(int startFrame, int endFrame);

  void enablePrefetch(bool newState);
  bool isPrefetchEnabled() const;

  TUINT64 getUsedSpace();

protected:
  //  virtual TRasterP doGetRaster(int frame) = 0;
  //  virtual void doPutRaster(int frame, const TRasterP &ras) = 0;

private:
  class Data;
  Data *m_data;
};

//------------------------------------------------------------------------------

class DVAPI TCachePersist {
public:
  TCachePersist(TRasterCodec *codec) : m_codec(codec) {}
  virtual ~TCachePersist() {}

  virtual void setFrameSize(int lx, int ly, int bpp) = 0;

  virtual TRasterP doGetRaster(int frame) = 0;
  virtual bool doGetRaster(int frame, TRaster32P &ras) const = 0;

  virtual bool doPutRaster(int frame, const TRasterP &ras) = 0;

  virtual void onInvalidate() = 0;
  virtual void onInvalidate(int startFrame, int endFrame) = 0;

  virtual UCHAR *getRawData(int frame, TINT32 &size, int &lx, int &ly) = 0;

  virtual TUINT64 getUsedSpace() = 0;

  // private:
  TRasterCodec *m_codec;
};

class DVAPI TRamCachePersist final : public TCachePersist {
public:
  TRamCachePersist(TRasterCodec *codec);
  ~TRamCachePersist();

  void setFrameSize(int lx, int ly, int bpp) override {}

  void onInvalidate() override;
  void onInvalidate(int startFrame, int endFrame) override;

  UCHAR *getRawData(int frame, TINT32 &size, int &lx, int &ly) override;

  TUINT64 getUsedSpace() override;

private:
  TRasterP doGetRaster(int frame) override;
  bool doGetRaster(int frame, TRaster32P &ras) const override;

  bool doPutRaster(int frame, const TRasterP &ras) override;

private:
  class Imp;
  Imp *m_imp;
};

//------------------------------------------------------------------------------

class DVAPI TDiskCachePersist final : public TCachePersist {
public:
  TDiskCachePersist(TRasterCodec *codec, const TFilePath &fullpath);
  ~TDiskCachePersist();

  void setFrameSize(int lx, int ly, int bpp) override;

  void onInvalidate() override;
  void onInvalidate(int startFrame, int endFrame) override;

  UCHAR *getRawData(int frame, TINT32 &size, int &lx, int &ly) override;

  TUINT64 getUsedSpace() override;

private:
  TRasterP doGetRaster(int frame) override;
  bool doGetRaster(int frame, TRaster32P &ras) const override;

  bool doPutRaster(int frame, const TRasterP &ras) override;

private:
  class Imp;
  Imp *m_imp;
};

//------------------------------------------------------------------------------
// TDiskCachePersist2 usa il Direct File I/O (acceso al disco non bufferizzato)

class DVAPI TDiskCachePersist2 final : public TCachePersist {
public:
  TDiskCachePersist2(TRasterCodec *codec, const TFilePath &fullpath);
  ~TDiskCachePersist2();

  void setFrameSize(int lx, int ly, int bpp) override;

  void onInvalidate() override;
  void onInvalidate(int startFrame, int endFrame) override;

  UCHAR *getRawData(int frame, TINT32 &size, int &lx, int &ly) override;

  TUINT64 getUsedSpace() override;

private:
  TRasterP doGetRaster(int frame) override;
  bool doGetRaster(int frame, TRaster32P &ras) const override;

  bool doPutRaster(int frame, const TRasterP &ras) override;

private:
  class Imp;
  Imp *m_imp;
};

//------------------------------------------------------------------------------

class DVAPI TCompressedLevel {
public:
  TCompressedLevel(const TFilePath &fullpath);
  ~TCompressedLevel();
  void setSize(TDimension d);
  void putImage(int frame, const TRaster32P &ras);
  TRaster32P getImage(int frame);
  void invalidate(int frame);
  void invalidateAll();

private:
  TCompressedLevel();  // not implemented

  TFilePath m_fullpath;
  TRawDataCodec *m_codec;

  int m_viewFrameMin, m_viewFrameMax;

#ifdef _WIN32
  HANDLE m_hFile, m_hMap;
  LPVOID m_fileMapAddress;

  int m_xSize, m_ySize;
  int m_frameSize;
  SYSTEM_INFO m_systemInfo;
  __int64 m_mapOffset;
  static DWORD m_maxViewSize;
  static DWORD m_maxFileSize;
  static DWORD m_reallocFileSize;
  //-------
  HANDLE initFile(const TFilePath &fname);
  HANDLE mapFile(HANDLE hFile, ULONGLONG size);
#endif

  static TThread::Mutex m_mutex;

  void setCurrentView(int frame, bool force = false);
};

#endif
