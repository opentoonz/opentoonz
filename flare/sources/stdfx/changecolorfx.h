#pragma once

#ifndef CHANGECOLORFX_H
#define CHANGECOLORFX_H

#include "stdfx.h"

class ChangeColorFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(ChangeColorFx)
  TRasterFxPort m_input;
  TPixelParamP m_from_color;
  TPixelParamP m_to_color;
  TDoubleParamP m_range;
  TDoubleParamP m_falloff;

public:
  ChangeColorFx()
      : m_from_color(TPixel32::Red)
      , m_to_color(TPixel32::Blue)
      , m_range(0.0)
      , m_falloff(0.0) {
    addPort("Source", m_input);
    addParam("range", m_range);
    addParam("falloff", m_falloff);
    addParam("from_color", m_from_color);
    addParam("to_color", m_to_color);
    m_range->setValueRange(0, 1);
    m_falloff->setValueRange(0, 1);
  }
  ~ChangeColorFx(){};

  bool getBBox(double frame, TRectD &bBox, TPixel32 &bgColor) {
    return m_input->getBBox(frame, bBox, bgColor);
  };

  void compute(TTile &tile, double frame, const TRasterFxRenderInfo *ri);
  TRect getInvalidRect(const TRect &max);
};
#endif
