#pragma once

#ifndef _TBIGMEMORYMANAGER_
#define _TBIGMEMORYMANAGER_

class Chunkinfo;

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "tcommon.h"
#include "tthreadmessage.h"
class TRaster;

class DVAPI TBigMemoryManager {
  TThread::Mutex m_mutex;
  UCHAR *m_theMemory;
  std::map<UCHAR *, Chunkinfo> m_chunks;
  UCHAR *allocate(UINT &size);

  TUINT32 m_availableMemory, m_allocatedMemory;
  std::map<UCHAR *, Chunkinfo>::iterator shiftBlock(
      const std::map<UCHAR *, Chunkinfo>::iterator &it, TUINT32 offset);
  TRaster *findRaster(TRaster *ras);
  void checkConsistency();
  UCHAR *remap(TUINT32 RequestedSize);
  void printLog(TUINT32 size);

public:
  TBigMemoryManager();
  ~TBigMemoryManager();
  bool init(TUINT32 sizeinKb);
  bool putRaster(TRaster *ras, bool canPutOnDisk = true);
  bool releaseRaster(TRaster *ras);
  void lock(UCHAR *buffer);
  void unlock(UCHAR *buffer);
  UCHAR *getBuffer(UINT size);
  static TBigMemoryManager *instance();
  bool isActive() const { return m_theMemory != 0; }
  // void releaseSubraster(UCHAR *chunk, TRaster*subRas);
  TUINT32 getAvailableMemoryinKb() const {
    assert(m_theMemory != 0);
    return m_availableMemory >> 10;
  }
  void getRasterInfo(int &rasterCount, TUINT32 &totRasterMemInKb,
                     int &notCachedRasterCount,
                     TUINT32 &notCachedRasterMemInKb);
#ifdef _DEBUG
  TUINT32 m_totRasterMemInKb;
  void printMap();
#endif
  int getAllocationPeak();
  int getAllocationMean();

  void setRunOutOfContiguousMemoryHandler(void (*callback)(unsigned long size));

private:
  friend class TRaster;
  void (*m_runOutCallback)(unsigned long);
};

#endif
