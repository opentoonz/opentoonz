

// Toonz core includes
#include "timagecache.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tmeshimage.h"
#include "timage_io.h"

// Qt includes (mutexing classes)
#include <QMutex>
#include <QMutexLocker>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>

#include "toonz/imagemanager.h"
#include "toonz/txshsimplelevel.h"

/* EXPLANATION (by Daniele):

  Images / Image Infos retrieval is quite a frequent task throughout Toonz - in
  particular,
  as Render operations tend to be multithreaded, it is important to ensure that
  the
  ImageManager treats these operations efficiently.

  Most of the image manager's job is that of caching hard-built image data so
  that successive
  queries avoid rebuilding the same images again.

  In a multithreaded environment, we must make sure that multiple threads block
  each other out
  as little as possible.

  Here are the main performance and threading notes:

    - Image infos are completely cached, while image cachability is
  user-specified.
      This is needed as some images must be loaded only temporarily.

    - One mutex will be used to protect the bindings table. It is the outermost
  mutex.

    - One mutex (read/write lock) will be used to protect access to EACH
  individual image.
      Testing should be required. If file access is found to be too strictly
  sequential,
      perhaps a single mutex could suffice.

    - Having different mutexes to protect images and image infos is currently
  not implemented,
      but could be. Testing required.
*/

/*
  TODO: TXshSimpleLevel::setFrame(...) usa aggiunte/rimozioni manuali di
  immagini associate
  a binding nell'ImageManager - aspettandosi che poiche' l'immagine e' presente
  in cache,
  verra' beccata...
*/

//************************************************************************************
//    Image Builder implementation
//************************************************************************************

DEFINE_CLASS_CODE(ImageBuilder, 100)

//-----------------------------------------------------------------------------

ImageBuilder::ImageBuilder()
    : TSmartObject(m_classCode)
    , m_imageBuildingLock(QReadWriteLock::Recursive)
    , m_cached(false)
    , m_modified(false)
    , m_imFlags(ImageManager::none) {}

//-----------------------------------------------------------------------------

ImageBuilder::~ImageBuilder() {}

//-----------------------------------------------------------------------------

bool ImageBuilder::areInfosCompatible(int imFlags, void *extData) {
  return m_info.m_valid;
}

//-----------------------------------------------------------------------------

bool ImageBuilder::isImageCompatible(int imFlags, void *extData) {
  return m_info.m_valid;
}

//-----------------------------------------------------------------------------

bool ImageBuilder::setImageInfo(TImageInfo &info, const TDimension &size) {
  info         = TImageInfo();
  info.m_lx    = size.lx;
  info.m_ly    = size.ly;
  info.m_x0    = 0;
  info.m_y0    = 0;
  info.m_x1    = size.lx - 1;
  info.m_y1    = size.ly - 1;
  info.m_valid = true;

  return true;
}

//-----------------------------------------------------------------------------

bool ImageBuilder::setImageInfo(TImageInfo &info, TImage *img) {
  info = TImageInfo();
  if (TRasterImageP ri = TRasterImageP(img)) {
    TRasterP ras = ri->getRaster();
    info.m_lx    = ras->getLx();
    info.m_ly    = ras->getLy();
    ri->getDpi(info.m_dpix, info.m_dpiy);
    TRect savebox = ri->getSavebox();
    info.m_x0     = savebox.x0;
    info.m_y0     = savebox.y0;
    info.m_x1     = savebox.x1;
    info.m_y1     = savebox.y1;
  } else if (TToonzImageP ti = TToonzImageP(img)) {
    TRasterP ras = ti->getRaster();
    info.m_lx    = ras->getLx();
    info.m_ly    = ras->getLy();
    ti->getDpi(info.m_dpix, info.m_dpiy);
    TRect savebox = ti->getSavebox();
    info.m_x0     = savebox.x0;
    info.m_y0     = savebox.y0;
    info.m_x1     = savebox.x1;
    info.m_y1     = savebox.y1;
  } else if (TMeshImageP mi = TMeshImageP(img)) {
    mi->getDpi(info.m_dpix, info.m_dpiy);
  }

  info.m_valid = true;
  return true;
}

//-----------------------------------------------------------------------------

bool ImageBuilder::setImageInfo(TImageInfo &info, TImageReader *ir) {
  info = TImageInfo();

  const TImageInfo *tmp = ir->getImageInfo();
  if (tmp) {
    info = *tmp;
    if (info.m_x1 < info.m_x0 || info.m_y1 < info.m_y0) {
      info.m_x0 = info.m_y0 = 0;
      info.m_x1             = info.m_lx - 1;
      info.m_y1             = info.m_ly - 1;
    }

    info.m_valid = true;
    return true;
  }

  return false;
}

//************************************************************************************
//    Image Manager Privates implementation
//************************************************************************************

struct ImageManager::Imp {
  QReadWriteLock m_tableLock;  //!< Lock for the builders table
  std::map<std::string, ImageBuilderP>
      m_builders;  //!< identifier -> ImageBuilder table

public:
  Imp() : m_tableLock(QReadWriteLock::Recursive) {}

  void clear() { m_builders.clear(); }
};

//************************************************************************************
//    Image Manager implementation
//************************************************************************************

ImageManager::ImageManager() : m_imp(new Imp) {}

//-----------------------------------------------------------------------------

ImageManager::~ImageManager() {}

//-----------------------------------------------------------------------------

ImageManager *ImageManager::instance() {
  // Re-introdotto possibile baco: voglio controllare se esiste ancora
  static ImageManager theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------------

void ImageManager::bind(const std::string &id, ImageBuilder *builderPtr) {
  if (!builderPtr) {
    unbind(id);
    return;
  }

  QWriteLocker locker(&m_imp->m_tableLock);

  ImageBuilderP &builderP = m_imp->m_builders[id];
  if (builderP && builderP->m_cached) TImageCache::instance()->remove(id);

  builderP = builderPtr;
}

//-----------------------------------------------------------------------------

bool ImageManager::unbind(const std::string &id) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  if (it == m_imp->m_builders.end()) return false;

  ImageBuilderP &builderP = it->second;
  if (builderP && builderP->m_cached) TImageCache::instance()->remove(id);

  m_imp->m_builders.erase(it);
  return true;
}

//-----------------------------------------------------------------------------

bool ImageManager::isBound(const std::string &id) const {
  QReadLocker locker(&m_imp->m_tableLock);
  return m_imp->m_builders.find(id) != m_imp->m_builders.end();
}

//-----------------------------------------------------------------------------

bool ImageManager::rebind(const std::string &srcId, const std::string &dstId) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator st =
      m_imp->m_builders.find(srcId);
  if (st == m_imp->m_builders.end()) return false;

  ImageBuilderP builder = st->second;

  m_imp->m_builders.erase(st);
  m_imp->m_builders[dstId] = builder;

  m_imp->m_builders[dstId]->m_cached   = true;
  m_imp->m_builders[dstId]->m_modified = true;

  TImageCache::instance()->remap(dstId, srcId);

  return true;
}

bool ImageManager::renumber(const std::string &srcId, const TFrameId &fid) {
  std::map<std::string, ImageBuilderP>::iterator st =
      m_imp->m_builders.find(srcId);
  if (st == m_imp->m_builders.end()) return false;

  m_imp->m_builders[srcId]->setFid(fid);

  return true;
}

//-----------------------------------------------------------------------------

void ImageManager::clear() {
  QWriteLocker locker(&m_imp->m_tableLock);

  TImageCache::instance()->clearSceneImages();
  m_imp->clear();
}

//-----------------------------------------------------------------------------

TImageInfo *ImageManager::getInfo(const std::string &id, int imFlags,
                                  void *extData) {
  // Lock for table read and try to find data in the cache
  QReadLocker tableLocker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  if (it == m_imp->m_builders.end()) return 0;

  ImageBuilderP &builder = it->second;

  assert(!((imFlags & ImageManager::toBeModified) && !builder->m_modified));

  // Check cached data
  if (builder->areInfosCompatible(imFlags, extData)) return &builder->m_info;

  QWriteLocker imageBuildingLocker(&builder->m_imageBuildingLock);

  // Re-check as waiting may have changed the situation
  if (builder->areInfosCompatible(imFlags, extData)) return &builder->m_info;

  TImageInfo info;
  if (builder->getInfo(info, imFlags, extData)) {
    builder->m_info = info;
    return &builder->m_info;
  }

  return 0;
}

//-----------------------------------------------------------------------------

TImageP ImageManager::getImage(const std::string &id, int imFlags,
                               void *extData) {
  assert(!((imFlags & ImageManager::toBeModified) &&
           (imFlags & ImageManager::dontPutInCache)));
  assert(!((imFlags & ImageManager::toBeModified) &&
           (imFlags & ImageManager::toBeSaved)));

  // Lock for table read and try to find data in the cache
  QReadLocker tableLocker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  if (it == m_imp->m_builders.end()) return TImageP();

  ImageBuilderP &builder = it->second;
  bool modified          = builder->m_modified;

  // Analyze imFlags
  bool _putInCache =
      TImageCache::instance()->isEnabled() && !(bool)(imFlags & dontPutInCache);
  bool _toBeModified = (imFlags & toBeModified);
  bool _toBeSaved    = (imFlags & toBeSaved);

  // Update the modified flag according to the specified flags
  if (_toBeModified)
    builder->m_modified = true;
  else if (_toBeSaved)
    builder->m_modified = false;

  // Now, fetch the image.
  TImageP img;

  if (builder->m_cached) {
    if (modified || builder->isImageCompatible(imFlags, extData)) {
      img = TImageCache::instance()->get(id, _toBeModified);

      assert(img);
      if (img) return img;
    }
  }

  // Lock for image building
  QWriteLocker imageBuildingLocker(&builder->m_imageBuildingLock);

  // As multiple threads may block on filesLocker, re-check if the image is now
  // available
  if (builder->m_cached) {
    if (modified || builder->isImageCompatible(imFlags, extData)) {
      img = TImageCache::instance()->get(id, _toBeModified);

      assert(img);
      if (img) return img;
    }
  }

  // The image was either not available or not conforming to the required
  // specifications.
  // We have to build it now, then.

  // Build the image
  img = builder->build(imFlags, extData);

  if (img && _putInCache) {
    builder->m_cached   = true;
    builder->m_modified = _toBeModified;

    TImageCache::instance()->add(id, img, true);
  }

  return img;
}

//-----------------------------------------------------------------------------
// load icon (and image) data of all frames into cache
void ImageManager::loadAllTlvIconsAndPutInCache(
    TXshSimpleLevel *level, std::vector<TFrameId> fids,
    std::vector<std::string> iconIds, bool cacheImagesAsWell) {
  if (fids.empty() || iconIds.empty()) return;
  // number of fid and iconId should be the same
  if ((int)fids.size() != (int)iconIds.size()) return;

  // obtain ImageLoader with the first fId
  TImageInfo info;
  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(level->getImageId(fids[0]));
  if (it != m_imp->m_builders.end()) {
    const ImageBuilderP &builder = it->second;
    assert(builder);
    assert(builder->getRefCount() > 0);

    // this function in reimpremented only in ImageLoader
    builder->buildAllIconsAndPutInCache(level, fids, iconIds,
                                        cacheImagesAsWell);
    builder->getInfo(info, ImageManager::none, 0);
  }
  if (cacheImagesAsWell) {
    // reset the savebox
    info.m_x0 = info.m_y0 = 0;
    info.m_x1             = info.m_lx - 1;
    info.m_y1             = info.m_ly - 1;

    // put flags to all builders
    for (int f = 0; f < fids.size(); f++) {
      std::map<std::string, ImageBuilderP>::iterator it =
          m_imp->m_builders.find(level->getImageId(fids[f]));
      if (it != m_imp->m_builders.end()) {
        const ImageBuilderP &builder = it->second;
        builder->setImageCachedAndModified();
        builder->m_info = info;
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool ImageManager::invalidate(const std::string &id) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  if (it == m_imp->m_builders.end()) return false;

  ImageBuilderP &builder = it->second;

  builder->invalidate();
  builder->m_cached = builder->m_modified = false;

  TImageCache::instance()->remove(id);

  return true;
}

//-----------------------------------------------------------------------------

bool ImageManager::setImage(const std::string &id, const TImageP &img) {
  if (!img) return invalidate(id);

  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  if (it == m_imp->m_builders.end()) return false;

  ImageBuilderP &builder = it->second;

  builder->invalidate();  // WARNING: Not all infos are correctly restored
  ImageBuilder::setImageInfo(
      builder->m_info,
      img.getPointer());  // from supplied image - must investigate further...

  TImageCache::instance()->add(id, img, true);
  builder->m_cached = builder->m_modified = true;

  return true;
}

//-----------------------------------------------------------------------------

ImageBuilder *ImageManager::getBuilder(const std::string &id) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  return (it == m_imp->m_builders.end()) ? (ImageBuilder *)0
                                         : it->second.getPointer();
}

//-----------------------------------------------------------------------------

bool ImageManager::isCached(const std::string &id) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  return (it == m_imp->m_builders.end()) ? false : it->second->m_cached;
}

//-----------------------------------------------------------------------------

bool ImageManager::isModified(const std::string &id) {
  QWriteLocker locker(&m_imp->m_tableLock);

  std::map<std::string, ImageBuilderP>::iterator it =
      m_imp->m_builders.find(id);
  return (it == m_imp->m_builders.end()) ? false : it->second->m_modified;
}
