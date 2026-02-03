

// String includes
#include "tconvert.h"

// Image includes
#include "timage_io.h"

// Toonz Image cache
#include "timagecache.h"

// TTile class
#include "ttile.h"

// Trop include
#include "trop.h"

// File I/O includes
// #include "tstream.h"

// Qt classes
#include <QRegion>
#include <QByteArray>

// Resources pool manager
#include "tcacheresourcepool.h"

#include "tcacheresource.h"

/*
//Debug
#define DIAGNOSTICS
#include "diagnostics.h"

//Debug stuff
namespace
{
  QString prefix("#resources.txt | RISORSE | ");
  QString prefixMem("#memory.txt | ");

  QString traduce(const TRectD& rect)
  {
    return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " "
               + QString::number(rect.x1) + " " + QString::number(rect.y1) +
"]";
  }

  QString traduce(const TRect& rect)
  {
    return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " "
               + QString::number(rect.x1+1) + " " + QString::number(rect.y1+1) +
"]";
  }

  QString traduce(const TTile& tile)
  {
    TDimension size(tile.getRaster()->getSize());
    TRectD tileRect(tile.m_pos, TDimensionD(size.lx,size.ly));
    return traduce(tileRect);
  }
}
*/

//====================================================================================================

/*
The TCacheResource class models the idea of sparsely defined tile objects.
Whereas common TTile instances can be used to represent images bounded on a
specific
rect of the plane, this model does not suit well in case the image object is
sparse across the image plane.
\n \n
The TCacheResource internal  representation assumes that the image plane is
split
in a fixed-length integer lattice, each cell of which may host an image object
in
case some part of the complex inside the cell has been defined.
The plane outside the complex is assumed to be transparent.
\n
The lattice origin must be specified in the complex's constructor, and all tiles
uploaded into and downloaded from it must have an integer displacement with
respect
to it - in other words, it is required that the complex has a well-defined pixel
geometry.
\n \n
Specific regions of the complex can be cleared through the apposite clear()
method.

\warning
This class does not ensure thread-safety. Concurrent accesses must be dealt by
users.
*/

//====================================================================================================

//  Docs stuff

/*! \fn QRegion TCacheResource::getAvailableRegion() const

Returns the current region currently available in the complex, with respect to
the complex's origin.
*/

/*! \fn int TCacheResource::getRasterType() const

Returns the type of raster currently present in the tile complex. Only images
providing a raster representation coherent with the complex's one may be
exchanged
with it.
*/

/*! \fn void TCacheResource::clear()
Clears the whole complex. This is an overloaded method equivalent to
clear(getAvailableRegion()), supplied for convenience.
*/

//====================================================================================================

//****************************************************************************************************
//    Preliminaries
//****************************************************************************************************

namespace {
// Store tile textures of 512 x 512 pixels. Their memory usage ranges around 1-2
// MB each.
const int latticeStep = 512;

static unsigned long cacheId = 0;

//-----------------------------------------------------------------

inline int getRasterType(const TRasterP &ras) {
  if ((TRaster32P)ras)
    return TCacheResource::RGBM32;
  else if ((TRaster64P)ras)
    return TCacheResource::RGBM64;
  else if ((TRasterFP)ras)
    return TCacheResource::RGBMFloat;
  else if ((TRasterCM32P)ras)
    return TCacheResource::CM32;

  return TCacheResource::NONE;
}

//-----------------------------------------------------------------

inline TRasterP getRaster(const TImageP &img) {
  TRasterImageP rimg(img);
  if (rimg) return rimg->getRaster();
  TToonzImageP timg(img);
  if (timg) return timg->getRaster();

  assert(!"Wrong image type!");
  return 0;
}

//-----------------------------------------------------------------

inline bool isEmpty(const TRect &rect) {
  return rect.x0 > rect.x1 || rect.y0 > rect.y1;
}

//-----------------------------------------------------------------

inline QRect toQRect(const TRect &r) {
  return QRect(r.x0, r.y0, r.getLx(), r.getLy());
}
inline TRect toTRect(const QRect &r) {
  return TRect(r.left(), r.top(), r.right(), r.bottom());
}
inline QPoint toQPoint(const TPoint &p) { return QPoint(p.x, p.y); }

//----------------------------------------------------------------------------

inline TRect getTileRect(const TTile &tile) {
  return TRect(TPoint(tfloor(tile.m_pos.x), tfloor(tile.m_pos.y)),
               tile.getRaster()->getSize());
}

//----------------------------------------------------------------------------

// Qt's contains actually returns QRegion::intersected... I wonder why...
inline bool contains(const QRegion &region, const TRect &rect) {
  return QRegion(toQRect(rect)).subtracted(region).isEmpty();
}

//----------------------------------------------------------------------------

inline void saveCompressed(const TFilePath &fp, const TRasterP &ras) {
  assert(ras->getLx() == latticeStep && ras->getLy() == latticeStep);
  unsigned int size = sq(latticeStep) * ras->getPixelSize();

  ras->lock();
  QByteArray data = qCompress((const char *)ras->getRawData(), size);
  ras->unlock();

  Tofstream oss(fp);
  oss.write((const char *)&size, sizeof(unsigned int));
  oss.write(data.constData(), data.size());
  assert(!oss.fail());
}

//----------------------------------------------------------------------------

inline void loadCompressed(const TFilePath &fp, TRasterP &ras,
                           TCacheResource::Type rasType) {
  Tifstream is(fp);

  if (rasType == TCacheResource::CM32)
    ras = TRasterCM32P(latticeStep, latticeStep);
  else if (rasType == TCacheResource::RGBM32)
    ras = TRaster32P(latticeStep, latticeStep);
  else if (rasType == TCacheResource::RGBM64)
    ras = TRaster64P(latticeStep, latticeStep);
  else if (rasType == TCacheResource::RGBMFloat)
    ras = TRasterFP(latticeStep, latticeStep);
  else
    assert(false);

  ras->lock();

  char *rawData = (char *)ras->getRawData();
  unsigned int dataSize;
  is.read((char *)&dataSize, sizeof(unsigned int));
  is.read(rawData, dataSize);

  // Observe that QByteArray::fromRawData does NOT cause a deep copy to occur.
  QByteArray data(QByteArray::fromRawData(rawData, dataSize));
  data = qUncompress(data);
  memcpy(rawData, data.constData(), data.size());

  ras->unlock();
}
}  // namespace

//****************************************************************************************************
//    TCacheResourceP implementation
//****************************************************************************************************

TCacheResourceP::TCacheResourceP(const std::string &imageName,
                                 bool createIfNone)
    : m_pointer(TCacheResourcePool::instance()->getResource(imageName,
                                                            createIfNone)) {
  if (m_pointer) m_pointer->addRef();
}

//----------------------------------------------------------------------------

TCacheResourceP::~TCacheResourceP() {
  if (m_pointer) {
    m_pointer->release();
    m_pointer = 0;
  }
}

//****************************************************************************************************
//    Member functions implementation
//****************************************************************************************************

//=====================
//    TCacheResource
//---------------------

TCacheResource::TCacheResource()
    : m_id(cacheId++)
    , m_tileType(NONE)
    , m_cellsCount(0)
    , m_locksCount(0)
    , m_backEnabled(false)
    , m_invalidated(false) {}

//-----------------------------------------------------------------

TCacheResource::~TCacheResource() { clear(); }

//-----------------------------------------------------------------

void TCacheResource::release() {
  if ((--m_refCount) <= 0) {
    // Attempt release from the resource pool
    TCacheResourcePool::instance()->releaseResource(this);
  }
}

//-----------------------------------------------------------------

inline TCacheResource::PointLess TCacheResource::getCellIndex(
    const TPoint &pos) const {
  return PointLess(tfloor(pos.x / (double)latticeStep),
                   tfloor(pos.y / (double)latticeStep));
}

//-----------------------------------------------------------------

inline TPoint TCacheResource::getCellPos(const PointLess &cellIndex) const {
  return TPoint(cellIndex.x * latticeStep, cellIndex.y * latticeStep);
}

//-----------------------------------------------------------------

// Returns the lattice cell containing the position pos.
inline TPoint TCacheResource::getCellPos(const TPoint &pos) const {
  TPoint cellIndex(tfloor(pos.x / (double)latticeStep),
                   tfloor(pos.y / (double)latticeStep));
  return TPoint(cellIndex.x * latticeStep, cellIndex.y * latticeStep);
}

//-----------------------------------------------------------------

// Returns the lattice cell containing the relative position pos.
inline TPoint TCacheResource::getCellPos(const TPointD &pos) const {
  TPoint cellIndex(tfloor(pos.x / (double)latticeStep),
                   tfloor(pos.y / (double)latticeStep));
  return TPoint(cellIndex.x * latticeStep, cellIndex.y * latticeStep);
}

//****************************************************************************************************
//    Tough stuff
//****************************************************************************************************

bool TCacheResource::checkRasterType(const TRasterP &ras, int &rasType) const {
  rasType = ::getRasterType(ras);
  if (rasType == NONE) {
    assert(!"The passed raster has unknown type!");
    return false;
  }
  if (m_tileType != NONE && m_tileType != rasType) {
    assert(!"The passed raster has not the same type of the cache resource!");
    return false;
  }

  return true;
}

//----------------------------------------------------------------

TRasterP TCacheResource::buildCompatibleRaster(const TDimension &size) {
  TRasterP result;
  if (m_tileType == RGBM32)
    result = TRaster32P(size);
  else if (m_tileType == RGBM64)
    result = TRaster64P(size);
  else if (m_tileType == RGBMFloat)
    result = TRasterFP(size);
  else if (m_tileType == CM32)
    result = TRasterCM32P(size);

  return result;
}

//----------------------------------------------------------------

bool TCacheResource::checkTile(const TTile &tile) const {
  // Ensure that tile has integer geoometry.
  TPointD tileFracPos(tile.m_pos.x - tfloor(tile.m_pos.x),
                      tile.m_pos.y - tfloor(tile.m_pos.y));
  if (tileFracPos.x != 0.0 || tileFracPos.y != 0.0) {
    assert(!"The passed tile must have integer geometry!");
    return false;
  }

  return true;
}

//----------------------------------------------------------------

inline std::string TCacheResource::getCellName(int idxX, int idxY) const {
  return "cell" + std::to_string(idxX) + "," + std::to_string(idxY);
}

//----------------------------------------------------------------

inline std::string TCacheResource::getCellCacheId(int idxX, int idxY) const {
  return "TCacheResource" + std::to_string(m_id) + getCellName(idxX, idxY);
}

//----------------------------------------------------------------

inline std::string TCacheResource::getCellCacheId(const TPoint &cellPos) const {
  return getCellCacheId(tfloor(cellPos.x / (double)latticeStep),
                        tfloor(cellPos.y / (double)latticeStep));
}

//-----------------------------------------------------------------

inline TRasterP TCacheResource::createCellRaster(int rasterType,
                                                 const std::string &cacheId) {
  TRasterP result;

  if (rasterType == TCacheResource::NONE) {
    assert(!"Unknown raster type!");
    return result;
  }

  TImageP img;
  if (rasterType == TCacheResource::RGBM32) {
    result = TRaster32P(latticeStep, latticeStep);
    img    = TRasterImageP(result);
  } else if (rasterType == TCacheResource::RGBM64) {
    result = TRaster64P(latticeStep, latticeStep);
    img    = TRasterImageP(result);
  } else if (rasterType == TCacheResource::RGBMFloat) {
    result = TRasterFP(latticeStep, latticeStep);
    img    = TRasterImageP(result);
  } else if (rasterType == TCacheResource::CM32) {
    result = TRasterCM32P(latticeStep, latticeStep);
    img    = TToonzImageP(result, result->getBounds());
  }

  TImageCache::instance()->add(cacheId, img);
  ++m_cellsCount;

  // DIAGNOSTICS_GLOADD("crCellsCnt", 1);

  return result;
}

//----------------------------------------------------------------

bool TCacheResource::canDownloadSome(const TRect &rect) const {
  return m_region.intersects(toQRect(rect));
}

//----------------------------------------------------------------

bool TCacheResource::canDownloadAll(const TRect &rect) const {
  return contains(m_region, rect);
}

//----------------------------------------------------------------

bool TCacheResource::canUpload(const TTile &tile) const {
  int tileType;
  return checkTile(tile) && checkRasterType(tile.getRaster(), tileType);
}

//----------------------------------------------------------------

//! Returns true if the passed tile is compatible with the complex, and some
//! part of it is downloadable.
bool TCacheResource::canDownloadSome(const TTile &tile) const {
  return checkTile(tile) && m_region.intersects(toQRect(getTileRect(tile)));
}

//----------------------------------------------------------------

//! Returns true if the passed tile is compatible, and it can be downloaded
//! entirely.
bool TCacheResource::canDownloadAll(const TTile &tile) const {
  return checkTile(tile) && contains(m_region, getTileRect(tile));
}

//----------------------------------------------------------------

//! Copies the passed tile in the tile complex. The passed tile \b must
//! possess integer geometry (ie tile.m_pos must have integer coordinates),
//! otherwise this function is a no-op.
bool TCacheResource::upload(const TPoint &pos, TRasterP ras) {
  int tileType;
  if (!checkRasterType(ras, tileType)) return false;

  if (m_tileType == NONE) m_tileType = tileType;

  // For all cells of the lattice which intersect the tile, upload the content
  // in the
  // complex
  TRect tileRect(ras->getBounds() + pos);
  TPoint initialPos(getCellPos(tileRect.getP00()));

  // DIAGNOSTICS_NUMBEREDSTRSET(prefix + QString::number((UINT) this) + " |
  // Stack | ",
  //"crStack", "upload", ::traduce(TRect(pos, ras->getSize())));

  TPoint currPos;
  for (currPos.x = initialPos.x; currPos.x <= tileRect.x1;
       currPos.x += latticeStep)
    for (currPos.y = initialPos.y; currPos.y <= tileRect.y1;
         currPos.y += latticeStep) {
      // Copy tile's content into the cell's raster.
      TRect cellRect(currPos, TDimension(latticeStep, latticeStep));

      TRect overlapRect(tileRect * cellRect);
      assert(!overlapRect.isEmpty());

      PointLess cellIndex(getCellIndex(currPos));
      std::pair<TRasterP, CellData *> cellInfos(touch(cellIndex));
      TRasterP cellRas(cellInfos.first);

      TRect temp(overlapRect - currPos);
      TRasterP overlappingCellRas(cellRas->extract(temp));
      temp = TRect(overlapRect - tileRect.getP00());
      TRasterP overlappingTileRas(ras->extract(temp));

      assert(overlappingCellRas->getBounds() ==
             overlappingTileRas->getBounds());
      TRop::copy(overlappingCellRas, overlappingTileRas);

      cellInfos.second->m_modified = true;
    }

  // Update the complex's content region
  m_region += toQRect(tileRect);

  return true;
}

//----------------------------------------------------------------

bool TCacheResource::upload(const TTile &tile) {
  if (!checkTile(tile)) return false;

  return upload(TPoint(tile.m_pos.x, tile.m_pos.y), tile.getRaster());
}

//----------------------------------------------------------------

//! Fills the passed tile with the data contained in the complex, returning
//! the copied region.
//! The same restriction of the upload() method applies here.
QRegion TCacheResource::download(const TPoint &pos, TRasterP ras) {
  int tileType;
  if (!checkRasterType(ras, tileType)) return QRegion();

  // Build the tile's rect
  TRect tileRect(ras->getBounds() + pos);

  if (!m_region.intersects(toQRect(tileRect))) return QRegion();

  // For all cells intersecting the tile's rect, copy all those intersecting the
  // complex's content region.
  TPoint initialPos(getCellPos(tileRect.getP00()));

  TPoint currPos;
  for (currPos.x = initialPos.x; currPos.x <= tileRect.x1;
       currPos.x += latticeStep)
    for (currPos.y = initialPos.y; currPos.y <= tileRect.y1;
         currPos.y += latticeStep) {
      TRect cellRect(currPos, TDimension(latticeStep, latticeStep));

      TRect overlapRect(tileRect * cellRect);
      assert(!overlapRect.isEmpty());
      QRect overlapQRect(toQRect(overlapRect));

      if (m_region.intersects(overlapQRect)) {
        // Extract the associated rasters and perform the copy to the input
        // tile.
        std::pair<TRasterP, CellData *> cellInfos(touch(getCellIndex(currPos)));
        TRasterP cellRas(cellInfos.first);

        TRect temp(overlapRect - currPos);
        TRasterP overlappingCellRas(cellRas->extract(temp));
        temp = TRect(overlapRect - tileRect.getP00());
        TRasterP overlappingTileRas(ras->extract(temp));

        TRop::copy(overlappingTileRas, overlappingCellRas);
      }
    }

  return m_region.intersected(QRegion(toQRect(tileRect)));
}

//----------------------------------------------------------------

QRegion TCacheResource::download(TTile &tile) {
  if (!checkTile(tile)) return QRegion();

  return download(TPoint(tile.m_pos.x, tile.m_pos.y), tile.getRaster());
}

//----------------------------------------------------------------

bool TCacheResource::downloadAll(const TPoint &pos, TRasterP ras) {
  int tileType;
  if (!checkRasterType(ras, tileType)) return false;

  // Build the tile's rect
  TRect tileRect(ras->getBounds() + pos);

  if (!contains(m_region, tileRect)) return false;

  // DIAGNOSTICS_NUMBEREDSTRSET(prefix + QString::number((UINT) this) + " |
  // Stack | ",
  //"crStack", "downloadAll", ::traduce(TRect(pos, ras->getSize())));

  // For all cells intersecting the tile's rect, copy all those intersecting the
  // complex's content region.
  TPoint initialPos(getCellPos(tileRect.getP00()));

  TPoint currPos;
  for (currPos.x = initialPos.x; currPos.x <= tileRect.x1;
       currPos.x += latticeStep)
    for (currPos.y = initialPos.y; currPos.y <= tileRect.y1;
         currPos.y += latticeStep) {
      TRect cellRect(currPos, TDimension(latticeStep, latticeStep));

      TRect overlapRect(tileRect * cellRect);
      assert(!overlapRect.isEmpty());
      QRect overlapQRect(toQRect(overlapRect));

      if (m_region.intersects(overlapQRect)) {
        // Extract the associated rasters and perform the copy to the input
        // tile.
        std::pair<TRasterP, CellData *> cellInfos(touch(getCellIndex(currPos)));
        TRasterP cellRas(cellInfos.first);

        TRect temp(overlapRect - currPos);
        TRasterP overlappingCellRas(cellRas->extract(temp));
        temp = TRect(overlapRect - tileRect.getP00());
        TRasterP overlappingTileRas(ras->extract(temp));

        TRop::copy(overlappingTileRas, overlappingCellRas);
      }
    }

  return true;
}

//----------------------------------------------------------------

bool TCacheResource::downloadAll(TTile &tile) {
  if (!checkTile(tile)) return false;

  return downloadAll(TPoint(tile.m_pos.x, tile.m_pos.y), tile.getRaster());
}

//-----------------------------------------------------------------

//! Clears the complex on the specified region. Please observe that the actually
//! cleared region
//! consists of all lattice cells intersecting the passed region, therefore
//! resulting in a cleared region
//! typically larger than passed one, up to the lattice granularity.
void TCacheResource::clear(QRegion region) {
  if (!m_region.intersects(region)) return;

  // Get the region bbox
  TRect bbox(toTRect(region.boundingRect()));

  // For all cells intersecting the bbox
  TPoint initialPos(getCellPos(bbox.getP00()));
  TPoint pos;
  for (pos.x = initialPos.x; pos.x <= bbox.x1; pos.x += latticeStep)
    for (pos.y = initialPos.y; pos.y <= bbox.y1; pos.y += latticeStep) {
      QRect cellQRect(
          toQRect(TRect(pos, TDimension(latticeStep, latticeStep))));

      if (region.intersects(cellQRect) && m_region.intersects(cellQRect)) {
        // Release the associated cell from cache and clear the cell from the
        // content region.
        TImageCache::instance()->remove(getCellCacheId(pos));
        m_region -= cellQRect;

        --m_cellsCount;

        // DIAGNOSTICS_GLOADD("crCellsCnt", -1);

        // Release the cell from m_cellDatas
        m_cellDatas[getCellIndex(pos)].m_modified = true;
      }
    }

  if (m_region.isEmpty()) {
    m_tileType   = NONE;
    m_locksCount = 0;
  }
}

//****************************************************************************************************
//    Palette management
//****************************************************************************************************

bool TCacheResource::uploadPalette(TPaletteP palette) {
  if (m_tileType == NONE) m_tileType = CM32;

  if (m_tileType != CM32) {
    assert(!"The resource already holds a non-colormap content!");
    return false;
  }

  m_palette = palette;
  return true;
}

//-----------------------------------------------------------------

void TCacheResource::downloadPalette(TPaletteP &palette) {
  palette = m_palette;
}

//****************************************************************************************************
//    References management
//****************************************************************************************************

void TCacheResource::addRef2(const TRect &rect) {
  // DIAGNOSTICS_NUMBEREDSTRSET(prefix + QString::number((UINT) this) + " |
  // Stack | ",
  //"crStack", "addRef", ::traduce(rect));

  // Add a reference to all cells intersecting the passed one
  TPoint initialPos(getCellPos(rect.getP00()));
  TPoint pos;
  for (pos.x = initialPos.x; pos.x <= rect.x1; pos.x += latticeStep)
    for (pos.y = initialPos.y; pos.y <= rect.y1; pos.y += latticeStep) {
      PointLess cellIndex(getCellIndex(pos));
      CellData &cellData    = m_cellDatas[cellIndex];
      cellData.m_referenced = true;
      cellData.m_refsCount++;
    }
}

//-----------------------------------------------------------------

void TCacheResource::release2(const TRect &rect) {
  // DIAGNOSTICS_NUMBEREDSTRSET(prefix + QString::number((UINT) this) + " |
  // Stack | ",
  //"crStack", "release", ::traduce(rect));

  if (m_locksCount > 0) return;

  std::map<PointLess, CellData>::iterator it;
  for (it = m_cellDatas.begin(); it != m_cellDatas.end();) {
    if (!it->second.m_referenced) {
      ++it;
      continue;
    }

    TPoint cellPos(getCellPos(it->first));
    TRect cellRect(cellPos, TDimension(latticeStep, latticeStep));

    if (isEmpty(cellRect * rect)) {
      ++it;
      continue;
    }

    QRect cellQRect(toQRect(cellRect));
    if (--it->second.m_refsCount <= 0) {
      releaseCell(cellQRect, it->first, it->second.m_modified);
      std::map<PointLess, CellData>::iterator jt = it++;
      m_cellDatas.erase(jt);
    } else
      ++it;
  }
}

//-----------------------------------------------------------------

void TCacheResource::addLock() {
  // DIAGNOSTICS_NUMBEREDSTR(prefix + QString::number((UINT) this) + " | Stack |
  // ",
  //"crStack", "addLock");

  ++m_locksCount;
}

//-----------------------------------------------------------------

void TCacheResource::releaseLock() {
  // DIAGNOSTICS_NUMBEREDSTR(prefix + QString::number((UINT) this) + " | Stack |
  // ",
  //"crStack", "releaseLock");

  m_locksCount = std::max(m_locksCount - 1, 0);

  if (m_locksCount > 0) return;

  // Check for released cells
  std::map<PointLess, CellData>::iterator it;
  for (it = m_cellDatas.begin(); it != m_cellDatas.end();)
    if (it->second.m_referenced) {
      TPoint cellPos(getCellPos(it->first));
      QRect cellQRect(cellPos.x, cellPos.y, latticeStep, latticeStep);

      releaseCell(cellQRect, it->first, it->second.m_modified);
      std::map<PointLess, CellData>::iterator jt = it++;
      m_cellDatas.erase(jt);
    } else
      ++it;
}

//-----------------------------------------------------------------

void TCacheResource::releaseCell(const QRect &cellQRect,
                                 const PointLess &cellIndex, bool doSave) {
  if (m_region.intersects(cellQRect)) {
    std::string cellCacheId(getCellCacheId(cellIndex.x, cellIndex.y));

    if (!(doSave && save(cellIndex))) m_region -= cellQRect;

    TImageCache::instance()->remove(cellCacheId);
    --m_cellsCount;

    // DIAGNOSTICS_GLOADD("crCellsCnt", -1);
  }
}

//-----------------------------------------------------------------

//! Returns the current size, in MB, of the cache resource.
int TCacheResource::size() const {
  // NOTE: It's better to store the size incrementally. This complies
  // with the possibility of specifying a bbox to fit the stored cells to...

  return m_tileType == NONE        ? 0
         : m_tileType == RGBM64    ? (m_cellsCount << 11)
         : m_tileType == RGBMFloat ? (m_cellsCount << 12)
                                   : (m_cellsCount << 10);
}

//****************************************************************************************************
//    Hard disk backing procedures
//****************************************************************************************************

void TCacheResource::enableBackup() {
  if (m_backEnabled) return;
  TCacheResourcePool::instance()->startBacking(this);
}

//-----------------------------------------------------------------

void TCacheResource::invalidate() { m_invalidated = true; }

//-----------------------------------------------------------------

void TCacheResource::setPath(const TFilePath &path) { m_path = path; }

//-----------------------------------------------------------------

const TFilePath &TCacheResource::getPath() const { return m_path; }

//-----------------------------------------------------------------

bool TCacheResource::save(const PointLess &cellIndex, TRasterP cellRas) const {
  if (!m_backEnabled || m_invalidated) return false;

  assert(!m_path.isEmpty());

  if (!cellRas)
    cellRas = getRaster(TImageCache::instance()->get(
        getCellCacheId(cellIndex.x, cellIndex.y), false));

  assert(m_tileType != NONE);

  TFilePath fp(TCacheResourcePool::instance()->getPath() + m_path +
               getCellName(cellIndex.x, cellIndex.y));

  if (m_tileType == CM32) {
    ::saveCompressed(fp, cellRas);
  } else {
    TImageWriter::save(fp.withType(".tif"), cellRas);
  }

  return true;
}

//-----------------------------------------------------------------

TRasterP TCacheResource::load(const PointLess &cellPos) {
  if (m_path.isEmpty()) return 0;

  TFilePath cellPath(TCacheResourcePool::instance()->getPath() + m_path +
                     TFilePath(getCellName(cellPos.x, cellPos.y)));
  TRasterP ras;
  if (m_tileType == CM32) {
    ::loadCompressed(cellPath, ras, CM32);
  } else {
    TImageReader::load(cellPath.withType(".tif"), ras);
  }

  return ras;
}

//-----------------------------------------------------------------

std::pair<TRasterP, TCacheResource::CellData *> TCacheResource::touch(
    const PointLess &cellIndex) {
  std::string cellId(getCellCacheId(cellIndex.x, cellIndex.y));

  std::map<PointLess, CellData>::iterator it = m_cellDatas.find(cellIndex);
  if (it != m_cellDatas.end()) {
    // Retrieve the raster from image cache
    TImageP img(TImageCache::instance()->get(cellId, true));
    if (img) return std::make_pair(getRaster(img), &it->second);
  }

  it = m_cellDatas.insert(std::make_pair(cellIndex, CellData())).first;

  // Then, attempt retrieval from back resource
  TRasterP ras(load(cellIndex));
  if (ras) {
    TImageCache::instance()->add(cellId, TRasterImageP(ras));
    return std::make_pair(ras, &it->second);
  }

  // Else, create it
  return std::make_pair(
      createCellRaster(m_tileType, cellId),  // increases m_cellsCount too
      &it->second);
}

//-----------------------------------------------------------------

void TCacheResource::save() {
  if (m_backEnabled && !m_invalidated) {
    assert(!m_path.isEmpty());

    // Save each modified cell raster
    std::map<PointLess, CellData>::iterator it;
    for (it = m_cellDatas.begin(); it != m_cellDatas.end(); ++it) {
      if (it->second.m_modified) save(it->first);
    }

    // Save the palette, if any
    // SHOULD BE MOVED TO THE CACHERESOURCEPOOL!!
    /*if(m_palette)
{
TFilePath fp(TCacheResourcePool::instance()->getPath() + m_path);
TOStream oss(fp);

m_palette->saveData(oss);
}*/
  }
}

//-----------------------------------------------------------------

void TCacheResource::save(const TFilePath &fp) {
  assert(!fp.isEmpty());

  std::map<PointLess, CellData>::iterator it;
  for (it = m_cellDatas.begin(); it != m_cellDatas.end(); ++it) {
    TRasterP cellRas = getRaster(TImageCache::instance()->get(
        getCellCacheId(it->first.x, it->first.y), false));

    assert(m_tileType != NONE);

    TFilePath cellFp(fp + TFilePath(getCellName(it->first.x, it->first.y)));

    if (m_tileType == CM32)
      ::saveCompressed(cellFp, cellRas);
    else
      TImageWriter::save(cellFp.withType(".tif"), cellRas);
  }
}

//-----------------------------------------------------------------

void TCacheResource::clear() {
  std::map<PointLess, CellData>::iterator it;
  for (it = m_cellDatas.begin(); it != m_cellDatas.end(); ++it) {
    std::string cellCacheId(getCellCacheId(it->first.x, it->first.y));
    TImageCache::instance()->remove(cellCacheId);
  }

  m_cellDatas.clear();
}

/*
//
***************************************************************************************************
//    Disk reference
//
***************************************************************************************************

#include <QSettings>

TCacheResource::DiskReference::DiskReference(const TFilePath& fp)
{
  QSettings settings(
    QString::fromStdWString((TCacheResourcePool::instance()->getPath() + m_path
+ "resource.ini").getWideString()),
    QSettings::IniFormat);

  settings.setValue("MemReference", 1);
}

//-----------------------------------------------------------------

TCacheResource::DiskReference::~DiskReference()
{
  QSettings settings(
    QString::fromStdWString((TCacheResourcePool::instance()->getPath() + m_path
+ "resource.ini").getWideString()),
    QSettings::IniFormat);

  int diskReference = settings.value("DiskReference").toInt();
  if(diskReference == 0)
    TCacheResourcePool::instance()->clearResource(QString::fromStdWString(m_path.getWideString()));
}
*/
