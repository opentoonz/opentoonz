#pragma once

#ifndef MYPAINT_HPP
#define MYPAINT_HPP

#include <cmath>
#include <algorithm>
#include <string>

extern "C" {
  #include <mypaint-brush.h>
  #include <mypaint-surface.h>
  #include <mypaint-brush-settings.h>
}

namespace mypaint {
  class Brush;

  //=======================================================
  //
  // Dab
  //
  //=======================================================

  class Dab {
  public:
    float x;
    float y;
    float radius;
    float colorR;
    float colorG;
    float colorB;
    float opaque;
    float hardness;
    float alphaEraser;
    float aspectRatio;
    float angle;
    float lockAlpha;
    float colorize;

    Dab():
      x(), y(), radius(),
      colorR(), colorG(), colorB(),
      opaque(), hardness(), alphaEraser(),
      aspectRatio(), angle(),
      lockAlpha(), colorize()
      { }

    Dab(
      float x, float y, float radius,
      float colorR = 0.f, float colorG = 0.f, float colorB = 0.f,
      float opaque = 1.f, float hardness = 0.5f, float alphaEraser = 1.f,
      float aspectRatio = 1.f, float angle = 0.f,
      float lockAlpha = 0.f, float colorize = 0.f
    ):
      x(x), y(y), radius(radius),
      colorR(colorR), colorG(colorG), colorB(colorB),
      opaque(opaque), hardness(hardness), alphaEraser(alphaEraser),
      aspectRatio(aspectRatio), angle(angle),
      lockAlpha(lockAlpha), colorize(lockAlpha)
      { }

    void clamp() {
      radius      = fabsf(radius);
      colorR      = std::min(std::max(colorR,      0.f), 1.f);
      colorG      = std::min(std::max(colorG,      0.f), 1.f);
      colorB      = std::min(std::max(colorB,      0.f), 1.f);
      opaque      = std::min(std::max(opaque,      0.f), 1.f);
      hardness    = std::min(std::max(hardness,    0.f), 1.f);
      alphaEraser = std::min(std::max(alphaEraser, 0.f), 1.f);
      aspectRatio = std::max(aspectRatio, 1.f);
      lockAlpha   = std::min(std::max(lockAlpha,   0.f), 1.f);
      colorize    = std::min(std::max(colorize,    0.f), 1.f);
    }

    Dab getClamped() const
      { Dab dab(*this); dab.clamp(); return dab; }
  };


  //=======================================================
  //
  // Surface
  //
  //=======================================================

  class Surface {
    friend class Brush;

    struct InternalSurface: public MyPaintSurface {
      Surface *m_owner;
    };

    InternalSurface m_surface;

    static int internalDrawDab(
        MyPaintSurface *self,
        float x, float y, float radius,
        float colorR, float colorG, float colorB,
        float opaque, float hardness, float alphaEraser,
        float aspectRatio, float angle,
        float lockAlpha, float colorize )
    {
      return static_cast<InternalSurface*>(self)->m_owner->drawDab( Dab(
        x, y, radius,
        colorR, colorG, colorB,
        opaque, hardness, alphaEraser,
        aspectRatio, angle,
        lockAlpha, colorize ));
    }

    static void internalGetColor(
        MyPaintSurface *self,
        float x, float y, float radius,
        float *colorR, float *colorG, float *colorB, float *colorA )
    {
      static_cast<InternalSurface*>(self)->m_owner->getColor(
        x, y,
        radius,
        *colorR, *colorG, *colorB, *colorA);
    }

  public:
    Surface():
      m_surface()
    {
      m_surface.m_owner = this;
      m_surface.draw_dab = internalDrawDab;
      m_surface.get_color = internalGetColor;
    }

    virtual ~Surface() { }

    virtual bool getColor(
        float x, float y, float radius,
        float &colorR, float &colorG, float &colorB, float &colorA ) = 0;

    virtual bool drawDab(const Dab &dab) = 0;
  };

  //=======================================================
  //
  // Brush
  //
  //=======================================================

  class Brush {
    MyPaintBrush *m_brush;

  public:
    Brush():
      m_brush(mypaint_brush_new())
    { fromDefaults(); }

    Brush(const Brush &other):
      m_brush(mypaint_brush_new())
    { fromBrush(other); }

    ~Brush()
    { mypaint_brush_unref(m_brush); }

    Brush& operator= (const Brush &other) {
      fromBrush(other);
      return *this;
    }

    void reset()
    { mypaint_brush_reset(m_brush); }

    void newStroke()
    { mypaint_brush_new_stroke(m_brush); }

    int strokeTo(Surface &surface, float x, float y,
                  float pressure, float xtilt, float ytilt, double dtime)
    {
      return mypaint_brush_stroke_to(m_brush, &surface.m_surface,
                                     x, y, pressure, xtilt, ytilt, dtime);
    }

    void setBaseValue(MyPaintBrushSetting id, float value)
    { mypaint_brush_set_base_value(m_brush, id, value); }

    float getBaseValue(MyPaintBrushSetting id) const
    { return mypaint_brush_get_base_value(m_brush, id); }

    bool isConstant(MyPaintBrushSetting id) const
    { return mypaint_brush_is_constant(m_brush, id); }

    int getInputsUsedN(MyPaintBrushSetting id) const
    { return mypaint_brush_get_inputs_used_n(m_brush, id); }

    void setMappingN(MyPaintBrushSetting id, MyPaintBrushInput input, int n)
    { mypaint_brush_set_mapping_n(m_brush, id, input, n); }

    int getMappingN(MyPaintBrushSetting id, MyPaintBrushInput input) const
    { return mypaint_brush_get_mapping_n(m_brush, id, input); }

    void setMappingPoint(MyPaintBrushSetting id, MyPaintBrushInput input, int index, float x, float y)
    { mypaint_brush_set_mapping_point(m_brush, id, input, index, x, y); }

    void getMappingPoint(MyPaintBrushSetting id, MyPaintBrushInput input, int index, float &x, float &y) const
    { mypaint_brush_get_mapping_point(m_brush, id, input, index, &x, &y); }

    float getState(MyPaintBrushState i) const
    { return mypaint_brush_get_state(m_brush, i); }

    void setState(MyPaintBrushState i, float value)
    { return mypaint_brush_set_state(m_brush, i, value); }

    double getTotalStrokePaintingTime() const
    { return mypaint_brush_get_total_stroke_painting_time(m_brush); }

    void setPrintInputs(bool enabled)
    { mypaint_brush_set_print_inputs(m_brush, enabled); }

    void fromDefaults() {
      mypaint_brush_from_defaults(m_brush);
    }

    void fromBrush(const Brush &other) {
      for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
        MyPaintBrushSetting id = (MyPaintBrushSetting)i;
        setBaseValue(id, other.getBaseValue(id));
        for(int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j) {
          MyPaintBrushInput input = (MyPaintBrushInput)j;
          int n = other.getMappingN(id, input);
          setMappingN(id, input, n);
          for(int index = 0; index < n; ++index) {
            float x = 0.f, y = 0.f;
            other.getMappingPoint(id, input, index, x, y);
            setMappingPoint(id, input, index, x, y);
          }
        }
      }
    }

    bool fromString(const std::string &s) {
      return mypaint_brush_from_string(m_brush, s.c_str());
    }
  };

  //=======================================================
  //
  // Setting
  //
  //=======================================================

  class Setting final {
  public:
    MyPaintBrushSetting id;
    std::string key;
    std::string name;
    std::string tooltip;
    bool constant;
    float min;
    float def;
    float max;

  private:
    Setting():               id(), constant(), min(), def(), max() { }
    Setting(const Setting&): id(), constant(), min(), def(), max() { }

  public:
    static const Setting* all() {
      static bool initialized = false;
      static Setting settings[MYPAINT_BRUSH_SETTINGS_COUNT];
      if (!initialized) {
        for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
          const MyPaintBrushSettingInfo *info = mypaint_brush_setting_info((MyPaintBrushSetting)i);
          settings[i].id       = (MyPaintBrushSetting)i;
          settings[i].key      = info->cname;
          settings[i].name     = mypaint_brush_setting_info_get_name(info);
          settings[i].tooltip  = mypaint_brush_setting_info_get_tooltip(info);
          settings[i].constant = info->constant;
          settings[i].min      = info->min;
          settings[i].def      = info->def;
          settings[i].max      = info->max;
        }
      }
      return settings;
    }

    static const Setting& byId(MyPaintBrushSetting id)
      { return all()[id]; }

    static const Setting* findByKey(const std::string &key) {
      const Setting* settings = all();
      for(int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i)
        if (settings[i].key == key)
          return &settings[i];
      return 0;
    }
  };

  //=======================================================
  //
  // Input
  //
  //=======================================================

  class Input final {
  public:
    MyPaintBrushInput id;
    std::string key;
    std::string name;
    std::string tooltip;
    float hardMin;
    float softMin;
    float normal;
    float softMax;
    float hardMax;

  private:
    Input():             id(), hardMin(), softMin(), normal(), softMax(), hardMax() { }
    Input(const Input&): id(), hardMin(), softMin(), normal(), softMax(), hardMax() { }

  public:
    static const Input* all() {
      static bool initialized = false;
      static Input inputs[MYPAINT_BRUSH_INPUTS_COUNT];
      if (!initialized) {
        for(int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i) {
          const MyPaintBrushInputInfo *info = mypaint_brush_input_info((MyPaintBrushInput)i);
          inputs[i].id       = (MyPaintBrushInput)i;
          inputs[i].key      = info->cname;
          inputs[i].name     = mypaint_brush_input_info_get_name(info);
          inputs[i].tooltip  = mypaint_brush_input_info_get_tooltip(info);
          inputs[i].hardMin = info->hard_min;
          inputs[i].softMin = info->soft_min;
          inputs[i].normal  = info->normal;
          inputs[i].softMax = info->soft_max;
          inputs[i].hardMax = info->hard_max;
        }
      }
      return inputs;
    }

    static const Input& byId(MyPaintBrushInput id) {
      return all()[id];
    }

    static const Input* findByKey(const std::string &key) {
      const Input* inputs = all();
      for(int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
        if (inputs[i].key == key)
          return &inputs[i];
      return 0;
    }
  };
}

#endif  // MYPAINT_HPP
