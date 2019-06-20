

#ifndef _DEBUG
#undef _STLP_DEBUG
#else
#define _STLP_DEBUG 1

#endif

#ifdef TNZCORE_LIGHT
#ifdef _DEBUGTOONZ
#undef _DEBUGTOONZ
#endif
#else
#ifdef _DEBUG
#define _DEBUGTOONZ _DEBUG
#endif
#endif

#include "timagecache.h"
#include "trasterimage.h"
#ifndef TNZCORE_LIGHT
#include "tvectorimage.h"
#include "trastercm.h"
#include "tropcm.h"
#endif

#include "tcodec.h"
#include "tfilepath_io.h"
#include "tconvert.h"
#include "tsystem.h"

#include "traster.h"

//#include "tstopwatch.h"
#include "tbigmemorymanager.h"

#include "tstream.h"
#include "tenv.h"
#include <deque>
#include <numeric>
#include <sstream>
#ifdef _WIN32
#include <crtdbg.h>
#endif

// Qt includes
#include <QThreadStorage>

//------------------------------------------------------------------------------

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class ImageBuilder;
class ImageInfo;

// std::ofstream os("C:\\cache.txt");

TUINT32 HistoryCount = 0;
//------------------------------------------------------------------------------

class TheCodec final : public TRasterCodecLz4 {
public:
  static TheCodec *instance() {
    if (!_instance) _instance = new TheCodec();

    return _instance;
  }

  void reset() {
    if (_instance) _instance->TRasterCodecLz4::reset();
  }

private:
  static TheCodec *_instance;
  TheCodec() : TRasterCodecLz4("Lz4_Codec", false) {}
};

TheCodec *TheCodec::_instance = 0;

//------------------------------------------------------------------------------

class CacheItem : public TSmartObject {
  DECLARE_CLASS_CODE
public:
  CacheItem()
      : m_cantCompress(false)
      , m_builder(0)
      , m_imageInfo(0)
      , m_modified(false) {}

  CacheItem(ImageBuilder *builder, ImageInfo *imageInfo)
      : m_cantCompress(false)
      , m_builder(builder)
      , m_imageInfo(imageInfo)
      , m_historyCount(0)
      , m_modified(false) {}

  virtual ~CacheItem() {}

  virtual TUINT32 getSize() const = 0;

  // getImage restituisce un'immagine non compressa
  virtual TImageP getImage() const = 0;

  bool m_cantCompress;
  ImageBuilder *m_builder;
  ImageInfo *m_imageInfo;
  std::string m_id;
  TUINT32 m_historyCount;
  bool m_modified;
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<CacheItem>;
#endif
typedef TSmartPointerT<CacheItem> CacheItemP;

DEFINE_CLASS_CODE(CacheItem, 101)

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class ImageInfo {
public:
  TDimension m_size;
  ImageInfo(const TDimension &size) : m_size(size) {}
  virtual ~ImageInfo() {}
  virtual ImageInfo *clone() = 0;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class ImageBuilder {
public:
  virtual ~ImageBuilder() {}
  virtual ImageBuilder *clone() = 0;
  virtual TImageP build(ImageInfo *info, const TRasterP &ras) = 0;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class RasterImageInfo final : public ImageInfo {
public:
  RasterImageInfo(const TRasterImageP &ri);

  void setInfo(const TRasterImageP &ri);

  ImageInfo *clone() override;

  double m_dpix, m_dpiy;
  std::string m_name;
  TRect m_savebox;
  bool m_isOpaque;
  TPoint m_offset;
  int m_subs;
};

RasterImageInfo::RasterImageInfo(const TRasterImageP &ri)
    : ImageInfo(ri->getRaster()->getSize()) {
  ri->getDpi(m_dpix, m_dpiy);
  m_name     = ri->getName();
  m_savebox  = ri->getSavebox();
  m_isOpaque = ri->isOpaque();
  m_offset   = ri->getOffset();
  m_subs     = ri->getSubsampling();
}

void RasterImageInfo::setInfo(const TRasterImageP &ri) {
  ri->setDpi(m_dpix, m_dpiy);
  ri->setName(m_name);
  ri->setSavebox(m_savebox);
  ri->setOpaqueFlag(m_isOpaque);
  ri->setOffset(m_offset);
  ri->setSubsampling(m_subs);
}

ImageInfo *RasterImageInfo::clone() { return new RasterImageInfo(*this); }

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef TNZCORE_LIGHT

#include "tpalette.h"
#include "ttoonzimage.h"

class ToonzImageInfo final : public ImageInfo {
public:
  ToonzImageInfo(const TToonzImageP &ti);
  ~ToonzImageInfo() {
    if (m_palette) m_palette->release();
  }

  ImageInfo *clone() override {
    ToonzImageInfo *ret = new ToonzImageInfo(*this);
    if (ret->m_palette) ret->m_palette->addRef();
    return ret;
  }

  void setInfo(const TToonzImageP &ti);

  double m_dpix, m_dpiy;
  std::string m_name;
  TRect m_savebox;
  TPoint m_offset;
  int m_subs;
  TPalette *m_palette;
};

ToonzImageInfo::ToonzImageInfo(const TToonzImageP &ti)
    : ImageInfo(ti->getSize()) {
  m_palette = ti->getPalette();
  if (m_palette) m_palette->addRef();

  ti->getDpi(m_dpix, m_dpiy);
  m_savebox = ti->getSavebox();
  m_offset  = ti->getOffset();
  m_subs    = ti->getSubsampling();
}

void ToonzImageInfo::setInfo(const TToonzImageP &ti) {
  ti->setPalette(m_palette);
  ti->setDpi(m_dpix, m_dpiy);
  ti->setOffset(m_offset);
  ti->setSubsampling(m_subs);
}

#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

class RasterImageBuilder final : public ImageBuilder {
public:
  ImageBuilder *clone() override { return new RasterImageBuilder(*this); }

  TImageP build(ImageInfo *info, const TRasterP &ras) override;
};

TImageP RasterImageBuilder::build(ImageInfo *info, const TRasterP &ras) {
  RasterImageInfo *riInfo = dynamic_cast<RasterImageInfo *>(info);
  assert(riInfo);

  int rcount       = ras->getRefCount();
  TRasterImageP ri = new TRasterImage();
#ifdef _DEBUGTOONZ
  ras->m_cashed = true;
#endif
  ri->setRaster(ras);
  riInfo->setInfo(ri);
  assert(ras->getRefCount() > rcount);
  return ri;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef TNZCORE_LIGHT

class ToonzImageBuilder final : public ImageBuilder {
public:
  ImageBuilder *clone() override { return new ToonzImageBuilder(*this); }

  TImageP build(ImageInfo *info, const TRasterP &ras) override;
};

TImageP ToonzImageBuilder::build(ImageInfo *info, const TRasterP &ras) {
  ToonzImageInfo *tiInfo = dynamic_cast<ToonzImageInfo *>(info);
  assert(tiInfo);

  TRasterCM32P rasCM32 = ras;
  assert(rasCM32);

  TRasterCM32P imgRasCM32;

  assert(TRect(tiInfo->m_size).contains(tiInfo->m_savebox));

  if (ras->getSize() != tiInfo->m_size) {
    TRasterCM32P fullRas(tiInfo->m_size);
    TRect rectToExtract(tiInfo->m_savebox);
    TPixelCM32 bgColor;
    fullRas->fillOutside(tiInfo->m_savebox, bgColor);
    fullRas->extractT(rectToExtract)->copy(ras);
    assert(rectToExtract == tiInfo->m_savebox);
    imgRasCM32 = fullRas;
  } else
    imgRasCM32 = rasCM32;
#ifdef _DEBUG
  imgRasCM32->m_cashed = true;
#endif
  TToonzImageP ti = new TToonzImage(imgRasCM32, tiInfo->m_savebox);
  tiInfo->setInfo(ti);
  return ti;
}
#endif

//------------------------------------------------------------------------------

class UncompressedOnMemoryCacheItem final : public CacheItem {
public:
  UncompressedOnMemoryCacheItem(const TImageP &image) : m_image(image) {
    TRasterImageP ri = m_image;

    if (ri) m_imageInfo = new RasterImageInfo(ri);
#ifndef TNZCORE_LIGHT
    else {
      TToonzImageP ti = m_image;
      if (ti)
        m_imageInfo = new ToonzImageInfo(ti);
      else
        m_imageInfo = 0;
    }
#else
    else
      m_imageInfo = 0;
#endif
  }

  ~UncompressedOnMemoryCacheItem() {
    if (m_imageInfo) delete m_imageInfo;
  }

  TUINT32 getSize() const override;
  TImageP getImage() const override { return m_image; }

  TImageP m_image;
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<UncompressedOnMemoryCacheItem>;
template class DVAPI
    TDerivedSmartPointerT<UncompressedOnMemoryCacheItem, CacheItem>;
#endif
typedef TDerivedSmartPointerT<UncompressedOnMemoryCacheItem, CacheItem>
    UncompressedOnMemoryCacheItemP;

//------------------------------------------------------------------------------

TUINT32 UncompressedOnMemoryCacheItem::getSize() const {
  TRasterImageP ri = m_image;
  if (ri) {
    TRasterP ras = ri->getRaster();
    if (ras)
      return ras->getLy() * ras->getRowSize();
    else
      return 0;
  } else {
#ifndef TNZCORE_LIGHT
    TToonzImageP ti = m_image;
    if (ti) {
      TDimension size = ti->getSize();
      return size.lx * size.ly * sizeof(TPixelCM32);
    }
#endif
  }

  return 0;
}

//------------------------------------------------------------------------------

class CompressedOnMemoryCacheItem final : public CacheItem {
public:
  CompressedOnMemoryCacheItem(const TImageP &img);

  CompressedOnMemoryCacheItem(const TRasterP &compressedRas,
                              ImageBuilder *builder, ImageInfo *info);

  ~CompressedOnMemoryCacheItem();

  TUINT32 getSize() const override;
  TImageP getImage() const override;

  TRasterP m_compressedRas;
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<CompressedOnMemoryCacheItem>;
template class DVAPI
    TDerivedSmartPointerT<CompressedOnMemoryCacheItem, CacheItem>;
#endif
typedef TDerivedSmartPointerT<CompressedOnMemoryCacheItem, CacheItem>
    CompressedOnMemoryCacheItemP;

//------------------------------------------------------------------------------

CompressedOnMemoryCacheItem::CompressedOnMemoryCacheItem(const TImageP &img)
    : m_compressedRas() {
  TRasterImageP ri = img;
  if (ri) {
    m_imageInfo     = new RasterImageInfo(ri);
    m_builder       = new RasterImageBuilder();
    TINT32 buffSize = 0;
    m_compressedRas =
        TheCodec::instance()->compress(ri->getRaster(), 1, buffSize);
  }
#ifndef TNZCORE_LIGHT
  else {
    TToonzImageP ti = img;
    if (ti) {
      m_imageInfo          = new ToonzImageInfo(ti);
      m_builder            = new ToonzImageBuilder();
      TRasterCM32P rasCM32 = ti->getRaster();
      TINT32 buffSize      = 0;
      m_compressedRas = TheCodec::instance()->compress(rasCM32, 1, buffSize);
    } else
      assert(false);
  }
#else
  else
    assert(false);
#endif
}

//------------------------------------------------------------------------------

CompressedOnMemoryCacheItem::CompressedOnMemoryCacheItem(const TRasterP &ras,
                                                         ImageBuilder *builder,
                                                         ImageInfo *info)
    : CacheItem(builder, info), m_compressedRas(ras) {}

//------------------------------------------------------------------------------

CompressedOnMemoryCacheItem::~CompressedOnMemoryCacheItem() {
  delete m_imageInfo;
}

//------------------------------------------------------------------------------

TUINT32 CompressedOnMemoryCacheItem::getSize() const {
  if (m_compressedRas)
    return m_compressedRas->getLx();
  else
    return 0;
}

//------------------------------------------------------------------------------

TImageP CompressedOnMemoryCacheItem::getImage() const {
  assert(m_compressedRas);

  // PER IL MOMENTO DISCRIMINO: DA ELIMINARE
  TRasterP ras;

  TheCodec::instance()->decompress(m_compressedRas, ras);
#ifdef _DEBUGTOONZ
  ras->m_cashed = true;
#endif
#ifndef TNZCORE_LIGHT
  ToonzImageBuilder *tibuilder = dynamic_cast<ToonzImageBuilder *>(m_builder);
  if (tibuilder)
    return tibuilder->build(m_imageInfo, ras);
  else
#endif
    return m_builder->build(m_imageInfo, ras);
}

//------------------------------------------------------------------------------

class CompressedOnDiskCacheItem final : public CacheItem {
public:
  CompressedOnDiskCacheItem(const TFilePath &fp, const TRasterP &compressedRas,
                            ImageBuilder *builder, ImageInfo *info);

  ~CompressedOnDiskCacheItem();

  TUINT32 getSize() const override { return 0; }
  TImageP getImage() const override;
  TFilePath m_fp;
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<CompressedOnDiskCacheItem>;
template class DVAPI
    TDerivedSmartPointerT<CompressedOnDiskCacheItem, CacheItem>;
#endif
typedef TDerivedSmartPointerT<CompressedOnDiskCacheItem, CacheItem>
    CompressedOnDiskCacheItemP;

//------------------------------------------------------------------------------

CompressedOnDiskCacheItem::CompressedOnDiskCacheItem(
    const TFilePath &fp, const TRasterP &compressedRas, ImageBuilder *builder,
    ImageInfo *info)
    : CacheItem(builder, info), m_fp(fp) {
  compressedRas->lock();

  Tofstream oss(m_fp);
  assert(compressedRas->getLy() == 1 && compressedRas->getPixelSize() == 1);
  TUINT32 size = compressedRas->getLx();
  oss.write((char *)&size, sizeof(TUINT32));
  oss.write((char *)compressedRas->getRawData(), size);
  assert(!oss.fail());

  compressedRas->unlock();
}

//------------------------------------------------------------------------------

CompressedOnDiskCacheItem::~CompressedOnDiskCacheItem() {
  delete m_imageInfo;
  TSystem::deleteFile(m_fp);
}

//------------------------------------------------------------------------------

TImageP CompressedOnDiskCacheItem::getImage() const {
  Tifstream is(m_fp);
  TUINT32 dataSize;
  is.read((char *)&dataSize, sizeof(TUINT32));
  TRasterGR8P ras(dataSize, 1);
  ras->lock();
  UCHAR *data = ras->getRawData();
  is.read((char *)data, dataSize);
  assert(!is.fail());
  ras->unlock();
  CompressedOnMemoryCacheItem item(ras, m_builder->clone(),
                                   m_imageInfo->clone());
  return item.getImage();
}

//------------------------------------------------------------------------------

class UncompressedOnDiskCacheItem final : public CacheItem {
  int m_pixelsize;

public:
  UncompressedOnDiskCacheItem(const TFilePath &fp, const TImageP &img);

  ~UncompressedOnDiskCacheItem();

  TUINT32 getSize() const override { return 0; }
  TImageP getImage() const override;
  // TRaster32P getRaster32() const;

  TFilePath m_fp;
};
#ifdef _WIN32
template class DVAPI TSmartPointerT<UncompressedOnDiskCacheItem>;
template class DVAPI
    TDerivedSmartPointerT<UncompressedOnDiskCacheItem, CacheItem>;
#endif
typedef TDerivedSmartPointerT<UncompressedOnDiskCacheItem, CacheItem>
    UncompressedOnDiskCacheItemP;

//------------------------------------------------------------------------------

UncompressedOnDiskCacheItem::UncompressedOnDiskCacheItem(const TFilePath &fp,
                                                         const TImageP &image)
    : CacheItem(0, 0), m_fp(fp) {
  TRasterImageP ri = image;

  TRasterP ras;
  if (ri) {
    m_imageInfo = new RasterImageInfo(ri);
    ras         = ri->getRaster();
  }
#ifndef TNZCORE_LIGHT
  else {
    TToonzImageP ti = image;
    if (ti) {
      m_imageInfo = new ToonzImageInfo(ti);
      ras         = ti->getRaster();
    } else
      assert(false);
  }
#else
  else
    assert(false);
#endif

  m_builder = 0;

  int dataSize = ras->getLx() * ras->getLy() * ras->getPixelSize();

  int lx      = ras->getLx();
  int ly      = ras->getLy();
  int wrap    = ras->getWrap();
  m_pixelsize = ras->getPixelSize();

  Tofstream oss(m_fp);
  // oss.write((char*)&dataSize, sizeof(TUINT32));
  // assert(!oss.fail());
  ras->lock();
  if (lx == wrap) {
    oss.write((char *)ras->getRawData(), dataSize);
    assert(!oss.fail());
  } else {
    char *buf = (char *)ras->getRawData();
    for (int i = 0; i < ly; i++, buf += wrap) {
      oss.write(buf, lx * m_pixelsize);
      assert(!oss.fail());
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------

UncompressedOnDiskCacheItem::~UncompressedOnDiskCacheItem() {
  delete m_imageInfo;
  TSystem::deleteFile(m_fp);
}

//------------------------------------------------------------------------------

TImageP UncompressedOnDiskCacheItem::getImage() const {
  Tifstream is(m_fp);
  TUINT32 dataSize =
      m_imageInfo->m_size.lx * m_imageInfo->m_size.ly * m_pixelsize;

  // is.read((char*)&dataSize, sizeof(TUINT32));
  // assert(unsigned(m_lx*m_ly*m_pixelsize)==dataSize);

  TRasterP ras;

  RasterImageInfo *rii = dynamic_cast<RasterImageInfo *>(m_imageInfo);

  if (rii) {
    if (m_pixelsize == 4)
      ras = (TRasterP)(TRaster32P(rii->m_size));
    else if (m_pixelsize == 8)
      ras = (TRasterP)(TRaster64P(rii->m_size));
    else if (m_pixelsize == 1)
      ras = (TRasterP)(TRasterGR8P(rii->m_size));
    else if (m_pixelsize == 2)
      ras = (TRasterP)(TRasterGR16P(rii->m_size));
    else
      assert(false);
    ras->lock();
    char *data = (char *)ras->getRawData();
    is.read(data, dataSize);
    ras->unlock();
#ifdef _DEBUGTOONZ
    ras->m_cashed = true;
#endif

    return RasterImageBuilder().build(m_imageInfo, ras);
  }
#ifndef TNZCORE_LIGHT
  else {
    ToonzImageInfo *tii = dynamic_cast<ToonzImageInfo *>(m_imageInfo);
    if (tii) {
      ras = (TRasterP)(TRasterCM32P(tii->m_size));
      ras->lock();
      char *data = (char *)ras->getRawData();
      is.read(data, dataSize);
      ras->unlock();
#ifdef _DEBUG
      ras->m_cashed = true;
#endif

      return ToonzImageBuilder().build(m_imageInfo, ras);
    } else {
      assert(false);
      return 0;
    }
  }
#else
  else {
    assert(false);
    return 0;
  }
#endif
}

//------------------------------------------------------------------------------

std::string TImageCache::getUniqueId(void) {
  static TAtomicVar count;
  std::stringstream ss;
  ss << ++count;
  return "IMAGECACHEUNIQUEID" + ss.str();
}

class TImageCache::Imp {
public:
  Imp() : m_rootDir() {
    // ATTENZIONE: e' molto piu' veloce se si usa memoria fisica
    // invece che virtuale: la virtuale e' tanta, non c'e' quindi bisogno
    // di comprimere le immagini, che grandi come sono vengono swappate su disco
    if (TBigMemoryManager::instance()->isActive()) return;

    m_reservedMemory = (TINT64)(TSystem::getMemorySize(true) * 0.10);
    if (m_reservedMemory < 64 * 1024) m_reservedMemory = 64 * 1024;
  }

  ~Imp() {
    if (m_rootDir != TFilePath()) TSystem::rmDirTree(m_rootDir);
  }

  bool inline notEnoughMemory() {
    if (TBigMemoryManager::instance()->isActive())
      return TBigMemoryManager::instance()->getAvailableMemoryinKb() <
             50 * 1024;
    else
      return TSystem::memoryShortage();
  }

  void doCompress();
  void doCompress(std::string id);
  UCHAR *compressAndMalloc(TUINT32 requestedSize);  // compress in the cache
                                                    // till it can nallocate the
                                                    // requested memory
  void outputMap(UINT chunkRequested, std::string filename);
  void remove(const std::string &id);
  void remap(const std::string &dstId, const std::string &srcId);
  TImageP get(const std::string &id, bool toBeModified);
  void add(const std::string &id, const TImageP &img, bool overwrite);
  TFilePath m_rootDir;

#ifndef TNZCORE_LIGHT
  QThreadStorage<bool *> m_isEnabled;
#else
  bool m_isEnabled;
#endif

  std::map<std::string, CacheItemP> m_uncompressedItems;
  std::map<TUINT32, std::string> m_itemHistory;
  std::map<std::string, CacheItemP> m_compressedItems;
  std::map<void *, std::string>
      m_itemsByImagePointer;  // items ordered by ImageP.getPointer()
  std::map<std::string, std::string> m_duplicatedItems;  // for duplicated items
                                                         // (when id1!=id2 but
                                                         // image1==image2) in
                                                         // the map: key is dup
                                                         // id, value is main id
  // memoria fisica totale della macchina che non puo' essere utilizzata;
  TINT64 m_reservedMemory;
  TThread::Mutex m_mutex;

  static int m_fileid;
};

int TImageCache::Imp::m_fileid;

//------------------------------------------------------------------------------
namespace {
inline void *getPointer(const TImageP &img) {
  TRasterImageP rimg = img;
  if (rimg) return rimg->getRaster().getPointer();
#ifndef TNZCORE_LIGHT
  TToonzImageP timg = img;
  if (timg) return timg->getRaster().getPointer();
#endif

  return img.getPointer();
}

// Returns true or false whether the image or its eventual raster are
// referenced by someone other than Toonz cache.
inline TINT32 hasExternalReferences(const TImageP &img) {
  int refCount;

  {
    TRasterImageP rimg = img;
    if (rimg) refCount = rimg->getRaster()->getRefCount();
  }

#ifndef TNZCORE_LIGHT
  {
    TToonzImageP timg = img;
    if (timg)
      refCount = timg->getRaster()->getRefCount() -
                 1;  //!!! the TToonzImage::getRaster method increments raster
                     //! refCount!(the TRasterImage::getRaster don't)
  }
#endif

  return std::max(refCount, img->getRefCount()) > 1;
}
}
//------------------------------------------------------------------------------

void TImageCache::Imp::doCompress() {
  // se la memoria usata per mantenere le immagini decompresse e' superiore
  // a un dato valore, comprimo alcune immagini non compresse non checked-out
  // in modo da liberare memoria

  // per il momento scorre tutte le immagini alla ricerca di immagini
  // non compresse non checked-out

  TThread::MutexLocker sl(&m_mutex);

  std::map<TUINT32, std::string>::iterator itu = m_itemHistory.begin();

  for (; itu != m_itemHistory.end() && notEnoughMemory();) {
    std::map<std::string, CacheItemP>::iterator it =
        m_uncompressedItems.find(itu->second);
    assert(it != m_uncompressedItems.end());
    CacheItemP item = it->second;

    UncompressedOnMemoryCacheItemP uitem = item;
    if (item->m_cantCompress ||
        (uitem && (!uitem->m_image || hasExternalReferences(uitem->m_image)))) {
      ++itu;
      continue;
    }
    std::string id = it->first;

#ifdef _WIN32
    assert(itu->first == it->second->m_historyCount);
    itu = m_itemHistory.erase(itu);
    m_itemsByImagePointer.erase(getPointer(item->getImage()));
    m_uncompressedItems.erase(it);
#else
    std::map<TUINT32, std::string>::iterator itu2 = itu;
    itu++;
    m_itemHistory.erase(itu2);
    m_itemsByImagePointer.erase(item->getImage().getPointer());
    m_uncompressedItems.erase(it);
#endif

    if (m_compressedItems.find(id) == m_compressedItems.end()) {
      assert(uitem);
      item->m_cantCompress = true;
      CacheItemP newItem   = new CompressedOnMemoryCacheItem(
          item->getImage());  // WARNING the codec buffer  allocation can CHANGE
                              // the cache.
      item->m_cantCompress = false;
      if (newItem->getSize() ==
          0)  /// non c'era memoria sufficiente per il buffer compresso....
      {
        assert(m_rootDir != TFilePath());
        TFilePath fp =
            m_rootDir + TFilePath(std::to_string(TImageCache::Imp::m_fileid++));
        newItem = new UncompressedOnDiskCacheItem(fp, item->getImage());
      }
      m_compressedItems[id] = newItem;
      item                  = CacheItemP();
      uitem                 = UncompressedOnMemoryCacheItemP();
      // doCompress();//restart, since interators can have been changed (see
      // comment above)
      // return;
      itu = m_itemHistory.begin();
    }
  }

  // se il quantitativo di memoria utilizzata e' superiore a un dato valore,
  // sposto
  // su disco alcune immagini compresse in modo da liberare memoria

  if (itu != m_itemHistory.end())  // memory is enough!
    return;

  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.begin();
  for (; itc != m_compressedItems.end() && notEnoughMemory(); ++itc) {
    CacheItemP item = itc->second;
    if (item->m_cantCompress) continue;

    CompressedOnMemoryCacheItemP citem = itc->second;
    if (citem) {
      assert(m_rootDir != TFilePath());
      TFilePath fp =
          m_rootDir + TFilePath(std::to_string(TImageCache::Imp::m_fileid++));

      CacheItemP newItem = new CompressedOnDiskCacheItem(
          fp, citem->m_compressedRas, citem->m_builder->clone(),
          citem->m_imageInfo->clone());

      itc->second                   = 0;
      m_compressedItems[itc->first] = newItem;
    }
  }
}

//------------------------------------------------------------------------------

void TImageCache::Imp::doCompress(std::string id) {
  TThread::MutexLocker sl(&m_mutex);

  // search id in m_uncompressedItems
  std::map<std::string, CacheItemP>::iterator it = m_uncompressedItems.find(id);
  if (it == m_uncompressedItems.end()) return;  // id not found: return

  // is item suitable for compression ?
  CacheItemP item                      = it->second;
  UncompressedOnMemoryCacheItemP uitem = item;
  if (item->m_cantCompress ||
      (uitem && (!uitem->m_image || hasExternalReferences(uitem->m_image))))
    return;

  // search id in m_itemHistory
  std::map<TUINT32, std::string>::iterator itu = m_itemHistory.begin();
  while (itu != m_itemHistory.end() && itu->second != id) ++itu;
  if (itu == m_itemHistory.end()) return;  // id not found: return

// delete itu from m_itemHistory
#ifdef _WIN32
  assert(itu->first == it->second->m_historyCount);
  itu = m_itemHistory.erase(itu);
  m_itemsByImagePointer.erase(getPointer(item->getImage()));
#else
  std::map<TUINT32, std::string>::iterator itu2 = itu;
  itu++;
  m_itemHistory.erase(itu2);
  m_itemsByImagePointer.erase(item->getImage().getPointer());
#endif

  // delete item from m_uncompressedItems
  m_uncompressedItems.erase(it);

  // check if item has been already compressed. this should never happen
  if (m_compressedItems.find(id) != m_compressedItems.end()) return;

  assert(uitem);
  item->m_cantCompress = true;  // ??
  CacheItemP newItem   = new CompressedOnMemoryCacheItem(
      item->getImage());  // WARNING the codec buffer  allocation can CHANGE the
                          // cache.
  item->m_cantCompress = false;  // ??
  if (newItem->getSize() ==
      0)  /// non c'era memoria sufficiente per il buffer compresso....
  {
    assert(m_rootDir != TFilePath());
    TFilePath fp =
        m_rootDir + TFilePath(std::to_string(TImageCache::Imp::m_fileid++));
    newItem = new UncompressedOnDiskCacheItem(fp, item->getImage());
  }
  m_compressedItems[id] = newItem;
  item                  = CacheItemP();
  uitem                 = UncompressedOnMemoryCacheItemP();
}

/*
  // se il quantitativo di memoria utilizzata e' superiore a un dato valore,
  sposto
  // su disco alcune immagini compresse in modo da liberare memoria

  if (itu != m_itemHistory.end()) //memory is enough!
    return;

  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.begin();
  for ( ; itc != m_compressedItems.end() && notEnoughMemory(); ++itc)
        {
          CacheItemP item = itc->second;
          if (item->m_cantCompress)
                  continue;

          CompressedOnMemoryCacheItemP citem = itc->second;
          if (citem)
                {
                  assert(m_rootDir!=TFilePath());
                  TFilePath fp = m_rootDir +
  TFilePath(toString(TImageCache::Imp::m_fileid++));

                  CacheItemP newItem = new CompressedOnDiskCacheItem(fp,
  citem->m_compressedRas,
                                                                                                                                                                                                                           citem->m_builder->clone(), citem->m_imageInfo->clone());

                  itc->second = 0;
                  m_compressedItems[itc->first] = newItem;
                }
        }
  */

//------------------------------------------------------------------------------

UCHAR *TImageCache::Imp::compressAndMalloc(TUINT32 size) {
  UCHAR *buf = 0;

  TThread::MutexLocker sl(&m_mutex);

  TheCodec::instance()->reset();

  // if (size!=0)
  //  size = size>>10;

  // assert(size==0 || TBigMemoryManager::instance()->isActive());

  std::map<TUINT32, std::string>::iterator itu = m_itemHistory.begin();
  while (
      (buf = TBigMemoryManager::instance()->getBuffer(size)) == 0 &&
      itu !=
          m_itemHistory
              .end())  //>TBigMemoryManager::instance()->getAvailableMemoryinKb()))
  {
    std::map<std::string, CacheItemP>::iterator it =
        m_uncompressedItems.find(itu->second);
    assert(it != m_uncompressedItems.end());
    CacheItemP item = it->second;

    UncompressedOnMemoryCacheItemP uitem = item;
    if (item->m_cantCompress ||
        (uitem && (!uitem->m_image || hasExternalReferences(uitem->m_image)))) {
      ++itu;
      continue;
    }

    if (m_compressedItems.find(it->first) == m_compressedItems.end()) {
      assert(uitem);
      CacheItemP newItem;
      // newItem = new CompressedOnMemoryCacheItem(item->getImage());
      // if (newItem->getSize()==0)
      //  {
      assert(m_rootDir != TFilePath());
      TFilePath fp =
          m_rootDir + TFilePath(std::to_string(TImageCache::Imp::m_fileid++));
      newItem = new UncompressedOnDiskCacheItem(fp, item->getImage());
      //  }

      m_compressedItems[it->first] = newItem;
    }

#ifdef _WIN32
    assert(itu->first == it->second->m_historyCount);
    itu = m_itemHistory.erase(itu);
    m_itemsByImagePointer.erase(getPointer(item->getImage()));
    m_uncompressedItems.erase(it);
#else
    std::map<TUINT32, std::string>::iterator itu2 = itu;
    itu++;
    m_itemHistory.erase(itu2);
    m_itemsByImagePointer.erase(item->getImage().getPointer());
    m_uncompressedItems.erase(it);
#endif
  }

  if (buf != 0) return buf;

  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.begin();
  for (; itc != m_compressedItems.end() &&
         (buf = TBigMemoryManager::instance()->getBuffer(size)) == 0;
       ++itc) {
    CacheItemP item = itc->second;
    if (item->m_cantCompress) continue;

    CompressedOnMemoryCacheItemP citem = itc->second;
    if (citem) {
      assert(m_rootDir != TFilePath());
      TFilePath fp =
          m_rootDir + TFilePath(std::to_string(TImageCache::Imp::m_fileid++));

      CacheItemP newItem = new CompressedOnDiskCacheItem(
          fp, citem->m_compressedRas, citem->m_builder->clone(),
          citem->m_imageInfo->clone());

      itc->second                   = 0;
      m_compressedItems[itc->first] = newItem;
    }
  }

  return buf;
}

//------------------------------------------------------------------------------

namespace {

int check       = 0;
const int magic = 123456;
}

static TImageCache *CacheInstance = 0;

TImageCache *TImageCache::instance() {
  if (CacheInstance == 0) CacheInstance = new TImageCache();
  return CacheInstance;
  /*
if (!ImageCache::m_instance)
{
ImageCache::m_instance = new ImageCache;
ImageCache::m_destroyer.m_imageCache = ImageCache::m_instance;
}
return ImageCache::m_instance;
  */
}

//------------------------------------------------------------------------------

TImageCache::TImageCache() : m_imp(new Imp()) {
  assert(check == 0);
  check = magic;
}

//------------------------------------------------------------------------------

TImageCache::~TImageCache() {
  assert(check == magic);
  check         = -1;
  CacheInstance = 0;
}

//------------------------------------------------------------------------------

void TImageCache::setEnabled(bool isEnabled) {
#ifndef TNZCORE_LIGHT
  QThreadStorage<bool *> &storage = m_imp->m_isEnabled;

  if (storage.hasLocalData() && *(storage.localData()) == isEnabled) return;
  if (!storage.hasLocalData())
    storage.setLocalData(new bool(isEnabled));
  else
    *(storage.localData()) = isEnabled;
#else
  m_imp->m_isEnabled = isEnabled;
#endif
}

//------------------------------------------------------------------------------

bool TImageCache::isEnabled() {
#ifndef TNZCORE_LIGHT
  QThreadStorage<bool *> &storage = m_imp->m_isEnabled;

  if (!storage.hasLocalData()) return true;
  return *(storage.localData());
#else
  return m_isEnabled;
#endif
}

//------------------------------------------------------------------------------

void TImageCache::setRootDir(const TFilePath &cacheDir) {
  if (m_imp->m_rootDir != TFilePath()) return;

  m_imp->m_rootDir =
      cacheDir + TFilePath(std::to_string(TSystem::getProcessId()));

#ifndef TNZCORE_LIGHT
  TFileStatus fs1(m_imp->m_rootDir);

  if (!fs1.doesExist()) TSystem::mkDir(m_imp->m_rootDir);

#endif
}

//------------------------------------------------------------------------------
/*
TFilePath TImageCache::getRootDir() const
{
return m_imp->m_rootDir;
}
*/
//------------------------------------------------------------------------------

UCHAR *TImageCache::compressAndMalloc(TUINT32 requestedSize) {
  return m_imp->compressAndMalloc(requestedSize);
}

//------------------------------------------------------------------------------

void TImageCache::add(const std::string &id, const TImageP &img,
                      bool overwrite) {
  if (!isEnabled()) return;
  m_imp->add(id, img, overwrite);
}

//------------------------------------------------------------------------------

void TImageCache::Imp::add(const std::string &id, const TImageP &img,
                           bool overwrite) {
  TThread::MutexLocker sl(&m_mutex);

#ifdef LEVO
  std::map<std::string, CacheItemP>::iterator it1 = m_uncompressedItems.begin();

  for (; it1 != m_uncompressedItems.end(); ++it1) {
    UncompressedOnMemoryCacheItemP item =
        (UncompressedOnMemoryCacheItemP)it1->second;
    // m_memUsage -= item->getSize();
    assert(item);
    TImageP refImg = item->getImage();
    if (refImg.getPointer() == img.getPointer() && it1->first != id)
      assert(!"opps gia' esiste in cache!");
  }
#endif

  std::map<std::string, CacheItemP>::iterator itUncompr =
      m_uncompressedItems.find(id);
  std::map<std::string, CacheItemP>::iterator itCompr =
      m_compressedItems.find(id);

#ifdef _DEBUGTOONZ
  TRasterImageP rimg = (TRasterImageP)img;
  TToonzImageP timg  = (TToonzImageP)img;
#endif

  if (itUncompr != m_uncompressedItems.end() ||
      itCompr !=
          m_compressedItems.end())  // already present in cache with same id...
  {
    if (overwrite) {
#ifdef _DEBUGTOONZ
      if (rimg)
        rimg->getRaster()->m_cashed = true;
      else if (timg)
        timg->getRaster()->m_cashed = true;
#endif
      std::map<std::string, CacheItemP>::iterator it;

      if (itUncompr != m_uncompressedItems.end()) {
        assert(m_itemHistory.find(itUncompr->second->m_historyCount) !=
               m_itemHistory.end());
        m_itemHistory.erase(itUncompr->second->m_historyCount);
        m_itemsByImagePointer.erase(getPointer(itUncompr->second->getImage()));
        m_uncompressedItems.erase(itUncompr);
      }
      if (itCompr != m_compressedItems.end()) m_compressedItems.erase(id);
    } else
      return;
  } else {
    std::map<std::string, std::string>::iterator dt =
        m_duplicatedItems.find(id);
    if ((dt != m_duplicatedItems.end()) && !overwrite) return;

    std::map<void *, std::string>::iterator it;
    if ((it = m_itemsByImagePointer.find(getPointer(img))) !=
        m_itemsByImagePointer
            .end())  // already present in cache with another id...
    {
      m_duplicatedItems[id] = it->second;
      return;
    }

    if (dt != m_duplicatedItems.end()) m_duplicatedItems.erase(dt);
  }

  CacheItemP item;

#ifdef _DEBUGTOONZ
  if (rimg)
    rimg->getRaster()->m_cashed = true;
  else if (timg)
    timg->getRaster()->m_cashed = true;
#endif

  item = new UncompressedOnMemoryCacheItem(img);
#ifdef TNZCORE_LIGHT
  item->m_cantCompress = false;
#else
  item->m_cantCompress = (TVectorImageP(img) ? true : false);
#endif
  item->m_id                             = id;
  m_uncompressedItems[id]                = item;
  m_itemsByImagePointer[getPointer(img)] = id;
  item->m_historyCount                   = HistoryCount;
  m_itemHistory[HistoryCount]            = id;
  HistoryCount++;

  doCompress();

#ifdef _DEBUGTOONZ
// int itemCount =
// m_imp->m_uncompressedItems.size()+m_imp->m_compressedItems.size();
// m_imp->outputDebug();
#endif
}

void TImageCache::remove(const std::string &id) { m_imp->remove(id); }

//------------------------------------------------------------------------------

void TImageCache::Imp::remove(const std::string &id) {
  if (CacheInstance == 0)
    return;  // the remove can be called when exiting from toonz...after the
             // imagecache was already freed!

  assert(check == magic);
  TThread::MutexLocker sl(&m_mutex);

  std::map<std::string, std::string>::iterator it1;
  if ((it1 = m_duplicatedItems.find(id)) !=
      m_duplicatedItems.end())  // it's a duplicated id...
  {
    m_duplicatedItems.erase(it1);
    return;
  }

  for (it1 = m_duplicatedItems.begin(); it1 != m_duplicatedItems.end(); ++it1)
    if (it1->second == id) break;

  if (it1 != m_duplicatedItems.end())  // it has duplicated, so cannot erase it;
                                       // I erase the duplicate, and assign its
                                       // id has the main id
  {
    std::string sonId = it1->first;
    m_duplicatedItems.erase(it1);
    remap(sonId, id);
    return;
  }

  std::map<std::string, CacheItemP>::iterator it = m_uncompressedItems.find(id);
  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.find(id);
  if (it != m_uncompressedItems.end()) {
    const CacheItemP &item = it->second;
    assert((UncompressedOnMemoryCacheItemP)item);
    assert(m_itemHistory.find(it->second->m_historyCount) !=
           m_itemHistory.end());
    m_itemHistory.erase(it->second->m_historyCount);
    m_itemsByImagePointer.erase(getPointer(it->second->getImage()));

#ifdef _DEBUGTOONZ
    if ((TRasterImageP)it->second->getImage())
      ((TRasterImageP)it->second->getImage())->getRaster()->m_cashed = false;
    else if ((TToonzImageP)it->second->getImage())
      ((TToonzImageP)it->second->getImage())->getRaster()->m_cashed = false;
#endif

    m_uncompressedItems.erase(it);
  }
  if (itc != m_compressedItems.end()) m_compressedItems.erase(itc);
}

//------------------------------------------------------------------------------

void TImageCache::remap(const std::string &dstId, const std::string &srcId) {
  m_imp->remap(dstId, srcId);
}

void TImageCache::Imp::remap(const std::string &dstId,
                             const std::string &srcId) {
  TThread::MutexLocker sl(&m_mutex);
  std::map<std::string, CacheItemP>::iterator it =
      m_uncompressedItems.find(srcId);
  if (it != m_uncompressedItems.end()) {
    CacheItemP citem = it->second;
    assert(m_itemHistory.find(citem->m_historyCount) != m_itemHistory.end());
    m_itemHistory.erase(citem->m_historyCount);
    m_itemsByImagePointer.erase(getPointer(citem->getImage()));
    m_uncompressedItems.erase(it);

    m_uncompressedItems[dstId]                           = citem;
    m_itemHistory[citem->m_historyCount]                 = dstId;
    m_itemsByImagePointer[getPointer(citem->getImage())] = dstId;
  }
  it = m_compressedItems.find(srcId);
  if (it != m_compressedItems.end()) {
    CacheItemP citem = it->second;
    m_compressedItems.erase(it);
    m_compressedItems[dstId] = citem;
  }
  std::map<std::string, std::string>::iterator it2 =
      m_duplicatedItems.find(srcId);
  if (it2 != m_duplicatedItems.end()) {
    std::string id = it2->second;
    m_duplicatedItems.erase(it2);
    m_duplicatedItems[dstId] = id;
  }
  for (it2 = m_duplicatedItems.begin(); it2 != m_duplicatedItems.end(); ++it2)
    if (it2->second == srcId) it2->second = dstId;
}

//------------------------------------------------------------------------------

void TImageCache::remapIcons(const std::string &dstId,
                             const std::string &srcId) {
  std::map<std::string, CacheItemP>::iterator it;
  std::map<std::string, std::string> table;
  std::string prefix = srcId + ":";
  int j              = (int)prefix.length();
  for (it = m_imp->m_uncompressedItems.begin();
       it != m_imp->m_uncompressedItems.end(); ++it) {
    std::string id                      = it->first;
    if (id.find(prefix) == 0) table[id] = dstId + ":" + id.substr(j);
  }
  for (std::map<std::string, std::string>::iterator it2 = table.begin();
       it2 != table.end(); ++it2) {
    remap(it2->second, it2->first);
  }
}

//------------------------------------------------------------------------------

void TImageCache::clear(bool deleteFolder) {
  TThread::MutexLocker sl(&m_imp->m_mutex);
  m_imp->m_uncompressedItems.clear();
  m_imp->m_itemHistory.clear();
  m_imp->m_compressedItems.clear();
  m_imp->m_duplicatedItems.clear();
  m_imp->m_itemsByImagePointer.clear();
  if (deleteFolder && m_imp->m_rootDir != TFilePath())
    TSystem::rmDirTree(m_imp->m_rootDir);
}

//------------------------------------------------------------------------------

void TImageCache::clearSceneImages() {
  TThread::MutexLocker sl(&m_imp->m_mutex);

  // note the ';' - which follows ':' in the ascii table
  m_imp->m_uncompressedItems.erase(
      m_imp->m_uncompressedItems.begin(),
      m_imp->m_uncompressedItems.lower_bound("$:"));
  m_imp->m_uncompressedItems.erase(m_imp->m_uncompressedItems.lower_bound("$;"),
                                   m_imp->m_uncompressedItems.end());

  m_imp->m_compressedItems.erase(m_imp->m_compressedItems.begin(),
                                 m_imp->m_compressedItems.lower_bound("$:"));
  m_imp->m_compressedItems.erase(m_imp->m_compressedItems.lower_bound("$;"),
                                 m_imp->m_compressedItems.end());

  m_imp->m_duplicatedItems.erase(m_imp->m_duplicatedItems.begin(),
                                 m_imp->m_duplicatedItems.lower_bound("$:"));
  m_imp->m_duplicatedItems.erase(m_imp->m_duplicatedItems.lower_bound("$;"),
                                 m_imp->m_duplicatedItems.end());

  // Clear maps whose id is on the second of map pairs.

  std::map<TUINT32, std::string>::iterator it;
  for (it = m_imp->m_itemHistory.begin(); it != m_imp->m_itemHistory.end();) {
    if (it->second.size() >= 2 && it->second[0] == '$' && it->second[1] == ':')
      ++it;
    else {
      std::map<TUINT32, std::string>::iterator app = it;
      app++;
      m_imp->m_itemHistory.erase(it);
      it = app;
    }
  }

  std::map<void *, std::string>::iterator jt;
  for (jt = m_imp->m_itemsByImagePointer.begin();
       jt != m_imp->m_itemsByImagePointer.end();) {
    if (jt->second.size() >= 2 && jt->second[0] == '$' && jt->second[1] == ':')
      ++jt;
    else {
      std::map<void *, std::string>::iterator app = jt;
      app++;
      m_imp->m_itemsByImagePointer.erase(jt);
      jt = app;
    }
  }
}

//------------------------------------------------------------------------------

bool TImageCache::isCached(const std::string &id) const {
  TThread::MutexLocker sl(&m_imp->m_mutex);
  return (m_imp->m_uncompressedItems.find(id) !=
              m_imp->m_uncompressedItems.end() ||
          m_imp->m_compressedItems.find(id) != m_imp->m_compressedItems.end() ||
          m_imp->m_duplicatedItems.find(id) != m_imp->m_duplicatedItems.end());
}

//------------------------------------------------------------------------------
#ifdef LEVO

bool TImageCache::getSize(const std::string &id, TDimension &size) const {
  QMutexLocker sl(&m_imp->m_mutex);

  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) {
    UncompressedOnMemoryCacheItemP uncompressed = it->second;
    assert(uncompressed);
    TToonzImageP ti = uncompressed->getImage();
    if (ti) {
      size = ti->getSize();
      return true;
    }

    TRasterImageP ri = uncompressed->getImage();
    if (ri && ri->getRaster()) {
      size = ri->getRaster()->getSize();
      return true;
    }
    return false;
  }
  std::map<std::string, CacheItemP>::iterator itc =
      m_imp->m_compressedItems.find(id);
  if (itc == m_imp->m_compressedItems.end()) return false;
  CacheItemP cacheItem = itc->second;

  if (cacheItem->m_imageInfo) {
    RasterImageInfo *rimageInfo =
        dynamic_cast<RasterImageInfo *>(cacheItem->m_imageInfo);
    if (rimageInfo) {
      size = rimageInfo->m_size;
      return true;
    }
    ToonzImageInfo *timageInfo =
        dynamic_cast<ToonzImageInfo *>(cacheItem->m_imageInfo);
    if (timageInfo) {
      size = timageInfo->m_size;
      return true;
    }
  }

  return false;
}

//------------------------------------------------------------------------------

bool TImageCache::getSavebox(const std::string &id, TRect &savebox) const {
  QMutexLocker sl(&m_imp->m_mutex);

  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) {
    UncompressedOnMemoryCacheItemP uncompressed = it->second;
    assert(uncompressed);

    TToonzImageP ti = uncompressed->getImage();
    if (ti) {
      savebox = ti->getSavebox();
      return true;
    }
    TRasterImageP ri = uncompressed->getImage();
    if (ri) {
      savebox = ri->getSavebox();
      return true;
    }
    return false;
  }
  std::map<std::string, CacheItemP>::iterator itc =
      m_imp->m_compressedItems.find(id);
  if (itc == m_imp->m_compressedItems.end()) return false;

  CacheItemP cacheItem = itc->second;

  assert(cacheItem->m_imageInfo);

  RasterImageInfo *rimageInfo =
      dynamic_cast<RasterImageInfo *>(cacheItem->m_imageInfo);
  if (rimageInfo) {
    savebox = rimageInfo->m_savebox;
    return true;
  }
#ifndef TNZCORE_LIGHT
  ToonzImageInfo *timageInfo =
      dynamic_cast<ToonzImageInfo *>(cacheItem->m_imageInfo);
  if (timageInfo) {
    savebox = timageInfo->m_savebox;
    return true;
  }
#endif
  return false;
}

//------------------------------------------------------------------------------

bool TImageCache::getDpi(const std::string &id, double &dpiX,
                         double &dpiY) const {
  QMutexLocker sl(&m_imp->m_mutex);

  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) {
    UncompressedOnMemoryCacheItemP uncompressed = it->second;
    assert(uncompressed);
    TToonzImageP ti = uncompressed->getImage();
    if (ti) {
      ti->getDpi(dpiX, dpiY);
      return true;
    }
    TRasterImageP ri = uncompressed->getImage();
    if (ri) {
      ri->getDpi(dpiX, dpiY);
      return true;
    }
    return false;
  }
  std::map<std::string, CacheItemP>::iterator itc =
      m_imp->m_compressedItems.find(id);
  if (itc == m_imp->m_compressedItems.end()) return false;
  CacheItemP cacheItem = itc->second;
  assert(cacheItem->m_imageInfo);

  RasterImageInfo *rimageInfo =
      dynamic_cast<RasterImageInfo *>(cacheItem->m_imageInfo);
  if (rimageInfo) {
    dpiX = rimageInfo->m_dpix;
    dpiY = rimageInfo->m_dpiy;
    return true;
  }
#ifndef TNZCORE_LIGHT
  ToonzImageInfo *timageInfo =
      dynamic_cast<ToonzImageInfo *>(cacheItem->m_imageInfo);
  if (timageInfo) {
    dpiX = timageInfo->m_dpix;
    dpiY = timageInfo->m_dpiy;
    return true;
  }
#endif
  return false;
}

//------------------------------------------------------------------------------
#endif

bool TImageCache::getSubsampling(const std::string &id, int &subs) const {
  TThread::MutexLocker sl(&m_imp->m_mutex);

  std::map<std::string, std::string>::iterator it1;
  if ((it1 = m_imp->m_duplicatedItems.find(id)) !=
      m_imp->m_duplicatedItems.end()) {
    assert(m_imp->m_duplicatedItems.find(it1->second) ==
           m_imp->m_duplicatedItems.end());
    return getSubsampling(it1->second, subs);
  }

  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) {
    UncompressedOnMemoryCacheItemP uncompressed = it->second;
    assert(uncompressed);
#ifndef TNZCORE_LIGHT
    if (TToonzImageP ti = uncompressed->getImage()) {
      subs = ti->getSubsampling();
      return true;
    }

    else
#endif
        if (TRasterImageP ri = uncompressed->getImage()) {
      subs = ri->getSubsampling();
      return true;
    } else
      return false;
  }
  std::map<std::string, CacheItemP>::iterator itc =
      m_imp->m_compressedItems.find(id);
  if (itc == m_imp->m_compressedItems.end()) return false;
  CacheItemP cacheItem = itc->second;
  assert(cacheItem->m_imageInfo);
  if (RasterImageInfo *rimageInfo =
          dynamic_cast<RasterImageInfo *>(cacheItem->m_imageInfo)) {
    subs = rimageInfo->m_subs;
    return true;
  }
#ifndef TNZCORE_LIGHT
  else if (ToonzImageInfo *timageInfo =
               dynamic_cast<ToonzImageInfo *>(cacheItem->m_imageInfo)) {
    subs = timageInfo->m_subs;
    return true;
  }
#endif
  else
    return false;
}

//------------------------------------------------------------------------------

bool TImageCache::hasBeenModified(const std::string &id, bool reset) const {
  TThread::MutexLocker sl(&m_imp->m_mutex);

  std::map<std::string, std::string>::iterator it;
  if ((it = m_imp->m_duplicatedItems.find(id)) !=
      m_imp->m_duplicatedItems.end()) {
    assert(m_imp->m_duplicatedItems.find(it->second) ==
           m_imp->m_duplicatedItems.end());
    return hasBeenModified(it->second, reset);
  }

  TImageP img;

  std::map<std::string, CacheItemP>::iterator itu =
      m_imp->m_uncompressedItems.find(id);
  if (itu != m_imp->m_uncompressedItems.end()) {
    if (reset && itu->second->m_modified) {
      itu->second->m_modified = false;
      return true;
    } else
      return itu->second->m_modified;
  }
  return true;  // not present in cache==modified (for particle purposes...)
}

//------------------------------------------------------------------------------

TImageP TImageCache::get(const std::string &id, bool toBeModified) const {
  return m_imp->get(id, toBeModified);
}

//------------------------------------------------------------------------------

TImageP TImageCache::Imp::get(const std::string &id, bool toBeModified) {
  TThread::MutexLocker sl(&m_mutex);

  std::map<std::string, std::string>::const_iterator it;
  if ((it = m_duplicatedItems.find(id)) != m_duplicatedItems.end()) {
    assert(m_duplicatedItems.find(it->second) == m_duplicatedItems.end());
    return get(it->second, toBeModified);
  }

  TImageP img;

  std::map<std::string, CacheItemP>::iterator itu =
      m_uncompressedItems.find(id);
  if (itu != m_uncompressedItems.end()) {
    img = itu->second->getImage();
    if (itu->second->m_historyCount !=
        HistoryCount - 1)  // significa che l'ultimo get non era sulla stessa
                           // immagine, quindi  serve aggiornare l'history!
    {
      assert(m_itemHistory.find(itu->second->m_historyCount) !=
             m_itemHistory.end());
      m_itemHistory.erase(itu->second->m_historyCount);
      m_itemHistory[HistoryCount] = id;
      itu->second->m_historyCount = HistoryCount;
      HistoryCount++;
    }
    if (toBeModified) {
      itu->second->m_modified = true;
      std::map<std::string, CacheItemP>::iterator itc =
          m_compressedItems.find(id);
      if (itc != m_compressedItems.end()) m_compressedItems.erase(itc);
    }
    return img;
  }

  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.find(id);
  if (itc == m_compressedItems.end()) return 0;

  CacheItemP cacheItem = itc->second;

  img = cacheItem->getImage();

  CacheItemP uncompressed;
  uncompressed                    = new UncompressedOnMemoryCacheItem(img);
  m_uncompressedItems[itc->first] = uncompressed;
  m_itemsByImagePointer[getPointer(img)] = itc->first;

  m_itemHistory[HistoryCount]  = itc->first;
  uncompressed->m_historyCount = HistoryCount;
  HistoryCount++;

  if (CompressedOnMemoryCacheItemP(cacheItem))
  // l'immagine compressa non la tengo insieme alla
  // uncompressa se e' troppo grande
  {
    if (10 * cacheItem->getSize() > uncompressed->getSize()) {
      m_compressedItems.erase(itc);
      itc = m_compressedItems.end();
    }
  } else
    assert((CompressedOnDiskCacheItemP)cacheItem ||
           (UncompressedOnDiskCacheItemP)cacheItem);  // deve essere compressa!

  if (toBeModified && itc != m_compressedItems.end()) {
    uncompressed->m_modified = true;
    m_compressedItems.erase(itc);
  }

  uncompressed->m_cantCompress = toBeModified;
  // se la memoria utilizzata e' superiore al massimo consentito, comprime
  doCompress();

  uncompressed->m_cantCompress = false;

//#define DO_MEMCHECK
#ifdef DO_MEMCHECK
  assert(_CrtCheckMemory());
#endif

  return img;
}

//------------------------------------------------------------------------------

namespace {

class AccumulateMemUsage {
public:
  int operator()(int oldValue, std::pair<std::string, CacheItemP> item) {
    return oldValue + item.second->getSize();
  }
};
}

UINT TImageCache::getMemUsage() const {
  TThread::MutexLocker sl(&m_imp->m_mutex);

  int ret = std::accumulate(m_imp->m_uncompressedItems.begin(),
                            m_imp->m_uncompressedItems.end(), 0,
                            AccumulateMemUsage());

  return ret + std::accumulate(m_imp->m_compressedItems.begin(),
                               m_imp->m_compressedItems.end(), 0,
                               AccumulateMemUsage());
}

//------------------------------------------------------------------------------

UINT TImageCache::getDiskUsage() const { return 0; }

//------------------------------------------------------------------------------

UINT TImageCache::getMemUsage(const std::string &id) const {
  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) return it->second->getSize();

  it = m_imp->m_compressedItems.find(id);
  if (it != m_imp->m_compressedItems.end()) return it->second->getSize();
  return 0;
}

//------------------------------------------------------------------------------

//! Returns the uncompressed image size (in KB) of the image associated with
//! passd id, or 0 if none was found.
UINT TImageCache::getUncompressedMemUsage(const std::string &id) const {
  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.find(id);
  if (it != m_imp->m_uncompressedItems.end()) return it->second->getSize();

  it = m_imp->m_compressedItems.find(id);
  if (it != m_imp->m_compressedItems.end()) return it->second->getSize();

  return 0;
}

//------------------------------------------------------------------------------

/*
int TImageCache::getItemCount() const
{
  return m_imp->m_uncompressedItems.size()+m_imp->m_compressedItems.size();
}
*/
//------------------------------------------------------------------------------

UINT TImageCache::getDiskUsage(const std::string &id) const { return 0; }

//------------------------------------------------------------------------------

void TImageCache::dump(std::ostream &os) const {
  os << "mem: " << getMemUsage() << std::endl;
  std::map<std::string, CacheItemP>::iterator it =
      m_imp->m_uncompressedItems.begin();
  for (; it != m_imp->m_uncompressedItems.end(); ++it) {
    os << it->first << std::endl;
  }
}

//------------------------------------------------------------------------------

void TImageCache::outputMap(UINT chunkRequested, std::string filename) {
  m_imp->outputMap(chunkRequested, filename);
}

//------------------------------------------------------------------------------

void TImageCache::Imp::outputMap(UINT chunkRequested, std::string filename) {
  TThread::MutexLocker sl(&m_mutex);
  //#ifdef _DEBUG
  // static int Count = 0;

  std::string st = filename /*+toString(Count++)*/ + ".txt";
  TFilePath fp(st);
  Tofstream os(fp);

  int umcount1 = 0;
  int umcount2 = 0;
  int umcount3 = 0;
  int cmcount  = 0;
  int cdcount  = 0;
  int umcount  = 0;
  int udcount  = 0;

  TUINT64 umsize1 = 0;
  TUINT64 umsize2 = 0;
  TUINT64 umsize3 = 0;
  TUINT64 cmsize  = 0;
  TUINT64 cdsize  = 0;
  TUINT64 umsize  = 0;
  TUINT64 udsize  = 0;

  std::map<std::string, CacheItemP>::iterator itu = m_uncompressedItems.begin();

  for (; itu != m_uncompressedItems.end(); ++itu) {
    UncompressedOnMemoryCacheItemP uitem = itu->second;
    if (uitem->m_image && hasExternalReferences(uitem->m_image)) {
      umcount1++;
      umsize1 += (TUINT64)(itu->second->getSize() / 1024.0);
    } else if (uitem->m_cantCompress) {
      umcount2++;
      umsize2 += (TUINT64)(itu->second->getSize() / 1024.0);
    } else {
      umcount3++;
      umsize3 += (TUINT64)(itu->second->getSize() / 1024.0);
    }
  }
  std::map<std::string, CacheItemP>::iterator itc = m_compressedItems.begin();
  for (; itc != m_compressedItems.end(); ++itc) {
    CacheItemP boh                      = itc->second;
    CompressedOnMemoryCacheItemP cmitem = itc->second;
    CompressedOnDiskCacheItemP cditem   = itc->second;
    UncompressedOnDiskCacheItemP uditem = itc->second;
    if (cmitem) {
      cmcount++;
      cmsize += cmitem->getSize();
    } else if (cditem) {
      cdcount++;
      cdsize += cditem->getSize();
    } else {
      assert(uditem);
      udcount++;
      udsize += uditem->getSize();
    }
  }

  TUINT64 currPhisMemoryAvail =
      (TUINT64)(TSystem::getFreeMemorySize(true) / 1024.0);
  // TUINT64 currVirtualMemoryAvail = TSystem::getFreeMemorySize(false)/1024.0;

  os << "************************************************************\n";

  os << "***requested memory: " +
            std::to_string((int)chunkRequested / 1048576.0) + " MB\n";
  // os<<"*** memory in rasters: " +
  // std::to_string((int)TRaster::getTotalMemoryInKB()/1024.0) + " MB\n";
  // os<<"***virtualmem " + std::to_string((int)currVirtualMemoryAvail) + "
  // MB\n";
  os << "***phismem " + std::to_string((int)currPhisMemoryAvail) +
            " MB; percent of tot:" +
            std::to_string(
                (int)((currPhisMemoryAvail * 100) / m_reservedMemory)) +
            "\n";
  // os<<"***bigmem available" +
  // std::to_string((int)TBigMemoryManager::instance()->getAvailableMemoryinKb());
  os << "***uncompressed NOT compressable(refcount>1)   " +
            std::to_string(umcount1) + " " + std::to_string(umsize1 / 1024.0) +
            " MB\n";
  os << "***uncompressed NOT compressable(cantCompress)   " +
            std::to_string(umcount2) + " " + std::to_string(umsize2 / 1024.0) +
            " MB\n";
  os << "***uncompressed compressable   " + std::to_string(umcount3) + " " +
            std::to_string(umsize3 / 1024.0) + " MB\n";
  os << "***compressed on mem  " + std::to_string(cmcount) + " " +
            std::to_string((int)cmsize / 1048576.0) + " MB\n";
  os << "***compressed on disk " + std::to_string(cdcount) + " " +
            std::to_string((int)cdsize / 1048576.0) + " MB\n";
  os << "***uncompressed on disk " + std::to_string(udcount) + " " +
            std::to_string((int)udsize / 1048576.0) + " MB\n";

  // TBigMemoryManager::instance()->printMap();

  //#endif
}

//------------------------------------------------------------------------------

void TImageCache::compress(const std::string &id) { m_imp->doCompress(id); }

//------------------------------------------------------------------------------

#ifndef TNZCORE_LIGHT

void TImageCache::add(const QString &id, const TImageP &img, bool overwrite) {
  if (!isEnabled()) return;
  m_imp->add(id.toStdString(), img, overwrite);
}

//------------------------------------------------------------------------------

void TImageCache::remove(const QString &id) { m_imp->remove(id.toStdString()); }

//------------------------------------------------------------------------------

TImageP TImageCache::get(const QString &id, bool toBeModified) const {
  return get(id.toStdString(), toBeModified);
}

#endif

//*************************************************************************************
//    TCachedImage implementation
//*************************************************************************************

DEFINE_CLASS_CODE(TCachedImage, 103)

TCachedImage::TCachedImage()
    : TSmartObject(m_classCode)
    , m_ref(TImageCache::instance()->getUniqueId()) {}

//------------------------------------------------------------------------------

TCachedImage::TCachedImage(const TImageP &img)
    : TSmartObject(m_classCode), m_ref(TImageCache::instance()->getUniqueId()) {
  setImage(img);
}

//------------------------------------------------------------------------------

TCachedImage::~TCachedImage() { TImageCache::instance()->remove(m_ref); }

//------------------------------------------------------------------------------

void TCachedImage::setImage(const TImageP &img, bool overwrite) {
  TImageCache::instance()->add(m_ref, img, overwrite);
}

//------------------------------------------------------------------------------

TImageP TCachedImage::image(bool toBeModified) {
  return TImageCache::instance()->get(m_ref, toBeModified);
}
