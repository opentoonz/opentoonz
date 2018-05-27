#pragma once

#ifndef COLORCLUSTERIZE_INCLUDED
#define COLORCLUSTERIZE_INCLUDED

//------------------------------------------------------------------------------
#include <set>
//#include "tpixel.h"
#include "traster.h"

#include <QList>

#undef DVAPI
#undef DVVAR
#ifdef TAPPTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TColorUtils {

/*-- 似ている色をまとめて1つのStyleにする --*/
DVAPI void buildPalette(std::set<TPixel32> &palette, const TRaster32P &raster,
                        int maxColorCount);
/*-- 全ての異なるピクセルの色を別のStyleにする --*/
DVAPI void buildPrecisePalette(std::set<TPixel32> &palette,
                               const TRaster32P &raster, int maxColorCount);
//  pick up color chip sorrounded by frames with specified color
DVAPI void buildColorChipPalette(QList<QPair<TPixel32, TPoint>> &palette,
                                 const TRaster32P &raster, int maxColorCount,
                                 const TPixel32 &gridColor,
                                 const int gridLineWidth,
                                 const int colorChipOrder);
}

//------------------------------------------------------------------------------

#endif
