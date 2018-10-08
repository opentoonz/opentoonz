

#include "tregion.h"

// TnzCore includes
#include "tregionprop.h"
#include "tcurves.h"
#include "tstroke.h"
#include "tcurveutil.h"

// tcg includes
#include "tcg/numeric_ops.h"
#include "tcg/poly_ops.h"

// STD includes
#include <set>

// DEFINE_CLASS_CODE(TEdge, 40)

//=============================================================================
/*
void foo()
{
  TEdgeP p1 = new TEdge();
  TEdgeP p2 = new TEdge(*p1);
  p1 = 0;

  TEdge *e = new TEdge;



}
*/

//=============================================================================

static bool compareEdge(const TEdge &a, const TEdge &b) {
  return a.m_s == b.m_s;
}

//-----------------------------------------------------------------------------

class TRegion::Imp {
  double m_polyStep;

public:
  TRegionProp *m_prop;

  mutable TRectD m_bBox;
  mutable bool m_isValidBBox;

  std::vector<TEdge *> m_edge;
  std::vector<TRegion *> m_includedRegionArray;

public:
  Imp()
      : m_polyStep(-1)
      , m_prop(0)
      , m_bBox()
      , m_isValidBBox(true)
      , m_edge()
      , m_includedRegionArray() {}

  ~Imp() {
    delete m_prop;
    for (UINT i = 0; i < m_includedRegionArray.size(); i++)
      delete m_includedRegionArray[i];
  }

  void printContains(const TPointD &p) const;

  void invalidateBBox() { m_isValidBBox = false; }

#ifdef _DEBUG
  void checkRegion() {
    for (UINT i = 0; i < m_edge.size(); i++) {
      TEdge *e = m_edge[i];
      assert(e->getStyle() >= 0 && e->getStyle() <= 1000);
      assert(e->m_w0 >= 0 && e->m_w1 <= 1);
      assert(e->m_s->getChunkCount() >= 0 && e->m_s->getChunkCount() <= 10000);
    }
  }
#endif

  TRectD getBBox() const {
    if (!m_isValidBBox) {
      m_bBox = TRectD();

      for (UINT i = 0; i < m_edge.size(); i++)
        m_bBox +=
            m_edge[i]->m_s->getBBox(std::min(m_edge[i]->m_w0, m_edge[i]->m_w1),
                                    std::max(m_edge[i]->m_w0, m_edge[i]->m_w1));

      m_isValidBBox = true;
    }
    return m_bBox;
  }

  bool contains(const TPointD &p) const;
  bool contains(const TRegion::Imp &p) const;

  // this function returns true only if p is contained in the region, taking
  // into account holes in it.
  bool noSubregionContains(const TPointD &p) const;
  void addSubregion(TRegion *region);
  // bool getPointInside(TPointD &p) const;
  bool slowContains(const TRegion::Imp &r) const;
  bool contains(const TStroke &s, bool mayIntersect) const;
  bool isSubRegionOf(const TRegion::Imp &r) const;
  bool getInternalPoint(TPointD &p, double left, double right, double y);
  void computeScanlineIntersections(double y,
                                    std::vector<double> &intersections) const;
  bool thereAreintersections(const TStroke &s) const;

  int leftScanlineIntersections(const TPointD &p, double(TPointD::*h),
                                double(TPointD::*v)) const;
};

//=============================================================================

TRegion *TRegion::findRegion(const TRegion &r) const {
  if (areAlmostEqual(r.getBBox(), getBBox(), 1e-3)) return (TRegion *)this;

  if (!getBBox().contains(r.getBBox())) return 0;
  TRegion *ret;

  for (UINT i = 0; i < m_imp->m_includedRegionArray.size(); i++)
    if ((ret = m_imp->m_includedRegionArray[i]->findRegion(r)) != 0) return ret;

  return 0;
}

//=============================================================================

TRegion::TRegion() : m_imp(new TRegion::Imp()) {
  // m_imp->m_fillStyle->setRegion(this);
}

//-----------------------------------------------------------------------------

TRegion::~TRegion() {}

//-----------------------------------------------------------------------------
/*
bool TRegion::Imp::contains(const TPointD &p) const
{
  bool leftIntersectionsAreOdd=false, rightIntersectionsAreOdd=false;

  if (!getBBox().contains(p))
    return false;

  vector<TPointD> poly;
  UINT i=0 ;
  //region2polyline(poly, *this);

  for(; i<m_edge.size(); i++)
    stroke2polyline( poly, *m_edge[i]->m_s, 1.0, m_edge[i]->m_w0,
m_edge[i]->m_w1);

  poly.push_back(poly.front());

  TRectD bbox = getBBox();

  double dist = (bbox.x1-bbox.x0)*0.5;

  TSegment horizSegment = TSegment(TPointD(bbox.x0-dist,
p.y),TPointD(bbox.x1+dist, p.y));

  for (i=0; i<poly.size()-1; i++)
  {
    vector<DoublePair> intersections;

    if (poly[i].y==poly[i+1].y && poly[i+1].y==p.y)
      continue;

    if (intersect(TSegment(poly[i], poly[i+1]), horizSegment,
      intersections))
    {
      assert(intersections.size()==1);

      TPointD pInt = horizSegment.getPoint(intersections[0].second);
      if (pInt==poly[i+1])
        continue;
      if (pInt.x>p.x)
        rightIntersectionsAreOdd = !rightIntersectionsAreOdd;
      else
        leftIntersectionsAreOdd = !leftIntersectionsAreOdd;
    }
  }


  //assert(!(leftIntersectionsAreOdd^rightIntersectionsAreOdd)); //intersections
must be even!

  return leftIntersectionsAreOdd;
}
*/
//-----------------------------------------------------------------------------

// questa funzione fa l'intersezione della porzione [t0, t1) della quadratica q
// con una retta orizzontale passante per p.y,
// e setta i due booleani in base a quante intersezioni stanno a sx e a dx di p

// il valore di ritorno dice se l'intersezione e' ad un estremo e se la curva
// sta tutta sopra(1) o tutta sotto) -1;
// questo valore viene riusato come input della successiva chiamata a findSide:
// se anche questa ha l'intersezione allo stesso estremo e sullo stesso
// lato (cuspide) quell'intersezione non conta(doppia, come con la tangente)

//-----------------------------------------------------------------------------

namespace {

inline int computeSide(const TQuadratic &q, double t, bool forward) {
  double speedY = q.getSpeedY(t);
  if (speedY == 0)  // se la tangente e' zero, non si riesce a capire su che
                    // semipiano sta la curva rispetto alla semiretta
                    // orizzontale campione. in questo caso e' sufficiente
                    // vedere dove giace il terzo controlpoint della quad .
    speedY =
        (forward
             ? (q.getP2().y - q.getPoint(t).y)
             : (q.getPoint(t).y -
                q.getP0().y));  // q.getSpeedY(t+(forward?0.00001:-0.00001));
  return speedY > 0 ? 1 : -1;
}

inline void computeIntersection(const TQuadratic &q, double t, double t0,
                                double t1, double x, int sideOfPrevious,
                                bool &leftAreOdd) {
  if (((t0 < t1 && t >= t0 && t < t1) || (t0 > t1 && t > t1 && t <= t0)) &&
      q.getX(t) <= x) {
    if (t == t0) {
      assert(sideOfPrevious != 0);
      if (computeSide(q, t0, t0 < t1) !=
          sideOfPrevious)  // cuspide! non considero l'intersezione
        return;
    }
    leftAreOdd = !leftAreOdd;
  }
}

//-----------------------------------------------------------------------------

__inline int findSides(const TPointD &p, const TQuadratic &q, double t0,
                       double t1, bool &leftAreOdd, int sideOfPrevious) {
  TRectD bbox = q.getBBox();

  // assert(!(t0==t1 && q.getPoint(t0).y==p.y));

  if (bbox.y0 > p.y || bbox.y1 < p.y) return 0;

  if (t0 == t1) return sideOfPrevious;

  double y0 = q.getP0().y;
  double y1 = q.getP1().y;
  double y2 = q.getP2().y;

  /* ottimizzazione....lenta.
if((q.getPoint(t0).y-p.y)*(q.getPoint(t1).y-p.y)<0)
{
   if(bbox.x1<p.x) {leftAreOdd = !leftAreOdd; return; }
   else if(bbox.x0>p.x) {rightAreOdd = !rightAreOdd; return; }
  }
*/

  double det;
  double alfa = y0 - 2 * y1 + y2;

  if (!areAlmostEqual(
          alfa, 0,
          1e-10))  // alfa, il coefficiente di t^2, non e' zero: due soluzioni
  {
    det = y1 * y1 - y0 * y2 + p.y * alfa;
    if (det < 0 ||
        (det == 0 && y0 != p.y &&
         y2 !=
             p.y))  // con det<0 no soluzioni reali o due soluzioni coincidenti
      // (a meno che le soluzioni non siano agli estremi, in quel caso e'
      //  una sola!), due intersezioni stesso lato, posso scartare
      return 0;
    else {
      double ta, tb;
      det = sqrt(det);
      ta = tb = (y0 - y1 + det) / alfa;
      computeIntersection(q, ta, t0, t1, p.x, sideOfPrevious, leftAreOdd);

      if (det != 0) {
        tb = (y0 - y1 - det) / alfa;
        computeIntersection(q, tb, t0, t1, p.x, sideOfPrevious, leftAreOdd);
      }

      if (ta == t1 || tb == t1)
        return computeSide(q, t1, t1 < t0);  // q.getSpeedY(t1)>0?1:-1;
      else
        return 0;
    }
  } else  // alfa, il coefficiente di t^2 e' zero: una  sola soluzione
  {
    if (y2 == y0)  // segmento orizzontale
      return sideOfPrevious;
    double t = (p.y - y0) / (y2 - y0);

    if ((t0 < t1 && t >= t0 && t < t1) || (t0 > t1 && t > t1 && t <= t0)) {
      if ((q.getP2().x - q.getP0().x) * t + q.getP0().x <= p.x) {
        leftAreOdd = !leftAreOdd;
        if (t == t0) {
          assert(sideOfPrevious != 0);
          if (((y2 - y0 > 0) ? 1 : -1) != sideOfPrevious)
            leftAreOdd = !leftAreOdd;
        }
      }
    }
    if (t == t1)
      return (y2 - y0 > 0) ? 1 : -1;  // q.getPoint(t0).y>p.y?1:-1;
    else
      return 0;
  }
}

//-----------------------------------------------------------------------------

void addIntersection(const TQuadratic &q, double t, double t0, double t1,
                     std::vector<double> &intersections, double intersection,
                     std::vector<int> &sides) {
  int side = 0;

  if (areAlmostEqual(t, t0, 1e-4))
    side =
        (q.getPoint(t0 + ((t1 > t0) ? 0.01 : -0.01)).y - q.getPoint(t0).y) > 0
            ? 1
            : -1;
  else if (areAlmostEqual(t, t1, 1e-4))
    side =
        (q.getPoint(t1 + ((t0 > t1) ? 0.01 : -0.01)).y - q.getPoint(t1).y) > 0
            ? 1
            : -1;

  if (!intersections.empty() &&
      areAlmostEqual(intersection, intersections.back(), 1e-4)) {
    // assert(areAlmostEqual(t, t0, 1e-3));
    assert(sides.back() != 0);

    if (side == sides.back()) {
      intersections.pop_back();
      sides.pop_back();
    }
  } else {
    intersections.push_back(intersection);
    sides.push_back(side);
  }
}

//-----------------------------------------------------------------------------

void findIntersections(double y, const TQuadratic &q, double t0, double t1,
                       std::vector<double> &intersections,
                       std::vector<int> &sides) {
  TRectD bbox = q.getBBox();

  int side = 0;
  if (t0 == t1 || bbox.y0 > y || bbox.y1 < y) return;

  double y0 = q.getP0().y;
  double y1 = q.getP1().y;
  double y2 = q.getP2().y;

  double alfa = y0 - 2 * y1 + y2;

  if (!areAlmostEqual(alfa, 0, 1e-10))  // la quadratica non e' un segmento
  {
    double det = y1 * y1 - y0 * y2 + y * alfa;
    assert(det >= 0);

    if (det < 1e-6)  // con det==0 soluzioni coincidenti
    {
      double t = (y0 - y1) / alfa;
      if (areAlmostEqual(t, t0, 1e-5) || areAlmostEqual(t, t1, 1e-5)) {
        double s = 1 - t;
        double intersection =
            q.getP0().x * s * s + 2 * t * s * q.getP1().x + t * t * q.getP2().x;
        addIntersection(q, t, t0, t1, intersections, intersection, sides);
      }
    } else {
      double ta, tb;
      bool rev = q.getPoint(t0).x > q.getPoint(t1).x;
      // if (alfa<0) rev = !rev;

      det = sqrt(det);

      ta                   = (y0 - y1 + det) / alfa;
      double sa            = 1 - ta;
      double intersectiona = q.getP0().x * sa * sa + 2 * ta * sa * q.getP1().x +
                             ta * ta * q.getP2().x;

      tb                   = (y0 - y1 - det) / alfa;
      double sb            = 1 - tb;
      double intersectionb = q.getP0().x * sb * sb + 2 * tb * sb * q.getP1().x +
                             tb * tb * q.getP2().x;

      if ((rev && intersectiona < intersectionb) ||
          (!rev && intersectiona > intersectionb))
        std::swap(intersectiona, intersectionb), std::swap(ta, tb);

      if ((t0 < t1 && ta >= t0 && ta <= t1) ||
          (t0 >= t1 && ta >= t1 && ta <= t0))
        addIntersection(q, ta, t0, t1, intersections, intersectiona, sides);

      if ((t0 < t1 && tb >= t0 && tb <= t1) ||
          (t0 >= t1 && tb >= t1 && tb <= t0))
        addIntersection(q, tb, t0, t1, intersections, intersectionb, sides);
    }
  } else if (y2 != y0)  // la quadratica e' un segmento non orizzontale
  {
    if (y2 == y0) return;
    double t = (y - y0) / (y2 - y0);

    if (!((t0 < t1 && t >= t0 && t <= t1) || (t0 >= t1 && t >= t1 && t <= t0)))
      return;

    double intersection = (q.getP2().x - q.getP0().x) * t + q.getP0().x;
    if (areAlmostEqual(t, t1, 1e-4))
      side = (q.getPoint(t0).y > q.getPoint(t1).y) ? 1 : -1;
    else if (areAlmostEqual(t, t0, 1e-4))
      side = (q.getPoint(t1).y > q.getPoint(t0).y) ? 1 : -1;
    if (!intersections.empty() &&
        areAlmostEqual(intersection, intersections.back(), 1e-4)) {
      // assert(areAlmostEqual(t, t0, 1e-4));
      assert(sides.back() != 0);
      assert(side != 0);
      if (side == sides.back()) {
        intersections.pop_back();
        sides.pop_back();
      }
    } else {
      intersections.push_back(intersection);
      sides.push_back(side);
    }

  } else  // la quadratica e' un segmento orizzontale
    findIntersections(
        y, TQuadratic(q.getPoint(t0),
                      0.5 * (q.getPoint(t0) + q.getPoint(t1)) + TPointD(0, 1.0),
                      q.getPoint(t1)),
        0, 1, intersections, sides);
}
}
//-----------------------------------------------------------------------------

bool TRegion::contains(const TPointD &p) const { return m_imp->contains(p); }

//-----------------------------------------------------------------------------

bool TRegion::contains(const TStroke &s, bool mayIntersect) const {
  return m_imp->contains(s, mayIntersect);
}

//-----------------------------------------------------------------------------
#ifdef _DEBUG
void TRegion::checkRegion() { m_imp->checkRegion(); }
#endif
//-----------------------------------------------------------------------------

bool TRegion::contains(const TRegion &r) const {
  return m_imp->contains(*r.m_imp);
}

//-----------------------------------------------------------------------------
/*
bool TRegion::getPointInside(TPointD &p) const
{
  return m_imp->getPointInside(p);
}
*/
//-----------------------------------------------------------------------------

TRegionProp *TRegion::getProp() {
  // if(m_working)  buttato m_working
  return m_imp->m_prop;
  /*
int styleId = getStyle();
if(!styleId ) return 0;
TColorStyle * style = palette->getStyle(styleId);
if (!style->isRegionStyle() || style->isEnabled() == false)
return 0;
if( !m_imp->m_prop || style != m_imp->m_prop->getColorStyle() )
{
delete m_imp->m_prop;
m_imp->m_prop = style->makeRegionProp(this);
}
return m_imp->m_prop;
*/
}

//-----------------------------------------------------------------------------

void TRegion::setProp(TRegionProp *prop) {
  assert(prop);
  delete m_imp->m_prop;
  m_imp->m_prop = prop;
}

//-----------------------------------------------------------------------------

/*
void TRegion::draw(const TVectorRenderData &rd)
{

  int styleId = getStyle();

  if(!styleId )
    return;

  TColorStyle * style = rd.m_palette->getStyle(styleId);

  if (!style->isRegionStyle() || style->isEnabled() == false)
    return;


  if( !m_imp->m_prop || style != m_imp->m_prop->getColorStyle() )
  {
    delete m_imp->m_prop;
    m_imp->m_prop = style->makeRegionProp(this);
  }

  m_imp->m_prop->draw(rd);
}
*/

//-----------------------------------------------------------------------------

static void checkPolyline(const std::vector<T3DPointD> &p) {
  int ret;

  if (p.size() < 3) return;

  TPointD p1;
  TPointD p2;

  int pointSize = (int)p.size() - 1;

  for (int i = 0; i < pointSize; i++) {
    for (int j = i + 1; j < pointSize; j++) {
      std::vector<DoublePair> res;

      p1 = TPointD(p[i].x, p[i].y);
      p2 = TPointD(p[i + 1].x, p[i + 1].y);

      TSegment s0(p1, p2);

      p1 = TPointD(p[j].x, p[j].y);
      p2 = TPointD(p[j + 1].x, p[j + 1].y);

      TSegment s1(p1, p2);

      ret = intersect(s0, s1, res);
      if (ret)
        assert((ret == 1) && (areAlmostEqual(res[0].first, 1) ||
                              areAlmostEqual(res[0].first, 0)) &&
               (areAlmostEqual(res[0].second, 1) ||
                areAlmostEqual(res[0].second, 0)));
    }
  }

  p1 = TPointD(p.back().x, p.back().y);
  p2 = TPointD(p[0].x, p[0].y);

  TSegment s0(p1, p2);

  for (int j = 0; j < pointSize; j++) {
    std::vector<DoublePair> res;

    p1 = TPointD(p[j].x, p[j].y);
    p2 = TPointD(p[j + 1].x, p[j + 1].y);
    TSegment s1(p1, p2);

    ret = intersect(s0, s1, res);
    if (ret)
      assert((ret == 1) && (areAlmostEqual(res[0].first, 1) ||
                            areAlmostEqual(res[0].first, 0)) &&
             (areAlmostEqual(res[0].second, 1) ||
              areAlmostEqual(res[0].second, 0)));
  }
}

//-----------------------------------------------------------------------------

bool TRegion::Imp::getInternalPoint(TPointD &p, double left, double right,
                                    double y) {
  assert(left < right);

  if (areAlmostEqual(left, right, 1e-2)) return false;

  double mid = 0.5 * (left + right);

  p = TPointD(mid, y);

  if (noSubregionContains(p)) return true;

  if (!getInternalPoint(p, left, mid, y))
    return getInternalPoint(p, mid, right, y);
  else
    return true;
}

//-----------------------------------------------------------------------------

bool TRegion::Imp::noSubregionContains(const TPointD &p) const {
  if (contains(p)) {
    for (int i = 0; i < (int)m_includedRegionArray.size(); i++)
      if (m_includedRegionArray[i]->contains(p)) return false;
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

void TRegion::computeScanlineIntersections(
    double y, std::vector<double> &intersections) const {
  m_imp->computeScanlineIntersections(y, intersections);
}

//-----------------------------------------------------------------------------

void TRegion::Imp::computeScanlineIntersections(
    double y, std::vector<double> &intersections) const {
  TRectD bbox = getBBox();
  if (y <= bbox.y0 || y >= bbox.y1) return;

  assert(intersections.empty());

  UINT i, firstSide = 0;
  std::vector<int> sides;

  for (i = 0; i < m_edge.size(); i++) {
    TEdge *e = m_edge[i];

    TStroke *s = e->m_s;
    if (s->getBBox().y0 > y || s->getBBox().y1 < y) continue;
    int chunkIndex0, chunkIndex1;
    double t0, t1;
    s->getChunkAndT(e->m_w0, chunkIndex0, t0);
    s->getChunkAndT(e->m_w1, chunkIndex1, t1);

    if (chunkIndex0 > chunkIndex1) {
      findIntersections(y, *s->getChunk(chunkIndex0), t0, 0, intersections,
                        sides);
      for (int j = chunkIndex0 - 1; j > chunkIndex1; j--)
        findIntersections(y, *s->getChunk(j), 1, 0, intersections, sides);
      findIntersections(y, *s->getChunk(chunkIndex1), 1, t1, intersections,
                        sides);
    } else if (chunkIndex0 < chunkIndex1) {
      findIntersections(y, *s->getChunk(chunkIndex0), t0, 1, intersections,
                        sides);
      for (int j = chunkIndex0 + 1; j < chunkIndex1; j++)
        findIntersections(y, *s->getChunk(j), 0, 1, intersections, sides);
      findIntersections(y, *s->getChunk(chunkIndex1), 0, t1, intersections,
                        sides);
    } else {
      findIntersections(y, *s->getChunk(chunkIndex0), t0, t1, intersections,
                        sides);
    }
  }

  if (intersections.size() > 0 &&
      intersections.front() == intersections.back()) {
    intersections.pop_back();
    if (!sides.empty() && sides.front() == sides.back() &&
        intersections.size() > 0)
      intersections.erase(intersections.begin());
  }

  std::sort(intersections.begin(), intersections.end());
  assert(intersections.size() % 2 == 0);
}

//-----------------------------------------------------------------------------
bool TRegion::Imp::contains(const TPointD &p) const {
  bool leftAreOdd = false;

  if (!getBBox().contains(p)) return false;

  // printContains(p);

  UINT i;

  int side = 0;

  for (i = 0; i < m_edge.size() * 2; i++)  // i pari, esplora gli edge,
  // i dispari esplora i segmenti di autoclose tra un edge e il successivo
  {
    if (i & 0x1) {
      TPointD p0 = m_edge[i / 2]->m_s->getPoint(m_edge[i / 2]->m_w1);
      TPointD p1;
      if (i / 2 < m_edge.size() - 1)
        p1 = m_edge[i / 2 + 1]->m_s->getPoint(m_edge[i / 2 + 1]->m_w0);
      else
        p1 = m_edge[0]->m_s->getPoint(m_edge[0]->m_w0);

      if (std::min(p0.y, p1.y) > p.y || std::max(p0.y, p1.y) < p.y) continue;

      if (!areAlmostEqual(p0, p1, 1e-2))
        side = findSides(p, TQuadratic(p0, 0.5 * (p0 + p1), p1), 0.0, 1.0,
                         leftAreOdd, side);

      continue;
    }

    TEdge *e = m_edge[i / 2];

    TStroke *s = e->m_s;
    if (s->getBBox().y0 > p.y || s->getBBox().y1 < p.y) continue;

    double t0, t1;
    int chunkIndex0, chunkIndex1;
    const TThickQuadratic *q0, *q1;

    s->getChunkAndT(e->m_w0, chunkIndex0, t0);
    s->getChunkAndT(e->m_w1, chunkIndex1, t1);
    q0 = s->getChunk(chunkIndex0);
    q1 = s->getChunk(chunkIndex1);

    if (i == 0 && areAlmostEqual(q0->getPoint(t0).y, p.y)) {
      double tEnd;
      int chunkIndexEnd;
      TEdge *edgeEnd = m_edge.back();
      edgeEnd->m_s->getChunkAndT(edgeEnd->m_w1, chunkIndexEnd, tEnd);
      assert(areAlmostEqual(
          edgeEnd->m_s->getChunk(chunkIndexEnd)->getPoint(tEnd).y, p.y));
      side =
          edgeEnd->m_s->getChunk(chunkIndexEnd)->getSpeed(tEnd).y > 0 ? 1 : -1;
    }

    if (chunkIndex0 != chunkIndex1) {
      /*if (chunkIndex0>chunkIndex1)
{
std::swap(chunkIndex0, chunkIndex1);
std::swap(t0, t1);
}*/
      if (chunkIndex0 > chunkIndex1) {
        side = findSides(p, *q0, t0, 0, leftAreOdd, side);
        for (int j = chunkIndex0 - 1; j > chunkIndex1; j--)
          side = findSides(p, *s->getChunk(j), 1, 0, leftAreOdd, side);
        side   = findSides(p, *q1, 1, t1, leftAreOdd, side);
      } else {
        side = findSides(p, *q0, t0, 1, leftAreOdd, side);
        for (int j = chunkIndex0 + 1; j < chunkIndex1; j++)
          side = findSides(p, *s->getChunk(j), 0, 1, leftAreOdd, side);
        side   = findSides(p, *q1, 0, t1, leftAreOdd, side);
      }

    } else
      side = findSides(p, *q0, t0, t1, leftAreOdd, side);

    if (i & 0x1) delete q0;
  }

  return leftAreOdd;
}

//-----------------------------------------------------------------------------

int TRegion::Imp::leftScanlineIntersections(const TPointD &p,
                                            double(TPointD::*h),
                                            double(TPointD::*v)) const {
  struct Locals {
    const Imp *m_this;
    double m_x, m_y, m_tol;
    double TPointD::*m_h, TPointD::*m_v;

    inline double x(const TPointD &p) const { return p.*m_h; }
    inline double y(const TPointD &p) const { return p.*m_v; }

    inline double get(const TQuadratic &q, double t,
                      double(TPointD::*val)) const {
      double one_t = 1.0 - t;
      return one_t * (one_t * q.getP0().*val + t * q.getP1().*val) +
             t * (one_t * q.getP1().*val + t * q.getP2().*val);
    }

    inline double getX(const TQuadratic &q, double t) const {
      return get(q, t, m_h);
    }
    inline double getY(const TQuadratic &q, double t) const {
      return get(q, t, m_v);
    }

    void getEdgeData(int e, TEdge *&ed, TStroke *&s, int &chunk0,
                     const TThickQuadratic *&q0, double &t0, int &chunk1,
                     const TThickQuadratic *&q1, double &t1) const {
      ed = m_this->m_edge[e];
      s  = ed->m_s;

      s->getChunkAndT(ed->m_w0, chunk0, t0);
      s->getChunkAndT(ed->m_w1, chunk1, t1);

      q0 = s->getChunk(chunk0);
      q1 = s->getChunk(chunk1);
    }

    bool isInYRange(double y0, double y1) const {
      return (y0 <= m_y &&
              m_y < y1)  // Assuming the first endpoint is vertical-including,
             || (y1 < m_y && m_y <= y0);  // while the latter is not. Vertical
                                          // conditions are EXACT.
    }

    bool areInYRange(const TQuadratic &q, double t0, double t1,
                     int (&solIdx)[2]) const {
      assert(0.0 <= t0 && t0 <= 1.0), assert(0.0 <= t1 && t1 <= 1.0);

      const TPointD &p0 = q.getP0(), &p1 = q.getP1(), &p2 = q.getP2();

      double der[2] = {y(p1) - y(p0), y(p0) - y(p1) + y(p2) - y(p1)}, s;

      double y0 = getY(q, t0), y1 = getY(q, t1);

      if (tcg::poly_ops::solve_1(der, &s, m_tol)) {
        if (t0 <= s && s < t1) {
          double ys = getY(q, s);

          solIdx[0] = (ys < m_y && m_y <= y0 || y0 <= m_y && m_y < ys) ? 0 : -1;
          solIdx[1] = (ys < m_y && m_y < y1 || y1 < m_y && m_y < ys) ? 1 : -1;
        } else if (t1 < s && s <= t0) {
          double ys = getY(q, s);

          solIdx[0] = (ys < m_y && m_y <= y0 || y0 <= m_y && m_y < ys) ? 1 : -1;
          solIdx[1] = (ys < m_y && m_y < y1 || y1 < m_y && m_y < ys) ? 0 : -1;
        } else {
          solIdx[0] = isInYRange(y0, y1) ? (t0 < s) ? 0 : 1 : -1;
          solIdx[1] = -1;
        }
      } else
        solIdx[1] = solIdx[0] = -1;

      return (solIdx[0] >= 0 || solIdx[1] >= 0);
    }

    int leftScanlineIntersections(const TSegment &seg, bool &ascending) {
      const TPointD &p0 = seg.getP0(), &p1 = seg.getP1();
      bool wasAscending = ascending;

      ascending = (y(p1) > y(p0))
                      ? true
                      : (y(p1) < y(p0))
                            ? false
                            : (wasAscending = !ascending,
                               ascending);  // Couples with the cusp check below

      if (!isInYRange(y(p0), y(p1))) return 0;

      if (m_y == y(p0))
        return int(x(p0) < m_x &&
                   ascending == wasAscending);  // Cusps treated here

      double y1_y0 = y(p1) - y(p0),  // (x, m_y) in (p0, p1) from here on
          poly[2]  = {(m_y - y(p0)) * (x(p1) - x(p0)), -y1_y0}, sx_x0;

      return tcg::poly_ops::solve_1(poly, &sx_x0, m_tol)
                 ? int(sx_x0 < m_x - x(p0))
                 : int(x(p0) < m_x &&
                       x(p1) < m_x);  // Almost horizontal segments are
    }                                 // flattened along the axes

    int isAscending(const TThickQuadratic &q, double t, bool forward) {
      double y0 = y(q.getP0()), y1 = y(q.getP1()), y2 = y(q.getP2()),
             y1_y0 = y1 - y0, y2_y1 = y2 - y1;

      double yspeed_2 = tcg::numeric_ops::lerp(y1_y0, y2_y1, t) *
                        (2 * int(forward) - 1),
             yaccel = y2_y1 - y1_y0;

      return (yspeed_2 > 0.0)
                 ? 1
                 : (yspeed_2 < 0.0) ? -1 : tcg::numeric_ops::sign(yaccel);
    }

    int leftScanlineIntersections(const TQuadratic &q, double t0, double t1,
                                  bool &ascending) {
      const TPointD &p0 = q.getP0(), &p1 = q.getP1(), &p2 = q.getP2();

      double y1_y0 = y(p1) - y(p0), accel = y(p2) - y(p1) - y1_y0;

      // Fallback to segment case whenever we have too flat quads
      if (std::fabs(accel) < m_tol)
        return leftScanlineIntersections(
            TSegment(q.getPoint(t0), q.getPoint(t1)), ascending);

      // Calculate new ascension
      int ascends       = isAscending(q, t1, t0 < t1);
      bool wasAscending = ascending;

      ascending =
          (ascends > 0)
              ? true
              : (ascends < 0)
                    ? false
                    : (wasAscending = !ascending,
                       ascending);  // Couples with the cusps check below

      // In case the y coords are not in range, quit
      int solIdx[2];
      if (!areInYRange(q, t0, t1, solIdx)) return 0;

      // Identify coordinates for which  q(t) == y
      double poly[3] = {y(p0) - m_y, 2.0 * y1_y0, accel}, s[2];

      int sCount = tcg::poly_ops::solve_2(
          poly, s);  // Tolerance dealt at the first bailout above
      if (sCount == 2) {
        // Calculate result
        int result = 0;

        if (solIdx[0] >= 0) {
          result += int(
              getX(q, s[solIdx[0]]) < m_x &&
              (getY(q, t0) != m_y || ascending == wasAscending));  // Cusp check
        }

        if (solIdx[1] >= 0) result += int(getX(q, s[solIdx[1]]) < m_x);

        return result;
      }

      return (assert(sCount == 0), 0);  // Should never happen, since m_y is in
                                        // range. If it ever happens,
      // it must be close to the extremal - so quit with no intersections.
    }

  } locals = {this, p.*h, p.*v, 1e-4, h, v};

  TEdge *ed;
  TStroke *s;
  int chunk0, chunk1;
  const TThickQuadratic *q0, *q1;
  double t0, t1;

  UINT e, eCount = m_edge.size();

  int leftInters = 0;
  bool ascending =
      (locals.getEdgeData(eCount - 1, ed, s, chunk0, q0, t0, chunk1, q1, t1),
       locals.isAscending(*q1, t1, (t0 < t1)));

  for (e = 0; e != eCount; ++e) {
    // Explore current edge
    {
      // Retrieve edge data
      locals.getEdgeData(e, ed, s, chunk0, q0, t0, chunk1, q1, t1);

      // Compare edge against scanline segment
      if (chunk0 != chunk1) {
        if (chunk0 < chunk1) {
          leftInters +=
              locals.leftScanlineIntersections(*q0, t0, 1.0, ascending);

          for (int c = chunk0 + 1; c != chunk1; ++c)
            leftInters += locals.leftScanlineIntersections(*s->getChunk(c), 0.0,
                                                           1.0, ascending);

          leftInters +=
              locals.leftScanlineIntersections(*q1, 0.0, t1, ascending);
        } else {
          leftInters +=
              locals.leftScanlineIntersections(*q0, t0, 0.0, ascending);

          for (int c = chunk0 - 1; c != chunk1; --c)
            leftInters += locals.leftScanlineIntersections(*s->getChunk(c), 1.0,
                                                           0.0, ascending);

          leftInters +=
              locals.leftScanlineIntersections(*q1, 1.0, t1, ascending);
        }
      } else
        leftInters += locals.leftScanlineIntersections(*q0, t0, t1, ascending);
    }

    // Explore autoclose segment at the end of current edge
    {
      int nextE = (e + 1) % int(m_edge.size());

      const TPointD &p0 = m_edge[e]->m_s->getPoint(m_edge[e]->m_w1),
                    &p1 = m_edge[nextE]->m_s->getPoint(m_edge[nextE]->m_w0);

      leftInters +=
          locals.leftScanlineIntersections(TSegment(p0, p1), ascending);
    }
  }

  return leftInters;
}

//-----------------------------------------------------------------------------

int TRegion::scanlineIntersectionsBefore(double x, double y,
                                         bool horizontal) const {
  static double TPointD::*const dir[2] = {&TPointD::x, &TPointD::y};
  return m_imp->leftScanlineIntersections(TPointD(x, y), dir[!horizontal],
                                          dir[horizontal]);
}

//-----------------------------------------------------------------------------

bool TRegion::contains(const TEdge &e) const {
  for (UINT i = 0; i < m_imp->m_edge.size(); i++)
    if (*m_imp->m_edge[i] == e) return true;

  return false;
}

// Una regione contiene un'altra se  non hanno strokes in comune e un qualsiasi
// punto
// della seconda e' contenuto nella prima.

bool TRegion::Imp::contains(const TRegion::Imp &r) const {
  if (!getBBox().contains(r.getBBox())) return false;

  for (UINT i = 0; i < r.m_edge.size(); i++)
    for (UINT j = 0; j < m_edge.size(); j++)
      if (*r.m_edge[i] == *m_edge[j]) return false;
  // nessuno stroke in comune!!

  /*
for ( i=0; i<r.m_edge.size(); i++)
{
  TEdge *e = r.m_edge[i];
  if (!contains(e->m_s->getThickPoint(e->m_w0)))
    return false;
  if (!contains(e->m_s->getThickPoint((e->m_w0+e->m_w1)/2.0)))
    return false;
  if (!contains(e->m_s->getThickPoint(e->m_w1)))
    return false;
  }
*/
  TEdge *e = r.m_edge[0];
  return contains(e->m_s->getThickPoint((e->m_w0 + e->m_w1) / 2.0));
}

//-----------------------------------------------------------------------------

bool TRegion::Imp::thereAreintersections(const TStroke &s) const {
  for (UINT i = 0; i < m_edge.size(); i++) {
    std::vector<DoublePair> dummy;
    if (intersect(m_edge[i]->m_s, &s, dummy, true)) return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

bool TRegion::Imp::contains(const TStroke &s, bool mayIntersect) const {
  if (!getBBox().contains(s.getBBox())) return false;
  if (mayIntersect && thereAreintersections(s)) return false;

  return contains(s.getThickPoint(0.5));
}
//-----------------------------------------------------------------------------

bool TRegion::isSubRegionOf(const TRegion &r) const {
  return m_imp->isSubRegionOf(*r.m_imp);
}

//-----------------------------------------------------------------------------
/*
bool TRegion::Imp::isSubRegionOf(const TRegion::Imp &r) const
{
UINT i, j;
bool found, areTouching=false;

if (!r.getBBox().contains(getBBox()))
  return false;

for (i=0; i<m_edge.size(); i++)
  {
  for (j=0, found=false; !found && j<r.m_edge.size(); j++)
    if (m_edge[i]->m_s==r.m_edge[j]->m_s)
      {
      double w0 = std::min(m_edge[i]->m_w0, m_edge[i]->m_w1) ;
      double w1 = std::max(m_edge[i]->m_w0, m_edge[i]->m_w1) ;
      double r_w0 = std::min(r.m_edge[j]->m_w0, r.m_edge[j]->m_w1);
      double r_w1 = std::max(r.m_edge[j]->m_w0, r.m_edge[j]->m_w1);

      if ((w0>=r_w0 || areAlmostEqual(w0, r_w0, 0.1)) &&
          (w1<=r_w1 || areAlmostEqual(w1, r_w1, 0.1)))
        {
        found=true;
        areTouching = true;
        }
      else
        found=false;
      //found=true;
      }
  if ((!found) &&
!r.contains(m_edge[i]->m_s->getPoint(0.5*(m_edge[i]->m_w0+m_edge[i]->m_w1))))
    return false;
  }
return areTouching;
}
*/
//-----------------------------------------------------------------------------

bool TRegion::Imp::isSubRegionOf(const TRegion::Imp &r) const {
  if (!r.getBBox().contains(getBBox())) return false;

  for (UINT i = 0; i < m_edge.size(); i++) {
    for (UINT j = 0; j < r.m_edge.size(); j++) {
      TEdge *e    = r.m_edge[j];
      TEdge *subE = m_edge[i];
      if (subE->m_index == e->m_index &&
          (subE->m_w0 < m_edge[i]->m_w1) == (e->m_w0 < e->m_w1)) {
        bool forward = (e->m_w0 < e->m_w1);

        if (forward && (subE->m_w0 >= e->m_w0 ||
                        areAlmostEqual(subE->m_w0, e->m_w0, 1e-3)) &&
            (subE->m_w1 <= e->m_w1 ||
             areAlmostEqual(subE->m_w1, e->m_w1, 1e-3)))
          return true;

        if (!forward && (subE->m_w0 <= e->m_w0 ||
                         areAlmostEqual(subE->m_w0, e->m_w0, 1e-3)) &&
            (subE->m_w1 >= e->m_w1 ||
             areAlmostEqual(subE->m_w1, e->m_w1, 1e-3)))
          return true;
      }
    }
  }
  return false;
}

//------------------------------------------------------------------------------
TRegion *TRegion::getRegion(const TPointD &p) {
  for (UINT i = 0; i < m_imp->m_includedRegionArray.size(); i++)
    if (m_imp->m_includedRegionArray[i]->contains(p))
      return m_imp->m_includedRegionArray[i]->getRegion(p);

  return this;
}

//-----------------------------------------------------------------------------

bool TRegion::getInternalPoint(TPointD &p) {
  return m_imp->getInternalPoint(p, getBBox().x0, getBBox().x1,
                                 0.5 * (getBBox().y0 + getBBox().y1));
}

//-----------------------------------------------------------------------------

int TRegion::fill(const TPointD &p, int styleId) {
  UINT i;

  for (i = 0; i < m_imp->m_includedRegionArray.size(); i++)
    if (m_imp->m_includedRegionArray[i]->contains(p))
      return m_imp->m_includedRegionArray[i]->fill(p, styleId);

  int ret = getStyle();
  setStyle(styleId);

  return ret;
}

//-----------------------------------------------------------------------------

bool TRegion::selectFill(const TRectD &selArea, int styleId) {
  bool hitSomeRegions = false;

  if (selArea.contains(getBBox())) {
    hitSomeRegions = true;
    setStyle(styleId);
  }

  int regNum = m_imp->m_includedRegionArray.size();

  for (int i = 0; i < regNum; i++)
    hitSomeRegions |=
        m_imp->m_includedRegionArray[i]->selectFill(selArea, styleId);

  return hitSomeRegions;
}

//-----------------------------------------------------------------------------

void TRegion::invalidateBBox() {
  m_imp->invalidateBBox();
  for (UINT i = 0; i < m_imp->m_includedRegionArray.size(); i++)
    m_imp->m_includedRegionArray[i]->invalidateBBox();
}

//-----------------------------------------------------------------------------
/*
TRectD TRegion::getBBox(vector<TRectD>& bboxReg,vector<TPointD>& intersPoint)
const
{
  return m_imp->getBBox(bboxReg,intersPoint);
}
*/
//-----------------------------------------------------------------------------

TRectD TRegion::getBBox() const { return m_imp->getBBox(); }

//-----------------------------------------------------------------------------

void TRegion::addEdge(TEdge *e, bool minimizeEdges) {
  if (minimizeEdges &&
      e->m_s->getMaxThickness() > 0.0 &&  // outline strokes ignore this
      !m_imp->m_edge.empty() && m_imp->m_edge.back()->m_index == e->m_index &&
      areAlmostEqual(m_imp->m_edge.back()->m_w1, e->m_w0, 1e-5))
    m_imp->m_edge.back()->m_w1 = e->m_w1;
  else
    m_imp->m_edge.push_back(e);
  m_imp->m_isValidBBox = false;
  // if (e->m_s->isSelfLoop())
  //  assert(m_imp->m_edge.size()==1);
}

//-----------------------------------------------------------------------------

TEdge *TRegion::getLastEdge() const {
  if (m_imp->m_edge.empty()) return 0;

  return m_imp->m_edge.back();
}

//-----------------------------------------------------------------------------

TEdge *TRegion::popBackEdge() {
  TEdge *ret;
  if (m_imp->m_edge.empty()) return 0;

  ret = m_imp->m_edge.back();
  m_imp->m_edge.pop_back();
  return ret;
}

//-----------------------------------------------------------------------------

TEdge *TRegion::popFrontEdge() {
  TEdge *ret;
  if (m_imp->m_edge.empty()) return 0;

  ret = m_imp->m_edge.front();
  m_imp->m_edge.erase(m_imp->m_edge.begin());
  return ret;
}

//-----------------------------------------------------------------------------

TEdge *TRegion::getEdge(UINT index) const { return m_imp->m_edge[index]; }

//-----------------------------------------------------------------------------

UINT TRegion::getEdgeCount() const { return m_imp->m_edge.size(); }

//-----------------------------------------------------------------------------

TRegion *TRegion::getSubregion(UINT index) const {
  return m_imp->m_includedRegionArray[index];
}

//-----------------------------------------------------------------------------

UINT TRegion::getSubregionCount() const {
  return m_imp->m_includedRegionArray.size();
}

//-----------------------------------------------------------------------------

void TRegion::deleteSubregion(UINT index) {
  assert(m_imp->m_includedRegionArray[index]->getSubregionCount() == 0);

  // delete m_imp->m_includedRegionArray[index];
  m_imp->m_includedRegionArray.erase(m_imp->m_includedRegionArray.begin() +
                                     index);
}

//-----------------------------------------------------------------------------

void TRegion::moveSubregionsTo(TRegion *r) {
  while (m_imp->m_includedRegionArray.size()) {
    r->m_imp->m_includedRegionArray.push_back(
        m_imp->m_includedRegionArray.back());
    m_imp->m_includedRegionArray.pop_back();
  }
}

//-----------------------------------------------------------------------------

void TRegion::Imp::printContains(const TPointD &p) const {
  std::ofstream of("C:\\temp\\region.txt");

  of << "point: " << p.x << " " << p.y << std::endl;

  for (UINT i = 0; i < (UINT)m_edge.size(); i++) {
    for (UINT j = 0; j < (UINT)m_edge[i]->m_s->getChunkCount(); j++) {
      const TThickQuadratic *q = m_edge[i]->m_s->getChunk(j);

      of << "******quad # " << j << std::endl;
      of << "   p0 " << q->getP0() << std::endl;
      of << "   p1 " << q->getP1() << std::endl;
      of << "   p2 " << q->getP2() << std::endl;
      of << "****** " << std::endl;
    }
  }

  of << std::endl;
}

//-----------------------------------------------------------------------------

void TRegion::print() {
  std::cout << "\nNum edges: " << getEdgeCount() << std::endl;
  for (UINT i = 0; i < getEdgeCount(); i++) {
    std::cout << "\nEdge #" << i;
    std::cout << ":P0(" << getEdge(i)->m_s->getChunk(0)->getP0().x << ","
              << getEdge(i)->m_s->getChunk(0)->getP0().y << ")";
    std::cout << ":P2("
              << getEdge(i)
                     ->m_s->getChunk(getEdge(i)->m_s->getChunkCount() - 1)
                     ->getP2()
                     .x
              << ","
              << getEdge(i)
                     ->m_s->getChunk(getEdge(i)->m_s->getChunkCount() - 1)
                     ->getP2()
                     .y
              << ")";
    std::cout << std::endl;
  }
  if (m_imp->m_includedRegionArray.size()) {
    std::cout << "***** questa regione contiene:" << std::endl;
    for (UINT i = 0; i < m_imp->m_includedRegionArray.size(); i++)
      m_imp->m_includedRegionArray[i]->print();
    std::cout << "***** fine" << std::endl;
  }
}

//-----------------------------------------------------------------------------

void TRegion::setStyle(int colorStyle) {
  for (UINT i = 0; i < getEdgeCount(); i++) getEdge(i)->setStyle(colorStyle);

  /*
if (!colorStyle || (colorStyle && colorStyle->isFillStyle())  )
{
for (UINT i=0; i<getEdgeCount(); i++)
getEdge(i)->setColorStyle(colorStyle);

delete m_imp->m_prop;
m_imp->m_prop = 0;
}
*/
}

//-----------------------------------------------------------------------------

TRegionId TRegion::getId() {
  assert(getEdgeCount() > 0);
  TEdge *edge;

  for (UINT i = 0; i < m_imp->m_edge.size(); i++)
    if (m_imp->m_edge[i]->m_index >= 0) {
      edge = m_imp->m_edge[i];
      return TRegionId(edge->m_s->getId(),
                       (float)((edge->m_w0 + edge->m_w1) * 0.5),
                       edge->m_w0 < edge->m_w1);
    }

  edge = m_imp->m_edge[0];
  return TRegionId(edge->m_s->getId(), (float)((edge->m_w0 + edge->m_w1) * 0.5),
                   edge->m_w0 < edge->m_w1);
}

//-----------------------------------------------------------------------------

void TRegion::invalidateProp() {
  if (m_imp->m_prop) m_imp->m_prop->notifyRegionChange();
}

//-----------------------------------------------------------------------------

int TRegion::getStyle() const {
  int ret = 0;
  UINT i = 0, j, n = getEdgeCount();
  for (; i < n; i++) {
    int styleId = getEdge(i)->getStyle();
    if (styleId != 0 && ret == 0) {
      // assert(styleId<100);
      ret = styleId;
      if (i > 0)
        for (j = 0; j < i; j++) getEdge(i)->setStyle(ret);
    } else if (styleId != ret)
      getEdge(i)->setStyle(ret);
  }
  return ret;
}

//-----------------------------------------------------------------------------
void TRegion::addSubregion(TRegion *region) { m_imp->addSubregion(region); }

void TRegion::Imp::addSubregion(TRegion *region) {
  for (std::vector<TRegion *>::iterator it = m_includedRegionArray.begin();
       it != m_includedRegionArray.end(); ++it) {
    if (region->contains(**it)) {
      // region->addSubregion(*it);
      region->addSubregion(*it);
      it = m_includedRegionArray.erase(it);
      while (it != m_includedRegionArray.end()) {
        if (region->contains(**it)) {
          region->addSubregion(*it);
          // region->addSubregion(*it);
          it = m_includedRegionArray.erase(it);
        } else
          it++;
      }

      m_includedRegionArray.push_back(region);
      return;
    } else if ((*it)->contains(*region)) {
      (*it)->addSubregion(region);
      //(*it)->addSubregion(region);
      return;
    }
  }
  m_includedRegionArray.push_back(region);
}
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
