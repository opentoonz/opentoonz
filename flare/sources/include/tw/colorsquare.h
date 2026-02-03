#pragma once

#ifndef TNZ_COLORSQUARE_INCLUDED
#define TNZ_COLORSQUARE_INCLUDED

#include "tw/tw.h"
#include "tw/colorslider.h"

//===================================================================
//
// Color square (si comporta come un color slider)
//   setAction(), set/getColorValue(), setType()
//   click&drag --> m_action->notify(col, dragging)
//
//===================================================================

class ColorSquare : public TWidget {
  TRaster32P m_raster;
  TColorSlider::Type m_type;
  TColorSlider::Color m_color;
  TColorSlider::Action *m_action;
  TPoint m_currentPos;
  TPoint m_oldMousePos;

public:
  ColorSquare(TWidget *parent, string name = "colorSquare");

  ~ColorSquare();

  void setAction(TColorSlider::Action *action);
  void setColorValue(const TColorSlider::Color &color);

  TColorSlider::Color getColorValue() const;

  void setType(TColorSlider::Type type);

  void configureNotify(const TDimension &d);

  void update();
  void repaint();

  void pick(const TPoint &p, bool dragging);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);
};

#endif
