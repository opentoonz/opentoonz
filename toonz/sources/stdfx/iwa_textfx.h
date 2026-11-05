#pragma once

#ifndef IWA_TEXTFX_H
#define IWA_TEXTFX_H

#include "tparamset.h"
#include "textawarebasefx.h"
#include "iwa_textfxlayout.h"

#include <QFont>
#include <QRectF>
#include <QString>

#include <memory>

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
  // Off (the default): the text is placed relative to the camera and only
  // scales with its column. On: center, box and glyphs are in the column's
  // space and follow its whole transform (position, rotation, scale, shear).
  TBoolParamP m_transformWithColumn;
  // Layout options. At their defaults the text is drawn exactly as before;
  // otherwise it is laid out by Iwa_TextFxLayout::Block (see
  // iwa_textfxlayout.h). Lengths are fx lengths.
  TBoolParamP m_kerning;          // The font's kerning (on by default)
  TDoubleParamP m_letterSpacing;  // Added to every character advance
  TDoubleParamP m_lineSpacing;    // Added to the font's line spacing
  TDoubleParamP m_indent;         // First line of every paragraph
  // On (the default): text larger than the box shrinks until it fits. Off:
  // it keeps its size and a word wider than the box overflows it.
  TBoolParamP m_shrinkToFit;

  template <typename RASTER, typename PIXEL>
  void putTextImage(const RASTER srcRas, TPoint &pos, QImage &img);

  QString sourceText() const;
  // True when a layout option is not at its default.
  bool usesLayoutOptions(double frame) const;
  // The layout options at `scale` layout pixels per fx length unit.
  Iwa_TextFxLayout::Options layoutOptions(double frame, double scale) const;
  // Lays out the text for the column-transformed mode at a fixed resolution
  // (Iwa_TextFxLayout::transformedScale). `font` gets the drawing size;
  // `box` is the text box at the origin and `block` the laid-out text in it;
  // `layout` gets the layout when a layout option is set. Returns false when
  // nothing can be drawn.
  bool transformedLayout(
      double frame, const QString &text, QFont &font, QRectF &box,
      QRectF &block, std::unique_ptr<Iwa_TextFxLayout::Block> &layout) const;
  void computeTransformed(TTile &tile, double frame, const TRenderSettings &ri,
                          const QString &text);
  // The camera-relative mode with a layout option set.
  void computeWithLayoutOptions(TTile &tile, double frame,
                                const TRenderSettings &ri, const QString &text);

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