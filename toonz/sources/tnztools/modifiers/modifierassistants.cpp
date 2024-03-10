

#include <tools/modifiers/modifierassistants.h>

// TnzTools includes
#include <tools/tool.h>

// TnzLib includes
#include <toonz/tapplication.h>
#include <toonz/txshlevelhandle.h>
#include <toonz/txsheethandle.h>
#include <toonz/txsheet.h>
#include <toonz/tframehandle.h>
#include <toonz/tobjecthandle.h>
#include <toonz/dpiscale.h>

// TnzCore includes
#include <tgl.h>
#include <tmetaimage.h>


//*****************************************************************************************
//    TModifierAssistants::Interpolator implementation
//*****************************************************************************************


TTrackPoint
TModifierAssistants::Interpolator::interpolate(double index) {
  TTrackPoint p = track.original ? track.calcPointFromOriginal(index)
                                 : track.interpolateLinear(index);
  return guidelines.empty() ? p : guidelines.front()->smoothTransformPoint(p, magnetism);
}


//*****************************************************************************************
//    TModifierAssistants implementation
//*****************************************************************************************


TModifierAssistants::TModifierAssistants(double magnetism):
  magnetism(magnetism),
  sensitiveLength(50.0) { }


bool
TModifierAssistants::scanAssistants(
  const TPointD *positions,
  int positionsCount,
  TGuidelineList *outGuidelines,
  bool draw,
  bool enabledOnly,
  bool drawGuidelines ) const
{
  if (TInputManager *manager = getManager())
  if (TInputHandler *handler = manager->getHandler())
    return TAssistant::scanAssistants(
      handler->inputGetTool(),
      positions,
      positionsCount,          
      outGuidelines,
      draw,
      enabledOnly,
      magnetism > 0,
      drawGuidelines,
      nullptr );
  return false;
}


void
TModifierAssistants::modifyTrack(
  const TTrack &track,
  TTrackList &outTracks )
{
  if (!track.handler) {
    Handler *handler = new Handler();
    track.handler = handler;
    handler->track = new TTrack(track);
    new Interpolator(*handler->track, magnetism);
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
  
  // remove points
  int start = track.size() - track.pointsAdded;
  if (start < 0) start = 0;

  if (intr->magnetism && start <= 0) {
    intr->guidelines.clear();
    scanAssistants(&track[0].position, 1, &intr->guidelines, false, true, false);
  }
  
  bool fixed = subTrack.fixedSize() || intr->guidelines.size() <= 1;

  // select guideline
  if (!fixed)
  if (TInputManager *manager = getManager())
  if (TInputHandler *handler = manager->getHandler())
  if (TTool *tool = handler->inputGetTool())
  if (TToolViewer *viewer = tool->getViewer()) {
    TAffine trackToScreen = tool->getMatrix();
    if (tool->getToolType() & TTool::LevelTool)
      if (TObjectHandle *objHandle = TTool::getApplication()->getCurrentObject())
        if (!objHandle->isSpline())
          trackToScreen *= TScale(viewer->getDpiScale().x, viewer->getDpiScale().y);
    trackToScreen *= viewer->get3dViewMatrix().get2d();
    TGuidelineP guideline = TGuideline::findBest(intr->guidelines, track, trackToScreen, fixed);
    if (guideline != intr->guidelines.front())
      for(int i = 1; i < (int)intr->guidelines.size(); ++i)
        if (intr->guidelines[i] == guideline) {
          std::swap(intr->guidelines[i], intr->guidelines.front());
          start = 0;
          break;
        }
  }

  // add points
  subTrack.truncate(start);
  for(int i = start; i < track.size(); ++i)
    subTrack.push_back( intr->interpolate(i), false );
  
  // fix points
  if (fixed || track.fixedFinished())
    subTrack.fix_to(track.fixedSize());

  track.resetChanges();
}


TRectD
TModifierAssistants::calcDrawBounds(const TTrackList&, const THoverList&) {
  if (scanAssistants(NULL, 0, NULL, false, false, false))
    return TConsts::infiniteRectD;
  return TRectD();
}


void
TModifierAssistants::drawTrack(const TTrack &track) {
  Handler *handler = dynamic_cast<Handler*>(track.handler.getPointer());
  if (!handler) return;
  
  TTrack &subTrack = *handler->track;
  Interpolator *intr = dynamic_cast<Interpolator*>(subTrack.getInterpolator().getPointer());
  if (!intr) return;
  
  const TGuidelineList &guidelines = intr->guidelines;
  if (guidelines.empty())
    return;
  
  guidelines.front()->draw(true);
  for(TGuidelineList::const_iterator i = guidelines.begin() + 1; i != guidelines.end(); ++i)
    (*i)->draw();
}


void
TModifierAssistants::draw(const TTrackList &tracks, const THoverList &hovers) {
  THoverList allHovers;
  allHovers.reserve(hovers.size() + tracks.size());
  if (tracks.empty()) // hide hovers if track exists
    allHovers.insert(allHovers.end(), hovers.begin(), hovers.end());
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    if (Handler *handler = dynamic_cast<Handler*>((*i)->handler.getPointer()))
      allHovers.push_back( handler->track->back().position );
  
  // draw assistants and guidelines
  scanAssistants(
    allHovers.empty() ? NULL : &allHovers.front(),
    (int)allHovers.size(),
    nullptr,
    true,
    false,
    true );

  // draw tracks
  TInputModifier::drawTracks(tracks);
}
