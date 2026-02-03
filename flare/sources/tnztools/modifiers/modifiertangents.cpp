

#include <tools/modifiers/modifiertangents.h>


//*****************************************************************************************
//    TModifierTangents::Interpolator implementation
//*****************************************************************************************


TTrackPoint
TModifierTangents::Interpolator::interpolate(double index) {
  double frac;
  int i0 = track.floorIndex(index, &frac);
  int i1 = i0 + 1;

  TTrackPoint p;

  // calculate tangent length to make monotonic subdivisions,
  // (because we don't have valid input time)
  const TTrackPoint &p0 = track[i0];
  const TTrackPoint &p1 = track[i1];
  TTrackTangent t0 = i0 >= 0 && i0 < (int)tangents.size() ? tangents[i0] : TTrackTangent();
  TTrackTangent t1 = i1 >= 0 && i1 < (int)tangents.size() ? tangents[i1] : TTrackTangent();
  double l = p1.length - p0.length;
  
  t0.position.x *= l;
  t0.position.y *= l;
  t0.pressure *= l;
  t0.tilt.x *= l;
  t0.tilt.y *= l;
  
  t1.position.x *= l;
  t1.position.y *= l;
  t1.pressure *= l;
  t1.tilt.x *= l;
  t1.tilt.y *= l;
  
  return TTrack::interpolationSpline(p0, p1, t0, t1, frac);
}


//*****************************************************************************************
//    TModifierTangents implementation
//*****************************************************************************************


TTrackTangent
TModifierTangents::calcTangent(const TTrack &track, int index) {
  if (index <= 0 || index >= track.size() - 1)
    return TTrackTangent();
  
  const TTrackPoint &p0 = track[index-1];
  const TTrackPoint &p2 = track[index+1];

  // calculate tangent length by time
  // for that we need know time of actual user input
  // instead of time when message dispatched
  //double k = p2.time - p0.time;
  
  // calculate tangent based on length, until we have no valid times
  double k = p2.length - p0.length;
  
  k = k > TConsts::epsilon ? 1/k : 0;
  return TTrackTangent(
    (p2.position - p0.position)*k,
    (p2.pressure - p0.pressure)*k,
    (p2.tilt     - p0.tilt    )*k );
}


void
TModifierTangents::modifyTrack(
  const TTrack &track,
  TTrackList &outTracks )
{
  if (!track.handler) {
    Handler *handler = new Handler();
    track.handler = handler;
    handler->track = new TTrack(track);
    new Interpolator(*handler->track);
  }
  
  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler)
    return;
  
  outTracks.push_back(handler->track);
  TTrack &subTrack = *handler->track;
  Interpolator *intr = dynamic_cast<Interpolator*>(subTrack.getInterpolator().getPointer());
  if (!intr)
    return;
  
  if (!track.changed())
    return;
  
  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;
  
  // update tangents
  int tangentStart = start - 1;
  if (tangentStart < 0) tangentStart = 0;
  intr->tangents.resize(tangentStart);
  for(int i = tangentStart; i < track.size(); ++i)
    intr->tangents.push_back(calcTangent(track, i));

  // update subTrack
  subTrack.truncate(start);
  for(int i = start; i < track.size(); ++i)
    subTrack.push_back(subTrack.pointFromOriginal(i), false);
  
  // fix points
  if (track.fixedFinished()) {
    subTrack.fix_all();
  } else
  if (track.fixedSize()) {
    subTrack.fix_to(std::max(track.fixedSize() - 1, 1));
  }
  
  track.resetChanges();
}

