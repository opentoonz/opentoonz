#pragma once

#ifndef MYPAINTBRUSHSTYLE_H
#define MYPAINTBRUSHSTYLE_H

#include "mypaint.h"

// TnzCore includes
#include "imagestyles.h"

#undef DVAPI
#undef DVVAR

#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************************
//    TMyPaintBrushStyle declaration
//**********************************************************************************

class DVAPI TMyPaintBrushStyle final : public TColorStyle, TImageStyle {
private:
  TFilePath m_path;
  TFilePath m_fullpath;
  mypaint::Brush m_brushOriginal;
  mypaint::Brush m_brushModified;
  TRasterP m_preview;
  TPixel32 m_color;

  std::map<MyPaintBrushSetting, float> m_baseValues;

  TFilePath decodePath(const TFilePath &path) const;
  void loadBrush(const TFilePath &path);

public:
  TMyPaintBrushStyle();
  TMyPaintBrushStyle(const TFilePath &path);
  TMyPaintBrushStyle(const TMyPaintBrushStyle &other);
  ~TMyPaintBrushStyle();

  TColorStyle *clone() const override
    { return new TMyPaintBrushStyle(*this); }

  TColorStyle &copy(const TColorStyle &other) override;

  static std::string getBrushType();
  static TFilePathSet getBrushesDirs();

  const TFilePath& getPath() const
    { return m_path; }
  const mypaint::Brush& getBrush() const
    { return m_brushModified; }
  const TRasterP& getPreview() const
    { return m_preview; }

  TStrokeProp* makeStrokeProp(const TStroke * /* stroke */) override
    { return 0; }
  TRegionProp* makeRegionProp(const TRegion * /* region */) override
    { return 0; }
  bool isRegionStyle() const override
    { return false; }
  bool isStrokeStyle() const override
    { return false; }

  bool hasMainColor() const override
    { return true; }
  TPixel32 getMainColor() const override
    { return m_color; }
  void setMainColor(const TPixel32 &color) override
    { m_color = color; }

  int getTagId() const override
    { return 4001; }

  QString getDescription() const override;

  void setBaseValue(MyPaintBrushSetting id, bool enable, float value);
  void resetBaseValues();

  void setBaseValue(MyPaintBrushSetting id, float value)
    { setBaseValue(id, true, value); }

  void setBaseValueEnabled(MyPaintBrushSetting id, bool enable)
    { setBaseValue(id, enable, getBaseValue(id)); }

  const std::map<MyPaintBrushSetting, float> getBaseValues() const
    { return m_baseValues; }

  float getBaseValue(MyPaintBrushSetting id) const {
    std::map<MyPaintBrushSetting, float>::const_iterator i = m_baseValues.find(id);
    return i == m_baseValues.end()
         ? m_brushOriginal.getBaseValue(id)
         : i->second;
  }

  bool getBaseValueEnabled(MyPaintBrushSetting id) const {
    std::map<MyPaintBrushSetting, float>::const_iterator i = m_baseValues.find(id);
    return i != m_baseValues.end();
  }

  int getParamCount() const override;
  QString getParamNames(int index) const override;
  ParamType getParamType(int index) const override;
  bool hasParamDefault(int index) const override;
  void setParamDefault(int index) override;
  bool isParamDefault(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  void setParamValue(int index, double value) override;
  double getParamValue(double_tag, int index) const override;

protected:
  void makeIcon(const TDimension &d) override;
  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;
};

#endif
