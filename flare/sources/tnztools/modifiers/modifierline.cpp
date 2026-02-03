

#include <tools/modifiers/modifierline.h>



//*****************************************************************************************
//    TModifierLine implementation
//*****************************************************************************************


static inline void calcFixedAngle(const TTrackPoint &p0, TTrackPoint &p1) {
  TPointD p = p1.position - p0.position;
  double l  = sqrt(p.x * p.x + p.y * p.y);
  if (l < TConsts::epsilon) {
    p = TPointD();
  } else {
    double a = atan2(p.y, p.x);
    a        = round(a * 4 / M_PI) * M_PI / 4;
    p.x      = cos(a) * l;
    p.y      = sin(a) * l;
  }
  p1.position = p0.position + p;
}


void TModifierLine::modifyTrack(const TTrack &track,
                                TTrackList &outTracks) {
  if (!track.handler) {
    Handler *handler  = new Handler();
    track.handler     = handler;
    handler->track    = new TTrack(track);
  }

  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler)
    return;
  
  outTracks.push_back(handler->track);
  TTrack &subTrack = *handler->track;
  
  if (!track.changed())
    return;

  subTrack.truncate(0);

  // calc max pressure
  int i1             = track.size();
  int i0             = i1 - track.pointsAdded;
  double maxPressure = handler->maxPressure;
  if (track.pointsRemoved) {
    maxPressure = 0;
    i0          = 0;
  }
  for(int i = i0; i < i1; ++i) {
    double p = track[i].pressure;
    if (maxPressure < p) maxPressure = p;
  }
  handler->maxPressure = maxPressure;
  
  if (track.size() > 0)
    subTrack.push_back(subTrack.pointFromOriginal(0), false);
  
  if (track.size() > 1) {
    TTrackPoint p = subTrack.pointFromOriginal(track.size() - 1);
    if (track.getKeyState(track.back()).isPressed(TKey::control))
      calcFixedAngle(subTrack.front(), p);
    subTrack.push_back(p, false);
  }
    
  if (track.fixedFinished())
    subTrack.fix_all();

  track.resetChanges();
}
