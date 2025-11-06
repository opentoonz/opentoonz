

// #define AUTOCLOSE_DEBUG

#include "texception.h"
#include "toonz/autoclose.h"
#include "trastercm.h"
#include "skeletonlut.h"
#include "toonz/fill.h"
#include <set>
#include <queue>
#include <unordered_set>

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

const bool isSmallEnclosedRegion(const TRasterCM32P &ras, int x, int y,
                                 int maxSize) {
  if (!ras || x < 0 || y < 0 || x >= ras->getLx() || y >= ras->getLy())
    return false;

  TPixelCM32 *startRow = ras->pixels(y);
  if (!startRow[x].isPurePaint()) return false;

  int paint = startRow[x].getPaint();

  static std::queue<TPoint> queue;
  static std::unordered_set<int> visited;

  queue = std::queue<TPoint>();
  visited.clear();
  visited.reserve(maxSize + 1);
  visited.insert(y * ras->getLx() + x);
  queue.push(TPoint(x, y));

  while (!queue.empty() && visited.size() <= maxSize) {
    TPoint p = queue.front();
    queue.pop();

    const int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int i = 0; i < 4; i++) {
      int nx = p.x + directions[i][0];
      int ny = p.y + directions[i][1];

      if (nx < 0 || ny < 0 || nx >= ras->getLx() || ny >= ras->getLy()) {
        return false;
      }

      int key = ny * ras->getLx() + nx;
      if (visited.find(key) != visited.end()) continue;

      TPixelCM32 *row = ras->pixels(ny);
      if (row[nx].getPaint() == paint && row[nx].isPurePaint()) {
        visited.insert(key);
        queue.push(TPoint(nx, ny));
        if (visited.size() > maxSize) return false;
      }
    }
  }

  return visited.size() <= maxSize;
}

bool doSegmentsIntersect(int x1, int y1, int x2, int y2, int x3, int y3, int x4,
                         int y4) {
  auto cross = [](int x1, int y1, int x2, int y2) -> int {
    return x1 * y2 - y1 * x2;
  };

  int dx1 = x2 - x1;
  int dy1 = y2 - y1;
  int dx2 = x4 - x3;
  int dy2 = y4 - y3;

  int d1 = cross(dx2, dy2, x1 - x3, y1 - y3);
  int d2 = cross(dx2, dy2, x2 - x3, y2 - y3);
  int d3 = cross(dx1, dy1, x3 - x1, y3 - y1);
  int d4 = cross(dx1, dy1, x4 - x1, y4 - y1);

  if (d1 == 0 || d2 == 0 || d3 == 0 || d4 == 0) return false;

  return (d1 * d2 < 0) && (d3 * d4 < 0);
}

// For Paint defined Region
void closeSegment(const TRasterCM32P &r, const TAutocloser::Segment &s,
                  const USHORT ink, const USHORT opacity) {
  int x1 = s.first.x, y1 = s.first.y;
  int x2 = s.second.x, y2 = s.second.y;

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
  std::vector<TPoint> points;
  int lastX = x, lastY = y;
  while (true) {
    TPixelCM32 *pix = r->pixels(y) + x;
    // Only check side pixels if current line pixel is purePaint
    if (pix->isPurePaint()) {
      pix->setInk(ink);
      pix->setTone(255 - opacity);
      bool shouldKeep = true;
      int linePaint   = pix->getPaint();

      int sx1        = x + nx;
      int sy1        = y + ny;
      int side1Paint = -1;
      int side1Tone  = -1;
      if (sx1 >= 0 && sx1 < r->getLx() && sy1 >= 0 && sy1 < r->getLy()) {
        side1Paint = r->pixels(sy1)[sx1].getPaint();
        side1Tone  = r->pixels(sy1)[sx1].getTone();
      }

      int sx2        = x - nx;
      int sy2        = y - ny;
      int side2Paint = -1;
      int side2Tone  = -1;
      if (sx2 >= 0 && sx2 < r->getLx() && sy2 >= 0 && sy2 < r->getLy()) {
        side2Tone  = r->pixels(sy2)[sx2].getTone();
        side2Paint = r->pixels(sy2)[sx2].getPaint();
      }
      bool connected = false;
      if (side1Tone == side2Tone && side1Tone == TPixelCM32::getMaxTone() &&
          side1Paint == side2Paint && linePaint == side1Paint) {
        // Check if Direct Connected
        connected = true;
      } else if (side1Tone != side2Tone &&
                 (side1Tone == TPixelCM32::getMaxTone() ||
                  side2Tone == TPixelCM32::getMaxTone())) {
        // Check if Cornor Connected
        int side3Tone  = -1;
        int side4Tone  = -1;
        int side3Paint = -1;
        int side4Paint = -1;

        int sx3 = x + ny;
        int sy3 = y + nx;
        if (sx3 >= 0 && sx3 < r->getLx() && sy3 >= 0 && sy3 < r->getLy()) {
          side3Tone  = r->pixels(sy3)[sx3].getTone();
          side3Paint = r->pixels(sy3)[sx3].getPaint();
        }

        int sx4 = x - ny;
        int sy4 = y - nx;
        if (sx4 >= 0 && sx4 < r->getLx() && sy4 >= 0 && sy4 < r->getLy()) {
          side4Tone  = r->pixels(sy4)[sx4].getTone();
          side4Paint = r->pixels(sy4)[sx4].getPaint();
        }

        const int MAX_TONE = TPixelCM32::getMaxTone();

        int nextX = x, nextY = y;
        int nextErr = err;
        int next_e2 = 2 * nextErr;

        bool moveX = (next_e2 > -abs_dy);
        bool moveY = (next_e2 < abs_dx);

        if (moveX) {
          nextErr -= abs_dy;
          nextX += sx;
        }
        if (moveY) {
          nextErr += abs_dx;
          nextY += sy;
        }

        if (side1Tone > side2Tone) {
          bool intersect3 = doSegmentsIntersect(lastX, lastY, nextX, nextY, sx1,
                                                sy1, sx3, sy3);

          connected |= (side1Paint == linePaint && side3Tone == MAX_TONE &&
                        side3Paint == linePaint && intersect3);

          bool intersect4 = doSegmentsIntersect(lastX, lastY, nextX, nextY, sx1,
                                                sy1, sx4, sy4);
          connected |= (side1Paint == linePaint && side4Tone == MAX_TONE &&
                        side4Paint == linePaint && intersect4);
        } else {
          bool intersect3 = doSegmentsIntersect(lastX, lastY, nextX, nextY, sx2,
                                                sy2, sx3, sy3);
          connected |= (side2Paint == linePaint && side3Tone == MAX_TONE &&
                        side3Paint == linePaint && intersect3);

          bool intersect4 = doSegmentsIntersect(lastX, lastY, nextX, nextY, sx2,
                                                sy2, sx4, sy4);
          connected |= (side2Paint == linePaint && side4Tone == MAX_TONE &&
                        side4Paint == linePaint && intersect4);
        }
      }
      if (connected) {
        bool side1Small = isSmallEnclosedRegion(r, sx1, sy1, 4);
        bool side2Small = isSmallEnclosedRegion(r, sx2, sy2, 4);
        if (side1Small || side2Small) {
          pix->setTone(TPixelCM32::getMaxTone());
          pix->setInk(0);
          shouldKeep = false;
        }
      } else if (!((x == x1 && y == y1) || (x == x2 && y == y2))) {
        pix->setTone(TPixelCM32::getMaxTone());
        pix->setInk(0);
        shouldKeep = false;
      }
      if (shouldKeep) points.push_back(TPoint(x, y));
    }

    if (x == x2 && y == y2) break;
    lastX  = x;
    lastY  = y;
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

  // Clear lonely pixels (Intersection Point)
  // Mostly an endpoint of one gap
  // It's also one point on the other gap close line
  if (points.size() == 1 && points.front() != s.first &&
      points.front() != s.second)
    return;
  for (auto [x, y] : points) {
    TPixelCM32 *pix = r->pixels(y) + x;
    bool lonely     = true;
    int paint       = r->pixels(y)[x].getPaint();
    const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; ++i) {
      int nx          = x + dx[i];
      int ny          = y + dy[i];
      TPixelCM32 *pix = r->pixels(ny) + nx;
      if (nx >= 0 && nx < r->getLx() && ny >= 0 && ny < r->getLy()) {
        if (pix->getInk() == ink && pix->getPaint() == paint) {
          lonely = false;
          break;
        }
      }
    }

    if (lonely) {
      pix->setTone(TPixelCM32::getMaxTone());
      pix->setInk(0);
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
    assert(closingSegmentArray.empty());

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

  if (DEF_REGION_WITH_PAINT)
    for (int i = 0; i < (int)closingSegmentArray.size(); i++)
      closeSegment(raux, closingSegmentArray[i], m_inkIndex, m_opacity);
  else
    for (int i = 0; i < (int)closingSegmentArray.size(); i++)
      drawSegment(raux, closingSegmentArray[i], m_inkIndex, m_opacity);
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::copy(const TRasterGR8P &br, TRaster32P &r) {
  assert(r->getLx() == br->getLx() && r->getLy() == br->getLy());
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
  assert(points.size() == marks.size());

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
  int i, distance, current = 0, closerIndex;
  int sqrDistance = m_closingDistance * m_closingDistance;
  bool found      = 0;
  std::vector<bool> marks(endpoints.size());

  while (current < (int)endpoints.size() - 1) {
    found = 0;
    for (i = current + 1; i < (int)marks.size(); i++) marks[i] = false;
    distance = 0;

    while (!found && (distance <= sqrDistance) && !allMarked(marks, current)) {
      closerIndex = closerPoint(endpoints, marks, current);
      if (exploreTwoSpots(endpoints[current], endpoints[closerIndex]) &&
          notInsidePath(endpoints[current].first,
                        endpoints[closerIndex].first)) {
        drawInByteRaster(endpoints[current].first,
                         endpoints[closerIndex].first);
        closingSegments.push_back(
            Segment(endpoints[current].first, endpoints[closerIndex].first));

        if (!EndpointTable[neighboursCode(
                getPtr(endpoints[closerIndex].first))]) {
          std::vector<Segment>::iterator it = endpoints.begin();
          std::advance(it, closerIndex);
          endpoints.erase(it);
          std::vector<bool>::iterator it1 = marks.begin();
          std::advance(it1, closerIndex);
          marks.erase(it1);
        }
        found = true;
      }
    }

    if (found) {
      std::vector<Segment>::iterator it = endpoints.begin();
      std::advance(it, current);
      endpoints.erase(it);
      std::vector<bool>::iterator it1 = marks.begin();
      std::advance(it1, current);
      marks.erase(it1);
    } else
      current++;
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
     * printf("che palle!!!!!!\n");*/

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
    } else if (p1.y > ly) {
      p1.x =
          tround(p0.x - (float)((p0.x - p1.x) * (p0.y - ly)) / (p0.y - p1.y));
      p1.y = ly;
    }
    it++;
  }
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::spotResearchOnePoint(
    std::vector<Segment> &endpoints, std::vector<Segment> &closingSegments) {
  int count = 0;
  bool ret  = false;

  while (count < (int)endpoints.size()) {
    TPoint p;

    if (exploreSpot(endpoints[count], p)) {
      Segment segment(endpoints[count].first, p);
      std::vector<Segment>::iterator it =
          std::find(closingSegments.begin(), closingSegments.end(), segment);
      if (it == closingSegments.end() &&
          notInsidePath(endpoints[count].first, p)) {
        ret = true;
        drawInByteRaster(endpoints[count].first, p);
        closingSegments.push_back(Segment(endpoints[count].first, p));
        cancelFromArray(endpoints, p, count);
        if (!EndpointTable[neighboursCode(getPtr(endpoints[count].first))]) {
          std::vector<Segment>::iterator it = endpoints.begin();
          std::advance(it, count);
          endpoints.erase(it);
          continue;
        }
      }
    }
    count++;
  }

  return ret;
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

/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/
/*=============================================================================*/

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

        if (preseed != 8) /*non e' un pixel isolato*/
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
    *walker |= 0x4; /* metto la marca di passaggio */

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

        } else /* il seed e' stato cancellato */
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
        assert(i != count);
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
//...............................

void TAutocloser::exec() {
  std::vector<TAutocloser::Segment> segments;
  compute(segments);
  draw(segments);
}

void TAutocloser::exec(std::string id) {
  std::vector<TAutocloser::Segment> segments;
  compute(segments);
  draw(segments);
  setSegmentCache(id, std::move(segments));
}

//...............................

TAutocloser::~TAutocloser() {}

//-------------------------------------------------

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
//-------------------------------------------------

void TAutocloser::draw(const std::vector<Segment> &closingSegmentArray) {
  m_imp->draw(closingSegmentArray);
}