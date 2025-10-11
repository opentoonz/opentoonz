

#include <stack>

#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include "trastercm.h"
#include "tropcm.h"

#ifdef _DEBUG
#ifdef _WIN32
#include <Qdebug>
#endif
#endif

//-----------------------------------------------------------------------------
namespace {  // Utility Function

//-----------------------------------------------------------------------------

inline int threshTone(const TPixelCM32 &pix, int fillDepth) {
  if (fillDepth == TPixelCM32::getMaxTone())
    return pix.getTone();
  else
    return ((pix.getTone()) > fillDepth) ? TPixelCM32::getMaxTone()
                                         : pix.getTone();
}

inline int adjustFillDepth(const int &fillDepth) {
  assert(fillDepth >= 0 && fillDepth < 16);

  switch (TPixelCM32::getMaxTone()) {
  case 15:
    return 15 - fillDepth;
  case 255:
    return ((15 - fillDepth) << 4) | (15 - fillDepth);
  default:
    assert(false);
    return -1;
  }
}

inline int threshMatte(int matte, int fillDepth) {
  if (fillDepth == 255)
    return matte;
  else
    return (matte < fillDepth) ? 255 : matte;
}

#ifdef _DEBUG
#ifdef _WIN32
void checkRow(const TRasterCM32P &ras, int xa, int xb, int y) {
  TPixelCM32 *pix0  = ras->pixels(y) + xa;
  TPixelCM32 *limit = ras->pixels(y) + xb;
  QDebug dbg        = qDebug().noquote().nospace();
  dbg << xa << "," << xb << "," << y << "\n";
  for (; pix0 <= limit; pix0++) {
    dbg << "{" << pix0->getInk() << ","
        << pix0->getPaint()
        //<< "," << pix0->getTone()
        << "} ";
  }
}
#endif
#endif

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

// dal punto x,y si espande a destra e a sinistra.
// la riga ridisegnata va da *xa a *xb compresi
// x1 <= *xa <= *xb <= x2
// N.B. se non viene disegnato neanche un pixel *xa>*xb
//
// "prevailing" is set to false on revert-filling the border of
// region in the Rectangular, Freehand and Polyline fill procedures
// in order to make the paint to protlude behind the line.

void fillRow(const TRasterCM32P &r, const TPoint &p, int &xa, int &xb,
             int paint, TPalette *palette, TTileSaverCM32 *saver,
             bool prevailing = true, int paintAtClickPos = 0,
             bool defRegionWithPaint     = true,
             bool usePrevailingReferFill = false) {
  int tone, oldtone;
  TPixelCM32 *pix, *pix0, *limit, *tmp_limit;

  /* vai a destra */
  TPixelCM32 *line = r->pixels(p.y);

  pix0    = line + p.x;
  pix     = pix0;
  limit   = line + r->getBounds().x1;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (; pix <= limit; pix++) {
    if (pix->getPaint() == paint) break;
    tone = pix->getTone();
    if (tone == 0 || (pix->getPaint() != paintAtClickPos && defRegionWithPaint))
      break;
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
      // not-yet-colored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        break;
      while (pix != pix0) {
        // iterate back in order to leave the pixel with the lowest tone
        // unpainted
        pix--;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone())
              continue;
          }
          break;
        }
      }
      pix++;
      break;
    }
    oldtone = tone;
  }
  if (prevailing && tone == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    int preVailingInk,
        oldPrevailingInk = 0;  // avoid prevailing different Ink styles
    for (; pix <= limit; pix++) {
      if (pix->getPaint() == paint) break;
      if (pix->getTone() != 0) break;
      if (prevailing) {
        preVailingInk = pix->getInk();
        if (oldPrevailingInk > 0 && preVailingInk != oldPrevailingInk) break;
        oldPrevailingInk = preVailingInk;
      }
    }
  }

  xb = p.x + pix - pix0 - 1;

  /* vai a sinistra */

  pix     = pix0;
  limit   = line + r->getBounds().x0;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (pix--; pix >= limit; pix--) {
    if (pix->getPaint() == paint) break;
    tone = pix->getTone();
    if (tone == 0 || pix->getPaint() != paintAtClickPos && defRegionWithPaint)
      break;
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
      // not-yet-colored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        break;
      while (pix != pix0) {
        // iterate forward in order to leave the pixel with the lowest tone
        // unpainted
        pix++;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone())
              continue;
          }
          break;
        }
      }
      pix--;
      break;
    }
    oldtone = tone;
  }
  if (prevailing && tone == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    int preVailingInk, oldPrevailingInk = 0;
    for (; pix >= limit; pix--) {
      if (pix->getPaint() == paint) break;
      if (pix->getTone() != 0) break;
      if (prevailing) {
        preVailingInk = pix->getInk();
        if (oldPrevailingInk > 0 && preVailingInk != oldPrevailingInk) break;
        oldPrevailingInk = preVailingInk;
      }
    }
  }

  xa = p.x + pix - pix0 + 1;

  if (saver) saver->save(TRect(xa, p.y, xb, p.y));
  if (xb >= xa) {
    pix = line + xa;
    int n;
    TPixelCM32 *lastPix = pix;
    for (n = 0; n < xb - xa + 1; n++, lastPix = pix, pix++) {
      /*--- Check if out of range ---*/
      if (pix->getPaint() != paintAtClickPos && defRegionWithPaint) continue;

      /*--- Paint refer lines   ---*/
      if (lastPix->getInk() != paint && !defRegionWithPaint) {
        if (pix->getInk() == TPixelCM32::getMaxInk() &&
            lastPix->getInk() != TPixelCM32::getMaxInk() && pix->isPureInk()) {
          pix->setInk(paint);
          if (usePrevailingReferFill) pix->setPaint(paint);
          break;
        } else if (pix->getInk() != TPixelCM32::getMaxInk() &&
                   lastPix->getInk() == TPixelCM32::getMaxInk() &&
                   lastPix->isPureInk()) {
          lastPix->setInk(paint);
          if (usePrevailingReferFill) lastPix->setPaint(paint);
        } else if (usePrevailingReferFill &&
                   pix->getInk() == TPixelCM32::getMaxInk() && pix->isPureInk())
          continue;
      }

      /*--- Paint auto-paint lines   ---*/
      if (palette && pix->isPurePaint()) {
        TPoint pInk = nearestInkNotDiagonal(r, TPoint(xa + n, p.y));
        if (pInk != TPoint(-1, -1)) {
          TPixelCM32 *pixInk =
              (TPixelCM32 *)r->getRawData() + (pInk.y * r->getWrap() + pInk.x);
          if (pixInk->getInk() != paint &&
              palette->getStyle(pixInk->getInk())->getFlags() != 0)
            inkFill(r, pInk, paint, 0, saver);
          else if (pixInk->getInk() == paintAtClickPos &&
                   paintAtClickPos != 1 && paint != 0)
            inkFill(r, pInk, paint, 0, saver);
        }
      }

      /*--- Do the normal paint ---*/
      if (pix->getInk() == TPixelCM32::getMaxInk() && !usePrevailingReferFill &&
          pix->isPureInk())
        continue;
      pix->setPaint(paint);
    }

    // Make sure the Surround ref Ink Pixels can be painted
    if (p.y > 0 && p.y < r->getLy() - 1 && !defRegionWithPaint) {
      pix = line + xa;
      for (n = 0; n < xb - xa + 1; n++, pix++) {
        if (pix->isPurePaint()) {
          TPixelCM32 *upPix = pix - r->getWrap();
          TPixelCM32 *dnPix = pix + r->getWrap();
          if (upPix->getInk() == TPixelCM32::getMaxInk()) {
            upPix->setInk(paint);
            if (usePrevailingReferFill) upPix->setPaint(paint);
          }
          if (dnPix->getInk() == TPixelCM32::getMaxInk()) {
            dnPix->setInk(paint);
            if (usePrevailingReferFill) dnPix->setPaint(paint);
          }
        }
      }
    }
  }
}

struct extendSeed {
  UINT xa;  // Left Pixel of Normal Style
  UINT xb;
  UINT xc;  // Left Pixel of Auto-Paint Style
  UINT xd;
  UINT y;  // Row highth
};
//-----------------------
// This function getLine with a given direction
// and starts from a autoPainted pixel,
// ends with another Ink pixel.
// Prevailing is on for default
// Called by extendInk2InkFill
void getRowInk2Ink(const TRasterCM32P &r, const TPoint &p, extendSeed &seed,
                   bool right, int paint, int &length, TTileSaverCM32 *saver) {
  int dx                  = right ? 1 : -1;
  const int maxAutoPaints = 50;

  TPixelCM32 *pix0 = r->pixels(p.y) + p.x;
  assert(pix0->getInk() == paint);
  if (pix0->getInk() != paint) return;
  int tone, oldTone;

  // Calculate in autoPaint Style area
  TPixelCM32 *pix;
  TPixelCM32 *tmp_limit;

  pix       = pix0;
  tmp_limit = (right ? r->pixels(p.y) + r->getBounds().x0
                     : r->pixels(p.y) + r->getBounds().x1 - 1);
  oldTone   = pix->getTone();
  int j     = 0;
  while ((pix)->getInk() == paint) {
    tone = pix->getTone();
    if (tone > oldTone) break;
    if (j > maxAutoPaints) break;
    pix -= dx;
    oldTone = tone;
    ++j;
  }
  seed.xc = pix - r->pixels(p.y) + dx;

  pix       = pix0;
  tmp_limit = (right ? r->pixels(p.y) + r->getBounds().x1 - 1
                     : r->pixels(p.y) + r->getBounds().x0);
  while (pix->getInk() == paint && pix != tmp_limit) {
    pix += dx;
  }
  seed.xd = pix - r->pixels(p.y) - dx;

  if (seed.xc > seed.xd) std::swap(seed.xc, seed.xd);

  // Out of autoPaint Ink Style area
  pix       = r->pixels(p.y) + (right ? seed.xd : seed.xc) + dx;
  tmp_limit = (right ? r->pixels(p.y) + r->getBounds().x1 - 1
                     : r->pixels(p.y) + r->getBounds().x0);

  // int paintAtClickedPos = pix->getPaint();
  bool outOfPaint = false, outOfInk = false;

  int i   = 0;
  j       = 0;
  oldTone = pix->getTone();
  while (pix != tmp_limit) {
    tone = pix->getTone();
    if (!pix->isPurePaint()) outOfPaint = true;
    if (outOfPaint && tone > oldTone) outOfInk = true;
    if (outOfInk) break;
    pix += dx;
    oldTone = tone;
    if (!outOfPaint && tone == TPixelCM32::getMaxTone())
      ++i;
    else if (tone == 0)
      ++j;
    if (i > length) break;
    if (j > maxAutoPaints) break;
  }

  pix -= dx;
  seed.xa = seed.xc, seed.xb = seed.xd;
  (right ? seed.xb : seed.xa) = pix - r->pixels(p.y);

  if (i > length)
    length -= 2;
  else
    length = (int)((seed.xb - seed.xa)) + 2;
}

void extendInk2InkFill(const TRasterCM32P &r, const TPoint &p, bool right,
                       int dy, int paint, TTileSaverCM32 *saver,
                       int maxLength = 6) {
  int xx = p.x;
  int yy = p.y;
  int xc = xx, xd = yy;
  assert((r->pixels(yy) + xx)->getInk() == paint);

  TPixelCM32 *pix;
  std::vector<extendSeed> seeds;
  auto extendAndFill = [&](const extendSeed &seed) {
    if (saver) saver->save(TRect(seed.xa, seed.y, seed.xb, seed.y));

    TPixelCM32 *pix = r->pixels(seed.y) + seed.xa;
    TPixelCM32 *end = r->pixels(seed.y) + seed.xb;
    for (; pix <= end; ++pix) {
      if (!USE_PREVAILING_REFER_FILL &&
          pix->getInk() == TPixelCM32::getMaxInk() && pix->getTone() == 0)
        continue;
      pix->setPaint(paint);
    }
  };
  auto isLineClosed = [&](const extendSeed &seed) -> bool {
    TPixelCM32 *pix = r->pixels(seed.y) + seed.xa;
    TPixelCM32 *end = r->pixels(seed.y) + seed.xb;
    for (; pix <= end; ++pix) {
      if (pix->isPurePaint() && pix->getPaint() != paint) return false;
    }
    return true;
  };
  auto checkIfClosed = [&](const extendSeed &seed) -> bool {
    // Check if leak
    int tone, oldTone;
    if (right) {
      tone = (r->pixels(yy - dy) + seed.xb)->getTone();
      if (tone == TPixelCM32::getMaxTone()) return false;
      for (int x = seed.xb; x > seed.xd; x--) {
        oldTone = tone;
        tone    = (r->pixels(yy - dy) + x)->getTone();
        if (tone <= oldTone) continue;
        if (tone < (r->pixels(yy) + x)->getTone()) {
          return false;
        }
      }
    } else {
      tone = (r->pixels(yy - dy) + seed.xa)->getTone();
      if (tone == TPixelCM32::getMaxTone()) return false;
      for (int x = seed.xa; x > seed.xc; x++) {
        oldTone = tone;
        tone    = (r->pixels(yy - dy) + x)->getTone();
        if (tone <= oldTone) continue;
        if (tone < (r->pixels(yy) + x)->getTone()) {
          return false;
        }
      }
    }
    return true;
  };

  bool areaClosed = false;
  while (maxLength > 0) {
    if (xc < 0 || xd > r->getLx()) break;
    if (xx < 0 || xx > r->getLx()) break;
    extendSeed seed;
    seed.y = yy;
    getRowInk2Ink(r, TPoint(xx, yy), seed, right, paint, maxLength, saver);
    if (seed.xd - seed.xc + 1 > maxLength) break;
    seeds.push_back(seed);
    if (isLineClosed(seed)) areaClosed = true;
    if (areaClosed) {
      for (const extendSeed &s : seeds) extendAndFill(s);
      seeds.clear();
      areaClosed = false;
    }
    yy += dy;
    if (yy < 0 || yy >= r->getLy()) break;
    int xc = seed.xc, xd = seed.xd;
    if (right) {
      for (--xc, ++xd; xc < xd; xd--) {
        pix = r->pixels(yy) + xd;
        if (pix->getInk() == paint && !pix->isPurePaint()) break;
      }
      xx = xd;
    } else {
      for (--xc, ++xd; xc < xd; xc++) {
        pix = r->pixels(yy) + xc;
        if (pix->getInk() == paint && !pix->isPurePaint()) break;
      }
      xx = xc;
    }

    if ((r->pixels(yy) + xx)->getInk() != paint) {
      if (checkIfClosed(seed)) {
        for (const extendSeed &s : seeds) extendAndFill(s);
        seeds.clear();
      }
      return;
    }
  }
}

// Prevailing is off for default
// Return false if the area is too big to fill
bool extendNormalFill(const TRasterCM32P &r, const TPoint &p, bool right,
                      int dy, int paint, int paintAtClickedPos,
                      TTileSaverCM32 *saver, const int maxCount = 8) {
  struct locals {
    static bool hasValidNeighbors(const TRasterCM32P &r, const int x,
                                  const int y, const int paint) {
      int fourCount      = 0;
      int eightCount     = 0;
      int purePaintCount = 0;
      TRect bounds       = r->getBounds();

      const int dx4[] = {0, -1, 1, 0};
      const int dy4[] = {-1, 0, 0, 1};

      const int dx8[] = {-1, -1, 1, 1};
      const int dy8[] = {-1, 1, -1, 1};

      int selfTone = (r->pixels(y) + x)->getTone();

      for (int i = 0; i < 4; ++i) {
        int nx = x + dx4[i], ny = y + dy4[i];
        if (bounds.contains(TPoint(nx, ny))) {
          TPixelCM32 *neighbor = r->pixels(ny) + nx;
          int neighbortone     = neighbor->getTone();
          if (neighbortone <= selfTone || neighbor->getPaint() == paint)
            ++fourCount;
          if (neighbortone == TPixelCM32::getMaxTone()) purePaintCount++;
        }
      }
      if (purePaintCount >= 3) return false;

      for (int i = 0; i < 4; ++i) {
        int nx = x + dx8[i], ny = y + dy8[i];
        if (bounds.contains(TPoint(nx, ny))) {
          TPixelCM32 *neighbor = r->pixels(ny) + nx;
          if (neighbor->getTone() <= selfTone) ++eightCount;
        }
      }
      return (fourCount >= 3) || (fourCount == 2 && eightCount >= 3);
    }
  };
  TPixelCM32 *pixel = r->pixels(p.y) + p.x;
  if (pixel->getTone() < 24) return true;
  int pixelCount          = 0;
  const int MaxCountOfRow = maxCount < 16 ? maxCount : 6;
  int x, y;
  int dx = right ? 1 : -1;
  std::vector<TPoint> points;
  std::vector<TPoint> seeds;

  points.reserve(maxCount + 4);

  seeds.push_back(p);

  TRect bounds     = r->getBounds();
  TPoint backPoint = p + TPoint(-dx, p.y);
  if (bounds.contains(backPoint))
    if ((pixel - dx)->getTone() > pixel->getTone()) return false;

  while (!seeds.empty()) {
    int rowPixelCount = 0;
    bool seedAdded    = false;
    TPoint point      = seeds.back();
    x                 = point.x;
    y                 = point.y;
    seeds.pop_back();
    if (!bounds.contains(TPoint(x, y))) continue;
    if (!locals::hasValidNeighbors(r, x, y, paint)) continue;
    pixel = r->pixels(y) + x;
    int tone, oldtone;
    tone          = pixel->getTone();
    oldtone       = 0;
    bool toneDown = false;
    int pixPaint  = pixel->getPaint();
    while (tone != 0 && (pixPaint == paintAtClickedPos ||
                         pixPaint == paint && !DEF_REGION_WITH_PAINT)) {
      points.push_back(TPoint(x, y));
      ++pixelCount;
      if (pixel->isPurePaint()) ++rowPixelCount;
      if (rowPixelCount > MaxCountOfRow || pixelCount > maxCount) return false;
      if (!seedAdded && bounds.contains(TPoint(x, y + dy)) &&
          !(r->pixels(y + dy) + x)->isPureInk()) {
        seeds.push_back(TPoint(x, y + dy));
        seedAdded = true;
      }
      if (x < 0 || x > r->getLx() - 1) break;
      oldtone = tone;
      pixel += dx;
      x += dx;
      tone = pixel->getTone();
      if (tone < oldtone && !toneDown) toneDown = true;
      if (toneDown && tone > oldtone) {
        pixel -= dx;
        x -= dx;
        points.pop_back();
        break;
      }
    }
  }
  if (points.empty()) return true;
  /*for (const TPoint &point : points)
    if (!locals::hasValidNeighbors(r, point.x, point.y, paint))
        return false;*/

  for (const TPoint &point : points) {
    if (!locals::hasValidNeighbors(r, point.x, point.y, paint)) return false;
    if (saver) saver->save(point);
    (r->pixels(point.y) + point.x)->setPaint(paint);
  }

  TPoint lastPoint = points.back();
  int currentY     = lastPoint.y;
  int targetY      = lastPoint.y + dy;
  TPoint leftPoint = TPoint(lastPoint.x + dx, targetY);

  if (!bounds.contains(leftPoint)) return false;

  int oldTone = 0;

  for (auto it = points.rbegin(); it != points.rend(); ++it) {
    if (it->y != currentY) break;

    int tone = r->pixels(targetY)[it->x].getTone();
    if (tone == 0) break;
    if (tone <= oldTone) leftPoint = TPoint(it->x, targetY);

    oldTone = tone;
  }

  if (bounds.contains(TPoint(leftPoint.x - dx, leftPoint.y)))
    if (r->pixels(leftPoint.y)[leftPoint.x - dx].getTone() >
        r->pixels(leftPoint.y)[leftPoint.x].getTone())
      leftPoint.x += dx;
  // leftPoint = TPoint(lastPoint.x + dx, targetY);
  extendNormalFill(r, leftPoint, right, dy, paint, paintAtClickedPos, saver,
                   maxCount);

  return true;
}

//-----------------------------------------------------------------------------

void extendFill(int paint, int paintAtClickedPos, int xc, int xd, int y, int dy,
                const TRasterCM32P &r, const FillParameters &params,
                TTileSaverCM32 *saver) {
  const int oldy = y - dy;

  // In case the autoPaint Line is already painted
  if (xd + 1 < r->getLx()) {
    TPixelCM32 *pix = r->pixels(oldy) + xd + 1;
    if (pix->getTone() <= TPixelCM32::getMaxTone() &&
        pix->getPaint() == paint && pix->getInk() == paint) {
      xd++;
    }
  }

  if (xc - 1 >= 0) {
    TPixelCM32 *pix = r->pixels(oldy) + xc - 1;
    if (pix->getTone() <= TPixelCM32::getMaxTone() &&
        pix->getPaint() == paint && pix->getInk() == paint) {
      xc--;
    }
  }

  int firstTone = (r->pixels(oldy) + xc)->getTone();
  for (TPixelCM32 *pix = r->pixels(oldy) + xc + 1; pix <= r->pixels(oldy) + xd;
       ++pix) {
    if (pix->getTone() != firstTone) break;
    if (pix == r->pixels(oldy) + xd) return;
  }

  // limit the area to prevailing pixels
  int newxc = xd, newxd = xc;
  TPixelCM32 *leftPix, *rightPix;

  leftPix  = r->pixels(oldy) + newxc;
  rightPix = r->pixels(oldy) + newxd;

  for (int oldTone = rightPix->getTone();
       newxd < xd && rightPix->getTone() >= oldTone;
       oldTone = rightPix->getTone(), rightPix++, newxd++) {
  }
  for (int oldTone = leftPix->getTone();
       newxc > xc && leftPix->getTone() >= oldTone;
       oldTone = leftPix->getTone(), leftPix--, newxc--) {
  }

  if (newxc > newxd) {
    std::swap(newxc, newxd);
    std::swap(leftPix, rightPix);
  }

#ifdef _STARTER_DEBUG
  for (TPixelCM32 *pix = r->pixels(oldy) + xc; pix <= r->pixels(oldy) + xd;
       ++pix) {
    pix->setInk(8);
    pix->setTone(0);
  }
  saver->save(TRect(newxc, y, newxd, y));
  for (TPixelCM32 *pix = r->pixels(y) + newxc; pix <= r->pixels(y) + newxd;
       ++pix) {
    pix->setInk(6);
    pix->setTone(0);
  }
  return;
#endif

  int leftStyle, rightStyle;
  leftStyle  = leftPix->getInk();
  rightStyle = rightPix->getInk();

  // Extend Ink with paint style + Ink fill
  {
    bool fillRight = leftStyle == paint && rightStyle != paint;
    bool fillLeft  = rightStyle == paint && leftStyle != paint;

    TPixelCM32 *pixel;
    pixel = leftPix + dy * r->getLy();
    int maxCount;
    if (!params.m_shiftFill)
      maxCount = xd - xc > 8 ? xd - xc : 8;
    else
      maxCount = params.m_maxFillDepth;
    if (fillRight) {
      extendInk2InkFill(r, TPoint(newxc, oldy), true, dy, paint, saver,
                        maxCount);
    } else if (fillLeft) {
      extendInk2InkFill(r, TPoint(newxd, oldy), false, dy, paint, saver,
                        maxCount);
    }
  }

  // extend normal fill
  {
    bool doExpendNormalFill =
        newxd - newxc > 1 && leftStyle != paint && rightStyle != paint;
    if (doExpendNormalFill) {
      auto isRightPaintdLess = [&](int xR, int xL, int y, int stepY,
                                   int paint) {
        int h      = r->getLy();
        int countR = 0, countL = 0;
        int yR = y + stepY, yL = y + stepY;

        const int maxSteps = 32;
        int step           = 0;

        while (yR >= 0 && yR < h && yL >= 0 && yL < h && step < maxSteps) {
          bool rPainted = (r->pixels(yR)[xR].getPaint() == paint);
          bool lPainted = (r->pixels(yL)[xL].getPaint() == paint);
#ifdef _NORMAL_EXTEND_DEBUG
          if (rPainted) {
            r->pixels(yR)[xR].setInk(6);
            r->pixels(yR)[xR].setTone(0);
          }
          if (lPainted) {
            r->pixels(yL)[xL].setInk(6);
            r->pixels(yL)[xL].setTone(0);
          }
#endif
          if (rPainted) {
            ++countR;
            yR += stepY;
          }

          if (lPainted) {
            ++countL;
            yL += stepY;
          }

          if (countR != countL || rPainted != lPainted) break;

          ++step;
        }
        if (countR == countL) return 0;
        return countR < countL ? 1 : 2;
      };

      int maxCount =
          params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
      if (maxCount < 6) maxCount = 6;
      if (!leftPix->isPureInk() || !rightPix->isPureInk()) maxCount *= 25;

      //  0:unknown, 1:right, 2:left
      int direction = isRightPaintdLess(newxd, newxc, oldy, -dy, paint);
      switch (direction) {
      case 0:
      case 1:
        extendNormalFill(r, TPoint(newxd, y), true, dy, paint,
                         paintAtClickedPos, saver, maxCount);
        if (direction != 0) break;
      case 2:
        extendNormalFill(r, TPoint(newxc, y), false, dy, paint,
                         paintAtClickedPos, saver, maxCount);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void findSegment(const TRaster32P &r, const TPoint &p, int &xa, int &xb,
                 const TPixel32 &color, const int fillDepth = 254) {
  int matte, oldmatte;
  TPixel32 *pix, *pix0, *limit, *tmp_limit;

  /* vai a destra */
  TPixel32 *line = r->pixels(p.y);

  pix0     = line + p.x;
  pix      = pix0;
  limit    = line + r->getBounds().x1;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix <= limit; pix++) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xb = p.x + pix - pix0 - 1;

  /* vai a sinistra */
  pix      = pix0;
  limit    = line + r->getBounds().x0;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix >= limit; pix--) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xa = p.x + pix - pix0 + 1;
}

//-----------------------------------------------------------------------------
// Used when the clicked pixel is solid or semi-transparent.
// Check if the fill is stemmed at the target pixel.
// Note that RGB values are used for checking the difference, not Alpha value.

bool doesStemFill(const TPixel32 &clickColor, const TPixel32 *targetPix,
                  const int fillDepth2) {
  // stop if the target pixel is transparent
  if (targetPix->m == 0) return true;
  // check difference of RGB values is larger than fillDepth
  int dr = (int)clickColor.r - (int)targetPix->r;
  int dg = (int)clickColor.g - (int)targetPix->g;
  int db = (int)clickColor.b - (int)targetPix->b;
  return (dr * dr + dg * dg + db * db) >
         fillDepth2;  // condition for "stem" the fill
}

//-----------------------------------------------------------------------------

void fullColorFindSegment(const TRaster32P &r, const TPoint &p, int &xa,
                          int &xb, const TPixel32 &color,
                          const TPixel32 &clickedPosColor,
                          const int fillDepth) {
  if (clickedPosColor.m == 0) {
    findSegment(r, p, xa, xb, color, fillDepth);
    return;
  }

  TPixel32 *pix, *pix0, *limit;
  // check to the right
  TPixel32 *line = r->pixels(p.y);

  pix0  = line + p.x;  // seed pixel
  pix   = pix0;
  limit = line + r->getBounds().x1;  // right end

  TPixel32 oldPix = *pix;

  int fillDepth2 = fillDepth * fillDepth;

  for (; pix <= limit; pix++) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xb = p.x + pix - pix0 - 1;

  // check to the left
  pix    = pix0;                      // seed pixel
  limit  = line + r->getBounds().x0;  // left end
  oldPix = *pix;
  for (; pix >= limit; pix--) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xa = p.x + pix - pix0 + 1;
}

//-----------------------------------------------------------------------------

class FillSeed {
public:
  int m_xa, m_xb;
  int m_y, m_dy;
  FillSeed(int xa, int xb, int y, int dy)
      : m_xa(xa), m_xb(xb), m_y(y), m_dy(dy) {}
};

//-----------------------------------------------------------------------------

bool isPixelInSegment(const std::vector<std::pair<int, int>> &segments, int x) {
  for (int i = 0; i < (int)segments.size(); i++) {
    std::pair<int, int> segment = segments[i];
    if (segment.first <= x && x <= segment.second) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void insertSegment(std::vector<std::pair<int, int>> &segments,
                   const std::pair<int, int> segment) {
  for (int i = segments.size() - 1; i >= 0; i--) {
    std::pair<int, int> app = segments[i];
    if (segment.first <= app.first && app.second <= segment.second)
      segments.erase(segments.begin() + i);
  }
  segments.push_back(segment);
}

//-----------------------------------------------------------------------------

bool floodCheck(const TPixel32 &clickColor, const TPixel32 *targetPix,
                const TPixel32 *oldPix, const int fillDepth) {
  auto fullColorThreshMatte = [](int matte, int fillDepth) -> int {
    return (matte <= fillDepth) ? matte : 255;
  };

  if (clickColor.m == 0) {
    int oldMatte = fullColorThreshMatte(oldPix->m, fillDepth);
    int matte    = fullColorThreshMatte(targetPix->m, fillDepth);
    return matte >= oldMatte && matte != 255;
  }
  int fillDepth2 = fillDepth * fillDepth;
  return !doesStemFill(clickColor, targetPix, fillDepth2);
}

bool scanTransparentRegion(TRasterCM32P ras, int x, int y,
                           std::vector<TPoint> &points, bool *visited,
                           const int maxSize) {
  if (!ras) return false;
  int lx = ras->getLx(), ly = ras->getLy();
  if (x < 0 || y < 0 || x >= lx || y >= ly) return true;
  if (visited[y * lx + x]) return true;

  TPixelCM32 *pix = ras->pixels(y) + x;
  if (pix->getPaint() != 0 || pix->getTone() < 64) return true;
  if (points.size() + 1 > maxSize) return false;

  visited[y * lx + x] = true;
  points.push_back(TPoint(x, y));

  bool ret = scanTransparentRegion(ras, x + 1, y, points, visited, maxSize) &&
             scanTransparentRegion(ras, x - 1, y, points, visited, maxSize) &&
             scanTransparentRegion(ras, x, y + 1, points, visited, maxSize) &&
             scanTransparentRegion(ras, x, y - 1, points, visited, maxSize);
  ret = ret && points.size() <= maxSize;
  return ret;
}

int getMostFrequentNeighborStyleId(TRasterCM32P ras,
                                   const std::vector<TPoint> &points,
                                   const bool *visited, bool &fillInk) {
  if (!ras) return 0;

  int lx = ras->getLx();
  int ly = ras->getLy();

  std::map<int, int> styleCount;
  int maxStyleId = 0;
  int maxCount   = 0;

  const std::vector<TPoint> diagonal = {TPoint(1, -1), TPoint(1, 1),
                                        TPoint(-1, 1), TPoint(-1, -1)};

  const std::vector<TPoint> cardinal = {TPoint(0, -1), TPoint(0, 1),
                                        TPoint(-1, 0), TPoint(1, 0)};

  for (const TPoint &p : points) {
    for (const TPoint &d : cardinal) {
      int nx = p.x + d.x;
      int ny = p.y + d.y;
      if (nx < 0 || ny < 0 || nx >= lx || ny >= ly) continue;
      if (visited[ny * lx + nx]) continue;

      TPixelCM32 *pix = ras->pixels(ny) + nx;

      int styleId = pix->getInk();
      if (styleId)
        styleCount[styleId] +=
            pix->getTone() + 1;  // cardinal Ink weights tone+1
      styleId = pix->getPaint();
      if (styleId) {
        styleCount[styleId] +=
            pix->getTone() + 2;  // cardinal Paint weights tone+2
        fillInk = false;
      }
    }
  }

  for (const TPoint &p : points) {
    for (const TPoint &d : diagonal) {
      int nx = p.x + d.x;
      int ny = p.y + d.y;
      if (nx < 0 || ny < 0 || nx >= lx || ny >= ly) continue;
      if (visited[ny * lx + nx]) continue;

      TPixelCM32 *pix = ras->pixels(ny) + nx;
      if (pix->isPureInk()) continue;
      int styleId = pix->getPaint();
      if (styleId) {
        styleCount[styleId] +=
            pix->getTone() + 3;  // Diagonal not pureInk Paint weights tone+3
        fillInk = false;
      }
    }
  }

  // Pick most heavy one
  for (const auto &entry : styleCount) {
    if (entry.second > maxCount) {
      maxCount   = entry.second;
      maxStyleId = entry.first;
    }
  }

  assert(maxStyleId);
  return maxStyleId;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------
/*-- Return true if fill processed --*/
bool fill(const TRasterCM32P &r, const FillParameters &params,
          TTileSaverCM32 *saver, const TRaster32P &Ref) {
  TPixelCM32 *pix, *limit, *pix0, *oldpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int tone, oldtone;
  TPoint p = params.m_p;
  int x = p.x, y = p.y;
  int paint = params.m_styleId;
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
  bool defRegionWithPaint     = params.m_defRegionWithPaint;
  bool usePrevailingReferFill = params.m_usePrevailingReferFill;

  /*-- getBounds returns the entire image --*/
  TRect bbbox = r->getBounds();

  /*- Return if clicked outside the screen -*/
  if (!bbbox.contains(p)) return false;

  if (paint == 0 && params.m_extendFill) {
    FillParameters tmp = FillParameters(params);
    tmp.m_styleId      = TPixelCM32::getMaxPaint() - 1;
    tmp.m_palette      = 0;
    fill(r, tmp, saver, Ref);
    std::vector<int> a = {TPixelCM32::getMaxPaint() - 1};
    TRop::eraseColors(r, &a, true);
    TRop::eraseColors(r, &a, false);
    return true;
  }

  /*- If the same color has already been painted, return -*/
  pix0                  = r->pixels(p.y) + p.x;
  int paintAtClickedPos = pix0->getPaint();
  if (paintAtClickedPos == paint)
    if (params.m_shiftFill) {
      FillParameters tmp = FillParameters(params);
      tmp.m_styleId      = 0;
      tmp.m_shiftFill    = false;
      tmp.m_minFillDepth = params.m_maxFillDepth;
      fill(r, tmp, saver, Ref);
      paintAtClickedPos = pix0->getPaint();
    } else
      return false;

  /*- If clicked at pure ink pixel, return -*/
  if (pix0->isPureInk()) return false;
  if (Ref.getPointer() != nullptr && Ref->pixels(y)[x].m == 255) return false;

  /*- If the "paint only transparent areas" option is enabled and the area is
   * already colored, return
   * -*/
  if (params.m_emptyOnly && pix0->getPaint() != 0 && !params.m_shiftFill)
    return false;

  // Put reference image, will be automatically removed by RAII
  /* To Save Undo Memory, saved refer pixel would be cleared */
  RefImageGuard refGuard(r, Ref);

  fillDepth = adjustFillDepth(fillDepth);

  std::stack<FillSeed> seeds;

  // Also do fillInk for autoPaint style Ink
  fillRow(r, p, xa, xb, paint, params.m_palette, saver, params.m_prevailing,
          paintAtClickedPos, defRegionWithPaint, usePrevailingReferFill);
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  int lasty         = y;
  int wrap          = r->getWrap();
  TPixelCM32 *line  = r->pixels(y);
  bool doExtendFill = params.m_extendFill;
  bool filled;
  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();
    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    line += (y - lasty) * wrap;
    pix    = line + xa;
    limit  = line + xb;
    oldpix = pix - dy * wrap;
    x      = xa;
    oldxd  = (std::numeric_limits<int>::min)();
    oldxc  = (std::numeric_limits<int>::max)();
    lasty  = y;
    filled = false;
    while (pix <= limit) {
      oldtone = threshTone(*oldpix, fillDepth);
      tone    = threshTone(*pix, fillDepth);
      // the last condition is added in order to prevent fill area from
      // protruding behind the colored line
      int pixPaint = pix->getPaint();
      if (pixPaint != paint && tone <= oldtone && tone != 0 &&
          (pixPaint == paintAtClickedPos || !defRegionWithPaint) &&
          (pixPaint != pix->getInk() || pixPaint == paintAtClickedPos)) {
        fillRow(r, TPoint(x, y), xc, xd, paint, params.m_palette, saver,
                params.m_prevailing, paintAtClickedPos, defRegionWithPaint,
                usePrevailingReferFill);
        filled |= tone == TPixelCM32::getMaxTone() && pix->getPaint() == paint;

        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));

    if (doExtendFill && !filled && xa < xb) {
      extendFill(paint, paintAtClickedPos, xa, xb, y, dy, r, params, saver);
    }
  }

  return true;
}

//-----------------------------------------------------------------------------

void fill(const TRaster32P &ras, const TRaster32P &ref,
          const FillParameters &params, TTileSaverFullColor *saver) {
  TPixel32 *pix, *limit, *pix0, *oldpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int matte, oldMatte;
  int x = params.m_p.x, y = params.m_p.y;
  TRaster32P workRas = ref ? ref : ras;

  TRect bbbox = workRas->getBounds();

  if (!bbbox.contains(params.m_p)) return;

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);

  // looking for any  pure transparent pixel along the border; if after filling
  // that pixel will be changed,
  // it means that I filled the bg and the savebox needs to be recomputed!
  TPixel32 borderIndex;
  TPixel32 *borderPix = 0;
  pix                 = workRas->pixels(0);
  int i;
  for (i = 0; i < workRas->getLx(); i++, pix++)  // border down
    if (pix->m == 0) {
      borderIndex = *pix;
      borderPix   = pix;
      break;
    }
  if (borderPix == 0)  // not found in border down...try border up (avoid left
                       // and right borders...so unlikely)
  {
    pix = workRas->pixels(workRas->getLy() - 1);
    for (i = 0; i < workRas->getLx(); i++, pix++)  // border up
      if (pix->m == 0) {
        borderIndex = *pix;
        borderPix   = pix;
        break;
      }
  }

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  // fillRow(r, params.m_p, xa, xb, color ,saver);
  findSegment(workRas, params.m_p, xa, xb, color);
  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    pix = pix0 = workRas->pixels(y) + xa;
    limit      = workRas->pixels(y) + xb;
    oldpix     = workRas->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();
    while (pix <= limit) {
      oldMatte  = threshMatte(oldpix->m, fillDepth);
      matte     = threshMatte(pix->m, fillDepth);
      bool test = false;
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      if (*pix != color && !test && matte >= oldMatte && matte != 255) {
        findSegment(workRas, TPoint(x, y), xc, xd, color);
        // segments[y].push_back(std::pair<int,int>(xc, xd));
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line    = ras->pixels(it->first);
    TPixel32 *refLine = 0;
    TPixel32 *refPix;
    if (ref) refLine = ref->pixels(it->first);
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (ref) refPix = refLine + segment.first;
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (ref) {
            *pix = *refPix;
            refPix++;
          } else
            *pix = pix->m == 0 ? color : overPix(color, *pix);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

static void rectFill(const TRaster32P &ras, const TRect &r,
                     const TPixel32 &color) {}

//-----------------------------------------------------------------------------

static TPoint nearestInk(const TRasterCM32P &r, const TPoint &p, int ray) {
  int i, j;
  TPixelCM32 *buf = (TPixelCM32 *)r->getRawData();

  for (j = std::max(p.y - ray, 0); j <= std::min(p.y + ray, r->getLy() - 1);
       j++)
    for (i = std::max(p.x - ray, 0); i <= std::min(p.x + ray, r->getLx() - 1);
         i++)
      if (!(buf + j * r->getWrap() + i)->isPurePaint()) return TPoint(i, j);

  return TPoint(-1, -1);
}

//-----------------------------------------------------------------------------

void inkFill(const TRasterCM32P &r, const TPoint &pin, int ink, int searchRay,
             TTileSaverCM32 *saver, TRect *insideRect) {
  r->lock();
  TPixelCM32 *pixels = (TPixelCM32 *)r->getRawData();
  int oldInk;
  TPoint p = pin;

  if ((pixels + p.y * r->getWrap() + p.x)->isPurePaint() &&
      (searchRay == 0 || (p = nearestInk(r, p, searchRay)) == TPoint(-1, -1))) {
    r->unlock();
    return;
  }
  TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);

  if (pix->getInk() == ink) {
    r->unlock();
    return;
  }

  oldInk = pix->getInk();

  std::stack<TPoint> seeds;
  seeds.push(p);

  while (!seeds.empty()) {
    p = seeds.top();
    seeds.pop();
    if (!r->getBounds().contains(p)) continue;
    if (insideRect && !insideRect->contains(p)) continue;

    TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);
    if (pix->isPurePaint() || pix->getInk() != oldInk) continue;

    if (saver) saver->save(p);

    pix->setInk(ink);

    seeds.push(TPoint(p.x - 1, p.y - 1));
    seeds.push(TPoint(p.x - 1, p.y));
    seeds.push(TPoint(p.x - 1, p.y + 1));
    seeds.push(TPoint(p.x, p.y - 1));
    seeds.push(TPoint(p.x, p.y + 1));
    seeds.push(TPoint(p.x + 1, p.y - 1));
    seeds.push(TPoint(p.x + 1, p.y));
    seeds.push(TPoint(p.x + 1, p.y + 1));
  }
  r->unlock();
}

//-----------------------------------------------------------------------------

void fullColorFill(const TRaster32P &ras, const FillParameters &params,
                   TTileSaverFullColor *saver) {
  int oldy, xa, xb, xc, xd, dy, oldxd, oldxc;
  TPixel32 *pix, *limit, *pix0, *oldpix;
  int x = params.m_p.x, y = params.m_p.y;

  TRect bbbox = ras->getBounds();
  if (!bbbox.contains(params.m_p)) return;

  TPixel32 clickedPosColor = *(ras->pixels(y) + x);

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();

  if (clickedPosColor == color) return;

  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  TPointD m_firstPoint, m_clickPoint;

  // convert fillDepth range from [0 - 15] to [0 - 255]
  fillDepth = (fillDepth << 4) | fillDepth;

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  fullColorFindSegment(ras, params.m_p, xa, xb, color, clickedPosColor,
                       fillDepth);

  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    // continue if the fill runs over image bounding
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    // left end of the pixels to be filled
    pix = pix0 = ras->pixels(y) + xa;
    // right end of the pixels to be filled
    limit = ras->pixels(y) + xb;
    // left end of the fill seed pixels
    oldpix = ras->pixels(oldy) + xa;

    x     = xa;
    oldxd = (std::numeric_limits<int>::min)();
    oldxc = (std::numeric_limits<int>::max)();

    // check pixels to right
    while (pix <= limit) {
      bool test = false;
      // check if the target is already in the range to be filled
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);

      if (*pix != color && !test &&
          floodCheck(clickedPosColor, pix, oldpix, fillDepth)) {
        // compute horizontal range to be filled
        fullColorFindSegment(ras, TPoint(x, y), xc, xd, color, clickedPosColor,
                             fillDepth);
        // insert segment to be filled
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        // create new fillSeed to invert direction, if needed
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        // jump to the next pixel to the right end of the range
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    // insert filled range as new fill seed
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  // pixels are actually filled here
  TPixel32 premultiColor = premultiply(color);

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line                                 = ras->pixels(it->first);
    TPixel32 *refLine                              = 0;
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (saver) {
          saver->save(
              TRect(segment.first, it->first, segment.second, it->first));
        }
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (clickedPosColor.m == 0)
            *pix = pix->m == 0 ? color : overPix(color, *pix);
          else if (color.m == 0 || color.m == 255)  // used for erasing area
            *pix = color;
          else
            *pix = overPix(*pix, premultiColor);
        }
      }
    }
  }
}

void fillHoles(const TRasterCM32P &ras, const int maxSize,
               TTileSaverCM32 *saver) {
  int lx        = ras->getLx();
  int ly        = ras->getLy();
  bool *visited = new bool[lx * ly]{false};

  auto isEmpty = [&](int x, int y) -> bool {
    TPixelCM32 *pix = ras->pixels(y) + x;
    return !pix->getPaint() && pix->getTone() > 64 && !visited[y * lx + x];
  };

  for (int y = 0; y < ly; y++) {
    int emptyCount = 0;
    for (int x = 0; x < lx; x++) {
      if (isEmpty(x, y)) {
        emptyCount++;
        continue;
      } else if (emptyCount && emptyCount <= maxSize) {
        std::vector<TPoint> points;
        int startX = x - emptyCount;
        if (scanTransparentRegion(ras, startX, y, points, visited, maxSize)) {
          bool fillInk = false;
          int style =
              getMostFrequentNeighborStyleId(ras, points, visited, fillInk);
          for (const TPoint &p : points) {
            if (saver) saver->save(p);
            if (fillInk) {
              ras->pixels(p.y)[p.x].setInk(style);
              ras->pixels(p.y)[p.x].setTone(0);
            } else
              ras->pixels(p.y)[p.x].setPaint(style);
          }
        } else
          for (const TPoint &p : points) {
            visited[p.y * lx + p.x] = false;
          }
      }
      emptyCount = 0;
    }
  }

  delete[] visited;
}
