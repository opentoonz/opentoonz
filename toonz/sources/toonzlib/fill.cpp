

#include "trastercm.h"
#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include <stack>

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
             bool prevailing = true) {
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
    if (tone == 0) break;
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
  if (tone == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      if (pix->getPaint() == paint) break;
      if (pix->getTone() != 0) break;
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
    if (tone == 0) break;
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
  if (tone == 0) {
    tmp_limit                    = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      if (pix->getPaint() == paint) break;
      if (pix->getTone() != 0) break;
    }
  }

  xa = p.x + pix - pix0 + 1;

  if (saver) saver->save(TRect(xa, p.y, xb, p.y));

  if (xb >= xa) {
    pix = line + xa;
    int n;
    for (n = 0; n < xb - xa + 1; n++, pix++) {
      if (palette && pix->isPurePaint()) {
        TPoint pInk = nearestInkNotDiagonal(r, TPoint(xa + n, p.y));
        if (pInk != TPoint(-1, -1)) {
          TPixelCM32 *pixInk =
              (TPixelCM32 *)r->getRawData() + (pInk.y * r->getWrap() + pInk.x);
          if (pixInk->getInk() != paint &&
              palette->getStyle(pixInk->getInk())->getFlags() != 0)
            inkFill(r, pInk, paint, 0, saver);
        }
      }

      pix->setPaint(paint);
    }
  }
}

//-----------------------------------------------------------------------------

void findSegment(const TRaster32P &r, const TPoint &p, int &xa, int &xb,
                 const TPixel32 &color) {
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
    if (matte < oldmatte || matte == 255) break;
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
    if (matte < oldmatte || matte == 255) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit                    = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
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

inline int threshTone(const TPixelCM32 &pix, int fillDepth) {
  if (fillDepth == TPixelCM32::getMaxTone())
    return pix.getTone();
  else
    return ((pix.getTone()) > fillDepth) ? TPixelCM32::getMaxTone()
                                         : pix.getTone();
}

//-----------------------------------------------------------------------------

inline int threshMatte(int matte, int fillDepth) {
  if (fillDepth == 255)
    return matte;
  else
    return (matte < fillDepth) ? 255 : matte;
}

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
}  // namespace
//-----------------------------------------------------------------------------
/*-- 戻り値はsaveBoxが更新されたかどうか --*/
bool fill(const TRasterCM32P &r, const FillParameters &params,
          TTileSaverCM32 *saver) {
  TPixelCM32 *pix, *limit, *pix0, *oldpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int tone, oldtone;
  TPoint p = params.m_p;
  int x = p.x, y = p.y;
  int paint = params.m_styleId;
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  /*-- getBoundsは画像全面 --*/
  TRect bbbox = r->getBounds();

  /*- 画面外のクリックの場合はreturn -*/
  if (!bbbox.contains(p)) return false;
  /*- 既に同じ色が塗られている場合はreturn -*/
  int paintAtClickedPos = (r->pixels(p.y) + p.x)->getPaint();
  if (paintAtClickedPos == paint) return false;
  /*- 「透明部分だけを塗る」オプションが有効で、既に色が付いている場合はreturn
   * -*/
  if (params.m_emptyOnly && (r->pixels(p.y) + p.x)->getPaint() != 0)
    return false;

  assert(fillDepth >= 0 && fillDepth < 16);

  switch (TPixelCM32::getMaxTone()) {
  case 15:
    fillDepth = (15 - fillDepth);
    break;
  case 255:
    fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);
    break;
  default:
    assert(false);
  }
  /*-- 四隅の色を見て、一つでも変わったらsaveBoxを更新する --*/
  TPixelCM32 borderIndex[4];
  TPixelCM32 *borderPix[4];
  pix            = r->pixels(0);
  borderPix[0]   = pix;
  borderIndex[0] = *pix;
  pix += r->getLx() - 1;
  borderPix[1]   = pix;
  borderIndex[1] = *pix;
  pix            = r->pixels(r->getLy() - 1);
  borderPix[2]   = pix;
  borderIndex[2] = *pix;
  pix += r->getLx() - 1;
  borderPix[3]   = pix;
  borderIndex[3] = *pix;

  std::stack<FillSeed> seeds;

  fillRow(r, p, xa, xb, paint, params.m_palette, saver, params.m_prevailing);
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
    pix = pix0 = r->pixels(y) + xa;
    limit      = r->pixels(y) + xb;
    oldpix     = r->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();
    while (pix <= limit) {
      oldtone = threshTone(*oldpix, fillDepth);
      tone    = threshTone(*pix, fillDepth);
      // the last condition is added in order to prevent fill area from
      // protruding behind the colored line
      if (pix->getPaint() != paint && tone <= oldtone && tone != 0 &&
          (pix->getPaint() != pix->getInk() ||
           pix->getPaint() == paintAtClickedPos)) {
        fillRow(r, TPoint(x, y), xc, xd, paint, params.m_palette, saver,
                params.m_prevailing);
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

  bool saveBoxChanged = false;
  for (int i = 0; i < 4; i++) {
    if (!((*borderPix[i]) == borderIndex[i])) {
      saveBoxChanged = true;
      break;
    }
  }
  return saveBoxChanged;
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
        pix             = line + segment.first;
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
