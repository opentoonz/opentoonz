#pragma once

#ifndef TOONZIMAGE_DATA_H
#define TOONZIMAGE_DATA_H

#include "tcommon.h"
#include "toonz/ttileset.h"
#include "tpalette.h"
#include "tstroke.h"
#include "toonzqt/dvmimedata.h"
#include "ttoonzimage.h"

#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TTileSetCM32;
class StrokesData;
class ToonzScene;

//===================================================================
// RasterImageData
//-------------------------------------------------------------------

class RasterImageData : public DvMimeData {
protected:
  double m_dpiX, m_dpiY;
  std::vector<TRectD> m_rects;
  std::vector<TStroke> m_strokes;
  std::vector<TStroke> m_originalStrokes;
  TAffine m_transformation;
  TDimension m_dim;

public:
  RasterImageData();
  ~RasterImageData();

  virtual void setData(const TRasterP &copiedRaster, const TPaletteP &palette,
                       double dpiX, double dpiY, const TDimension &dim,
                       const std::vector<TRectD> &rects,
                       const std::vector<TStroke> &strokes,
                       const std::vector<TStroke> &originalStrokes,
                       const TAffine &transformation) = 0;

  virtual void getData(TRasterP &copiedRaster, double &dpiX, double &dpiY,
                       std::vector<TRectD> &rects,
                       std::vector<TStroke> &strokes,
                       std::vector<TStroke> &originalStrokes,
                       TAffine &transformation,
                       TPalette *targetPalette) const = 0;

  virtual StrokesData *toStrokesData(ToonzScene *scene) const = 0;
  virtual TPointD getDpi() const                              = 0;
  TDimension getDim() const { return m_dim; }

  RasterImageData *clone() const override = 0;

  // Necessary for undo purpose!!!!
  virtual int getMemorySize() const = 0;
};

//===================================================================
// ToonzImageData
//-------------------------------------------------------------------
/*-- SelectionToolで選択した画像のデータ --*/
class DVAPI ToonzImageData final : public RasterImageData {
  TRasterCM32P m_copiedRaster;
  TPaletteP m_palette;

  std::set<int> m_usedStyles;

public:
  ToonzImageData();
  ToonzImageData(const ToonzImageData &);
  ~ToonzImageData();
  // data <- floating ti;
  void setData(const TRasterP &copiedRaster, const TPaletteP &palette,
               double dpiX, double dpiY, const TDimension &dim,
               const std::vector<TRectD> &rects,
               const std::vector<TStroke> &strokes,
               const std::vector<TStroke> &originalStrokes,
               const TAffine &transformation) override;

  // floating ti <- data;
  void getData(TRasterP &copiedRaster, double &dpiX, double &dpiY,
               std::vector<TRectD> &rects, std::vector<TStroke> &strokes,
               std::vector<TStroke> &originalStrokes, TAffine &transformation,
               TPalette *targetPalette) const override;

  StrokesData *toStrokesData(ToonzScene *scene) const override;
  TPointD getDpi() const override { return TPointD(m_dpiX, m_dpiY); }

  ToonzImageData *clone() const override { return new ToonzImageData(*this); }
  int getMemorySize() const override;
};

//===================================================================
// FullColorImageData
//-------------------------------------------------------------------

class DVAPI FullColorImageData final : public RasterImageData {
  TRasterP m_copiedRaster;
  TPaletteP m_palette;

public:
  FullColorImageData();
  FullColorImageData(const FullColorImageData &);
  ~FullColorImageData();

  // data <- floating ti;
  void setData(const TRasterP &copiedRaster, const TPaletteP &palette,
               double dpiX, double dpiY, const TDimension &dim,
               const std::vector<TRectD> &rects,
               const std::vector<TStroke> &strokes,
               const std::vector<TStroke> &originalStrokes,
               const TAffine &transformation) override;

  // floating ti <- data;
  void getData(TRasterP &copiedRaster, double &dpiX, double &dpiY,
               std::vector<TRectD> &rects, std::vector<TStroke> &strokes,
               std::vector<TStroke> &originalStrokes, TAffine &transformation,
               TPalette *targetPalette) const override;

  StrokesData *toStrokesData(ToonzScene *scene) const override;
  TPointD getDpi() const override { return TPointD(m_dpiX, m_dpiY); }

  FullColorImageData *clone() const override {
    return new FullColorImageData(*this);
  }
  int getMemorySize() const override;
};

#endif
