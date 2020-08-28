

#include <tools/modifiers/modifiertest.h>

// std includes
#include <cmath>

//*****************************************************************************************
//    TModifierTest::Modifier implementation
//*****************************************************************************************

TModifierTest::Modifier::Modifier(TTrackHandler &handler, double angle,
                                  double radius, double speed)
    : TTrackModifier(handler), angle(angle), radius(radius), speed(speed) {}

TTrackPoint TModifierTest::Modifier::calcPoint(double originalIndex) {
  TTrackPoint p = TTrackModifier::calcPoint(originalIndex);

  if (p.length > TConsts::epsilon) {
    double frac;
    int i0 = original.floorIndex(originalIndex, &frac);
    int i1 = original.ceilIndex(originalIndex);
    if (i0 < 0) return p;

    if (Handler *handler = dynamic_cast<Handler *>(&this->handler)) {
      double angle =
          this->angle +
          speed * TTrack::interpolationLinear(handler->angles[i0],
                                              handler->angles[i1], frac);
      double radius = 2.0 * this->radius * p.pressure;
      double s      = sin(angle);
      double c      = cos(angle);

      TPointD tangent =
          original.calcTangent(originalIndex, fabs(2.0 * this->radius / speed));
      p.position.x -= tangent.y * s * radius;
      p.position.y += tangent.x * s * radius;
      p.pressure *= 1.0 - 0.5 * c;
    }
  } else {
    p.pressure = 0.0;
  }

  return p;
}

//*****************************************************************************************
//    TModifierTest implementation
//*****************************************************************************************

TModifierTest::TModifierTest(int count, double radius)
    : enabled(), count(count), radius(radius) {}

void TModifierTest::modifyTrack(const TTrack &track,
                                const TInputSavePoint::Holder &savePoint,
                                TTrackList &outTracks) {
  const double segmentSize = 2.0 * M_PI / 10.0;

  if (!track.handler && enabled) {
    if (track.getKeyState(track.front().time).isPressed(TKey(Qt::Key_T))) {
      // TModifierTest::Handler for spiro
      track.handler = new Handler(track);
      for (int i = 0; i < count; ++i)
        track.handler->tracks.push_back(new TTrack(new Modifier(
            *track.handler, i * 2.0 * M_PI / (double)count, radius, 0.25)));
    }
  }

  Handler *handler = dynamic_cast<Handler *>(track.handler.getPointer());
  if (!handler) {
    TInputModifier::modifyTrack(track, savePoint, outTracks);
    return;
  }

  outTracks.insert(outTracks.end(), track.handler->tracks.begin(),
                   track.handler->tracks.end());
  if (!track.changed()) return;

  int start            = track.size() - track.pointsAdded;
  if (start < 0) start = 0;

  // remove angles
  double lastAngle =
      start < (int)handler->angles.size()
          ? handler->angles[start]
          : handler->angles.empty() ? 0.0 : handler->angles.back();
  handler->angles.resize(start, lastAngle);

  // add angles
  for (int i = start; i < track.size(); ++i) {
    if (i > 0) {
      double dl = track[i].length - track[i - 1].length;
      double da = track[i].pressure > TConsts::epsilon
                      ? dl / (radius * track[i].pressure)
                      : 0.0;
      handler->angles.push_back(handler->angles[i - 1] + da);
    } else {
      handler->angles.push_back(0.0);
    }
  }

  // process sub-tracks
  for (TTrackList::const_iterator ti = handler->tracks.begin();
       ti != handler->tracks.end(); ++ti) {
    TTrack &subTrack          = **ti;
    double currentSegmentSize = segmentSize;
    if (const Modifier *modifier =
            dynamic_cast<const Modifier *>(subTrack.modifier.getPointer()))
      if (fabs(modifier->speed) > TConsts::epsilon)
        currentSegmentSize = segmentSize / fabs(modifier->speed);

    // remove points
    int subStart =
        subTrack.floorIndex(subTrack.indexByOriginalIndex(start - 1)) + 1;
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
                subTrack.modifier->calcPoint((double)i - 1.0 + frac));
        }
      }
      subTrack.push_back(subTrack.modifier->calcPoint(i));
    }
  }

  track.resetChanges();
}
