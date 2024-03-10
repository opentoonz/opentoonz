
#ifndef NDEBUG

#include <tools/modifiers/modifiertest.h>

// std includes
#include <cmath>


//*****************************************************************************************
//    TModifierTest::Interpolator implementation
//*****************************************************************************************

TTrackPoint TModifierTest::Interpolator::interpolateFromOriginal(double originalIndex) {
  Handler *handler = track.original
                   ? dynamic_cast<Handler*>(track.original->handler.getPointer())
                   : nullptr;
  if (!handler)
    return track.interpolateLinear(track.indexByOriginalIndex(originalIndex));
  
  TTrackPoint p = track.calcPointFromOriginal(originalIndex);

  double frac;
  int i0 = track.original->floorIndex(originalIndex, &frac);
  int i1 = track.original->ceilIndex(originalIndex);

  double angle = TTrack::interpolationLinear(
    handler->angles[i0], handler->angles[i1], frac );
  double a = angle*speed + this->angle;

  double kr = 1 - 1/(angle + 1);
  double s = sin(a);
  double r = radius*p.pressure*kr*s;
  
  double d = fabs(2.0*radius);
  if (fabs(speed) > TConsts::epsilon)
    d /= fabs(speed);

  TPointD tangent = track.original->calcTangent(originalIndex, d);
  p.position.x -= tangent.y * r;
  p.position.y += tangent.x * r;
  p.pressure *= fabs(s);

  return p;
}


TTrackPoint TModifierTest::Interpolator::interpolate(double index) {
  return interpolateFromOriginal(track.originalIndexByIndex(index));
}


//*****************************************************************************************
//    TModifierTest implementation
//*****************************************************************************************

TModifierTest::TModifierTest(int count, double radius, double speed)
    : count(count), radius(radius), speed(speed) {}

void TModifierTest::modifyTrack(const TTrack &track,
                                TTrackList &outTracks) {
  const double segmentSize = 2.0 * M_PI / 10.0;

  if ( !track.handler
    && track.getKeyState(track.front().time).isPressed(TKey::alt) )
  {
    Handler *handler = new Handler(this->radius);
    track.handler = handler;
    for (int i = 0; i < count; ++i) {
      handler->tracks.push_back(new TTrack(track));
      TTrack &subTrack = *handler->tracks.back();
      new Interpolator(subTrack, i*2*M_PI/(double)count, radius, 0.25);
    }
  }
  
  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler)
    return TInputModifier::modifyTrack(track, outTracks);
  
  outTracks.insert(outTracks.end(), handler->tracks.begin(), handler->tracks.end());
  if (!track.changed())
    return;

  double radius = handler->radius;
  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;

  // remove angles
  handler->angles.resize(start);
  
  // add angles
  for(int i = start; i < track.size(); ++i) {
    if (i) {
      const TTrackPoint &p0 = track[i - 1];
      const TTrackPoint &p1 = track[i];
      double dl = p1.length - p0.length;
      double da = p1.pressure > TConsts::epsilon
                ? dl / (radius * p1.pressure)
                : 0.0;
      handler->angles.push_back(handler->angles.back() + da);
    } else {
      handler->angles.push_back(0.0);
    }
  }
  
  // process sub-tracks
  for(TTrackList::const_iterator ti = handler->tracks.begin(); ti != handler->tracks.end(); ++ti) {
    TTrack &subTrack = **ti;
    Interpolator *intr = dynamic_cast<Interpolator*>(subTrack.getInterpolator().getPointer());
    if (!intr)
      continue;
    
    double currentSegmentSize = segmentSize;
    if (fabs(intr->speed) > TConsts::epsilon)
      currentSegmentSize /= fabs(intr->speed);

    // remove points
    int subStart = start > 0
                 ? subTrack.floorIndex(subTrack.indexByOriginalIndex(start - 1)) + 1
                 : 0;
    subTrack.truncate(subStart);

    // add points
    for (int i = start; i < track.size(); ++i) {
      if (i > 0) {
        double prevAngle = handler->angles[i - 1];
        double nextAngle = handler->angles[i];
        if (fabs(nextAngle - prevAngle) > 1.5 * currentSegmentSize) {
          double step = currentSegmentSize / fabs(nextAngle - prevAngle);
          double end  = 1.0 - 0.5 * step;
          for (double frac = step; frac < end; frac += step)
            subTrack.push_back(
                intr->interpolateFromOriginal(i - 1 + frac), false );
        }
      }
      subTrack.push_back(intr->interpolateFromOriginal(i), false);
    }
    
    // fix points
    if (track.fixedFinished())
      subTrack.fix_all();
    if (track.fixedSize())
      subTrack.fix_to(
        subTrack.floorIndex(subTrack.indexByOriginalIndex(track.fixedSize() - 1)) + 1 );
  }
  
  track.resetChanges();
}

#endif
