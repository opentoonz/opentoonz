/*------------------------------------
Iwa_SpectrumFx
参照画像を位相差として、干渉色を出力する
------------------------------------*/

#include "iwa_spectrumfx.h"

#include "iwa_cie_d65.h"
#include "iwa_xyz.h"

namespace {
const float PI = 3.14159265f;
}

/*------------------------------------
 Calculate soap bubble color map
------------------------------------*/
void Iwa_SpectrumFx::calcBubbleMap(float3 *bubbleColor, double frame,
                                   bool isLinear, double colorSpaceGamma,
                                   bool computeAngularAxis) {
  int i, j, k;  /* bubbleColor[j][k] = [256][3] */
  float d;      /* Thickness of the film (μm) */
  int ram;      /* rambda iterator */
  float rambda; /* wavelength of light (μm) */
  struct REFLECTIVITY {
    /* transmission and reflection amplitudes for each boundary */
    float r_ab, t_ab, r_ba, t_ba;
    float r_real, r_img; /* reflection amplitude of the film */
    float R;             /* energy reflectance */
  } p, s;
  float R_final;                   /* combined energy reflectance */
  float phi;                       /* phase */
  float color_x, color_y, color_z; /* xyz color channels */

  /* obtain parameters */
  float intensity       = (float)m_intensity->getValue(frame);
  float refractiveIndex = (float)m_refractiveIndex->getValue(frame);
  float thickMax        = (float)m_thickMax->getValue(frame);
  float thickMin        = (float)m_thickMin->getValue(frame);

  // Currently isLinear should be always false
  assert(!isLinear);
  float gammaAdjust = (getFxVersion() == 1 && isLinear) ? (float)colorSpaceGamma
                      : (getFxVersion() >= 2 && !isLinear)
                          ? 1.f / (float)colorSpaceGamma
                          : 1.f;

  float rgbGamma[3] = {(float)m_RGamma->getValue(frame) * gammaAdjust,
                       (float)m_GGamma->getValue(frame) * gammaAdjust,
                       (float)m_BGamma->getValue(frame) * gammaAdjust};

  float lensFactor = (float)m_lensFactor->getValue(frame);
  float shift      = (float)m_spectrumShift->getValue(frame);
  float fadeWidth  = (float)m_loopSpectrumFadeWidth->getValue(frame) / 2.0f;

  /* for Iwa_SpectrumFx, incident angle is fixed to 0,
     for Iwa_SoapBubbleFx, compute for all discrete incident angles*/
  int i_max        = (computeAngularAxis) ? 256 : 1;
  float3 *bubble_p = bubbleColor;

  /* for each discrete incident angle */
  for (i = 0; i < i_max; i++) {
    /* incident angle (radian) */
    float angle_in = PI / 2.0f / 255.0f * (float)i;
    /* refraction angle (radian) */
    float angle_re = asinf(sinf(angle_in) / refractiveIndex);

    /* transmission and reflection amplitudes for each boundary, for each
     * polarization */
    float cos_in = cosf(angle_in);
    float cos_re = cosf(angle_re);

    // compute the offset in order to make the seam of looped-spectrum curved
    // along the stripe
    float seam_offset = 0.0f;
    if (fadeWidth != 0.0f) {  // if the fade width is 0, the seam does not curve
      float base_light_diff =
          (thickMax + thickMin) / cosf(asinf(1 / refractiveIndex));
      float offset_width = 0.5f * (base_light_diff - thickMax - thickMin);
      seam_offset        = 0.5f * (base_light_diff *
                                cosf(asinf(cosf(angle_in) / refractiveIndex)) -
                            thickMax - thickMin - offset_width);
    }

    // P-polarized light
    p.r_ab = (cos_re - refractiveIndex * cos_in) /
             (cos_re + refractiveIndex * cos_re);
    p.t_ab = (1.0f - p.r_ab) / refractiveIndex;
    p.r_ba = -p.r_ab;
    p.t_ba = (1.0f + p.r_ab) * refractiveIndex;
    // S-polarized light
    s.r_ab = (cos_in - refractiveIndex * cos_re) /
             (cos_in + refractiveIndex * cos_re);
    s.t_ab = 1.0f + s.r_ab;
    s.r_ba = -s.r_ab;
    s.t_ba = 1.0f - s.r_ab;

    /* for each discrete thickness */
    for (j = 0; j < 256; j++) {
      // normalize within 0-1 and shift
      float t = (float)j / 255.0f + shift;
      // get fractional part
      t -= std::floor(t);
      // apply lens factor
      t = powf(t, lensFactor);

      float tmp_rgb[2][3];
      float tmp_t[2];
      float tmp_ratio[2];

      if (t < seam_offset - fadeWidth) {
        tmp_t[0]     = t + 1.0f;
        tmp_t[1]     = 0;  // unused
        tmp_ratio[0] = 1.0f;
        tmp_ratio[1] = 0.0f;
      } else if (t < seam_offset + fadeWidth) {
        tmp_t[0]     = t;
        tmp_t[1]     = t + 1.0f;
        tmp_ratio[0] = 0.5f + 0.5f * (t - seam_offset) / fadeWidth;
        tmp_ratio[1] = 1.0f - tmp_ratio[0];
      } else if (t > 1.0f + seam_offset + fadeWidth) {
        tmp_t[0]     = t - 1.0f;
        tmp_t[1]     = 0;  // unused
        tmp_ratio[0] = 1.0f;
        tmp_ratio[1] = 0.0f;
      } else if (t > 1.0f + seam_offset - fadeWidth) {
        tmp_t[0]     = t;
        tmp_t[1]     = t - 1.0f;
        tmp_ratio[0] = 0.5f + 0.5f * (1.0f - t + seam_offset) / fadeWidth;
        tmp_ratio[1] = 1.0f - tmp_ratio[0];
      } else {  // no fade
        tmp_t[0]     = t;
        tmp_t[1]     = 0;  // unused
        tmp_ratio[0] = 1.0f;
        tmp_ratio[1] = 0.0f;
      }

      /* compute colors for two thickness values and fade them*/
      for (int fadeId = 0; fadeId < 2; fadeId++) {
        // if composite ratio is 0, skip computing
        if (tmp_ratio[fadeId] == 0.0f) continue;

        /* calculate the thickness of film (μm) */
        d = thickMin + (thickMax - thickMin) * tmp_t[fadeId];

        /* there may be a case that the thickness is smaller than 0 */
        if (d < 0.0f) d = 0.0f;

        /* initialize XYZ color channels */
        color_x = 0.0f;
        color_y = 0.0f;
        color_z = 0.0f;

        /* for each wavelength (in the range of visible light, 380nm-710nm) */
        for (ram = 0; ram < 34; ram++) {
          /* wavelength `λ` (μm) */
          rambda = 0.38f + 0.01f * (float)ram;
          /* phase of light */
          phi = 4.0f * PI * refractiveIndex * d * cos_re / rambda;
          /* reflection amplitude of the film for each polarization */
          // P-polarized light
          p.r_real = p.r_ab + p.t_ab * p.r_ba * p.t_ba * cosf(phi);
          p.r_img  = p.t_ab * p.r_ba * p.t_ba * sinf(phi);
          // S-polarized light
          s.r_real = s.r_ab + s.t_ab * s.r_ba * s.t_ba * cosf(phi);
          s.r_img  = s.t_ab * s.r_ba * s.t_ba * sinf(phi);

          p.R = p.r_real * p.r_real + p.r_img * p.r_img;
          s.R = s.r_real * s.r_real + s.r_img * s.r_img;

          /* combined energy reflectance */
          R_final = (p.R + s.R) / 2.0f;

          /* accumulate XYZ channel values */
          color_x += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 0];
          color_y += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 1];
          color_z += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 2];

        } /* next wavelength (ram) */

        tmp_rgb[fadeId][0] =
            3.240479f * color_x - 1.537150f * color_y - 0.498535f * color_z;
        tmp_rgb[fadeId][1] =
            -0.969256f * color_x + 1.875992f * color_y + 0.041556f * color_z;
        tmp_rgb[fadeId][2] =
            0.055648f * color_x - 0.204043f * color_y + 1.057311f * color_z;

        /* clamp overflows */
        for (k = 0; k < 3; k++) {
          if (tmp_rgb[fadeId][k] < 0.0f) tmp_rgb[fadeId][k] = 0.0f;

          /* gamma adjustment */
          tmp_rgb[fadeId][k] = powf((tmp_rgb[fadeId][k] / 255.0f), rgbGamma[k]);

          // clamp will be done when storing the result raster
          // if (tmp_rgb[fadeId][k] >= 1.0f) tmp_rgb[fadeId][k] = 1.0f;
        }
      }
      bubble_p->x = tmp_rgb[0][0] * tmp_ratio[0] + tmp_rgb[1][0] * tmp_ratio[1];
      bubble_p->y = tmp_rgb[0][1] * tmp_ratio[0] + tmp_rgb[1][1] * tmp_ratio[1];
      bubble_p->z = tmp_rgb[0][2] * tmp_ratio[0] + tmp_rgb[1][2] * tmp_ratio[1];
      bubble_p++;

    } /*- next thickness d (j) -*/
  }   /*- next incident angle (i) -*/
}

//------------------------------------
Iwa_SpectrumFx::Iwa_SpectrumFx()
    : m_intensity(1.0)
    , m_refractiveIndex(1.25)
    , m_thickMax(1.0)
    , m_thickMin(0.0)
    , m_RGamma(1.0)
    , m_GGamma(1.0)
    , m_BGamma(1.0)
    , m_lensFactor(1.0)
    , m_lightThres(1.0)
    , m_lightIntensity(1.0)
    , m_loopSpectrumFadeWidth(0.0)
    , m_spectrumShift(0.0) {
  // Fx Version 1: *2.2 Gamma when linear rendering (it duplicately applies
  // gamma and is not correct) Fx Version 2: *1/2.2 Gamma when non-linear
  // rendering
  setFxVersion(2);

  addInputPort("Source", m_input);
  addInputPort("Light", m_light);
  bindParam(this, "intensity", m_intensity);
  bindParam(this, "refractiveIndex", m_refractiveIndex);
  bindParam(this, "thickMax", m_thickMax);
  bindParam(this, "thickMin", m_thickMin);
  bindParam(this, "RGamma", m_RGamma);
  bindParam(this, "GGamma", m_GGamma);
  bindParam(this, "BGamma", m_BGamma);
  bindParam(this, "lensFactor", m_lensFactor);
  bindParam(this, "lightThres", m_lightThres);
  bindParam(this, "lightIntensity", m_lightIntensity);
  bindParam(this, "loopSpectrumFadeWidth", m_loopSpectrumFadeWidth);
  bindParam(this, "spectrumShift", m_spectrumShift);

  m_intensity->setValueRange(0.0, 8.0);
  m_refractiveIndex->setValueRange(1.0, 3.0);
  m_thickMax->setValueRange(-1.5, 2.0);
  m_thickMin->setValueRange(-1.5, 2.0);
  m_RGamma->setValueRange(0.001, 5.0);
  m_GGamma->setValueRange(0.001, 5.0);
  m_BGamma->setValueRange(0.001, 5.0);
  m_lensFactor->setValueRange(0.01, 10.0);
  m_lightThres->setValueRange(-5.0, 1.0);
  m_lightIntensity->setValueRange(0.0, 1.0);
  m_loopSpectrumFadeWidth->setValueRange(0.0, 1.0);
  m_spectrumShift->setValueRange(-10.0, 10.0);

  enableComputeInFloat(true);
}

//------------------------------------
void Iwa_SpectrumFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &settings) {
  if (!m_input.isConnected()) return;

  /*- 薄膜干渉色マップ -*/
  float3 *bubbleColor;

  TDimensionI dim(tile.getRaster()->getLx(), tile.getRaster()->getLy());

  /*- 256段階で干渉色を計算 -*/
  TRasterGR8P bubbleColor_ras(sizeof(float3) * 256, 1);
  bubbleColor_ras->lock();
  bubbleColor = (float3 *)bubbleColor_ras->getRawData();

  /*- シャボン色マップの生成 -*/
  calcBubbleMap(bubbleColor, frame, tile.getRaster()->isLinear(),
                settings.m_colorSpaceGamma);

  /*- いったん素材をTileに収める -*/
  m_input->compute(tile, frame, settings);

  /*--------------------
   ここで、Lightが刺さっていた場合は、Lightのアルファを使用＆HDRThresでスクリーン合成
  --------------------*/
  TRasterP lightRas = 0;
  if (m_light.isConnected()) {
    TTile light_tile;
    m_light->allocateAndCompute(light_tile, tile.m_pos, dim, tile.getRaster(),
                                frame, settings);

    lightRas = light_tile.getRaster();
    lightRas->lock();
  }

  TRaster32P ras32 = (TRaster32P)tile.getRaster();
  TRaster64P ras64 = (TRaster64P)tile.getRaster();
  TRasterFP rasF   = (TRasterFP)tile.getRaster();
  {
    if (ras32) {
      if (lightRas)
        convertRasterWithLight<TRaster32P, TPixel32>(
            ras32, dim, bubbleColor, (TRaster32P)lightRas,
            (float)m_lightThres->getValue(frame),
            (float)m_lightIntensity->getValue(frame));
      else
        convertRaster<TRaster32P, TPixel32>(ras32, dim, bubbleColor);
    } else if (ras64) {
      if (lightRas)
        convertRasterWithLight<TRaster64P, TPixel64>(
            ras64, dim, bubbleColor, (TRaster64P)lightRas,
            (float)m_lightThres->getValue(frame),
            (float)m_lightIntensity->getValue(frame));
      else
        convertRaster<TRaster64P, TPixel64>(ras64, dim, bubbleColor);
    } else if (rasF) {
      if (lightRas)
        convertRasterWithLight<TRasterFP, TPixelF>(
            rasF, dim, bubbleColor, (TRasterFP)lightRas,
            (float)m_lightThres->getValue(frame),
            (float)m_lightIntensity->getValue(frame));
      else
        convertRaster<TRasterFP, TPixelF>(rasF, dim, bubbleColor);
    }
  }

  // メモリ解放
  //  brightness_ras->unlock();
  bubbleColor_ras->unlock();
  if (lightRas) lightRas->unlock();
}

//------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_SpectrumFx::convertRaster(const RASTER ras, TDimensionI dim,
                                   float3 *bubbleColor) {
  float rr, gg, bb, aa;
  float spec_r, spec_g, spec_b;
  float brightness;

  bool doClamp = (ras->getPixelSize() != 16);

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = ras->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      aa = (float)pix->m / PIXEL::maxChannelValue;
      if (aa == 0.0f) /*- アルファが０なら変化なし -*/
      {
        pix++;
        continue;
      }
      /*- depremutiplyはしないでおく -*/
      rr         = (float)pix->r / (float)PIXEL::maxChannelValue;
      gg         = (float)pix->g / (float)PIXEL::maxChannelValue;
      bb         = (float)pix->b / (float)PIXEL::maxChannelValue;
      brightness = 0.298912f * rr + 0.586611f * gg + 0.114478f * bb;

      /*- 反転 -*/
      brightness = 1.0f - brightness;
      /*- 輝度MAXの場合 -*/
      if (brightness >= 1.0f) {
        spec_r = bubbleColor[255].x * aa;
        spec_g = bubbleColor[255].y * aa;
        spec_b = bubbleColor[255].z * aa;
      } else {
        /*- 線形補間する -*/
        int index   = (int)(brightness * 255.0f);
        float ratio = brightness * 255.0f - (float)index;

        spec_r = bubbleColor[index].x * (1.0f - ratio) +
                 bubbleColor[index + 1].x * ratio;
        spec_g = bubbleColor[index].y * (1.0f - ratio) +
                 bubbleColor[index + 1].y * ratio;
        spec_b = bubbleColor[index].z * (1.0f - ratio) +
                 bubbleColor[index + 1].z * ratio;
        spec_r *= aa;
        spec_g *= aa;
        spec_b *= aa;
      }
      /*- 元のピクセルに書き戻す -*/
      if (doClamp) {
        float val;
        /*- チャンネル範囲にクランプ -*/
        val    = spec_r * (float)PIXEL::maxChannelValue + 0.5f;
        pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
        val    = spec_g * (float)PIXEL::maxChannelValue + 0.5f;
        pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
        val    = spec_b * (float)PIXEL::maxChannelValue + 0.5f;
        pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
      } else {  // floating point case
        pix->r = spec_r;
        pix->g = spec_g;
        pix->b = spec_b;
      }

      pix++;
    }
  }
}

//------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_SpectrumFx::convertRasterWithLight(const RASTER ras, TDimensionI dim,
                                            float3 *bubbleColor,
                                            const RASTER lightRas,
                                            float lightThres,
                                            float lightIntensity) {
  float rr, gg, bb, aa;
  float spec_r, spec_g, spec_b;
  float brightness;

  bool doClamp = (ras->getPixelSize() != 16);

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *light_pix = lightRas->pixels(j);
    PIXEL *pix       = ras->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      aa = (float)light_pix->m / PIXEL::maxChannelValue;
      if (aa == 0.0f) /*- アルファが０なら透明にする -*/
      {
        *pix = PIXEL::Transparent;
        light_pix++;
        pix++;
        continue;
      }
      /*- depremutiplyはしないでおく -*/
      rr         = (float)pix->r / (float)PIXEL::maxChannelValue;
      gg         = (float)pix->g / (float)PIXEL::maxChannelValue;
      bb         = (float)pix->b / (float)PIXEL::maxChannelValue;
      brightness = 0.298912f * rr + 0.586611f * gg + 0.114478f * bb;

      /*- 反転 -*/
      brightness = 1.0f - brightness;
      /*- 輝度MAXの場合 -*/
      if (brightness >= 1.0f) {
        spec_r = bubbleColor[255].x;
        spec_g = bubbleColor[255].y;
        spec_b = bubbleColor[255].z;
      } else {
        /*- 線形補間する -*/
        int index   = (int)(brightness * 255.0f);
        float ratio = brightness * 255.0f - (float)index;

        spec_r = bubbleColor[index].x * (1.0f - ratio) +
                 bubbleColor[index + 1].x * ratio;
        spec_g = bubbleColor[index].y * (1.0f - ratio) +
                 bubbleColor[index + 1].y * ratio;
        spec_b = bubbleColor[index].z * (1.0f - ratio) +
                 bubbleColor[index + 1].z * ratio;
      }

      /*- ここで、Light画像とのスクリーン合成を行う -*/
      float HDR_Factor;
      if (aa <= lightThres || lightThres == 1.0f)
        HDR_Factor = 0.0;
      else
        HDR_Factor = lightIntensity * (aa - lightThres) / (1.0 - lightThres);

      float light_r = (float)light_pix->r / (float)PIXEL::maxChannelValue;
      float light_g = (float)light_pix->g / (float)PIXEL::maxChannelValue;
      float light_b = (float)light_pix->b / (float)PIXEL::maxChannelValue;
      /*- スクリーン合成結果と虹色をHDR_Factorで混ぜる -*/
      spec_r = (1.0f - HDR_Factor) * spec_r +
               HDR_Factor * (spec_r + light_r - spec_r * light_r);
      spec_g = (1.0f - HDR_Factor) * spec_g +
               HDR_Factor * (spec_g + light_g - spec_g * light_g);
      spec_b = (1.0f - HDR_Factor) * spec_b +
               HDR_Factor * (spec_b + light_b - spec_b * light_b);

      spec_r *= aa;
      spec_g *= aa;
      spec_b *= aa;

      /*- 元のピクセルに書き戻す -*/
      if (doClamp) {
        float val;
        /*- チャンネル範囲にクランプ -*/
        val    = spec_r * (float)PIXEL::maxChannelValue + 0.5f;
        pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
        val    = spec_g * (float)PIXEL::maxChannelValue + 0.5f;
        pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
        val    = spec_b * (float)PIXEL::maxChannelValue + 0.5f;
        pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                               ? (float)PIXEL::maxChannelValue
                                               : val);
      } else {  // floating point case
        pix->r = spec_r;
        pix->g = spec_g;
        pix->b = spec_b;
      }
      pix->m = light_pix->m;

      pix++;
      light_pix++;
    }
  }
}

/*------------------------------------
 素材タイルを０〜１に正規化して格納
------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_SpectrumFx::setSourceRasters(const RASTER ras,
                                      float4 *in_out_tile_host,
                                      const RASTER light_ras,
                                      float4 *light_host, TDimensionI dim,
                                      bool useLight) {
  float4 *chann_p      = in_out_tile_host;
  float4 *lightChann_p = light_host;

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix      = ras->pixels(j);
    PIXEL *lightPix = (useLight) ? light_ras->pixels(j) : 0;
    for (int i = 0; i < dim.lx; i++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;
      pix++;
      chann_p++;

      if (useLight) {
        (*lightChann_p).x = (float)lightPix->r / (float)PIXEL::maxChannelValue;
        (*lightChann_p).y = (float)lightPix->g / (float)PIXEL::maxChannelValue;
        (*lightChann_p).z = (float)lightPix->b / (float)PIXEL::maxChannelValue;
        (*lightChann_p).w = (float)lightPix->m / (float)PIXEL::maxChannelValue;
        lightPix++;
        lightChann_p++;
      }
    }
  }
}

/*------------------------------------
 出力結果をChannel値に変換してタイルに格納
------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_SpectrumFx::outputRasters(const RASTER outRas,
                                   float4 *in_out_tile_host, TDimensionI dim) {
  float4 *chann_p = in_out_tile_host;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = outRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      float val;
      val    = (*chann_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chann_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chann_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chann_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      pix++;
      chann_p++;
    }
  }
}

//------------------------------------

bool Iwa_SpectrumFx::doGetBBox(double frame, TRectD &bBox,
                               const TRenderSettings &info) {
  if (!m_input.isConnected()) {
    bBox = TRectD();
    return false;
  }
  return m_input->doGetBBox(frame, bBox, info);
}

//------------------------------------

bool Iwa_SpectrumFx::canHandle(const TRenderSettings &info, double frame) {
  return true;
}

FX_PLUGIN_IDENTIFIER(Iwa_SpectrumFx, "iwa_SpectrumFx")
