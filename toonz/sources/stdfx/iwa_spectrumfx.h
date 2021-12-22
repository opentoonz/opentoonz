#pragma once

/*------------------------------------
 Iwa_SpectrumFx
 参照画像を位相差として、干渉色を出力する
------------------------------------*/

#ifndef IWA_SPECTRUM_H
#define IWA_SPECTRUM_H

#include "stdfx.h"
#include "tfxparam.h"

struct float3 {
  float x, y, z;
  float3 operator*(const float &a) { return {x * a, y * a, z * a}; }
  float3 operator+(const float3 &a) { return {x + a.x, y + a.y, z + a.z}; }
};
struct float4 {
  float x, y, z, w;
};

class Iwa_SpectrumFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_SpectrumFx)

protected:
  TRasterFxPort m_input; /*- 位相差マップの入力 -*/
  TRasterFxPort m_light; /*- 光源用マップの入力 -*/

  TDoubleParamP m_intensity;
  TDoubleParamP m_refractiveIndex;
  TDoubleParamP m_thickMax;
  TDoubleParamP m_thickMin;
  TDoubleParamP m_RGamma;
  TDoubleParamP m_GGamma;
  TDoubleParamP m_BGamma;

  TDoubleParamP m_loopSpectrumFadeWidth;
  TDoubleParamP m_spectrumShift;

  TDoubleParamP m_lensFactor;
  TDoubleParamP m_lightThres;
  TDoubleParamP m_lightIntensity;

  /*- シャボン色マップの生成 -*/
  void calcBubbleMap(float3 *bubbleColor, double frame,
                     bool computeAngularAxis = false);

  template <typename RASTER, typename PIXEL>
  void convertRaster(const RASTER ras, TDimensionI dim, float3 *bubbleColor);
  template <typename RASTER, typename PIXEL>
  void convertRasterWithLight(const RASTER ras, TDimensionI dim,
                              float3 *bubbleColor, const RASTER lightRas,
                              float lightThres, float lightIntensity);

  /*- 素材タイルを０〜１に正規化して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setSourceRasters(const RASTER ras, float4 *in_out_tile_host,
                        const RASTER light_ras, float4 *light_host,
                        TDimensionI dim, bool useLight);

  /*- 出力結果をChannel値に変換してタイルに格納 -*/
  template <typename RASTER, typename PIXEL>
  void outputRasters(const RASTER outRas, float4 *in_out_tile_host,
                     TDimensionI dim);

public:
  Iwa_SpectrumFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  void doCompute_CUDA(TTile &tile, double frame,
                      const TRenderSettings &settings);

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;
};

#endif
