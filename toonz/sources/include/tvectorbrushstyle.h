#pragma once

#ifndef T_VECTOR_BRUSH_STYLE_H
#define T_VECTOR_BRUSH_STYLE_H

#include "tfilepath.h"
#include "tvectorimage.h"
#include "tcolorstyles.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************
//    Vector Brush Style  declaration
//**********************************************************************

class DVAPI TVectorBrushStyle final : public TColorStyle {
  static TFilePath m_rootDir;

  std::string m_brushName;
  TVectorImageP m_brush;
  int m_colorCount;

public:
  TVectorBrushStyle();
  TVectorBrushStyle(const std::string &brushName,
                    TVectorImageP vi = TVectorImageP());
  ~TVectorBrushStyle();

  TColorStyle *clone() const override;
  QString getDescription() const override;

  static TFilePath getRootDir() { return m_rootDir; }
  static void setRootDir(const TFilePath &path) {
    m_rootDir = path + "vector brushes";
  }

  void loadBrush(const std::string &brushName);

  int getTagId() const override { return 3000; }

  TVectorImageP getImage() const { return m_brush; }

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return true; }

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override;
  TRegionProp *makeRegionProp(const TRegion *region) override {
    assert(false);
    return 0;
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override;
  void setMainColor(const TPixel32 &color) override;

  int getColorParamCount() const override;
  TPixel32 getColorParamValue(int index) const override;
  void setColorParamValue(int index, const TPixel32 &color) override;

protected:
  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;

private:
  int getColorStyleId(int index) const;
};

#endif  // T_VECTOR_BRUSH_STYLE_H
