#pragma once

#ifndef WARP_H
#define WARP_H

#include "tfxparam.h"
#include "trop.h"
#include "trasterfx.h"

//-----------------------------------------------------------------------

struct WarpParams {
  int m_shrink;
  double m_warperScale;
  double m_intensity;
  bool m_sharpen;
};

struct LPoint {
  TPointD s;  // Warped lattice point
  TPointD d;  // Original lattice point
};

struct Lattice {
  int m_width;     // Number of lattice columns
  int m_height;    // Number of lattice rows
  LPoint *coords;  // Grid vertex coordinates

  Lattice() : coords(0) {}
  ~Lattice() {
    if (coords) delete[] coords;
  }
};

namespace  // Ugly...
{
inline int myCeil(double x) {
  return ((x - (int)(x)) > TConsts::epsilon ? (int)(x) + 1 : (int)(x));
}

inline TRect convert(const TRectD &r, TPointD &dp) {
  TRect ri(tfloor(r.x0), tfloor(r.y0), myCeil(r.x1), myCeil(r.y1));
  dp.x = r.x0 - ri.x0;
  dp.y = r.y0 - ri.y0;
  assert(dp.x >= 0 && dp.y >= 0);
  return ri;
}
}

//---------------------------------------------------------------------------

inline double getWarpRadius(const WarpParams &params) {
  return 2.55 * 1.5 * 1.5 * fabs(params.m_intensity);
}

//---------------------------------------------------------------------------

inline double getWarperEnlargement(const WarpParams &params) {
  // It accounts for:
  //  * Resample factor (1 - due to triangle filtering)
  //  * Eventual grid smoothening (6 - as the blur radius applied after
  //  resampling)
  //  * grid interpolation (2 - for the shepard interpolant radius)
  int enlargement = 3;
  if (!params.m_sharpen) enlargement += 6;
  return enlargement;
}

//---------------------------------------------------------------------------

void getWarpComputeRects(TRectD &outputComputeRect, TRectD &warpedComputeRect,
                         const TRectD &warpedBox, const TRectD &requestedRect,
                         const WarpParams &params);

//---------------------------------------------------------------------------

//! Deals with raster tiles and invokes warper functions
void warp(TRasterP &tileRas, const TRasterP &rasIn, TRasterP &warper,
          TPointD rasInPos, TPointD warperPos, const WarpParams &params);

#endif
