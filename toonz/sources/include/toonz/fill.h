#pragma once

#ifndef T_FILL_INCLUDED
#define T_FILL_INCLUDED

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <set>
#include "ttilesaver.h"
#include "timage.h"
#include "tpalette.h"
#include "tgeometry.h"
#include "tpixelcm.h"
#include "traster.h"
#include "trastercm.h"
#include "tropcm.h"

#include "preferences.h"
#define DEF_REGION_WITH_PAINT                                                  \
  Preferences::instance()->getBoolValue(PreferencesItemId::DefRegionWithPaint)
#define USE_PREVAILING_REFER_FILL                                              \
  Preferences::instance()->getBoolValue(PreferencesItemId::ReferFillPrevailing)

class TPalette;

class FillParameters {
public:
  int m_styleId;
  std::wstring m_fillType;
  bool m_emptyOnly, m_segment;
  int m_minFillDepth;
  int m_maxFillDepth;
  bool m_shiftFill;
  TPoint m_p;
  TPalette *m_palette;  // Whether to fill autoPaint Ink
  bool m_prevailing;
  bool m_defRegionWithPaint;
  bool m_usePrevailingReferFill;

  FillParameters()
      : m_styleId(0)
      , m_fillType()
      , m_emptyOnly(false)
      , m_segment(false)
      , m_minFillDepth(0)
      , m_maxFillDepth(0)
      , m_p()
      , m_shiftFill(false)
      , m_palette(0)
      , m_prevailing(true)
      , m_defRegionWithPaint(true)
      , m_usePrevailingReferFill(false) {
    m_defRegionWithPaint     = DEF_REGION_WITH_PAINT;
    m_usePrevailingReferFill = USE_PREVAILING_REFER_FILL;
  }
  FillParameters(const FillParameters &params)
      : m_styleId(params.m_styleId)
      , m_fillType(params.m_fillType)
      , m_emptyOnly(params.m_emptyOnly)
      , m_segment(params.m_segment)
      , m_minFillDepth(params.m_minFillDepth)
      , m_maxFillDepth(params.m_maxFillDepth)
      , m_p(params.m_p)
      , m_shiftFill(params.m_shiftFill)
      , m_palette(params.m_palette)
      , m_prevailing(params.m_prevailing)
      , m_defRegionWithPaint(params.m_defRegionWithPaint)
      , m_usePrevailingReferFill(params.m_usePrevailingReferFill) {}
};

//=============================================================================
// forward declarations
class TStroke;
class TTileSaverCM32;
class TTileSaverFullColor;

//=============================================================================

// returns true if the savebox is changed typically, if you fill the bg)
DVAPI bool fill(const TRasterCM32P &r, const FillParameters &params,
                TTileSaverCM32 *saver = 0,
                const TRaster32P &ref = TRaster32P());

DVAPI void fill(const TRaster32P &ras, const TRaster32P &ref,
                const FillParameters &params, TTileSaverFullColor *saver = 0);

DVAPI void inkFill(const TRasterCM32P &r, const TPoint &p, int ink,
                   int searchRay, TTileSaverCM32 *saver = 0,
                   TRect *insideRect = 0);

bool DVAPI inkSegment(const TRasterCM32P &r, const TPoint &p, int ink,
                      float growFactor, bool isSelective,
                      TTileSaverCM32 *saver = 0);

void DVAPI rectFillInk(const TRasterCM32P &ras, const TRect &r, int color);

void DVAPI fillautoInks(TRasterCM32P &r, TRect &rect,
                        const TRasterCM32P &rbefore, TPalette *plt);

void DVAPI fullColorFill(const TRaster32P &ras, const FillParameters &params,
                         TTileSaverFullColor *saver = 0);

void DVAPI fillHoles(const TRasterCM32P &ras, const int size,
                     TTileSaverCM32 *saver = nullptr);

//=============================================================================
//! The class AreaFiller allows to fill a raster area, delimited by rect or
//! spline.
/*!The class provides two methods: one to fill a rect, rectFill(), one to fill a
   spline, defined by \b TStroke, strokeFill(), in a raster \b TRasterCM32P.
*/
//=============================================================================

class DVAPI AreaFiller {
  typedef TPixelCM32 Pixel;
  TRasterCM32P m_ras;
  TRaster32P m_refRas;
  TRect m_bounds;
  Pixel *m_pixels;
  TPalette *m_palette;
  int m_wrap;
  int m_color;

public:
  AreaFiller(const TRasterCM32P &ras, const TRaster32P &ref = TRaster32P(),
             TPalette *palette = nullptr);
  ~AreaFiller();
  /*!
Fill \b rect in raster with \b color.
\n If \b fillPaints is false fill only ink in rect;
else if \b fillInks is false fill only paint delimited by ink;
else fill ink and paint in rect.
*/
  bool rectFill(const TRect &rect, int color, bool onlyUnfilled,
                bool fillPaints, bool fillInks, bool fillAllautoPaintLines = false);

  // Only for Fill Check
  bool rectFastFill(const TRect &rect, int color);

  /*!
Fill the raster region contained in spline \b s with \b color.
\n If  \b fillPaints is false fill only ink in region contained in spline;
else if \b fillInks is false fill only paint delimited by ink;
else fill ink and paint in region contained in spline.
*/
  void strokeFill(const TRect &rect, TStroke *s, int color, bool onlyUnfilled,
                  bool fillPaints, bool fillInks, bool fillAllautoPaintLines);

private:
  const void processPixel(TPixelCM32 &pix, const TPixelCM32 &bak, bool invert,
                          int color, bool onlyUnfilled, bool fillPaints,
                          bool fillInks, bool defRegionWithPaint,
                          bool usePrevailingReferFill);
  const void removeUnneedReferLines(const TRasterCM32P &ras);
};

class DVAPI FullColorAreaFiller {
  TRaster32P m_ras;
  TRect m_bounds;
  TPixel32 *m_pixels;
  int m_wrap;
  int m_color;

public:
  FullColorAreaFiller(const TRaster32P &ras);
  ~FullColorAreaFiller();
  /*!
Fill \b rect in raster with \b color.
\n If \b fillPaints is false fill only ink in rect;
else if \b fillInks is false fill only paint delimited by ink;
else fill ink and paint in rect.
*/
  void rectFill(const TRect &rect, const FillParameters &params,
                bool onlyUnfilled);
};

class RefImageGuard {
  const TRasterCM32P &m_r;
  bool m_refPlaced;

public:
  RefImageGuard(const TRasterCM32P &raster, const TRaster32P &Ref)
      : m_r(raster), m_refPlaced(Ref.getPointer() != nullptr) {
    m_r->lock();
    if (m_refPlaced) TRop::putRefImage(const_cast<TRasterCM32P &>(m_r), Ref);
  }

  ~RefImageGuard() {
    m_r->unlock();
    if (m_refPlaced) TRop::eraseRefInks(const_cast<TRasterCM32P &>(m_r));
  }
};

#endif
