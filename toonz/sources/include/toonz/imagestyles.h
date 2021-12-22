#pragma once

#ifndef TIMAGESTYLES_H
#define TIMAGESTYLES_H

// TnzCore includes
#include "tsimplecolorstyles.h"
#include "tlevel.h"
#include "traster.h"
#include "tstrokeoutline.h"

// Qt includes
#include <QCoreApplication>

#undef DVAPI
#undef DVVAR

#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class ToonzScene;

//**********************************************************************************
//    TTextureStyle  declaration
//**********************************************************************************
class TTextureParams {
public:
  enum TYPE {
    FIXED = 0,  // texture is applied fixed respect the image raster borders.
    AUTOMATIC,  // texture is centered on the centroid of the filled area to
                // texture
    RANDOM
  };

  TYPE m_type;
  double m_scale;
  double m_rotation;
  TPointD m_displacement;
  double m_contrast;
  bool m_isPattern;
  TPixel32 m_patternColor;

  TTextureParams()
      : m_type(AUTOMATIC)
      , m_scale(1.0)
      , m_rotation(0.0)
      , m_displacement()
      , m_contrast(1.0)
      , m_isPattern(false)
      , m_patternColor(TPixel::White) {}

  bool operator==(const TTextureParams &p) const {
    if (m_type != p.m_type) return false;
    if (m_scale != p.m_scale) return false;
    if (m_rotation != p.m_rotation) return false;
    if (m_displacement != p.m_displacement) return false;
    if (m_contrast != p.m_contrast) return false;
    if (m_isPattern != p.m_isPattern) return false;
    if (m_patternColor != p.m_patternColor) return false;
    return true;
  }
};

class DVAPI TImageStyle {
protected:
  static TFilePath m_libraryDir;
  static ToonzScene *m_currentScene;

public:
  TImageStyle(){};
  static void setLibraryDir(const TFilePath &fp) { m_libraryDir = fp; }
  static void setCurrentScene(ToonzScene *currentScene) {
    m_currentScene = currentScene;
  }
};

//--------------------------------------------------------------------------------------------------------

class DVAPI TTextureStyle final : public TOutlineStyle,
                                  TRasterStyleFx,
                                  TImageStyle {
public:
private:
  TTextureParams m_params;
  TRaster32P m_texture;
  TFilePath m_texturePath, m_texturePathLoaded;

  TTessellator *m_tessellator;
  TPixel32 m_averageColor;

protected:
  void makeIcon(const TDimension &d) override;

  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;

public:
  // TTextureStyle();
  // TTextureStyle(const TFilePath &path);
  TTextureStyle(const TRasterP &ras, const TFilePath &m_texturePath);
  TTextureStyle(const TTextureStyle &);

  ~TTextureStyle();

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;
  QString getParamNames(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;

  void getParamRange(int index, QStringList &enumItems) const override;
  bool getParamValue(TColorStyle::bool_tag, int index) const override;
  int getParamValue(TColorStyle::int_tag, int index) const override;
  TFilePath getParamValue(TColorStyle::TFilePath_tag, int index) const override;
  bool isPattern() const { return m_params.m_isPattern; }
  static void fillCustomTextureIcon(const TRaster32P &r);

  //-------------------------------------------------------------------

  void setParamValue(int index, const TFilePath &value) override;
  void setParamValue(int index, double value) override;
  void setParamValue(int index, bool value) override;
  void setParamValue(int index, int value) override;

  int getColorParamCount() const override;
  TPixel32 getColorParamValue(int index) const override;
  void setColorParamValue(int index, const TPixel32 &color) override;

  TColorStyle *clone() const override;
  QString getDescription() const override;

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_averageColor; }
  void setMainColor(const TPixel32 &color) override {
    m_params.m_patternColor = color;
  }

  // void draw(const TVectorRenderData &rd,  TStrokeOutline* outline) const;
  void drawRegion(const TColorFunction *cf, const bool antiAliasing,
                  TRegionOutline &outline) const override;
  // void drawRegion( const TVectorRenderData &rd, TRegionOutline &boundary )
  // const;
  void drawStroke(const TColorFunction *cf, TStrokeOutline *outline,
                  const TStroke *stroke) const override;

  TRasterStyleFx *getRasterStyleFx() override { return this; }

  bool isRasterStyle() const override { return true; }

  void computeOutline(const TStroke *stroke, TStrokeOutline &outline,
                      TOutlineUtil::OutlineParameter param) const override;

  void setTexture(const TRasterP &color);
  TRasterP getTexture() const;

  TPixel32 getAverageColor() const override;

  int getTagId() const override;

  // static TRaster32P loadTexture(const TFilePath &path);

  bool isPaintStyle() const override { return true; }
  bool isInkStyle() const override { return true; }
  bool inkFxNeedRGBMRaster() const override { return true; }

  bool compute(const Params &params) const override {
    return doCompute(params);
  }  // faccio questo per compilare su mac! le virtuali pubbliche devono essere
     // inline

private:
  bool doCompute(const Params &params) const;
  void setAverageColor();
  bool loadTextureRaster();
  TRaster32P loadTextureRasterWithFrame(int frame) const;
  // Not assignable
  TTextureStyle &operator=(const TTextureStyle &);
};

#endif
