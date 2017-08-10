

#include "tgl.h"
#include "toonz/strokegenerator.h"
//#include "tofflinegl.h"

#include "tstroke.h"
#include "toonz/preferences.h"

using namespace std;

//-------------------------------------------------------------------

void StrokeGenerator::clear() {
  m_points.clear();
  m_modifiedRegion    = TRectD();
  m_paintedPointCount = 0;
  m_p0 = m_p1 = TPointD();
}

//-------------------------------------------------------------------

bool StrokeGenerator::isEmpty() const { return m_points.empty(); }

//-------------------------------------------------------------------

void StrokeGenerator::add(const TThickPoint &point, double pixelSize2) {
  if (m_points.empty()) {
    double x = point.x, y = point.y, d = point.thick + 3;
    m_points.push_back(point);
    TRectD rect(x - d, y - d, x + d, y + d);
    m_modifiedRegion     = rect;
    m_lastModifiedRegion = rect;
    m_p0 = m_p1 = point;
  } else {
    TThickPoint lastPoint = m_points.back();
    if (tdistance2(lastPoint, point) >= 4 * pixelSize2) {
      m_points.push_back(point);
      double d = std::max(point.thick, lastPoint.thick) + 3;
      TRectD rect(TRectD(lastPoint, point).enlarge(d));
      m_modifiedRegion += rect;
      m_lastModifiedRegion += rect;
    } else {
      m_points.back().thick = std::max(m_points.back().thick, point.thick);
    }
  }
}

//-------------------------------------------------------------------

void StrokeGenerator::filterPoints() {
  if (m_points.size() < 10) return;

  //  filtra m_points iniziali: generalmente elevate variazioni di thickness
  //  si hanno tra m_points[0] (al massimo m_points[1]) e i successivi)
  int size1 = m_points.size();
  int kMin  = 0;
  int kMax  = std::min(
      4,
      size1 -
          2);  //  confronta 5 m_points iniziali con i successivi corrispondenti
  int k = kMax;
  for (k = kMax; k >= kMin; --k) {
    TThickPoint currPoint = m_points[k];
    TThickPoint nextPoint = m_points[k + 1];
    double dist           = tdistance(currPoint, nextPoint);
    double deltaThick     = fabs(currPoint.thick - nextPoint.thick);
    if (deltaThick > 0.6 * dist)  //  deltaThick <= dist (condizione
                                  //  approssimata di non-autocontenimento per
                                  //  TTQ)
    {
      vector<TThickPoint>::iterator it1 = m_points.begin();
      vector<TThickPoint>::iterator it2 = it1 + k + 1;
      m_points.erase(it1, it2);  //  cancella da m_points[0] a m_points[k]
      assert((int)m_points.size() == size1 - k - 1);
      break;
    }
  }
  //  filtra m_points finali: generalmente elevate variazioni di thickness
  //  si hanno tra m_points[size - 1] (al massimo m_points[size - 2]) e i
  //  predecessori)
  int size2 = m_points.size();
  kMax      = size2 - 1;
  kMin      = std::max(
      kMax - 4,
      1);  //  confronta 5 m_points finali con i predecessori corrispondenti
  k = kMin;
  for (k = kMin; k <= kMax; ++k) {
    TThickPoint currPoint = m_points[k];
    TThickPoint prevPoint = m_points[k - 1];
    double dist           = tdistance(currPoint, prevPoint);
    double deltaThick     = fabs(currPoint.thick - prevPoint.thick);
    if (deltaThick > 0.6 * dist)  //  deltaThick <= dist (condizione
                                  //  approssimata di non-autocontenimento per
                                  //  TTQ)
    {
      int kTmp = k;
      while (k <= kMax)  //  cancella da m_points[k] a m_points[size2 - 1]
      {
        m_points.pop_back();
        ++k;
      }
      assert((int)m_points.size() == size2 - (kMax - kTmp + 1));
      break;
    }
  }
}

//-------------------------------------------------------------------

void StrokeGenerator::drawFragments(int first, int last) {
  if (m_points.empty()) return;
  int i                                  = first;
  if (last >= (int)m_points.size()) last = m_points.size() - 1;
  const double h                         = 0.01;
  TThickPoint a;
  TThickPoint b;
  TThickPoint c;
  TPointD v;

  // If drawing a straight line, a stroke can have only two points
  if (m_points.size() == 2) {
    a = m_points[0];
    b = m_points[1];
    if (Preferences::instance()->getShow0ThickLines()) {
      if (a.thick == 0) a.thick = 0.1;
      if (b.thick == 0) b.thick = 0.1;
    }
    // m_p0 = m_p1 = b;
    assert(tdistance(b, a) > h);
    v          = a.thick * normalize(rotate90(b - a));
    m_p0       = a + v;
    m_p1       = a - v;
    v          = b.thick * normalize(rotate90(b - a));
    TPointD p0 = b + v;
    TPointD p1 = b - v;
    glBegin(GL_POLYGON);
    tglVertex(m_p0);
    tglVertex(m_p1);
    tglVertex(p1);
    tglVertex(p0);
    glEnd();
    m_p0 = p0;
    m_p1 = p1;
    glBegin(GL_LINE_STRIP);
    tglVertex(a);
    tglVertex(b);
    glEnd();
    return;
  }

  while (i < last) {
    a = m_points[i - 1];
    b = m_points[i];
    c = m_points[i + 1];
    if (Preferences::instance()->getShow0ThickLines()) {
      if (a.thick == 0) a.thick = 0.1;
      if (b.thick == 0) b.thick = 0.1;
      if (c.thick == 0) c.thick = 0.1;
    }
    if (a.thick >= h && b.thick >= h && tdistance2(b, a) >= h &&
        tdistance2(a, c) >= h) {
      if (i - 1 == 0) {
        assert(tdistance(b, a) > h);
        v    = a.thick * normalize(rotate90(b - a));
        m_p0 = a + v;
        m_p1 = a - v;
      }
      assert(tdistance(c, a) > h);
      v          = b.thick * normalize(rotate90(c - a));
      TPointD p0 = b + v;
      TPointD p1 = b - v;
      glBegin(GL_POLYGON);
      tglVertex(m_p0);
      tglVertex(m_p1);
      tglVertex(p1);
      tglVertex(p0);
      glEnd();
      m_p0 = p0;
      m_p1 = p1;
    } else {
      m_p0 = m_p1 = b;
    }
    glBegin(GL_LINE_STRIP);
    tglVertex(a);
    tglVertex(b);
    glEnd();
    i++;
  }
}

//-------------------------------------------------------------------

void StrokeGenerator::drawLastFragments() {
  if (m_points.empty()) return;
  int n          = m_points.size();
  int i          = m_paintedPointCount;
  const double h = 0.01;

  if (i == 0) {
    TThickPoint a = m_points[0];
    if (a.thick >= h) tglDrawDisk(a, a.thick);
    i++;
  }

  drawFragments(i, n - 1);

  m_paintedPointCount = std::max(0, n - 2);
}

//-------------------------------------------------------------------

void StrokeGenerator::drawAllFragments() {
  if (m_points.empty()) return;

  int n          = m_points.size();
  int i          = 0;
  const double h = 0.01;

  TThickPoint a = m_points[0];
  if (a.thick >= h) tglDrawDisk(a, a.thick);

  drawFragments(1, n - 1);
  /*
//last fragment
TPointD p0 = c+v;
TPointD p1 = c-v;
glBegin(GL_POLYGON);
tglVertex(m_p0);
tglVertex(m_p1);
tglVertex(p1);
tglVertex(p0);
glEnd();
*/
  a = m_points.back();
  if (a.thick >= h) tglDrawDisk(a, a.thick);
}

//-------------------------------------------------------------------

TRectD StrokeGenerator::getModifiedRegion() const { return m_modifiedRegion; }

//-------------------------------------------------------------------

void StrokeGenerator::removeMiddlePoints() {
  int size = m_points.size();
  if (size > 2) {
    m_points.erase(m_points.begin() + 1, m_points.begin() + (size - 1));
  }
}

//-------------------------------------------------------------------

TRectD StrokeGenerator::getLastModifiedRegion() {
  TRectD lastModifiedRegion = m_lastModifiedRegion;
  m_lastModifiedRegion.empty();
  return lastModifiedRegion;
}

//-------------------------------------------------------------------

TPointD StrokeGenerator::getFirstPoint() { return m_points[0]; }

//-------------------------------------------------------------------

TStroke *StrokeGenerator::makeStroke(double error, UINT onlyLastPoints) const {
  if (onlyLastPoints == 0 || onlyLastPoints > m_points.size())
    return TStroke::interpolate(m_points, error);

  vector<TThickPoint> lastPoints(onlyLastPoints);
  vector<TThickPoint>::const_iterator first =
      m_points.begin() + (m_points.size() - onlyLastPoints);
  copy(first, m_points.end(), lastPoints.begin());

  return TStroke::interpolate(lastPoints, error);
}

//-------------------------------------------------------------------
