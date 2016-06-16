#pragma once

//-----------------------------------------------------------------------------
//  drawutil.h:
//    Private header to common fuction in drawcurves.cpp and drawregions.cpp
//-----------------------------------------------------------------------------

#ifndef DRAWUTIL_H
#define DRAWUTIL_H

#include "tgl.h"
//#include "traster.h"
//#include "tcurves.h"
//#include "tregion.h"

class TRegion;

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//! structure with information about texture to create
struct TextureInfoForGL {
  //  GLenum target,
  //  GLint level,
  GLint internalformat;
  GLsizei width;
  GLsizei height;
  // GLint border,
  GLenum format;
  GLenum type;
  const GLvoid *pixels;
};

//=============================================================================

/**
 *
 */
DVAPI TRasterP prepareTexture(const TRasterP &ras, TextureInfoForGL &texinfo);

/**
 *
 */
DVAPI double computeStep(const TStroke &s, double pixelSize);

/**
 *  Draw stroke centerline between parameters from and to.
 */
DVAPI void drawStrokeCenterline(const TStroke &stroke, double pixelSize,
                                double from = 0.0, double to = 1.0);

DVAPI void stroke2polyline(std::vector<TPointD> &pnts, const TStroke &stroke,
                           double pixelSize, double w0 = 0.0, double w1 = 1.0,
                           bool lastRepeatable = false);

DVAPI void region2polyline(std::vector<T3DPointD> &pnts, const TRegion *region,
                           double pixeSize);

DVAPI TStroke *makeEllipticStroke(double thick, TPointD center, double radiusX,
                                  double radiusY);

#endif  // DRAWUTIL_H
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
