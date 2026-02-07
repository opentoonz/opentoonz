

#include "controlpointselection.h"
#include "tvectorimage.h"
#include "tmathutil.h"
#include "controlpointeditortool.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"

#include <memory>

using namespace ToolUtils;

//-----------------------------------------------------------------------------

namespace {

//-----------------------------------------------------------------------------

constexpr double MIN_DISTANCE    = 0.02;
constexpr double MIN_CROSS_VALUE = 0.09;  // more than 5°
constexpr double LINEAR_FACTOR   = 0.01;

//-----------------------------------------------------------------------------

inline bool isLinearPoint(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2) {
  return (tdistance(p0, p1) < MIN_DISTANCE) &&
         (tdistance(p1, p2) < MIN_DISTANCE);
}

//-----------------------------------------------------------------------------

//! Returns \b true if point \b p1 is a cusp.
bool isCuspPoint(const TPointD &p0, const TPointD &p1, const TPointD &p2) {
  TPointD p0_p1(p0 - p1), p2_p1(p2 - p1);
  double n1 = norm(p0_p1), n2 = norm(p2_p1);

  // Partial linear points are ALWAYS cusps (since directions from them are
  // determined by neighbours, not by the points themselves)
  if ((n1 < MIN_DISTANCE) || (n2 < MIN_DISTANCE)) return true;

  p0_p1 = p0_p1 * (1.0 / n1);
  p2_p1 = p2_p1 * (1.0 / n2);

  return (p0_p1 * p2_p1 > 0) ||
         (std::fabs(cross(p0_p1, p2_p1)) > MIN_CROSS_VALUE);
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
/*! Insert a point in the longest chunk between chunk \b indexA and chunk \b
 * indexB. */
void insertPoint(TStroke *stroke, int indexA, int indexB) {
  assert(stroke);
  int chunkCount = indexB - indexA;
  if (chunkCount % 2 == 0) return;

  double maxLength = 0;
  double firstW = 0.0, lastW = 1.0;

  for (int j = indexA; j < indexB; ++j) {
    // find the longest chunk
    double w0 = stroke->getW(stroke->getChunk(j)->getP0());
    double w1 = (j == stroke->getChunkCount() - 1)
                    ? 1.0
                    : stroke->getW(stroke->getChunk(j)->getP2());

    double length0     = stroke->getLength(w0);
    double length1     = stroke->getLength(w1);
    double chunkLength = length1 - length0;

    if (chunkLength > maxLength) {
      firstW    = w0;
      lastW     = w1;
      maxLength = chunkLength;
    }
  }

  stroke->insertControlPoints((firstW + lastW) * 0.5);
}

}  // namespace

//=============================================================================
// ControlPointEditorStroke
//-----------------------------------------------------------------------------

ControlPointEditorStroke::ControlPointEditorStroke(
    const ControlPointEditorStroke &other)
    : m_controlPoints(other.m_controlPoints)
    , m_vi(other.m_vi)
    , m_strokeIndex(other.m_strokeIndex) {}

ControlPointEditorStroke &ControlPointEditorStroke::operator=(
    const ControlPointEditorStroke &other) {
  if (this != &other) {
    m_controlPoints = other.m_controlPoints;
    m_vi            = other.m_vi;
    m_strokeIndex   = other.m_strokeIndex;
  }
  return *this;
}

ControlPointEditorStroke *ControlPointEditorStroke::clone() const {
  return new ControlPointEditorStroke(*this);
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::nextIndex(int index) const {
  int cpCount = static_cast<int>(m_controlPoints.size());
  if (++index < cpCount) return index;

  if (isSelfLoop()) {
    index %= cpCount;
    return (index < 0) ? index + cpCount : index;
  }

  return -1;
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::prevIndex(int index) const {
  int cpCount = static_cast<int>(m_controlPoints.size());
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

  for (int i = stroke->getChunkCount() - 1; i > 0; --i) {
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
  int cpCount = stroke->getControlPointCount();

  if (cpCount == 3) {
    const TThickQuadratic *chunk = stroke->getChunk(0);
    if (chunk->getP0() == chunk->getP1() && chunk->getP0() == chunk->getP2()) {
      // It's a single point
      m_controlPoints.append(
          ControlPoint(0, TPointD(0.0, 0.0), TPointD(0.0, 0.0), true));
      return;
    }
  }

  for (int i = 0; i < cpCount; i += 4) {
    TThickPoint speedIn, speedOut;
    TThickPoint p     = stroke->getControlPoint(i);
    TThickPoint precP = stroke->getControlPoint(i - 1);
    TThickPoint nextP = stroke->getControlPoint(i + 1);

    if (0 < i && i < cpCount - 1) {
      // calculate speedIn and speedOut
      speedIn  = p - precP;
      speedOut = nextP - p;
    }

    if (i == 0) {
      // calculate only speedOut
      speedOut = nextP - p;
      if (isSelfLoop()) {
        precP   = stroke->getControlPoint(cpCount - 2);
        speedIn = p - precP;
      }
    }

    if (i == cpCount - 1) {
      // calculate only speedIn
      speedIn = p - precP;
    }

    if (i == cpCount - 1 && isSelfLoop()) {
      // If stroke is selfLoop, insert only the first of two coincident points
      break;
    }

    bool isCusp = ((i != 0 && i != cpCount - 1) || (isSelfLoop() && i == 0))
                      ? isCuspPoint(precP, p, nextP)
                      : true;

    m_controlPoints.append(ControlPoint(i, speedIn, speedOut, isCusp));
  }
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getPureDependentPoint(int index) const {
  TStroke *stroke = getStroke();
  if (!stroke) return {};

  bool selfLoop  = isSelfLoop();
  int cpCount    = selfLoop ? static_cast<int>(m_controlPoints.size()) + 1
                            : static_cast<int>(m_controlPoints.size());
  int nextIndex  = (selfLoop && index == cpCount - 2) ? 0 : index + 1;
  int pointIndex = m_controlPoints[index].m_pointIndex;

  TThickPoint oldP(stroke->getControlPoint(pointIndex + 2));
  TPointD oldSpeedOutP = stroke->getControlPoint(pointIndex + 1);
  TPointD oldSpeedInP  = stroke->getControlPoint(pointIndex + 3);

  double dist = tdistance(oldSpeedOutP, oldSpeedInP);
  double t = (dist > 1e-4) ? tdistance(oldSpeedInP, convert(oldP)) / dist : 0.5;

  TPointD speedOutPoint(getSpeedOutPoint(index));
  TPointD nextSpeedInPoint(getSpeedInPoint(nextIndex));

  return {(1 - t) * nextSpeedInPoint + t * speedOutPoint, oldP.thick};
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::getDependentPoints(
    int index, std::vector<std::pair<int, TThickPoint>> &points) const {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  int cpCount = static_cast<int>(m_controlPoints.size());
  if (index == cpCount && isSelfLoop()) {
    // strange, but was treated...
    index = 0;
  }

  if (index == 0 && cpCount == 1) {
    // Single point case
    TStroke *stroke = getStroke();
    TThickPoint pos(stroke->getControlPoint(m_controlPoints[0].m_pointIndex));

    points.emplace_back(1, pos);
    points.emplace_back(2, pos);
    return;
  }

  int prev = prevIndex(index);
  if (prev >= 0) {
    int prevPointIndex = m_controlPoints[prev].m_pointIndex;

    if (isSpeedOutLinear(prev))
      points.emplace_back(prevPointIndex + 1, getSpeedOutPoint(prev));
    points.emplace_back(prevPointIndex + 2, getPureDependentPoint(prev));
    points.emplace_back(prevPointIndex + 3, getSpeedInPoint(index));
  }

  int next = nextIndex(index);
  if (next >= 0) {
    int pointIndex = m_controlPoints[index].m_pointIndex;

    points.emplace_back(pointIndex + 1, getSpeedOutPoint(index));
    points.emplace_back(pointIndex + 2, getPureDependentPoint(index));
    if (isSpeedInLinear(next))
      points.emplace_back(pointIndex + 3, getSpeedInPoint(next));
  }
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::updatePoints() {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  bool selfLoop = isSelfLoop();
  // If only one point remains, stroke cannot be self-loop
  if (selfLoop && m_controlPoints.size() == 1) {
    stroke->setSelfLoop(false);
    selfLoop = false;
  }

  // If self loop, add one more point to cpCount
  std::vector<TThickPoint> points;

  int cpCount = selfLoop ? static_cast<int>(m_controlPoints.size()) + 1
                         : static_cast<int>(m_controlPoints.size());
  if (cpCount == 1) {
    // Single point case
    points.resize(3, getControlPoint(0));
  } else {
    std::vector<std::pair<int, TThickPoint>> dependentPoints;

    points.push_back(getControlPoint(0));
    points.push_back(getSpeedOutPoint(0));

    int currPointIndex = m_controlPoints[0].m_pointIndex + 1;
    for (int i = 1; i < cpCount; ++i) {
      bool isLastSelfLoopPoint = (selfLoop && i == cpCount - 1);
      int index                = isLastSelfLoopPoint ? 0 : i;

      TThickPoint p  = getControlPoint(index);
      int pointIndex = isLastSelfLoopPoint
                           ? getStroke()->getControlPointCount()
                           : m_controlPoints[index].m_pointIndex;

      dependentPoints.clear();
      getDependentPoints(index, dependentPoints);

      size_t j;
      for (j = 0;
           j < dependentPoints.size() && dependentPoints[j].first < pointIndex;
           ++j) {
        if (currPointIndex < dependentPoints[j].first) {
          currPointIndex = dependentPoints[j].first;
          points.push_back(dependentPoints[j].second);
        }
      }

      points.push_back(p);

      for (; j < dependentPoints.size(); ++j) {
        if (currPointIndex < dependentPoints[j].first) {
          currPointIndex = dependentPoints[j].first;
          points.push_back(dependentPoints[j].second);
        }
      }
    }
  }

  stroke->reshape(points.data(), points.size());
  m_vi->notifyChangedStrokes(m_strokeIndex);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::updateDependentPoint(int index) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  std::vector<std::pair<int, TThickPoint>> points;
  getDependentPoints(index, points);

  for (const auto &[pointIdx, point] : points)
    stroke->setControlPoint(pointIdx, point);

  m_vi->notifyChangedStrokes(m_strokeIndex);
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveSpeedOut(int index, const TPointD &delta,
                                            double minDistance) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  // If the next cp has linear speed in, it must be recomputed
  bool selfLoop = isSelfLoop();
  int cpCount   = selfLoop ? static_cast<int>(m_controlPoints.size()) + 1
                           : static_cast<int>(m_controlPoints.size());
  int nextIndex = (selfLoop && index == cpCount - 2) ? 0 : index + 1;
  if (m_controlPoints[nextIndex].m_isCusp && isSpeedInLinear(nextIndex))
    setLinearSpeedIn(nextIndex, true, false);

  // Update the speedOut
  m_controlPoints[index].m_speedOut += delta;
  const TPointD &newP = m_controlPoints[index].m_speedOut;

  if (areAlmostEqual(newP.x, 0, minDistance) &&
      areAlmostEqual(newP.y, 0, minDistance)) {
    // Set to linear
    setLinearSpeedOut(index);
    return;
  }

  if (!m_controlPoints[index].m_isCusp && !isSpeedInLinear(index)) {
    // Recalculate SpeedIn
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
  int cpCount   = selfLoop ? static_cast<int>(m_controlPoints.size()) + 1
                           : static_cast<int>(m_controlPoints.size());
  int prevIndex = (selfLoop && index == 0) ? cpCount - 2 : index - 1;
  if (m_controlPoints[prevIndex].m_isCusp && isSpeedOutLinear(prevIndex))
    setLinearSpeedOut(prevIndex, true, false);

  // Update the speedIn
  m_controlPoints[index].m_speedIn -= delta;
  const TPointD &newP = m_controlPoints[index].m_speedIn;

  if (areAlmostEqual(newP.x, 0, minDistance) &&
      areAlmostEqual(newP.y, 0, minDistance)) {
    // Set to linear
    setLinearSpeedIn(index);
    return;
  }

  if (!m_controlPoints[index].m_isCusp && !isSpeedOutLinear(index)) {
    // Recalculate SpeedOut
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
  int cpCount   = selfLoop ? static_cast<int>(m_controlPoints.size()) + 1
                           : static_cast<int>(m_controlPoints.size());

  TThickPoint p = stroke->getControlPoint(pointIndex);
  p             = TThickPoint(p + delta, p.thick);
  stroke->setControlPoint(pointIndex, p);
  if (pointIndex == 0 && selfLoop)
    stroke->setControlPoint(stroke->getControlPointCount() - 1, p);

  // Directions must be recalculated in the linear cases
  if ((selfLoop || index > 0) && isSpeedInLinear(index)) {
    setLinearSpeedIn(index, true, false);
  }

  if ((selfLoop || index < cpCount - 1) && isSpeedOutLinear(index)) {
    setLinearSpeedOut(index, true, false);
  }
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::setStroke(const TVectorImageP &vi,
                                         int strokeIndex) {
  bool ret = true;
  if (m_strokeIndex == strokeIndex && m_vi == vi) ret = false;

  m_strokeIndex = strokeIndex;
  m_vi          = vi;

  if (!vi || strokeIndex == -1) {
    m_controlPoints.clear();
    return true;
  }

  TStroke *stroke              = getStroke();
  const TThickQuadratic *chunk = stroke->getChunk(0);

  if (stroke->getControlPointCount() == 3 && chunk->getP0() == chunk->getP1() &&
      chunk->getP0() == chunk->getP2()) {
    resetControlPoints();
    return ret;
  }

  adjustChunkParity();
  resetControlPoints();
  return ret;
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getControlPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index &&
         index < static_cast<int>(m_controlPoints.size()));
  return stroke->getControlPoint(m_controlPoints[index].m_pointIndex);
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::getIndexPointInStroke(int index) const {
  return m_controlPoints[index].m_pointIndex;
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getSpeedInPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index &&
         index < static_cast<int>(m_controlPoints.size()));

  const ControlPoint &cp = m_controlPoints[index];
  return stroke->getControlPoint(cp.m_pointIndex) - cp.m_speedIn;
}

//-----------------------------------------------------------------------------

TThickPoint ControlPointEditorStroke::getSpeedOutPoint(int index) const {
  TStroke *stroke = getStroke();
  assert(stroke && 0 <= index &&
         index < static_cast<int>(m_controlPoints.size()));

  const ControlPoint &cp = m_controlPoints[index];
  return stroke->getControlPoint(cp.m_pointIndex) + cp.m_speedOut;
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::isCusp(int index) const {
  assert(0 <= index && index < static_cast<int>(getControlPointCount()));
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
  assert(index < static_cast<int>(m_controlPoints.size()));
  return (std::fabs(m_controlPoints[index].m_speedIn.x) <= MIN_DISTANCE) &&
         (std::fabs(m_controlPoints[index].m_speedIn.y) <= MIN_DISTANCE);
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::isSpeedOutLinear(int index) const {
  assert(index < static_cast<int>(m_controlPoints.size()));
  return (std::fabs(m_controlPoints[index].m_speedOut.x) <= MIN_DISTANCE) &&
         (std::fabs(m_controlPoints[index].m_speedOut.y) <= MIN_DISTANCE);
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

  TThickPoint point     = stroke->getControlPoint(pointIndex);
  TThickPoint precPoint = (pointIndex > 2)
                              ? stroke->getControlPoint(pointIndex - 3)
                              : TThickPoint();

  if (linear) {
    TThickPoint p(point - precPoint);
    double n                         = norm(p);
    TThickPoint speedIn              = (n != 0.0)
                                           ? (LINEAR_FACTOR / n) * p
                                           : TThickPoint(LINEAR_FACTOR, LINEAR_FACTOR, 0.0);
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

  TThickPoint point     = stroke->getControlPoint(pointIndex);
  TThickPoint nextPoint = (pointIndex < cpCount - 3)
                              ? stroke->getControlPoint(pointIndex + 3)
                              : TThickPoint();

  if (linear) {
    TThickPoint p(nextPoint - point);
    double n                          = norm(p);
    TThickPoint speedOut              = (n != 0.0)
                                            ? (LINEAR_FACTOR / n) * p
                                            : TThickPoint(LINEAR_FACTOR, LINEAR_FACTOR, 0.0);
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

  bool precChanged = false;
  bool nextChanged = false;

  if (isLinear != isSpeedInLinear(index)) {
    setLinearSpeedIn(index, isLinear, updatePoints);
    precChanged = true;
  }

  if (isLinear != isSpeedOutLinear(index)) {
    setLinearSpeedOut(index, isLinear, updatePoints);
    nextChanged = true;
  }

  bool ret = nextChanged || precChanged;
  if (ret) m_controlPoints[index].m_isCusp = true;
  return ret;
}

//-----------------------------------------------------------------------------

bool ControlPointEditorStroke::setControlPointsLinear(
    const std::set<int> &points, bool isLinear) {
  bool isChanged = false;
  for (int point : points)
    isChanged = setLinear(point, isLinear, false) || isChanged;

  for (int point : points) updateDependentPoint(point);

  return isChanged;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::moveControlPoint(int index,
                                                const TPointD &delta) {
  TStroke *stroke = getStroke();
  if (!stroke) return;
  assert(stroke && 0 <= index &&
         index < static_cast<int>(getControlPointCount()));

  moveSingleControlPoint(index, delta);
  updateDependentPoint(index);
}

//-----------------------------------------------------------------------------

int ControlPointEditorStroke::addControlPoint(const TPointD &pos) {
  TStroke *stroke = getStroke();
  if (!stroke) return -1;

  double d       = 0.01;
  int indexAtPos = -1;
  int cpCount    = stroke->getControlPointCount();

  if (cpCount <= 3) {
    // it's a single chunk representing a point
    getPointTypeAt(pos, d, indexAtPos);
    return indexAtPos;
  }

  double w       = stroke->getW(pos);
  int pointIndex = stroke->getControlPointIndexAfterParameter(w);

  int index = 0;  // Initialize index
  for (int i = 0; i < getControlPointCount(); ++i) {
    // Find ControlPoint corresponding to pointIndex
    if (pointIndex == m_controlPoints[i].m_pointIndex + 1 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 2 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 3 ||
        pointIndex == m_controlPoints[i].m_pointIndex + 4) {
      index = i;
      break;  // Found
    }
  }

  ControlPoint precCp = m_controlPoints[index];
  assert(precCp.m_pointIndex >= 0);
  std::vector<TThickPoint> points;

  for (int i = 0; i < cpCount; ++i) {
    if (i != precCp.m_pointIndex + 1 && i != precCp.m_pointIndex + 2 &&
        i != precCp.m_pointIndex + 3)
      points.push_back(stroke->getControlPoint(i));

    if (i == precCp.m_pointIndex + 2) {
      bool isBeforePointLinear = isSpeedOutLinear(index);
      int nextIndex            = (isSelfLoop() &&
                       index == static_cast<int>(m_controlPoints.size()) - 1)
                                     ? 0
                                     : index + 1;
      bool isNextPointLinear =
          nextIndex < static_cast<int>(m_controlPoints.size()) &&
          isSpeedInLinear(nextIndex);

      TThickPoint a0 = stroke->getControlPoint(precCp.m_pointIndex);
      TThickPoint a1 = stroke->getControlPoint(precCp.m_pointIndex + 1);
      TThickPoint a2 = stroke->getControlPoint(precCp.m_pointIndex + 2);
      TThickPoint a3 = stroke->getControlPoint(precCp.m_pointIndex + 3);
      TThickPoint a4 = stroke->getControlPoint(precCp.m_pointIndex + 4);
      double dist2   = tdistance2(pos, TPointD(a2));
      TThickPoint d0, d1, d2, d3, d4, d5, d6;

      if (isBeforePointLinear && isNextPointLinear) {
        // If both points are linear, insert a linear point
        d0 = a1;
        d3 = stroke->getThickPoint(w);
        d6 = a3;
        d2 = computeLinearPoint(d0, d3, LINEAR_FACTOR, true);   // SpeedIn
        d4 = computeLinearPoint(d3, d6, LINEAR_FACTOR, false);  // SpeedOut
        d1 = 0.5 * (d0 + d2);
        d5 = 0.5 * (d4 + d6);
      } else if (dist2 < 32) {
        // Very close to the point that is not displayed
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
          // if in the second chunk, swap points
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
        d1 = computeLinearPoint(d0, d2, LINEAR_FACTOR, false);
      else if (isNextPointLinear && !isBeforePointLinear)
        d5 = computeLinearPoint(d4, d6, LINEAR_FACTOR, true);

      points.push_back(d0);
      points.push_back(d1);
      points.push_back(d2);
      points.push_back(d3);
      points.push_back(d4);
      points.push_back(d5);
      points.push_back(d6);
    }
  }

  stroke->reshape(points.data(), points.size());
  resetControlPoints();

  getPointTypeAt(pos, d, indexAtPos);
  return indexAtPos;
}

//-----------------------------------------------------------------------------

void ControlPointEditorStroke::deleteControlPoint(int index) {
  TStroke *stroke = getStroke();
  if (!stroke) return;

  assert(stroke && 0 <= index &&
         index < static_cast<int>(getControlPointCount()));

  // It's a single chunk representing a point
  if (stroke->getControlPointCount() <= 3 ||
      (isSelfLoop() && stroke->getControlPointCount() <= 5)) {
    m_controlPoints.clear();
    m_vi->deleteStroke(m_strokeIndex);
    return;
  }

  QList<int> newPointsIndex;
  for (int i = 0; i < static_cast<int>(getControlPointCount()) - 1; ++i)
    newPointsIndex.push_back(m_controlPoints[i].m_pointIndex);

  m_controlPoints.removeAt(index);
  updatePoints();

  // Update point indices in stroke
  assert(static_cast<int>(newPointsIndex.size()) ==
         static_cast<int>(getControlPointCount()));
  for (int i = 0; i < static_cast<int>(getControlPointCount()); ++i)
    m_controlPoints[i].m_pointIndex = newPointsIndex.at(i);

  int prev = prevIndex(index);
  if (prev >= 0 && isSpeedOutLinear(prev)) {
    setLinearSpeedOut(prev);
    updateDependentPoint(prev);
  }

  if (index < static_cast<int>(m_controlPoints.size()) &&
      isSpeedInLinear(index)) {
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
  // Checks for self-loop stroke case
  if (isSelfLoop() && beforeIndex == 0 && nextIndex == cpCount - 1)
    std::swap(beforeIndex, nextIndex);

  int beforePointIndex = m_controlPoints[beforeIndex].m_pointIndex;
  int nextPointIndex   = (isSelfLoop() && nextIndex == 0)
                             ? stroke->getControlPointCount() - 1
                             : m_controlPoints[nextIndex].m_pointIndex;
  double w             = stroke->getW(pos);
  double w0            = stroke->getParameterAtControlPoint(beforePointIndex);
  double w4            = stroke->getParameterAtControlPoint(nextPointIndex);
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
    t = 1 - std::fabs(w - w0) / std::fabs(w4 - w0);
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
    s = 1 - std::fabs(w4 - w) / std::fabs(w4 - w0);
    moveSingleControlPoint(nextIndex, s * delta);
    s = 1 - s;
  }

  moveSpeedOut(beforeIndex, delta * s, 0);
  moveSpeedIn(nextIndex, delta * t, 0);
  updatePoints();
}

//-----------------------------------------------------------------------------

ControlPointEditorStroke::PointType ControlPointEditorStroke::getPointTypeAt(
    const TPointD &pos, double &distance2, int &index) const {
  TStroke *stroke = getStroke();
  if (!stroke) {
    index = -1;
    return NONE;
  }

  double w              = stroke->getW(pos);
  TPointD p             = stroke->getPoint(w);
  double strokeDistance = tdistance2(p, pos);

  int precPointIndex     = -1;
  double minPrecDistance = 0;
  double minDistance2    = distance2;
  index                  = -1;
  PointType type         = NONE;
  int cpCount            = static_cast<int>(m_controlPoints.size());

  for (int i = 0; i < cpCount; ++i) {
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

  if (minDistance2 < distance2) {
    distance2 = minDistance2;
  } else if (strokeDistance > distance2) {
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

  connect(linear, &QAction::triggered, this, &ControlPointSelection::setLinear);
  connect(unlinear, &QAction::triggered, this,
          &ControlPointSelection::setUnlinear);
}

//-----------------------------------------------------------------------------

void ControlPointSelection::setLinear() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  TVectorImageP vi(tool->getImage(false));
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;

  // Use unique_ptr for automatic cleanup
  std::unique_ptr<TUndo> undoPtr;

  if (tool->getApplication()->getCurrentObject()->isSpline()) {
    if (tool->getXsheet() &&
        tool->getXsheet()->getStageObject(tool->getObjectId())) {
      undoPtr = std::make_unique<UndoPath>(
          tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
    }
  } else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    if (level) {
      auto cpEditorUndo = std::make_unique<UndoControlPointEditor>(
          level, tool->getCurrentFid());
      cpEditorUndo->addOldStroke(currentStrokeIndex,
                                 vi->getVIStroke(currentStrokeIndex));
      undoPtr = std::move(cpEditorUndo);
    }
  }

  if (m_controlPointEditorStroke->getControlPointCount() == 0 || !undoPtr) {
    return;
  }

  bool isChanged = m_controlPointEditorStroke->setControlPointsLinear(
      m_selectedPoints, true);

  if (!isChanged) return;

  TUndoManager::manager()->add(undoPtr.release());
  tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ControlPointSelection::setUnlinear() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  TVectorImageP vi(tool->getImage(false));
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;

  // Use unique_ptr for automatic cleanup
  std::unique_ptr<TUndo> undoPtr;

  if (tool->getApplication()->getCurrentObject()->isSpline()) {
    if (tool->getXsheet() &&
        tool->getXsheet()->getStageObject(tool->getObjectId())) {
      undoPtr = std::make_unique<UndoPath>(
          tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
    }
  } else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    if (level) {
      auto cpEditorUndo = std::make_unique<UndoControlPointEditor>(
          level, tool->getCurrentFid());
      cpEditorUndo->addOldStroke(currentStrokeIndex,
                                 vi->getVIStroke(currentStrokeIndex));
      undoPtr = std::move(cpEditorUndo);
    }
  }

  if (m_controlPointEditorStroke->getControlPointCount() == 0 || !undoPtr) {
    return;
  }

  bool isChanged = m_controlPointEditorStroke->setControlPointsLinear(
      m_selectedPoints, false);

  if (!isChanged) return;

  TUndoManager::manager()->add(undoPtr.release());
  tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ControlPointSelection::deleteControlPoints() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  // cancel deleting while dragging points
  ControlPointEditorTool *cpTool = dynamic_cast<ControlPointEditorTool *>(tool);
  if (cpTool && cpTool->isBusy()) return;

  TVectorImageP vi(tool->getImage(false));
  int currentStrokeIndex = m_controlPointEditorStroke->getStrokeIndex();
  if (!vi || isEmpty() || currentStrokeIndex == -1) return;

  // Initialize UNDO
  std::unique_ptr<TUndo> undoPtr;
  bool isCurrentObjectSpline =
      tool->getApplication()->getCurrentObject()->isSpline();

  if (isCurrentObjectSpline) {
    if (tool->getXsheet() &&
        tool->getXsheet()->getStageObject(tool->getObjectId())) {
      undoPtr = std::make_unique<UndoPath>(
          tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
    }
  } else {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    if (level) {
      auto cpEditorUndo = std::make_unique<UndoControlPointEditor>(
          level, tool->getCurrentFid());
      cpEditorUndo->addOldStroke(currentStrokeIndex,
                                 vi->getVIStroke(currentStrokeIndex));
      undoPtr = std::move(cpEditorUndo);
    }
  }

  if (!undoPtr) return;

  for (int i = m_controlPointEditorStroke->getControlPointCount() - 1; i >= 0;
       --i)
    if (isSelected(i)) m_controlPointEditorStroke->deleteControlPoint(i);

  if (m_controlPointEditorStroke->getControlPointCount() == 0) {
    m_controlPointEditorStroke->setStroke(TVectorImageP(), -1);
    if (!isCurrentObjectSpline) {
      UndoControlPointEditor *cpEditorUndo =
          dynamic_cast<UndoControlPointEditor *>(undoPtr.get());
      if (cpEditorUndo) cpEditorUndo->isStrokeDelete(true);
    }
  }

  // Spline cannot be completely deleted!!!
  if (vi->getStrokeCount() == 0) {
    if (TTool::getApplication()->getCurrentObject()->isSpline()) {
      std::vector<TPointD> points;
      constexpr double d = 10;
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

  // Register UNDO
  TUndoManager::manager()->add(undoPtr.release());
}

//-----------------------------------------------------------------------------

void ControlPointSelection::enableCommands() {
  enableCommand(this, "MI_Clear", &ControlPointSelection::deleteControlPoints);
}
