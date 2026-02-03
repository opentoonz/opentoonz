

#ifdef _WIN32
#ifndef UNICODE
#define UNICODE
#endif
#endif

#include "tbigmemorymanager.h"
#include "traster.h"
#include "trastercm.h"
// #include "tspecialstyleid.h"
#include "tpixel.h"
#include "tpixelgr.h"
#include "timagecache.h"

DEFINE_CLASS_CODE(TRaster, 1)

//------------------------------------------------------------

TRaster::TRaster(int lx, int ly, int pixelSize)
    : TSmartObject(m_classCode)
    , m_pixelSize(pixelSize)
    , m_lx(lx)
    , m_ly(ly)
    , m_wrap(lx)
    , m_parent(0)
    , m_bufferOwner(true)
    , m_buffer(0)
    , m_lockCount(0)
    , m_isLinear(false)
#ifdef _DEBUG
    , m_cashed(false)
#endif

{
  // try
  {
    assert(pixelSize > 0);
    assert(lx > 0 && ly > 0);
    TBigMemoryManager::instance()->putRaster(this);

    // m_buffer = new UCHAR[lx*ly*pixelSize];

    if (!m_buffer) {
#ifdef _WIN32
      static bool firstTime = true;
      if (firstTime) {
        firstTime          = false;
        unsigned long size = pixelSize * lx * ly;
        TImageCache::instance()->outputMap(size, "C:\\runout");
        (*TBigMemoryManager::instance()->m_runOutCallback)(size);
      }
#endif
      return;
    }

    // TBigMemoryManager::instance()->checkConsistency();

    // m_totalMemory += ((lx*ly*pixelSize)>>10);
  }
  /* catch(...)
{
TImageCache::instance()->putAllOnDisk();
m_buffer = BigMemoryManager.getMemoryChunk(lx*ly*pixelSize, this);
//m_buffer = new UCHAR[lx*ly*pixelSize];
m_totalMemory += ((lx*ly*pixelSize)>>10);
#ifdef _WIN32
MessageBox( NULL, "Run out of contiguous physical memory: please save all and
restart toonz!", "Warning", MB_OK);
#endif
}*/
}

//------------------------------------------------------------

TRaster::TRaster(int lx, int ly, int pixelSize, int wrap, UCHAR *buffer,
                 TRaster *parent, bool bufferOwner)
    : TSmartObject(m_classCode)
    , m_pixelSize(pixelSize)
    , m_lx(lx)
    , m_ly(ly)
    , m_wrap(wrap)
    , m_buffer(buffer)
    , m_bufferOwner(bufferOwner)
    , m_lockCount(0)
    , m_isLinear(false)
#ifdef _DEBUG
    , m_cashed(false)
#endif
    , m_parent(nullptr)

{
  if (parent) {
    assert(bufferOwner == false);
    while (parent->m_parent) parent = parent->m_parent;
    parent->addRef();
    setLinear(parent->isLinear());
  }
#ifdef _DEBUG
  else if (bufferOwner)
    TBigMemoryManager::instance()->m_totRasterMemInKb +=
        ((m_lx * m_ly * m_pixelSize) >> 10);
#endif

  m_parent = parent;

  assert(pixelSize > 0);
  assert(lx > 0 && ly > 0);
  assert(wrap >= lx);
  assert(m_buffer);
  // if (parent)
  TBigMemoryManager::instance()->putRaster(this);

  //  TBigMemoryManager::instance()->checkConsistency();
}

//------------------------------------------------------------

// TAtomicVar TRaster::m_totalMemory;

//------------------------------------------------------------

// unsigned long TRaster::getTotalMemoryInKB(){ return m_totalMemory;}

//------------------------------------------------------------

TRaster::~TRaster() {
  bool parent = false;
#ifdef _DEBUG
// TBigMemoryManager::instance()->checkConsistency();
#endif
  // bool ret =
  TBigMemoryManager::instance()->releaseRaster(this);
#ifdef _DEBUG
// TBigMemoryManager::instance()->checkConsistency();
#endif
  if (m_parent) {
    assert(!m_bufferOwner);
    m_parent->release();
    m_parent = 0;
    parent   = true;
  }

  // if(m_buffer && m_bufferOwner)
  //  {
  // delete [] m_buffer;
  // m_totalMemory += -((m_lx*m_ly*m_pixelSize)>>10);
  // assert(m_totalMemory>=0);
  //  }
  //  UCHAR* aux = m_buffer;
  m_buffer = 0;

#ifdef _DEBUG
// TBigMemoryManager::instance()->checkConsistency();
#endif
}

//------------------------------------------------------------
void TRaster::beginRemapping() { m_mutex.lock(); }

void TRaster::endRemapping() { m_mutex.unlock(); }

/*
 void TRaster::lock()
   {
   if (m_parent) m_parent->lock();
   else  ++m_lockCount;
   //TBigMemoryManager::instance()->lock(m_parent?(m_parent->m_buffer):m_buffer);
   }

 void TRaster::unlock()
   {
   if (m_parent) m_parent->unlock();
   else
     {
     assert(m_lockCount>0);
     --m_lockCount;
     }

   //TBigMemoryManager::instance()->unlock(m_parent?(m_parent->m_buffer):m_buffer);
   }
*/

/*
template<class T>
  TRasterT<T>::TRasterT<T>(int lx, int ly) : TRaster(lx,ly,sizeof(T)) {}

  // utilizzo di un raster preesistente
  template<class T>
  TRasterT<T>::TRasterT<T>(int lx, int ly, int wrap, T *buffer, TRasterT<T>
*parent)
     : TRaster(lx,ly,sizeof(T), wrap
     , reinterpret_cast<UCHAR*>(buffer), parent) {}
*/
//------------------------------------------------------------

void TRaster::fillRawData(const UCHAR *color) {
  if (m_lx == 0 || m_ly == 0) return;

  // N.B. uso la convenzione stl per end()

  const int wrapSize = m_wrap * m_pixelSize;
  const int rowSize  = m_lx * m_pixelSize;
  UCHAR *buf1        = m_parent ? m_parent->m_buffer : m_buffer;
  lock();
  unsigned char *firstPixel = getRawData();
  const unsigned char *lastPixel =
      firstPixel + wrapSize * (m_ly - 1) + m_pixelSize * (m_lx - 1);

  // riempio la prima riga
  unsigned char *pixel          = firstPixel;
  const unsigned char *endpixel = firstPixel + rowSize;
  while (pixel < endpixel) {
    assert(firstPixel <= pixel && pixel <= lastPixel);
    ::memcpy(pixel, color, m_pixelSize);
    pixel += m_pixelSize;
  }

  // riempio le altre
  pixel += wrapSize - rowSize;
  const unsigned char *endrow = pixel + wrapSize * (m_ly - 1);
  while (pixel < endrow) {
    assert(firstPixel <= pixel && pixel + rowSize - m_pixelSize <= lastPixel);
    ::memcpy(pixel, firstPixel, rowSize);
    pixel += wrapSize;
  }
  UCHAR *buf2 = m_parent ? m_parent->m_buffer : m_buffer;
  unlock();
}

//------------------------------------------------------------

void TRaster::fillRawDataOutside(const TRect &rect,
                                 const unsigned char *pixel) {
  if (m_lx == 0 || m_ly == 0) return;
  TRect r = rect * getBounds();
  if (r.isEmpty()) return;

  if (r.y0 > 0)  // fascia "inferiore"
  {
    TRect bounds(0, 0, m_lx - 1, r.y0 - 1);
    extract(bounds)->fillRawData(pixel);
  }

  if (rect.y1 < m_ly - 1)  // fascia "superiore"
  {
    TRect bounds(0, r.y1 + 1, m_lx - 1, m_ly - 1);
    extract(bounds)->fillRawData(pixel);
  }

  if (rect.x0 > 0)  // zona "a sinistra"
  {
    TRect bounds(0, r.y0, r.x0 - 1, r.y1);
    extract(bounds)->fillRawData(pixel);
  }

  if (rect.x1 < m_lx - 1)  // zona "a destra"
  {
    TRect bounds(r.x1 + 1, r.y0, m_lx - 1, r.y1);
    extract(bounds)->fillRawData(pixel);
  }
}

//------------------------------------------------------------

void TRaster::copy(const TRasterP &src0, const TPoint &offset) {
  assert(m_pixelSize == src0->getPixelSize());
  TRect rect = getBounds() * (src0->getBounds() + offset);
  if (rect.isEmpty()) return;
  TRasterP dst = extract(rect);
  TRect r(rect);
  r -= offset;
  // TRasterP src = src0->extract(rect - offset);
  TRasterP src = src0->extract(r);
  assert(dst->getSize() == src->getSize());
  dst->lock();
  src0->lock();
  if (dst->getLx() == dst->getWrap() && src->getLx() == src->getWrap()) {
    int size = rect.getLx() * rect.getLy() * m_pixelSize;
    ::memcpy(dst->getRawData(), src->getRawData(), size);
  } else {
    int rowSize         = dst->getLx() * m_pixelSize;
    int srcWrapSize     = src->getWrap() * m_pixelSize;
    int dstWrapSize     = dst->getWrap() * m_pixelSize;
    const UCHAR *srcRow = src->getRawData();
    UCHAR *dstRow       = dst->getRawData();
    UCHAR *maxDstRow    = dstRow + dstWrapSize * dst->getLy();
    while (dstRow < maxDstRow) {
      ::memcpy(dstRow, srcRow, rowSize);
      dstRow += dstWrapSize;
      srcRow += srcWrapSize;
    }
  }
  setLinear(src0->isLinear());
  dst->unlock();
  src0->unlock();
}

//------------------------------------------------------------

void TRaster::yMirror() {
  const int rowSize  = m_lx * m_pixelSize;
  const int wrapSize = m_wrap * m_pixelSize;
  std::unique_ptr<UCHAR[]> auxBuf(new UCHAR[rowSize]);
  lock();
  UCHAR *buff1 = getRawData();
  UCHAR *buff2 = getRawData(0, (m_ly - 1));
  while (buff1 < buff2) {
    ::memcpy(auxBuf.get(), buff1, rowSize);
    ::memcpy(buff1, buff2, rowSize);
    ::memcpy(buff2, auxBuf.get(), rowSize);
    buff1 += wrapSize;
    buff2 -= wrapSize;
  }
  unlock();
}

//------------------------------------------------------------

void TRaster::xMirror() {
  const int wrapSize        = m_wrap * m_pixelSize;
  const int lastPixelOffset = (m_lx - 1) * m_pixelSize;
  std::unique_ptr<UCHAR[]> auxBuf(new UCHAR[m_pixelSize]);
  lock();
  UCHAR *row = getRawData();
  for (int i = 0; i < m_ly; i++) {
    UCHAR *a = row, *b = row + lastPixelOffset;
    while (a < b) {
      ::memcpy(auxBuf.get(), a, m_pixelSize);
      ::memcpy(a, b, m_pixelSize);
      ::memcpy(b, auxBuf.get(), m_pixelSize);
      a += m_pixelSize;
      b -= m_pixelSize;
    }
    row += wrapSize;
  }
  unlock();
}

//------------------------------------------------------------

void TRaster::rotate180() {
  // const int rowSize = m_lx * m_pixelSize;
  const int wrapSize = m_wrap * m_pixelSize;
  std::unique_ptr<UCHAR[]> auxBuf(new UCHAR[m_pixelSize]);
  lock();
  UCHAR *buff1 = getRawData();
  UCHAR *buff2 = buff1 + wrapSize * (m_ly - 1) + m_pixelSize * (m_lx - 1);
  if (m_wrap == m_lx) {
    while (buff1 < buff2) {
      ::memcpy(auxBuf.get(), buff1, m_pixelSize);
      ::memcpy(buff1, buff2, m_pixelSize);
      ::memcpy(buff2, auxBuf.get(), m_pixelSize);
      buff1 += m_pixelSize;
      buff2 -= m_pixelSize;
    }
  } else {
    for (int y = 0; y < m_ly / 2; y++) {
      UCHAR *a = buff1, *b = buff2;
      for (int x = 0; x < m_lx; x++) {
        ::memcpy(auxBuf.get(), a, m_pixelSize);
        ::memcpy(a, b, m_pixelSize);
        ::memcpy(b, auxBuf.get(), m_pixelSize);
        a += m_pixelSize;
        b -= m_pixelSize;
      }
      buff1 += wrapSize;
      buff2 -= wrapSize;
    }
  }
  unlock();
}

//------------------------------------------------------------

void TRaster::rotate90() {
  /*
  UCHAR *auxBuf= new UCHAR[m_pixelSize];


for(int y=m_ly;y>0;y--)
{
  UCHAR *a = getRawData() + wrapSize * (y-1) + m_pixelSize * (m_lx-1);
for (int x=m_lx-1;x>=0;x--)
  {
    UCHAR *b = a - (m_ly-1)*m_pixelSize *(m_lx-x);
    ::memcpy(auxBuf,  a,   m_pixelSize);
    ::memcpy(a,       b,   m_pixelSize);
    ::memcpy(b,    auxBuf, m_pixelSize);
    a-=m_pixelSize;
  }

}
*/
}

//------------------------------------------------------------

void TRaster::clear() {
  TRasterCM32 *ras = dynamic_cast<TRasterCM32 *>(this);
  if (ras) {
    // ras->fill(TPixelCM32(0,BackgroundStyle,TPixelCM32::getMaxTone()));
    ras->fill(TPixelCM32());
  } else {
    const int rowSize = getRowSize();
    lock();
    if (m_wrap == m_lx) {
      int bufferSize = rowSize * m_ly;
      memset(getRawData(), 0, bufferSize);
    } else
      for (int y = m_ly - 1; y >= 0; y--) {
        UCHAR *buffer = getRawData(0, y);
        memset(buffer, 0, rowSize);
      }
    unlock();
  }
}

//------------------------------------------------------------

void TRaster::remap(UCHAR *newLocation) {
  if (m_parent) {
    assert(m_parent->m_buffer > newLocation);

    assert(m_parent->m_parent == 0);

    int offset = (int)(m_buffer - m_parent->m_buffer);
    assert(offset >= 0);

    // m_parent->remap(newLocation);
    m_buffer = newLocation + offset;
  } else {
    assert(m_buffer > newLocation);
    m_buffer = newLocation;
  }
}

//------------------------------------------------------------

void TRaster::clearOutside(const TRect &rect) {
  if (m_lx == 0 || m_ly == 0) return;
  TRect r = rect * getBounds();
  if (r.isEmpty()) return;

  if (r.y0 > 0)  // fascia "inferiore"
  {
    TRect bounds(0, 0, m_lx - 1, r.y0 - 1);
    extract(bounds)->clear();
  }

  if (rect.y1 < m_ly - 1)  // fascia "superiore"
  {
    TRect bounds(0, r.y1 + 1, m_lx - 1, m_ly - 1);
    extract(bounds)->clear();
  }

  if (rect.x0 > 0)  // zona "a sinistra"
  {
    TRect bounds(0, r.y0, r.x0 - 1, r.y1);
    extract(bounds)->clear();
  }

  if (rect.x1 < m_lx - 1)  // zona "a destra"
  {
    TRect bounds(r.x1 + 1, r.y0, m_lx - 1, r.y1);
    extract(bounds)->clear();
  }
}
