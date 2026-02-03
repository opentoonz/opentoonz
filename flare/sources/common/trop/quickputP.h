#pragma once

#ifndef QUICKPUT_P_INCLUDED
#define QUICKPUT_P_INCLUDED

#include "trop.h"

void quickPut(const TRasterP &dn, const TRasterP &up, const TAffine &aff,
              TRop::ResampleFilterType filterType,
              const TPixel32 &colorScale = TPixel32::Black,
              bool doPremultiply = false, bool whiteTransp = false,
              bool firstColumn = false, bool doRasterDarkenBlendedView = false);

void quickPut(const TRasterP &dn, const TRasterP &up, const TAffine &aff,
              TRop::ResampleFilterType filterType, const TPixel32 &colorScale,
              bool doPremultiply, bool whiteTransp, bool firstColumn,
              bool doRasterDarkenBlendedView);

void quickResample(const TRasterP &dn, const TRasterP &up, const TAffine &aff,
                   TRop::ResampleFilterType filterType);

void quickPutCmapped(const TRasterP &out, const TRasterCM32P &up,
                     const TPaletteP &plt, const TAffine &aff);

#ifdef __LP64__
void quickResample_optimized(const TRasterP &dn, const TRasterP &up,
                             const TAffine &aff,
                             TRop::ResampleFilterType filterType);
#endif

#endif
