#pragma once

#ifndef TROPCM_INCLUDED
#define TROPCM_INCLUDED

#include "trop.h"
//#include "trastercm.h"
//#include "tpalette.h"
//#include "ttile.h"
#include "ttoonzimage.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TROP_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TTile;
class TLevelP;
class TToonzImageP;

namespace TRop {

DVAPI void convert(const TRaster32P &rasOut, const TRasterCM32P &rasIn,
                   const TPaletteP palette, bool transparencyCheck = false);

DVAPI void convert(
    const TRaster32P &rasOut, const TRasterCM32P &rasIn, TPaletteP palette,
    const TRect &theClipRect,  // il rect su cui e' applicata la conversione
    bool transparencyCheck = false, bool applyFx = false);

DVAPI void convert(const TTile &dst, const TTile &src, const TPaletteP plt,
                   bool transparencyCheck, bool applyFxs);

// matchlinePrevalence ==0->always ink down; matchlinePrevalence == 100 always
// ink up;
DVAPI void applyMatchLines(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                           const TPaletteP &pltOut, const TPaletteP &matchPlt,
                           int inkIndex, int matchlinePrevalence,
                           std::map<int, int> &usedInks);
DVAPI void mergeCmapped(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                        const TPaletteP &pltOut, int matchlinePrevalence,
                        std::map<int, int> &usedColors);
DVAPI void overlayCmapped(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                          const TPaletteP &pltOut, const TPaletteP &upPlt,
                          std::map<int, int> &usedColors);
// DVAPI void applyMatchline(const vector<MatchlinePair>& matchingLevels, int
// inkIndex, int matchlinePrevalence);
// DVAPI void deleteMatchline(const vector<TToonzImageP>& level, const
// vector<int>& inkIndexes);
// DVAPI void eraseInks(TRasterCM32P ras, vector<int>& inkIds, bool
// keepInks=false);

DVAPI void eraseColors(TRasterCM32P ras, std::vector<int> *colorIds,
                       bool eraseInks);  // colorsId==0 ->erase ALL
// DVAPI void  eraseColors(TRasterCM32P ras, vector<int>& colorIds, bool
// eraseInks, bool keepColor);
DVAPI void eraseStyleIds(TToonzImage *image, const std::vector<int> styleIds);

DVAPI void resample(const TRasterP &out, const TRasterCM32P &in,
                    const TPaletteP palette, const TAffine &aff,
                    ResampleFilterType filterType = Triangle, double blur = 1.);

DVAPI void convolve_3_i(TRasterP rout, TRasterCM32P rin,
                        const TPaletteP &palette, int dx, int dy,
                        double conv[]);

DVAPI void convolve_i(TRasterP rout, TRasterCM32P rin, const TPaletteP &palette,
                      int dx, int dy, double conv[], int radius);

DVAPI void fracmove(TRasterP rout, TRasterCM32P rin, const TPaletteP &palette,
                    double dx, double dy);

DVAPI void zoomOutCm32Rgbm(const TRasterCM32P &rin, TRaster32P &rout,
                           const TPalette &plt, int x1, int y1, int x2, int y2,
                           int newx, int newy, int absZoomLevel);

// these function create a small raster in rout shrinking rin, typically to make
// an icon.  the shrink method preserves pixels: even a single isolated pixel
// is kept in the shrinked image.
// shrinking is done without clipping  original image and preserving aspect
// ratio (no stretch!): therefore, horizontal or vertical transparent stripes on
// borders are added when needed.
// warning: rout width must be <= rin width; same for height.
// DVAPI void makeIcon(TRaster32P& rout,    const TRasterCM32P& rin, const
// TPaletteP& palette, bool onBlackBg);
DVAPI void makeIcon(TRasterCM32P &rout, const TRasterCM32P &rin);

DVAPI void expandPaint(const TRasterCM32P &rasCM);

}  // namespace TRop

#endif
