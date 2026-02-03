#pragma once

#ifndef TNZ_COLORSLIDER_INCLUDED
#define TNZ_COLORSLIDER_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TColorSlider : public TWidget {
public:
  class DVAPI Color : public TPixel32 {
  public:
    int h, s, v;
    Color() : TPixel32(), h(0), s(0), v(0) {}
    Color(const TPixel32 &c) : TPixel32(c), h(0), s(0), v(0) { updateHsv(); }

    Color &operator=(const TPixel32 &c) {
      r = c.r;
      g = c.g;
      b = c.b;
      m = c.m;
      updateHsv();
      return *this;
    }

    void updateRgb();
    void updateHsv();
  };

  class DVAPI Action {
  public:
    Action() {}
    virtual ~Action() {}
    virtual void notify(const Color &color, bool dragging) = 0;
  };

  enum Type {
    RedScale,
    GreenScale,
    BlueScale,
    MatteScale,
    HueScale,
    SaturationScale,
    ValueScale
  };

  TColorSlider(TWidget *parent, Type type, string name = "colorSlider");
  ~TColorSlider();

  void draw();

  void configureNotify(const TDimension &d);

  void setType(Type type);
  Type getType() const;

  int getValue() const;

  void setColorValue(const Color &color);
  Color getColorValue() const;

  void setAction(Action *action);

  void enableLabel(bool on);

  // void onTimer(int v);

  void invalidate();

  void notify(bool dragging);  // chiama la callback

  class Data;

private:
  Data *m_data;
};

template <class T>
class TColorSliderAction : public TColorSlider::Action {
public:
  typedef void (T::*CommandMethod)(const TColorSlider::Color &color,
                                   bool dragging);

  TColorSliderAction(T *target, CommandMethod method)
      : m_target(target), m_method(method){};
  void notify(const TColorSlider::Color &color, bool dragging) {
    (m_target->*m_method)(color, dragging);
  };

private:
  T *m_target;
  CommandMethod m_method;
};

#endif
