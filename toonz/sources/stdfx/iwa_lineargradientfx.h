#pragma once
#ifndef IWA_LINEARGRADIENTFX_H
#define IWA_LINEARGRADIENTFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

class Iwa_LinearGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_LinearGradientFx)

  TIntEnumParamP m_curveType;
  TPointParamP m_startPoint, m_endPoint;
  TPixelParamP m_startColor, m_endColor;

  TDoubleParamP m_wave_amplitude;
  TDoubleParamP m_wave_freq;
  TDoubleParamP m_wave_phase;

public:
  Iwa_LinearGradientFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif