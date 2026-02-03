#pragma once

/*------------------------------------------------
  Iwa_AdjustExposureFx
  明るさを露光値で調整するエフェクト
------------------------------------------------*/

#ifndef ADJUST_EXPOSURE_H
#define ADJUST_EXPOSURE_H

#include "stdfx.h"
#include "tfxparam.h"
#include "iwa_bokeh_util.h"  // ExposureConverter

struct float4 {
  float x, y, z, w;
};

class Iwa_AdjustExposureFx final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_AdjustExposureFx)

protected:
  TRasterFxPort m_source;
  TDoubleParamP m_hardness;     // gamma (version 1)
  TDoubleParamP m_gamma;        // gamma (version 2)
  TDoubleParamP m_gammaAdjust;  // Gamma offset from the current color space
                                // gamma (version 3)
  TDoubleParamP m_scale;        /*- 明るさのスケール値 -*/
  TDoubleParamP m_offset;       /*- 明るさのオフセット値 -*/

  /*- タイルの画像を０〜１に正規化してホストメモリに読み込む -*/
  template <typename RASTER, typename PIXEL>
  void setSourceRaster(const RASTER srcRas, float4 *dstMem, TDimensionI dim);
  void setSourceRasterF(const TRasterFP srcRas, float4 *dstMem,
                        TDimensionI dim);
  /*- 出力結果をChannel値に変換して格納 -*/
  template <typename RASTER, typename PIXEL>
  void setOutputRaster(float4 *srcMem, const RASTER dstRas, TDimensionI dim);
  void setOutputRasterF(float4 *srcMem, const TRasterFP dstRas,
                        TDimensionI dim);

public:
  Iwa_AdjustExposureFx();

  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;

  void doCompute_CPU(double frame, TDimensionI &dim, float4 *tile_host,
                     const ExposureConverter &conv);

  void doFloatCompute(const TRasterFP rasF, double frame, TDimensionI &dim,
                      const ExposureConverter &conv);

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  bool canHandle(const TRenderSettings &info, double frame) override;

  void onFxVersionSet() final override;

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override;
};

#endif