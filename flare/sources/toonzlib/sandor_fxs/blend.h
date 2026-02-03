#pragma once

#ifndef BLEND_INCLUDE
#define BLEND_INCLUDE

#include "traster.h"
#include "trastercm.h"

#include "ttoonzimage.h"

//------------------------------------------------------------------------------------------

struct BlendParam {
  std::vector<int>
      colorsIndexes;    // List of color indexes to be blended together
  double intensity;     // Blur radius
  int smoothness;       // Number of neighbouring Blur Samples per pixel
  bool stopAtCountour;  // Blur is obstacled by not chosen color indexes
  int superSampling;
};

//------------------------------------------------------------------------------------------

template <typename PIXEL>
void blend(TToonzImageP ti, TRasterPT<PIXEL> rasOut,
           const std::vector<BlendParam> &params);

#endif  // BLEND_INCLUDE
