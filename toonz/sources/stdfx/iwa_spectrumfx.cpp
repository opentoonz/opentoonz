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
 シャボン色マップの生成
------------------------------------*/
void Iwa_SpectrumFx::calcBubbleMap(float3 *bubbleColor, double frame) {
  int j, k;     /*- bubbleColor[j][k] = [256][3] -*/
  float d;      /*- 膜厚(μm) -*/
  int ram;      /*- 波長のfor文用 -*/
  float rambda; /*- 波長(μm) -*/
  struct REFLECTIVITY {
    float r_ab, t_ab, r_ba, t_ba; /*- 各境界での振幅反射率、振幅透過率 -*/
    float r_real, r_img; /*- 薄膜の振幅反射率 -*/
    float R;             /*- エネルギー反射率 -*/
  } p, s;
  float R_final;                   /*- エネルギー反射率の最終版 -*/
  float phi;                       /*- 位相 -*/
  float color_x, color_y, color_z; /*- ｘｙｚ表色系 -*/

  float temp_rgb_f[3];

  /*- パラメータを得る -*/
  float intensity       = (float)m_intensity->getValue(frame);
  float refractiveIndex = (float)m_refractiveIndex->getValue(frame);
  float thickMax        = (float)m_thickMax->getValue(frame);
  float thickMin        = (float)m_thickMin->getValue(frame);
  float rgbGamma[3]     = {(float)m_RGamma->getValue(frame),
                       (float)m_GGamma->getValue(frame),
                       (float)m_BGamma->getValue(frame)};
  float lensFactor = (float)m_lensFactor->getValue(frame);

  /*- 入射角は０で固定 -*/

  /*- 各境界での振幅反射率、振幅透過率の計算(PS偏光とも) -*/
  /*- P偏光 -*/
  p.r_ab = (1.0 - refractiveIndex) / (1.0 + refractiveIndex);
  p.t_ab = (1.0f - p.r_ab) / refractiveIndex;
  p.r_ba = -p.r_ab;
  p.t_ba = (1.0f + p.r_ab) * refractiveIndex;
  /*- S偏光 -*/
  s.r_ab = (1.0 - refractiveIndex) / (1.0 + refractiveIndex);
  s.t_ab = 1.0f + s.r_ab;
  s.r_ba = -s.r_ab;
  s.t_ba = 1.0f - s.r_ab;

  for (j = 0; j < 256; j++) { /*- 膜厚d -*/
    /*- 膜厚d(μm)の計算 -*/
    d = thickMin +
        (thickMax - thickMin) * powf(((float)j / 255.0f), lensFactor);

    /*-  膜厚が負になることもありうる。その場合は d = 0 に合わせる -*/
    if (d < 0.0f) d = 0.0f;

    /*- これから積算するので、XYZ表色系各チャンネルの初期化 -*/
    color_x = 0.0f;
    color_y = 0.0f;
    color_z = 0.0f;

    for (ram = 0; ram < 34; ram++) { /*- 波長λ（380nm-710nm） -*/
      /*- 波長λ(μm)の計算 -*/
      rambda = 0.38f + 0.01f * (float)ram;
      /*- 位相の計算 -*/
      phi = 4.0f * PI * refractiveIndex * d / rambda;
      /*- 薄膜の振幅反射率の計算（PS偏光とも） -*/
      /*- P偏光 -*/
      p.r_real = p.r_ab + p.t_ab * p.r_ba * p.t_ba * cosf(phi);
      p.r_img  = p.t_ab * p.r_ba * p.t_ba * sinf(phi);
      /*- S偏光 -*/
      s.r_real = s.r_ab + s.t_ab * s.r_ba * s.t_ba * cosf(phi);
      s.r_img  = s.t_ab * s.r_ba * s.t_ba * sinf(phi);

      p.R = p.r_real * p.r_real + p.r_img * p.r_img;
      s.R = s.r_real * s.r_real + s.r_img * s.r_img;

      /*- エネルギー反射率 -*/
      R_final = (p.R + s.R) / 2.0f;

      color_x += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 0];
      color_y += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 1];
      color_z += intensity * cie_d65[ram] * R_final * xyz[ram * 3 + 2];

    } /*- 次のramへ(波長λ) -*/

    temp_rgb_f[0] =
        3.240479f * color_x - 1.537150f * color_y - 0.498535f * color_z;
    temp_rgb_f[1] =
        -0.969256f * color_x + 1.875992f * color_y + 0.041556f * color_z;
    temp_rgb_f[2] =
        0.055648f * color_x - 0.204043f * color_y + 1.057311f * color_z;

    /*- オーバーフローをまるめる -*/
    for (k = 0; k < 3; k++) {
      if (temp_rgb_f[k] < 0.0f) temp_rgb_f[k] = 0.0f;

      /*- ガンマ処理 -*/
      temp_rgb_f[k] = powf((temp_rgb_f[k] / 255.0f), rgbGamma[k]);

      if (temp_rgb_f[k] >= 1.0f) temp_rgb_f[k] = 1.0f;
    }
    bubbleColor[j].x = temp_rgb_f[0];
    bubbleColor[j].y = temp_rgb_f[1];
    bubbleColor[j].z = temp_rgb_f[2];

  } /*- 次のjへ(膜厚d) -*/
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
    , m_lightIntensity(1.0) {
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

  m_intensity->setValueRange(0.0, 8.0);
  m_refractiveIndex->setValueRange(1.0, 3.0);
  m_thickMax->setValueRange(-1.5, 2.0);
  m_thickMin->setValueRange(-1.5, 2.0);
  m_RGamma->setValueRange(0.001, 1.0);
  m_GGamma->setValueRange(0.001, 1.0);
  m_BGamma->setValueRange(0.001, 1.0);
  m_lensFactor->setValueRange(0.01, 10.0);
  m_lightThres->setValueRange(-5.0, 1.0);
  m_lightIntensity->setValueRange(0.0, 1.0);
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
  calcBubbleMap(bubbleColor, frame);

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
    }
  }

  //メモリ解放
  // brightness_ras->unlock();
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
