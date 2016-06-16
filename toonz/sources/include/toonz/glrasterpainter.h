#pragma once

#include "tgeometry.h"
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

namespace GLRasterPainter {

DVAPI void drawRaster(const TAffine &aff, const TRasterImageP &ri,
                      bool premultiplies = false);
DVAPI void drawRaster(const TAffine &aff, const TToonzImageP &ti,
                      bool showSavebox);
DVAPI void drawRaster(const TAffine &aff, UCHAR *buffer, int wrap, int bpp,
                      const TDimension &rasSize, bool premultiplied);
DVAPI void drawRaster(const TImageP &image, const TDimension &viewerSize,
                      const TAffine &aff, bool showSavebox, bool premultiplied);
}
