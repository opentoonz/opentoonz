#pragma once

#ifndef TSIMPLECOLORSTYLES_H
#define TSIMPLECOLORSTYLES_H

// TnzCore includes
#include "tcolorstyles.h"
#include "tlevel.h"
#include "traster.h"
#include "tstrokeoutline.h"

// Qt includes
#include <QCoreApplication>

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//    Forward declarations

class TStrokeProp;
class TRegionProp;
class TRegionOutline;
class TTessellator;
class TColorFunction;
class TVectorImage;

//=================================================

//**********************************************************************************
//    TSimpleStrokeStyle  declaration
//**********************************************************************************

/*!
  Base class for stroke color styles.
*/

class DVAPI TSimpleStrokeStyle : public TColorStyle {
public:
  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;

  TRegionProp *makeRegionProp(const TRegion *) override {
    assert(false);
    return 0;
  }

  virtual void drawStroke(const TColorFunction *cf,
                          const TStroke *stroke) const = 0;
};

//**********************************************************************************
//    TOutlineStyle  declaration
//**********************************************************************************

class DVAPI TOutlineStyle : public TColorStyle {
public:
  class StrokeOutlineModifier {
  public:
    StrokeOutlineModifier() {}
    virtual ~StrokeOutlineModifier() {}
    virtual StrokeOutlineModifier *clone() const = 0;

    virtual void modify(TStrokeOutline &outline) const = 0;
  };

  class RegionOutlineModifier {
  public:
    RegionOutlineModifier() {}
    virtual ~RegionOutlineModifier() {}
    virtual RegionOutlineModifier *clone() const = 0;

    virtual void modify(TRegionOutline &outline) const = 0;
  };

protected:
  //  StrokeOutlineModifier *m_strokeOutlineModifier;
  RegionOutlineModifier *m_regionOutlineModifier;

public:
  TOutlineStyle();
  TOutlineStyle(const TOutlineStyle &);
  virtual ~TOutlineStyle();

  // StrokeOutlineModifier* getStrokeOutlineModifier() const { return
  // m_strokeOutlineModifier; }
  // void setStrokeOutlineModifier(StrokeOutlineModifier *modifier);

  RegionOutlineModifier *getRegionOutlineModifier() const {
    return m_regionOutlineModifier;
  }
  void setRegionOutlineModifier(RegionOutlineModifier *modifier);

  bool isRegionStyle() const override { return true; }
  bool isStrokeStyle() const override { return true; }

  virtual void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                              TOutlineUtil::OutlineParameter param) const;

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;
  TRegionProp *makeRegionProp(const TRegion *region) override;

  // virtual void drawRegion( const TVectorRenderData &rd, TRegionOutline
  // &outline ) const =0 ;
  virtual void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                          TRegionOutline &outline) const = 0;

  virtual void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                          const TStroke *stroke) const = 0;
  // draw aliased stroke. currently reimplemented by TSolidColorStyle only
  virtual void drawAliasedStroke(const TColorFunction *cf,
                                 TStrokeOutline *outline,
                                 const TStroke *stroke) const {
    drawStroke(cf, outline, stroke);
  };

protected:
  // Not assignable
  TOutlineStyle &operator=(const TOutlineStyle &);
};

//-------------------------------------------------------------------

typedef TSmartPointerT<TOutlineStyle> TOutlineStyleP;

//**********************************************************************************
//    TSolidColorStyle  declaration
//**********************************************************************************

class DVAPI TSolidColorStyle : public TOutlineStyle {
  TPixel32 m_color;
  TTessellator *m_tessellator;

protected:
  void makeIcon(const TDimension &d) override;

  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;

public:
  TSolidColorStyle(const TPixel32 &color = TPixel32::Black);
  TSolidColorStyle(const TSolidColorStyle &);
  ~TSolidColorStyle();

  TColorStyle *clone() const override;

  QString getDescription() const override;
  std::string getBrushIdName() const override;
  static std::size_t staticBrushIdHash();

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &outline) const override;

  void doDrawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                    const TStroke *s, bool antialias) const;

  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *s) const override {
    doDrawStroke(cf, outline, s, true);
  }
  void drawAliasedStroke(const TColorFunction *cf, TStrokeOutline *outline,
                         const TStroke *s) const override {
    doDrawStroke(cf, outline, s, false);
  }

  int getTagId() const override;

private:
  // Not assignable
  TSolidColorStyle &operator=(const TSolidColorStyle &);
};

//**********************************************************************************
//    TCenterLineStrokeStyle  declaration
//**********************************************************************************

/*!
  Constant thickness stroke style.
*/

class DVAPI TCenterLineStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_color;
  USHORT m_stipple;
  double m_width;

public:
  TCenterLineStrokeStyle(const TPixel32 &color = TPixel32(0, 0, 0, 255),
                         USHORT stipple = 0x0, double width = 1.0);

  TColorStyle *clone() const override;

  QString getDescription() const override;
  std::string getBrushIdName() const override;

  TPixel32 getColor() const { return m_color; }
  USHORT getStipple() const { return m_stipple; }

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;

  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  int getTagId() const override;

protected:
  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;

private:
  // Not assignable
  TCenterLineStrokeStyle &operator=(const TCenterLineStrokeStyle &);
};

//------------------------------------------------------------------------------

//**********************************************************************************
//    TRasterImagePatternStrokeStyle  declaration
//**********************************************************************************

class DVAPI TRasterImagePatternStrokeStyle final : public TColorStyle {
  static TFilePath m_rootDir;

protected:
  TLevelP m_level;
  std::string m_name;
  double m_space, m_rotation;

public:
  TRasterImagePatternStrokeStyle();
  TRasterImagePatternStrokeStyle(const std::string &patternName);

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  int getLevelFrameCount() { return m_level->getFrameCount(); }

  void computeTransformations(std::vector<TAffine> &positions,
                              const TStroke *stroke) const;
  void drawStroke(const TVectorRenderData &rd,
                  const std::vector<TAffine> &positions,
                  const TStroke *stroke) const;

  void invalidate(){};

  TColorStyle *clone() const override;
  TColorStyle *clone(std::string brushIdName) const override;

  QString getDescription() const override;
  std::string getBrushIdName() const override;

  bool hasMainColor() const override { return false; }
  TPixel32 getMainColor() const override { return TPixel32::Black; }
  void setMainColor(const TPixel32 &) override {}

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;
  TRegionProp *makeRegionProp(const TRegion *) override {
    assert(false);
    return 0;
  };

  int getTagId() const override { return 2000; };
  void getObsoleteTagIds(std::vector<int> &ids) const override;

  void loadLevel(const std::string &patternName);
  static TFilePath getRootDir();
  static void setRootDir(const TFilePath &path) {
    m_rootDir = path + "custom styles";
  }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  TRectD getStrokeBBox(const TStroke *stroke) const override;

protected:
  void makeIcon(const TDimension &d) override;

  void loadData(TInputStreamInterface &) override;
  void loadData(int oldId, TInputStreamInterface &) override;

  void saveData(TOutputStreamInterface &) const override;

private:
  // Not assignable
  TRasterImagePatternStrokeStyle &operator=(
      const TRasterImagePatternStrokeStyle &);
};

//**********************************************************************************
//    TVectorImagePatternStrokeStyle  declaration
//**********************************************************************************

class DVAPI TVectorImagePatternStrokeStyle final : public TColorStyle {
  static TFilePath m_rootDir;

protected:
  TLevelP m_level;
  std::string m_name;
  double m_space, m_rotation;

public:
  TVectorImagePatternStrokeStyle();
  TVectorImagePatternStrokeStyle(const std::string &patternName);

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  int getLevelFrameCount() { return m_level->getFrameCount(); }

  void computeTransformations(std::vector<TAffine> &positions,
                              const TStroke *stroke) const;
  void drawStroke(const TVectorRenderData &rd,
                  const std::vector<TAffine> &positions,
                  const TStroke *stroke) const;

  void invalidate(){};

  TColorStyle *clone() const override;
  TColorStyle *clone(std::string brushIdName) const override;

  QString getDescription() const override;
  std::string getBrushIdName() const override;

  bool hasMainColor() const override { return false; }
  TPixel32 getMainColor() const override { return TPixel32::Black; }
  void setMainColor(const TPixel32 &) override {}

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;
  TRegionProp *makeRegionProp(const TRegion *) override {
    assert(false);
    return 0;
  };

  int getTagId() const override { return 2800; };
  void getObsoleteTagIds(std::vector<int> &ids) const override;

  void loadLevel(const std::string &patternName);
  static TFilePath getRootDir();
  static void setRootDir(const TFilePath &path) {
    m_rootDir = path + "custom styles";
  }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  static void clearGlDisplayLists();

  TRectD getStrokeBBox(const TStroke *stroke) const override;

protected:
  void makeIcon(const TDimension &d) override;

  void loadData(TInputStreamInterface &) override;
  void loadData(int oldId, TInputStreamInterface &) override;

  void saveData(TOutputStreamInterface &) const override;

private:
  // Not assignable
  TVectorImagePatternStrokeStyle &operator=(
      const TVectorImagePatternStrokeStyle &);
};

#endif  // TSIMPLECOLORSTYLES_H
