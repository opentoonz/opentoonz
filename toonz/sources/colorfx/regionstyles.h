#pragma once

#ifndef TDERIVEDREGIONSTYLES_H
#define TDERIVEDREGIONSTYLES_H

// TnzCore includes
#include "tvectorimage.h"
#include "tregionoutline.h"
#include "tsimplecolorstyles.h"

#undef DVAPI
#undef DVVAR

#ifdef COLORFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//======================================================

//    Forward declarations

class TRandom;

//======================================================

//============================================================

class MovingModifier : public TOutlineStyle::RegionOutlineModifier {
  TPointD m_move;

public:
  MovingModifier(const TPointD &point) : m_move(point) {}

  TOutlineStyle::RegionOutlineModifier *clone() const;

  TPointD getMovePoint() const { return m_move; }

  void modify(TRegionOutline &outline) const;

public:
  void loadData(TInputStreamInterface &is) { is >> m_move.x >> m_move.y; }

  void saveData(TOutputStreamInterface &os) const {
    os << m_move.x << m_move.y;
  }
};

//============================================================

class MovingSolidColor : public TSolidColorStyle {
public:
  MovingSolidColor(const TPixel32 &color, const TPointD &move);

  TColorStyle *clone() const;

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  int getTagId() const { return 1125; };
  QString getDescription() const {
    return QCoreApplication::translate("MovingSolidColor", "Offset");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

//============================================================

class DVAPI ShadowStyle : public TSolidColorStyle {
  TPointD m_shadowDirection;
  TPixel32 m_shadowColor;
  double m_density;
  double m_len;

public:
  ShadowStyle(const TPixel32 &bgColor, const TPixel32 &shadowColor,
              const TPointD &shadowDirection = TPointD(-1, -1),
              double len = 30.0, double density = 0.4);

  TColorStyle *clone() const;

  void makeIcon(const TDimension &d);
  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  // TPixel32 getMainColor() const {return m_shadowColor; }
  // void setMainColor(const TPixel32 &color){ m_shadowColor=color; }

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  int getTagId() const { return 1127; };
  QString getDescription() const {
    return QCoreApplication::translate("ShadowStyle", "Hatched Shading");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;

  // it is too slow and if the region is too complex, some flash readers
  // (IExplorer) crash.
  // So it's better drawing it as a normal solid color
  // void drawRegion( TFlash& flash, const TRegion* r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void drawPolyline(const TColorFunction *cf, std::vector<T3DPointD> &polyline,
                    TPointD shadowDirection) const;
};

//============================================================

class DVAPI ShadowStyle2 : public TSolidColorStyle {
  TPointD m_shadowDirection;
  TPixel32 m_shadowColor;
  double m_shadowLength;

public:
  ShadowStyle2(const TPixel32 &bgColor, const TPixel32 &shadowColor,
               const TPointD &shadowDirection = TPointD(-1, -1),
               double shadowLength            = 70.0);

  TColorStyle *clone() const;

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  // TPixel32 getMainColor() const {return m_shadowColor; }
  // void setMainColor(const TPixel32 &color){ m_shadowColor=color; }

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  int getTagId() const { return 1135; };
  QString getDescription() const {
    return QCoreApplication::translate("ShadowStyle2", "Plain Shadow");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void drawPolyline(const TColorFunction *cf,
                    const std::vector<T3DPointD> &polyline,
                    TPointD shadowDirection) const;
  int drawPolyline(TFlash &flash, std::vector<T3DPointD> &polyline,
                   TPointD shadowDirection, const bool isDraw = true) const;
};

//============================================================

class RubberModifier : public TOutlineStyle::RegionOutlineModifier {
  double m_deform;

public:
  RubberModifier(double deform) : m_deform(deform) {}

  void loadData(TInputStreamInterface &is) { is >> m_deform; }
  void saveData(TOutputStreamInterface &os) const { os << m_deform; }

  double getDeform() { return m_deform; }
  void setDeform(const double deform) { m_deform = deform; }

  void modify(TRegionOutline &outline) const;

  TOutlineStyle::RegionOutlineModifier *clone() const;
};

//============================================================

class DVAPI TRubberFillStyle : public TSolidColorStyle {
  typedef std::vector<TQuadratic> QuadraticVector;
  typedef std::vector<TQuadratic *> QuadraticPVector;

public:
  TRubberFillStyle(const TPixel32 &color, double deform);

  TColorStyle *clone() const;

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void makeIcon(const TDimension &d);

  int getTagId() const { return 1128; };
  QString getDescription() const {
    return QCoreApplication::translate("TRubberFillStyle", "Blob");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void transformPolylines();
};

//============================================================

class DVAPI TPointShadowFillStyle : public TSolidColorStyle {
  TPointD m_shadowDirection;
  TPixel32 m_shadowColor;
  double m_shadowSize;
  double m_density;
  double m_pointSize;

public:
  TPointShadowFillStyle(const TPixel32 &bgColor, const TPixel32 &shadowColor,
                        const TPointD &shadowDirection = TPointD(-1, -1),
                        double density = 0.1, double shadowSize = 30.0,
                        double pointSize = 5.0);

  TColorStyle *clone() const;

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  /*
TPixel32 getMainColor() const {return m_shadowColor; }
void setMainColor(const TPixel32 &color){ m_shadowColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  int getTagId() const { return 1129; };
  QString getDescription() const {
    return QCoreApplication::translate("TPointShadowFillStyle",
                                       "Sponge Shading");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  double triangleArea(const TPointD &a, const TPointD &b,
                      const TPointD &c) const;
  void shadowOnEdge_parallel(const TPointD &p0, const TPointD &p1,
                             const TPointD &p2, TRandom &rnd) const;
  int shadowOnEdge_parallel(TFlash &flash, const TPointD &p0, const TPointD &p1,
                            const TPointD &p2, TRandom &rnd,
                            const double radius, const bool isDraw) const;

  void deleteSameVerts(TRegionOutline::Boundary::iterator &rit,
                       std::vector<T3DPointD> &pv) const;
};

//============================================================

class DVAPI TDottedFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_dotSize;
  double m_dotDist;
  bool m_isShifted;

public:
  TDottedFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                   const double dotSize, const double dotDist,
                   const bool isShifted);

  TDottedFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1130; };
  QString getDescription() const {
    return QCoreApplication::translate("TDottedFillStyle", "Polka Dots");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  int nbClip(const double LDotDist, const bool LIsShifted,
             const TRectD &bbox) const;
};

//============================================================

class DVAPI TCheckedFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_HDist, m_HAngle;
  double m_VDist, m_VAngle, m_Thickness;

public:
  TCheckedFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                    const double HDist, const double HAngle, const double VDist,
                    const double VAngle, const double Thickness);

  TCheckedFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1131; };
  QString getDescription() const {
    return QCoreApplication::translate("TCheckedFillStyle", "Square");
  }

private:
  void getHThickline(const TPointD &lc, const double lx, TPointD &p0,
                     TPointD &p1, TPointD &p2, TPointD &p3) const;
  void getVThickline(const TPointD &lc, const double ly, TPointD &p0,
                     TPointD &p1, TPointD &p2, TPointD &p3) const;
  int nbClip(const TRectD &bbox) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

//============================================================

class ArtisticModifier : public TOutlineStyle::RegionOutlineModifier {
  TPointD m_move;
  double m_period;

public:
  ArtisticModifier(const TPointD &point, double period)
      : m_move(point), m_period(period) {}

  TOutlineStyle::RegionOutlineModifier *clone() const;

  void loadData(TInputStreamInterface &is) {
    is >> m_move.x >> m_move.y >> m_period;
  }
  void saveData(TOutputStreamInterface &os) const {
    os << m_move.x << m_move.y << m_period;
  }

  TPointD getMovePoint() const { return m_move; }
  double getPeriod() const { return m_period; }

  void modify(TRegionOutline &outline) const;
};

//============================================================

class ArtisticSolidColor : public TSolidColorStyle {
public:
  ArtisticSolidColor(const TPixel32 &color, const TPointD &move, double period);

  TColorStyle *clone() const;

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  int getTagId() const { return 1132; };
  QString getDescription() const {
    return QCoreApplication::translate("ArtisticSolidColor", "Irregular");
  }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

//============================================================

class DVAPI TChalkFillStyle : public TSolidColorStyle {
  TPixel32 m_color0;
  double m_density, m_size;

public:
  TChalkFillStyle(const TPixel32 &color0, const TPixel32 &color1,
                  const double density, const double size);
  TChalkFillStyle(const TPixel32 &color0, const TPixel32 &color1);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_color0; }
void setMainColor(const TPixel32 &color){ m_color0=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  QString getDescription() const {
    return QCoreApplication::translate("TChalkFillStyle", "Chalk");
  }
  void loadData(int oldId, TInputStreamInterface &);
  void getObsoleteTagIds(std::vector<int> &ids) const { ids.push_back(1133); }
  int getTagId() const { return 1143; };

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

//============================================================

class DVAPI TChessFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_HDist, m_VDist, m_Angle;

public:
  TChessFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                  const double HDist, const double VDist, const double Angle);
  TChessFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1136; };
  QString getDescription() const {
    return QCoreApplication::translate("TChessFillStyle", "Chessboard");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void makeGrid(TRectD &bbox, TRotation &rotM, std::vector<TPointD> &grid,
                int &nbClip) const;
};

//============================================================

class DVAPI TStripeFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_Dist, m_Angle, m_Thickness;

public:
  TStripeFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                   const double Dist, const double Angle,
                   const double Thickness);
  TStripeFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1137; };
  QString getDescription() const {
    return QCoreApplication::translate("TStripeFillStyle", "Banded");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

  void makeIcon(const TDimension &d);

private:
  void getThickline(const TPointD &lc, const double ly, TPointD &p0,
                    TPointD &p1, TPointD &p2, TPointD &p3) const;
  int nbClip(const TRectD &bbox) const;
};

//============================================================

class DVAPI TLinGradFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_Angle;
  double m_XPos, m_YPos, m_Size;

public:
  TLinGradFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                    const double Angle, const double XPos, const double YPos,
                    const double Size);
  TLinGradFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1138; };
  QString getDescription() const {
    return QCoreApplication::translate("TLinGradFillStyle", "Linear Gradient");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void getRects(const TRectD &bbox, std::vector<TPointD> &r0,
                std::vector<TPointD> &r1, std::vector<TPointD> &r2) const;

  void getRect(const TRectD &bbox, std::vector<TPointD> &r) const;
};

//============================================================

class DVAPI TRadGradFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_Radius;
  double m_XPos, m_YPos;
  double m_Smooth;

public:
  TRadGradFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                    const double XPos, const double YPos, const double Radius,
                    const double Smooth);
  TRadGradFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  //  TPixel32 getMainColor() const {return m_pointColor; }
  //  void setMainColor(const TPixel32 &color){ m_pointColor=color; }

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1139; };
  QString getDescription() const {
    return QCoreApplication::translate("TRadGradFillStyle", "Radial Gradient");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

//============================================================

class DVAPI TCircleStripeFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor;
  double m_XPos, m_YPos;
  double m_Dist, m_Thickness;

public:
  TCircleStripeFillStyle(const TPixel32 &bgColor, const TPixel32 &pointColor,
                         const double XPos, const double YPos,
                         const double Dist, const double Thickness);
  TCircleStripeFillStyle(const TPixel32 &color);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor; }
void setMainColor(const TPixel32 &color){ m_pointColor=color; }
*/

  int getColorParamCount() const { return 2; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1140; };
  QString getDescription() const {
    return QCoreApplication::translate("TCircleStripeFillStyle", "Concentric");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void getCircleStripeQuads(const TPointD &center, const double r1,
                            const double r2, std::vector<TPointD> &pv) const;
  void drawCircleStripe(const TPointD &center, const double r1, const double r2,
                        const TPixel32 &col) const;
};

//============================================================

class DVAPI TMosaicFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor[4];
  double m_size;
  double m_deform;
  double m_minThickness;
  double m_maxThickness;

public:
  TMosaicFillStyle(const TPixel32 &bgColor, const TPixel32 pointColor[4],
                   const double size, const double deform,
                   const double minThickness, const double maxThickness);
  TMosaicFillStyle(const TPixel32 bgColor);

  TColorStyle *clone() const;

  /*
TPixel32 getMainColor() const {return m_pointColor[0]; }
void setMainColor(const TPixel32 &color){ m_pointColor[0]=color; }
*/

  int getColorParamCount() const { return 5; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1141; };
  QString getDescription() const {
    return QCoreApplication::translate("TMosaicFillStyle", "Stained Glass");
  }

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;

private:
  void preaprePos(const TRectD &box, std::vector<TPointD> &v, int &lX, int &lY,
                  TRandom &rand) const;
  bool getQuad(const int ix, const int iy, const int lX, const int lY,
               std::vector<TPointD> &v, TPointD *pquad, TRandom &rand) const;
};

//============================================================

class DVAPI TPatchFillStyle : public TSolidColorStyle {
  TPixel32 m_pointColor[6];
  double m_size;
  double m_deform;
  double m_thickness;

public:
  TPatchFillStyle(const TPixel32 &bgColor, const TPixel32 pointColor[6],
                  const double size, const double deform,
                  const double thickness);
  TPatchFillStyle(const TPixel32 &bgColor);

  TColorStyle *clone() const;

  int getColorParamCount() const { return 7; }
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  bool isRegionStyle() const { return true; }
  bool isStrokeStyle() const { return false; }

  int getParamCount() const;
  TColorStyle::ParamType getParamType(int index) const;

  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &boundary) const;
  void drawRegion(TFlash &flash, const TRegion *r) const;

  int getTagId() const { return 1142; };
  QString getDescription() const {
    return QCoreApplication::translate("TPatchFillStyle", "Beehive");
  }

private:
  void preaprePos(const TRectD &box, std::vector<TPointD> &v, int &lX, int &lY,
                  TRandom &rand) const;
  bool getQuadLine(const TPointD &a, const TPointD &b, const double thickn,
                   TPointD *quad) const;
  void drawGLQuad(const TPointD *quad) const;
  int nbClip(const int lX, const int lY, const std::vector<TPointD> &v) const;
  void drawFlashQuad(TFlash &flash, const TPointD *quad) const;
  void drawFlashTriangle(TFlash &flash, const TPointD &p1, const TPointD &p2,
                         const TPointD &p3) const;

protected:
  void loadData(TInputStreamInterface &is);
  void saveData(TOutputStreamInterface &os) const;
};

#endif  // TDERIVEDREGIONSTYLES_H
