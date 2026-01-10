

// #define AUTOCLOSE_DEBUG

#include "texception.h"
#include "toonz/autoclose.h"
#include "trastercm.h"
#include "skeletonlut.h"
#include "toonz/fill.h"
#include <set>
#include <queue>
#include <unordered_set>
#include <algorithm>

// #define AUT_SPOT_SAMPLES 40
using namespace SkeletonLut;

class TAutocloser::Imp {
public:
  struct Seed {
    UCHAR *m_ptr;
    UCHAR m_preseed;
    Seed(UCHAR *ptr, UCHAR preseed) : m_ptr(ptr), m_preseed(preseed) {}
  };
  UINT m_aut_spot_samples;

  int m_closingDistance;
  double m_spotAngle;  // Half Value
  int m_inkIndex;
  int m_opacity;
  TRasterP m_raster;
  TRasterGR8P m_bRaster;
  UCHAR *m_br;

  int m_bWrap;

  int m_displaceVector[8];
  TPointD m_displAverage;
  int m_visited;

  double m_csp, m_snp, m_csm, m_snm, m_csa, m_sna, m_csb, m_snb;

  // For Debug
  std::vector<Segment> *m_currentClosingSegments;

  Imp(const TRasterP &r, int distance = 10, double angle = M_PI_2,
      int index = 0, int opacity = 0)
      : m_raster(r)
      , m_spotAngle(angle)
      , m_closingDistance(distance)
      , m_inkIndex(index)
      , m_opacity(opacity) {}

  ~Imp() {}

  bool inline isInk(UCHAR *br) { return (*br) & 0x1; }
  inline void eraseInk(UCHAR *br) { *(br) &= 0xfe; }

  UCHAR inline ePix(UCHAR *br) { return (*(br + 1)); }
  UCHAR inline wPix(UCHAR *br) { return (*(br - 1)); }
  UCHAR inline nPix(UCHAR *br) { return (*(br + m_bWrap)); }
  UCHAR inline sPix(UCHAR *br) { return (*(br - m_bWrap)); }
  UCHAR inline swPix(UCHAR *br) { return (*(br - m_bWrap - 1)); }
  UCHAR inline nwPix(UCHAR *br) { return (*(br + m_bWrap - 1)); }
  UCHAR inline nePix(UCHAR *br) { return (*(br + m_bWrap + 1)); }
  UCHAR inline sePix(UCHAR *br) { return (*(br - m_bWrap + 1)); }
  UCHAR inline neighboursCode(UCHAR *seed) {
    return ((swPix(seed) & 0x1) | ((sPix(seed) & 0x1) << 1) |
            ((sePix(seed) & 0x1) << 2) | ((wPix(seed) & 0x1) << 3) |
            ((ePix(seed) & 0x1) << 4) | ((nwPix(seed) & 0x1) << 5) |
            ((nPix(seed) & 0x1) << 6) | ((nePix(seed) & 0x1) << 7));
  }

  //.......................

  inline bool notMarkedBorderInk(UCHAR *br) {
    return ((((*br) & 0x5) == 1) &&
            (ePix(br) == 0 || wPix(br) == 0 || nPix(br) == 0 || sPix(br) == 0));
  }

  //.......................
  UCHAR *getPtr(int x, int y) { return m_br + m_bWrap * y + x; }
  UCHAR *getPtr(const TPoint &p) { return m_br + m_bWrap * p.y + p.x; }

  TPoint getCoordinates(UCHAR *br) {
    TPoint p;
    int pixelCount = br - m_bRaster->getRawData();
    p.y            = pixelCount / m_bWrap;
    p.x            = pixelCount - p.y * m_bWrap;
    return p;
  }

  //.......................
  void compute(std::vector<Segment> &closingSegmentArray);
  void draw(const std::vector<Segment> &closingSegmentArray);
  void skeletonize(std::vector<TPoint> &endpoints);
  void findSeeds(std::vector<Seed> &seeds, std::vector<TPoint> &endpoints);
  void erase(std::vector<Seed> &seeds, std::vector<TPoint> &endpoints);
  void circuitAndMark(UCHAR *seed, UCHAR preseed);
  bool circuitAndCancel(UCHAR *seed, UCHAR preseed,
                        std::vector<TPoint> &endpoints);
  void findMeetingPoints(std::vector<TPoint> &endpoints,
                         std::vector<Segment> &closingSegments);
  void calculateWeightAndDirection(std::vector<Segment> &orientedEndpoints);
  bool spotResearchTwoPoints(std::vector<Segment> &endpoints,
                             std::vector<Segment> &closingSegments);
  bool spotResearchOnePoint(std::vector<Segment> &endpoints,
                            std::vector<Segment> &closingSegments);

  void copy(const TRasterGR8P &braux, TRaster32P &raux);
  int exploreTwoSpots(const TAutocloser::Segment &s0,
                      const TAutocloser::Segment &s1);
  int notInsidePath(const TPoint &p, const TPoint &q);
  bool hasInkBetween(const TPoint &p1, const TPoint &p2);
  void drawInByteRaster(const TPoint &p0, const TPoint &p1);
  TPoint visitEndpoint(UCHAR *br);
  bool exploreSpot(const Segment &s, TPoint &p);
  bool exploreRay(UCHAR *br, Segment s, TPoint &p);
  void visitPix(UCHAR *br, int toVisit, const TPoint &dis);
  void cancelMarks(UCHAR *br);
  void cancelFromArray(std::vector<Segment> &array, TPoint p, int &count);
};

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/

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

/*------------------------------------------------------------------------*/

#define EXPLORE_RAY_ISTR(istr)                                                 \
  if (!inside_ink) {                                                           \
    if (((*br) & 0x1) && !((*br) & 0x80)) {                                    \
      p.x = istr;                                                              \
      p.y = (s.first.y < s.second.y) ? s.first.y + y : s.first.y - y;          \
      return true;                                                             \
    }                                                                          \
  } else if (inside_ink && !((*br) & 0x1))                                     \
    inside_ink = 0;

/*------------------------------------------------------------------------*/

//-------------------------------------------------

namespace {

inline bool isInk(const TPixel32 &pix) { return pix.r < 80; }

/*------------------------------------------------------------------------*/

TRasterGR8P fillByteRaster(const TRasterCM32P &r, TRasterGR8P &bRaster) {
  int i, j;
  int lx = r->getLx();
  int ly = r->getLy();
  // bRaster->create(lx+4, ly+4);
  UCHAR *br = bRaster->getRawData();

  for (i = 0; i < lx + 4; i++) *(br++) = 0;

  for (i = 0; i < lx + 4; i++) *(br++) = 131;

  for (i = 0; i < ly; i++) {
    *(br++)         = 0;
    *(br++)         = 131;
    TPixelCM32 *pix = r->pixels(i);
    for (j = 0; j < lx; j++, pix++) {
      if (pix->getTone() != pix->getMaxTone())
        *(br++) = 3;
      else
        *(br++) = 0;
    }
    *(br++) = 131;
    *(br++) = 0;
  }

  for (i = 0; i < lx + 4; i++) *(br++) = 131;

  for (i = 0; i < lx + 4; i++) *(br++) = 0;

  return bRaster;
}

/*------------------------------------------------------------------------*/

#define SET_INK                                                                \
  if (buf->getTone() == buf->getMaxTone())                                     \
    *buf = TPixelCM32(inkIndex, buf->getPaint(), 255 - opacity);

unsigned char buildPaintnInkNeighborPattern(int x, int y, TRasterCM32P r,
                                            int centerPaint) {
  const int dx[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
  const int dy[8] = {-1, -1, -1, 0, 1, 1, 1, 0};

  unsigned char pattern = 0;
  // qDebug() << "Initial pattern:" << (int)pattern;

  for (int i = 0; i < 8; ++i) {
    int nx = x + dx[i];
    int ny = y + dy[i];

    if (nx >= 0 && nx < r->getLx() && ny >= 0 && ny < r->getLy()) {
      TPixelCM32 *neighborPix = r->pixels(ny) + nx;
      int tone                = neighborPix->getTone();
      int paint               = neighborPix->getPaint();
      bool isLine = tone != TPixelCM32::getMaxTone() || paint != centerPaint;
      /*qDebug() << QString("NO.%6: (%1,%2):%3,%4 %5")
                      .arg(nx)
                      .arg(ny)
                      .arg(tone)
                      .arg(paint)
                      .arg(isLine ? 1 : 0)
                      .arg(i);*/
      if (isLine) {
        pattern |= (1 << (7 - i));
      }
      // qDebug() << "Pattern:"
      //          << QString::number(pattern, 2).rightJustified(8, '0');
    } else
      pattern |= (1 << (7 - i));
  }
  // qDebug() << "Final pattern: decimal =" << (int)pattern << "| binary = 0b"
  //          << QString::number(pattern, 2).rightJustified(8, '0');
  return pattern;
}

// 8-neighbor connection lookup table for surrounding 8 pixels
const unsigned char connectivity_lut[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1,  // 0
    1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1,  // 16
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 32
    1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1,  // 48
    1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 64
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  // 80
    1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 96
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  // 112
    1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 128
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 144
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 160
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 176
    1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 192
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  // 208
    1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,  // 224
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,  // 240
};
// For Paint&Ink defined Region
void closeSegment(const TRasterCM32P &r, const TAutocloser::Segment &s,
                  const USHORT ink, const USHORT opacity) {
  int x1 = s.first.x, y1 = s.first.y;
  int x2 = s.second.x, y2 = s.second.y;
  typedef std::vector<std::pair<int, int>> Points;

  if (x1 > x2) {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }

  int dx = x2 - x1;
  int dy = y2 - y1;

  int nx = 0, ny = 0;

  if (dy >= 0) {
    if (dy <= dx) {
      nx = 0;
      ny = 1;
    } else {
      nx = 1;
      ny = 0;
    }
  } else {
    int abs_dy = -dy;
    if (abs_dy <= dx) {
      nx = 0;
      ny = -1;
    } else {
      nx = 1;
      ny = 0;
    }
  }

  int abs_dx = std::abs(dx);
  int abs_dy = std::abs(dy);
  int sx     = (dx > 0) ? 1 : -1;
  int sy     = (dy > 0) ? 1 : -1;
  int err    = abs_dx - abs_dy;

  int x = x1;
  int y = y1;
  Points paintedPoints;
  int crossLineTimes = 0;
  TPixelCM32 *pix    = r->pixels(y) + x;
  while (true) {
    // Only check side pixels if current line pixel is purePaint
    pix = r->pixels(y) + x;
    if (pix->isPurePaint()) {
      pix->setInk(ink);
      pix->setTone(255 - opacity);
      paintedPoints.push_back({x, y});
    }

    if (x == x2 && y == y2) break;
    int e2 = 2 * err;
    if (e2 > -abs_dy) {
      err -= abs_dy;
      x += sx;
    }
    if (e2 < abs_dx) {
      err += abs_dx;
      y += sy;
    }
  }

  Points toRemovePoints;
  Points restPoints;

  for (auto &[x, y] : paintedPoints) {
    TPixelCM32 *pix = r->pixels(y) + x;
    if (pix->getInk() != ink) continue;
    int linePaint = pix->getPaint();

    unsigned char pattern = buildPaintnInkNeighborPattern(x, y, r, linePaint);

    bool shouldRemove = connectivity_lut[pattern];

    if (shouldRemove) {
      toRemovePoints.push_back({x, y});
    } else  // Keep it!
    {
      restPoints.push_back({x, y});
    }
  }

  for (auto &[x, y] : toRemovePoints) {
    TPixelCM32 *pix = r->pixels(y) + x;
    *pix            = TPixelCM32(0, pix->getPaint(), pix->getMaxTone());
  }

  for (auto &[x, y] : restPoints) {
    TPixelCM32 *pix = r->pixels(y) + x;
    if (pix->getInk() != ink) continue;
    int linePaint = pix->getPaint();

    unsigned char pattern = buildPaintnInkNeighborPattern(x, y, r, linePaint);

    bool shouldRemove = connectivity_lut[pattern];

    if (shouldRemove) {
      TPixelCM32 *pix = r->pixels(y) + x;
      *pix            = TPixelCM32(0, pix->getPaint(), pix->getMaxTone());
    }
  }
  return;
}

void drawSegment(TRasterCM32P &r, const TAutocloser::Segment &s,
                 USHORT inkIndex, USHORT opacity) {
  int wrap        = r->getWrap();
  TPixelCM32 *buf = r->pixels();
  /*
  int i, j;
  for (i=0; i<r->getLy();i++)
  {
    for (j=0; j<r->getLx();j++, buf++)
  *buf = (1<<4)|0xf;
  buf += wrap-r->getLx();
    }
  return;
  */

  int x, y, dx, dy, d, incr_1, incr_2;

  int x1 = s.first.x;
  int y1 = s.first.y;
  int x2 = s.second.x;
  int y2 = s.second.y;

  if (x1 > x2) {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }

  buf += y1 * wrap + x1;

  dx = x2 - x1;
  dy = y2 - y1;

  x = y = 0;
  SET_INK;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf += wrap + 1), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf += wrap), (buf += wrap + 1), SET_INK)
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (buf++), (buf -= (wrap - 1)), SET_INK)
    else
      DRAW_SEGMENT(y, x, dy, dx, (buf -= wrap), (buf -= (wrap - 1)), SET_INK)
  }

  SET_INK;
}

/*------------------------------------------------------------------------*/

}  // namespace
/*------------------------------------------------------------------------*/

void TAutocloser::Imp::compute(std::vector<Segment> &closingSegmentArray) {
  std::vector<TPoint> endpoints;
  try {
    // assert(closingSegmentArray.empty());

    TRasterCM32P raux;

    if (!(raux = (TRasterCM32P)m_raster))
      throw TException("Unable to autoclose a not CM32 image.");

    if (m_raster->getLx() == 0 || m_raster->getLy() == 0)
      throw TException("Autoclose error: bad image size");

    // Lx = r->lx;
    // Ly = r->ly;

    TRasterGR8P braux(raux->getLx() + 4, raux->getLy() + 4);
    braux->lock();
    fillByteRaster(raux, braux);

    TRect r(2, 2, braux->getLx() - 3, braux->getLy() - 3);
    m_bRaster = braux->extract(r);
    m_br      = m_bRaster->getRawData();
    m_bWrap   = m_bRaster->getWrap();

    m_displaceVector[0] = -m_bWrap - 1;
    m_displaceVector[1] = -m_bWrap;
    m_displaceVector[2] = -m_bWrap + 1;
    m_displaceVector[3] = -1;
    m_displaceVector[4] = +1;
    m_displaceVector[5] = m_bWrap - 1;
    m_displaceVector[6] = m_bWrap;
    m_displaceVector[7] = m_bWrap + 1;

    skeletonize(endpoints);

    findMeetingPoints(endpoints, closingSegmentArray);
    // copy(m_bRaster, raux);
    braux->unlock();

  }

  catch (TException &e) {
    throw e;
  }
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::draw(const std::vector<Segment> &closingSegmentArray) {
  TRasterCM32P raux;

  if (!(raux = (TRasterCM32P)m_raster))
    throw TException("Unable to autoclose a not CM32 image.");

  if (m_raster->getLx() == 0 || m_raster->getLy() == 0)
    throw TException("Autoclose error: bad image size");

  if (DEF_REGION_WITH_PAINT) {
    for (int i = 0; i < (int)closingSegmentArray.size(); i++)
      closeSegment(raux, closingSegmentArray[i], m_inkIndex, m_opacity);
  } else
    for (int i = 0; i < (int)closingSegmentArray.size(); i++)
      drawSegment(raux, closingSegmentArray[i], m_inkIndex, m_opacity);
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::copy(const TRasterGR8P &br, TRaster32P &r) {
  // assert(r->getLx() == br->getLx() && r->getLy() == br->getLy());
  int i, j;

  int lx = r->getLx();
  int ly = r->getLy();

  UCHAR *bbuf = br->getRawData();
  TPixel *buf = (TPixel *)r->getRawData();

  for (i = 0; i < ly; i++) {
    for (j = 0; j < lx; j++, buf++, bbuf++) {
      buf->m = 255;
      if ((*bbuf) & 0x40)
        buf->r = 255, buf->g = buf->b = 0;
      else if (isInk(bbuf))
        buf->r = buf->g = buf->b = 0;
      else
        buf->r = buf->g = buf->b = 255;
    }
    buf += r->getWrap() - lx;
    bbuf += br->getWrap() - lx;
  }
}

/*=============================================================================*/

namespace {

int intersect_segment(int x1, int y1, int x2, int y2, int i, double *ris) {
  if ((i < std::min(y1, y2)) || (i > std::max(y1, y2)) || (y1 == y2)) return 0;

  *ris = ((double)((x1 - x2) * (i - y2)) / (double)(y1 - y2) + x2);

  return 1;
}

/*=============================================================================*/

inline int distance2(const TPoint p0, const TPoint p1) {
  return (p0.x - p1.x) * (p0.x - p1.x) + (p0.y - p1.y) * (p0.y - p1.y);
}

/*=============================================================================*/

int closerPoint(const std::vector<TAutocloser::Segment> &points,
                std::vector<bool> &marks, int index) {
  // assert(points.size() == marks.size());

  int min, curr;
  int minval = (std::numeric_limits<int>::max)();

  min = index + 1;

  for (curr = index + 1; curr < (int)points.size(); curr++)
    if (!(marks[curr])) {
      int distance = distance2(points[index].first, points[curr].first);

      if (distance < minval) {
        minval = distance;
        min    = curr;
      }
    }

  marks[min] = true;
  return min;
}

/*------------------------------------------------------------------------*/

int intersect_triangle(int x1a, int y1a, int x2a, int y2a, int x3a, int y3a,
                       int x1b, int y1b, int x2b, int y2b, int x3b, int y3b) {
  int minx, maxx, miny, maxy, i;
  double xamin, xamax, xbmin, xbmax, val;

  miny = std::max(std::min({y1a, y2a, y3a}), std::min({y1b, y2b, y3b}));
  maxy = std::min(std::max({y1a, y2a, y3a}), std::max({y1b, y2b, y3b}));
  if (maxy < miny) return 0;

  minx = std::max(std::min({x1a, x2a, x3a}), std::min({x1b, x2b, x3b}));
  maxx = std::min(std::max({x1a, x2a, x3a}), std::max({x1b, x2b, x3b}));
  if (maxx < minx) return 0;

  for (i = miny; i <= maxy; i++) {
    xamin = xamax = xbmin = xbmax = 0.0;

    intersect_segment(x1a, y1a, x2a, y2a, i, &xamin);

    if (intersect_segment(x1a, y1a, x3a, y3a, i, &val)) {
      if (xamin) {
        xamax = val;
      } else {
        xamin = val;
      }
    }

    if (!xamax) intersect_segment(x2a, y2a, x3a, y3a, i, &xamax);

    if (xamax < xamin) {
      val = xamin, xamin = xamax, xamax = val;
    }

    intersect_segment(x1b, y1b, x2b, y2b, i, &xbmin);

    if (intersect_segment(x1b, y1b, x3b, y3b, i, &val)) {
      if (xbmin) {
        xbmax = val;
      } else {
        xbmin = val;
      }
    }

    if (!xbmax) intersect_segment(x2b, y2b, x3b, y3b, i, &xbmax);

    if (xbmax < xbmin) {
      val = xbmin, xbmin = xbmax, xbmax = val;
    }

    if (!((tceil(xamax) < tfloor(xbmin)) || (tceil(xbmax) < tfloor(xamin))))
      return 1;
  }
  return 0;
}

/*------------------------------------------------------------------------*/

}  // namespace

/*------------------------------------------------------------------------*/

int TAutocloser::Imp::notInsidePath(const TPoint &p, const TPoint &q) {
  int tmp, x, y, dx, dy, d, incr_1, incr_2;
  int x1, y1, x2, y2;

  x1 = p.x;
  y1 = p.y;
  x2 = q.x;
  y2 = q.y;

  if (x1 > x2) {
    tmp = x1, x1 = x2, x2 = tmp;
    tmp = y1, y1 = y2, y2 = tmp;
  }
  UCHAR *br = getPtr(x1, y1);

  dx = x2 - x1;
  dy = y2 - y1;
  x = y = 0;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (br++), (br += m_bWrap + 1),
                   if (!((*br) & 0x2)) return true)
    else
      DRAW_SEGMENT(y, x, dy, dx, (br += m_bWrap), (br += m_bWrap + 1),
                   if (!((*br) & 0x2)) return true)
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (br++), (br -= m_bWrap - 1),
                   if (!((*br) & 0x2)) return true)
    else
      DRAW_SEGMENT(y, x, dy, dx, (br -= m_bWrap), (br -= m_bWrap - 1),
                   if (!((*br) & 0x2)) return true)
  }

  return 0;
}

bool TAutocloser::Imp::hasInkBetween(const TPoint &p1, const TPoint &p2) {
  int tmp, x, y, dx, dy, d, incr_1, incr_2;
  int x1, y1, x2, y2;
  int inkCount = 0;

  x1 = p1.x;
  y1 = p1.y;
  x2 = p2.x;
  y2 = p2.y;

  if (x1 > x2) {
    tmp = x1, x1 = x2, x2 = tmp;
    tmp = y1, y1 = y2, y2 = tmp;
  }
  UCHAR *br = getPtr(x1, y1);

  dx = x2 - x1;
  dy = y2 - y1;
  x = y = 0;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(
          x, y, dx, dy, (br++), (br += m_bWrap + 1), if (*br != 0) {
            inkCount++;
            if (inkCount > 2) return true;
          })
    else
      DRAW_SEGMENT(
          y, x, dy, dx, (br += m_bWrap), (br += m_bWrap + 1), if (*br != 0) {
            inkCount++;
            if (inkCount > 2) return true;
          })
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(
          x, y, dx, dy, (br++), (br -= m_bWrap - 1), if (*br != 0) {
            inkCount++;
            if (inkCount > 2) return true;
          })
    else
      DRAW_SEGMENT(
          y, x, dy, dx, (br -= m_bWrap), (br -= m_bWrap - 1), if (*br != 0) {
            inkCount++;
            if (inkCount > 2) return true;
          })
  }

  return inkCount != 2;
}

/*------------------------------------------------------------------------*/
int TAutocloser::Imp::exploreTwoSpots(const TAutocloser::Segment &s0,
                                      const TAutocloser::Segment &s1) {
  int x1a, y1a, x2a, y2a, x3a, y3a, x1b, y1b, x2b, y2b, x3b, y3b;

  x1a = s0.first.x;
  y1a = s0.first.y;
  x1b = s1.first.x;
  y1b = s1.first.y;

  TPoint p0aux = s0.second;
  TPoint p1aux = s1.second;
#ifdef AUTOCLOSE_DEBUG
  m_currentClosingSegments->push_back(s0);
  m_currentClosingSegments->push_back(s1);
#endif
  if (x1a == p0aux.x && y1a == p0aux.y) return 0;
  if (x1b == p1aux.x && y1b == p1aux.y) return 0;

  x2a = tround(x1a + (p0aux.x - x1a) * m_csp - (p0aux.y - y1a) * m_snp);
  y2a = tround(y1a + (p0aux.x - x1a) * m_snp + (p0aux.y - y1a) * m_csp);
  x3a = tround(x1a + (p0aux.x - x1a) * m_csm - (p0aux.y - y1a) * m_snm);
  y3a = tround(y1a + (p0aux.x - x1a) * m_snm + (p0aux.y - y1a) * m_csm);

  x2b = tround(x1b + (p1aux.x - x1b) * m_csp - (p1aux.y - y1b) * m_snp);
  y2b = tround(y1b + (p1aux.x - x1b) * m_snp + (p1aux.y - y1b) * m_csp);
  x3b = tround(x1b + (p1aux.x - x1b) * m_csm - (p1aux.y - y1b) * m_snm);
  y3b = tround(y1b + (p1aux.x - x1b) * m_snm + (p1aux.y - y1b) * m_csm);

#ifdef AUTOCLOSE_DEBUG
  m_currentClosingSegments->push_back(Segment(s0.first, TPoint(x2a, y2a)));
  m_currentClosingSegments->push_back(Segment(s0.first, TPoint(x3a, y3a)));
  m_currentClosingSegments->push_back(Segment(s1.first, TPoint(x2b, y2b)));
  m_currentClosingSegments->push_back(Segment(s1.first, TPoint(x3b, y3b)));
#endif

  return (intersect_triangle(x1a, y1a, p0aux.x, p0aux.y, x2a, y2a, x1b, y1b,
                             p1aux.x, p1aux.y, x2b, y2b) ||
          intersect_triangle(x1a, y1a, p0aux.x, p0aux.y, x3a, y3a, x1b, y1b,
                             p1aux.x, p1aux.y, x2b, y2b) ||
          intersect_triangle(x1a, y1a, p0aux.x, p0aux.y, x2a, y2a, x1b, y1b,
                             p1aux.x, p1aux.y, x3b, y3b) ||
          intersect_triangle(x1a, y1a, p0aux.x, p0aux.y, x3a, y3a, x1b, y1b,
                             p1aux.x, p1aux.y, x3b, y3b));
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::findMeetingPoints(
    std::vector<TPoint> &endpoints, std::vector<Segment> &closingSegments) {
  int i;
  double alfa;
  m_aut_spot_samples = (UINT)m_spotAngle;

  m_spotAngle *= (M_PI / 180.0);

  // spotResearchTwoPoints
  // Angle Range: 0°~36°
  double limitedAngle = m_spotAngle / 10;
  m_csp               = cos(limitedAngle);
  m_snp               = sin(limitedAngle);
  m_csm               = cos(-limitedAngle);
  m_snm               = sin(-limitedAngle);

  // spotResearchOnePoints
  alfa  = m_spotAngle / m_aut_spot_samples;
  m_csa = cos(alfa);
  m_sna = sin(alfa);
  m_csb = cos(-alfa);
  m_snb = sin(-alfa);

  std::vector<Segment> orientedEndpoints(endpoints.size());
  for (i = 0; i < (int)endpoints.size(); i++)
    orientedEndpoints[i].first = endpoints[i];

  int size = -1;
#ifdef AUTOCLOSE_DEBUG
  m_currentClosingSegments = &closingSegments;
#endif
  while ((int)closingSegments.size() > size && !orientedEndpoints.empty()) {
    size = closingSegments.size();
    do calculateWeightAndDirection(orientedEndpoints);
    while (spotResearchTwoPoints(orientedEndpoints, closingSegments));

    do calculateWeightAndDirection(orientedEndpoints);
    while (spotResearchOnePoint(orientedEndpoints, closingSegments));
  }
}

/*------------------------------------------------------------------------*/

static bool allMarked(const std::vector<bool> &marks, int index) {
  int i;

  for (i = index + 1; i < (int)marks.size(); i++)
    if (!marks[i]) return false;
  return true;
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::spotResearchTwoPoints(
    std::vector<Segment> &endpoints, std::vector<Segment> &closingSegments) {
  if (endpoints.size() < 2) return false;

  bool found = false;

  const int sqrDistance = m_closingDistance * m_closingDistance;
  const int lx          = m_raster->getLx();
  const int ly          = m_raster->getLy();

  // Lambda for fallback (used in two places)
  auto fallback = [&]() -> bool {
    std::vector<bool> keep(endpoints.size(), true);

    for (int i = 0; i < (int)endpoints.size(); ++i) {
      if (!keep[i]) continue;
      const TPoint &p = endpoints[i].first;

      for (int j = i + 1; j < (int)endpoints.size(); ++j) {
        if (!keep[j]) continue;
        const TPoint &q = endpoints[j].first;

        int dist = distance2(p, q);
        if (dist > sqrDistance) continue;

        if (exploreTwoSpots(endpoints[i], endpoints[j]) &&
            notInsidePath(p, q) && !hasInkBetween(p, q)) {
          drawInByteRaster(p, q);
          closingSegments.push_back(Segment(p, q));
          keep[i] = false;
          if (!EndpointTable[neighboursCode(getPtr(q))]) {
            keep[j] = false;
          }
          found = true;  // <-- uses found from main function
        }
      }
    }

    if (found) {
      endpoints.erase(std::remove_if(endpoints.begin(), endpoints.end(),
                                     [&keep, idx = 0](const Segment &) mutable {
                                       return !keep[idx++];
                                     }),
                      endpoints.end());
    }
    return found;
  };

  // Quick fallback for few endpoints
  // Use brute-force only when there are very few gaps (faster in simple cases)
  // Recommended values:
  // - 40: conservative more frames use brute-force
  // - 60–80: best balance for typical animation (recommended)
  // >= 100: forces grid on most frames (dense/complex drawings)
  if (endpoints.size() < 60) {
    return fallback();
  }

  // ========== DYNAMIC CELL_SIZE CALCULATION ==========
  int CELL_SIZE;

  // Calculate endpoint density (endpoints per pixel)
  double area    = lx * ly;
  double density = (area > 0) ? (endpoints.size() / area) : 0;

  // Heuristic: ideal CELL_SIZE based on density and distance
  if (density > 0) {
    // Formula: cell_size = sqrt(TARGET_ENDPOINTS_PER_CELL / density)
    // where TARGET_ENDPOINTS_PER_CELL is the desired number of endpoints per
    // cell
    const double TARGET_ENDPOINTS_PER_CELL = 8.0;  // Adjustable value

    double ideal_cell_size = std::sqrt(TARGET_ENDPOINTS_PER_CELL / density);

    // Limit by practical constraints
    CELL_SIZE = static_cast<int>(ideal_cell_size);

    // Minimum and maximum limits
    const int MIN_CELL_SIZE = std::max(5, m_closingDistance / 4);
    const int MAX_CELL_SIZE =
        std::min(50,  // Absolute limit
                 std::max(m_closingDistance * 2, std::max(lx, ly) / 10));

    CELL_SIZE = std::clamp(CELL_SIZE, MIN_CELL_SIZE, MAX_CELL_SIZE);

    // Adjust based on number of endpoints
    // For many endpoints, smaller cells may be better
    if (endpoints.size() > 500) {
      CELL_SIZE = std::max(MIN_CELL_SIZE, CELL_SIZE * 2 / 3);
    }
  } else {
    // Zero density (shouldn't happen, but as fallback)
    CELL_SIZE =
        std::max(10, std::min(m_closingDistance, std::max(lx, ly) / 10));
  }

  // Ensure grid is not too large or too small
  const int GRID_W = (lx + CELL_SIZE - 1) / CELL_SIZE;
  const int GRID_H = (ly + CELL_SIZE - 1) / CELL_SIZE;

  // If grid too small (less than 9 cells), use fallback
  if (GRID_W * GRID_H < 9) {
    return fallback();
  }

  // ========== OPTIMIZED SPATIAL GRID ==========
  std::vector<std::vector<int>> grid(GRID_W * GRID_H);

  // Fill grid
  for (int i = 0; i < (int)endpoints.size(); ++i) {
    const TPoint &p = endpoints[i].first;
    // Use clamp for safety
    int gx = std::clamp(p.x / CELL_SIZE, 0, GRID_W - 1);
    int gy = std::clamp(p.y / CELL_SIZE, 0, GRID_H - 1);
    grid[gy * GRID_W + gx].push_back(i);
  }

  std::vector<bool> keep(endpoints.size(), true);

  const int cellRadius = (m_closingDistance + CELL_SIZE - 1) / CELL_SIZE;

  for (int i = 0; i < (int)endpoints.size(); ++i) {
    if (!keep[i]) continue;
    const TPoint &p = endpoints[i].first;
    // Adjustment: Also in lookup
    int gx = std::clamp(p.x / CELL_SIZE, 0, GRID_W - 1);
    int gy = std::clamp(p.y / CELL_SIZE, 0, GRID_H - 1);

    bool pairFound = false;
    for (int dy = -cellRadius; dy <= cellRadius && !pairFound; ++dy) {
      for (int dx = -cellRadius; dx <= cellRadius && !pairFound; ++dx) {
        int ngx = gx + dx;
        int ngy = gy + dy;
        if (ngx < 0 || ngx >= GRID_W || ngy < 0 || ngy >= GRID_H) continue;

        const auto &cell = grid[ngy * GRID_W + ngx];
        for (int j_idx : cell) {
          if (j_idx <= i || !keep[j_idx]) continue;
          const TPoint &q = endpoints[j_idx].first;

          // Optimization: quick bounding box
          int dx_abs = std::abs(p.x - q.x);
          int dy_abs = std::abs(p.y - q.y);
          if (dx_abs > m_closingDistance || dy_abs > m_closingDistance)
            continue;

          int dist = distance2(p, q);
          if (dist > sqrDistance) continue;

          if (exploreTwoSpots(endpoints[i], endpoints[j_idx]) &&
              notInsidePath(p, q) && !hasInkBetween(p, q)) {
            drawInByteRaster(p, q);
            closingSegments.push_back(Segment(p, q));
            keep[i] = false;
            if (!EndpointTable[neighboursCode(getPtr(q))]) {
              keep[j_idx] = false;
            }
            found     = true;
            pairFound = true;
            break;  // Exit inner loop
          }
        }
      }
    }
  }

  if (found) {
    endpoints.erase(std::remove_if(endpoints.begin(), endpoints.end(),
                                   [&keep, idx = 0](const Segment &) mutable {
                                     return !keep[idx++];
                                   }),
                    endpoints.end());
  }

  return found;
}

/*------------------------------------------------------------------------*/
/*
static void clear_marks(POINT *p)
{
while (p)
  {
  p->mark = 0;
  p = p->next;
  }
}


static int there_are_unmarked(POINT *p)
{
while (p)
  {
  if (!p->mark) return 1;
  p = p->next;
  }
return 0;
}
*/

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::calculateWeightAndDirection(
    std::vector<Segment> &orientedEndpoints) {
  // UCHAR *br;
  int lx = m_raster->getLx();
  int ly = m_raster->getLy();

  std::vector<Segment>::iterator it = orientedEndpoints.begin();

  while (it != orientedEndpoints.end()) {
    TPoint p0  = it->first;
    TPoint &p1 = it->second;

    // br = (UCHAR *)m_bRaster->pixels(p0.y)+p0.x;
    // code = neighboursCode(br);
    /*if (!EndpointTable[code])
{
it = orientedEndpoints.erase(it);
continue;
}*/
    TPoint displAverage = visitEndpoint(getPtr(p0));

    p1 = p0 - displAverage;

    /*if ((point->x2<0 && point->y2<0) || (point->x2>Lx && point->y2>Ly))
     * printf("what a pain!!!!!!\n");*/

    if (p1.x < 0) {
      p1.y = tround(p0.y - (float)((p0.y - p1.y) * p0.x) / (p0.x - p1.x));
      p1.x = 0;
    } else if (p1.x > lx) {
      p1.y =
          tround(p0.y - (float)((p0.y - p1.y) * (p0.x - lx)) / (p0.x - p1.x));
      p1.x = lx;
    }

    if (p1.y < 0) {
      p1.x = tround(p0.x - (float)((p0.x - p1.x) * p0.y) / (p0.y - p1.y));
      p1.y = 0;
    } else if (p1.y >= ly) {
      p1.x =
          tround(p0.x - (float)((p0.x - p1.x) * (p0.y - ly)) / (p0.y - p1.y));
      p1.y = ly - 1;
    }
    it++;
  }
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::spotResearchOnePoint(
    std::vector<Segment> &endpoints, std::vector<Segment> &closingSegments) {
  if (endpoints.empty()) return false;

  bool found = false;
  std::vector<bool> keep(endpoints.size(), true);

  for (int i = 0; i < (int)endpoints.size(); ++i) {
    if (!keep[i]) continue;

    TPoint p;
    if (exploreSpot(endpoints[i], p)) {
      Segment segment(endpoints[i].first, p);

      // Avoid duplicates
      auto it =
          std::find(closingSegments.begin(), closingSegments.end(), segment);
      if (it == closingSegments.end() && notInsidePath(endpoints[i].first, p)) {
        drawInByteRaster(endpoints[i].first, p);
        closingSegments.push_back(segment);
        found = true;

        // Mark current endpoint for possible removal
        if (!EndpointTable[neighboursCode(getPtr(endpoints[i].first))]) {
          keep[i] = false;
        }

        // Mark endpoints that end at 'p' (if they exist)
        for (int j = 0; j < (int)endpoints.size(); ++j) {
          if (keep[j] && endpoints[j].first == p) {
            keep[j] = false;
          }
        }
      }
    }
  }

  // Final removal compacting the vector
  if (found) {
    endpoints.erase(std::remove_if(endpoints.begin(), endpoints.end(),
                                   [&keep, index = 0](const Segment &) mutable {
                                     return !keep[index++];
                                   }),
                    endpoints.end());
  }

  return found;
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::exploreSpot(const Segment &s, TPoint &p) {
  int x1, y1, x2, y2, x3, y3, i;
  double x2a, y2a, x2b, y2b, xnewa, ynewa, xnewb, ynewb;
  int lx = m_raster->getLx();
  int ly = m_raster->getLy();

  x1 = s.first.x;
  y1 = s.first.y;
  x2 = s.second.x;
  y2 = s.second.y;
  if (x2 == 0 || x2 == lx || y2 == 0 || y2 == ly - 1) {
    p.x = x2;
    p.y = y2;
    return true;
  }
  if (x1 == x2 && y1 == y2) return 0;

  if (exploreRay(getPtr(x1, y1), s, p)) return true;

  x2a = x2b = (double)x2;
  y2a = y2b = (double)y2;
  for (i = 0; i < m_aut_spot_samples; i++) {
    xnewa = x1 + (x2a - x1) * m_csa - (y2a - y1) * m_sna;
    ynewa = y1 + (y2a - y1) * m_csa + (x2a - x1) * m_sna;
    x3    = tround(xnewa);
    y3    = tround(ynewa);
#ifdef AUTOCLOSE_DEBUG
    m_currentClosingSegments->push_back(
        Segment(s.first, TPoint(tround(xnewa), tround(ynewa))));
#else
    if ((x3 != tround(x2a) || y3 != tround(y2a)) && x3 > 0 && x3 < lx &&
        y3 > 0 && y3 < ly &&
        exploreRay(
            getPtr(x1, y1),
            Segment(TPoint(x1, y1), TPoint(tround(xnewa), tround(ynewa))), p))
      return true;
#endif
    x2a = xnewa;
    y2a = ynewa;

    xnewb = x1 + (x2b - x1) * m_csb - (y2b - y1) * m_snb;
    ynewb = y1 + (y2b - y1) * m_csb + (x2b - x1) * m_snb;
    x3    = tround(xnewb);
    y3    = tround(ynewb);
#ifdef AUTOCLOSE_DEBUG
    m_currentClosingSegments->push_back(
        Segment(s.first, TPoint(tround(xnewb), tround(ynewb))));
#else
    if ((x3 != tround(x2b) || y3 != tround(y2b)) && x3 > 0 && x3 < lx &&
        y3 > 0 && y3 < ly &&
        exploreRay(
            getPtr(x1, y1),
            Segment(TPoint(x1, y1), TPoint(tround(xnewb), tround(ynewb))), p))
      return true;
#endif
    x2b = xnewb;
    y2b = ynewb;
  }
#ifdef AUTOCLOSE_DEBUG
  return true;
#else
  return false;
#endif
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::exploreRay(UCHAR *br, Segment s, TPoint &p) {
  int x, y, dx, dy, d, incr_1, incr_2, inside_ink;

  inside_ink = 1;

  x = 0;
  y = 0;

  if (s.first.x < s.second.x) {
    dx = s.second.x - s.first.x;
    dy = s.second.y - s.first.y;
    if (dy >= 0)
      if (dy <= dx)
        DRAW_SEGMENT(x, y, dx, dy, (br++), (br += m_bWrap + 1),
                     EXPLORE_RAY_ISTR((s.first.x + x)))
      else
        DRAW_SEGMENT(y, x, dy, dx, (br += m_bWrap), (br += m_bWrap + 1),
                     EXPLORE_RAY_ISTR((s.first.x + x)))
    else {
      dy = -dy;
      if (dy <= dx)
        DRAW_SEGMENT(x, y, dx, dy, (br++), (br -= m_bWrap - 1),
                     EXPLORE_RAY_ISTR((s.first.x + x)))
      else
        DRAW_SEGMENT(y, x, dy, dx, (br -= m_bWrap), (br -= m_bWrap - 1),
                     EXPLORE_RAY_ISTR((s.first.x + x)))
    }
  } else {
    dx = s.first.x - s.second.x;
    dy = s.second.y - s.first.y;
    if (dy >= 0)
      if (dy <= dx)
        DRAW_SEGMENT(x, y, dx, dy, (br--), (br += m_bWrap - 1),
                     EXPLORE_RAY_ISTR((s.first.x - x)))
      else
        DRAW_SEGMENT(y, x, dy, dx, (br += m_bWrap), (br += m_bWrap - 1),
                     EXPLORE_RAY_ISTR((s.first.x - x)))
    else {
      dy = -dy;
      if (dy <= dx)
        DRAW_SEGMENT(x, y, dx, dy, (br--), (br -= m_bWrap + 1),
                     EXPLORE_RAY_ISTR((s.first.x - x)))
      else
        DRAW_SEGMENT(y, x, dy, dx, (br -= m_bWrap), (br -= m_bWrap + 1),
                     EXPLORE_RAY_ISTR((s.first.x - x)))
    }
  }
  return false;
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::drawInByteRaster(const TPoint &p0, const TPoint &p1) {
  int x, y, dx, dy, d, incr_1, incr_2;
  UCHAR *br;

  if (p0.x > p1.x) {
    br = getPtr(p1);
    dx = p0.x - p1.x;
    dy = p0.y - p1.y;
  } else {
    br = getPtr(p0);
    dx = p1.x - p0.x;
    dy = p1.y - p0.y;
  }

  x = y = 0;

  if (dy >= 0) {
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (br++), (br += m_bWrap + 1), ((*br) |= 0x41))
    else
      DRAW_SEGMENT(y, x, dy, dx, (br += m_bWrap), (br += m_bWrap + 1),
                   ((*br) |= 0x41))
  } else {
    dy = -dy;
    if (dy <= dx)
      DRAW_SEGMENT(x, y, dx, dy, (br++), (br -= m_bWrap - 1), ((*br) |= 0x41))
    else
      DRAW_SEGMENT(y, x, dy, dx, (br -= m_bWrap), (br -= m_bWrap - 1),
                   ((*br) |= 0x41))
  }
}

/*------------------------------------------------------------------------*/

TPoint TAutocloser::Imp::visitEndpoint(UCHAR *br)

{
  m_displAverage = TPointD();

  m_visited = 0;

  visitPix(br, m_closingDistance, TPoint());
  cancelMarks(br);

  return TPoint(convert((1.0 / m_visited) * m_displAverage));
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::visitPix(UCHAR *br, int toVisit, const TPoint &dis) {
  UCHAR b = 0;
  int i, pixToVisit = 0;

  *br |= 0x10;
  m_visited++;
  m_displAverage.x += dis.x;
  m_displAverage.y += dis.y;

  toVisit--;
  if (toVisit == 0) return;

  for (i = 0; i < 8; i++) {
    UCHAR *v = br + m_displaceVector[i];
    if (isInk(v) && !((*v) & 0x10)) {
      b |= (1 << i);
      pixToVisit++;
    }
  }

  if (pixToVisit == 0) return;

  if (pixToVisit <= 4) toVisit = troundp(toVisit / (double)pixToVisit);

  if (toVisit == 0) return;

  int x[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  int y[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

  for (i = 0; i < 8; i++)
    if (b & (1 << i))
      visitPix(br + m_displaceVector[i], toVisit, dis + TPoint(x[i], y[i]));
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::cancelMarks(UCHAR *br) {
  *br &= 0xef;
  int i;

  for (i = 0; i < 8; i++) {
    UCHAR *v = br + m_displaceVector[i];

    if (isInk(v) && (*v) & 0x10) cancelMarks(v);
  }
}

/*=============================================================================*/

void TAutocloser::Imp::skeletonize(std::vector<TPoint> &endpoints) {
  std::vector<Seed> seeds;

  findSeeds(seeds, endpoints);

  erase(seeds, endpoints);
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::findSeeds(std::vector<Seed> &seeds,
                                 std::vector<TPoint> &endpoints) {
  int i, j;
  UCHAR preseed;

  UCHAR *br = m_br;

  for (i = 0; i < m_bRaster->getLy(); i++) {
    for (j = 0; j < m_bRaster->getLx(); j++, br++) {
      if (notMarkedBorderInk(br)) {
        preseed = FirstPreseedTable[neighboursCode(br)];

        if (preseed != 8) /*not an isolated pixel*/
        {
          seeds.push_back(Seed(br, preseed));
          circuitAndMark(br, preseed);
        } else {
          (*br) |= 0x8;
          endpoints.push_back(getCoordinates(br));
        }
      }
    }
    br += m_bWrap - m_bRaster->getLx();
  }
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::circuitAndMark(UCHAR *seed, UCHAR preseed) {
  UCHAR *walker;
  UCHAR displ, prewalker;

  *seed |= 0x4;

  displ = NextPointTable[(neighboursCode(seed) << 3) | preseed];
  // assert(displ>=0 && displ<8);

  walker    = seed + m_displaceVector[displ];
  prewalker = displ ^ 0x7;

  while ((walker != seed) || (preseed != prewalker)) {
    *walker |= 0x4; /* set the pass mark */

    displ = NextPointTable[(neighboursCode(walker) << 3) | prewalker];
    //  assert(displ>=0 && displ<8);
    walker += m_displaceVector[displ];
    prewalker = displ ^ 0x7;
  }

  return;
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::erase(std::vector<Seed> &seeds,
                             std::vector<TPoint> &endpoints) {
  int i, size = 0, oldSize;
  UCHAR *seed, preseed, code, displ;
  oldSize = seeds.size();

  while (oldSize != size) {
    oldSize = size;
    size    = seeds.size();

    for (i = oldSize; i < size; i++) {
      seed    = seeds[i].m_ptr;
      preseed = seeds[i].m_preseed;

      if (!isInk(seed)) {
        code = NextSeedTable[neighboursCode(seed)];
        seed += m_displaceVector[code & 0x7];
        preseed = (code & 0x38) >> 3;
      }

      if (circuitAndCancel(seed, preseed, endpoints)) {
        if (isInk(seed)) {
          displ = NextPointTable[(neighboursCode(seed) << 3) | preseed];
          //				assert(displ>=0 && displ<8);
          seeds.push_back(Seed(seed + m_displaceVector[displ], displ ^ 0x7));

        } else /* the seed has been erased */
        {
          code = NextSeedTable[neighboursCode(seed)];
          seeds.push_back(
              Seed(seed + m_displaceVector[code & 0x7], (code & 0x38) >> 3));
        }
      }
    }
  }
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::circuitAndCancel(UCHAR *seed, UCHAR preseed,
                                        std::vector<TPoint> &endpoints) {
  UCHAR *walker, *previous;
  UCHAR displ, prewalker;
  bool ret = false;

  displ = NextPointTable[(neighboursCode(seed) << 3) | preseed];
  // assert(displ>=0 && displ<8);

  if ((displ == preseed) && !((*seed) & 0x8)) {
    endpoints.push_back(getCoordinates(seed));
    *seed |= 0x8;
  }

  walker    = seed + m_displaceVector[displ];
  prewalker = displ ^ 0x7;

  while ((walker != seed) || (preseed != prewalker)) {
    //	assert(prewalker>=0 && prewalker<8);
    displ = NextPointTable[(neighboursCode(walker) << 3) | prewalker];
    //  assert(displ>=0 && displ<8);

    if ((displ == prewalker) && !((*walker) & 0x8)) {
      endpoints.push_back(getCoordinates(walker));
      *walker |= 0x8;
    }
    previous = walker + m_displaceVector[prewalker];
    if (ConnectionTable[neighboursCode(previous)]) {
      ret = true;
      if (previous != seed) eraseInk(previous);
    }
    walker += m_displaceVector[displ];
    prewalker = displ ^ 0x7;
  }

  displ = NextPointTable[(neighboursCode(walker) << 3) | prewalker];

  if ((displ == preseed) && !((*seed) & 0x8)) {
    endpoints.push_back(getCoordinates(seed));
    *seed |= 0x8;
  }

  if (ConnectionTable[neighboursCode(seed + m_displaceVector[preseed])]) {
    ret = true;
    eraseInk(seed + m_displaceVector[preseed]);
  }

  if (ConnectionTable[neighboursCode(seed)]) {
    ret = true;
    eraseInk(seed);
  }

  return ret;
}

/*=============================================================================*/

void TAutocloser::Imp::cancelFromArray(std::vector<Segment> &array, TPoint p,
                                       int &count) {
  std::vector<Segment>::iterator it = array.begin();
  int i                             = 0;

  for (; it != array.end(); ++it, i++)
    if (it->first == p) {
      if (!EndpointTable[neighboursCode(getPtr(p))]) {
        // assert(i != count);
        if (i < count) count--;
        array.erase(it);
      }
      return;
    }
}

/*------------------------------------------------------------------------*/
/*
int is_in_list(LIST list, UCHAR *br)
{
POINT *aux;
aux = list.head;

while(aux)
  {
  if (aux->p == br) return 1;
  aux = aux->next;
  }
return 0;
}
*/

/*=============================================================================*/

std::unordered_map<std::string, std::vector<TAutocloser::Segment>>
    TAutocloser::m_cache;
std::mutex TAutocloser::m_mutex;

TAutocloser::TAutocloser(const TRasterP &r, int distance, double angle, int ink,
                         int opacity, std::set<int> autoPaints)
    : m_imp(new Imp(r, distance, angle, ink, opacity))
    , m_autoPaintStyles(autoPaints) {}

TAutocloser::TAutocloser(const TRasterP &r, int ink, const AutocloseSettings st,
                         std::set<int> autoPaints)
    : m_imp(new Imp(r, st.m_closingDistance, st.m_spotAngle, ink, st.m_opacity))
    , m_autoPaintStyles(autoPaints) {}
/*------------------------------------------------------------------------*/

void TAutocloser::exec() {
  std::vector<TAutocloser::Segment> segments;
  compute(segments);
  draw(segments);
}

void TAutocloser::exec(std::string id) {
  std::vector<TAutocloser::Segment> segments;
  compute(segments);
  draw(segments);

  // Save to cache using the inline function
  setSegmentCache(id, std::move(segments));
}

/*------------------------------------------------------------------------*/

TAutocloser::~TAutocloser() {}

/*------------------------------------------------------------------------*/

void TAutocloser::compute(std::vector<Segment> &closingSegmentArray) {
  m_imp->compute(closingSegmentArray);
  if (TRasterCM32P raux = (TRasterCM32P)m_imp->m_raster) {
    if (!m_autoPaintStyles.empty()) {
      closingSegmentArray.erase(
          std::remove_if(closingSegmentArray.begin(), closingSegmentArray.end(),
                         [&](const std::pair<TPoint, TPoint> &seg) {
                           TPixelCM32 *pix1 =
                               raux->pixels(seg.first.y) + seg.first.x;
                           TPixelCM32 *pix2 =
                               raux->pixels(seg.second.y) + seg.second.x;

                           return m_autoPaintStyles.find(pix1->getInk()) !=
                                      m_autoPaintStyles.end() ||
                                  m_autoPaintStyles.find(pix2->getInk()) !=
                                      m_autoPaintStyles.end();
                         }),
          closingSegmentArray.end());
    }
  }
}
/*------------------------------------------------------------------------*/

void TAutocloser::draw(const std::vector<Segment> &closingSegmentArray) {
  m_imp->draw(closingSegmentArray);
}
