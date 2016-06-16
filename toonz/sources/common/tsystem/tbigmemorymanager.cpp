

#include "traster.h"
#include "tbigmemorymanager.h"
#include "timagecache.h"
#include "tsystem.h"
#include "tconvert.h"
#include <set>
#include "tfilepath_io.h"

#ifdef _DEBUG
std::set<TRaster *> Rasters;
#endif

#ifdef TNZCORE_LIGHT
#ifdef MessageBox
#undef MessageBox
#endif

#define MessageBox MessageBoxA
#endif

class Chunkinfo {
public:
  TUINT32 m_size;
  // int m_locks;
  std::vector<TRaster *> m_rasters;
  // bool m_putInNormalMemory;
  Chunkinfo(TUINT32 size, TRaster *ras)  //, bool putInNormalMemory=false)
      : m_size(size)
        //, m_locks(0)
        ,
        m_rasters()
  //, m_putInNormalMemory(putInNormalMemory)
  {
    if (ras) m_rasters.push_back(ras);
  }

  Chunkinfo()
      : m_size(0)
      //, m_locks(0)
      , m_rasters()
  /*, m_putInNormalMemory(false)*/ {}
};

//------------------------------------------------------------

//! Sets the global callback handler for the 'Run out of contiguous memory'
//! event
//! The callback receives the size (in bytes) of the raster which caused the
//! problem.
void TBigMemoryManager::setRunOutOfContiguousMemoryHandler(
    void (*callback)(unsigned long)) {
  m_runOutCallback = callback;
}

//------------------------------------------------------------------------------

namespace {
int allocationPeakKB               = 0;
unsigned long long allocationSumKB = 0;
unsigned long allocationCount      = 0;
}

//------------------------------------------------------------------------------

//! Returns the \b peak size, in KB, of the allocated rasters in current Toonz
//! session.
int TBigMemoryManager::getAllocationPeak() { return allocationPeakKB; }

//------------------------------------------------------------------------------

//! Returns the \b mean size, in KB, of the allocated rasters in current Toonz
//! session.
int TBigMemoryManager::getAllocationMean() {
  return allocationSumKB / allocationCount;
}

//------------------------------------------------------------------------------

TBigMemoryManager *TBigMemoryManager::instance() {
  static TBigMemoryManager *theManager = 0;

  if (theManager) return theManager;

  return theManager = new TBigMemoryManager();
}

//------------------------------------------------------------------------------
/*
TBigMemoryManager::~TBigMemoryManager()
{

if (m_theMemory==0) return;
QMutexLocker sl(m_mutex);
assert(m_chunks.empty());

free(m_theMemory);
theManager = 0;
}
*/

//------------------------------------------------------------------------------

UCHAR *TBigMemoryManager::allocate(UINT &size) {
  TThread::MutexLocker sl(&m_mutex);
  UCHAR *chunk = (UCHAR *)calloc(size, 1);
  while (chunk == 0 && size > 128 * 1024 * 1024) {
    size -= 128 * 1024 * 1024;
    chunk = (UCHAR *)calloc(size, 1);
  }
  return chunk;
}

//------------------------------------------------------------------------------

TBigMemoryManager::TBigMemoryManager()
    : m_chunks()
    , m_theMemory(0)
    , m_availableMemory(0)
    , m_allocatedMemory(0)
#ifdef _DEBUG
    , m_totRasterMemInKb(0)
#endif
{
}

//------------------------------------------------------------------------------

bool TBigMemoryManager::init(TUINT32 sizeinKb) {
  TThread::MutexLocker sl(&m_mutex);

  if (sizeinKb == 0) return true;

  if (sizeinKb >= 2 * 1024 * 1024) {
    //  MessageBox( NULL, "TRONCO!!!", "Warning", MB_OK);
    sizeinKb = (TUINT32)(1.8 * 1024 * 1024);
  }

  m_availableMemory = sizeinKb * 1024;

  m_theMemory       = allocate(m_availableMemory);
  m_allocatedMemory = m_availableMemory;

  if (!m_theMemory) {
    //  MessageBox( NULL, "Ouch!can't allocate Big Chunk!", "Warning", MB_OK);
    m_theMemory       = 0;
    m_availableMemory = 0;
    return false;
  }
  // char str[1024];
  // sprintf_s(str, "chiesto %d MB, allocato un big chunk di %d MB",
  // sizeinKb/1024, m_availableMemory/(1024*1024));
  // MessageBox( NULL, str, "Warning", MB_OK);

  m_chunks[m_theMemory + m_availableMemory] = Chunkinfo(0, 0);
  return true;
}

//------------------------------------------------------------------------------
/*
void TBigMemoryManager::lock(UCHAR *buffer)
{
QMutexLocker sl(m_mutex);
if (m_theMemory==0) return;
assert(buffer);
map<UCHAR*, Chunkinfo>::iterator it = m_chunks.find(buffer);

if (it==m_chunks.end()) return;

++it->second.m_locks;
}
*/
//------------------------------------------------------------------------------

/*
void TBigMemoryManager::unlock(UCHAR *buffer)
{
QMutexLocker sl(m_mutex);
if (m_theMemory==0) return;
assert(buffer);
map<UCHAR*, Chunkinfo>::iterator it = m_chunks.find(buffer);

if (it==m_chunks.end()) return;
int locks = it->second.m_locks;

assert(locks>0);

--it->second.m_locks;
}
*/

//------------------------------------------------------------------------------

UCHAR *TBigMemoryManager::getBuffer(UINT size) {
  if (m_theMemory == 0) return (UCHAR *)calloc(size, 1);

  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();
  UCHAR *buffer     = m_theMemory;
  TUINT32 chunkSize = 0;
  UCHAR *address    = 0;
  while (it != m_chunks.end()) {
    /*if (it->second.m_putInNormalMemory)
{it++; continue;}*/

    if ((TUINT32)((it->first) - (buffer + chunkSize)) >= size) {
      address = buffer + chunkSize;
      break;
    }

    buffer    = it->first;
    chunkSize = it->second.m_size;
    it++;
  }
  if (address) memset(address, 0x00, size);
  return address;
}

//---------------------------------------------------------------------------------

#ifdef _DEBUG

void TBigMemoryManager::getRasterInfo(int &rasterCount,
                                      TUINT32 &totRasterMemInKb,
                                      int &notCachedRasterCount,
                                      TUINT32 &notCachedRasterMemInKb) {
  totRasterMemInKb       = 0;
  notCachedRasterMemInKb = 0;
  notCachedRasterCount   = 0;
  rasterCount            = Rasters.size();

  std::set<TRaster *>::iterator it = Rasters.begin();
  while (it != Rasters.end()) {
    TRaster *r = *it;
    assert(r->m_parent == 0);
    totRasterMemInKb += r->getLx() * r->getLy() * r->getPixelSize() >> 10;
    if (!(*it)->m_cashed && r->getLy() > 1) {
      notCachedRasterCount++;
      notCachedRasterMemInKb +=
          r->getLx() * r->getLy() * r->getPixelSize() >> 10;
    }
    ++it;
  }
}

#endif

bool TBigMemoryManager::putRaster(TRaster *ras, bool canPutOnDisk) {
  if (!ras->m_parent && ras->m_buffer) {
#ifdef _DEBUG
    if (ras->m_bufferOwner) Rasters.insert(ras);
#endif
    return true;
  }

#ifdef _DEBUG
  checkConsistency();
#endif

  TUINT32 size = ras->getLx() * ras->getLy() * ras->getPixelSize();

  if (size == 0) {
    ras->m_buffer = 0;
    return true;
  }

  if (m_theMemory == 0)  // il bigmemorymanager e' inattivo
  {
    if (!ras->m_parent) {
      int sizeKB       = size >> 10;
      allocationPeakKB = std::max(allocationPeakKB, sizeKB);
      allocationSumKB += sizeKB;
      allocationCount++;
    }

    if (!ras->m_parent && !(ras->m_buffer = (UCHAR *)calloc(size, 1))) {
      // MessageBox( NULL, "Ouch!can't allocate!", "Warning", MB_OK);
      // non c'e' memoria; provo a comprimere
      /*TImageCache::instance()->doCompress(); 
    if (!(ras->m_buffer = (UCHAR *)malloc(size)))*/  // andata male pure cosi';
                                                     // metto tutto su disco
      // TImageCache::instance()->outputMap(size, "C:\\logCacheFailure");
      TINT64 availMemInKb = TSystem::getFreeMemorySize(true);
      if (availMemInKb > (size >> 10)) {
        // char str[1024];
        // sprintf_s(str, "Non alloco, ma : memoria (KB) richiesta : %d -
        // memoria disponibile : %d", size>>10, availMemInKb);
        // MessageBox( NULL, (LPCSTR)str, (LPCSTR)"Segmentation!", MB_OK);
      }
      ras->m_buffer = TImageCache::instance()->compressAndMalloc(size);

      if (!ras->m_buffer) {
        // char str[1024];
        // sprintf_s(str, "E' andata male: faccio il log della cache.");
        // MessageBox( NULL, (LPCSTR)str, (LPCSTR)"Segmentation!", MB_OK);
        TImageCache::instance()->outputMap(size, "C:\\logCacheTotalFailure");
      } else {
#ifdef _DEBUG
        m_totRasterMemInKb += size >> 10;
        Rasters.insert(ras);
#endif
      }
      return ras->m_buffer != 0;
    } else {
      if (!ras->m_parent) {
#ifdef _DEBUG
        m_totRasterMemInKb += size >> 10;
        Rasters.insert(ras);
#endif
      }
      return true;
    }
  }

  // il bigmemorymanager e' attivo
  TThread::MutexLocker sl(&m_mutex);

  /*
if (m_availableMemory<size && !ras->m_parent)
{
TImageCache::instance()->compressAndMalloc(size);
if (m_availableMemory>=size)
return TBigMemoryManager::instance()->putRaster(ras);
else
return (ras->m_buffer = (UCHAR *)malloc(size))!=0;


}*/

  if (ras->m_parent) {
    std::map<UCHAR *, Chunkinfo>::iterator it =
        m_chunks.find(ras->m_parent->m_buffer);

    if (it != m_chunks.end()) {
#ifdef _DEBUG
// assert(!it->second.m_rasters.empty());
// for (UINT i=0; i<it->second.m_rasters.size(); i++)
//  assert (it->second.m_rasters[i]!=ras);
#endif

      it->second.m_rasters.push_back(ras);
    }
#ifdef _DEBUG
    checkConsistency();
#endif
    return true;
  }

  UCHAR *address = 0;
  bool remapped  = false;

  assert(m_chunks.size() > 0);

  /*if (m_chunks.size()==1) //c'e' solo l'elemento che marca la fine del
bufferone
{
if ((TUINT32)(m_chunks.begin()->first-m_theMemory)>=size)
address = m_theMemory;
}
else
{*/

  address = getBuffer(size);

#ifdef _DEBUG
  checkConsistency();
#endif

  if (address == 0 && m_availableMemory >= size) {
    address  = remap(size);
    remapped = true;
#ifdef _DEBUG
    checkConsistency();
#endif
  } else if (address == 0) {
    printLog(size);
    // assert(!"la bigmemory e' piena...scritto log");
  }
  //}
  if (address == 0) {
    if (canPutOnDisk)
      address = TImageCache::instance()->compressAndMalloc(size);
    if (address == 0) return (ras->m_buffer = (UCHAR *)calloc(size, 1)) != 0;
  }

  // assert(address);

  ras->m_buffer     = address;
  m_chunks[address] = Chunkinfo(size, ras);
  assert(m_availableMemory >= size);
  m_availableMemory -= size;
#ifdef _DEBUG
  checkConsistency();
#endif
  return true;
}

//------------------------------------------------------------------------------

TRaster *TBigMemoryManager::findRaster(TRaster *ras) {
  // return 0;

  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();
  while (it != m_chunks.end()) {
    for (UINT i = 0; i < it->second.m_rasters.size(); i++)
      if (it->second.m_rasters[i] == ras) return ras;
    it++;
  }
  return 0;
}

//------------------------------------------------------------------------------
#ifdef _DEBUG

void TBigMemoryManager::printMap() {
  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();
  TSystem::outputDebug("BIGMEMORY chunks totali: " +
                       std::to_string((int)m_chunks.size()) + "\n");

  int count = 0;

  while (it != m_chunks.end()) {
    TSystem::outputDebug(
        "chunk #" + std::to_string((int)count++) + "dimensione(kb):" +
        std::to_string((int)(it->second.m_size >> 10)) + "num raster:" +
        std::to_string((int)(it->second.m_rasters.size())) + "\n");

    it++;
  }
}

#endif

bool TBigMemoryManager::releaseRaster(TRaster *ras) {
  TThread::MutexLocker sl(&m_mutex);
  UCHAR *buffer = (ras->m_parent) ? (ras->m_parent->m_buffer) : (ras->m_buffer);
  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.find(buffer);

  if (m_theMemory == 0 || it == m_chunks.end()) {
    assert(buffer);
    if (!ras->m_parent && ras->m_bufferOwner) {
      free(buffer);
#ifdef _DEBUG
      m_totRasterMemInKb -=
          (ras->getPixelSize() * ras->getLx() * ras->getLy()) >> 10;
      Rasters.erase(ras);
#endif
    }

    // assert(findRaster(ras)==0);

    return false;
  }

  assert(ras->m_lockCount == 0);

  if (it->second.m_rasters.size() >
      1)  // non e' il solo raster ad usare il buffer; non libero
  {
    std::vector<TRaster *>::iterator it2 = it->second.m_rasters.begin();
    for (; it2 != it->second.m_rasters.end(); ++it2) {
      if (ras == *it2) {
        it->second.m_rasters.erase(it2);

#ifdef _DEBUG
// assert(!it->second.m_rasters.empty());
// assert(findRaster(ras)==0);
#endif

        return true;
      }
    }
    assert(false);
    return false;
  } else if (ras->m_bufferOwner)  // libero!
  {
#ifdef _DEBUG
    checkConsistency();
#endif
    /*if (it->second.m_putInNormalMemory && ras->m_bufferOwner)
    free(it->first);
else */
    m_availableMemory += it->second.m_size;
    m_chunks.erase(it);
  }

#ifdef _DEBUG
  // assert(findRaster(ras)==0);
  checkConsistency();
#endif

  return true;
}
//------------------------------------------------------------------------------

void TBigMemoryManager::checkConsistency() {
  return;

  // QMutexLocker sl(m_mutex);

  int count = 0;
  // int size = m_chunks.size();
  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();
  UCHAR *endAddress = m_theMemory;
  TUINT32 freeMem = 0, allocMem = 0;

  while (it != m_chunks.end()) {
    count++;
    // assert(it->second.m_rasters.size()==0 || it->second.m_rasters.size()>0);

    if (endAddress != 0 /*&& !it->second.m_putInNormalMemory*/) {
      freeMem += (TUINT32)(it->first - endAddress);
      allocMem += it->second.m_size;
    }
    assert(endAddress <= it->first);
    endAddress = it->first + it->second.m_size;
    for (UINT i = 0; i < it->second.m_rasters.size(); i++) {
      TRaster *ras            = it->second.m_rasters[i];
      it->second.m_rasters[i] = 0;  // ogni raster deve apparire una sola volta
      assert(findRaster(ras) == 0);
      it->second.m_rasters[i] = ras;

      UCHAR *buf1 = (ras->m_parent) ? ras->m_parent->m_buffer : ras->m_buffer;
      UCHAR *buf2 = it->first;
      assert(buf1 == buf2);
      UINT size;
      if (ras->m_parent)
        size = ras->m_parent->getLx() * ras->m_parent->getLy() *
               ras->m_parent->getPixelSize();
      else
        size = ras->getLx() * ras->getLy() * ras->getPixelSize();
      assert(size == it->second.m_size);
    }
    it++;
  }

  if (m_theMemory) {
    assert(allocMem + m_availableMemory == m_allocatedMemory);

    assert(freeMem == m_availableMemory);
  }
}

//------------------------------------------------------------------------------

std::map<UCHAR *, Chunkinfo>::iterator TBigMemoryManager::shiftBlock(
    const std::map<UCHAR *, Chunkinfo>::iterator &it, TUINT32 offset) {
  UCHAR *newAddress = it->first - offset;

  if (offset > it->second.m_size)
    memcpy(newAddress, it->first, it->second.m_size);  // se NON overlappano.
  else
    memmove(newAddress, it->first, it->second.m_size);  // se overlappano.

  m_chunks[newAddress] = Chunkinfo(it->second.m_size, it->second.m_rasters[0]);
  std::map<UCHAR *, Chunkinfo>::iterator it1 = m_chunks.find(newAddress);

  assert(it1->first < it1->second.m_rasters[0]->m_buffer);
  UINT i = 0;
  for (i = 0; i < it->second.m_rasters.size();
       i++)  // prima rimappo i subraster, senza toccare il buffer del parent...
  {
    TRaster *ras = it->second.m_rasters[i];
    assert(i > 0 || !ras->m_parent);
    if (!ras->m_parent) continue;
    assert(ras->m_parent->m_buffer == it->first);
    ras->remap(newAddress);
    if (i > 0) it1->second.m_rasters.push_back(ras);
  }

  it->second.m_rasters[0]->remap(newAddress);  // ORA rimappo il parent

#ifdef _DEBUG
  for (i = 1; i < it->second.m_rasters.size(); i++)  //..poi i raster padri
  {
    // TRaster*ras = it->second.m_rasters[i];

    assert(it->second.m_rasters[i]->m_parent);
    // ras->remap(newAddress);
    // if (i>0)
    //  it1->second.m_rasters.push_back(ras);
  }
  assert(it1->second.m_rasters.size() == it->second.m_rasters.size());
#endif

  m_chunks.erase(it);
  it1 = m_chunks.find(newAddress);  // non dovrebbe servire, ma per prudenza...
  assert(it1->first == it1->second.m_rasters[0]->m_buffer);
  return it1;
}

//------------------------------------------------------------------------------

UCHAR *TBigMemoryManager::remap(TUINT32 size)  // size==0 -> remappo tutto
{
  bool locked = false;
// QMutexLocker sl(m_mutex); //gia' scopata

#ifdef _DEBUG
  checkConsistency();
#endif
  UINT i;
  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();

  try {
    UCHAR *buffer     = m_theMemory;
    TUINT32 chunkSize = 0;
    while (it != m_chunks.end()) {
      /*if (it->second.m_putInNormalMemory)
{it++; continue;}*/

      TUINT32 gap = (TUINT32)((it->first) - (buffer + chunkSize));
      if (size > 0 && gap >= size)  // trovato chunk sufficiente
        return buffer + chunkSize;
      else if (gap > 0 && it->second.m_size > 0)  // c'e' un frammento di
                                                  // memoria, accorpo; ma solo
                                                  // se non sto in fondo
      {
        std::vector<TRaster *> &rasters = it->second.m_rasters;
        assert(rasters[0]->m_parent == 0);

        // devo controllare il lockCount solo sul parent, la funzione lock()
        // locka solo il parent;
        for (i = 0; i < rasters.size(); i++) rasters[i]->beginRemapping();

        if (rasters[0]->m_lockCount == 0)
          it = shiftBlock(it, gap);
        else
          locked = true;

        for (i = 0; i < rasters.size(); i++) rasters[i]->endRemapping();

        // rasters.clear();
      }

      buffer    = it->first;
      chunkSize = it->second.m_size;
      it++;
    }

  } catch (...) {
    for (i = 0; i < it->second.m_rasters.size(); i++)
      it->second.m_rasters[i]->endRemapping();
  }

  if (size >
      0)  // e' andata male...non liberato un blocco di grandezza sufficiente
  {
    printLog(size);
    assert(!"Niente memoria! scritto log");
    if (!locked)
      assert(false);  // se entro nella remap, di sicuro c'e' 'size'  memoria
                      // disponibile; basta deframmentarla
  }
  return 0;
}

//------------------------------------------------------------------------------

void TBigMemoryManager::printLog(TUINT32 size) {
  TFilePath fp("C:\\memorymaplog.txt");
  Tofstream os(fp);

  os << "memoria totale: " << m_allocatedMemory / 1024 << " KB\n";
  os << "memoria richiesta: " << size / 1024 << " KB\n";
  os << "memoria libera: " << m_availableMemory / 1024 << " KB\n\n\n";

  std::map<UCHAR *, Chunkinfo>::iterator it = m_chunks.begin();
  UCHAR *buffer  = m_theMemory;
  UINT chunkSize = 0;
  for (; it != m_chunks.end(); it++) {
    TUINT32 gap = (TUINT32)((it->first) - (buffer + chunkSize));
    if (gap > 0) os << "- gap di " << gap / 1024 << " KB\n";
    if (it->second.m_size > 0)
      os << "- raster di " << it->second.m_size / 1024 << " KB"
         << ((it->second.m_rasters[0]->m_lockCount > 0) ? " LOCCATO!\n" : "\n");
    buffer    = it->first;
    chunkSize = it->second.m_size;
  }
}
