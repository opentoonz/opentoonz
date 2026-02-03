

#include <tools/modifiers/modifiersmooth.h>
#include <algorithm>



//*****************************************************************************************
//    TModifierSmooth implementation
//*****************************************************************************************


TModifierSmooth::TModifierSmooth(int radius):
  radius(radius) { }


void
TModifierSmooth::modifyTrack(
  const TTrack &track,
  TTrackList &outTracks )
{
  int radius = abs(this->radius);
  
  if (!track.handler && radius) {
    Handler *handler = new Handler(radius);
    track.handler = handler;
    handler->track = new TTrack(track);
  }

  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler)
    return TInputModifier::modifyTrack(track, outTracks);
  
  radius = handler->radius;
  outTracks.push_back(handler->track);
  TTrack &subTrack = *handler->track;
  
  if (!track.changed())
    return;
  
  // remove points
  int start = std::max(0, track.size() - track.pointsAdded);
  subTrack.truncate(start);

  // add points
  // extra points will be added at the beginning and at the end
  int size = track.size() + 2*radius;
  double k = 1.0/(2*radius + 1);
  TTrackTangent accum;
  for(int i = start - 2*radius; i < size; ++i) {
    
    const TTrackPoint &p1 = track[i];
    accum.position += p1.position;
    accum.pressure += p1.pressure;
    accum.tilt     += p1.tilt;
    if (i < start)
      continue;

    double originalIndex;
    if (i <= radius) {
      originalIndex = i/(double)(radius + 1);
    } else
    if (i >= size - radius - 1) {
      originalIndex = track.size() - 1 - (size - i - 1)/(double)(radius + 1);
    } else {
      originalIndex = i - radius;
    }
    
    TTrackPoint p = subTrack.pointFromOriginal(i - radius);
    p.position = accum.position*k;
    p.pressure = accum.pressure*k;
    p.tilt = accum.tilt*k;
    p.originalIndex = originalIndex;
    p.final = p.final && i == size - 1;
    subTrack.push_back(p, false);
    
    const TTrackPoint &p0 = track[i - 2*radius];
    accum.position -= p0.position;
    accum.pressure -= p0.pressure;
    accum.tilt     -= p0.tilt;
  }
  
  // fix points
  if (track.fixedFinished()) {
    subTrack.fix_all();
  } else
  if (track.fixedSize()) {
    subTrack.fix_to(track.fixedSize());
  }
  
  track.resetChanges();
}

