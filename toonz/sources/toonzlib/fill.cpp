

#include <stack>
#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "toonz/ttileset.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include "trastercm.h"
#include "tropcm.h"
//-----------------------------------------------------------------------------
namespace {  // Utility Functions
//-----------------------------------------------------------------------------
/*! Thresholds tone value based on fill depth.
    If fillDepth is max tone, returns original tone.
    Otherwise, binarizes: returns max tone if above fillDepth, else original
   tone.
*/
inline int threshTone(const TPixelCM32 &pix, int fillDepth) {
  if (fillDepth == TPixelCM32::getMaxTone())
    return pix.getTone();
  else
    return ((pix.getTone()) > fillDepth) ? TPixelCM32::getMaxTone()
                                         : pix.getTone();
}

/*! Adjusts fill depth to match tone range (15 or 255).
    For 15-tone: 15 - fillDepth
    For 255-tone: (15 - fillDepth) repeated in both nibbles
*/
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

/*! Thresholds matte (alpha) value.
    If fillDepth is 255, returns original matte.
    Otherwise, returns 255 if matte < fillDepth, else original.
*/
inline int threshMatte(int matte, int fillDepth) {
  if (fillDepth == 255)
    return matte;
  else
    return (matte < fillDepth) ? 255 : matte;
}

/*! Finds nearest non-pure-paint (ink) pixel in cardinal directions (no
   diagonal). Returns first valid ink pixel found (right �� left �� down �� up).
    Returns (-1,-1) if none found.
*/
inline TPoint nearestInkNotDiagonal(const TRasterCM32P &r, const TPoint &p) {
  TPixelCM32 *buf = (TPixelCM32 *)r->pixels(p.y) + p.x;
  if (p.x < r->getLx() - 1 && !(buf + 1)->isPurePaint())
    return TPoint(p.x + 1, p.y);
  if (p.x > 0 && !(buf - 1)->isPurePaint()) return TPoint(p.x - 1, p.y);
  if (p.y < r->getLy() - 1 && !(buf + r->getWrap())->isPurePaint())
    return TPoint(p.x, p.y + 1);
  if (p.y > 0 && !(buf - r->getWrap())->isPurePaint())
    return TPoint(p.x, p.y - 1);
  return TPoint(-1, -1);
}

/*! Collects unique neighboring ink pixels (cardinal directions only).
    Avoids duplicates by tracking ink IDs in a set.
    Returns vector of unique ink positions.
*/
inline std::vector<TPoint> getNeighborInks(const TRasterCM32P &r,
                                           const TPoint &p) {
  TPixelCM32 *buf = (TPixelCM32 *)r->pixels(p.y) + p.x;
  std::vector<TPoint> points;
  std::set<int> inks;
  TPixelCM32 *pix;
  int ink;
  if (p.x < r->getLx() - 1) {
    pix = buf + 1;
    if (!pix->isPurePaint()) {
      ink = pix->getInk();
      points.push_back(TPoint(p.x + 1, p.y));
      inks.insert(ink);
    }
  }
  if (p.x > 0) {
    pix = buf - 1;
    if (!pix->isPurePaint()) {
      ink = pix->getInk();
      if (inks.find(ink) == inks.end()) {
        points.push_back(TPoint(p.x - 1, p.y));
        inks.insert(ink);
      }
    }
  }
  if (p.y < r->getLy() - 1) {
    pix = buf + r->getWrap();
    if (!pix->isPurePaint()) {
      ink = pix->getInk();
      if (inks.find(ink) == inks.end()) {
        points.push_back(TPoint(p.x, p.y + 1));
        inks.insert(ink);
      }
    }
  }
  if (p.y > 0) {
    pix = buf - r->getWrap();
    if (!pix->isPurePaint()) {
      ink = pix->getInk();
      if (inks.find(ink) == inks.end()) {
        points.push_back(TPoint(p.x, p.y - 1));
        inks.insert(ink);
      }
    }
  }
  return points;
}

// Expands fill from point (x,y) both left and right.
// The redrawn row spans from *xa to *xb (inclusive).
// Constraint: x1 <= *xa <= *xb <= x2
// Note: if no pixel is painted, *xa > *xb
//
// "prevailing" is disabled during border revert-fill in Rectangular,
// Freehand, and Polyline fill to allow paint to extend behind lines.
void fillRow(const TRasterCM32P &r, const TPoint &p, int &xa, int &xb,
             int paint, TPalette *palette, TTileSaverCM32 *saver,
             bool prevailing = true, int paintAtClickPos = 0,
             bool defRegionWithPaint     = true,
             bool usePrevailingReferFill = false) {
  int tone, oldtone;
  TPixelCM32 *pix, *pix0, *limit, *tmp_limit;
  /* Expand to the right */
  TPixelCM32 *line = r->pixels(p.y);
  pix0             = line + p.x;
  pix              = pix0;
  limit            = line + r->getBounds().x1;
  oldtone          = pix->getTone();
  tone             = oldtone;
  for (; pix <= limit; pix++) {
    if (pix->getPaint() == paint) break;
    tone = pix->getTone();
    if (tone == 0 || (pix->getPaint() != paintAtClickPos && defRegionWithPaint))
      break;
    // Prevent fill from protruding behind colored lines
    if (tone > oldtone) {
      // Handle uncolored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        break;
      while (pix != pix0) {
        // Step back to leave lowest-tone pixel unpainted
        pix--;
        // Allow painting of 1-pixel semi-transparent lines
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // Ensure current pixel is not lowest tone among vertical neighbors
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
    tmp_limit = pix + 10;  // Default edge-stop distance = 10 pixels
    if (limit > tmp_limit) limit = tmp_limit;
    int preVailingInk,
        oldPrevailingInk = 0;  // Prevent mixing different ink styles
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
  /* Expand to the left */
  pix     = pix0;
  limit   = line + r->getBounds().x0;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (pix--; pix >= limit; pix--) {
    if (pix->getPaint() == paint) break;
    tone = pix->getTone();
    if (tone == 0 || (pix->getPaint() != paintAtClickPos && defRegionWithPaint))
      break;
    // Prevent fill from protruding behind colored lines
    if (tone > oldtone) {
      // Handle uncolored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        break;
      while (pix != pix0) {
        // Step forward to leave lowest-tone pixel unpainted
        pix++;
        // Allow painting of 1-pixel semi-transparent lines
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // Ensure current pixel is not lowest tone among vertical neighbors
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
      /*--- Skip if outside fill region ---*/
      if (pix->getPaint() != paintAtClickPos && defRegionWithPaint) continue;
      /*--- Paint reference lines ---*/
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
      /*--- Auto-paint neighboring auto-paint ink lines ---*/
      if (palette && pix->isPurePaint() && paint != 0) {
        auto inks = getNeighborInks(r, TPoint(xa + n, p.y));
        for (TPoint pInk : inks) {
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
      /*--- Apply normal paint ---*/
      if (pix->getInk() == TPixelCM32::getMaxInk() && !usePrevailingReferFill &&
          pix->isPureInk())
        continue;
      pix->setPaint(paint);
    }
    // Ensure surrounding reference ink pixels are painted
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
//-----------------------------------------------------------------------------
void findSegment(const TRaster32P &r, const TPoint &p, int &xa, int &xb,
                 const TPixel32 &color, const int fillDepth = 254) {
  int matte, oldmatte;
  TPixel32 *pix, *pix0, *limit, *tmp_limit;
  /* Expand to the right */
  TPixel32 *line = r->pixels(p.y);
  pix0           = line + p.x;
  pix            = pix0;
  limit          = line + r->getBounds().x1;
  oldmatte       = pix->m;
  matte          = oldmatte;
  for (; pix <= limit; pix++) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix + 10;  // Default edge-stop = 10 pixels
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xb = p.x + pix - pix0 - 1;
  /* Expand to the left */
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
// Used when clicked pixel is solid or semi-transparent.
// Checks if fill should stop at target pixel.
// Compares RGB difference (not alpha).
bool doesStemFill(const TPixel32 &clickColor, const TPixel32 *targetPix,
                  const int fillDepth2) {
  // Stop if target is fully transparent
  if (targetPix->m == 0) return true;
  // Stop if RGB difference exceeds threshold
  int dr = (int)clickColor.r - (int)targetPix->r;
  int dg = (int)clickColor.g - (int)targetPix->g;
  int db = (int)clickColor.b - (int)targetPix->b;
  return (dr * dr + dg * dg + db * db) > fillDepth2;  // Condition to halt fill
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
  // Check right
  TPixel32 *line  = r->pixels(p.y);
  pix0            = line + p.x;  // Seed pixel
  pix             = pix0;
  limit           = line + r->getBounds().x1;  // Right boundary
  TPixel32 oldPix = *pix;
  int fillDepth2  = fillDepth * fillDepth;
  for (; pix <= limit; pix++) {
    // Stop if target matches fill color
    if (*pix == color) break;
    // Continue if same as previous pixel
    if (*pix == oldPix) continue;
    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;
    oldPix = *pix;
  }
  xb = p.x + pix - pix0 - 1;
  // Check left
  pix    = pix0;
  limit  = line + r->getBounds().x0;  // Left boundary
  oldPix = *pix;
  for (; pix >= limit; pix--) {
    if (*pix == color) break;
    if (*pix == oldPix) continue;
    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;
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
  if (points.size() + 1 > static_cast<size_t>(maxSize)) return false;
  visited[y * lx + x] = true;
  points.push_back(TPoint(x, y));
  bool ret = scanTransparentRegion(ras, x + 1, y, points, visited, maxSize) &&
             scanTransparentRegion(ras, x - 1, y, points, visited, maxSize) &&
             scanTransparentRegion(ras, x, y + 1, points, visited, maxSize) &&
             scanTransparentRegion(ras, x, y - 1, points, visited, maxSize);
  ret = ret && points.size() <= static_cast<size_t>(maxSize);
  return ret;
}
int getMostFrequentNeighborStyleId(TRasterCM32P ras,
                                   const std::vector<TPoint> &points,
                                   const bool *visited, bool &fillInk) {
  if (!ras) return 0;
  int lx = ras->getLx();
  int ly = ras->getLy();
  std::map<int, int> styleCount;
  int maxStyleId                     = 0;
  int maxCount                       = 0;
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
      int styleId     = pix->getInk();
      if (styleId)
        styleCount[styleId] +=
            pix->getTone() + 1;  // Cardinal ink: weight = tone + 1
      styleId = pix->getPaint();
      if (styleId) {
        styleCount[styleId] +=
            pix->getTone() + 2;  // Cardinal paint: weight = tone + 2
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
            pix->getTone() +
            3;  // Diagonal non-pureInk paint: weight = tone + 3
        fillInk = false;
      }
    }
  }
  // Select style with highest weighted count
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
/*-- Returns true if fill was applied --*/
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
  /*-- getBounds returns full image rect --*/
  TRect bbbox = r->getBounds();
  /*- Abort if click is outside image -*/
  if (!bbbox.contains(p)) return false;
  /*- Abort if clicked pixel already has target paint -*/
  pix0                  = r->pixels(p.y) + p.x;
  int paintAtClickedPos = pix0->getPaint();
  if (paintAtClickedPos == paint) {
    if (params.m_shiftFill) {
      FillParameters tmp = FillParameters(params);
      tmp.m_styleId      = 0;
      tmp.m_shiftFill    = false;
      tmp.m_minFillDepth = params.m_maxFillDepth;
      fill(r, tmp, saver, Ref);
      paintAtClickedPos = pix0->getPaint();
    } else {
      return false;
    }
  }
  /*- Abort if clicked on pure ink -*/
  if (pix0->isPureInk()) return false;
  if (Ref.getPointer() != nullptr && Ref->pixels(y)[x].m == 255) return false;
  /*- Abort if "empty only" is on and area is already painted -*/
  if (params.m_emptyOnly && pix0->getPaint() != 0 && paint != 0 &&
      !params.m_shiftFill)
    return false;
  // RAII guard to manage reference image
  /* To save undo memory, saved reference pixels are cleared */
  RefImageGuard refGuard(r, Ref);
  fillDepth = adjustFillDepth(fillDepth);
  std::stack<FillSeed> seeds;
  // Fill initial row and seed stack
  fillRow(r, p, xa, xb, paint, params.m_palette, saver, params.m_prevailing,
          paintAtClickedPos, defRegionWithPaint, usePrevailingReferFill);
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));
  int lasty        = y;
  int wrap         = r->getWrap();
  TPixelCM32 *line = r->pixels(y);
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
    while (pix <= limit) {
      oldtone = threshTone(*oldpix, fillDepth);
      tone    = threshTone(*pix, fillDepth);
      // Additional condition prevents fill from bleeding behind colored lines
      int pixPaint = pix->getPaint();
      if (pixPaint != paint && tone <= oldtone && tone != 0 &&
          (pixPaint == paintAtClickedPos || !defRegionWithPaint) &&
          (pixPaint != pix->getInk() || pixPaint == paintAtClickedPos)) {
        fillRow(r, TPoint(x, y), xc, xd, paint, params.m_palette, saver,
                params.m_prevailing, paintAtClickedPos, defRegionWithPaint,
                usePrevailingReferFill);
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1) {
          oldxd = xd;
        } else {
          if (oldxd >= 0) {
            seeds.push(FillSeed(oldxc, oldxd, y, dy));
          }
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
  TRect bbbox        = workRas->getBounds();
  if (!bbbox.contains(params.m_p)) return;
  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
  assert(fillDepth >= 0 && fillDepth < 16);
  fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);
  // Detect border transparency to determine if savebox needs recompute
  TPixel32 borderIndex;
  TPixel32 *borderPix = 0;
  pix                 = workRas->pixels(0);
  int i;
  for (i = 0; i < workRas->getLx(); i++, pix++)  // Bottom border
    if (pix->m == 0) {
      borderIndex = *pix;
      borderPix   = pix;
      break;
    }
  if (borderPix == 0)  // Try top border if not found
  {
    pix = workRas->pixels(workRas->getLy() - 1);
    for (i = 0; i < workRas->getLx(); i++, pix++)  // Top border
      if (pix->m == 0) {
        borderIndex = *pix;
        borderPix   = pix;
        break;
      }
  }
  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;
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
  TPaletteP plt            = params.m_palette;
  TPixel32 color           = plt->getStyle(params.m_styleId)->getMainColor();
  if (clickedPosColor == color) return;
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
  assert(fillDepth >= 0 && fillDepth < 16);
  TPointD m_firstPoint, m_clickPoint;
  // Convert fillDepth from [0-15] to [0-255]
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
    // Skip if fill exceeds image bounds
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    // Left boundary of fill area
    pix = pix0 = ras->pixels(y) + xa;
    // Right boundary of fill area
    limit = ras->pixels(y) + xb;
    // Left boundary of seed row
    oldpix = ras->pixels(oldy) + xa;
    x      = xa;
    oldxd  = (std::numeric_limits<int>::min)();
    oldxc  = (std::numeric_limits<int>::max)();
    // Process pixels to the right
    while (pix <= limit) {
      bool test = false;
      // Check if already scheduled for fill
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      if (*pix != color && !test &&
          floodCheck(clickedPosColor, pix, oldpix, fillDepth)) {
        // Compute horizontal fill range
        fullColorFindSegment(ras, TPoint(x, y), xc, xd, color, clickedPosColor,
                             fillDepth);
        // Record fill segment
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        // Seed opposite direction if needed
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        // Jump to right edge of new segment
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    // Add pending segment as new seed
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }
  // Apply actual pixel painting
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
          else if (color.m == 0 || color.m == 255)  // Erase mode
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
  auto isEmpty  = [&](int x, int y) -> bool {
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
