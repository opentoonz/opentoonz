

#include <tools/modifiers/modifiersimplify.h>
#include <algorithm>


//*****************************************************************************************
//    TModifierSimplify implementation
//*****************************************************************************************


TModifierSimplify::TModifierSimplify(double step):
  step(step) { }


void
TModifierSimplify::modifyTrack(
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

  if (!track.changed())
    return;
  
  // remove points
  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;
  int subStart = subTrack.floorIndex(subTrack.indexByOriginalIndex(start));
  if (subStart < 0) subStart = 0;
  start = track.floorIndex(subTrack[subStart].originalIndex);
  if (start < 0) start = 0;
  subTrack.truncate(subStart);

  // add points
  double step2 = step*step;
  TTrackPoint p0 = subTrack.back();
  for(int i = start; i < track.size(); ++i) {
    const TTrackPoint &p1 = subTrack.pointFromOriginal(i);
    if (!subTrack.empty() && tdistance2(p1.position, p0.position) < step2) {
      if (p0.pressure < p1.pressure) p0.pressure = p1.pressure;
      if (i == track.size() - 1) p0.position = p1.position;
      p0.tilt          = p1.tilt;
      p0.time          = p1.time;
      p0.final         = p1.final;
      subTrack.pop_back();
      subTrack.push_back(p0, false);
    } else {
      p0 = p1;
      subTrack.push_back(p0, false);
    }
  }

  // fix points
  if (track.fixedFinished())
    subTrack.fix_all();
  else
  if (track.fixedSize())
    subTrack.fix_to(
      subTrack.floorIndex( subTrack.indexByOriginalIndex(track.fixedSize()-1) ));
  
  track.resetChanges();
}
