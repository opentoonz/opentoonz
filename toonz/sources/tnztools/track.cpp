

#include <tools/track.h>


//*****************************************************************************************
//    Static fields
//*****************************************************************************************

TTrack::Id TTrack::m_lastId = 0;


//*****************************************************************************************
//    TTrackTransform implemantation
//*****************************************************************************************

TAffine TTrackTransform::makeTiltTransform(const TAffine &a) {
  double l1 = a.a11*a.a11 + a.a21*a.a22;
  double l2 = a.a11*a.a11 + a.a21*a.a22;
  double l = std::max(l1, l2);
  double k = l > TConsts::epsilon*TConsts::epsilon ? 1/sqrt(l) : 0;
  return TAffine( a.a11*k, a.a12*k, 0,
                  a.a21*k, a.a22*k, 0 );
}


//*****************************************************************************************
//    TTrackIntrOrig implemantation
//*****************************************************************************************

TTrackPoint
TTrackIntrOrig::interpolate(double index) {
  return track.original ? track.calcPointFromOriginal(track.originalIndexByIndex(index))
                        : track.interpolateLinear(index);
}


//*****************************************************************************************
//    TTrack implemantation
//*****************************************************************************************

TTrack::TTrack(
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  const TInputState::KeyHistory::Holder &keyHistory,
  const TInputState::ButtonHistory::Holder &buttonHistory,
  bool hasPressure,
  bool hasTilt,
  double timeOffset
):
  id(++m_lastId),
  deviceId(deviceId),
  touchId(touchId),
  keyHistory(keyHistory),
  buttonHistory(buttonHistory),
  hasPressure(hasPressure),
  hasTilt(hasTilt),
  original(),
  timeOffset(timeOffset),
  rootTimeOffset(timeOffset),
  pointsRemoved(),
  pointsAdded(),
  fixedPointsAdded(),
  m_pointsFixed()
  { }

TTrack::TTrack(const TTrack &original, double timeOffset):
  id(++m_lastId),
  deviceId(original.deviceId),
  touchId(original.touchId),
  keyHistory(original.keyHistory),
  buttonHistory(original.buttonHistory),
  hasPressure(original.hasPressure),
  hasTilt(original.hasTilt),
  original(&original),
  timeOffset(timeOffset),
  rootTimeOffset(original.rootTimeOffset + timeOffset),
  pointsRemoved(),
  pointsAdded(),
  fixedPointsAdded(),
  m_pointsFixed()
  { }

const TTrack*
TTrack::root() const
  { return original ? original->root() : this; }

int
TTrack::level() const
  { return original ? original->level() + 1 : 0; }

int
TTrack::floorIndex(double index, double *outFrac) const {
  int i = floorIndexNoClamp(index);
  if (i > size() - 1) {
    if (outFrac) *outFrac = 0.0;
    return size() - 1;
  }
  if (i < 0) {
    if (outFrac) *outFrac = 0.0;
    return 0;
  }
  if (outFrac) *outFrac = std::max(0.0, index - (double)i);
  return i;
}

void
TTrack::push_back(const TTrackPoint &point, bool fixed) {
  assert(m_points.empty() || !m_points.back().final);
  m_points.push_back(point);
  TTrackPoint &p = m_points.back();
  if (m_points.size() <= 1) {
    p.length = 0;
  } else {
    const TTrackPoint &prev = *(m_points.rbegin() + 1);

    // fix originalIndex
    if (p.originalIndex < prev.originalIndex)
        p.originalIndex = prev.originalIndex;

    // fix time
    p.time = std::max(p.time, prev.time + TToolTimer::step);

    // calculate length
    p.length = prev.length + tdistance(p.position, prev.position);
  }
  ++pointsAdded;
  if (fixed) fix_all();
}

void
TTrack::pop_back(int count) {
  if (count > size()) count = size();
  if (count <= 0) return;
  assert(size() - count >= m_pointsFixed);
  m_points.resize(size() - count);
  if (pointsAdded > count)
    { pointsAdded -= count; return; }
  if (pointsAdded > 0)
    { count -= pointsAdded; pointsAdded = 0; }
  pointsRemoved += count;
}

void
TTrack::fix_points(int count) {
  count = std::min(count, previewSize());
  assert(count >= 0);
  if (count <= 0) return;
  m_pointsFixed += count;
  fixedPointsAdded += count;
}


TPointD
TTrack::calcTangent(double index, double distance) const {
  double minDistance = 10.0*TConsts::epsilon;
  if (distance < minDistance) distance = minDistance;
  TTrackPoint p = calcPoint(index);
  TTrackPoint pp = calcPoint(indexByLength(p.length - distance));
  TPointD dp = p.position - pp.position;
  double lenSqr = dp.x*dp.x + dp.y*dp.y;
  return lenSqr > TConsts::epsilon*TConsts::epsilon ? dp*(1.0/sqrt(lenSqr)) : TPointD();
}

double
TTrack::rootIndexByIndex(double index) const {
  return original
       ? original->rootIndexByIndex( originalIndexByIndex(index) )
       : index;
}

TTrackPoint
TTrack::calcRootPoint(double index) const {
  return original
       ? original->calcRootPoint( originalIndexByIndex(index) )
       : calcPoint(index);
}
