

#include "traster.h"
#include "tpixelutils.h"

#include "trop.h"

//***********************************************************************************************
//    Local namespace stuff
//***********************************************************************************************

namespace {

template <typename T>
struct RaylitFuncTraits {
  typedef void (*function_type)(T *, T *, int, int, int, int, const TRect &,
                                const TRect &, const TRop::RaylitParams &);
};

//--------------------------------------------------------------------------------------------

template <typename T>
void performStandardRaylit(T *bufIn, T *bufOut, int dxIn, int dyIn, int dxOut,
                           int dyOut, const TRect &srcRect,
                           const TRect &dstRect,
                           const TRop::RaylitParams &params) {
  /* NOTATION:  Diagram assuming octant 1

  /                   |
 /                    |
/  - ray_final_y      | octLy
/ 1                    |
+----                   |
_____  octLx


So, octLx and octLy are the octant's lx and ly;  ray_final_y is the final height
of the ray we're tracing
*/

  // Build colors-related variables
  int max = T::maxChannelValue;
  /*-- 透明部分の色 --*/
  int transp_val = (params.m_invert) ? max : 0, opaque_val = max - transp_val;
  int value, val_r, val_g, val_b, val_m;
  double lightness, r_fac, g_fac, b_fac, m_fac;
  /*-- 8bit/16bitの違いを吸収する係数 --*/
  double factor = max / 255.0;

  double scale =
      params.m_scale;  // NOTE: These variable initializations are, well,
  double decay = log(params.m_decay / 100.0 + 1.0) +
                 1.0;  // heuristic at least. They were probably tested
  double intensity =
      1e8 * log(params.m_intensity / 100.0 + 1.0) /
      scale;  // to be good, but didn't quite make any REAL sense.
  double smoothness = log(params.m_smoothness * 5.0 / 100.0 + 1.0);  //
  // They could be done MUCH better, but changing them
  /*-- 1ステップ進んだ時、次のピクセルで光源が無かったときの光の弱まる割合 --*/
  double neg_delta_p =
      smoothness *
      intensity;  // would alter the way raylit has been applied until now.
  /*-- 1ステップ進んだ時、次のピクセルで光源が有ったときの光の強まる割合 --*/
  double quot_delta_p = intensity / max;  //
  // Should be changed at some point, though...
  /*--
   * m_colorはRaylitFxのColor値。r_fac、g_fac、b_facは各チャンネルをPremultiplyした値
   * --*/
  m_fac = (params.m_color.m / 255.0);
  r_fac = m_fac * (params.m_color.r / 255.0);
  g_fac = m_fac * (params.m_color.g / 255.0);
  b_fac = m_fac * (params.m_color.b / 255.0);

  // Geometry-related variables
  int x, y, ray_final_y;
  int octLx = dstRect.x1 - dstRect.x0;

  double rayPosIncrementX = 1.0 / scale;

  double sq_z = sq(params.m_lightOriginSrc.z);  // We'll be making square
                                                // distances from p, so square
                                                // it once now

  // Perform raylit
  T *pixIn, *pixOut;

  for (ray_final_y = 0; ray_final_y < octLx; ++ray_final_y) {
    // Initialize increment variables
    lightness = 0.0;

    double rayPosIncrementY = rayPosIncrementX * (ray_final_y / (double)octLx);

    // Use an integer counter to know when y must increase. Will add ray_final_y
    // as long as
    // a multiple of octLx-1 is reached, then increase
    int yIncrementCounter = 0, yIncrementThreshold = octLx - 1;

    // Trace a single ray of light
    TPointD rayPos(rayPosIncrementX, rayPosIncrementY);

    for (x = dstRect.x0, y = dstRect.y0, pixIn = bufIn, pixOut = bufOut;
         (x < dstRect.x1) && (y < dstRect.y1); ++x) {
      bool insideSrc = (x >= srcRect.x0) && (x < srcRect.x1) &&
                       (y >= srcRect.y0) && (y < srcRect.y1);
      if (insideSrc) {
        // Add a light component depending on source's matte
        if (pixIn->m == opaque_val)
          lightness = std::max(
              0.0, lightness - neg_delta_p);  // No light source - ray fading
        else {
          if (pixIn->m == transp_val)
            lightness += intensity;  // Full light source - ray enforcing
          else
            lightness = std::max(
                0.0, lightness +  // Half light source
                         (params.m_invert ? pixIn->m : (max - pixIn->m)) *
                             quot_delta_p);  //   matte-linear enforcing
        }

        if (params.m_includeInput) {
          val_r = pixIn->r;
          val_g = pixIn->g;
          val_b = pixIn->b;
          val_m = pixIn->m;
        } else
          val_r = val_g = val_b = val_m = 0;
      } else {
        if (!params.m_invert)
          lightness += intensity;
        else
          lightness = std::max(0.0, lightness - neg_delta_p);

        val_r = val_g = val_b = val_m = 0;
      }

      bool insideDst = (x >= 0) && (y >= 0);
      if (insideDst) {
        // Write the corresponding destination pixel
        if (lightness > 0.0)
          value = (int)(factor * lightness /
                            (rayPos.x *
                             pow((double)(sq(rayPos.x) + sq(rayPos.y) + sq_z),
                                 decay)) +
                        0.5);  // * ^-d...  0.5 rounds
        else
          value = 0;

        // NOTE: pow() could be slow. If that is the case, it could be cached
        // for the whole octant along the longest ray at integer positions,
        // and then linearly interpolated between those... Have to profile this
        // before resorting to that...

        val_r += value * r_fac;
        val_g += value * g_fac;
        val_b += value * b_fac;
        val_m += value * m_fac;

        pixOut->r = (val_r > max) ? max : val_r;
        pixOut->g = (val_g > max) ? max : val_g;
        pixOut->b = (val_b > max) ? max : val_b;
        pixOut->m = (val_m > max) ? max : val_m;
      }

      // Increment variables along the x-axis
      pixIn += dxIn, pixOut += dxOut;

      rayPos.x += rayPosIncrementX, rayPos.y += rayPosIncrementY;

      // Increment variables along the y-axis
      if ((yIncrementCounter += ray_final_y) >= yIncrementThreshold) {
        ++y, pixIn += dyIn, pixOut += dyOut;
        yIncrementCounter -= yIncrementThreshold;
      }
    }
  }
}

//--------------------------------------------------------------------------------------------

template <typename T>
void performColorRaylit(T *bufIn, T *bufOut, int dxIn, int dyIn, int dxOut,
                        int dyOut, const TRect &srcRect, const TRect &dstRect,
                        const TRop::RaylitParams &params) {
  // Build colors-related variables
  int max = T::maxChannelValue;

  int val_r, val_g, val_b, val_m;
  double lightness_r, lightness_g, lightness_b;
  double factor = max / 255.0;

  double scale =
      params.m_scale;  // NOTE: These variable initializations are, well,
  double decay = log(params.m_decay / 100.0 + 1.0) +
                 1.0;  // heuristic at least. They were probably tested
  double intensity =
      1e8 * log(params.m_intensity / 100.0 + 1.0) /
      scale;  // to be good, but didn't quite make any REAL sense.
  double smoothness = log(params.m_smoothness * 5.0 / 100.0 + 1.0);  //
  // They could be done MUCH better, but changing them
  double neg_delta_p =
      smoothness *
      intensity;  // would alter the way raylit has been applied until now.
  double quot_delta_p = intensity / max;  //
  // Should be changed at some point, though...

  // Geometry-related variables
  int x, y, ray_final_y;
  int octLx = dstRect.x1 - dstRect.x0;

  double rayPosIncrementX = 1.0 / scale;

  double fac, sq_z = sq(params.m_lightOriginSrc.z);  // We'll be making square
                                                     // distances from p, so
                                                     // square it once now

  // Perform raylit
  T *pixIn, *pixOut;

  for (ray_final_y = 0; ray_final_y < octLx; ++ray_final_y) {
    // Initialize increment variables
    lightness_r = lightness_g = lightness_b = 0.0;
    int l, l_max;

    double rayPosIncrementY = rayPosIncrementX * (ray_final_y / (double)octLx);

    // Use an integer counter to know when y must increase. Will add ray_final_y
    // as long as
    // a multiple of octLx-1 is reached, then increase
    int yIncrementCounter = 0, yIncrementThreshold = octLx - 1;

    // Trace a single ray of light
    TPointD rayPos(rayPosIncrementX, rayPosIncrementY);

    for (x = dstRect.x0, y = dstRect.y0, pixIn = bufIn, pixOut = bufOut;
         (x < dstRect.x1) && (y < dstRect.y1); ++x) {
      bool insideSrc = (x >= srcRect.x0) && (x < srcRect.x1) &&
                       (y >= srcRect.y0) && (y < srcRect.y1);
      if (insideSrc) {
        val_r = pixIn->r;
        val_g = pixIn->g;
        val_b = pixIn->b;
        val_m = pixIn->m;

        lightness_r = std::max(0.0, val_r ? lightness_r + val_r * quot_delta_p
                                          : lightness_r - neg_delta_p);
        lightness_g = std::max(0.0, val_g ? lightness_g + val_g * quot_delta_p
                                          : lightness_g - neg_delta_p);
        lightness_b = std::max(0.0, val_b ? lightness_b + val_b * quot_delta_p
                                          : lightness_b - neg_delta_p);

        if (!params.m_includeInput) val_r = val_g = val_b = val_m = 0;
      } else {
        lightness_r = std::max(0.0, lightness_r - neg_delta_p);
        lightness_g = std::max(0.0, lightness_g - neg_delta_p);
        lightness_b = std::max(0.0, lightness_b - neg_delta_p);

        val_r = val_g = val_b = val_m = 0;
      }

      bool insideDst = (x >= 0) && (y >= 0);
      if (insideDst) {
        // Write the corresponding destination pixel
        fac =
            factor / (rayPos.x *
                      pow((double)(sq(rayPos.x) + sq(rayPos.y) + sq_z), decay));

        // NOTE: pow() could be slow. If that is the case, it could be cached
        // for the whole octant along the longest ray at integer positions,
        // and then linearly interpolated between those... Have to profile this
        // before resorting to that...

        val_r += l = (int)(fac * lightness_r + 0.5);
        l_max      = l;
        val_g += l = (int)(fac * lightness_g + 0.5);
        l_max      = std::max(l, l_max);
        val_b += l = (int)(fac * lightness_b + 0.5);
        l_max      = std::max(l, l_max);
        val_m += l_max;

        pixOut->r = (val_r > max) ? max : val_r;
        pixOut->g = (val_g > max) ? max : val_g;
        pixOut->b = (val_b > max) ? max : val_b;
        pixOut->m = (val_m > max) ? max : val_m;
      }

      // Increment variables along the x-axis
      pixIn += dxIn, pixOut += dxOut;

      rayPos.x += rayPosIncrementX, rayPos.y += rayPosIncrementY;

      // Increment variables along the y-axis
      if ((yIncrementCounter += ray_final_y) >= yIncrementThreshold) {
        ++y, pixIn += dyIn, pixOut += dyOut;
        yIncrementCounter -= yIncrementThreshold;
      }
    }
  }
}

//--------------------------------------------------------------------------------------------
/*-- ピザ状に8分割された領域の1つを計算する --*/
template <typename T>
void computeOctant(const TRasterPT<T> &src, const TRasterPT<T> &dst, int octant,
                   const TRop::RaylitParams &params,
                   typename RaylitFuncTraits<T>::function_type raylitFunc) {
  // Build octant geometry variables
  int x0, x1, lxIn, lxOut, dxIn, dxOut;
  int y0, y1, lyIn, lyOut, dyIn, dyOut;

  const T3DPoint &pIn  = params.m_lightOriginSrc,
                 &pOut = params.m_lightOriginDst;
  int srcWrap = src->getWrap(), dstWrap = dst->getWrap();

  T *bufIn = (T *)src->getRawData() + tfloor(pIn.y) * srcWrap + tfloor(pIn.x);
  T *bufOut =
      (T *)dst->getRawData() + tfloor(pOut.y) * dstWrap + tfloor(pOut.x);

  TRect srcRect(src->getBounds() +
                TPoint(tround(pOut.x - pIn.x), tround(pOut.y - pIn.y)));

  lxIn = src->getLx(), lxOut = dst->getLx();
  lyIn = src->getLy(), lyOut = dst->getLy();

  /*-- 1ピクセルずつ進むときの移動値 --*/
  // Vertical octant pairs
  if (octant == 1 || octant == 8)
    dxIn = 1, dxOut = 1, x0 = tfloor(pOut.x), x1 = lxOut;
  if (octant == 2 || octant == 7)
    dyIn = 1, dyOut = 1, y0 = tfloor(pOut.x), y1 = lxOut;
  if (octant == 3 || octant == 6)
    dyIn = -1, dyOut = -1, y0 = lxOut - tfloor(pOut.x) - 1, y1 = lxOut,
    std::swap(srcRect.x0, srcRect.x1), srcRect.x0 = lxOut - srcRect.x0,
    srcRect.x1 = lxOut - srcRect.x1;
  if (octant == 4 || octant == 5)
    dxIn = -1, dxOut = -1, x0 = lxOut - tfloor(pOut.x) - 1, x1 = lxOut,
    std::swap(srcRect.x0, srcRect.x1), srcRect.x0 = lxOut - srcRect.x0,
    srcRect.x1 = lxOut - srcRect.x1;

  // Horizontal octant pairs
  if (octant == 2 || octant == 3)
    dxIn = srcWrap, dxOut = dstWrap, x0 = tfloor(pOut.y), x1 = lyOut;
  if (octant == 1 || octant == 4)
    dyIn = srcWrap, dyOut = dstWrap, y0 = tfloor(pOut.y), y1 = lyOut;
  if (octant == 5 || octant == 8)
    dyIn = -srcWrap, dyOut = -dstWrap, y0 = lyOut - tfloor(pOut.y) - 1,
    y1 = lyOut, std::swap(srcRect.y0, srcRect.y1), srcRect.y0 = lyOut - srcRect.y0,
    srcRect.y1 = lyOut - srcRect.y1;
  if (octant == 6 || octant == 7)
    dxIn = -srcWrap, dxOut = -dstWrap, x0 = lyOut - tfloor(pOut.y) - 1,
    x1 = lyOut, std::swap(srcRect.y0, srcRect.y1), srcRect.y0 = lyOut - srcRect.y0,
    srcRect.y1 = lyOut - srcRect.y1;

  /*-- 縦向きのピザ領域を計算する場合は、90度回転してから --*/
  // Swap x and y axis where necessary
  if (octant == 2 || octant == 3 || octant == 6 || octant == 7) {
    std::swap(lxIn, lyIn), std::swap(lxOut, lyOut);
    std::swap(srcRect.x0, srcRect.y0), std::swap(srcRect.x1, srcRect.y1);
  }

  int octLx = (x1 - x0), octLy = (y1 - y0);

  assert(octLx > 0 && octLy > 0);
  if (octLx <= 0 && octLy <= 0) return;

  raylitFunc(bufIn, bufOut, dxIn, dyIn, dxOut, dyOut, srcRect,
             TRect(x0, y0, x1, y1), params);
}

//--------------------------------------------------------------------------------------------

/*
  OCTANTS:

    \ 3 | 2 /
     \  |  /
      \ | /
     4 \|/ 1
    ----+----
     5 /|\ 8
      / | \
     /  |  \
    / 6 | 7 \
*/

template <typename T>
void doRaylit(const TRasterPT<T> &src, const TRasterPT<T> &dst,
              const TRop::RaylitParams &params,
              typename RaylitFuncTraits<T>::function_type raylitFunc) {
  int lxOut = dst->getLx(), lyOut = dst->getLy();
  const T3DPoint &p = params.m_lightOriginDst;

  src->lock();
  dst->lock();

  // Depending on the position of p, only some of the quadrants need to be built
  if (p.y < lyOut) {
    if (p.x < lxOut) {
      // Compute the raylit fx on each octant independently
      computeOctant(src, dst, 1, params, raylitFunc);
      computeOctant(src, dst, 2, params, raylitFunc);
    }

    if (p.x >= 0) {
      computeOctant(src, dst, 3, params, raylitFunc);
      computeOctant(src, dst, 4, params, raylitFunc);
    }
  }

  if (p.y >= 0) {
    if (p.x >= 0) {
      computeOctant(src, dst, 5, params, raylitFunc);
      computeOctant(src, dst, 6, params, raylitFunc);
    }

    if (p.x < lxOut) {
      computeOctant(src, dst, 7, params, raylitFunc);
      computeOctant(src, dst, 8, params, raylitFunc);
    }
  }

  dst->unlock();
  src->unlock();
}

}  // namespace

//***********************************************************************************************
//    TRop::raylit implementation
//***********************************************************************************************

void TRop::raylit(const TRasterP &dstRas, const TRasterP &srcRas,
                  const RaylitParams &params) {
  if ((TRaster32P)dstRas && (TRaster32P)srcRas)
    doRaylit<TPixel32>(srcRas, dstRas, params,
                       &performStandardRaylit<TPixel32>);
  else if ((TRaster64P)dstRas && (TRaster64P)srcRas)
    doRaylit<TPixel64>(srcRas, dstRas, params,
                       &performStandardRaylit<TPixel64>);
  else
    throw TException("TRop::raylit unsupported pixel type");
}

//--------------------------------------------------------------------------------------------

void TRop::glassRaylit(const TRasterP &dstRas, const TRasterP &srcRas,
                       const RaylitParams &params) {
  if ((TRaster32P)dstRas && (TRaster32P)srcRas)
    doRaylit<TPixel32>(srcRas, dstRas, params, &performColorRaylit<TPixel32>);
  else if ((TRaster64P)dstRas && (TRaster64P)srcRas)
    doRaylit<TPixel64>(srcRas, dstRas, params, &performColorRaylit<TPixel64>);
  else
    throw TException("TRop::raylit unsupported pixel type");
}
