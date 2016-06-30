#pragma once

/*------------------------------------
 Iwa_PerspectiveDistortFx
 奥行き方向に台形歪みを行うエフェクト
 ディティールを保持するため、引き伸ばす量に応じて素材の解像度を上げる
------------------------------------*/

#ifndef IWA_PERSPECTIVE_DISTORT_H
#define IWA_PERSPECTIVE_DISTORT_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

struct float4 {
  float x, y, z, w;
};

class Iwa_PerspectiveDistortFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_PerspectiveDistortFx)
protected:
  TRasterFxPort m_source; /*- 入力画像 -*/

  TPointParamP m_vanishingPoint; /*- 消失点 -*/
  TPointParamP m_anchorPoint;    /*- 基準点 -*/

  TDoubleParamP m_precision; /*- 細かさ。解像度の引き上げ度合い -*/

  /*- 出力結果をChannel値に変換して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim,
                       int drawLevel);

  /*- タイルの画像を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim);

public:
  Iwa_PerspectiveDistortFx();

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &rend_sets) override;

  void doCompute_CPU(TTile &tile, const double frame,
                     const TRenderSettings &settings, TPointD &vanishingPoint,
                     TPointD &anchorPoint, float4 *source_host,
                     float4 *result_host, TDimensionI &sourceDim,
                     TDimensionI &resultDim, const double precision,
                     const double offs);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif