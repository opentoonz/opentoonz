#pragma once

#ifndef TREGIONPROP_H
#define TREGIONPROP_H

#include "tgeometry.h"
#include "tregionoutline.h"
#include "tsimplecolorstyles.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//============================================================================//

// forward declarations

class TColorStyle;
class TRegion;
class TVectorRenderData;
class TInputStreamInterface;
class TOutputStreamInterface;

template <class T>
class TRasterPT;
class TPixelRGBM32;

typedef TPixelRGBM32 TPixel32;
typedef TRasterPT<TPixel32> TRaster32P;

//============================================================================//
//                                TRegionProp //
//============================================================================//

class TRegionProp {
  const TRegion *const m_region;

protected:
  bool m_regionChanged;
  int m_styleVersionNumber;

public:
  TRegionProp(const TRegion *region);

  virtual ~TRegionProp() {}

  //! Note: update internal data if isRegionChanged()
  virtual void draw(const TVectorRenderData &rd) = 0;

  const TRegion *getRegion() const { return m_region; }

  virtual const TColorStyle *getColorStyle() const = 0;

  virtual void notifyRegionChange() { m_regionChanged = true; }

  virtual TRegionProp *clone(const TRegion *region) const = 0;

private:
  // not implemented
  TRegionProp(const TRegionProp &);
  TRegionProp &operator=(const TRegionProp &);
};

//-------------------------------------------------------------------

class OutlineRegionProp final : public TRegionProp {
  double m_pixelSize;
  TOutlineStyleP m_colorStyle;

  TRegionOutline m_outline;

  //-------------------------------------------------------------------

  void computeRegionOutline();

public:
  OutlineRegionProp(const TRegion *region, const TOutlineStyleP regionStyle);

  void draw(const TVectorRenderData &rd) override;

  const TColorStyle *getColorStyle() const override;

  TRegionProp *clone(const TRegion *region) const override;
};

#endif
