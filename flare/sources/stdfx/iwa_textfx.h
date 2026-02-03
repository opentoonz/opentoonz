#pragma once

#ifndef IWA_TEXTFX_H
#define IWA_TEXTFX_H

#include "tparamset.h"
#include "textawarebasefx.h"

//******************************************************************
//	Iwa_Text Fx  class
//******************************************************************

class Iwa_TextFx final : public TextAwareBaseFx {
  FX_PLUGIN_DECLARATION(Iwa_TextFx)
protected:
  TStringParamP m_text;

  TIntEnumParamP m_hAlign;

  TPointParamP m_center;
  TDoubleParamP m_width;
  TDoubleParamP m_height;

  TFontParamP m_font;
  TPixelParamP m_textColor;
  TPixelParamP m_boxColor;
  TBoolParamP m_showBorder;

  template <typename RASTER, typename PIXEL>
  void putTextImage(const RASTER srcRas, TPoint &pos, QImage &img);

public:
  Iwa_TextFx();

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
};
#endif