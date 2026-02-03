#pragma once

#ifndef TTILESAVER_INCLUDE
#define TTILESAVER_INCLUDE

#include "trastercm.h"

class TTileSetCM32;
class TTileSetFullColor;

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TTileSaverCM32 {
  TRasterCM32P m_raster;
  TTileSetCM32 *m_tileSet;
  int m_rowSize;
  std::vector<int> m_savedTiles;
  void saveTile(int row, int col);

public:
  TTileSaverCM32(const TRasterCM32P &raster, TTileSetCM32 *tileSet);

  void save(TRect rect);
  void save(TPoint point);

  TTileSetCM32 *getTileSet() const;
};

//********************************************************************************

class DVAPI TTileSaverFullColor {
  TRasterP m_raster;
  TTileSetFullColor *m_tileSet;
  int m_rowSize;
  std::vector<int> m_savedTiles;
  void saveTile(int row, int col);

public:
  TTileSaverFullColor(const TRasterP &raster, TTileSetFullColor *tileSet);

  void save(TRect rect);
  void save(TPoint point);

  TTileSetFullColor *getTileSet() const;
};

#endif
