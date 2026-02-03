#pragma once

/*------------------------------------
 Iwa_MotionBlurCompFx
 露光量／オブジェクトの軌跡を考慮したモーションブラー
 背景との露光値合成を可能にする
//------------------------------------*/

#ifndef IWA_MOTION_BLUR_COMP_H
#define IWA_MOTION_BLUR_COMP_H

#include "tfxparam.h"
#include "stdfx.h"
#include "iwa_bokeh_util.h"  // ExposureConverter

#include "motionawarebasefx.h"

class TStageObjectId;

struct float2 {
  float x, y;
};
struct float3 {
  float x, y, z;
};
struct float4 {
  float x, y, z, w;
};
// struct int2 {
//   int x, y;
// };

/*- m_premultiTypeの取る値 -*/
enum PremultiTypes {
  AUTO = 0,
  SOURCE_IS_PREMULTIPLIED,
  SOURCE_IS_NOT_PREMUTIPLIED
};

class Iwa_MotionBlurCompFx final : public MotionAwareBaseFx {
  FX_PLUGIN_DECLARATION(Iwa_MotionBlurCompFx)

protected:
  TRasterFxPort m_input;
  TRasterFxPort m_background;

  TDoubleParamP m_hardness;     // gamma (version 1)
  TDoubleParamP m_gamma;        // gamma (version 2)
  TDoubleParamP m_gammaAdjust;  // Gamma offset from the current color space
                                // gamma (version 3)

  /*-- 左右をぼかすためのパラメータ --*/
  TDoubleParamP m_startValue; /*- シャッター開け時のフィルタ値 -*/
  /*- シャッター開け時〜現時点までのフィルタ値のガンマ補正値 -*/
  TDoubleParamP m_startCurve;
  TDoubleParamP m_endValue; /*- シャッター閉じ時のフィルタ値 -*/
  /*- 現時点〜シャッター閉じ時までのフィルタ値のガンマ補正値 -*/
  TDoubleParamP m_endCurve;

  /*- 残像モードのトグル -*/
  TBoolParamP m_zanzoMode;

  TIntEnumParamP m_premultiType;

  /*- ソース画像を０〜１に正規化してホストメモリに読み込む
          ソース画像がPremultipyされているか、コンボボックスで指定されていない場合は
          ここで判定する -*/
  template <typename RASTER, typename PIXEL>
  bool setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim,
                       PremultiTypes type = AUTO);
  /*- 出力結果をChannel値に変換して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int2 margin);

  /*-  フィルタをつくり、正規化する -*/
  void makeMotionBlurFilter_CPU(float *filter_p, TDimensionI &filterDim,
                                int marginLeft, int marginBottom,
                                float4 *pointsTable, int pointAmount,
                                float startValue, float startCurve,
                                float endValue, float endCurve);

  /*- 残像フィルタをつくり、正規化する -*/
  void makeZanzoFilter_CPU(float *filter_p, TDimensionI &filterDim,
                           int marginLeft, int marginBottom,
                           float4 *pointsTable, int pointAmount,
                           float startValue, float startCurve, float endValue,
                           float endCurve);

  /*- RGB値(０〜１)を露光値に変換 -*/
  void convertRGBtoExposure_CPU(float4 *in_tile_p, TDimensionI &dim,
                                const ExposureConverter &conv,
                                bool sourceIsPremultiplied);

  /*- 露光値をフィルタリングしてぼかす -*/
  void applyBlurFilter_CPU(float4 *in_tile_p, float4 *out_tile_p,
                           TDimensionI &dim, float *filter_p,
                           TDimensionI &filterDim, int marginLeft,
                           int marginBottom, int marginRight, int marginTop,
                           TDimensionI &outDim);

  /*- 露光値をdepremultipy→RGB値(０〜１)に戻す→premultiply -*/
  void convertExposureToRGB_CPU(float4 *out_tile_p, TDimensionI &dim,
                                const ExposureConverter &conv);

  /*- 背景があり、前景が動かない場合、単純にOverする -*/
  void composeWithNoMotion(TTile &tile, double frame,
                           const TRenderSettings &settings);

  /*- 背景を露光値にして通常合成 -*/
  void composeBackgroundExposure_CPU(float4 *out_tile_p,
                                     TDimensionI &enlargedDimIn,
                                     int marginRight, int marginTop,
                                     TTile &back_tile, TDimensionI &dimOut,
                                     const ExposureConverter &conv);

public:
  Iwa_MotionBlurCompFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  void doCompute_CPU(TTile &tile, double frame, const TRenderSettings &settings,
                     float4 *pointsTable, int pointAmount, double gamma,
                     double shutterStart, double shutterEnd,
                     int traceResolution, float startValue, float startCurve,
                     float endValue, float endCurve, int marginLeft,
                     int marginRight, int marginTop, int marginBottom,
                     TDimensionI &enlargedDimIn, TTile &enlarge_tile,
                     TDimensionI &dimOut, TDimensionI &filterDim,
                     TTile &back_tile);

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;
  /*- 参考にしているオブジェクトが動いている可能性があるので、
          エイリアスは毎フレーム変える -*/
  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
  void onFxVersionSet() final override;

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override;
};

#endif