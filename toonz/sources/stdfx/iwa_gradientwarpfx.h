#pragma once

/*------------------------------------
 Iwa_GradientWarpFx
 参照画像の勾配方向にWarpするエフェクト
------------------------------------*/

#ifndef IWA_GRADIENT_WARP_H
#define IWA_GRADIENT_WARP_H

#include "tfxparam.h"
#include "stdfx.h"

struct float2 {
  float x, y;
};
struct float4 {
  float x, y, z, w;
};
struct int2 {
  int x, y;
};

class Iwa_GradientWarpFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_GradientWarpFx)

protected:
  TRasterFxPort m_source; /*- 入力画像 -*/
  TRasterFxPort m_warper; /*- 参照画像 -*/

  TDoubleParamP m_h_maxlen;
  TDoubleParamP m_v_maxlen;
  TDoubleParamP
      m_scale; /*- ワープ量を増やすスカラー値。この値はマージン値には影響しない
                  -*/

  void get_render_real_hv(const double frame, const TAffine affine,
                          double &h_maxlen, double &v_maxlen);

  void get_render_enlarge(const double frame, const TAffine affine,
                          TRectD &bBox);

  /*- タイルの画像を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim);

  /*- タイルの画像の輝度値を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setWarperRaster(const RASTER srcRas, float *dstMem, TDimensionI dim);

  /*- 出力結果をChannel値に変換して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int2 margin);

  void doCompute_CPU(TTile &tile, const double frame,
                     const TRenderSettings &settings, double hLength,
                     double vLength, int margin, TDimensionI &enlargedDim,
                     float4 *source_host, float *warper_host,
                     float4 *result_host);

  float4 getSourceVal_CPU(float4 *source_host, TDimensionI &enlargedDim,
                          int pos_x, int pos_y);

  float4 interp_CPU(float4 val1, float4 val2, float ratio);

public:
  Iwa_GradientWarpFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;
};

#endif