#pragma once

#ifndef T_FILL_INCLUDED
#define T_FILL_INCLUDED

class TPalette;

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

class FillParameters {
public:
  int m_styleId;
  std::wstring m_fillType;
  bool m_emptyOnly, m_segment;
  int m_minFillDepth;
  int m_maxFillDepth;
  bool m_shiftFill;
  TPoint m_p;
  TPalette *m_palette;
  bool m_prevailing;

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
      , m_prevailing(true) {}
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
      , m_prevailing(params.m_prevailing) {}
};

//=============================================================================
// forward declarations
class TStroke;
class TTileSaverCM32;
class TTileSaverFullColor;

//=============================================================================

// returns true if the savebox is changed 9tipically, if you fill the bg)
DVAPI bool fill(const TRasterCM32P &r, const FillParameters &params,
                TTileSaverCM32 *saver = 0);

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
  TRect m_bounds;
  Pixel *m_pixels;
  int m_wrap;
  int m_color;

public:
  AreaFiller(const TRasterCM32P &ras);
  ~AreaFiller();
  /*!
Fill \b rect in raster with \b color.
\n If \b fillPaints is false fill only ink in rect;
else if \b fillInks is false fill only paint delimited by ink;
else fill ink and paint in rect.
*/
  void rectFill(const TRect &rect, int color, bool onlyUnfilled,
                bool fillPaints, bool fillInks);

  /*!
Fill the raster region contained in spline \b s with \b color.
\n If  \b fillPaints is false fill only ink in region contained in spline;
else if \b fillInks is false fill only paint delimited by ink;
else fill ink and paint in region contained in spline.
*/
  void strokeFill(TStroke *s, int color, bool onlyUnfilled, bool fillPaints,
                  bool fillInks);
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

#endif
