

#include "trastercm.h"
#include "toonz/fill.h"
#include "tregion.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/toonzimageutils.h"
#include "skeletonlut.h"
#include "tpixelutils.h"
#include "tropcm.h"
#include "tenv.h"
#include <stack>

#include "toonz/preferences.h"

using namespace SkeletonLut;

//-----------------------------------------------------------------------------
namespace {  // Utility Function
//-----------------------------------------------------------------------------
inline TPoint nearestInkNotDiagonal(const TRasterCM32P &r, const TPoint &p) {
  TPixelCM32 *buf = (TPixelCM32 *)r->pixels(p.y) + p.x;

  if (p.x < r->getLx() - 1 && (!(buf + 1)->isPurePaint()))
    return TPoint(p.x + 1, p.y);

  if (p.x > 0 && (!(buf - 1)->isPurePaint())) return TPoint(p.x - 1, p.y);

  if (p.y < r->getLy() - 1 && (!(buf + r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y + 1);

  if (p.y > 0 && (!(buf - r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y - 1);

  return TPoint(-1, -1);
}

void computeSeeds(const TRasterCM32P &r, TStroke *stroke,
                  std::vector<std::pair<TPoint, int>> &seeds) {
  int length = (int)stroke->getLength();
  TRect bbox = r->getBounds();

  TPoint oldP;

  for (int i = 0; i < length; i++) {
    TPoint p = convert(stroke->getPointAtLength(i));
    if (p == oldP || !bbox.contains(p)) continue;
    seeds.push_back(
        std::pair<TPoint, int>(p, (r->pixels(p.y) + p.x)->getPaint()));
    oldP = p;
  }
}

//-----------------------------------------------------------------------------

void fillRegionExcept(const TRasterCM32P &ras, TRegion *r, int positive,
                      int negative, TPalette *plt, int color, bool fillInks,
                      bool fillAllautoPaintLines) {
  TRect bbox = convert(r->getBBox());
  bbox *= ras->getBounds();
  ras->lock();

  for (int i = bbox.y0; i <= bbox.y1; i++) {
    TPixelCM32 *line = ras->pixels(i);
    std::vector<double> intersections;
    r->computeScanlineIntersections(i, intersections);
    assert(!(intersections.size() & 0x1));

    for (UINT j = 0; j < intersections.size(); j += 2) {
      if (intersections[j] == intersections[j + 1]) continue;
      int from        = std::max(tfloor(intersections[j]), bbox.x0);
      int to          = std::min(tceil(intersections[j + 1]), bbox.x1);
      TPixelCM32 *pix = line + from;
      for (int k = from; k < to; k++, pix++) {
        if (fillInks && !pix->isPurePaint()) pix->setInk(color);
        if (pix->getPaint() != positive) pix->setPaint(negative);
        if (!fillInks && plt) {
          if (fillAllautoPaintLines &&
              plt->getStyle(pix->getInk())->getFlags() != 0)
            pix->setInk(color);
          else if (pix->getPaint() == negative && pix->getTone()) {
            TPoint pInk        = nearestInkNotDiagonal(ras, TPoint(k, i));
            TPixelCM32 *inkPix = ras->pixels(pInk.y) + pInk.x;
            int ink            = inkPix->getInk();
            if (pInk != TPoint(-1, -1) && plt->getStyle(ink)->getFlags() != 0)
              inkFill(ras, pInk, color, 0);
          }
        }
      }
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void fillOutsideOfStroke(const TRasterCM32P &r, TStroke *stroke, int color) {
  int length = (int)stroke->getLength();
  TRect bbox = r->getBounds();
  int lx     = r->getLx();
  int ly     = r->getLy();

  TPoint oldP;
  FillParameters params;
  params.m_styleId    = color;
  params.m_prevailing = true;
  for (int i = 0; i < length; i++) {
    TPoint p = convert(stroke->getPointAtLength(i));
    if (p == oldP || !bbox.contains(p) ||
        r->pixels(p.y)[p.x].getPaint() == color)
      continue;
    params.m_p = p;
    fill(r, params);
    oldP = p;
  }
}

//-----------------------------------------------------------------------------

void restoreColors(const TRasterCM32P &r,
                   const std::vector<std::pair<TPoint, int>> &seeds) {
  FillParameters params;
  // in order to make the paint to protlude behind the line
  params.m_prevailing = false;
  for (UINT i = 0; i < seeds.size(); i++) {
    params.m_p       = seeds[i].first;
    params.m_styleId = seeds[i].second;
    fill(r, params);
  }
}

//-----------------------------------------------------------------------------

/*!
  Return true if all \b pixels in \b rect are pure paint; otherwise return
  false.
*/
bool areRectPixelsPurePaint(TPixelCM32 *pixels, TRect rect, int wrap) {
  int dx          = rect.x1 - rect.x0;
  TPixelCM32 *pix = pixels + rect.y0 * wrap + rect.x0;
  int x, y;
  for (y = rect.y0; y <= rect.y1; y++, pix += wrap - dx - 1)
    for (x = rect.x0; x <= rect.x1; x++, pix++)
      if (!pix->isPurePaint()) return false;
  return true;
}

//-----------------------------------------------------------------------------

/*!
  Return true if all \b pixels in \b rect are transparent; otherwise return
  false.
*/
bool areRectPixelsTransparent(TPixel32 *pixels, TRect rect, int wrap) {
  int dx        = rect.x1 - rect.x0;
  TPixel32 *pix = pixels + rect.y0 * wrap + rect.x0;
  int x, y;
  for (y = rect.y0; y <= rect.y1; y++, pix += wrap - dx - 1)
    for (x = rect.x0; x <= rect.x1; x++, pix++)
      if (pix->m <= 0) return false;
  return true;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// AreaFiller

AreaFiller::AreaFiller(const TRasterCM32P &ras, const TRaster32P &ref,
                       TPalette *palette)
    : m_ras(ras)
    , m_bounds(ras->getBounds())
    , m_pixels(ras->pixels())
    , m_wrap(ras->getWrap())
    , m_color(0)
    , m_palette(palette)
    , m_refRas(ref) {
  m_ras->lock();
}

//-----------------------------------------------------------------------------

AreaFiller::~AreaFiller() { m_ras->unlock(); }

//-----------------------------------------------------------------------------
// questa funzione viene chiamata dopo il fill rect delle aree, e colora gli
// inchiostri di tipo "autoink"
// che confinano con le aree appena fillate con il rect. rbefore e' il rect del
// raster prima del rectfill.
void fillautoInks(TRasterCM32P &rin, TRect &rect, const TRasterCM32P &rbefore,
                  TPalette *plt) {
  assert(plt);
  TRasterCM32P r = rin->extract(rect);
  assert(r->getSize() == rbefore->getSize());
  int i, j;

  for (i = 0; i < r->getLy(); i++) {
    TPixelCM32 *pix  = r->pixels(i);
    TPixelCM32 *pixb = rbefore->pixels(i);
    for (j = 0; j < r->getLx(); j++, pix++, pixb++) {
      int paint = pix->getPaint();
      int tone  = pix->getTone();
      int ink   = pix->getInk();
      if (paint != pixb->getPaint() && ink != paint &&
          plt->getStyle(ink)->getFlags() != 0)
        inkFill(rin, TPoint(j, i) + rect.getP00(), paint, 0, NULL, &rect);
    }
  }
}

//-----------------------------------------------------------------------------

bool AreaFiller::rectFill(const TRect &rect, int color, bool onlyUnfilled,
                          bool fillPaints, bool fillInks,
                          bool fillAllautoPaintLines) {
  // Viene trattato il caso fillInks
  /*- In case of FillInk only -*/
  m_ras->lock();
  TRect r = m_bounds * rect;

  int dx = r.x1 - r.x0;
  int dy = (r.y1 - r.y0) * m_wrap;
  if (dx < 2 || dy < 2)  // rect degenere(area contenuta nulla), skippo.
    return false;
  // Se il rettangolo non contiene il bordo del raster e se tutti i pixels
  // contenuti nel rettangolo sono pure paint non deve fare nulla!
  if (!DEF_REGION_WITH_PAINT && !rect.contains(m_bounds) &&
      areRectPixelsPurePaint(m_pixels, r, m_wrap))
    return false;

  TRasterCM32P ras       = m_ras->extract(rect.x0, rect.y0, rect.x1, rect.y1);
  TRasterCM32P backupRas = ras->clone();

  // Put reference image, will be automatically removed by RAII
  RefImageGuard refGuard(m_ras, m_refRas);

  // fill borders with maxPaint
  FillParameters params;
  params.m_prevailing = true;
  params.m_styleId    = TPixelCM32::getMaxPaint();

  int ly = ras->getLy();
  int lx = ras->getLx();

  TPixelCM32 *upPix = ras->pixels(0);
  TPixelCM32 *dnPix = backupRas->pixels(ly - 1);
  bool topSame      = r.y0 == m_bounds.y0;
  bool bottomSame   = r.y1 == m_bounds.y1;
  bool leftSame     = r.x0 == m_bounds.x0;
  bool rightSame    = r.x1 == m_bounds.x1;

  int sameCount =
      Preferences::instance()->getFillOnlySavebox()
          ? (int)topSame + (int)bottomSame + (int)leftSame + (int)rightSame
          : 0;

  // --- Top Edge ---
  if (!(sameCount != 4 && topSame)) {
    TPixelCM32 *upPix = ras->pixels(0);
    for (int x = 1; x < lx - 1; ++x) {
      if (upPix->getPaint() != TPixelCM32::getMaxPaint()) {
        params.m_p = TPoint(x, 0);
        fill(ras, params);
      }
      ++upPix;
    }
  }

  // --- Bottom Edge ---
  if (!(sameCount != 4 && bottomSame)) {
    TPixelCM32 *dnPix = backupRas->pixels(ly - 1);
    for (int x = 1; x < lx - 1; ++x) {
      if (dnPix->getPaint() != TPixelCM32::getMaxPaint()) {
        params.m_p = TPoint(x, ly - 1);
        fill(ras, params);
      }
      ++dnPix;
    }
  }

  // --- Left Edge ---
  if (!(sameCount != 4 && leftSame)) {
    for (int y = 0; y < ly; ++y) {
      TPixelCM32 *pix = ras->pixels(y);
      if (pix->getPaint() != TPixelCM32::getMaxPaint()) {
        params.m_p = TPoint(0, y);
        fill(ras, params);
      }
    }
  }

  // --- Right Edge ---
  if (!(sameCount != 4 && rightSame)) {
    for (int y = 0; y < ly; ++y) {
      TPixelCM32 *pix = ras->pixels(y) + (lx - 1);
      if (pix->getPaint() != TPixelCM32::getMaxPaint()) {
        params.m_p = TPoint(lx - 1, y);
        fill(ras, params);
      }
    }
  }
  removeUnneedReferLines(ras);

  bool defRegionWithPaint     = params.m_defRegionWithPaint;
  bool usePrevailingReferFill = params.m_usePrevailingReferFill;
  for (int y = 0; y < ly; ++y) {
    TPixelCM32 *pix = ras->pixels(y);
    TPixelCM32 *bak = backupRas->pixels(y);
    for (int x = 0; x < lx; ++x, ++pix, ++bak) {
      // paint auto-paint lines
      if (fillInks && !pix->isPurePaint())
        pix->setInk(color);  // Paint all inks
      processPixel(*pix, *bak, true, color, onlyUnfilled, fillPaints, fillInks,
                   defRegionWithPaint, usePrevailingReferFill);
      if (!fillInks && m_palette) {
        if (fillAllautoPaintLines &&
            m_palette->getStyle(pix->getInk())->getFlags() != 0)
          inkSegment(ras, TPoint(x, y), color, 2.5, true);
        else if (pix->getPaint() == color && pix->getTone()) {
          TPoint pInk        = nearestInkNotDiagonal(ras, TPoint(x, y));
          TPixelCM32 *inkPix = ras->pixels(pInk.y) + pInk.x;
          int ink            = inkPix->getInk();
          if (pInk != TPoint(-1, -1) &&
              m_palette->getStyle(ink)->getFlags() != 0)
            inkFill(ras, pInk, color, 0);
        }
      }
    }
  }

  m_ras->unlock();

  return true;
}

// The rectFill must be fast enough for Fill Check
bool AreaFiller::rectFastFill(const TRect &rect, int color) {
  // Viene trattato il caso fillInks
  /*- In case of FillInk only -*/
  m_ras->lock();
  TRect r = m_bounds * rect;

  int dx = r.x1 - r.x0;
  int dy = (r.y1 - r.y0) * m_wrap;
  if (dx < 2 || dy < 2)  // rect degenere(area contenuta nulla), skippo.
    return false;

  TRasterCM32P ras = m_ras->extract(r.x0, r.y0, r.x1, r.y1);

  int ly = ras->getLy();
  int lx = ras->getLx();

  // Get the bound seed
  struct BoundSeed {
    int x, y, paint;
  };
  std::vector<BoundSeed> boundSeeds;
  bool fillTop     = r.y0 != m_bounds.y0;
  bool fillBot     = r.y1 != m_bounds.y1;
  bool fillLft     = r.x0 != m_bounds.x0;
  bool fillRgt     = r.x1 != m_bounds.x1;
  int expectedSize = 0;
  if (fillTop) expectedSize += lx;
  if (fillBot) expectedSize += lx;
  if (fillLft) expectedSize += std::max(0, ly - 2);
  if (fillRgt) expectedSize += std::max(0, ly - 2);
  boundSeeds.reserve(expectedSize);

  // Top edge seed
  if (fillTop) {
    TPixelCM32 *topLine = ras->pixels(0);
    for (int x = 0; x < lx; ++x)
      boundSeeds.push_back({x, 0, topLine[x].getPaint()});
  }

  // Bottom edge seed
  if (fillBot) {
    TPixelCM32 *botLine = ras->pixels(ly - 1);
    for (int x = 0; x < lx; ++x)
      boundSeeds.push_back({x, ly - 1, botLine[x].getPaint()});
  }

  // Left edge seed
  if (fillLft) {
    for (int y = 1; y < ly - 1; ++y) {
      TPixelCM32 *line = ras->pixels(y);
      boundSeeds.push_back({0, y, line[0].getPaint()});
    }
  }

  // Right edge seed
  if (fillRgt) {
    for (int y = 1; y < ly - 1; ++y) {
      TPixelCM32 *line = ras->pixels(y);
      boundSeeds.push_back({lx - 1, y, line[lx - 1].getPaint()});
    }
  }

  // Fill with paint
  for (int y = 0; y < ly; ++y) {
    TPixelCM32 *pix = ras->pixels(y);
    for (int x = 0; x < lx; ++x, ++pix) {
      // do not paint if not empty pixel
      if (pix->getPaint() != 0) continue;
      pix->setPaint(color);
    }
  }

  // Restore paints from seeds
  FillParameters params;
  params.m_prevailing = false;
  for (const auto &b : boundSeeds) {
    // no need to paint painted region
    if (ras->pixels(b.y)[b.x].getPaint() != color) continue;

    params.m_p       = TPoint(b.x, b.y);
    params.m_styleId = b.paint;
    fill(ras, params);
  }

  m_ras->unlock();
  return true;
}

//-----------------------------------------------------------------------------

void AreaFiller::strokeFill(const TRect &rect, TStroke *stroke, int color,
                            const bool onlyUnfilled, const bool fillPaints,
                            const bool fillInks, bool fillAllautoPaintLines) {
  TRect box  = rect;
  TRect bbox = m_ras->getBounds();
  box *= bbox;
  if (color == 0) m_palette = 0;

  assert(!box.isEmpty());
  TRasterCM32P ras       = m_ras->extract(box);
  TRasterCM32P backupRas = ras->clone();

  // Put reference image, will be automatically removed by RAII
  RefImageGuard refGuard(m_ras, m_refRas);

  bool defRegionWithPaint     = DEF_REGION_WITH_PAINT;
  bool usePrevailingReferFill = USE_PREVAILING_REFER_FILL;
  bool refFill                = m_refRas.getPointer() != nullptr;

  // std::vector<std::pair<TPoint, int>> seeds;
  // computeSeeds(m_ras, stroke, seeds);
  fillOutsideOfStroke(ras, stroke, TPixelCM32::getMaxPaint() - 1);
  // Some pixels might be painted to in fillRow(),
  // with PAINT Bounds off and refer/gap on
  int lx = box.getLx(), ly = box.getLy();

  // Restore those lines painted by fillRow() here
  if (!defRegionWithPaint && refFill) {
    TPixelCM32 *pix  = ras->pixels(0);
    TPixelCM32 *pix1 = ras->pixels(ly - 1) + lx;
    for (int y = 0; y < ly; ++y) {
      TPixelCM32 *pix = ras->pixels(y);
      for (int x = 0; x < lx; ++x, ++pix) {
        if (pix->getInk() == TPixelCM32::getMaxInk() - 1) {
          pix->setInk(TPixelCM32::getMaxInk());
        }
      }
    }
  }

  TVectorImage app;
  app.addStroke(stroke);
  app.findRegions();
  // fillRegionsExcept() also process lines

  // Before the refer lines be painted, remove unnecessary lines
  if (refFill && fillInks) {
    for (UINT i = 0; i < app.getRegionCount(); i++)
      fillRegionExcept(ras, app.getRegion(i), TPixelCM32::getMaxPaint() - 1,
                       TPixelCM32::getMaxPaint(), 0, color, false, false);
    removeUnneedReferLines(ras);
  }
  // Paint Lines and Areas
  for (UINT i = 0; i < app.getRegionCount(); i++)
    fillRegionExcept(ras, app.getRegion(i), TPixelCM32::getMaxPaint() - 1,
                     TPixelCM32::getMaxPaint(), m_palette, color, fillInks,
                     fillAllautoPaintLines);
  app.removeStroke(0);

  if (!fillInks && refFill) removeUnneedReferLines(ras);

  for (int y = 0; y < ly; ++y) {
    TPixelCM32 *pix = ras->pixels(y);
    TPixelCM32 *bak = backupRas->pixels(y);
    for (int x = 0; x < lx; ++x, ++pix, ++bak) {
      processPixel(*pix, *bak, false, color, onlyUnfilled, fillPaints, fillInks,
                   defRegionWithPaint, usePrevailingReferFill);
    }
  }
  m_ras->unlock();
}

const void AreaFiller::processPixel(TPixelCM32 &pix, const TPixelCM32 &bak,
                                    bool invert, int color, bool onlyUnfilled,
                                    bool fillPaints, bool fillInks,
                                    bool defRegionWithPaint,
                                    bool usePrevailingReferFill) {
  /*--- Process Area need to be painted */
  if (invert ? pix.getPaint() != TPixelCM32::getMaxPaint()
             : pix.getPaint() == TPixelCM32::getMaxPaint()) {
    /*--- Process refer Lines  ---*/
    // paint refer lines
    if (pix.getInk() == TPixelCM32::getMaxInk() && !defRegionWithPaint)
      pix.setInk(color);

    /*--- Process ALL PAINT  ---*/
    if (fillPaints) {
      // do not paint if not empty pixel
      if (onlyUnfilled && bak.getPaint() != 0) pix.setPaint(bak.getPaint());
      // do not paint under refer pure INK
      else if (!usePrevailingReferFill &&
               pix.getInk() == TPixelCM32::getMaxInk() && pix.isPureInk())
        pix.setPaint(bak.getPaint());
      // restore paint under maxPaint
      else
        pix.setPaint(color);
    }
    // Restore Area should not be filled
    // !fillPaints and pix not inked case
    else
      pix.setPaint(bak.getPaint());
  }
  /*--- Restore Area should not be filled */
  else
    pix.setPaint(bak.getPaint());
}

// Mainly two kinds of lines:
// 1. Lines inside the closed region
// 2. Lines outside the closed region
const void AreaFiller::removeUnneedReferLines(const TRasterCM32P &ras) {
  const int maxInk  = TPixelCM32::getMaxInk();
  const int maxTone = TPixelCM32::getMaxTone();
  const int wrap    = m_wrap;
  int lx            = ras->getLx();
  int ly            = ras->getLy();
  if (lx < 3 || ly < 3) return;
  TPixelCM32 *pix = ras->pixels(0);

  for (int y = 1; y < ly - 1; y++) {
    TPixelCM32 *pix    = ras->pixels(y) + 1;
    TPixelCM32 *rowEnd = pix + (lx - 2);
    while (pix < rowEnd) {
      if (pix->getInk() != maxInk) {
        pix++;
        continue;
      }

      TPixelCM32 *left  = pix - 1;
      TPixelCM32 *right = pix + 1;
      TPixelCM32 *up    = pix + wrap;
      TPixelCM32 *down  = pix - wrap;

      int leftPaint  = left->getPaint();
      int rightPaint = right->getPaint();
      int upPaint    = up->getPaint();
      int downPaint  = down->getPaint();

      int leftInk  = left->getInk();
      int rightInk = right->getInk();
      int upInk    = up->getInk();
      int downInk  = down->getInk();

      bool leftIsMax  = (left->getInk() == maxInk);
      bool rightIsMax = (right->getInk() == maxInk);
      bool upIsMax    = (up->getInk() == maxInk);
      bool downIsMax  = (down->getInk() == maxInk);

      unsigned char mask =
          (leftIsMax << 3) | (rightIsMax << 2) | (upIsMax << 1) | downIsMax;

      switch (mask) {
        // 2 Inks and some 3 Inks
      case 0b0001:
      case 0b0010:
      case 0b0011:
        if (leftPaint == rightPaint) {
          *pix = TPixelCM32(0, leftPaint, maxTone);
        }
        break;

      case 0b0100:
      case 0b1000:
      case 0b1100:
        if (upPaint == downPaint) {
          *pix = TPixelCM32(0, upPaint, maxTone);
        }
        break;

        // 1 INK
      case 0b0000:
        if (leftPaint == rightPaint) {
          *pix = TPixelCM32(0, leftPaint, maxTone);
        } else if (upPaint == downPaint) {
          *pix = TPixelCM32(0, upPaint, maxTone);
        }
        break;

        // 3 INKs
      case 0b1001:
        *pix = TPixelCM32(0, leftPaint, maxTone);
        break;
      case 0b0101:
        *pix = TPixelCM32(0, upPaint, maxTone);
        break;
      case 0b1010:
        *pix = TPixelCM32(0, leftPaint, maxTone);
        break;
      case 0b0110:
        *pix = TPixelCM32(0, upPaint, maxTone);
        break;

        // 4 INKs
      case 0b0111:
        *pix = TPixelCM32(0, leftPaint, maxTone);
        break;
      case 0b1011:
        *pix = TPixelCM32(0, rightPaint, maxTone);
        break;
      case 0b1101:
        *pix = TPixelCM32(0, upPaint, maxTone);
        break;
      case 0b1110:
        *pix = TPixelCM32(0, downPaint, maxTone);
        break;

      default:
        assert(false);
        continue;
      }

      pix++;
    }
  }
  return;
}

//=============================================================================
// FullColorAreaFiller

FullColorAreaFiller::FullColorAreaFiller(const TRaster32P &ras)
    : m_ras(ras)
    , m_bounds(ras->getBounds())
    , m_pixels(ras->pixels())
    , m_wrap(ras->getWrap())
    , m_color(0) {
  m_ras->lock();
}

//-----------------------------------------------------------------------------

FullColorAreaFiller::~FullColorAreaFiller() { m_ras->unlock(); }

//-----------------------------------------------------------------------------

void FullColorAreaFiller::rectFill(const TRect &rect,
                                   const FillParameters &params,
                                   bool onlyUnfilled) {
  TRect bbox = m_ras->getBounds();
  TRect r    = rect * bbox;
  if (r.isEmpty()) return;

  TRaster32P workRas = m_ras->extract(r);
  TRaster32P copy    = workRas->clone();

  TPixel32 color = params.m_palette->getStyle(params.m_styleId)->getMainColor();

  // Fillo tutto il quadaratino con color
  int x, y;
  for (y = 0; y < workRas->getLy(); y++) {
    TPixel32 *line = workRas->pixels(y);
    for (x = 0; x < workRas->getLx(); x++)
      *(line + x) = overPix(color, workRas->pixels(y)[x]);
  }

  FillParameters paramsApp = params;
  TPixel32 refColor;
  for (y = 0; y < workRas->getLy(); y++) {
    paramsApp.m_p = TPoint(0, y);
    if (y == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
  for (y = 0; y < workRas->getLy(); y++) {
    paramsApp.m_p = TPoint(workRas->getLx() - 1, y);
    if (y == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }

  for (x = 0; x < workRas->getLx(); x++) {
    paramsApp.m_p = TPoint(x, 0);
    if (x == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
  for (x = 0; x < workRas->getLx(); x++) {
    paramsApp.m_p = TPoint(x, workRas->getLy() - 1);
    if (x == 0 ||
        refColor != workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x]) {
      fill(workRas, copy, paramsApp);
      refColor = workRas->pixels(paramsApp.m_p.y)[paramsApp.m_p.x];
    }
  }
}

//=============================================================================
// InkSegmenter

const int damInk = TPixelCM32::getMaxInk() - 1;

//-----------------------------------------------------------------------------

#define GROW_FACTOR 2.51

//-----------------------------------------------------------------------------

class InkSegmenter {
  int m_lx, m_ly, m_wrap;
  int m_displaceVector[8];
  TPixelCM32 *m_buf;
  TRect m_bBox;
  TRasterCM32P m_r;
  TTileSaverCM32 *m_saver;
  float m_growFactor;

public:
  InkSegmenter(const TRasterCM32P &r, float growFactor, TTileSaverCM32 *saver)
      : m_r(r)
      , m_lx(r->getLx())
      , m_ly(r->getLy())
      , m_wrap(r->getWrap())
      , m_buf((TPixelCM32 *)r->getRawData())
      , m_bBox(r->getBounds())
      , m_saver(saver)
      , m_growFactor(growFactor) {
    m_displaceVector[0] = -m_wrap - 1;
    m_displaceVector[1] = -m_wrap;
    m_displaceVector[2] = -m_wrap + 1;
    m_displaceVector[3] = -1;
    m_displaceVector[4] = +1;
    m_displaceVector[5] = m_wrap - 1;
    m_displaceVector[6] = m_wrap;
    m_displaceVector[7] = m_wrap + 1;
  }

  //-----------------------------------------------------------------------------

  bool compute(const TPoint &pin, int ink, bool isSelective) {
    TPixelCM32 *pix;
    int distance;
    TPixelCM32 *master;
    TPoint mp, sp;
    TPixelCM32 *slave;
    TPixelCM32 *d11, *d12, *d21, *d22;
    TPoint d1p1, d1p2, d2p1, d2p2;
    TPoint p = pin;

    if (!m_bBox.contains(p)) return false;

    if ((m_buf + p.y * m_wrap + p.x)->isPurePaint() &&
        ((p = nearestInk(p, 2)) == TPoint(-1, -1)))
      return false;

    pix = m_buf + p.y * m_wrap + p.x;

    /*-- If the same ink is used, RETURN --*/
    if (pix->getInk() == ink) return false;

    if (!ConnectionTable[neighboursCode(pix, p)]) {
      master = slave = pix;
      mp = sp  = p;
      distance = 0;
    } else
      distance = findTwinPoints(pix, p, master, mp, slave, sp);

    if (distance == -1) return false;

    if (!findDam(master, mp, slave, sp, distance, d11, d1p1, d12, d1p2))
      d11 = d12 = d21 = d22 = 0;
    else
      findDamRev(master, mp, slave, sp, distance, d21, d2p1, d22, d2p2);

    // vector<pair<TPixelCM32*, int> > oldInks;

    drawSegment(d1p1, d1p2, damInk, m_saver);
    drawSegment(d2p1, d2p2, damInk, m_saver);

    inkSegmentFill(p, ink, isSelective, m_saver);

    // UINT i;

    drawSegment(d1p1, d1p2, ink, m_saver);
    drawSegment(d2p1, d2p2, ink, m_saver);

    /*	for (i=0; i<oldInks.size(); i++)
    (oldInks[i].first)->setInk(ink);*/

    return true;
  }

private:
  void drawSegment(
      const TPoint &p0, const TPoint &p1, int ink,
      /*vector<pair<TPixelCM32*, int> >& oldInks,*/ TTileSaverCM32 *saver);

  int findTwinPoints(TPixelCM32 *pix, const TPoint &p, TPixelCM32 *&master,
                     TPoint &mp, TPixelCM32 *&slave, TPoint &sp);
  int searchForNearestSlave(TPixelCM32 *pix1, TPixelCM32 *pix2,
                            const TPoint &p1, TPoint &p2, TPixelCM32 *&slave,
                            TPoint &sp);
  int rearrangePoints(TPixelCM32 *&master, TPoint &mp, TPixelCM32 *&slave,
                      int s_prewalker, TPoint &sp, int walk);
  int rearrangePointsRev(TPixelCM32 *&master, TPoint &mp, TPixelCM32 *&slave,
                         int s_prewalker, TPoint &sp, int walk);
  int dragSlave(TPoint mp, TPixelCM32 *&slave, int &s_prewalker, TPoint &sp);
  int dragSlaveRev(TPoint mp, TPixelCM32 *&slave, int &s_prewalker, TPoint &sp,
                   TPixelCM32 *first_slave);
  bool findDam(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave, TPoint sp,
               int distance, TPixelCM32 *&d11, TPoint &d1p1, TPixelCM32 *&d12,
               TPoint &d1p2);
  void findDamRev(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave, TPoint sp,
                  int distance, TPixelCM32 *&d11, TPoint &d1p1,
                  TPixelCM32 *&d12, TPoint &d1p2);
  int nextPointIsGoodRev(TPoint mp, TPoint sp, TPixelCM32 *slave,
                         int s_prewalker, int distance);
  int nextPointIsGood(TPoint mp, TPoint sp, TPixelCM32 *slave, int s_prewalker,
                      int distance);
  void inkSegmentFill(const TPoint &p, int ink, bool isSelective,
                      TTileSaverCM32 *saver);
  TPoint nearestInk(const TPoint &p, int ray);
  inline int stepReversed(TPixelCM32 *walker, int prewalker, int &distance,
                          const TPoint &p1, TPoint &p2);
  inline int stepForward(TPixelCM32 *walker, int prewalker, int &distance,
                         const TPoint &p1, TPoint &p2);

  TPixelCM32 *ePix(TPixelCM32 *br) { return (br + 1); }
  TPixelCM32 *wPix(TPixelCM32 *br) { return (br - 1); }
  TPixelCM32 *nPix(TPixelCM32 *br) { return (br + m_wrap); }
  TPixelCM32 *sPix(TPixelCM32 *br) { return (br - m_wrap); }
  TPixelCM32 *swPix(TPixelCM32 *br) { return (br - m_wrap - 1); }
  TPixelCM32 *nwPix(TPixelCM32 *br) { return (br + m_wrap - 1); }
  TPixelCM32 *nePix(TPixelCM32 *br) { return (br + m_wrap + 1); }
  TPixelCM32 *sePix(TPixelCM32 *br) { return (br - m_wrap + 1); }

  UCHAR neighboursCode(TPixelCM32 *seed, const TPoint &p) {
    // assert(p == TPoint((seed-m_buf)%m_wrap, (seed-m_buf)/m_wrap));
    bool w = (p.x > 0), e = (p.x < m_lx - 1), s = (p.y > 0),
         n = (p.y < m_ly - 1);

    return (((s && w) ? ((!swPix(seed)->isPurePaint())) : 0) |
            ((s) ? ((!sPix(seed)->isPurePaint()) << 1) : 0) |
            ((s && e) ? ((!sePix(seed)->isPurePaint()) << 2) : 0) |
            ((w) ? ((!wPix(seed)->isPurePaint()) << 3) : 0) |
            ((e) ? ((!ePix(seed)->isPurePaint()) << 4) : 0) |
            ((n && w) ? ((!nwPix(seed)->isPurePaint()) << 5) : 0) |
            ((n) ? ((!nPix(seed)->isPurePaint()) << 6) : 0) |
            ((n && e) ? ((!nePix(seed)->isPurePaint()) << 7) : 0));
  }
};

//-----------------------------------------------------------------------------

#define DRAW_SEGMENT(a, b, da, db, istr1, istr2, block)                        \
  {                                                                            \
    d      = 2 * db - da;                                                      \
    incr_1 = 2 * db;                                                           \
    incr_2 = 2 * (db - da);                                                    \
    while (a < da) {                                                           \
      if (d <= 0) {                                                            \
        d += incr_1;                                                           \
        a++;                                                                   \
        istr1;                                                                 \
      } else {                                                                 \
        d += incr_2;                                                           \
        a++;                                                                   \
        b++;                                                                   \
        istr2;                                                                 \
      }                                                                        \
      block;                                                                   \
    }                                                                          \
  }
#define SET_INK                                                                \
  {                                                                            \
    if (saver) saver->save(TPoint(x1 + x, y1 + y));                            \
    /*if (buf->getInk()!=damInk)*/                                             \
    /*  oldInks.push_back(pair<TPixelCM32*, int>(buf, buf->getInk()));*/       \
    buf->setInk(ink);                                                          \
  }

//-----------------------------------------------------------------------------

void InkSegmenter::drawSegment(
    const TPoint &p0, const TPoint &p1, int ink,
    /*vector<pair<TPixelCM32*, int> >& oldInks,*/ TTileSaverCM32 *saver) {
  int x, y, dx, dy, d, incr_1, incr_2;

  int x1 = p0.x;
  int y1 = p0.y;
  int x2 = p1.x;
  int y2 = p1.y;

  if (x1 > x2) {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }

  TPixelCM32 *buf = m_r->pixels() + y1 * m_wrap + x1;
  /*if (buf->getInk()!=damInk)
          oldInks.push_back(pair<TPixelCM32*, int>(buf, buf->getInk()));
  if ((m_r->pixels() + y2*m_wrap + x2)->getInk()!=damInk)
          oldInks.push_back(pair<TPixelCM32*, int>(m_r->pixels() + y2*m_wrap +
  x2, (m_r->pixels() + y2*m_wrap + x2)->getInk()));*/

  if (saver) {
    saver->save(p0);
    saver->save(p1);
  }

  buf->setInk(ink);
  (m_r->pixels() + y2 * m_wrap + x2)->setInk(ink);

  dx = x2 - x1;
  dy = y2 - y1;

  x = y = 0;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf += m_wrap + 1), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf += m_wrap), (buf += m_wrap + 1), SET_INK)
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf -= (m_wrap - 1)), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf -= m_wrap), (buf -= (m_wrap - 1)),
                   SET_INK)
  }
}

//-----------------------------------------------------------------------------

void InkSegmenter::inkSegmentFill(const TPoint &p, int ink, bool isSelective,
                                  TTileSaverCM32 *saver) {
  int x = p.x, y = p.y;
  int lx             = m_r->getLx();
  int ly             = m_r->getLy();
  TPixelCM32 *pixels = (TPixelCM32 *)m_r->getRawData();
  TPixelCM32 *pix    = pixels + p.y * m_wrap + x;
  int oldInk;

  if (pix->isPurePaint() || pix->getInk() == ink) return;

  if (isSelective) oldInk = pix->getInk();

  std::stack<TPoint> seeds;
  seeds.push(p);

  while (!seeds.empty()) {
    TPoint seed = seeds.top();
    seeds.pop();
    // if(!m_r->getBounds().contains(seed)) continue;
    x               = seed.x;
    y               = seed.y;
    TPixelCM32 *pix = pixels + (y * m_wrap + x);
    if (pix->isPurePaint() || pix->getInk() == ink || pix->getInk() == damInk ||
        (isSelective && pix->getInk() != oldInk))
      continue;

    if (saver) saver->save(seed);

    pix->setInk(ink);

    if (x > 0) seeds.push(TPoint(x - 1, y));
    if (y > 0) seeds.push(TPoint(x, y - 1));
    if (y < ly - 1) seeds.push(TPoint(x, y + 1));
    if (x < lx - 1) seeds.push(TPoint(x + 1, y));

    if (x == lx - 1 || x == 0 || y == ly - 1 || y == 0) continue;

    if (ePix(pix)->getInk() == damInk || wPix(pix)->getInk() == damInk ||
        sPix(pix)->getInk() == damInk || nPix(pix)->getInk() == damInk ||
        nePix(pix)->getInk() == damInk || sePix(pix)->getInk() == damInk ||
        swPix(pix)->getInk() == damInk || nwPix(pix)->getInk() == damInk)
      continue;

    seeds.push(TPoint(x - 1, y - 1));
    seeds.push(TPoint(x - 1, y + 1));
    seeds.push(TPoint(x + 1, y - 1));
    seeds.push(TPoint(x + 1, y + 1));
  }
}

//-----------------------------------------------------------------------------

TPoint InkSegmenter::nearestInk(const TPoint &p, int ray) {
  int i, j;

  for (j = std::max(p.y - ray, 0); j <= std::min(p.y + ray, m_ly - 1); j++)
    for (i = std::max(p.x - ray, 0); i <= std::min(p.x + ray, m_lx - 1); i++)
      if (!(m_buf + j * m_wrap + i)->isPurePaint()) return TPoint(i, j);

  return TPoint(-1, -1);
}

//-----------------------------------------------------------------------------

int InkSegmenter::findTwinPoints(TPixelCM32 *pix, const TPoint &p,
                                 TPixelCM32 *&master, TPoint &mp,
                                 TPixelCM32 *&slave, TPoint &sp) {
  TPixelCM32 *row_p1, *col_p1, *row_p2, *col_p2;
  int distance;
  int row_x1, row_x2, col_y1, col_y2;

  row_p1 = pix - 1;
  row_x1 = p.x - 1;

  while (row_x1 + 1 < m_lx && !(row_p1 + 1)->isPurePaint()) {
    row_p1++;
    row_x1++;
  }

  row_p2 = pix + 1;
  row_x2 = p.x + 1;

  while (row_x2 - 1 > 0 && !(row_p2 - 1)->isPurePaint()) {
    row_p2--;
    row_x2--;
  }

  master = row_p1;
  mp.x   = row_x1;
  mp.y   = p.y;

  col_p1 = pix - m_wrap;
  col_y1 = p.y - 1;

  while (col_y1 + 1 < m_ly && !(col_p1 + m_wrap)->isPurePaint()) {
    col_p1 += m_wrap;
    col_y1++;
  }

  col_p2 = pix + m_wrap;
  col_y2 = p.y + 1;

  while (col_y2 - 1 > 0 && !(col_p2 - 1)->isPurePaint()) {
    col_p2 -= m_wrap;
    col_y2--;
  }

  if (row_x1 - row_x2 <= col_y1 - col_y2) {
    master = row_p1;
    mp     = TPoint(row_x1, p.y);

    TPoint auxp(row_x2, p.y);
    if ((distance =
             searchForNearestSlave(row_p1, row_p2, mp, auxp, slave, sp)) != 0)
      return distance;

    master = col_p1;
    mp     = TPoint(p.x, col_y1);

    auxp = TPoint(p.x, col_y2);
    if ((distance = searchForNearestSlave(col_p1, col_p2, mp, auxp, slave,
                                          sp)) == 0 /*&& !is_connecting(p1)*/)
      return -1;
  } else {
    master = col_p1;
    mp     = TPoint(p.x, col_y1);

    TPoint auxp(p.x, col_y2);
    if ((distance =
             searchForNearestSlave(col_p1, col_p2, mp, auxp, slave, sp)) != 0)
      return distance;

    master = row_p1;
    mp     = TPoint(row_x1, p.y);
    auxp   = TPoint(row_x2, p.y);
    if ((distance = searchForNearestSlave(row_p1, row_p2, mp, auxp, slave,
                                          sp)) == 0 /*&& !is_connecting(p1)*/)
      return -1;
  }

  return distance;
}

//-----------------------------------------------------------------------------

inline void newP(int next, TPoint &p) {
  switch (next) {
  case 0:
  case 3:
  case 5:
    p.x -= 1;
    break;
  case 2:
  case 4:
  case 7:
    p.x += 1;
    break;
  }

  switch (next) {
  case 0:
  case 1:
  case 2:
    p.y -= 1;
    break;
  case 5:
  case 6:
  case 7:
    p.y += 1;
    break;
  }
}

//-----------------------------------------------------------------------------

inline int InkSegmenter::stepReversed(TPixelCM32 *walker, int prewalker,
                                      int &distance, const TPoint &p1,
                                      TPoint &p2) {
  int next = NextPointTableRev[(neighboursCode(walker, p2) << 3) | prewalker];
  newP(next, p2);
  distance = norm2(p1 - p2);
  return next;
}

/*------------------------------------------------------------------------*/

inline int InkSegmenter::stepForward(TPixelCM32 *walker, int prewalker,
                                     int &distance, const TPoint &p1,
                                     TPoint &p2) {
  int next = NextPointTable[(neighboursCode(walker, p2) << 3) | prewalker];
  newP(next, p2);
  distance = norm2(p1 - p2);
  return next;
}

//-----------------------------------------------------------------------------

int InkSegmenter::searchForNearestSlave(TPixelCM32 *pix1, TPixelCM32 *pix2,
                                        const TPoint &p1, TPoint &p2,
                                        TPixelCM32 *&slave, TPoint &sp) {
  int curr_distance, new_distance;
  UCHAR prewalker, next;
  TPixelCM32 *walker;
  TPoint currp2;

  currp2 = p2;

  curr_distance = norm2(p1 - p2);
  walker        = pix2;
  slave         = pix2;
  sp            = p2;

  prewalker = FirstPreseedTable[neighboursCode(walker, p2)];

  next = stepForward(walker, prewalker, new_distance, p1, p2);

  if (curr_distance != 0 && new_distance < curr_distance) {
    while (p2.x > 0 && p2.x < m_lx - 1 && p2.y > 0 && p2.y < m_ly - 1 &&
           new_distance < curr_distance && new_distance != 0) {
      curr_distance = new_distance;
      sp.x          = p2.x;
      sp.y          = p2.y;
      walker        = walker + m_displaceVector[next];
      slave         = walker;
      prewalker     = (~next) & 0x7;
      next          = stepForward(walker, prewalker, new_distance, p1, p2);
    }
    if (new_distance != 0) return curr_distance;
  }

  curr_distance = norm2(p1 - p2);
  walker        = pix2;
  p2            = currp2;

  UCHAR code = neighboursCode(walker, p2);
  next       = FirstPreseedTable[code];
  next       = NextPointTable[(code << 3) | next];
  prewalker  = next;

  next = stepReversed(walker, prewalker, new_distance, p1, p2);

  if (p2.x > 0 && p2.x < m_lx - 1 && p2.y > 0 && p2.y < m_ly - 1 &&
      curr_distance != 0 && new_distance < curr_distance) {
    while (new_distance < curr_distance && new_distance > 0) {
      curr_distance = new_distance;
      sp            = p2;

      walker = walker + m_displaceVector[next];
      slave  = walker;

      prewalker = (~next) & 0x7;
      next      = stepReversed(walker, prewalker, new_distance, p1, p2);
    }
    if (new_distance != 0) return curr_distance;
  } else if (new_distance != 0)
    return curr_distance;
  return 0;
}

//-----------------------------------------------------------------------------

int InkSegmenter::rearrangePoints(TPixelCM32 *&master, TPoint &mp,
                                  TPixelCM32 *&slave, int s_prewalker,
                                  TPoint &sp, int walk) {
  int s_next;

  while (walk-- && sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1) {
    s_next =
        NextPointTableRev[((neighboursCode(slave, sp)) << 3) | s_prewalker];

    newP(s_next, sp);

    slave       = slave + m_displaceVector[s_next];
    s_prewalker = (~s_next) & 0x7;
  }
  return 1;
}

//-----------------------------------------------------------------------------

int InkSegmenter::rearrangePointsRev(TPixelCM32 *&master, TPoint &mp,
                                     TPixelCM32 *&slave, int s_prewalker,
                                     TPoint &sp, int walk) {
  int s_next;

  while (walk-- && sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1) {
    s_next = NextPointTable[((neighboursCode(slave, sp)) << 3) | s_prewalker];
    // s_next = NEXT_POINT_24(*slave, s_prewalker);

    newP(s_next, sp);

    slave       = slave + m_displaceVector[s_next];
    s_prewalker = (~s_next) & 0x7;
  }
  return 1;
}

//-----------------------------------------------------------------------------

int InkSegmenter::dragSlave(TPoint mp, TPixelCM32 *&slave, int &s_prewalker,
                            TPoint &sp) {
  int distance, s_next, new_distance;
  int ret = 0;

  distance = norm2(mp - sp);

  s_next = stepForward(slave, s_prewalker, new_distance, mp, sp);

  while (sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1 &&
         (new_distance < distance ||
          nextPointIsGood(mp, sp, slave + m_displaceVector[s_next],
                          (~s_next) & 0x7, distance))) {
    if (!ret) ret = 1;
    distance    = new_distance;
    slave       = slave + m_displaceVector[s_next];
    s_prewalker = (~s_next) & 0x7;
    s_next      = stepForward(slave, s_prewalker, new_distance, mp, sp);
  }

  newP(((~s_next) & 0x7), sp);
  return ret;
}

//-----------------------------------------------------------------------------

int InkSegmenter::dragSlaveRev(TPoint mp, TPixelCM32 *&slave, int &s_prewalker,
                               TPoint &sp, TPixelCM32 *first_slave) {
  int distance, new_distance, s_next;
  int ret = 0;

  distance = norm2(mp - sp);

  s_next = stepReversed(slave, s_prewalker, new_distance, mp, sp);

  while (sp.x > 0 && sp.x < m_lx - 1 && sp.y > 0 && sp.y < m_ly - 1 &&
         (new_distance < distance ||
          nextPointIsGoodRev(mp, sp, slave + m_displaceVector[s_next],
                             (~s_next) & 0x7, distance))) {
    if (!ret) ret = 1;
    distance = new_distance;
    slave    = slave + m_displaceVector[s_next];
    if (slave == first_slave) return -1;

    s_prewalker = (~s_next) & 0x7;
    s_next      = stepReversed(slave, s_prewalker, new_distance, mp, sp);
  }

  newP(((~s_next) & 0x7), sp);
  return ret;
}

//-----------------------------------------------------------------------------

bool InkSegmenter::findDam(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave,
                           TPoint sp, int distance, TPixelCM32 *&d11,
                           TPoint &d1p1, TPixelCM32 *&d12, TPoint &d1p2)

{
  int ref_distance, m_prewalker, s_prewalker, m_next, next, ret;
  unsigned int walkalone = 0;
  TPixelCM32 *first_slave, *first_master;

  first_slave  = slave;
  first_master = master;

  ref_distance = tround(m_growFactor * ((float)distance + 1));

  m_prewalker = FirstPreseedTable[neighboursCode(master, mp)];

  if (!ConnectionTable[neighboursCode(master, mp)])
    s_prewalker = FirstPreseedTableRev[neighboursCode(slave, sp)];
  else {
    UCHAR code  = neighboursCode(slave, sp);
    next        = FirstPreseedTable[code];
    next        = NextPointTable[(code << 3) | next];
    s_prewalker = next;
  }

  while (mp.x > 0 && mp.x < m_lx - 1 && mp.y > 0 && mp.y < m_ly - 1 &&
         distance < ref_distance &&
         !(((m_next = NextPointTable[((neighboursCode(master, mp)) << 3) |
                                     m_prewalker]) == s_prewalker) &&
           master == slave)) {
    newP(m_next, mp);
    master      = master + m_displaceVector[m_next];
    m_prewalker = (~m_next) & 0x7;

    ret = dragSlaveRev(mp, slave, s_prewalker, sp, first_slave);

    if (ret == -1) return false;

    if (ret == 0)
      walkalone++;
    else
      walkalone = 0;

    if (master == first_master) break;
    distance = norm2(mp - sp);
  }

  if (walkalone > 0)
    rearrangePoints(master, mp, slave, s_prewalker, sp, walkalone);

  d11  = master;
  d1p1 = mp;
  d12  = slave;
  d1p2 = sp;
  return 1;
}

//-----------------------------------------------------------------------------

void InkSegmenter::findDamRev(TPixelCM32 *master, TPoint mp, TPixelCM32 *slave,
                              TPoint sp, int distance, TPixelCM32 *&d11,
                              TPoint &d1p1, TPixelCM32 *&d12, TPoint &d1p2)

{
  int ref_distance, m_prewalker, s_prewalker, m_next, next;
  unsigned int walkalone = 0;
  TPixelCM32 *first_master;

  first_master = master;

  ref_distance = tround(GROW_FACTOR * ((float)distance + 1));

  m_prewalker = FirstPreseedTableRev[neighboursCode(master, mp)];

  if (!ConnectionTable[neighboursCode(master, mp)]) {
    UCHAR code  = neighboursCode(slave, sp);
    next        = FirstPreseedTableRev[code];
    next        = NextPointTableRev[(code << 3) | next];
    s_prewalker = next;
  } else
    s_prewalker = FirstPreseedTable[neighboursCode(slave, sp)];

  while (mp.x > 0 && mp.x < m_lx - 1 && mp.y > 0 && mp.y < m_ly - 1 &&
         distance < ref_distance &&
         !(((m_next = NextPointTableRev[((neighboursCode(master, mp)) << 3) |
                                        m_prewalker]) == s_prewalker) &&
           master == slave)) {
    newP(m_next, mp);
    master      = master + m_displaceVector[m_next];
    m_prewalker = (~m_next) & 0x7;
    if (!dragSlave(mp, slave, s_prewalker, sp))
      walkalone++;
    else
      walkalone = 0;
    if (master == first_master) break;
    distance = norm2(mp - sp);
  }

  if (walkalone > 0)
    rearrangePointsRev(master, mp, slave, s_prewalker, sp, walkalone);

  d11  = master;
  d1p1 = mp;
  d12  = slave;
  d1p2 = sp;
}

//-----------------------------------------------------------------------------

int InkSegmenter::nextPointIsGood(TPoint mp, TPoint sp, TPixelCM32 *slave,
                                  int s_prewalker, int distance) {
  int s_next;

  s_next = NextPointTable[((neighboursCode(slave, sp)) << 3) | s_prewalker];

  newP(s_next, sp);

  return (norm2(mp - sp) <= distance);
}

//-----------------------------------------------------------------------------

int InkSegmenter::nextPointIsGoodRev(TPoint mp, TPoint sp, TPixelCM32 *slave,
                                     int s_prewalker, int distance) {
  int s_next;

  s_next = NextPointTableRev[((neighboursCode(slave, sp)) << 3) | s_prewalker];

  newP(s_next, sp);

  return (norm2(mp - sp) <= distance);
}

//-----------------------------------------------------------------------------

bool inkSegment(const TRasterCM32P &r, const TPoint &p, int ink,
                float growFactor, bool isSelective, TTileSaverCM32 *saver) {
  r->lock();
  InkSegmenter is(r, growFactor, saver);
  bool ret = is.compute(p, ink, isSelective);
  r->unlock();
  return ret;
}
