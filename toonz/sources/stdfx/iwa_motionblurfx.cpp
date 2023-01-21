/*------------------------------------
Iwa_MotionBlurCompFx
Motion blur fx considering the exposure of light and trajectory of the object.
Available to composite exposure value with background.
//------------------------------------*/

#include "iwa_motionblurfx.h"
#include "tfxattributes.h"

#include "toonz/tstageobject.h"

#include "trop.h"

/* Normalize the source image to 0 - 1 and read it into the host memory.
 * Check if the source image is premultiped or not here, if it is not specified
 * in the combo box. */
template <typename RASTER, typename PIXEL>
bool Iwa_MotionBlurCompFx::setSourceRaster(const RASTER srcRas, float4 *dstMem,
                                           TDimensionI dim,
                                           PremultiTypes type) {
  bool isPremultiplied = (type == SOURCE_IS_NOT_PREMUTIPLIED) ? false : true;

  float4 *chann_p = dstMem;

  float threshold = 100.0 / (float)TPixel64::maxChannelValue;

  int max = 0;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;

      /* If there are pixels whose RGB values ​​are larger than the alpha
       * channel, determine the source is not premutiplied */
      // CAUTION: this condition won't work properly ith HDR image pixels!
      if (type == AUTO && isPremultiplied &&
          (((*chann_p).x > (*chann_p).w && (*chann_p).x > threshold) ||
           ((*chann_p).y > (*chann_p).w && (*chann_p).y > threshold) ||
           ((*chann_p).z > (*chann_p).w && (*chann_p).z > threshold)))
        isPremultiplied = false;

      pix++;
      chann_p++;
    }
  }

  if (isPremultiplied) {
    chann_p = dstMem;
    for (int i = 0; i < dim.lx * dim.ly; i++, chann_p++) {
      if ((*chann_p).x > (*chann_p).w) (*chann_p).x = (*chann_p).w;
      if ((*chann_p).y > (*chann_p).w) (*chann_p).y = (*chann_p).w;
      if ((*chann_p).z > (*chann_p).w) (*chann_p).z = (*chann_p).w;
    }
  }

  return isPremultiplied;
}

/*------------------------------------------------------------
 Convert output result to Channel value and store it in the tile
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_MotionBlurCompFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                           TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    PIXEL *pix     = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++) {
      float val;
      val    = (*chan_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      pix++;
      chan_p++;
    }
  }
}

template <>
void Iwa_MotionBlurCompFx::setOutputRaster<TRasterFP, TPixelF>(
    float4 *srcMem, const TRasterFP dstRas, TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    TPixelF *pix   = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++) {
      pix->r = (*chan_p).x;
      pix->g = (*chan_p).y;
      pix->b = (*chan_p).z;
      pix->m = (*chan_p).w;
      pix++;
      chan_p++;
    }
  }
}

/*------------------------------------------------------------
 Create and normalize filters
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::makeMotionBlurFilter_CPU(
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    float4 *pointsTable, int pointAmount, float startValue, float startCurve,
    float endValue, float endCurve) {
  /* Variable for adding filter value*/
  float fil_val_sum = 0.0f;
  /* The current filter position to be looped in the 'for' statement */
  float *current_fil_p = filter_p;
  /* For each coordinate in the filter */
  for (int fily = 0; fily < filterDim.ly; fily++) {
    for (int filx = 0; filx < filterDim.lx; filx++, current_fil_p++) {
      /* Get filter coordinates */
      float2 pos = {static_cast<float>(filx - marginLeft),
                    static_cast<float>(fily - marginBottom)};
      /* Value to be updated */
      float nearestDist2         = 100.0f;
      int nearestIndex           = -1;
      float nearestFramePosRatio = 0.0f;

      /* Find the nearest point for each pair of sample points */
      for (int v = 0; v < pointAmount - 1; v++) {
        float4 p0 = pointsTable[v];
        float4 p1 = pointsTable[v + 1];

        /* If it is not within the range, continue */
        if (pos.x < std::min(p0.x, p1.x) - 1.0f ||
            pos.x > std::max(p0.x, p1.x) + 1.0f ||
            pos.y < std::min(p0.y, p1.y) - 1.0f ||
            pos.y > std::max(p0.y, p1.y) + 1.0f)
          continue;

        /* Since it is within the range, obtain the distance between the line
         * segment and the point. */
        /* Calculate the inner product of 'p0'->sampling point and 'p0'->'p1' */
        float2 vec_p0_sample = {static_cast<float>(pos.x - p0.x),
                                static_cast<float>(pos.y - p0.y)};
        float2 vec_p0_p1     = {static_cast<float>(p1.x - p0.x),
                                static_cast<float>(p1.y - p0.y)};
        float dot =
            vec_p0_sample.x * vec_p0_p1.x + vec_p0_sample.y * vec_p0_p1.y;
        /* Calculate the square of distance */
        float dist2;
        float framePosRatio;
        /* If it is before 'p0' */
        if (dot <= 0.0f) {
          dist2 = vec_p0_sample.x * vec_p0_sample.x +
                  vec_p0_sample.y * vec_p0_sample.y;
          framePosRatio = 0.0f;
        } else {
          /* Calculate the square of the length of the trajectory vector */
          float length2 = p0.z * p0.z;

          /* If it is between 'p0' and 'p1'
           * If the trajectory at p is a point,
           * 'length2' becomes 0, so it will never fall into this condition.
           * So, there should not be worry of becoming ZeroDivide. */
          if (dot < length2) {
            float p0_sample_dist2 = vec_p0_sample.x * vec_p0_sample.x +
                                    vec_p0_sample.y * vec_p0_sample.y;
            dist2         = p0_sample_dist2 - dot * dot / length2;
            framePosRatio = dot / length2;
          }
          /* If it is before 'p1' */
          else {
            float2 vec_p1_sample = {pos.x - p1.x, pos.y - p1.y};
            dist2                = vec_p1_sample.x * vec_p1_sample.x +
                    vec_p1_sample.y * vec_p1_sample.y;
            framePosRatio = 1.0f;
          }
        }
        /* If the distance is farther than (√ 2 + 1) / 2, continue
         * Because it is a comparison with dist2, the value is squared */
        if (dist2 > 1.4571f) continue;

        /* Update if distance is closer */
        if (dist2 < nearestDist2) {
          nearestDist2         = dist2;
          nearestIndex         = v;
          nearestFramePosRatio = framePosRatio;
        }
      }

      /* If neighborhood vector of the current pixel can not be found,
       * set the filter value to 0 and return */
      if (nearestIndex == -1) {
        *current_fil_p = 0.0f;
        continue;
      }

      /* Count how many subpixels (16 * 16) of the current pixel
       * are in the range 0.5 from the neighborhood vector.
       */
      int count  = 0;
      float4 np0 = pointsTable[nearestIndex];
      float4 np1 = pointsTable[nearestIndex + 1];
      for (int yy = 0; yy < 16; yy++) {
        /* Y coordinate of the subpixel */
        float subPosY = pos.y + ((float)yy - 7.5f) / 16.0f;
        for (int xx = 0; xx < 16; xx++) {
          /* X coordinate of the subpixel */
          float subPosX = pos.x + ((float)xx - 7.5f) / 16.0f;

          float2 vec_np0_sub = {subPosX - np0.x, subPosY - np0.y};
          float2 vec_np0_np1 = {np1.x - np0.x, np1.y - np0.y};
          float dot =
              vec_np0_sub.x * vec_np0_np1.x + vec_np0_sub.y * vec_np0_np1.y;
          /* Calculate the square of the distance */
          float dist2;
          /* If it is before 'p0' */
          if (dot <= 0.0f)
            dist2 =
                vec_np0_sub.x * vec_np0_sub.x + vec_np0_sub.y * vec_np0_sub.y;
          else {
            /* Compute the square of the length of the trajectory vector */
            float length2 = np0.z * np0.z;
            /* If it is between 'p0' and 'p1' */
            if (dot < length2) {
              float np0_sub_dist2 =
                  vec_np0_sub.x * vec_np0_sub.x + vec_np0_sub.y * vec_np0_sub.y;
              dist2 = np0_sub_dist2 - dot * dot / length2;
            }
            /* if it is before 'p1' */
            else {
              float2 vec_np1_sub = {subPosX - np1.x, subPosY - np1.y};
              dist2 =
                  vec_np1_sub.x * vec_np1_sub.x + vec_np1_sub.y * vec_np1_sub.y;
            }
          }
          /* Increment count if squared distance is less than 0.25 */
          if (dist2 <= 0.25f) count++;
        }
      }
      /* 'safeguard' - If the count is 0, set the field value to 0 and return.
       */
      if (count == 0) {
        *current_fil_p = 0.0f;
        continue;
      }

      /* Count is 256 at a maximum */
      float countRatio = (float)count / 256.0f;

      /* The brightness of the filter value is inversely proportional
       * to the area of ​​the line of width 1 made by the vector.
       *
       * Since there are semicircular caps with radius 0.5
       * before and after the vector, it will never be 0-divide
       * even if the length of vector is 0.
       */

      /* Area of the neighborhood vector when width = 1 */
      float vecMenseki = 0.25f * 3.14159265f + np0.z;

      //-----------------
      /* Next, get the value of the gamma strength */
      /* Offset value of the frame of the neighbor point */
      float curveValue;
      float frameOffset =
          np0.w * (1.0f - nearestFramePosRatio) + np1.w * nearestFramePosRatio;
      /* If the frame is exactly at the frame origin,
       * or there is no attenuation value, set curveValue to 1 */
      if (frameOffset == 0.0f || (frameOffset < 0.0f && startValue == 1.0f) ||
          (frameOffset > 0.0f && endValue == 1.0f))
        curveValue = 1.0f;
      else {
        /* Change according to positive / negative of offset */
        float value, curve, ratio;
        if (frameOffset < 0.0f) /* Start side */
        {
          value = startValue;
          curve = startCurve;
          ratio = 1.0f - (frameOffset / pointsTable[0].w);
        } else {
          value = endValue;
          curve = endCurve;
          ratio = 1.0f - (frameOffset / pointsTable[pointAmount - 1].w);
        }

        curveValue = value + (1.0f - value) * powf(ratio, 1.0f / curve);
      }
      //-----------------

      /* Store field value */
      *current_fil_p = curveValue * countRatio / vecMenseki;
      fil_val_sum += *current_fil_p;
    }
  }

  /* Normalization */
  current_fil_p = filter_p;
  for (int f = 0; f < filterDim.lx * filterDim.ly; f++, current_fil_p++) {
    *current_fil_p /= fil_val_sum;
  }
}

/*------------------------------------------------------------
 Create afterimage filter and normalize
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::makeZanzoFilter_CPU(
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    float4 *pointsTable, int pointAmount, float startValue, float startCurve,
    float endValue, float endCurve) {
  /* Variable for adding filter value */
  float fil_val_sum = 0.0f;
  /* The current filter position to be looped in the 'for' statement */
  float *current_fil_p = filter_p;
  /* For each coordinate in the filter */
  for (int fily = 0; fily < filterDim.ly; fily++) {
    for (int filx = 0; filx < filterDim.lx; filx++, current_fil_p++) {
      /* Get filter coordinates */
      float2 pos = {(float)(filx - marginLeft), (float)(fily - marginBottom)};
      /* Variable to be accumulated */
      float filter_sum = 0.0f;
      /* Measure the distance for each sample point and accumulate the density
       */
      for (int v = 0; v < pointAmount; v++) {
        float4 p0 = pointsTable[v];
        /* If it is not within the distance 1 around the coordinates of 'p0',
         * continue */
        if (pos.x < p0.x - 1.0f || pos.x > p0.x + 1.0f || pos.y < p0.y - 1.0f ||
            pos.y > p0.y + 1.0f)
          continue;

        /* Linear interpolation with 4 neighboring pixels */
        float xRatio = 1.0f - std::abs(pos.x - p0.x);
        float yRatio = 1.0f - std::abs(pos.y - p0.y);

        /* Next, get the value of the gamma strength */
        /* Offset value of the frame of the neighbor point */
        float curveValue;
        float frameOffset = p0.w;
        /* If the frame is exactly at the frame origin,
         * or there is no attenuation value, set curveValue to 1 */
        if (frameOffset == 0.0f || (frameOffset < 0.0f && startValue == 1.0f) ||
            (frameOffset > 0.0f && endValue == 1.0f))
          curveValue = 1.0f;
        else {
          /* Change according to positive / negative of offset */
          float value, curve, ratio;
          if (frameOffset < 0.0f) /* Start side */
          {
            value = startValue;
            curve = startCurve;
            ratio = 1.0f - (frameOffset / pointsTable[0].w);
          } else {
            value = endValue;
            curve = endCurve;
            ratio = 1.0f - (frameOffset / pointsTable[pointAmount - 1].w);
          }

          curveValue = value + (1.0f - value) * powf(ratio, 1.0f / curve);
        }
        //-----------------

        /* Filter value integration */
        filter_sum += xRatio * yRatio * curveValue;
      }

      /* Store value */
      *current_fil_p = filter_sum;
      fil_val_sum += *current_fil_p;
    }
  }

  /* Normalization */
  current_fil_p = filter_p;
  for (int f = 0; f < filterDim.lx * filterDim.ly; f++, current_fil_p++) {
    *current_fil_p /= fil_val_sum;
  }
}

/*------------------------------------------------------------
 Unpremultiply -> convert to exposure value -> premultiply
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::convertRGBtoExposure_CPU(
    float4 *in_tile_p, TDimensionI &dim, const ExposureConverter &conv,
    bool sourceIsPremultiplied) {
  float4 *cur_tile_p = in_tile_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, cur_tile_p++) {
    /* if alpha is 0, return */
    if (cur_tile_p->w == 0.0f) {
      cur_tile_p->x = 0.0f;
      cur_tile_p->y = 0.0f;
      cur_tile_p->z = 0.0f;
      continue;
    }

    /* Unpremultiply on sources that are premultiplied, such as regular Level.
     * It is not done for 'digital overlay' (image with alpha mask added by
     * using Photoshop, as known as 'DigiBook' in Japanese animation industry)
     * etc. */
    if (sourceIsPremultiplied) {
      /* unpremultiply */
      cur_tile_p->x /= cur_tile_p->w;
      cur_tile_p->y /= cur_tile_p->w;
      cur_tile_p->z /= cur_tile_p->w;
    }

    /* convert RGB to Exposure */
    cur_tile_p->x = conv.valueToExposure(cur_tile_p->x);
    cur_tile_p->y = conv.valueToExposure(cur_tile_p->y);
    cur_tile_p->z = conv.valueToExposure(cur_tile_p->z);

    /* Then multiply with the alpha channel */
    cur_tile_p->x *= cur_tile_p->w;
    cur_tile_p->y *= cur_tile_p->w;
    cur_tile_p->z *= cur_tile_p->w;
  }
}

/*------------------------------------------------------------
 Filter and blur exposure values
 Loop for the range of 'outDim'.
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::applyBlurFilter_CPU(
    float4 *in_tile_p, float4 *out_tile_p, TDimensionI &enlargedDim,
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    int marginRight, int marginTop, TDimensionI &outDim) {
  for (int i = 0; i < outDim.lx * outDim.ly; i++) {
    /* in_tile_dev and out_tile_dev contain data with dimensions lx * ly.
     * So, convert i to coordinates for output. */
    int2 outPos  = {i % outDim.lx + marginRight, i / outDim.lx + marginTop};
    int outIndex = outPos.y * enlargedDim.lx + outPos.x;
    /* put the result in out_tile_dev [outIndex] */

    /* Prepare a container to accumulate values ​​- */
    float4 value = {0.0f, 0.0f, 0.0f, 0.0f};

    /* Loop by filter size.
     * Note that the filter is used to 'collect' pixels at sample points
     * so flip the filter vertically and horizontally and sample it */
    int filterIndex = 0;
    for (int fily = -marginBottom; fily < filterDim.ly - marginBottom; fily++) {
      /* Sample coordinates and index of the end of this scan line */
      int2 samplePos  = {outPos.x + marginLeft, outPos.y - fily};
      int sampleIndex = samplePos.y * enlargedDim.lx + samplePos.x;

      for (int filx = -marginLeft; filx < filterDim.lx - marginLeft;
           filx++, filterIndex++, sampleIndex--) {
        /* If the filter value is 0 or the sample pixel is transparent, continue
         */
        if (filter_p[filterIndex] == 0.0f || in_tile_p[sampleIndex].w == 0.0f)
          continue;
        /* multiply the sample point value by the filter value and integrate */
        value.x += in_tile_p[sampleIndex].x * filter_p[filterIndex];
        value.y += in_tile_p[sampleIndex].y * filter_p[filterIndex];
        value.z += in_tile_p[sampleIndex].z * filter_p[filterIndex];
        value.w += in_tile_p[sampleIndex].w * filter_p[filterIndex];
      }
    }

    out_tile_p[outIndex].x = value.x;
    out_tile_p[outIndex].y = value.y;
    out_tile_p[outIndex].z = value.z;
    out_tile_p[outIndex].w = value.w;
  }
}

/*------------------------------------------------------------
 Unpremultiply the exposure value
 -> convert back to RGB value (0 to 1)
 -> premultiply
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::convertExposureToRGB_CPU(
    float4 *out_tile_p, TDimensionI &dim, const ExposureConverter &conv) {
  float4 *cur_tile_p = out_tile_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, cur_tile_p++) {
    /* if alpha is 0 return */
    if (cur_tile_p->w == 0.0f) {
      cur_tile_p->x = 0.0f;
      cur_tile_p->y = 0.0f;
      cur_tile_p->z = 0.0f;
      continue;
    }

    // unpremultiply
    cur_tile_p->x /= cur_tile_p->w;
    cur_tile_p->y /= cur_tile_p->w;
    cur_tile_p->z /= cur_tile_p->w;

    /* Convert Exposure to RGB value */
    cur_tile_p->x = conv.exposureToValue(cur_tile_p->x);
    cur_tile_p->y = conv.exposureToValue(cur_tile_p->y);
    cur_tile_p->z = conv.exposureToValue(cur_tile_p->z);

    // multiply
    cur_tile_p->x *= cur_tile_p->w;
    cur_tile_p->y *= cur_tile_p->w;
    cur_tile_p->z *= cur_tile_p->w;
  }
}

/*------------------------------------------------------------
 If there is a background, and the foreground does not move, simply over
------------------------------------------------------------*/
void Iwa_MotionBlurCompFx::composeWithNoMotion(
    TTile &tile, double frame, const TRenderSettings &settings) {
  assert(m_background.isConnected());

  m_background->compute(tile, frame, settings);

  TTile fore_tile;
  m_input->allocateAndCompute(fore_tile, tile.m_pos,
                              tile.getRaster()->getSize(), tile.getRaster(),
                              frame, settings);

  TRasterP up(fore_tile.getRaster()), down(tile.getRaster());
  TRop::over(down, up);
}

/*------------------------------------------------------------
  Convert background to exposure value and over it
------------------------------------------------------------*/
void Iwa_MotionBlurCompFx::composeBackgroundExposure_CPU(
    float4 *out_tile_p, TDimensionI &enlargedDimIn, int marginRight,
    int marginTop, TTile &back_tile, TDimensionI &dimOut,
    const ExposureConverter &conv) {
  /* Memory allocation of host */
  TRasterGR8P background_host_ras(sizeof(float4) * dimOut.lx, dimOut.ly);
  background_host_ras->lock();
  float4 *background_host = (float4 *)background_host_ras->getRawData();

  bool bgIsPremultiplied;

  /* normalize the background image to 0 - 1 and read it into the host memory
   */
  TRaster32P backRas32 = (TRaster32P)back_tile.getRaster();
  TRaster64P backRas64 = (TRaster64P)back_tile.getRaster();
  TRasterFP backRasF   = (TRasterFP)back_tile.getRaster();
  if (backRas32)
    bgIsPremultiplied = setSourceRaster<TRaster32P, TPixel32>(
        backRas32, background_host, dimOut);
  else if (backRas64)
    bgIsPremultiplied = setSourceRaster<TRaster64P, TPixel64>(
        backRas64, background_host, dimOut);
  else if (backRasF)
    bgIsPremultiplied =
        setSourceRaster<TRasterFP, TPixelF>(backRasF, background_host, dimOut);

  float4 *bg_p = background_host;
  float4 *out_p;

  for (int j = 0; j < dimOut.ly; j++) {
    out_p = out_tile_p + ((marginTop + j) * enlargedDimIn.lx + marginRight);
    for (int i = 0; i < dimOut.lx; i++, bg_p++, out_p++) {
      /* if upper layer is completely opaque continue */
      if ((*out_p).w >= 1.0f) continue;

      /* even if the lower layer is completely transparent continue */
      if ((*bg_p).w < 0.0001f) continue;

      float3 bgExposure = {(*bg_p).x, (*bg_p).y, (*bg_p).z};

      /* Unpremultiply on sources that are premultiplied, such as regular Level.
       * It is not done for 'digital overlay' (image with alpha mask added by
       * using Photoshop, as known as 'DigiBook' in Japanese animation industry)
       * etc.
       */
      if (bgIsPremultiplied) {
        // Unpremultiply
        bgExposure.x /= (*bg_p).w;
        bgExposure.y /= (*bg_p).w;
        bgExposure.z /= (*bg_p).w;
      }

      /* Set RGB value to Exposure*/
      bgExposure.x = conv.valueToExposure(bgExposure.x);
      bgExposure.y = conv.valueToExposure(bgExposure.y);
      bgExposure.z = conv.valueToExposure(bgExposure.z);

      // multiply
      bgExposure.x *= (*bg_p).w;
      bgExposure.y *= (*bg_p).w;
      bgExposure.z *= (*bg_p).w;

      /* Over composite with front layers */
      (*out_p).x = (*out_p).x + bgExposure.x * (1.0f - (*out_p).w);
      (*out_p).y = (*out_p).y + bgExposure.y * (1.0f - (*out_p).w);
      (*out_p).z = (*out_p).z + bgExposure.z * (1.0f - (*out_p).w);
      /* Alpha value also over-composited */
      (*out_p).w = (*out_p).w + (*bg_p).w * (1.0f - (*out_p).w);
    }
  }
  background_host_ras->unlock();
}

//------------------------------------------------------------

Iwa_MotionBlurCompFx::Iwa_MotionBlurCompFx()
    : m_hardness(0.3)
    , m_gamma(2.2)
    , m_gammaAdjust(0.)
    /* Parameters for blurring left and right */
    , m_startValue(1.0)
    , m_startCurve(1.0)
    , m_endValue(1.0)
    , m_endCurve(1.0)
    , m_zanzoMode(false)
    , m_premultiType(new TIntEnumParam(AUTO, "Auto")) {
  /* Bind common parameters */
  addInputPort("Source", m_input);
  addInputPort("Back", m_background);

  bindParam(this, "hardness", m_hardness);
  bindParam(this, "gamma", m_gamma);
  bindParam(this, "gammaAdjust", m_gammaAdjust);
  bindParam(this, "shutterStart", m_shutterStart);
  bindParam(this, "shutterEnd", m_shutterEnd);
  bindParam(this, "traceResolution", m_traceResolution);
  bindParam(this, "motionObjectType", m_motionObjectType);
  bindParam(this, "motionObjectIndex", m_motionObjectIndex);

  bindParam(this, "startValue", m_startValue);
  bindParam(this, "startCurve", m_startCurve);
  bindParam(this, "endValue", m_endValue);
  bindParam(this, "endCurve", m_endCurve);

  bindParam(this, "zanzoMode", m_zanzoMode);

  bindParam(this, "premultiType", m_premultiType);

  /* Common parameter range setting */
  m_hardness->setValueRange(0.05, 10.0);
  m_gamma->setValueRange(1.0, 10.0);
  m_gammaAdjust->setValueRange(-5., 5.);
  m_startValue->setValueRange(0.0, 1.0);
  m_startCurve->setValueRange(0.1, 10.0);
  m_endValue->setValueRange(0.0, 1.0);
  m_endCurve->setValueRange(0.1, 10.0);

  m_premultiType->addItem(SOURCE_IS_PREMULTIPLIED, "Source is premultiplied");
  m_premultiType->addItem(SOURCE_IS_NOT_PREMUTIPLIED,
                          "Source is NOT premultiplied");

  getAttributes()->setIsSpeedAware(true);
  enableComputeInFloat(true);

  // Version 1: Exposure is computed by using Hardness
  //            E = std::pow(10.0, (value - 0.5) / hardness)
  // Version 2: Exposure is computed by using Gamma, for easier combination with
  // the linear color space
  //            E = std::pow(value, gamma)
  // Version 3: Gamma is computed by rs.m_colorSpaceGamma + gammaAdjust
  // this must be called after binding the parameters (see onFxVersionSet())
  setFxVersion(3);
}

//--------------------------------------------

void Iwa_MotionBlurCompFx::onFxVersionSet() {
  if (getFxVersion() == 1) {  // use hardness
    getParams()->getParamVar("hardness")->setIsHidden(false);
    getParams()->getParamVar("gamma")->setIsHidden(true);
    getParams()->getParamVar("gammaAdjust")->setIsHidden(true);
    return;
  }
  getParams()->getParamVar("hardness")->setIsHidden(true);

  bool useGamma = getFxVersion() == 2;
  if (useGamma) {
    // Automatically update version
    if (m_gamma->getKeyframeCount() == 0 &&
        areAlmostEqual(m_gamma->getDefaultValue(), 2.2)) {
      useGamma = false;
      setFxVersion(3);
    }
  }
  getParams()->getParamVar("gamma")->setIsHidden(!useGamma);
  getParams()->getParamVar("gammaAdjust")->setIsHidden(useGamma);
}

//------------------------------------------------------------

void Iwa_MotionBlurCompFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &settings) {
  /* Do not process if not connected */
  if (!m_input.isConnected() && !m_background.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  /* For BG only connection */
  if (!m_input.isConnected()) {
    m_background->compute(tile, frame, settings);
    return;
  }

  /* Get parameters */
  QList<TPointD> points = getAttributes()->getMotionPoints();
  double gamma;
  // The hardness value had been used inversely with the bokeh fxs.
  // Now the convertion functions are shared with the bokeh fxs,
  // so we need to change the hardness to reciprocal in order to obtain
  // the same result as previous versions.
  if (getFxVersion() == 1)
    gamma = 1. / m_hardness->getValue(frame);
  else {  // gamma
    if (getFxVersion() == 2)
      gamma = m_gamma->getValue(frame);
    else
      gamma = std::max(
          1., settings.m_colorSpaceGamma + m_gammaAdjust->getValue(frame));
    if (tile.getRaster()->isLinear()) gamma /= settings.m_colorSpaceGamma;
  }
  double shutterStart = m_shutterStart->getValue(frame);
  double shutterEnd   = m_shutterEnd->getValue(frame);
  int traceResolution = m_traceResolution->getValue();
  float startValue    = (float)m_startValue->getValue(frame);
  float startCurve    = (float)m_startCurve->getValue(frame);
  float endValue      = (float)m_endValue->getValue(frame);
  float endCurve      = (float)m_endCurve->getValue(frame);

  /* Do not process if there are no more than two trajectory data */
  if (points.size() < 2) {
    if (!m_background.isConnected()) m_input->compute(tile, frame, settings);
    /* If there is a background and the foreground does not move, simply over */
    else
      composeWithNoMotion(tile, frame, settings);
    return;
  }
  /* Get display area */
  TRectD bBox =
      TRectD(tile.m_pos /* Render position on Pixel unit */
             ,
             TDimensionD(/* Size of Render image (Pixel unit) */
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));

  /* Get upper, lower, left and right margin */
  double minX = 0.0;
  double maxX = 0.0;
  double minY = 0.0;
  double maxY = 0.0;
  for (int p = 0; p < points.size(); p++) {
    if (points.at(p).x > maxX) maxX = points.at(p).x;
    if (points.at(p).x < minX) minX = points.at(p).x;
    if (points.at(p).y > maxY) maxY = points.at(p).y;
    if (points.at(p).y < minY) minY = points.at(p).y;
  }
  int marginLeft   = (int)ceil(std::abs(minX));
  int marginRight  = (int)ceil(std::abs(maxX));
  int marginTop    = (int)ceil(std::abs(maxY));
  int marginBottom = (int)ceil(std::abs(minY));

  /* Return the input tile as-is if there is not movement
   * (= filter margins are all 0). */
  if (marginLeft == 0 && marginRight == 0 && marginTop == 0 &&
      marginBottom == 0) {
    if (!m_background.isConnected()) m_input->compute(tile, frame, settings);
    /* If there is a background, and the foreground does not move, simply over
     */
    else
      composeWithNoMotion(tile, frame, settings);
    return;
  }

  /* resize the bbox with the upper/lower/left/right inverted margins.
   */
  TRectD enlargedBBox(bBox.x0 - (double)marginRight,
                      bBox.y0 - (double)marginTop, bBox.x1 + (double)marginLeft,
                      bBox.y1 + (double)marginBottom);

  // std::cout<<"Margin Left:"<<marginLeft<<" Right:"<<marginRight<<
  //	" Bottom:"<<marginBottom<<" Top:"<<marginTop<<std::endl;

  TDimensionI enlargedDimIn(/* Rounded to the nearest Pixel */
                            (int)(enlargedBBox.getLx() + 0.5),
                            (int)(enlargedBBox.getLy() + 0.5));

  TTile enlarge_tile;
  m_input->allocateAndCompute(enlarge_tile, enlargedBBox.getP00(),
                              enlargedDimIn, tile.getRaster(), frame, settings);

  /* If background is required */
  TTile back_Tile;
  if (m_background.isConnected()) {
    m_background->allocateAndCompute(back_Tile, tile.m_pos,
                                     tile.getRaster()->getSize(),
                                     tile.getRaster(), frame, settings);
  }

  //-------------------------------------------------------
  /* Compute range */
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TDimensionI filterDim(marginLeft + marginRight + 1,
                        marginTop + marginBottom + 1);

  /* Release of pointsTable is done within each doCompute */
  int pointAmount     = points.size();
  float4 *pointsTable = new float4[pointAmount];
  float dt = (float)(shutterStart + shutterEnd) / (float)traceResolution;
  for (int p = 0; p < pointAmount; p++) {
    pointsTable[p].x = (float)points.at(p).x;
    pointsTable[p].y = (float)points.at(p).y;
    /* z stores the distance of p -> p + 1 vector */
    if (p < pointAmount - 1) {
      float2 vec       = {(float)(points.at(p + 1).x - points.at(p).x),
                          (float)(points.at(p + 1).y - points.at(p).y)};
      pointsTable[p].z = sqrtf(vec.x * vec.x + vec.y * vec.y);
    }
    /* w stores shutter time offset */
    pointsTable[p].w = -(float)shutterStart + (float)p * dt;
  }

  doCompute_CPU(tile, frame, settings, pointsTable, pointAmount, gamma,
                shutterStart, shutterEnd, traceResolution, startValue,
                startCurve, endValue, endCurve, marginLeft, marginRight,
                marginTop, marginBottom, enlargedDimIn, enlarge_tile, dimOut,
                filterDim, back_Tile);
}

//------------------------------------------------------------

void Iwa_MotionBlurCompFx::doCompute_CPU(
    TTile &tile, double frame, const TRenderSettings &settings,
    float4 *pointsTable, int pointAmount, double gamma, double shutterStart,
    double shutterEnd, int traceResolution, float startValue, float startCurve,
    float endValue, float endCurve, int marginLeft, int marginRight,
    int marginTop, int marginBottom, TDimensionI &enlargedDimIn,
    TTile &enlarge_tile, TDimensionI &dimOut, TDimensionI &filterDim,
    TTile &back_tile) {
  /* Processing memory */
  float4 *in_tile_p;  /* With margin */
  float4 *out_tile_p; /* With margin */
  /* Filter */
  float *filter_p;

  /* Memory allocation */
  TRasterGR8P in_tile_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  in_tile_ras->lock();
  in_tile_p = (float4 *)in_tile_ras->getRawData();
  TRasterGR8P out_tile_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  out_tile_ras->lock();
  out_tile_p = (float4 *)out_tile_ras->getRawData();
  TRasterGR8P filter_ras(sizeof(float) * filterDim.lx, filterDim.ly);
  filter_ras->lock();
  filter_p = (float *)filter_ras->getRawData();

  bool sourceIsPremultiplied;
  /* normalize the source image to 0 - 1 and read it into memory */
  TRaster32P ras32 = (TRaster32P)enlarge_tile.getRaster();
  TRaster64P ras64 = (TRaster64P)enlarge_tile.getRaster();
  TRasterFP rasF   = (TRasterFP)enlarge_tile.getRaster();
  if (ras32)
    sourceIsPremultiplied = setSourceRaster<TRaster32P, TPixel32>(
        ras32, in_tile_p, enlargedDimIn,
        (PremultiTypes)m_premultiType->getValue());
  else if (ras64)
    sourceIsPremultiplied = setSourceRaster<TRaster64P, TPixel64>(
        ras64, in_tile_p, enlargedDimIn,
        (PremultiTypes)m_premultiType->getValue());
  else if (rasF)
    sourceIsPremultiplied = setSourceRaster<TRasterFP, TPixelF>(
        rasF, in_tile_p, enlargedDimIn,
        (PremultiTypes)m_premultiType->getValue());

  /* When afterimage mode is off */
  if (!m_zanzoMode->getValue()) {
    /* Create and normalize filters */
    makeMotionBlurFilter_CPU(filter_p, filterDim, marginLeft, marginBottom,
                             pointsTable, pointAmount, startValue, startCurve,
                             endValue, endCurve);
  }
  /* When afterimage mode is ON */
  else {
    /* Create / normalize the afterimage filter */
    makeZanzoFilter_CPU(filter_p, filterDim, marginLeft, marginBottom,
                        pointsTable, pointAmount, startValue, startCurve,
                        endValue, endCurve);
  }

  delete[] pointsTable;

  /* Unpremultiply RGB value (0 to 1)
   * -> convert it to exposure value
   * -> premultiply again
   */
  if (getFxVersion() == 1)
    convertRGBtoExposure_CPU(
        in_tile_p, enlargedDimIn,
        HardnessBasedConverter(gamma, settings.m_colorSpaceGamma,
                               enlarge_tile.getRaster()->isLinear()),
        sourceIsPremultiplied);
  else
    convertRGBtoExposure_CPU(in_tile_p, enlargedDimIn,
                             GammaBasedConverter(gamma), sourceIsPremultiplied);

  /* Filter and blur exposure value */
  applyBlurFilter_CPU(in_tile_p, out_tile_p, enlargedDimIn, filter_p, filterDim,
                      marginLeft, marginBottom, marginRight, marginTop, dimOut);
  /* Memory release */
  in_tile_ras->unlock();
  filter_ras->unlock();

  /* If there is a background, do Exposure multiplication */
  if (m_background.isConnected()) {
    if (getFxVersion() == 1)
      composeBackgroundExposure_CPU(
          out_tile_p, enlargedDimIn, marginRight, marginTop, back_tile, dimOut,
          HardnessBasedConverter(gamma, settings.m_colorSpaceGamma,
                                 tile.getRaster()->isLinear()));
    else
      composeBackgroundExposure_CPU(out_tile_p, enlargedDimIn, marginRight,
                                    marginTop, back_tile, dimOut,
                                    GammaBasedConverter(gamma));
  }
  /* Unpremultiply the exposure value
   * -> convert back to RGB value (0 to 1)
   * -> premultiply */
  if (getFxVersion() == 1)
    convertExposureToRGB_CPU(
        out_tile_p, enlargedDimIn,
        HardnessBasedConverter(gamma, settings.m_colorSpaceGamma,
                               tile.getRaster()->isLinear()));
  else
    convertExposureToRGB_CPU(out_tile_p, enlargedDimIn,
                             GammaBasedConverter(gamma));

  /* Clear raster */
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  TRasterFP outRasF   = (TRasterFP)tile.getRaster();
  int2 margin         = {marginRight, marginTop};
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(out_tile_p, outRas32, enlargedDimIn,
                                          margin);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(out_tile_p, outRas64, enlargedDimIn,
                                          margin);
  else if (outRasF)
    setOutputRaster<TRasterFP, TPixelF>(out_tile_p, outRasF, enlargedDimIn,
                                        margin);

  /* Memory release */
  out_tile_ras->unlock();
}

//------------------------------------------------------------

bool Iwa_MotionBlurCompFx::doGetBBox(double frame, TRectD &bBox,
                                     const TRenderSettings &info) {
  if (!m_input.isConnected() && !m_background.isConnected()) {
    bBox = TRectD();
    return false;
  }

  /* Rough implementation - return infinite size if the background is connected
   */
  if (m_background.isConnected()) {
    bool _ret = m_background->doGetBBox(frame, bBox, info);
    bBox      = TConsts::infiniteRectD;
    return _ret;
  }

  bool ret = m_input->doGetBBox(frame, bBox, info);

  if (bBox == TConsts::infiniteRectD) return true;

  QList<TPointD> points = getAttributes()->getMotionPoints();
  /* Compute the margin from the bounding box of the moved trajectory */
  /* Obtain the maximum absolute value of the coordinates of each trajectory
   * point */
  /* Get upper, lower, left and right margins */
  double minX = 0.0;
  double maxX = 0.0;
  double minY = 0.0;
  double maxY = 0.0;
  for (int p = 0; p < points.size(); p++) {
    if (points.at(p).x > maxX) maxX = points.at(p).x;
    if (points.at(p).x < minX) minX = points.at(p).x;
    if (points.at(p).y > maxY) maxY = points.at(p).y;
    if (points.at(p).y < minY) minY = points.at(p).y;
  }
  int marginLeft   = (int)ceil(std::abs(minX));
  int marginRight  = (int)ceil(std::abs(maxX));
  int marginTop    = (int)ceil(std::abs(maxY));
  int marginBottom = (int)ceil(std::abs(minY));

  TRectD enlargedBBox(
      bBox.x0 - (double)marginLeft, bBox.y0 - (double)marginBottom,
      bBox.x1 + (double)marginRight, bBox.y1 + (double)marginTop);

  bBox = enlargedBBox;

  return ret;
}

//------------------------------------------------------------

bool Iwa_MotionBlurCompFx::canHandle(const TRenderSettings &info,
                                     double frame) {
  return true;
}

/*------------------------------------------------------------
 Since there is a possibility that the reference object is moving,
 Change the alias every frame
------------------------------------------------------------*/

std::string Iwa_MotionBlurCompFx::getAlias(double frame,
                                           const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias of the effects related to the input ports separated by commas
  // a port that is not connected to an alias blank (empty string)
  int i;
  for (i = 0; i < getInputPortCount(); i++) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  std::string paramalias("");
  for (i = 0; i < getParams()->getParamCount(); i++) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }
  unsigned long id = getIdentifier();
  return alias + std::to_string(frame) + "," + std::to_string(id) + paramalias +
         "]";
}

//------------------------------------------------------------

bool Iwa_MotionBlurCompFx::toBeComputedInLinearColorSpace(
    bool settingsIsLinear, bool tileIsLinear) const {
  return settingsIsLinear;
}

FX_PLUGIN_IDENTIFIER(Iwa_MotionBlurCompFx, "iwa_MotionBlurCompFx")
