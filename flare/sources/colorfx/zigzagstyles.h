#pragma once

#ifndef ZIGZAG_STROKE_STYLE_H
#define ZIGZAG_STROKE_STYLE_H

#include "tpixel.h"

#ifdef POICIPENSO

class TZigzagStrokeStyle final : public TStrokeStyle {
  TPixel32 m_color;
  double m_density;

public:
  TZigzagStrokeStyle(const TPixel32 &color);
  TZigzagStrokeStyle();

  void invalidate() {}

  void changeParameter(double delta);
  TStrokeStyle *clone() const;

  void draw(double pixelSize, const TColorFunction * = 0);

  void loadData(TInputStreamInterface &is) { is >> m_color >> m_density; }
  void saveData(TOutputStreamInterface &os) const {
    os << m_color << m_density;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const { return 115; };
  bool operator==(const TStrokeStyle &style) const {
    if (getTagId() != style.getTagId()) return false;
    return m_color == ((TZigzagStrokeStyle &)style).m_color &&
           m_density == ((TZigzagStrokeStyle &)style).m_density;
  }
};

class TImageBasedZigzagStrokeStyle final : public TStrokeStyle {
  TPixel32 m_color;
  double m_textScale;
  TRaster32P m_texture;

public:
  TImageBasedZigzagStrokeStyle(const TPixel32 &color);
  TImageBasedZigzagStrokeStyle();
  TImageBasedZigzagStrokeStyle(const TRaster32P texture);

  void invalidate() {}
  inline int realTextCoord(const int a, const int l) const;

  void changeParameter(double delta);
  TStrokeStyle *clone() const;

  void draw(double pixelSize, const TColorFunction * = 0);

  void loadData(TInputStreamInterface &is) { is >> m_color >> m_textScale; }
  void saveData(TOutputStreamInterface &os) const {
    os << m_color << m_textScale;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const { return 116; };
  bool operator==(const TStrokeStyle &style) const {
    if (getTagId() != style.getTagId()) return false;
    return m_color == ((TImageBasedZigzagStrokeStyle &)style).m_color &&
           m_textScale == ((TImageBasedZigzagStrokeStyle &)style).m_textScale;
  }
};

#endif

#endif
