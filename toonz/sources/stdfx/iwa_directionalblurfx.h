#pragma once

/*------------------------------------
 Iwa_DirectionalBlurFx
 ボケ足の伸ばし方を選択でき、参照画像を追加した DirectionalBlur
//------------------------------------*/

#ifndef IWA_DIRECTIONAL_BLUR_H
#define IWA_DIRECTIONAL_BLUR_H

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

class Iwa_DirectionalBlurFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_DirectionalBlurFx)

  TRasterFxPort m_input;
  TRasterFxPort m_reference;

  TDoubleParamP m_angle;
  TDoubleParamP m_intensity;
  TBoolParamP m_bidirectional;
  /*- リニア／ガウシアン／平均化 -*/
  TIntEnumParamP m_filterType;

  /*- 参照画像の輝度を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setReferenceRaster(const RASTER srcRas, float *dstMem, TDimensionI dim);

  /*- ソース画像を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim);

  /*- 出力結果をChannel値に変換して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int2 margin);

  void doCompute_CPU(TTile &tile, double frame, const TRenderSettings &settings,
                     TPointD &blur, bool bidirectional, int marginLeft,
                     int marginRight, int marginTop, int marginBottom,
                     TDimensionI &enlargedDimIn, TTile &enlarge_tile,
                     TDimensionI &dimOut, TDimensionI &filterDim,
                     float *reference_host);

  void makeDirectionalBlurFilter_CPU(float *filter, TPointD &blur,
                                     bool bidirectional, int marginLeft,
                                     int marginRight, int marginTop,
                                     int marginBottom, TDimensionI &filterDim);

public:
  Iwa_DirectionalBlurFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif