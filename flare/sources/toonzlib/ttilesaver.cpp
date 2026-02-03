

#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"

#define TILE_BIT_SIZE 6
#define TILE_SIZE_MINUS_1 63

// divisione per tileSize approssimata per eccesso
inline int divSup(int x) {
  // return (x & TILE_SIZE_MINUS_1)? (x>>TILE_BIT_SIZE)+1 : (x>>TILE_BIT_SIZE);
  return ((x + TILE_SIZE_MINUS_1) >> TILE_BIT_SIZE);
}

//------------------------------------------------------------------------

TTileSaverCM32::TTileSaverCM32(const TRasterCM32P &raster,
                               TTileSetCM32 *tileSet)
    : m_raster(raster)
    , m_tileSet(tileSet)
    , m_rowSize(divSup(raster->getLx()))
    , m_savedTiles(m_rowSize * divSup(raster->getLy()), 0) {}

//------------------------------------------------------------------------

void TTileSaverCM32::saveTile(int row, int col) {
  UINT index = m_rowSize * row + col;
  assert(index < m_savedTiles.size());
  if (m_savedTiles[index]) return;
  m_savedTiles[index] = 1;
  int x               = col << TILE_BIT_SIZE;
  int y               = row << TILE_BIT_SIZE;
  TRect tileBounds(x, y, x + TILE_SIZE_MINUS_1, y + TILE_SIZE_MINUS_1);
  m_tileSet->add(m_raster, tileBounds);
}

//------------------------------------------------------------------------

void TTileSaverCM32::save(TRect rect) {
  if (!m_raster->getBounds().overlaps(rect)) return;
  rect = rect * m_raster->getBounds();

  TPoint p0 = rect.getP00();
  TPoint p1 = rect.getP11();

  int col0 = p0.x >> TILE_BIT_SIZE;
  int row0 = p0.y >> TILE_BIT_SIZE;
  int col1 = p1.x >> TILE_BIT_SIZE;
  int row1 = p1.y >> TILE_BIT_SIZE;

  for (int row = row0; row <= row1; ++row)
    for (int col = col0; col <= col1; ++col) saveTile(row, col);
}

//------------------------------------------------------------------------

void TTileSaverCM32::save(TPoint point) {
  if (m_raster->getBounds().contains(point))
    saveTile(point.y >> TILE_BIT_SIZE, point.x >> TILE_BIT_SIZE);
}

//------------------------------------------------------------------------

TTileSetCM32 *TTileSaverCM32::getTileSet() const { return m_tileSet; }

//********************************************************************************

TTileSaverFullColor::TTileSaverFullColor(const TRasterP &raster,
                                         TTileSetFullColor *tileSet)
    : m_raster(raster)
    , m_tileSet(tileSet)
    , m_rowSize(divSup(raster->getLx()))
    , m_savedTiles(m_rowSize * divSup(raster->getLy()), 0) {}

//------------------------------------------------------------------------

void TTileSaverFullColor::saveTile(int row, int col) {
  UINT index = m_rowSize * row + col;
  assert(index < m_savedTiles.size());
  if (m_savedTiles[index]) return;
  m_savedTiles[index] = 1;
  int x               = col << TILE_BIT_SIZE;
  int y               = row << TILE_BIT_SIZE;
  TRect tileBounds(x, y, x + TILE_SIZE_MINUS_1, y + TILE_SIZE_MINUS_1);
  m_tileSet->add(m_raster, tileBounds);
}

//------------------------------------------------------------------------

void TTileSaverFullColor::save(TRect rect) {
  if (!m_raster->getBounds().overlaps(rect)) return;
  rect = rect * m_raster->getBounds();

  TPoint p0 = rect.getP00();
  TPoint p1 = rect.getP11();

  int col0 = p0.x >> TILE_BIT_SIZE;
  int row0 = p0.y >> TILE_BIT_SIZE;
  int col1 = p1.x >> TILE_BIT_SIZE;
  int row1 = p1.y >> TILE_BIT_SIZE;

  for (int row = row0; row <= row1; ++row)
    for (int col = col0; col <= col1; ++col) saveTile(row, col);
}

//------------------------------------------------------------------------

void TTileSaverFullColor::save(TPoint point) {
  if (m_raster->getBounds().contains(point))
    saveTile(point.y >> TILE_BIT_SIZE, point.x >> TILE_BIT_SIZE);
}

//------------------------------------------------------------------------

TTileSetFullColor *TTileSaverFullColor::getTileSet() const { return m_tileSet; }
