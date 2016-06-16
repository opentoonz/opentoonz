#pragma once

#ifndef TRASTERIMAGE_UTILS_INCLUDED
#define TRASTERIMAGE_UTILS_INCLUDED

#include "tgeometry.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "trasterfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TStroke;
class TTileSetFullColor;

namespace TRasterImageUtils {

DVAPI TRect addStroke(const TRasterImageP &ri, TStroke *stroke, TRectD clip,
                      double opacity, bool doAntialiasing = true);

DVAPI TRect convertWorldToRaster(const TRectD &area, const TRasterImageP ri);
DVAPI TRectD convertRasterToWorld(const TRect &area, const TRasterImageP ri);

DVAPI TRasterImageP vectorToFullColorImage(
    const TVectorImageP &vi, const TAffine &aff, TPalette *palette,
    const TPointD &outputPos, const TDimension &outputSize,
    const std::vector<TRasterFxRenderDataP> *fxs = 0,
    bool transformThickness                      = false);
DVAPI TRect eraseRect(const TRasterImageP &ri, const TRectD &area);
DVAPI std::vector<TRect> paste(const TRasterImageP &ti,
                               const TTileSetFullColor *tileSet);
DVAPI void addSceneNumbering(const TRasterImageP &ri, int globalIndex,
                             const std::wstring &sceneName, int sceneIndex);
DVAPI void addGlobalNumbering(const TRasterImageP &ri,
                              const std::wstring &sceneName, int globalIndex);
}  // namespace

#endif  // TRASTERIMAGE_UTILS_INCLUDED
