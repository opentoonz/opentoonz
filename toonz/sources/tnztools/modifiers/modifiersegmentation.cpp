

#include <tools/modifiers/modifiersegmentation.h>
#include <algorithm>


//*****************************************************************************************
//    TModifierSegmentation implementation
//*****************************************************************************************


TModifierSegmentation::TModifierSegmentation(const TPointD &step, int maxLevel):
  m_maxLevel(0)
  { setStep(step); setMaxLevel(maxLevel); }


void
TModifierSegmentation::setStep(const TPointD &step) {
  m_step.x = std::max(TConsts::epsilon, fabs(step.x));
  m_step.y = std::max(TConsts::epsilon, fabs(step.y));
}


void
TModifierSegmentation::setMaxLevel(int maxLevel) {
  m_maxLevel = std::max(0, maxLevel);
}


void
TModifierSegmentation::addSegments(
  TTrack &track,
  const TTrackPoint &p0,
  const TTrackPoint &p1,
  int level)
{
  TPointD d = p1.position - p0.position;

  if (level <= 0 || (fabs(d.x) <= m_step.x && fabs(d.y) <= m_step.y)) {
    track.push_back(p1, false);
    return;
  }

  --level;
  TTrackPoint p = track.calcPointFromOriginal(0.5*(p0.originalIndex + p1.originalIndex));
  addSegments(track, p0, p, level);
  addSegments(track, p, p1, level);
}


void
TModifierSegmentation::modifyTrack(
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
  int subStart = start > 0 ? subTrack.floorIndex(subTrack.indexByOriginalIndex(start-1)) + 1 : 0;
  subTrack.truncate(subStart);

  // add points
  TTrackPoint p0 = subTrack.pointFromOriginal(start - 1);
  for(int i = start; i < track.size(); ++i) {
    TTrackPoint p1 = subTrack.pointFromOriginal(i);
    addSegments(subTrack, p0, p1, m_maxLevel);
    p0 = p1;
  }

  // fix points
  if (track.fixedSize())
    subTrack.fix_to(
      subTrack.floorIndex(subTrack.indexByOriginalIndex(track.fixedSize() - 1)) + 1 );
  
  track.resetChanges();
}
