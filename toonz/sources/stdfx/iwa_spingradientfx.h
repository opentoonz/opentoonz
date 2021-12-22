#pragma once
#ifndef IWA_SPINGRADIENTFX_H
#define IWA_SPINGRADIENTFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

class Iwa_SpinGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_SpinGradientFx)

  TIntEnumParamP m_curveType;
  TPointParamP m_center;
  TDoubleParamP m_startAngle, m_endAngle;
  TPixelParamP m_startColor, m_endColor;

public:
  Iwa_SpinGradientFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif