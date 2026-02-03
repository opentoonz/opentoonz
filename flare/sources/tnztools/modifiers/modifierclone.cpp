
#include <tools/modifiers/modifierclone.h>


//*****************************************************************************************
//    TModifierClone::Interpolator implementation
//*****************************************************************************************

TTrackPoint TModifierClone::Interpolator::interpolateFromOriginal(double originalIndex)
  { return transform.apply( track.calcPointFromOriginal(originalIndex) ); }
TTrackPoint TModifierClone::Interpolator::interpolate(double index)
  { return interpolateFromOriginal(track.originalIndexByIndex(index)); }


//*****************************************************************************************
//    TModifierClone implementation
//*****************************************************************************************

TModifierClone::TModifierClone(bool keepOriginals, int skipFirst, int skipLast):
  keepOriginals(keepOriginals), skipFirst(skipFirst), skipLast(skipLast) { }

void TModifierClone::modifyTrack(const TTrack &track,
                                 TTrackList &outTracks)
{
  if (!track.handler) {
    Handler *handler = new Handler();
    track.handler = handler;
    if (keepOriginals) {
      handler->original = new TTrack(track);
      new TTrackIntrOrig(*handler->original);
    }
    for(TTrackTransformList::const_iterator i = transforms.begin(); i != transforms.end(); ++i) {
      handler->tracks.push_back(new TTrack(track));
      TTrack &subTrack = *handler->tracks.back();
      new Interpolator(subTrack, *i);
    }
  }
  
  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler)
    return;
  
  if (handler->original)
    outTracks.push_back(handler->original);
  outTracks.insert(outTracks.end(), handler->tracks.begin(), handler->tracks.end());
  if (!track.changed())
    return;

  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;

  // process original
  if (handler->original) {
    TTrack &subTrack = *handler->original;
    subTrack.truncate(start);
    for (int i = start; i < track.size(); ++i)
      subTrack.push_back(subTrack.pointFromOriginal(i), false);
    subTrack.fix_to(track.fixedSize());
  }
  
  // process sub-tracks
  for(TTrackList::iterator i = handler->tracks.begin(); i != handler->tracks.end(); ++i) {
    TTrack &subTrack = **i;
    Interpolator *intr = dynamic_cast<Interpolator*>(subTrack.getInterpolator().getPointer());
    if (!intr)
      continue;
    subTrack.truncate(start);
    for (int i = start; i < track.size(); ++i)
      subTrack.push_back(intr->interpolateFromOriginal(i), false);
    subTrack.fix_to(track.fixedSize());
  }
  
  track.resetChanges();
}

void TModifierClone::modifyTracks(
  const TTrackList &tracks,
  TTrackList &outTracks )
{
  int cnt = (int)tracks.size();
  int i0 = skipFirst;
  int i1 = cnt - skipLast;
  for(int i = 0; i < cnt; ++i)
    if (i0 <= i && i < i1) modifyTrack(*tracks[i], outTracks);
      else TInputModifier::modifyTrack(*tracks[i], outTracks);
}

