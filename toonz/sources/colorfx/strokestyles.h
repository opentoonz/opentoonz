#pragma once

#ifndef STROKESTYLES_H
#define STROKESTYLES_H

// TnzCore includes
#include "tsimplecolorstyles.h"
#include "tvectorimage.h"
#include "tstrokeprop.h"
#include "tgl.h"

class TVectorRendeData;
class TRandom;

#undef DVAPI
#undef DVVAR

#ifdef COLORFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

typedef struct {
  TPointD point;
  double dbl1;
  double dbl2;
} PointAnd2Double;

typedef std::vector<TPointD> Points;

typedef struct {
  float blend;
  Points points;
} BlendAndPoint;

typedef std::vector<std::pair<TPointD, TPixel32>> PointsAndColors;
typedef std::vector<Points> PointMatrix;
typedef std::vector<std::pair<TPointD, double>> PointsAndDoubles;
typedef std::vector<std::pair<GLenum, Points>> DrawmodePointsMatrix;
typedef std::vector<TRectD> RectVector;
typedef std::vector<PointAnd2Double> PointsAnd2Doubles;
typedef std::vector<double> Doubles;
typedef std::vector<BlendAndPoint> BlendAndPoints;
//=============================================================================

template <class T>
class TOptimizedStrokeStyleT : public TColorStyle {
public:
  TOptimizedStrokeStyleT() {}

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;

  TRegionProp *makeRegionProp(const TRegion *region) override {
    assert(false);
    return 0;
  };

  virtual void computeData(T &data, const TStroke *stroke,
                           const TColorFunction *cf) const = 0;
  virtual void drawStroke(const TColorFunction *cf, T &data,
                          const TStroke *stroke) const = 0;
};

//-------------------------------------------------------------------

class TFurStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  double m_cs, m_sn, m_angle, m_length;
  TPixel32 m_color;

public:
  TFurStrokeStyle();

  TColorStyle *clone() const override;

  void invalidate() {}

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TFurStrokeStyle", "Herringbone");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_angle << m_length;
  }

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_angle >> m_length;
    m_cs = cos(m_angle);
    m_sn = sin(m_angle);
  }

  int getTagId() const override { return 103; };
};

//-------------------------------------------------------------------

class TChainStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;

public:
  TChainStrokeStyle(const TPixel32 &color);
  TChainStrokeStyle();

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TChainStrokeStyle", "Chain");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override { is >> m_color; }
  void saveData(TOutputStreamInterface &os) const override { os << m_color; }
  int getTagId() const override { return 104; };
};

//-------------------------------------------------------------------

class TSprayStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_color;
  double m_blend, m_intensity, m_radius;

public:
  TSprayStrokeStyle();

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TSprayStrokeStyle", "Circlets");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_blend >> m_intensity >> m_radius;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_blend << m_intensity << m_radius;
  }
  int getTagId() const override { return 106; };
};

//-------------------------------------------------------------------

class TGraphicPenStrokeStyle final
    : public TOptimizedStrokeStyleT<DrawmodePointsMatrix> {
  TPixel32 m_color;
  double m_intensity;

public:
  TGraphicPenStrokeStyle();

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TGraphicPenStrokeStyle", "Dashes");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void computeData(DrawmodePointsMatrix &data, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, DrawmodePointsMatrix &data,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_intensity;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_intensity;
  }
  int getTagId() const override { return 107; };
};

//-------------------------------------------------------------------

class TDottedLineStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_in, m_line, m_out, m_blank;

public:
  TDottedLineStrokeStyle();

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TDottedLineStrokeStyle", "Vanishing");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_in >> m_line >> m_out >> m_blank;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_in << m_line << m_out << m_blank;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 111; }
};

//-------------------------------------------------------------------

class TRopeStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_bend;

public:
  TRopeStrokeStyle();

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TRopeStrokeStyle", "Rope");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override { is >> m_color >> m_bend; }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_bend;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 108; }
};

//-------------------------------------------------------------------

class TCrystallizeStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_period, m_opacity;

public:
  TCrystallizeStrokeStyle();

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;

  void invalidate() {}

  TColorStyle *clone() const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TCrystallizeStrokeStyle", "Tulle");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_period >> m_opacity;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_period << m_opacity;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 109; }
};

//-------------------------------------------------------------------

class TBraidStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_colors[3];
  double m_period;

public:
  TBraidStrokeStyle();

  TColorStyle *clone() const override;

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TBraidStrokeStyle", "Plait");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_colors[0]; }
  void setMainColor(const TPixel32 &color) override { m_colors[0] = color; }

  int getColorParamCount() const override { return 3; }
  TPixel32 getColorParamValue(int index) const override;
  void setColorParamValue(int index, const TPixel32 &color) override;

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_colors[0] >> m_colors[1] >> m_colors[2] >> m_period;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_colors[0] << m_colors[1] << m_colors[2] << m_period;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 136; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(112);
  }
};

//-------------------------------------------------------------------

class TSketchStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_color;
  double m_density;

public:
  TSketchStrokeStyle();

  TColorStyle *clone() const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TSketchStrokeStyle", "Fuzz");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_density;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_density;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 113; }
};

//-------------------------------------------------------------------

class TBubbleStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_color0, m_color1;

public:
  TBubbleStrokeStyle();

  TColorStyle *clone() const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TBubbleStrokeStyle", "Bubbles");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color0; }
  void setMainColor(const TPixel32 &color) override { m_color0 = color; }

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override {
    return index == 0 ? m_color0 : m_color1;
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    if (index == 0)
      m_color0 = color;
    else
      m_color1 = color;
  }

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color0 >> m_color1;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color0 << m_color1;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 114; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(137);
  }
};

//-------------------------------------------------------------------

class TTissueStrokeStyle final : public TOptimizedStrokeStyleT<PointMatrix> {
  TPixel32 m_color;
  double m_density, m_border;

public:
  TTissueStrokeStyle();

  TColorStyle *clone() const override;

  void computeData(PointMatrix &data, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, PointMatrix &data,
                  const TStroke *stroke) const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TTissueStrokeStyle", "Gauze");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_density >> m_border;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_density << m_border;
  }

  int getTagId() const override { return 117; }
};

//-------------------------------------------------------------------

class TBiColorStrokeStyle final : public TOutlineStyle {
  TPixel32 m_color0, m_color1;
  double m_parameter;

public:
  TBiColorStrokeStyle();

  TColorStyle *clone() const override;

  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;
  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TBiColorStrokeStyle", "Shade");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color0; }
  void setMainColor(const TPixel32 &color) override { m_color0 = color; }

  void loadData(TInputStreamInterface &is) override;
  void loadData(int oldId, TInputStreamInterface &) override;

  void saveData(TOutputStreamInterface &os) const override;

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override {
    return index == 0 ? m_color0 : m_color1;
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    if (index == 0)
      m_color0 = color;
    else
      m_color1 = color;
  }

  int getTagId() const override { return 135; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(115);
    ids.push_back(119);
  }
};

//-------------------------------------------------------------------

class TNormal2StrokeStyle final : public TOutlineStyle {
  TPixel32 m_color;
  double m_lightx, m_lighty, m_shininess, m_metal, m_bend;

public:
  TNormal2StrokeStyle();

  TColorStyle *clone() const override;

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const override;
  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TNormal2StrokeStyle", "Bump");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_lightx >> m_lighty >> m_shininess >> m_metal >> m_bend;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_lightx << m_lighty << m_shininess << m_metal << m_bend;
  }

  int getTagId() const override { return 120; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(121);
  }
};

//-------------------------------------------------------------------

class TChalkStrokeStyle2 final : public TOptimizedStrokeStyleT<Doubles> {
  TPixel32 m_color;
  double m_blend, m_intensity, m_in, m_out, m_noise;

public:
  TChalkStrokeStyle2();

  TColorStyle *clone() const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TChalkStrokeStyle2", "Chalk");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void computeData(Doubles &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Doubles &positions,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_blend >> m_intensity >> m_in >> m_out >> m_noise;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_blend << m_intensity << m_in << m_out << m_noise;
  }
  int getTagId() const override { return 123; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(105);
  }
};

//-------------------------------------------------------------------

class TBlendStrokeStyle2 final
    : public TOptimizedStrokeStyleT<PointsAndDoubles> {
  TPixel32 m_color;
  double m_blend, m_in, m_out;

public:
  TBlendStrokeStyle2();

  TColorStyle *clone() const override;

  void computeData(PointsAndDoubles &data, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, PointsAndDoubles &data,
                  const TStroke *stroke) const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TBlendStrokeStyle2", "Fade");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_blend >> m_in >> m_out;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_blend << m_in << m_out;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 125; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(110);
  }
};

//-------------------------------------------------------------------

class TTwirlStrokeStyle final : public TOptimizedStrokeStyleT<Doubles> {
  TPixel32 m_color;
  double m_period, m_blend;

public:
  TTwirlStrokeStyle();

  TColorStyle *clone() const override;

  void computeData(Doubles &data, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Doubles &data,
                  const TStroke *stroke) const override;
  // void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TTwirlStrokeStyle", "Ribbon");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_period >> m_blend;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_period << m_blend;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 126; }
};

//-------------------------------------------------------------------

class TSawToothStrokeStyle final : public TOutlineStyle {
  TPixel32 m_color;
  double m_parameter;

public:
  TSawToothStrokeStyle(TPixel32 color   = TPixel32::Blue,
                       double parameter = 20.0);

  TColorStyle *clone() const override;

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const override {}
  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void invalidate() {}

  void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                      TOutlineUtil::OutlineParameter param) const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TSawToothStrokeStyle", "Jagged");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  int getTagId() const override { return 127; };
  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_parameter;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_parameter;
  }
};

//-------------------------------------------------------------------

class TMultiLineStrokeStyle2 final
    : public TOptimizedStrokeStyleT<BlendAndPoints> {
  TPixel32 m_color0, m_color1;
  double m_intensity, m_length, m_thick, m_noise;

public:
  TMultiLineStrokeStyle2();

  TColorStyle *clone() const override;

  void computeData(BlendAndPoints &data, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, BlendAndPoints &data,
                  const TStroke *stroke) const override;
  // void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TMultiLineStrokeStyle2", "Gouache");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color0; }
  void setMainColor(const TPixel32 &color) override { m_color0 = color; }

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override {
    return index == 0 ? m_color0 : m_color1;
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    if (index == 0)
      m_color0 = color;
    else
      m_color1 = color;
  }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color0 >> m_color1 >> m_intensity >> m_length >> m_thick >> m_noise;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color0 << m_color1 << m_intensity << m_length << m_thick << m_noise;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 138; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(118);
    ids.push_back(128);
  }
};

//-------------------------------------------------------------------

class TZigzagStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_minDist, m_maxDist;
  double m_minAngle, m_maxAngle;
  double m_thickness;

  // void drawBLines(RectVector& rects) const;
  void setRealMinMax() const;
  bool getZigZagPosition(const TStroke *stroke, TRandom &rnd, const double s,
                         const int first, const double minTranslLength,
                         TThickPoint &pos, TThickPoint &pos1) const;

public:
  TZigzagStrokeStyle();

  TColorStyle *clone() const override;

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;
  // void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TZigzagStrokeStyle", "Zigzag");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_minDist >> m_maxDist >> m_minAngle >> m_maxAngle >>
        m_thickness;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_minDist << m_maxDist << m_minAngle << m_maxAngle
       << m_thickness;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 129; }
};

//-------------------------------------------------------------------

class TSinStrokeStyle final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_frequency, m_thick;

public:
  TSinStrokeStyle();

  TColorStyle *clone() const override;

  void computeData(Points &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, Points &positions,
                  const TStroke *stroke) const override;
  // void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TSinStrokeStyle", "Wave");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_frequency >> m_thick;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_frequency << m_thick;
  }

  int getTagId() const override { return 130; }
};

//-------------------------------------------------------------------

class TFriezeStrokeStyle2 final : public TOptimizedStrokeStyleT<Points> {
  TPixel32 m_color;
  double m_parameter, m_thick;

public:
  TFriezeStrokeStyle2();

  TColorStyle *clone() const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("TFriezeStrokeStyle2", "Curl");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  void computeData(std::vector<TPointD> &positions, const TStroke *stroke,
                   const TColorFunction *cf) const override;
  void drawStroke(const TColorFunction *cf, std::vector<TPointD> &positions,
                  const TStroke *stroke) const override;
  // void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;

  void loadData(TInputStreamInterface &is) override {
    is >> m_color >> m_parameter >> m_thick;
  }
  void loadData(int oldId, TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color << m_parameter << m_thick;
  }
  int getTagId() const override { return 133; };
  void getObsoleteTagIds(std::vector<int> &ids) const override {
    ids.push_back(102);
  }
};

//-------------------------------------------------------------------

class TDualColorStrokeStyle2 final : public TOutlineStyle {
  TPixel32 m_color0, m_color1;
  double m_parameter;

public:
  TDualColorStrokeStyle2(TPixel32 color0  = TPixel32::Blue,
                         TPixel32 color1  = TPixel32::Yellow,
                         double parameter = 20.0);

  TColorStyle *clone() const override;

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const override {}
  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void invalidate() {}

  void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                      TOutlineUtil::OutlineParameter param) const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TDualColorStrokeStyle2", "Striped");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color0; }
  void setMainColor(const TPixel32 &color) override { m_color0 = color; }

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override {
    return (index == 0) ? m_color0 : m_color1;
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    if (index == 0)
      m_color0 = color;
    else
      m_color1 = color;
  }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  int getTagId() const override { return 132; };
  void loadData(TInputStreamInterface &is) override {
    is >> m_color0 >> m_color1 >> m_parameter;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color0 << m_color1 << m_parameter;
  }
};

//-------------------------------------------------------------------

class TLongBlendStrokeStyle2 final : public TOutlineStyle {
  TPixel32 m_color0, m_color1;
  double m_parameter;

public:
  TLongBlendStrokeStyle2(TPixel32 color0  = TPixel32::Blue,
                         TPixel32 color1  = TPixel32::Transparent,
                         double parameter = 20.0);

  TColorStyle *clone() const override;

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const override {}
  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void invalidate() {}

  void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                      TOutlineUtil::OutlineParameter param) const override;

  QString getDescription() const override {
    return QCoreApplication::translate("TLongBlendStrokeStyle2", "Watercolor");
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color0; }
  void setMainColor(const TPixel32 &color0) override { m_color0 = color0; }

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override {
    return index == 0 ? m_color0 : m_color1;
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    if (index == 0)
      m_color0 = color;
    else
      m_color1 = color;
  }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  int getTagId() const override { return 134; };
  void loadData(TInputStreamInterface &is) override {
    is >> m_color0 >> m_color1 >> m_parameter;
  }
  void saveData(TOutputStreamInterface &os) const override {
    os << m_color0 << m_color1 << m_parameter;
  }
};

//-------------------------------------------------------------------

#ifdef _DEBUG

class OutlineViewerStyle final : public TSolidColorStyle {
  double m_parameter[4];

  bool m_boolPar;
  int m_intPar;
  int m_enumPar;
  TFilePath m_pathPar;

protected:
  void loadData(TInputStreamInterface &is) override;
  void saveData(TOutputStreamInterface &os) const override;

public:
  OutlineViewerStyle(TPixel32 color = TPixel32::Black, double parameter0 = 0.0,
                     double parameter1 = 0.0, double parameter2 = 2.0,
                     double parameter3 = 3.0);

  TColorStyle *clone() const override;

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;

  bool getParamValue(TColorStyle::bool_tag, int index) const override;
  void setParamValue(int index, bool value) override;

  void getParamRange(int index, int &min, int &max) const override;
  int getParamValue(TColorStyle::int_tag, int index) const override;
  void setParamValue(int index, int value) override;

  void getParamRange(int index, QStringList &enumItems) const override;

  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  TFilePath getParamValue(TColorStyle::TFilePath_tag, int index) const override;
  void setParamValue(int index, const TFilePath &path) override;

  void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                      TOutlineUtil::OutlineParameter param) const override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;
  QString getDescription() const override {
    return QCoreApplication::translate("OutlineViewerStyle",
                                       "OutlineViewer(OnlyDebug)");
  }
  int getTagId() const override { return 99; };
};

#endif

//-------------------------------------------------------------------

class TMatrioskaStrokeStyle;

class TMatrioskaStrokeProp final : public TStrokeProp {
protected:
  double m_outlinePixelSize;
  TMatrioskaStrokeStyle *m_colorStyle;

  std::vector<TStrokeOutline> m_outline;

  std::vector<TStroke *> m_appStrokes;

public:
  TMatrioskaStrokeProp(const TStroke *stroke, TMatrioskaStrokeStyle *style);
  ~TMatrioskaStrokeProp();

  const TColorStyle *getColorStyle() const override;

  TStrokeProp *clone(const TStroke *stroke) const override;
  void draw(const TVectorRenderData &rd) override;
};

class TMatrioskaStrokeStyle final : public TSolidColorStyle {
  double m_parameter;
  TPixel32 m_color2;

protected:
  void loadData(TInputStreamInterface &is) override;
  void saveData(TOutputStreamInterface &os) const override;

public:
  TMatrioskaStrokeStyle(TPixel32 color1  = TPixel32::Magenta,
                        TPixel32 color2  = TPixel32::Blue,
                        double parameter = 6.0, bool alternate = true);

  TColorStyle *clone() const override;

  int getColorParamCount() const override { return 2; }
  TPixel32 getColorParamValue(int index) const override;
  void setColorParamValue(int index, const TPixel32 &color) override;

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  QString getDescription() const override {
    return QCoreApplication::translate("TMatrioskaStrokeStyle", "Toothpaste");
  }
  int getTagId() const override { return 141; };
};

#endif  // STROKESTYLES_H
