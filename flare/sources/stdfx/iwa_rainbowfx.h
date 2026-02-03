#pragma once
#ifndef IWA_CORRIDORGRADIENTFX_H
#define IWA_CORRIDORGRADIENTFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

struct double3 {
  double r, g, b;
};

class Iwa_RainbowFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_RainbowFx)

  TPointParamP m_center;

  TDoubleParamP m_intensity;
  TDoubleParamP m_radius;
  TDoubleParamP m_width_scale;
  TDoubleParamP m_inside;
  TDoubleParamP m_secondary_rainbow;
  TBoolParamP m_alpha_rendering;

  double getSizePixelAmount(const double val, const TAffine affine);
  void buildRainbowColorMap(double3 *core, double3 *wide, double intensity,
                            double inside, double secondary, bool doClamp);
  inline double3 angleToColor(double angle, double3 *core, double3 *wide);

  template <typename RASTER, typename PIXEL>
  void setOutputRaster(const RASTER ras, TDimensionI dim, double3 *outBuf_p);

public:
  Iwa_RainbowFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  bool toBeComputedInLinearColorSpace(bool settingsIsLinear,
                                      bool tileIsLinear) const override;
};

#endif