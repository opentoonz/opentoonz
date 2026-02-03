#pragma once

#ifndef GRADIENTS_H
#define GRADIENTS_H

#include "tfxparam.h"
#include "trop.h"
#include "trasterfx.h"

struct MultiRAdialParams {
  int m_shrink;
  double m_scale;

  double m_intensity;
  double m_gridStep;
};

enum GradientCurveType { EaseInOut = 0, Linear, EaseIn, EaseOut };

/*---------------------------------------------------------------------------*/

//! Deals with raster tiles and invokes multiradial functions

void multiRadial(const TRasterP &ras, TPointD posTrasf,
                 const TSpectrumParamP colors, double period, double count,
                 double cycle, const TAffine &aff, double frame,
                 double inner = 0.0, GradientCurveType type = Linear);

void multiLinear(const TRasterP &ras, TPointD posTrasf,
                 const TSpectrumParamP colors, double period, double count,
                 double amplitude, double freq, double phase, double cycle,
                 const TAffine &aff, double frame,
                 GradientCurveType type = EaseInOut);

#endif
