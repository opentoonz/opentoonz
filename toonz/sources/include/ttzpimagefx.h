#pragma once

#ifndef TTZPIMAGEFX_H
#define TTZPIMAGEFX_H

// TnzCore includes
#include "tpalette.h"
#include "trastercm.h"

// TnzBase includes
#include "trasterfxrenderdata.h"

// STD includes
#include <vector>
#include <set>
#include <string>

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************************************
//    ExternalPaletteFxRenderData  declaration
//**********************************************************************************************

class DVAPI ExternalPaletteFxRenderData final : public TRasterFxRenderData {
public:
  TPaletteP m_palette;
  std::string m_name;

public:
  ExternalPaletteFxRenderData(TPaletteP palette, const std::string &name);

  float typeIndex() const override { return 0.0f; }

  bool operator==(const TRasterFxRenderData &data) const override;
  std::string toString() const override;
};

//**********************************************************************************************
//    PaletteFilterFxRenderData  declaration
//**********************************************************************************************

enum FilterType {
  eApplyToInksAndPaints = 0,
  eApplyToInksKeepingAllPaints,
  eApplyToPaintsKeepingAllInks,
  eApplyToInksAndPaints_NoGap,
  eApplyToInksDeletingAllPaints,
  eApplyToPaintsDeletingAllInks
};

//------------------------------------------------------------------------------

class DVAPI PaletteFilterFxRenderData final : public TRasterFxRenderData {
public:
  bool m_keep;
  FilterType m_type;
  std::set<int> m_colors;

public:
  PaletteFilterFxRenderData();

  float typeIndex() const override {
    return (m_type == eApplyToInksAndPaints) ? 0.5f : 1.0f;
  }

  bool operator==(const TRasterFxRenderData &data) const override;
  std::string toString() const override;
};

//------------------------------------------------------------------------------

void DVAPI insertIndexes(std::vector<std::string> items,
                         PaletteFilterFxRenderData *t);
void DVAPI parseIndexes(std::string indexes, std::vector<std::string> &items);

//**********************************************************************************************
//    SandorFxRenderData  (possible) parameters
//**********************************************************************************************

enum Type { BlendTz, Calligraphic, ArtAtContour, OutBorder };

//------------------------------------------------------------------------------

class DVAPI BlendTzParams {
public:
  std::wstring m_colorIndex;
  bool m_noBlending;
  double m_amount;
  double m_smoothness;
  int m_superSampling;

public:
  BlendTzParams()
      : m_colorIndex(L"")
      , m_noBlending(false)
      , m_amount(0.0)
      , m_smoothness(0.0)
      , m_superSampling(0) {}
};

//------------------------------------------------------------------------------

class DVAPI CalligraphicParams {
public:
  std::wstring m_colorIndex;
  double m_thickness;
  double m_horizontal;
  double m_vertical;
  double m_upWDiagonal;
  double m_doWDiagonal;
  double m_accuracy;
  double m_noise;

public:
  CalligraphicParams()
      : m_thickness(0.0)
      , m_horizontal(0.0)
      , m_vertical(0.0)
      , m_upWDiagonal(0.0)
      , m_doWDiagonal(0.0)
      , m_accuracy(0.0)
      , m_noise(0.0)
      , m_colorIndex(L"") {}
};

//------------------------------------------------------------------------------

class DVAPI ArtAtContourParams {
public:
  double m_maxSize;
  double m_minSize;
  double m_maxOrientation;
  double m_minOrientation;
  bool m_randomness;
  double m_maxDistance;
  double m_minDistance;
  double m_density;
  bool m_keepLine;
  bool m_keepColor;
  bool m_includeAlpha;
  std::wstring m_colorIndex;

public:
  ArtAtContourParams()
      : m_maxSize(0.0)
      , m_minSize(0.0)
      , m_maxOrientation(0.0)
      , m_minOrientation(0.0)
      , m_randomness(false)
      , m_maxDistance(0.0)
      , m_minDistance(0.0)
      , m_density(0.0)
      , m_keepLine(false)
      , m_keepColor(false)
      , m_includeAlpha(true)
      , m_colorIndex(L"") {}
};

//**********************************************************************************************
//    SandorFxRenderData  declaration
//**********************************************************************************************

class DVAPI SandorFxRenderData final : public TRasterFxRenderData {
public:
  Type m_type;
  BlendTzParams m_blendParams;
  CalligraphicParams m_callParams;
  ArtAtContourParams m_contourParams;
  int m_border, m_shrink;
  int m_argc;
  const char *m_argv[12];
  TRectD m_controllerBBox;
  TRasterP m_controller;
  std::string m_controllerAlias;

public:
  SandorFxRenderData(Type type, int argc, const char *argv[], int border,
                     int shrink, const TRectD &controllerBBox = TRectD(),
                     const TRasterP &controller = TRasterP());

  float typeIndex() const override { return (m_type == BlendTz) ? 2.0f : 3.0f; }

  bool operator==(const TRasterFxRenderData &data) const override;
  std::string toString() const override;

  TRectD getBBoxEnlargement(const TRectD &bbox) override;
};

#endif
