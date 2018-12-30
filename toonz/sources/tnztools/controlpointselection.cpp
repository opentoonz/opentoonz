

#include "controlpointselection.h"
#include "tvectorimage.h"
#include "tmathutil.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

using namespace ToolUtils;

namespace {

//-----------------------------------------------------------------------------

inline bool isLinearPoint(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2) {
  return (tdistance(p0, p1) < 0.02) && (tdistance(p1, p2) < 0.02);
}

//-----------------------------------------------------------------------------

//! Ritorna \b true se il punto \b p1 e' una cuspide.
bool isCuspPoint(const TPointD &p0, const TPointD &p1, const TPointD &p2) {
  TPointD p0_p1(p0 - p1), p2_p1(p2 - p1);
  double n1 = norm(p0_p1), n2 = norm(p2_p1);

  // Partial linear points are ALWAYS cusps (since directions from them are
  // determined by neighbours, not by the points themselves)
  if ((n1 < 0.02) || (n2 < 0.02)) return true;

  p0_p1 = p0_p1 * (1.0 / n1);
  p2_p1 = p2_p1 * (1.0 / n2);

  return (p0_p1 * p2_p1 > 0) ||
         (fabs(cross(p0_p1, p2_p1)) > 0.09);  // more than 5° is yes

  // Distance-based check. Unscalable...
  // return
  // !areAlmostEqual(tdistance(p0,p2),tdistance(p0,p1)+tdistance(p1,p2),2);
}

//-----------------------------------------------------------------------------

TThickPoint computeLinearPoint(const TThickPoint &p1, const TThickPoint &p2,
                               double factor, bool isIn) {
  TThickPoint p = p2 - p1;
  TThickPoint v = p * (1 / norm(p));
  if (isIn) return p2 - factor * v;
  return p1 + factor * v;
}

//-----------------------------------------------------------------------------
/*! Insert a point in the most long chunk between chunk \b indexA and chunk \b
 * indexB. */
void insertPoint(TStroke *stroke, int indexA, int indexB) {
  assert(stroke);
  int j          = 0;
  int chunkCount = indexB - indexA;
  if (chunkCount % 2 == 0) return;
  double length = 0;
  double firstW, lastW;
  for (j = indexA; j < indexB; j++) {
    // cerco il chunk piu' lungo
    double w0 = stroke->getW(stroke->getChunk(j)->getP0());
    double w1;
    if (j == stroke->getChunkCount() - 1)
      w1 = 1;
    else
      w1           = stroke->getW(stroke->getChunk(j)->getP2());
    double length0 = stroke->getLength(w0);
    double length1 = stroke->getLength(w1);
    if (length < length1 - length0) {
      firstW = w0;
      lastW  = w1;
      length = length1 - length0;
    }
  }
  stroke->insertControlPoints((firstW + lastW) * 0.5);
}

}  // namespace

//=============================================================================
// ControlPointEditorStroke
//-----------------------------------------------------------------------------

ControlPointEditorStroke *ControlPointEditorStroke::clone() const {
  ControlPointEditorStroke *controlPointEditorStroke =
      new ControlPointEditorStroke();
  controlPointEditorStroke->setStroke(m_vi->clone(), m_strokeIndex);
  return controlPointEditorStroke;
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::nextIndex(int index) const {
  int cpCount = m_controlPoints.size();
  if (++index < cpCount) return index;

  if (isSelfLoop()) {
    index %= cpCount;
    return (index < 0) ? index + cpCount : index;
  }

  return -1;
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::prevIndex(int index) const {
  int cpCount = m_controlPoints.size();
  if (--index >= 0) return index;

  if (isSelfLoop()) {
    index %= cpCount;
    return (index < 0) ? index + cpCount : index;
  }

  return -1;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::adjustChunkParity() {
  TStroke *stroke = getStroke();
  if (!stroke) return;
  int firstChunk;
  int secondChunk = stroke->getChunkCount();
  int i;
  for (i = stroke->getChunkCount() - 1; i > 0; i--) {
    if (tdistance(stroke->getChunk(i - 1)->getP0(),
                  stroke->getChunk(i)->getP2()) < 0.5)
      continue;
    TPointD p0 = stroke->getChunk(i - 1)->getP1();
    TPointD p1 = stroke->getChunk(i - 1)->getP2();
    TPointD p2 = stroke->getChunk(i)->getP1();
    if (isCuspPoint(p0, p1, p2) || isLinearPoint(p0, p1, p2)) {
      firstChunk = i;
      insertPoint(stroke, firstChunk, secondChunk);
      secondChunk = firstChunk;
    }
  }
  insertPoint(stroke, 0, secondChunk);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::resetControlPoints() {
  TStroke *stroke = getStroke();
  if (!stroke) return;
  m_controlPoints.clear();
  int i;
  int cpCount = stroke->getControlPointCount();
  if (cpCount == 3) {
    const TThickQuadratic *chunk = stroke->getChunk(0);
    if (chunk->getP0() == chunk->getP1() &&
        chunk->getP0() == chunk->getP2())  // E' un punto
    {
      m_controlPoints.push_back(
          ControlPoint(0, TPointD(0.0, 0.0), TPointD(0.0, 0.0), true));
      return;
    }
  }
  for (i = 0; i < cpCount; i = i + 4) {
    TThickPoint speedIn, speedOut;
    bool isPickOut    = false;
    TThickPoint p     = stroke->getControlPoint(i);
    TThickPoint precP = stroke->getControlPoint(i - 1);
    TThickPoint nextP = stroke->getControlPoint(i + 1);
    if (0 < i && i < cpCount - 1)  // calcola speedIn e speedOut
    {
      speedIn  = p - precP;
      speedOut = nextP - p;
    }
    if (i == 0)  // calcola solo lo speedOut
    {
      speedOut                  = nextP - p;
      if (isSelfLoop()) speedIn = p - stroke->getControlPoint(cpCount - 2);
    }
    if (i == cpCount - 1)  // calcola solo lo speedIn
      speedIn = p - precP;
    if (i == cpCount - 1 && isSelfLoop())
      break;  // Se lo stroke e' selfLoop inserisco solo il primo dei due punti
              // coincidenti

    bool isCusp = ((i != 0 && i != cpCount - 1) || (isSelfLoop() && i == 0))
                      ? isCuspPoint(precP, p, nextP)
                      : true;
    m_controlPoints.push_back(ControlPoint(i, speedIn, speedOut, isCusp));
  }
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getPureDependentPoint(int index) const {
  TStroke *stroke = getStroke();
  if (!stroke) return TThickPoint();
  bool selfLoop = isSelfLoop();
  int cpCount = selfLoop ? m_controlPoints.size() + 1 : m_controlPoints.size();
  int nextIndex  = (selfLoop && index == cpCount - 2) ? 0 : index + 1;
  int pointIndex = m_controlPoints[index].m_pointIndex;

  TThickPoint oldP(stroke->getControlPoint(pointIndex + 2));
  TPointD oldSpeedOutP = stroke->getControlPoint(pointIndex + 1);
  TPointD oldSpeedInP  = stroke->getControlPoint(pointIndex + 3);

  double dist = tdistance(oldSpeedOutP, oldSpeedInP);
  double t = (dist > 1e-4) ? tdistance(oldSpeedInP, convert(oldP)) / dist : 0.5;

  TPointD speedOutPoint(getSpeedOutPoint(index));
  TPointD nextSpeedInPoint(getSpeedInPoint(nextIndex));

  return TThickPoint((1 - t) * nextSpeedInPoint + t * speedOutPoint,
                     oldP.thick);

  // return TThickPoint(0.5 * (speedOutPoint + nextSpeedInPoint), oldThick);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::getDependentPoints(
    int index, std::vector<std::pair<int, TThickPoint>> &points) const {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  int cpCount = m_controlPoints.size();
  if (index == cpCount && isSelfLoop())  // strange, but was treated...
    index = 0;

  if (index == 0 && cpCount == 1) {
    // Single point case
    TStroke *stroke = getStroke();
    TThickPoint pos(stroke->getControlPoint(m_controlPoints[0].m_pointIndex));

    points.push_back(std::make_pair(1, pos));
    points.push_back(std::make_pair(2, pos));
    return;
  }

  int prev = prevIndex(index);
  if (prev >= 0) {
    int prevPointIndex = m_controlPoints[prev].m_pointIndex;

    if (isSpeedOutLinear(prev))
      points.push_back(
          std::make_pair(prevPointIndex + 1, getSpeedOutPoint(prev)));
    points.push_back(
        std::make_pair(prevPointIndex + 2, getPureDependentPoint(prev)));
    points.push_back(
        std::make_pair(prevPointIndex + 3, getSpeedInPoint(index)));
  }

  int next = nextIndex(index);
  if (next >= 0) {
    int pointIndex = m_controlPoints[index].m_pointIndex;

    points.push_back(std::make_pair(pointIndex + 1, getSpeedOutPoint(index)));
    points.push_back(
        std::make_pair(pointIndex + 2, getPureDependentPoint(index)));
    if (isSpeedInLinear(next))
      points.push_back(std::make_pair(pointIndex + 3, getSpeedInPoint(next)));
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::updatePoints() {
  TStroke *stroke = getStroke();
  if (!stroke) return;
  bool selfLoop = isSelfLoop();
  // Se e' rimasto un unico punto non ha senso che la stroke sia selfloop
  if (selfLoop && m_controlPoints.size() == 1) {
    stroke->setSelfLoop(false);
    selfLoop = false;
  }

  // Se e' self loop  devo aggiungere un punto in piu' al cpCount
  std::vector<TThickPoint> points;

  int cpCount = selfLoop ? m_controlPoints.size() + 1 : m_controlPoints.size();
  if (cpCount == 1)
    // Single point case
    points.resize(3, getControlPoint(0));
  else {
    std::vector<std::pair<int, TThickPoint>> dependentPoints;

    points.push_back(getControlPoint(0));
    points.push_back(getSpeedOutPoint(0));

    int i, pointIndex, currPointIndex = m_controlPoints[0].m_pointIndex + 1;
    for (i = 1; i < cpCount; ++i) {
      bool isLastSelfLoopPoint = (selfLoop && i == cpCount - 1);
      int index                = isLastSelfLoopPoint ? 0 : i;

      TThickPoint p = getControlPoint(index);
      pointIndex    = isLastSelfLoopPoint ? getStroke()->getControlPointCount()
                                       : m_controlPoints[index].m_pointIndex;

      dependentPoints.clear();
      getDependentPoints(index, dependentPoints);

      int j;
      for (j = 0; j < (int)dependentPoints.size() &&
                  dependentPoints[j].first < pointIndex;
           j++) {
        if (currPointIndex < dependentPoints[j].first) {
          currPointIndex = dependentPoints[j].first;
          points.push_back(dependentPoints[j].second);
        }
      }

      points.push_back(p);

      for (; j < (int)dependentPoints.size(); j++) {
        if (currPointIndex < dependentPoints[j].first) {
          currPointIndex = dependentPoints[j].first;
          points.push_back(dependentPoints[j].second);
        }
      }
    }
  }

  stroke->reshape(&points[0], points.size());
  m_vi->notifyChangedStrokes(m_strokeIndex);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::updateDependentPoint(int index) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  std::vector<std::pair<int, TThickPoint>> points;
  getDependentPoints(index, points);

  int i;
  for (i = 0; i < (int)points.size(); i++)
    stroke->setControlPoint(points[i].first, points[i].second);

  m_vi->notifyChangedStrokes(m_strokeIndex);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSpeedOut(int index, const TPointD &delta,
                                            double minDistance) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  // If the next cp has linear speed in, it must be recomputed
  bool selfLoop = isSelfLoop();
  int cpCount = selfLoop ? m_controlPoints.size() + 1 : m_controlPoints.size();
  int nextIndex = (selfLoop && index == cpCount - 2) ? 0 : index + 1;
  if (m_controlPoints[nextIndex].m_isCusp && isSpeedInLinear(nextIndex))
    setLinearSpeedIn(nextIndex, true, false);

  // Update the speedOut
  m_controlPoints[index].m_speedOut += delta;
  TPointD newP = m_controlPoints[index].m_speedOut;
  if (areAlmostEqual(newP.x, 0, minDistance) &&
      areAlmostEqual(newP.y, 0, minDistance))  // Setto a linear
  {
    setLinearSpeedOut(index);
    return;
  }
  if (!m_controlPoints[index].m_isCusp && !isSpeedInLinear(index)) {
    // Devo ricalcolare lo SpeedIn
    TPointD v(m_controlPoints[index].m_speedOut *
              (1.0 / norm(m_controlPoints[index].m_speedOut)));
    m_controlPoints[index].m_speedIn =
        TThickPoint(v * norm(m_controlPoints[index].m_speedIn),
                    m_controlPoints[index].m_speedIn.thick);
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSpeedIn(int index, const TPointD &delta,
                                           double minDistance) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  // If the prev cp has linear speed out, it must be recomputed
  bool selfLoop = isSelfLoop();
  int cpCount = selfLoop ? m_controlPoints.size() + 1 : m_controlPoints.size();
  int prevIndex = (selfLoop && index == 0) ? cpCount - 2 : index - 1;
  if (m_controlPoints[prevIndex].m_isCusp && isSpeedOutLinear(prevIndex))
    setLinearSpeedOut(prevIndex, true, false);

  // Update the speedOut
  m_controlPoints[index].m_speedIn -= delta;
  TPointD newP = m_controlPoints[index].m_speedIn;
  if (areAlmostEqual(newP.x, 0, minDistance) &&
      areAlmostEqual(newP.y, 0, minDistance))  // Setto a linear
  {
    setLinearSpeedIn(index);
    return;
  }
  if (!m_controlPoints[index].m_isCusp && !isSpeedOutLinear(index)) {
    // Devo ricalcolare lo SpeedOut
    TPointD v(m_controlPoints[index].m_speedIn *
              (1.0 / norm(m_controlPoints[index].m_speedIn)));
    m_controlPoints[index].m_speedOut =
        TThickPoint(v * norm(m_controlPoints[index].m_speedOut),
                    m_controlPoints[index].m_speedOut.thick);
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSingleControlPoint(int index,
                                                      const TPointD &delta) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  int pointIndex = m_controlPoints[index].m_pointIndex;
  assert(stroke && 0 <= pointIndex &&
         pointIndex < stroke->getControlPointCount());

  bool selfLoop = isSelfLoop();
  int cpCount = selfLoop ? m_controlPoints.size() + 1 : m_controlPoints.size();

  TThickPoint p = stroke->getControlPoint(pointIndex);
  p             = TThickPoint(p + delta, p.thick);
  stroke->setControlPoint(pointIndex, p);
  if (pointIndex == 0 && selfLoop)
    stroke->setControlPoint(stroke->getControlPointCount() - 1, p);

  // Directions must be recalculated in the linear cases
  if ((selfLoop || index > 0) && isSpeedInLinear(index)) {
    setLinearSpeedIn(index, true, false);

    // Furthermore, if the NEIGHBOUR point is linear, it has to be
    // recalculated too
    int prevIndex = (selfLoop && index == 0) ? cpCount - 2 : index - 1;
    if (m_controlPoints[prevIndex].m_isCusp && isSpeedOutLinear(prevIndex))
      setLinearSpeedOut(prevIndex, true, false);
  }
  if ((selfLoop || index < cpCount - 1) && isSpeedOutLinear(index)) {
    setLinearSpeedOut(index, true, false);

    int nextIndex = (selfLoop && index == cpCount - 2) ? 0 : index + 1;
    if (m_controlPoints[nextIndex].m_isCusp && isSpeedInLinear(nextIndex))
      setLinearSpeedIn(nextIndex, true, false);
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::setStroke(const TVectorImageP &vi,
                                         int strokeIndex) {
  m_strokeIndex = strokeIndex;
  m_vi          = vi;
  if (!vi || strokeIndex == -1) {
    m_controlPoints.clear();
    return;
  }
  TStroke *stroke              = getStroke();
  const TThickQuadratic *chunk = stroke->getChunk(0);
  if (stroke->getControlPointCount() == 3 && chunk->getP0() == chunk->getP1() &&
      chunk->getP0() == chunk->getP2()) {
    resetControlPoints();
    return;
  }
  adjustChunkParity();
  resetControlPoints();
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getControlPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index && index < (int)m_controlPoints.size());
  return stroke->getControlPoint(m_controlPoints[index].m_pointIndex);
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::getIndexPointInStroke(int index) const {
  return m_controlPoints[index].m_pointIndex;
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getSpeedInPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index && index < (int)m_controlPoints.size());

  ControlPoint cp = m_controlPoints[index];
  return stroke->getControlPoint(cp.m_pointIndex) - cp.m_speedIn;
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getSpeedOutPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index && index < (int)m_controlPoints.size());

  ControlPoint cp = m_controlPoints[index];
  return stroke->getControlPoint(cp.m_pointIndex) + cp.m_speedOut;
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::isCusp(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index && index < (int)getControlPointCount());
  return m_controlPoints[index].m_isCusp;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::setCusp(int index, bool isCusp,
                                       bool setSpeedIn) {
  m_controlPoints[index].m_isCusp = isCusp;
  if (isCusp == true) return;
  moveSpeed(index, TPointD(0.0, 0.0), setSpeedIn, 0.0);
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::isSpeedInLinear(int index) const {
  assert(index < (int)m_controlPoints.size());
  return (fabs(m_controlPoints[index].m_speedIn.x) <= 0.02) &&
         (fabs(m_controlPoints[index].m_speedIn.y) <= 0.02);
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::isSpeedOutLinear(int index) const {
  assert(index < (int)m_controlPoints.size());
  return (fabs(m_controlPoints[index].m_speedOut.x) <= 0.02) &&
         (fabs(m_controlPoints[index].m_speedOut.y) <= 0.02);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::setLinearSpeedIn(int index, bool linear,
                                                bool updatePoints) {
  TStroke *stroke = getStroke();
  if (!stroke || m_controlPoints.size() == 1) return;
  int pointIndex = m_controlPoints[index].m_pointIndex;
  if (pointIndex == 0) {
    if (isSelfLoop())
      pointIndex = stroke->getControlPointCount() - 1;
    else
      return;
  }
  int precIndex =
      (index == 0 && isSelfLoop()) ? m_controlPoints.size() - 1 : index - 1;

  TThickPoint point     = stroke->getControlPoint(pointIndex);
  TThickPoint precPoint = (pointIndex > 2)
                              ? stroke->getControlPoint(pointIndex - 3)
                              : TThickPoint();

  if (linear) {
    TThickPoint p(point - precPoint);
    double n = norm(p);
    TThickPoint speedIn =
        (n != 0.0) ? (0.01 / n) * p : TThickPoint(0.001, 0.001, 0.0);
    m_controlPoints[index].m_speedIn = speedIn;
  } else {
    TThickPoint newPrec2             = (precPoint + point) * 0.5;
    TThickPoint speedIn              = (point - newPrec2) * 0.5;
    m_controlPoints[index].m_speedIn = speedIn;
  }
  if (updatePoints) updateDependentPoint(index);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::setLinearSpeedOut(int index, bool linear,
                                                 bool updatePoints) {
  TStroke *stroke = getStroke();
  if (!stroke || m_controlPoints.size() == 1) return;
  int cpCount    = stroke->getControlPointCount();
  int pointIndex = m_controlPoints[index].m_pointIndex;
  if (pointIndex == cpCount - 1) {
    if (isSelfLoop())
      pointIndex = 0;
    else
      return;
  }
  int nextIndex =
      (index == m_controlPoints.size() - 1 && isSelfLoop()) ? 0 : index + 1;

  TThickPoint point     = stroke->getControlPoint(pointIndex);
  TThickPoint nextPoint = (pointIndex < cpCount - 3)
                              ? stroke->getControlPoint(pointIndex + 3)
                              : TThickPoint();

  if (linear) {
    TThickPoint p(nextPoint - point);
    double n = norm(p);
    TThickPoint speedOut =
        (n != 0.0) ? (0.01 / n) * p : TThickPoint(0.001, 0.001, 0.0);
    m_controlPoints[index].m_speedOut = speedOut;
  } else {
    TThickPoint newNext2              = (nextPoint + point) * 0.5;
    TThickPoint speedOut              = (newNext2 - point) * 0.5;
    m_controlPoints[index].m_speedOut = speedOut;
  }
  if (updatePoints) updateDependentPoint(index);
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::setLinear(int index, bool isLinear,
                                         bool updatePoints) {
  bool movePrec = (!isSelfLoop()) ? index > 0 : true;
  bool moveNext = (!isSelfLoop()) ? (index < getControlPointCount() - 1) : true;
  if (isLinear != isSpeedInLinear(index))
    setLinearSpeedIn(index, isLinear, updatePoints);
  else
    movePrec = false;
  if (isLinear != isSpeedOutLinear(index))
    setLinearSpeedOut(index, isLinear, updatePoints);
  else
    moveNext                               = false;
  bool ret                                 = moveNext || movePrec;
  if (ret) m_controlPoints[index].m_isCusp = true;
  return ret;
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::setControlPointsLinear(std::set<int> points,
                                                      bool isLinear) {
  std::set<int>::iterator it;
  bool isChanged = false;
  for (it = points.begin(); it != points.end(); it++)
    isChanged = setLinear(*it, isLinear, false) || isChanged;
  for (it = points.begin(); it != points.end(); it++) updateDependentPoint(*it);
  return isChanged;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveControlPoint(int index,
                                                const TPointD &delta) {
  TStroke *stroke = getStroke();
  if (!stroke) return;
  assert(stroke && 0 <= index && index < (int)getControlPointCount());

  moveSingleControlPoint(index, delta);
  updateDependentPoint(index);
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::addControlPoint(const TPointD &pos) {
  TStroke *stroke = getStroke();
  if (!stroke) return -1;
  double d = 0.01;
  int indexAtPos;
  int cpCount = stroke->getControlPointCount();
  if (cpCount <= 3)  // e' un unico chunk e in questo caso rappresenta un punto
  {
    getPointTypeAt(pos, d, indexAtPos);
    return indexAtPos;
  }

  double w       = stroke->getW(pos);
  int pointIndex = stroke->getControlPointIndexAfterParameter(w);

  int i, index;
  for (i = 0; i < getControlPointCount(); i++) {
    // Cerco il ControlPoint corrispondente all'indice pointIndex. OSS.:
    // Effettuo il
    // controllo da zero a getControlPointCount()-1 per gestire il caso del
    // selfLoop
    if (pointIndex == m_controlPoints[i].m_pointIndex + 1 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 2 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 3 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 4)
      index = i;
  }

  ControlPoint precCp = m_controlPoints[index];
  assert(precCp.m_pointIndex >= 0);
  std::vector<TThickPoint> points;

  for (i = 0; i < cpCount; i++) {
    if (i != precCp.m_pointIndex + 1 && i != precCp.m_pointIndex + 2 &&
        i != precCp.m_pointIndex + 3)
      points.push_back(stroke->getControlPoint(i));
    if (i == precCp.m_pointIndex + 2) {
      bool isBeforePointLinear = isSpeedOutLinear(index);
      int nextIndex =
          (isSelfLoop() && index == m_controlPoints.size() - 1) ? 0 : index + 1;
      bool isNextPointLinear =
          nextIndex < (int)m_controlPoints.size() && isSpeedInLinear(nextIndex);

      TThickPoint a0 = stroke->getControlPoint(precCp.m_pointIndex);
      TThickPoint a1 = stroke->getControlPoint(precCp.m_pointIndex + 1);
      TThickPoint a2 = stroke->getControlPoint(precCp.m_pointIndex + 2);
      TThickPoint a3 = stroke->getControlPoint(precCp.m_pointIndex + 3);
      TThickPoint a4 = stroke->getControlPoint(precCp.m_pointIndex + 4);
      double dist2   = tdistance2(pos, TPointD(a2));
      TThickPoint d0, d1, d2, d3, d4, d5, d6;

      if (isBeforePointLinear && isNextPointLinear) {
        // Se sono entrambi i punti lineari  inserisco un punto lineare
        d0 = a1;
        d3 = stroke->getThickPoint(w);
        d6 = a3;
        d2 = computeLinearPoint(d0, d3, 0.01, true);   // SpeedIn
        d4 = computeLinearPoint(d3, d6, 0.01, false);  // SpeedOut
        d1 = 0.5 * (d0 + d2);
        d5 = 0.5 * (d4 + d6);
      } else if (dist2 < 32) {
        // Sono molto vicino al punto che non viene visualizzato
        TThickPoint b0 = 0.5 * (a0 + a1);
        TThickPoint b1 = 0.5 * (a2 + a1);
        TThickPoint c0 = 0.5 * (b0 + b1);

        TThickPoint b2 = 0.5 * (a2 + a3);
        TThickPoint b3 = 0.5 * (a3 + a4);

        TThickPoint c1 = 0.5 * (b2 + b3);
        d0             = b0;
        d1             = c0;
        d2             = b1;
        d3             = a2;
        d4             = b2;
        d5             = c1;
        d6             = b3;
      } else {
        bool isInFirstChunk = true;
        if (pointIndex > precCp.m_pointIndex + 2) {
          // nel caso in cui sono nel secondo chunk scambio i punti
          a0 = a4;
          std::swap(a1, a3);
          isInFirstChunk = false;
        }

        double w0 = (isSelfLoop() && precCp.m_pointIndex + 4 == cpCount - 1 &&
                     !isInFirstChunk)
                        ? 1
                        : stroke->getW(a0);
        double w1 = stroke->getW(a2);
        double t  = (w - w0) / (w1 - w0);

        TThickPoint p  = stroke->getThickPoint(w);
        TThickPoint b0 = TThickPoint((1 - t) * a0 + t * a1,
                                     (1 - t) * a0.thick + t * a1.thick);
        TThickPoint b1 = TThickPoint((1 - t) * a1 + t * a2,
                                     (1 - t) * a1.thick + t * a2.thick);
        TThickPoint c0 =
            TThickPoint(0.5 * a0 + 0.5 * b0, (1 - t) * a0.thick + t * b0.thick);
        TThickPoint c1 =
            TThickPoint(0.5 * b0 + 0.5 * p, (1 - t) * b0.thick + t * p.thick);
        TThickPoint c2 =
            TThickPoint(0.5 * c0 + 0.5 * c1, (1 - t) * c0.thick + t * c1.thick);

        d0 = (isInFirstChunk) ? c0 : a3;
        d1 = (isInFirstChunk) ? c2 : a2;
        d2 = (isInFirstChunk) ? c1 : b1;
        d3 = p;
        d4 = (isInFirstChunk) ? b1 : c1;
        d5 = (isInFirstChunk) ? a2 : c2;
        d6 = (isInFirstChunk) ? a3 : c0;
      }
      if (isBeforePointLinear && !isNextPointLinear)
        d1 = computeLinearPoint(d0, d2, 0.01, false);
      else if (isNextPointLinear && !isBeforePointLinear)
        d5 = computeLinearPoint(d4, d6, 0.01, true);
      points.push_back(d0);
      points.push_back(d1);
      points.push_back(d2);
      points.push_back(d3);
      points.push_back(d4);
      points.push_back(d5);
      points.push_back(d6);
    }
  }

  stroke->reshape(&points[0], points.size());
  resetControlPoints();

  getPointTypeAt(pos, d, indexAtPos);
  return indexAtPos;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::deleteControlPoint(int index) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  assert(stroke && 0 <= index && index < (int)getControlPointCount());
  // E' un unico chunk e in questo caso rappresenta un punto
  if (stroke->getControlPointCount() <= 3 ||
      (isSelfLoop() && stroke->getControlPointCount() <= 5)) {
    m_controlPoints.clear();
    m_vi->deleteStroke(m_strokeIndex);
    return;
  }

  QList<int> newPointsIndex;
  int i;
  for (i = 0; i < (int)getControlPointCount() - 1; i++)
    newPointsIndex.push_back(m_controlPoints[i].m_pointIndex);

  m_controlPoints.removeAt(index);
  updatePoints();

  // Aggiorno gli indici dei punti nella stroke
  assert((int)newPointsIndex.size() == (int)getControlPointCount());
  for (i                            = 0; i < (int)getControlPointCount(); i++)
    m_controlPoints[i].m_pointIndex = newPointsIndex.at(i);

  int prev = prevIndex(index);
  if (prev >= 0 && isSpeedOutLinear(prev)) {
    setLinearSpeedOut(prev);
    updateDependentPoint(prev);
  }
  if (index < (int)m_controlPoints.size() && isSpeedInLinear(index)) {
    setLinearSpeedIn(index);
    updateDependentPoint(index);
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSpeed(int index, const TPointD &delta,
                                         bool isIn, double minDistance) {
  if (!isIn)
    moveSpeedOut(index, delta, minDistance);
  else
    moveSpeedIn(index, delta, minDistance);

  updateDependentPoint(index);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSegment(int beforeIndex, int nextIndex,
                                           const TPointD &delta,
                                           const TPointD &pos) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  int cpCount = getControlPointCount();
  // Verifiche per il caso in cui lo stroke e' selfLoop
  if (isSelfLoop() && beforeIndex == 0 && nextIndex == cpCount - 1)
    std::swap(beforeIndex, nextIndex);

  int beforePointIndex = m_controlPoints[beforeIndex].m_pointIndex;
  int nextPointIndex   = (isSelfLoop() && nextIndex == 0)
                           ? stroke->getControlPointCount() - 1
                           : m_controlPoints[nextIndex].m_pointIndex;
  double w  = stroke->getW(pos);
  double w0 = stroke->getParameterAtControlPoint(beforePointIndex);
  double w4 = stroke->getParameterAtControlPoint(nextPointIndex);
  if (w0 > w) return;
  assert(w0 <= w && w <= w4);

  double t = 1;
  double s = 1;

  if (isSpeedOutLinear(beforeIndex)) {
    m_controlPoints[beforeIndex].m_speedOut =
        (stroke->getControlPoint(nextPointIndex) -
         stroke->getControlPoint(beforePointIndex)) *
        0.3;
    if (!isSpeedInLinear(beforeIndex))
      m_controlPoints[beforeIndex].m_isCusp = true;
  } else if (!isSpeedOutLinear(beforeIndex) && !isSpeedInLinear(beforeIndex) &&
             !isCusp(beforeIndex)) {
    t = 1 - fabs(w - w0) / fabs(w4 - w0);
    moveSingleControlPoint(beforeIndex, t * delta);
    t = 1 - t;
  }

  if (isSpeedInLinear(nextIndex)) {
    m_controlPoints[nextIndex].m_speedIn =
        (stroke->getControlPoint(nextPointIndex) -
         stroke->getControlPoint(beforePointIndex)) *
        0.3;
    if (!isSpeedOutLinear(nextIndex))
      m_controlPoints[nextIndex].m_isCusp = true;
  } else if (!isSpeedInLinear(nextIndex) && !isSpeedOutLinear(nextIndex) &&
             !isCusp(nextIndex)) {
    s = 1 - fabs(w4 - w) / fabs(w4 - w0);
    moveSingleControlPoint(nextIndex, s * delta);
    s = 1 - s;
  }

  moveSpeedOut(beforeIndex, delta * s, 0);
  // updateDependentPoint(beforeIndex);
  moveSpeedIn(nextIndex, delta * t, 0);
  // updateDependentPoint(nextIndex);

  updatePoints();
}

//-----------------------------------------------------------------------------

ControlPointEditorStroke::PointType ControlPointEditorStroke::getPointTypeAt(
    const TPointD &pos, double &distance2, int &index) const {
  TStroke *stroke = getStroke();
  if (!stroke) return NONE;
  double w              = stroke->getW(pos);
  TPointD p             = stroke->getPoint(w);
  double strokeDistance = tdistance2(p, pos);

  int precPointIndex     = -1;
  double minPrecDistance = 0;
  double minDistance2    = distance2;
  index                  = -1;
  PointType type         = NONE;
  int cpCount            = m_controlPoints.size();
  int i;
  for (i = 0; i < cpCount; i++) {
    ControlPoint cPoint = m_controlPoints[i];
    TPointD point       = stroke->getControlPoint(cPoint.m_pointIndex);
    double cpDistance2  = tdistance2(pos, point);
    double distanceIn2  = !isSpeedInLinear(i)
                             ? tdistance2(pos, point - cPoint.m_speedIn)
                             : cpDistance2 + 1;
    double distanceOut2 = !isSpeedOutLinear(i)
                              ? tdistance2(pos, point + cPoint.m_speedOut)
                              : cpDistance2 + 1;
    if (i == 0 && !isSelfLoop())
      distanceIn2 = std::max(cpDistance2, distanceOut2) + 1;
    if (i == cpCount - 1 && !isSelfLoop())
      distanceOut2 = std::max(cpDistance2, distanceIn2) + 1;

    if (cpDistance2 < distanceIn2 && cpDistance2 < distanceOut2 &&
        (cpDistance2 < minDistance2 || index < 0)) {
      minDistance2 = cpDistance2;
      index        = i;
      type         = CONTROL_POINT;
    } else if (distanceIn2 < cpDistance2 && distanceIn2 < distanceOut2 &&
               (distanceIn2 < minDistance2 || index < 0)) {
      minDistance2 = distanceIn2;
      index        = i;
      type         = SPEED_IN;
    } else if (distanceOut2 < cpDistance2 && distanceOut2 < distanceIn2 &&
               (distanceOut2 < minDistance2 || index < 0)) {
      minDistance2 = distanceOut2;
      index        = i;
      type         = SPEED_OUT;
    }

    double cpw =
        stroke->getParameterAtControlPoint(m_controlPoints[i].m_pointIndex);
    if (w <= cpw) continue;
    double precDistance = w - cpw;
    if (precPointIndex < 0 || precDistance < minPrecDistance) {
      precPointIndex  = i;
      minPrecDistance = precDistance;
    }
  }

  if (minDistance2 < distance2)
    distance2 = minDistance2;
  else if (strokeDistance > distance2) {
    distance2 = strokeDistance;
    index     = -1;
    type      = NONE;
  } else {
    distance2 = minPrecDistance;
    index     = precPointIndex;
    type      = SEGMENT;
  }

  return type;
}

//=============================================================================
// ControlPointSelection
//-----------------------------------------------------------------------------

bool ControlPointSelection::isSelected(int index) const {
  return m_selectedPoints.find(index) != m_selectedPoints.end();
}

//-----------------------------------------------------------------------------

void ControlPointSelection::select(int index) {
  m_selectedPoints.insert(index);
}

//-----------------------------------------------------------------------------

void ControlPointSelection::unselect(int index) {
  m_selectedPoints.erase(index);
}

//-----------------------------------------------------------------------------

void ControlPointSelection::addMenuItems(QMenu *menu) {
  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  if (isEmpty() || currentStrokeIndex == -1 ||
      (m_controlPointEditorStroke &&
       m_controlPointEditorStroke->getControlPointCount() <= 1))
    return;
  QAction *linear   = menu->addAction(tr("Set Linear Control Point"));
  QAction *unlinear = menu->addAction(tr("Set Nonlinear Control Point"));
  menu->addSeparator();
  bool ret = connect(linear, SIGNAL(triggered()), this, SLOT(setLinear()));
  ret =
      ret && connect(unlinear, SIGNAL(triggered()), this, SLOT(setUnlinear()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void ControlPointSelection::setLinear() {
  TTool *tool            = TTool::getApplication()->getCurrentTool()->getTool();
  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  TVectorImageP vi(tool->getImage(false));
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;
  TUndo *undo;
  if (tool->getApplication()->getCurrentObject()->isSpline())
    undo = new UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
  else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoControlPointEditor *cpEditorUndo =
        new UndoControlPointEditor(level, tool->getCurrentFid());
    cpEditorUndo->addOldStroke(currentStrokeIndex,
                               vi->getVIStroke(currentStrokeIndex));
    undo = cpEditorUndo;
  }
  if (m_controlPointEditorStroke->getControlPointCount() == 0) return;

  bool isChanged = m_controlPointEditorStroke->setControlPointsLinear(
      m_selectedPoints, true);

  if (!isChanged) return;
  TUndoManager::manager()->add(undo);
  tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ControlPointSelection::setUnlinear() {
  TTool *tool            = TTool::getApplication()->getCurrentTool()->getTool();
  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  TVectorImageP vi(tool->getImage(false));
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;

  TUndo *undo;
  if (tool->getApplication()->getCurrentObject()->isSpline())
    undo = new UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
  else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoControlPointEditor *cpEditorUndo =
        new UndoControlPointEditor(level, tool->getCurrentFid());
    cpEditorUndo->addOldStroke(currentStrokeIndex,
                               vi->getVIStroke(currentStrokeIndex));
    undo = cpEditorUndo;
  }
  if (m_controlPointEditorStroke->getControlPointCount() == 0) return;

  bool isChanged = m_controlPointEditorStroke->setControlPointsLinear(
      m_selectedPoints, false);

  if (!isChanged) return;
  TUndoManager::manager()->add(undo);
  tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ControlPointSelection::deleteControlPoints() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  TVectorImageP vi(tool->getImage(false));
  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;

  // Inizializzo l'UNDO
  TUndo *undo;
  bool isCurrentObjectSpline =
      tool->getApplication()->getCurrentObject()->isSpline();
  if (isCurrentObjectSpline)
    undo = new UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
  else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoControlPointEditor *cpEditorUndo =
        new UndoControlPointEditor(level, tool->getCurrentFid());
    cpEditorUndo->addOldStroke(currentStrokeIndex,
                               vi->getVIStroke(currentStrokeIndex));
    undo = cpEditorUndo;
  }

  int i;
  for (i = m_controlPointEditorStroke->getControlPointCount() - 1; i >= 0; i--)
    if (isSelected(i)) m_controlPointEditorStroke->deleteControlPoint(i);

  if (m_controlPointEditorStroke->getControlPointCount() == 0) {
    m_controlPointEditorStroke->setStroke((TVectorImage *)0, -1);
    if (!isCurrentObjectSpline) {
      UndoControlPointEditor *cpEditorUndo =
          dynamic_cast<UndoControlPointEditor *>(undo);
      if (cpEditorUndo) cpEditorUndo->isStrokeDelete(true);
    }
  }

  // La spline non puo' essere cancellata completamente!!!
  if (vi->getStrokeCount() == 0) {
    if (TTool::getApplication()->getCurrentObject()->isSpline()) {
      std::vector<TPointD> points;
      double d = 10;
      points.push_back(TPointD(-d, 0));
      points.push_back(TPointD(0, 0));
      points.push_back(TPointD(d, 0));
      TStroke *stroke = new TStroke(points);
      vi->addStroke(stroke, false);
      m_controlPointEditorStroke->setStrokeIndex(0);
    }
  }
  tool->notifyImageChanged();
  selectNone();
  // Registro l'UNDO
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void ControlPointSelection::enableCommands() {
  enableCommand(this, "MI_Clear", &ControlPointSelection::deleteControlPoints);
}
