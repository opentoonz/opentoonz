

#include "tl2lautocloser.h"

#include "tgl.h"
#include "tenv.h"
#include "tvectorimage.h"
#include "tstroke.h"

#include <QDebug>

TEnv::DoubleVar VectorCloseValue("VectorCloseValue", 5);

//=============================================================================
#ifdef _WIN32

class MyTimer {
  bool m_enabled;
  LARGE_INTEGER m_freq;
  LARGE_INTEGER m_startTime, m_overhead;

public:
  MyTimer() : m_enabled(false) {
    if (QueryPerformanceFrequency(&m_freq)) {
      m_enabled = true;
      LARGE_INTEGER curTime;
      QueryPerformanceCounter(&m_startTime);
      QueryPerformanceCounter(&curTime);
      m_overhead.QuadPart = curTime.QuadPart - m_startTime.QuadPart;
    }
  }

  void start() {
    if (!m_enabled) return;
    QueryPerformanceCounter(&m_startTime);
  }

  double elapsedSeconds() {
    LARGE_INTEGER curTime;
    QueryPerformanceCounter(&curTime);
    LONGLONG microseconds = 1000000 * (curTime.QuadPart - m_startTime.QuadPart -
                                       m_overhead.QuadPart) /
                            m_freq.QuadPart;
    return 0.000001 * (double)microseconds;
  }
};
#endif

//=============================================================================

namespace {

// find the stroke curvature at w
// cfr. http://en.wikipedia.org/wiki/Curvature
TPointD getCurvature(TStroke *stroke, double w) {
  const double h = 0.0001;
  double w0      = std::max(0.0, w - h);
  double w1      = std::min(1.0, w + h);
  TPointD p0     = stroke->getPoint(w0);
  TPointD p1     = stroke->getPoint(w1);
  double ds      = norm(p0 - p1);
  double f       = (w1 - w0) / ds;

  TPointD d  = stroke->getSpeed(w) * f;
  TPointD d0 = stroke->getSpeed(w0) * f;
  TPointD d1 = stroke->getSpeed(w1) * f;
  TPointD dd = (d1 - d0) * (1.0 / ds);

  double c    = (d.x * dd.y - d.y * dd.x) / pow(d.x * d.x + d.y * d.y, 1.5);
  TPointD crv = normalize(rotate90(d)) * c;

  return crv;
}

//===========================================================================

//
// A point along a stroke: ths stroke itself, pos, w,s, crv
//
struct StrokePoint {
  double w, s;
  TPointD pos, crv, crvdir;  // crvdir is crv normalized (or 0)
  TPointD tgdir;             // tgdir is the normalized tangent
  TStroke *stroke;
  StrokePoint(TStroke *stroke_, double s_)
      : stroke(stroke_), w(stroke_->getParameterAtLength(s_)), s(s_) {
    pos      = stroke->getPoint(w);
    crv      = getCurvature(stroke, w);
    double c = norm(crv);
    if (c > 0)
      crvdir = crv * (1.0 / c);
    else
      crvdir         = TPointD();
    tgdir            = stroke->getSpeed(w);
    double tgdirNorm = norm(tgdir);
    if (tgdirNorm > 0.000001)
      tgdir = tgdir * (1.0 / tgdirNorm);
    else
      tgdir = TPointD(0, 0);
  }
  StrokePoint() : w(0), s(0), pos(), crv(), crvdir(), stroke(0) {}
};

//===========================================================================

// a set of StrokePoint evenly spaced along the whole stroke
// (curvilinear distance between two adjacent point is "inc")
struct StrokePointSet {
  TStroke *stroke;
  std::vector<StrokePoint> points;
  StrokePointSet(TStroke *stroke_ = 0) : stroke(stroke_) {
    const double inc = VectorCloseValue;
    if (stroke_) {
      double length = stroke->getLength();
      double s      = 0;
      if (stroke->isSelfLoop()) length -= inc;
      while (s < length) {
        points.push_back(StrokePoint(stroke, s));
        s += inc;
      }
    }
  }
};

//===========================================================================

class StrokesIntersection {
public:
  std::vector<double> m_ida, m_idb;  // distances to the closest intersection
                                     // (referred to StrokePointSet)

  StrokesIntersection() {}
  StrokesIntersection(const StrokePointSet &psa, const StrokePointSet &psb,
                      const std::vector<DoublePair> *intersection);

  ~StrokesIntersection() {}

  void update(const StrokePointSet &psa, const StrokePointSet &psb,
              const std::vector<DoublePair> &intersections);

  static void wrap(std::vector<double> &is, TStroke *stroke);

  static void computeIntersectionDistances(std::vector<double> &id,
                                           const StrokePointSet &ps,
                                           const std::vector<double> &is);

  StrokesIntersection *swapped() const {
    StrokesIntersection *s = new StrokesIntersection();
    s->m_ida               = m_idb;
    s->m_idb               = m_ida;
    return s;
  }
};

//---------------------------------------------------------------------------

StrokesIntersection::StrokesIntersection(
    const StrokePointSet &psa, const StrokePointSet &psb,
    const std::vector<DoublePair> *intersections) {
  if (!psa.stroke || !psb.stroke) return;  // it should never happen
  std::vector<DoublePair> myIntersections;
  if (!intersections) {
    intersections = &myIntersections;
    intersect(psa.stroke, psb.stroke, myIntersections);
  }
  update(psa, psb, *intersections);
}

//---------------------------------------------------------------------------

void StrokesIntersection::update(const StrokePointSet &psa,
                                 const StrokePointSet &psb,
                                 const std::vector<DoublePair> &intersections) {
  TStroke *strokea = psa.stroke;
  TStroke *strokeb = psb.stroke;
  m_ida.clear();
  m_idb.clear();

  if (!strokea || !strokeb) return;  // it should never happen

  m_ida.resize(psa.points.size(), -1);
  m_idb.resize(psb.points.size(), -1);

  int n = (int)intersections.size();
  if (n <= 0) return;

  // compute intersection arclengths
  std::vector<double> isa(n), isb(n);
  for (int i = 0; i < n; i++) {
    isa[i] = strokea->getLength(intersections[i].first);
    isb[i] = strokeb->getLength(intersections[i].second);
  }

  if (strokea == strokeb) {
    // una sola stroke. ogni intersezione deve valere per due
    isa.insert(isa.end(), isb.begin(), isb.end());
    std::sort(isa.begin(), isa.end());
    isb = isa;
  } else {
    // due stroke. ordino
    std::sort(isa.begin(), isa.end());
    std::sort(isb.begin(), isb.end());
  }
  if (strokea->isSelfLoop() && !isa.empty()) wrap(isa, strokea);
  if (strokeb->isSelfLoop() && !isb.empty()) wrap(isb, strokeb);
  computeIntersectionDistances(m_ida, psa, isa);
  computeIntersectionDistances(m_idb, psb, isb);
}

//---------------------------------------------------------------------------

// the stroke is a self loop. the last intersection is mirrored before s=0
// the first intersection is mirroed after s=length
void StrokesIntersection::wrap(std::vector<double> &is, TStroke *stroke) {
  assert(stroke->isSelfLoop());
  if (!is.empty()) {
    double length = stroke->getLength();
    is.insert(is.begin(), is.back() - length);
    is.push_back(is[1] + length);
  }
}

//---------------------------------------------------------------------------

// for each StrokePoint computes the related intersection distance (i.e. the
// distance to
// the closest intersection)
void StrokesIntersection::computeIntersectionDistances(
    std::vector<double> &id, const StrokePointSet &ps,
    const std::vector<double> &is) {
  id.clear();
  id.resize(ps.points.size(), -1);
  int isn = (int)is.size();
  int k   = 0;
  for (int i = 0; i < (int)ps.points.size(); i++) {
    double d = -1;
    if (k < isn) {
      double s = ps.points[i].s;
      while (k + 1 < isn && is[k + 1] < s) k++;
      if (is[k] > s)
        d = is[k] - s;
      else if (k + 1 < isn)
        d = std::min(is[k + 1] - s, s - is[k]);
      else
        d = s - is[k];
    }
    id[i] = d;
  }
}
//---------------------------------------------------------------------------

}  // namespace

//=============================================================================

class TL2LAutocloser::Imp {
public:
  double m_maxDist2;
  std::map<TStroke *, StrokePointSet *> m_strokes;
  std::map<std::pair<TStroke *, TStroke *>, StrokesIntersection *>
      m_intersections;

  // debug
  std::pair<StrokePointSet *, StrokePointSet *> m_lastStrokePair;
  std::vector<TL2LAutocloser::Segment> m_segments;

  Imp()
      : m_maxDist2(50 * 50)
      , m_lastStrokePair((StrokePointSet *)0, (StrokePointSet *)0) {}
  ~Imp();

  StrokePointSet *getPointSet(TStroke *stroke) {
    std::map<TStroke *, StrokePointSet *>::iterator it = m_strokes.find(stroke);
    if (it != m_strokes.end()) return it->second;
    StrokePointSet *ps = new StrokePointSet(stroke);
    m_strokes[stroke]  = ps;
    return ps;
  }

  StrokesIntersection *getIntersection(
      TStroke *strokea, TStroke *strokeb,
      const std::vector<DoublePair> *intersection) {
    std::map<std::pair<TStroke *, TStroke *>, StrokesIntersection *>::iterator
        it = m_intersections.find(std::make_pair(strokea, strokeb));
    if (it != m_intersections.end()) return it->second;
    StrokesIntersection *si =
        new StrokesIntersection(strokea, strokeb, intersection);
    m_intersections[std::make_pair(strokea, strokeb)] = si;
    m_intersections[std::make_pair(strokeb, strokea)] = si->swapped();
    return si;
  }

  void search(std::vector<TL2LAutocloser::Segment> &segments, TStroke *strokea,
              TStroke *strokeb, const std::vector<DoublePair> *intersection);

  void drawLinks();
  void drawStroke(StrokePointSet *);
  void drawStrokes();
};

//-----------------------------------------------------------------------------

TL2LAutocloser::Imp::~Imp() {
  std::map<TStroke *, StrokePointSet *>::iterator i;
  for (i = m_strokes.begin(); i != m_strokes.end(); ++i) delete i->second;
  std::map<std::pair<TStroke *, TStroke *>, StrokesIntersection *>::iterator j;
  for (j = m_intersections.begin(); j != m_intersections.end(); ++j)
    delete j->second;
  m_strokes.clear();
  m_intersections.clear();
}

//-----------------------------------------------------------------------------

void TL2LAutocloser::Imp::drawLinks() {
  glColor3d(0, 0, 1);
  glBegin(GL_LINES);
  for (int i = 0; i < (int)m_segments.size(); i++) {
    tglVertex(m_segments[i].p0);
    tglVertex(m_segments[i].p1);
  }
  glEnd();
}

void TL2LAutocloser::Imp::drawStroke(StrokePointSet *ps) {
  if (ps && ps->points.size() >= 2) {
    glBegin(GL_LINES);
    for (int i = 0; i < (int)ps->points.size(); i++)
      tglVertex(ps->points[i].pos);
    glEnd();
  }
}

void TL2LAutocloser::Imp::drawStrokes() {
  if (m_lastStrokePair.first) {
    if (m_lastStrokePair.first == m_lastStrokePair.second) {
      glColor3d(1, 0, 1);
      drawStroke(m_lastStrokePair.first);
    } else {
      glColor3d(1, 0, 0);
      drawStroke(m_lastStrokePair.first);
      glColor3d(0, 1, 0);
      drawStroke(m_lastStrokePair.second);
    }
  }
}

//-----------------------------------------------------------------------------

// search autoclose segments
void TL2LAutocloser::Imp::search(std::vector<TL2LAutocloser::Segment> &segments,
                                 TStroke *strokea, TStroke *strokeb,
                                 const std::vector<DoublePair> *intersections) {
  m_lastStrokePair.first = m_lastStrokePair.second = 0;
  if (strokea == 0 || strokeb == 0) return;
  /*
#ifdef _WIN32
MyTimer timer;
qDebug() << "search started";
timer.start();
#endif
*/
  bool sameStroke = strokea == strokeb;

  StrokePointSet *psa     = getPointSet(strokea);
  StrokePointSet *psb     = sameStroke ? psa : getPointSet(strokeb);
  m_lastStrokePair.first  = psa;
  m_lastStrokePair.second = psb;
  StrokesIntersection *si = getIntersection(strokea, strokeb, intersections);

  // find links (i.e. best matching point in psb for each point in psa)

  std::vector<std::pair<int, int>> links;
  int na = (int)psa->points.size();
  int nb = (int)psb->points.size();
  int i;
  for (i = 0; i < na; i++) {
    double minDist2 = 0;
    int k           = -1;

    int j0             = 0;
    if (sameStroke) j0 = i + 1;
    for (int j = 0; j < nb; j++) {
      TPointD delta = psb->points[j].pos - psa->points[i].pos;
      double dist2  = norm2(delta);

      if (dist2 > m_maxDist2) continue;
      if (dist2 < 0.000001) continue;

      double dist = sqrt(dist2);
      delta       = delta * (1.0 / dist);
      double cs   = 0.005;

      if ((psa->points[i].crvdir * delta) > -cs) continue;
      if ((psb->points[j].crvdir * delta) < cs) continue;

      if (sameStroke) {
        double ds = fabs(psa->points[i].s - psb->points[j].s);
        if (ds < dist * 1.5) continue;
      }
      if ((si->m_ida[i] > 0 && si->m_ida[i] < dist) ||
          (si->m_idb[j] > 0 && si->m_idb[j] < dist))
        continue;
      if (k < 0 || dist2 < minDist2) {
        k        = j;
        minDist2 = dist2;
      }
    }
    if (k >= 0 && (!sameStroke || k > i) && (0 < i && i < na - 1) &&
        (0 < k && k < nb - 1)) {
      links.push_back(std::make_pair(i, k));
    }
  }

  /*
for(i=0;i<(int)links.size();i++)
{
int ia = links[i].first;
int ib = links[i].second;
double mind2 = norm2(psa->points[ia].pos - psb->points[ib].pos);
TL2LAutocloser::Segment segment;
segment.stroke0 = strokea;
segment.stroke1 = strokeb;
segment.w0 = psa->points[ia].w;
segment.w1 = psb->points[ib].w;
segment.p0 = strokea->getThickPoint(segment.w0);
segment.p1 = strokeb->getThickPoint(segment.w1);
segment.dist2 = mind2;
segments.push_back(segment);
}
*/

  // select best links
  for (i = 0; i < (int)links.size(); i++) {
    int ia       = links[i].first;
    int ib       = links[i].second;
    double d2    = norm2(psa->points[ia].pos - psb->points[ib].pos);
    double mind2 = d2;
    int k        = i;
    while (i + 1 < (int)links.size()) {
      int ia2    = links[i + 1].first;
      int ib2    = links[i + 1].second;
      double d22 = norm2(psa->points[ia2].pos - psb->points[ib2].pos);
      double d2m = (d2 + d22) * 0.5;
      double dsa = psa->points[ia2].s - psa->points[ia].s;
      double dsb = psb->points[ib2].s - psb->points[ib].s;
      if (dsa * dsa > d2m || dsb * dsb > d2m) break;
      if (d22 < mind2) {
        mind2 = d22;
        k     = i + 1;
      }
      ia = ia2;
      ib = ib2;
      d2 = d22;
      i++;
    }
    ia = links[k].first;
    ib = links[k].second;
    TL2LAutocloser::Segment segment;
    segment.stroke0 = strokea;
    segment.stroke1 = strokeb;
    segment.w0      = psa->points[ia].w;
    segment.w1      = psb->points[ib].w;
    segment.p0      = strokea->getThickPoint(segment.w0);
    segment.p1      = strokeb->getThickPoint(segment.w1);
    segment.dist2   = mind2;
    segments.push_back(segment);
  }
  /*
#ifdef _WIN32
double elapsed = timer.elapsedSeconds();
qDebug() << "search completed. time=" << elapsed << "s";
#endif
*/
}

//=============================================================================

TL2LAutocloser::TL2LAutocloser() : m_imp(new Imp()) {}

//-----------------------------------------------------------------------------

TL2LAutocloser::~TL2LAutocloser() {}

//-----------------------------------------------------------------------------

void TL2LAutocloser::setMaxDistance2(double dist2) {
  m_imp->m_maxDist2 = dist2;
}

//-----------------------------------------------------------------------------

double TL2LAutocloser::getMaxDistance2() const { return m_imp->m_maxDist2; }

//-----------------------------------------------------------------------------

void TL2LAutocloser::search(std::vector<Segment> &segments, TStroke *stroke0,
                            TStroke *stroke1) {
  if (stroke0 && stroke1) m_imp->search(segments, stroke0, stroke1, 0);
}

//-----------------------------------------------------------------------------

void TL2LAutocloser::search(std::vector<Segment> &segments, TStroke *stroke0,
                            TStroke *stroke1,
                            const std::vector<DoublePair> &intersection) {
  if (stroke0 && stroke1)
    m_imp->search(segments, stroke0, stroke1, &intersection);
}

//-----------------------------------------------------------------------------

void TL2LAutocloser::draw() {
  m_imp->drawStrokes();
  m_imp->drawLinks();
}
