#pragma once
#ifndef IWA_CORRIDORGRADIENTFX_H
#define IWA_CORRIDORGRADIENTFX_H

#include "tfxparam.h"
#include "stdfx.h"
#include "tparamset.h"

class Iwa_CorridorGradientFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_CorridorGradientFx)

  TIntEnumParamP m_shape;
  TIntEnumParamP m_curveType;
  TPointParamP m_points[2][4];  // [in/out][Qt::Corner]

  TPixelParamP m_innerColor;
  TPixelParamP m_outerColor;

public:
  Iwa_CorridorGradientFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif