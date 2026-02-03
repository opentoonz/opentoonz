#pragma once

#ifndef BRIGHTCONTFX_H
#define BRIGHTCONTFX_H

#include "stdfx.h"

class Bright_ContFx : public TStandardRasterFx, public TDoubleParamObserver {
  FX_PLUGIN_DECLARATION(Bright_ContFx)

  TRasterFxPort m_input;
  TDoubleParamP m_bright;
  TDoubleParamP m_contrast;

public:
  Bright_ContFx() : m_bright(127.5), m_contrast(50.0) {
    addParam("brightness", m_bright);
    addParam("contrast", m_contrast);
    m_bright->setValueRange(0, 255);
    m_contrast->setValueRange(0, 100);
    addPort("Source", m_input);
  }

  ~Bright_ContFx(){};

  // TParamObserver method
  void onChange(const TParamChange &change) { notify(TFxParamChange(change)); }

  bool getBBox(double frame, TRectD &bBox, TPixel32 &bgColor) {
    return m_input->getBBox(frame, bBox, bgColor);
  };

  void compute(TTile &tile, double frame, const TRasterFxRenderInfo *);

  TRect getInvalidRect(const TRect &max);
};
#endif
