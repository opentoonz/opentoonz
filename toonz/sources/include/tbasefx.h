#pragma once

#ifndef TBASEFX_INCLUDED
#define TBASEFX_INCLUDED

// TnzBase includes
#include "trasterfx.h"
#include "tzeraryfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//************************************************************************
//    TBaseRasterFx  definition
//************************************************************************

//! Defines built-in Toonz fxs.
class DVAPI TBaseRasterFx : public TRasterFx {
public:
  std::string getPluginId() const override { return "Base"; }
};

//************************************************************************
//    TBaseZeraryFx  definition
//************************************************************************

//! Defines built-in Toonz zerary fxs.
class DVAPI TBaseZeraryFx : public TZeraryFx {
public:
  std::string getPluginId() const override { return "Base"; }
};

//******************************************************************************
//    ColumnColorFilterFx  declaration
//******************************************************************************

class ColumnColorFilterFx final : public TBaseRasterFx {
  FX_DECLARATION(ColumnColorFilterFx)
  TPixel32 m_colorFilter;

protected:
  TRasterFxPort m_port;

public:
  ColumnColorFilterFx();

  ~ColumnColorFilterFx(){};

  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }

  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;

  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  void setColorFilter(TPixel32 color) { m_colorFilter = color; }

  std::string getAlias(double frame,
                       const TRenderSettings &info) const override;
};

#endif  // TBASEFX_INCLUDED
