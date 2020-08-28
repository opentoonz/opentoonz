

#include <tools/modifiers/modifiertangents.h>

//*****************************************************************************************
//    TModifierTangents::Modifier implementation
//*****************************************************************************************

TTrackPoint TModifierTangents::Modifier::calcPoint(double originalIndex) {
  double frac;
  int i0 = original.floorIndex(originalIndex, &frac);
  int i1 = original.ceilIndex(originalIndex);

  TTrackPoint p;
  if (i0 >= 0) {
    // calculate tangent length to make monotonic subdivisions,
    // (because we don't have valid input time)
    const TTrackPoint &p0 = original[i0];
    const TTrackPoint &p1 = original[i1];
    TTrackTangent t0 =
        i0 < (int)tangents.size() ? tangents[i0] : TTrackTangent();
    TTrackTangent t1 =
        i1 < (int)tangents.size() ? tangents[i1] : TTrackTangent();
    double l    = fabs(p1.length - p0.length);
    t0.position = t0.position * l;
    t1.position = t1.position * l;
    p           = TTrack::interpolationSpline(p0, p1, t0, t1, frac);
  }
  p.originalIndex = originalIndex;
  return p;
}

//*****************************************************************************************
//    TModifierTangents implementation
//*****************************************************************************************

TTrackTangent TModifierTangents::calcLastTangent(const TTrack &track) const {
  if (track.size() < 2) return TTrackTangent();
  const TTrackPoint &p0 = track[track.size() - 2];
  const TTrackPoint &p1 = track.back();

  // calculate tangent (with normalized position)
  // with valid points time normalization does not required
  double dl = p1.length - p0.length;
  return TTrackTangent(dl > TConsts::epsilon
                           ? (p1.position - p0.position) * (1.0 / dl)
                           : TPointD(),
                       p1.pressure - p0.pressure, p1.tilt - p0.tilt);
}

void TModifierTangents::modifyTrack(const TTrack &track,
                                    const TInputSavePoint::Holder &savePoint,
                                    TTrackList &outTracks) {
  if (!track.handler) {
    track.handler = new TTrackHandler(track);
    track.handler->tracks.push_back(new TTrack(new Modifier(*track.handler)));
  }

  if (track.handler->tracks.empty()) return;

  TTrack &subTrack   = *track.handler->tracks.front();
  Modifier *modifier = dynamic_cast<Modifier *>(subTrack.modifier.getPointer());
  if (!modifier) return;

  outTracks.push_back(track.handler->tracks.front());

  if (!track.changed() && track.size() == subTrack.size() &&
      track.size() == (int)modifier->tangents.size())
    return;

  if (!track.changed() && subTrack.size() == track.size() - 1) {
    // add temporary point
    modifier->tangents.push_back(calcLastTangent(track));
    subTrack.push_back(track.back());
  } else {
    // apply permanent changes

    // remove points
    int start            = track.size() - track.pointsAdded;
    if (start < 0) start = 0;
    if (start > 1) --start;
    subTrack.truncate(start);
    TTrackTangent lastTangent = start < (int)modifier->tangents.size()
                                    ? modifier->tangents[start]
                                    : modifier->tangents.empty()
                                          ? TTrackTangent()
                                          : modifier->tangents.back();
    modifier->tangents.resize(start, lastTangent);

    // add first point
    int index = start;
    if (index == 0) {
      modifier->tangents.push_back(TTrackTangent());
      subTrack.push_back(track.back());
      ++index;
    }

    // add points with tangents
    if (track.size() > 2) {
      while (index < track.size() - 1) {
        const TTrackPoint &p0 = track[index - 1];
        const TTrackPoint &p1 = track[index];
        const TTrackPoint &p2 = track[index + 1];

        // calculate tangents length by time
        // for that we need read time of user input
        // instead of time when message dispatched
        // double dt = p2.time - p0.time;
        // double k = dt > TConsts::epsilon ? (p1.time - p0.time)/dt : 0.0;
        // TTrackTangent tangent(
        //  (p2.position - p0.position)*k,
        //  (p2.pressure - p0.pressure)*k,
        //  (p2.tilt - p0.tilt)*k
        //  (p2.worldPosition - p0.worldPosition)*k,
        //  (p2.screenPosition - p0.screenPosition)*k );

        // calculate tangent (with normalized position)
        TPointD d = p2.position - p0.position;
        double k  = norm2(d);
        k = k > TConsts::epsilon * TConsts::epsilon ? 1.0 / sqrt(k) : 0.0;
        TTrackTangent tangent(d * k, (p2.pressure - p0.pressure) * 0.5,
                              (p2.tilt - p0.tilt) * 0.5,
                              (p2.worldPosition - p0.worldPosition) * 0.5,
                              (p2.screenPosition - p0.screenPosition) * 0.5);

        modifier->tangents.push_back(tangent);
        subTrack.push_back(p1);
        ++index;
      }
    }

    track.resetChanges();

    // release previous key point
    modifier->savePoint.reset();

    if (track.finished()) {
      // finish
      modifier->tangents.push_back(calcLastTangent(track));
      subTrack.push_back(track.back());
    } else {
      // save key point
      modifier->savePoint = savePoint;
    }
  }
}
