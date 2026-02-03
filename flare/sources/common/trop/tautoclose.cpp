

#include "texception.h"
#include "tautoclose.h"
#include "trastercm.h"
#include "skeletonlut.h"

#define AUT_SPOT_SAMPLES 10

using namespace SkeletonLut;

class TAutocloser::Imp {
public:
  struct Seed {
    UCHAR *m_ptr;
    UCHAR m_preseed;
    Seed(UCHAR *ptr, UCHAR preseed) : m_ptr(ptr), m_preseed(preseed) {}
  };

  int m_closingDistance;
  double m_spotAngle;
  int m_inkIndex;
  TRasterP m_raster;
  TRasterGR8P m_bRaster;
  UCHAR *m_br;

  int m_bWrap;

  int m_displaceVector[8];
  TPointD m_displAverage;
  int m_visited;

  double m_csp, m_snp, m_csm, m_snm, m_csa, m_sna, m_csb, m_snb;

  Imp(const TRasterP &r)
      : m_raster(r)
      , m_spotAngle((180 * TConsts::pi) / 360.0)
      , m_closingDistance(10)
      , m_inkIndex(0) {}

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
  void compute(vector<Segment> &closingSegmentArray);
  void skeletonize(vector<TPoint> &endpoints);
  void findSeeds(vector<Seed> &seeds, vector<TPoint> &endpoints);
  void erase(vector<Seed> &seeds, vector<TPoint> &endpoints);
  void circuitAndMark(UCHAR *seed, UCHAR preseed);
  bool circuitAndCancel(UCHAR *seed, UCHAR preseed, vector<TPoint> &endpoints);
  void findMeetingPoints(vector<TPoint> &endpoints,
                         vector<Segment> &closingSegments);
  void calculateWeightAndDirection(vector<Segment> &orientedEndpoints);
  bool spotResearchTwoPoints(vector<Segment> &endpoints,
                             vector<Segment> &closingSegments);
  bool spotResearchOnePoint(vector<Segment> &endpoints,
                            vector<Segment> &closingSegments);

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
  void cancelFromArray(vector<Segment> &array, TPoint p, int &count);
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

TRasterGR8P makeByteRaster(const TRasterCM32P &r) {
  int lx = r->getLx();
  int ly = r->getLy();
  TRasterGR8P bRaster(lx + 4, ly + 4);
  int i, j;

  // bRaster->create(lx+4, ly+4);
  bRaster->lock();
  UCHAR *br = bRaster->getRawData();

  for (i = 0; i < lx + 4; i++) *(br++) = 0;

  for (i = 0; i < lx + 4; i++) *(br++) = 131;

  r->lock();
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
  r->unlock();
  for (i = 0; i < lx + 4; i++) *(br++) = 131;

  for (i = 0; i < lx + 4; i++) *(br++) = 0;

  bRaster->unlock();
  return bRaster;
}

/*------------------------------------------------------------------------*/

#define SET_INK                                                                \
  if (buf->getTone() == buf->getMaxTone()) *buf = TPixelCM32(inkIndex, 22, 0);

void drawSegment(TRasterCM32P &r, const TAutocloser::Segment &s,
                 USHORT inkIndex) {
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
}

/*------------------------------------------------------------------------*/

}  // namespace
/*------------------------------------------------------------------------*/

void TAutocloser::Imp::compute(vector<Segment> &closingSegmentArray) {
  vector<TPoint> endpoints;
  try {
    assert(closingSegmentArray.empty());

    TRasterCM32P raux;

    if (!(raux = (TRasterCM32P)m_raster))
      throw TException("Unable to autoclose a not CM32 image.");

    if (m_raster->getLx() == 0 || m_raster->getLy() == 0)
      throw TException("Autoclose error: bad image size");

    // Lx = r->lx;
    // Ly = r->ly;

    TRasterGR8P braux = makeByteRaster(raux);

    m_bRaster =
        braux->extract(TRect(2, 2, braux->getLx() - 3, braux->getLy() - 3));
    m_bRaster->lock();
    m_br    = m_bRaster->getRawData();
    m_bWrap = m_bRaster->getWrap();

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
    raux->lock();
    for (int i = 0; i < (int)closingSegmentArray.size(); i++)
      drawSegment(raux, closingSegmentArray[i], m_inkIndex);
    raux->unlock();
    // copy(m_bRaster, raux);
    m_bRaster->unlock();
    m_br = 0;
  }

  catch (TException &e) {
    throw e;
  }
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::copy(const TRasterGR8P &br, TRaster32P &r) {
  assert(r->getLx() == br->getLx() && r->getLy() == br->getLy());
  int i, j;

  int lx = r->getLx();
  int ly = r->getLy();
  br->lock();
  r->lock();
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

  br->unlock();
  r->unlock();
}

/*=============================================================================*/

namespace {

int intersect_segment(int x1, int y1, int x2, int y2, int i, double *ris) {
  if ((i < tmin(y1, y2)) || (i > tmax(y1, y2)) || (y1 == y2)) return 0;

  *ris = ((double)((x1 - x2) * (i - y2)) / (double)(y1 - y2) + x2);

  return 1;
}

/*=============================================================================*/

inline int distance2(const TPoint p0, const TPoint p1) {
  return (p0.x - p1.x) * (p0.x - p1.x) + (p0.y - p1.y) * (p0.y - p1.y);
}

/*=============================================================================*/

int closerPoint(const vector<TAutocloser::Segment> &points, vector<bool> &marks,
                int index) {
  assert(points.size() == marks.size());

  int min, curr;
  int minval = 9999999;

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

  miny = tmax(tmin(y1a, y2a, y3a), tmin(y1b, y2b, y3b));
  maxy = tmin(tmax(y1a, y2a, y3a), tmax(y1b, y2b, y3b));
  if (maxy < miny) return 0;

  minx = tmax(tmin(x1a, x2a, x3a), tmin(x1b, x2b, x3b));
  maxx = tmin(tmax(x1a, x2a, x3a), tmax(x1b, x2b, x3b));
  if (maxx < minx) return 0;

  for (i = miny; i <= maxy; i++) {
    xamin = xamax = xbmin = xbmax = 0.0;

    intersect_segment(x1a, y1a, x2a, y2a, i, &xamin);

    if (intersect_segment(x1a, y1a, x3a, y3a, i, &val))
      if (xamin)
        xamax = val;
      else
        xamin = val;

    if (!xamax) intersect_segment(x2a, y2a, x3a, y3a, i, &xamax);

    if (xamax < xamin) {
      val = xamin, xamin = xamax, xamax = val;
    }

    intersect_segment(x1b, y1b, x2b, y2b, i, &xbmin);

    if (intersect_segment(x1b, y1b, x3b, y3b, i, &val))
      if (xbmin)
        xbmax = val;
      else
        xbmin = val;

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

void TAutocloser::Imp::findMeetingPoints(vector<TPoint> &endpoints,
                                         vector<Segment> &closingSegments) {
  int i;
  double alfa;
  alfa  = m_spotAngle / AUT_SPOT_SAMPLES;
  m_csp = cos(m_spotAngle / 5);
  m_snp = sin(m_spotAngle / 5);
  m_csm = cos(-m_spotAngle / 5);
  m_snm = sin(-m_spotAngle / 5);
  m_csa = cos(alfa);
  m_sna = sin(alfa);
  m_csb = cos(-alfa);
  m_snb = sin(-alfa);

  vector<Segment> orientedEndpoints(endpoints.size());
  for (i                       = 0; i < (int)endpoints.size(); i++)
    orientedEndpoints[i].first = endpoints[i];

  int size = -1;

  while ((int)closingSegments.size() > size && !orientedEndpoints.empty()) {
    size = closingSegments.size();
    do
      calculateWeightAndDirection(orientedEndpoints);
    while (spotResearchTwoPoints(orientedEndpoints, closingSegments));

    do
      calculateWeightAndDirection(orientedEndpoints);
    while (spotResearchOnePoint(orientedEndpoints, closingSegments));
  }
}

/*------------------------------------------------------------------------*/

bool allMarked(const vector<bool> &marks, int index) {
  int i;

  for (i = index + 1; i < (int)marks.size(); i++)
    if (!marks[i]) return false;
  return true;
}

/*------------------------------------------------------------------------*/

bool TAutocloser::Imp::spotResearchTwoPoints(vector<Segment> &endpoints,
                                             vector<Segment> &closingSegments) {
  int i, distance, current = 0, closerIndex;
  int sqrDistance = m_closingDistance * m_closingDistance;
  bool found      = 0;
  vector<bool> marks(endpoints.size());

  while (current < (int)endpoints.size() - 1) {
    found = 0;
    for (i = current + 1; i < (int)marks.size(); i++) marks[i] = false;
    distance                                                   = 0;

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
    vector<Segment> &orientedEndpoints) {
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

bool TAutocloser::Imp::spotResearchOnePoint(vector<Segment> &endpoints,
                                            vector<Segment> &closingSegments) {
  int count = 0;
  bool ret  = false;

  while (count < (int)endpoints.size()) {
    TPoint p;

    if (exploreSpot(endpoints[count], p)) {
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

  for (i = 0; i < AUT_SPOT_SAMPLES; i++) {
    xnewa = x1 + (x2a - x1) * m_csa - (y2a - y1) * m_sna;
    ynewa = y1 + (y2a - y1) * m_csa + (x2a - x1) * m_sna;
    x3    = tround(xnewa);
    y3    = tround(ynewa);
    if ((x3 != tround(x2a) || y3 != tround(y2a)) && x3 > 0 && x3 < lx &&
        y3 > 0 && y3 < ly &&
        exploreRay(
            getPtr(x1, y1),
            Segment(TPoint(x1, y1), TPoint(tround(xnewa), tround(ynewa))), p))
      return true;

    x2a = xnewa;
    y2a = ynewa;

    xnewb = x1 + (x2b - x1) * m_csb - (y2b - y1) * m_snb;
    ynewb = y1 + (y2b - y1) * m_csb + (x2b - x1) * m_snb;
    x3    = tround(xnewb);
    y3    = tround(ynewb);
    if ((x3 != tround(x2b) || y3 != tround(y2b)) && x3 > 0 && x3 < lx &&
        y3 > 0 && y3 < ly &&
        exploreRay(
            getPtr(x1, y1),
            Segment(TPoint(x1, y1), TPoint(tround(xnewb), tround(ynewb))), p))
      return true;

    x2b = xnewb;
    y2b = ynewb;
  }
  return false;
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

void TAutocloser::Imp::skeletonize(vector<TPoint> &endpoints) {
  vector<Seed> seeds;

  findSeeds(seeds, endpoints);

  erase(seeds, endpoints);
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::findSeeds(vector<Seed> &seeds,
                                 vector<TPoint> &endpoints) {
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
  assert(displ >= 0 && displ < 8);

  walker    = seed + m_displaceVector[displ];
  prewalker = displ ^ 0x7;

  while ((walker != seed) || (preseed != prewalker)) {
    *walker |= 0x4; /* metto la marca di passaggio */

    displ = NextPointTable[(neighboursCode(walker) << 3) | prewalker];
    assert(displ >= 0 && displ < 8);
    walker += m_displaceVector[displ];
    prewalker = displ ^ 0x7;
  }

  return;
}

/*------------------------------------------------------------------------*/

void TAutocloser::Imp::erase(vector<Seed> &seeds, vector<TPoint> &endpoints) {
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
          assert(displ >= 0 && displ < 8);
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
                                        vector<TPoint> &endpoints) {
  UCHAR *walker, *previous;
  UCHAR displ, prewalker;
  bool ret = false;

  displ = NextPointTable[(neighboursCode(seed) << 3) | preseed];
  assert(displ >= 0 && displ < 8);

  if ((displ == preseed) && !((*seed) & 0x8)) {
    endpoints.push_back(getCoordinates(seed));
    *seed |= 0x8;
  }

  walker    = seed + m_displaceVector[displ];
  prewalker = displ ^ 0x7;

  while ((walker != seed) || (preseed != prewalker)) {
    assert(prewalker >= 0 && prewalker < 8);
    displ = NextPointTable[(neighboursCode(walker) << 3) | prewalker];
    assert(displ >= 0 && displ < 8);

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

void TAutocloser::Imp::cancelFromArray(vector<Segment> &array, TPoint p,
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

TAutocloser::TAutocloser(const TRasterP &r) { m_imp = new Imp(r); }

//.......................

TAutocloser::~TAutocloser() { delete m_imp; }

//-------------------------------------------------

// if this function is never used, use default values
void TAutocloser::setClosingDistance(int d) { m_imp->m_closingDistance = d; }

//-------------------------------------------------

int TAutocloser::getClosingDistance() const { return m_imp->m_closingDistance; }

//-------------------------------------------------

void TAutocloser::setSpotAngle(double degrees) {
  if (degrees <= 0.0) degrees = 1.0;

  m_imp->m_spotAngle = (degrees * TConsts::pi) / 360.0;
}

//-------------------------------------------------

double TAutocloser::getSpotAngle() const { return m_imp->m_spotAngle; }

//-------------------------------------------------

void TAutocloser::setInkIndex(int inkIndex) { m_imp->m_inkIndex = inkIndex; }

//-------------------------------------------------

int TAutocloser::getInkIndex() const { return m_imp->m_inkIndex; }

//-------------------------------------------------

void TAutocloser::compute(vector<Segment> &closingSegmentArray) {
  m_imp->compute(closingSegmentArray);
}
//-------------------------------------------------
