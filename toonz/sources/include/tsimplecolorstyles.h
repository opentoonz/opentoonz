#pragma once

#ifndef TSIMPLECOLORSTYLES_H
#define TSIMPLECOLORSTYLES_H

// TnzCore includes
#include "tcolorstyles.h"
#include "tlevel.h"
#include "traster.h"
#include "tstrokeoutline.h"
#include "trailcycle.h"

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
  TrailCycle::Mode m_trailCycle = TrailCycle::Mode::Off;
  int m_trailFrameOffset        = 0;
  double m_trailSizeMultiplier  = 1.0;
  bool m_toonzRasterSource      = false;

public:
  TRasterImagePatternStrokeStyle();
  TRasterImagePatternStrokeStyle(const std::string &patternName);

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  int getLevelFrameCount() const { return m_level->getFrameCount(); }

  void computeTransformations(std::vector<TAffine> &positions,
                              const TStroke *stroke) const;
  void drawStroke(const TVectorRenderData &rd,
                  const std::vector<TAffine> &positions,
                  const TStroke *stroke) const;

  void invalidate(){};

  TColorStyle *clone() const override;
  TColorStyle &copy(const TColorStyle &other) override;
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

  TrailCycle::Mode getTrailCycleMode() const { return m_trailCycle; }
  void setTrailCycleMode(TrailCycle::Mode mode) {
    m_trailCycle = TrailCycle::modeFromValue(int(mode));
  }

  // 0 keeps the existing cycle; positive values select an exact source frame.
  int getTrailFrameOffset() const { return m_trailFrameOffset; }
  void setTrailFrameOffset(int frame) {
    m_trailFrameOffset = frame > 0 ? frame : 0;
  }
  // Convert the requested drawing number to the renderer's zero-based index.
  // Missing drawings (including gaps and letter-only variants) return -1.
  int getTrailStartFrameIndex() const;

  double getTrailSizeMultiplier() const { return m_trailSizeMultiplier; }
  void setTrailSizeMultiplier(double multiplier);

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;
  void getParamRange(int index, int &min, int &max) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;
  void getParamRange(int index, QStringList &items) const override;
  int getParamValue(TColorStyle::int_tag, int index) const override;
  void setParamValue(int index, int value) override;

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
  TrailCycle::Mode m_trailCycle = TrailCycle::Mode::Off;
  int m_trailFrameOffset        = 0;
  double m_trailSizeMultiplier  = 1.0;

public:
  TVectorImagePatternStrokeStyle();
  TVectorImagePatternStrokeStyle(const std::string &patternName);

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  int getLevelFrameCount() const { return m_level->getFrameCount(); }

  void computeTransformations(std::vector<TAffine> &positions,
                              const TStroke *stroke) const;
  void drawStroke(const TVectorRenderData &rd,
                  const std::vector<TAffine> &positions,
                  const TStroke *stroke) const;

  void invalidate(){};

  TColorStyle *clone() const override;
  TColorStyle &copy(const TColorStyle &other) override;
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

  TrailCycle::Mode getTrailCycleMode() const { return m_trailCycle; }
  void setTrailCycleMode(TrailCycle::Mode mode) {
    m_trailCycle = TrailCycle::modeFromValue(int(mode));
  }

  // 0 keeps the existing cycle; positive values select an exact source frame.
  int getTrailFrameOffset() const { return m_trailFrameOffset; }
  void setTrailFrameOffset(int frame) {
    m_trailFrameOffset = frame > 0 ? frame : 0;
  }
  // Convert the requested drawing number to the renderer's zero-based index.
  // Missing drawings (including gaps and letter-only variants) return -1.
  int getTrailStartFrameIndex() const;

  double getTrailSizeMultiplier() const { return m_trailSizeMultiplier; }
  void setTrailSizeMultiplier(double multiplier);

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;
  void getParamRange(int index, int &min, int &max) const override;

  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;
  void getParamRange(int index, QStringList &items) const override;
  int getParamValue(TColorStyle::int_tag, int index) const override;
  void setParamValue(int index, int value) override;

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

// Shared access for the Settings page, palette persistence and the Brush.
namespace TrailStyles {

// Used by both the Trail chooser and its source loader. Raster conversion keeps
// the full canvas and returns premultiplied pixels for Toonz Raster images.
DVAPI QString sourceFilters();
DVAPI TFilePath findSource(const TFilePath &root, const std::string &name);
DVAPI TRaster32P rasterSource(const TImageP &image, TPalette *levelPalette,
                              int sourceFrame = -1);

constexpr int cycleParam          = 2;
constexpr int frameOffsetParam    = 3;
constexpr int sizeMultiplierParam = 4;

inline bool isTrail(const TColorStyle *style) {
  return dynamic_cast<const TRasterImagePatternStrokeStyle *>(style) ||
         dynamic_cast<const TVectorImagePatternStrokeStyle *>(style);
}

inline TrailCycle::Mode getMode(const TColorStyle *style) {
  if (auto *trail = dynamic_cast<const TRasterImagePatternStrokeStyle *>(style))
    return trail->getTrailCycleMode();
  if (auto *trail = dynamic_cast<const TVectorImagePatternStrokeStyle *>(style))
    return trail->getTrailCycleMode();
  return TrailCycle::Mode::Off;
}

inline void setMode(TColorStyle *style, TrailCycle::Mode mode) {
  if (auto *trail = dynamic_cast<TRasterImagePatternStrokeStyle *>(style))
    trail->setTrailCycleMode(mode);
  else if (auto *trail = dynamic_cast<TVectorImagePatternStrokeStyle *>(style))
    trail->setTrailCycleMode(mode);
}

inline int getFrameOffset(const TColorStyle *style) {
  if (auto *trail = dynamic_cast<const TRasterImagePatternStrokeStyle *>(style))
    return trail->getTrailFrameOffset();
  if (auto *trail = dynamic_cast<const TVectorImagePatternStrokeStyle *>(style))
    return trail->getTrailFrameOffset();
  return 0;
}

inline void setFrameOffset(TColorStyle *style, int frame) {
  if (auto *trail = dynamic_cast<TRasterImagePatternStrokeStyle *>(style))
    trail->setTrailFrameOffset(frame);
  else if (auto *trail = dynamic_cast<TVectorImagePatternStrokeStyle *>(style))
    trail->setTrailFrameOffset(frame);
}

inline double getSizeMultiplier(const TColorStyle *style) {
  if (auto *trail = dynamic_cast<const TRasterImagePatternStrokeStyle *>(style))
    return trail->getTrailSizeMultiplier();
  if (auto *trail = dynamic_cast<const TVectorImagePatternStrokeStyle *>(style))
    return trail->getTrailSizeMultiplier();
  return 1.0;
}

inline void setSizeMultiplier(TColorStyle *style, double multiplier) {
  if (auto *trail = dynamic_cast<TRasterImagePatternStrokeStyle *>(style))
    trail->setTrailSizeMultiplier(multiplier);
  else if (auto *trail = dynamic_cast<TVectorImagePatternStrokeStyle *>(style))
    trail->setTrailSizeMultiplier(multiplier);
}

inline int startFrameIndex(const TColorStyle *style) {
  if (auto *trail = dynamic_cast<const TRasterImagePatternStrokeStyle *>(style))
    return trail->getTrailStartFrameIndex();
  if (auto *trail = dynamic_cast<const TVectorImagePatternStrokeStyle *>(style))
    return trail->getTrailStartFrameIndex();
  return -1;
}

inline int frameCount(const TColorStyle *style) {
  if (auto *trail = dynamic_cast<const TRasterImagePatternStrokeStyle *>(style))
    return trail->getLevelFrameCount();
  if (auto *trail = dynamic_cast<const TVectorImagePatternStrokeStyle *>(style))
    return trail->getLevelFrameCount();
  return 0;
}

}  // namespace TrailStyles

#endif  // TSIMPLECOLORSTYLES_H
