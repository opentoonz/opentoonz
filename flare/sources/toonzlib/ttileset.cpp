

#include "toonz/ttileset.h"
#include "tcodec.h"
#include "timagecache.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
//------------------------------------------------------------------------------------------

TTileSet::Tile::Tile() : m_rasterBounds(TRect()), m_dim(), m_pixelSize(0) {}

//------------------------------------------------------------------------------------------

TTileSet::Tile::Tile(const TRasterP &ras, const TPoint &p)
    : m_rasterBounds(ras->getBounds() + p)
    , m_dim(ras->getSize())
    , m_pixelSize(ras->getPixelSize()) {}

//------------------------------------------------------------------------------------------

TTileSet::Tile::~Tile() {}

//------------------------------------------------------------------------------------------

TTileSet::~TTileSet() { clearPointerContainer(m_tiles); }

//------------------------------------------------------------------------------------------

void TTileSet::add(Tile *tile) { m_tiles.push_back(tile); }

//------------------------------------------------------------------------------------------

void TTileSet::getRects(std::vector<TRect> &rects) const {
  for (Tiles::const_iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
    rects.push_back((*it)->m_rasterBounds);
}

//------------------------------------------------------------------------------------------

TRect TTileSet::getBBox() const {
  if (m_tiles.empty()) {
    return TRect();
  }
  Tiles::const_iterator it = m_tiles.begin();
  TRect bbox               = (*it)->m_rasterBounds;
  for (; it != m_tiles.end(); ++it) bbox += (*it)->m_rasterBounds;
  return bbox;
}

//------------------------------------------------------------------------------------------

int TTileSet::getMemorySize() const {
  int i, size = 0;
  for (i = 0; i < m_tiles.size(); i++) {
    size += m_tiles[i]->getSize();
  }
  return size;
}

//******************************************************************************************

TTileSetCM32::Tile::Tile() : TTileSet::Tile() {}

//------------------------------------------------------------------------------------------

TTileSetCM32::Tile::Tile(const TRasterCM32P &ras, const TPoint &p)
    : TTileSet::Tile(TRasterP(ras), p) {
  TImageCache::instance()->add(id(), TToonzImageP(ras, ras->getBounds()));
}

//------------------------------------------------------------------------------------------

TTileSetCM32::Tile::~Tile() { TImageCache::instance()->remove(id()); }

//------------------------------------------------------------------------------------------

TTileSetCM32::~TTileSetCM32() {}

//------------------------------------------------------------------------------------------

void TTileSetCM32::Tile::getRaster(TRasterCM32P &ras) const {
  TToonzImageP timg = (TToonzImageP)TImageCache::instance()->get(id(), true);
  if (!timg) return;
  ras = timg->getRaster();
  assert(ras);
}

//------------------------------------------------------------------------------------------

TTileSetCM32::Tile *TTileSetCM32::Tile::clone() const {
  Tile *tile           = new Tile();
  tile->m_rasterBounds = m_rasterBounds;
  TToonzImageP timg    = (TToonzImageP)TImageCache::instance()->get(id(), true);
  if (!timg) return tile;
  TImageCache::instance()->add(tile->id(), timg->clone());
  return tile;
}

//------------------------------------------------------------------------------------------

void TTileSetCM32::add(const TRasterP &ras, TRect rect) {
  TRect bounds = ras->getBounds();
  if (!bounds.overlaps(rect)) return;
  rect *= bounds;
  assert(!rect.isEmpty());
  assert(bounds.contains(rect));
  TTileSet::add(new Tile(ras->extract(rect)->clone(), rect.getP00()));
}

//------------------------------------------------------------------------------------------

const TTileSetCM32::Tile *TTileSetCM32::getTile(int index) const {
  assert(0 <= index && index < getTileCount());
  TTileSetCM32::Tile *tile = dynamic_cast<TTileSetCM32::Tile *>(m_tiles[index]);
  assert(tile);
  return tile;
}

//------------------------------------------------------------------------------------------

TTileSetCM32::Tile *TTileSetCM32::editTile(int index) const {
  assert(0 <= index && index < getTileCount());
  TTileSetCM32::Tile *tile = dynamic_cast<TTileSetCM32::Tile *>(m_tiles[index]);
  assert(tile);
  return tile;
}

//------------------------------------------------------------------------------------------

TTileSetCM32 *TTileSetCM32::clone() const {
  TTileSetCM32 *tileSet    = new TTileSetCM32(m_srcImageSize);
  Tiles::const_iterator it = m_tiles.begin();
  for (; it != m_tiles.end(); ++it) tileSet->m_tiles.push_back((*it)->clone());
  return tileSet;
}

//******************************************************************************************

TTileSetFullColor::Tile::Tile() : TTileSet::Tile() {}

//------------------------------------------------------------------------------------------

TTileSetFullColor::Tile::Tile(const TRasterP &ras, const TPoint &p)
    : TTileSet::Tile(ras, p) {
  TImageCache::instance()->add(id(), TRasterImageP(ras));
}

//------------------------------------------------------------------------------------------

TTileSetFullColor::Tile::~Tile() { TImageCache::instance()->remove(id()); }

//------------------------------------------------------------------------------------------

TTileSetFullColor::~TTileSetFullColor() {}

//------------------------------------------------------------------------------------------

void TTileSetFullColor::Tile::getRaster(TRasterP &ras) const {
  TRasterImageP img = (TRasterImageP)TImageCache::instance()->get(id(), true);
  if (!img) return;
  ras = img->getRaster();
  assert(!!ras);
}

//------------------------------------------------------------------------------------------

TTileSetFullColor::Tile *TTileSetFullColor::Tile::clone() const {
  Tile *tile           = new Tile();
  tile->m_rasterBounds = m_rasterBounds;
  TRasterImageP img = (TRasterImageP)TImageCache::instance()->get(id(), true);
  if (!img) return tile;
  TRasterImageP clonedImage(img->getRaster()->clone());
  TImageCache::instance()->add(tile->id(), clonedImage);
  return tile;
}

//------------------------------------------------------------------------------------------

void TTileSetFullColor::add(const TRasterP &ras, TRect rect) {
  TRect bounds = ras->getBounds();
  if (!bounds.overlaps(rect)) return;
  rect *= bounds;
  assert(!rect.isEmpty());
  assert(bounds.contains(rect));
  TTileSet::add(new Tile(ras->extract(rect)->clone(), rect.getP00()));
}

//------------------------------------------------------------------------------------------

const TTileSetFullColor::Tile *TTileSetFullColor::getTile(int index) const {
  assert(0 <= index && index < getTileCount());
  TTileSetFullColor::Tile *tile =
      dynamic_cast<TTileSetFullColor::Tile *>(m_tiles[index]);
  assert(tile);
  return tile;
}

//------------------------------------------------------------------------------------------

TTileSetFullColor::Tile *TTileSetFullColor::editTile(int index) const {
  assert(0 <= index && index < getTileCount());
  TTileSetFullColor::Tile *tile =
      dynamic_cast<TTileSetFullColor::Tile *>(m_tiles[index]);
  assert(tile);
  return tile;
}

//------------------------------------------------------------------------------------------

TTileSetFullColor *TTileSetFullColor::clone() const {
  TTileSetFullColor *tileSet = new TTileSetFullColor(m_srcImageSize);
  Tiles::const_iterator it   = m_tiles.begin();
  for (; it != m_tiles.end(); ++it) tileSet->m_tiles.push_back((*it)->clone());
  return tileSet;
}
