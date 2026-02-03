#pragma once

#ifndef CONVERT2FILLED_INCLUDED
#define CONVERT2FILLED_INCLUDED

#include "tfilepath.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "trasterimage.h"
#include "ttoonzimage.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI Convert2Tlv {
private:
  TLevelP m_level1;
  TLevel::Iterator m_it;
  TLevelReaderP m_lr1;
  TLevelReaderP m_lr2;
  TLevelWriterP m_lw;
  std::map<TPixel, int> m_colorMap;
  TDimension m_size;
  int m_count;
  int m_from, m_to;
  TPalette *m_palette;
  int m_colorTolerance;
  int m_lastIndex, m_maxPaletteIndex;
  int m_antialiasType;
  int m_antialiasValue;

  bool m_isUnpaintedFromNAA;
  bool m_appendDefaultPalette;

  double m_dpi;

  void buildToonzRaster(TRasterCM32P &rout, const TRasterP &rin1,
                        const TRasterP &rin2);
  void doFill(TRasterCM32P &rout, const TRaster32P &rin);
  void buildInksFromGrayTones(TRasterCM32P &rout, const TRasterP &rin);
  std::map<TPixel, int>::const_iterator findNearestColor(const TPixel &color);
  void buildInks(TRasterCM32P &rout, const TRaster32P &rin);
  TPalette *buildPalette();
  void removeAntialias(TRasterCM32P &r);

  void buildInksForNAAImage(TRasterCM32P &rout, const TRaster32P &rin);

public:
  TFilePath m_levelIn1, m_levelIn2, m_levelOut, m_palettePath;
  bool m_autoclose, m_premultiply;

  // i livelli passati devono essere  tif o png.
  // se filepath2=TFilePath(), viene creata una tlv unpainted, altrimenti
  // painted.
  // nel secondo caso, i due livelli specificati (in qualsiasi ordine, la classe
  // capisce da sola qual'e' l'unpainted e la painted)
  // devono contenere la versione painted e quella unpainted della stessa
  // sequenza di immagini
  // la tlv e la tpl vengono salvate con il nome e il folder di filepath1.
  Convert2Tlv(const TFilePath &filepath1, const TFilePath &filepath2,
              const TFilePath &outFolder, const QString &outName, int from,
              int to, bool doAutoclose, const TFilePath &palettePath,
              int colorTolerance, int antialiasType, int antialiasValue,
              bool isUnpaintedFromNAA, bool appendDefaultPalette, double dpi);

  bool init(std::string &errorMessage);
  int getFramesToConvertCount();
  bool abort();
  bool convertNext(std::string &errorMessage);
};

// gmt, 3/1/2013; making raster>toonzraster conversion available to scripts
// it seems too complex to modify Convert2Tlv, that refer to file levels instead
// of memory levels.
// RasterToToonzRasterConverter uses the same algorithms (more or less)
// TODO: refactor the two classes
// NOTE: you don't need to specify a palette (with
// RasterToToonzRasterConverter::setPalette()): in that case a suitable palette
// is generated
class DVAPI RasterToToonzRasterConverter {
  TPaletteP m_palette;

public:
  RasterToToonzRasterConverter();
  ~RasterToToonzRasterConverter();

  void setPalette(const TPaletteP &palette);
  const TPaletteP &getPalette() const { return m_palette; }

  TRasterCM32P convert(const TRasterP &inputRaster);
  TRasterCM32P convert(const TRasterP &inksInputRaster,
                       const TRasterP &paintInputRaster);

  TToonzImageP convert(const TRasterImageP &ri);
};

#endif
