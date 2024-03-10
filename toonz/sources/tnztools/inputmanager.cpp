

#include <tpixelutils.h>
#include <tools/inputmanager.h>

// TnzCore includes
#include <tgl.h>


//*****************************************************************************************
//    static members
//*****************************************************************************************

static const bool debugInputManager = false;
TInputState::TouchId TInputManager::m_lastTouchId = 0;


//*****************************************************************************************
//    TInputModifier implementation
//*****************************************************************************************


void
TInputModifier::setManager(TInputManager *manager) {
  if (m_manager != manager)
    { m_manager = manager; onSetManager(); }
}


void
TInputModifier::modifyTrack(
  const TTrack &track,
  TTrackList &outTracks )
{
  if (!track.handler) {
    TSubTrackHandler *handler = new TSubTrackHandler();
    track.handler = handler;
    handler->track = new TTrack(track);
    new TTrackIntrOrig(*handler->track);
  }

  TSubTrackHandler *handler = dynamic_cast<TSubTrackHandler*>(track.handler.getPointer());
  if (!handler)
    return;
  
  outTracks.push_back(handler->track);
  TTrack &subTrack = *handler->track;

  if (!track.changed())
    return;
  
  int start = std::max(0, track.size() - track.pointsAdded);
  subTrack.truncate(start);
  for(int i = start; i < track.size(); ++i)
    subTrack.push_back(subTrack.pointFromOriginal(i), false);
  subTrack.fix_to(track.fixedSize());
  
  track.resetChanges();
}


void
TInputModifier::modifyTracks(
    const TTrackList &tracks,
    TTrackList &outTracks )
{
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    modifyTrack(**i, outTracks);
}


void
TInputModifier::modifyHover(
  const TPointD &hover,
  THoverList &outHovers )
{
  outHovers.push_back(hover);
}


void
TInputModifier::modifyHovers(
  const THoverList &hovers,
  THoverList &outHovers )
{
  for(THoverList::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    modifyHover(*i, outHovers);
}


TRectD
TInputModifier::calcDrawBounds(const TTrackList &tracks, const THoverList &hovers) {
  TRectD bounds;
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    bounds += calcDrawBoundsTrack(**i);
  for(std::vector<TPointD>::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    bounds += calcDrawBoundsHover(*i);
  return bounds;
}


void
TInputModifier::drawTracks(const TTrackList &tracks) {
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i)
    drawTrack(**i);
}


void
TInputModifier::drawHovers(const std::vector<TPointD> &hovers) {
  for(std::vector<TPointD>::const_iterator i = hovers.begin(); i != hovers.end(); ++i)
    drawHover(*i);
}


void
TInputModifier::draw(const TTrackList &tracks, const std::vector<TPointD> &hovers) {
  drawTracks(tracks);
  drawHovers(hovers);
}


//*****************************************************************************************
//    TInputHandler implementation
//*****************************************************************************************


bool
TInputHandler::inputKeyEvent(
  bool press,
  TInputState::Key,
  QKeyEvent *event,
  const TInputManager& )
{
  return press && event && inputKeyDown(event);
}


void
TInputHandler::inputButtonEvent(
  bool press,
  TInputState::DeviceId,
  TInputState::Button button,
  const TInputManager &manager )
{
  if (press && button == Qt::RightButton && !manager.getOutputHovers().empty())
    inputRightButtonDown(manager.getOutputHovers().front(), manager.state);
}


void
TInputHandler::inputHoverEvent(const TInputManager &manager) {
  if (!manager.getOutputHovers().empty())
    inputMouseMove(manager.getOutputHovers().front(), manager.state);
}


void
TInputHandler::inputPaintTrackPoint(const TTrackPoint &point, const TTrack &track, bool firstTrack, bool preview) {
  if (firstTrack && !preview) {
    if (track.pointsAdded == track.size())
      inputLeftButtonDown(point, track);
    else
    if (point.final)
      inputLeftButtonUp(point, track);
    else
      inputLeftButtonDrag(point, track);
  }
}


void
TInputHandler::inputPaintTracks(const TTrackList &tracks) {
  // prepare tracks counters
  for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
    (*i)->pointsAdded = (*i)->fixedPointsAdded + (*i)->previewSize();
    (*i)->resetRemoved();
  }
  
  // begin paint
  bool preview = false;
  bool paintStarted = false;

  // paint track points in chronological order
  while(true) {
    // find earlier not painted point
    TTrackP track;
    TTimerTicks minTicks = 0;
    double minTimeOffset = 0.0;
    for(TTrackList::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
      const TTrack &t = **i;
      if (t.pointsAdded > 0) {
        TTimerTicks ticks = t.ticks();
        double timeOffset = t.rootTimeOffset + t.current().time;
        if (!track || (ticks - minTicks)*TToolTimer::frequency + timeOffset - minTimeOffset < 0.0) {
          track = *i;
          minTicks = ticks;
          minTimeOffset = timeOffset;
        }
      }
    }
    
    if (!track)
      break; // all tracks are painted
    
    if (track->pointsAdded <= track->previewSize())
      preview = true;
    if (!paintStarted)
      { inputPaintTracksBegin(); paintStarted = true; }
    inputPaintTrackPoint(track->current(), *track, track == tracks.front(), preview);
    
    // update counters
    --track->pointsAdded;
    if (!preview) {
      assert(track->fixedPointsAdded > 0);
      --track->fixedPointsAdded;
    }
  }

  // end paint
  if (paintStarted)
    inputPaintTracksEnd();
}


//*****************************************************************************************
//    TInputManager implementation
//*****************************************************************************************


TInputManager::TInputManager():
  m_lastTicks(TToolTimer::ticks()),
  m_handler(),
  m_tracks(1),
  m_hovers(1),
  m_started(),
  drawPreview()
{ }


void
TInputManager::paintTracks() {
  // run modifiers
  for(int i = 0; i < (int)m_modifiers.size(); ++i) {
    m_tracks[i+1].clear();
    m_modifiers[i]->modifyTracks(m_tracks[i], m_tracks[i+1]);
  }
  TTrackList &subTracks = m_tracks.back();

  
  // begin painting if need
  bool changed = false;
  for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
    if ((*i)->changed())
      { changed = true; break; }
  if (!m_started && changed) {
    m_started = true;
    if (m_handler) m_handler->inputSetBusy(true);
  }

  
  // paint tracks
  if (changed && m_handler)
    m_handler->inputPaintTracks(subTracks);
  
  
  // end painting if all tracks are finished
  bool allFinished = true;
  for(TTrackList::const_iterator i = m_tracks.front().begin(); i != m_tracks.front().end(); ++i)
    if (!(*i)->fixedFinished())
      { allFinished = false; break; }
  if (allFinished)
    for(TTrackList::const_iterator i = subTracks.begin(); i != subTracks.end(); ++i)
      if (!(*i)->fixedFinished())
        { allFinished = false; break; }
        
  if (allFinished) {
    if (m_started) {
      if (m_handler) m_handler->inputSetBusy(false);
      m_started = false;
    }
    for(int i = 0; i < (int)m_tracks.size(); ++i)
      m_tracks[i].clear();
  }
}


int
TInputManager::trackCompare(
  const TTrack &track,
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId ) const
{
  if (track.deviceId < deviceId) return -1;
  if (deviceId < track.deviceId) return  1;
  if (track.touchId < touchId) return -1;
  if (touchId < track.touchId) return 1;
  return 0;
}


const TTrackP&
TInputManager::createTrack(
  int index,
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  TTimerTicks ticks,
  bool hasPressure,
  bool hasTilt )
{
  TTrackP track = new TTrack(
    deviceId,
    touchId,
    state.keyHistoryHolder(ticks),
    state.buttonHistoryHolder(deviceId, ticks),
    hasPressure,
    hasTilt );
  return *m_tracks.front().insert(m_tracks[0].begin() + index, track);
}


const TTrackP&
TInputManager::getTrack(
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  TTimerTicks ticks,
  bool hasPressure,
  bool hasTilt )
{
  TTrackList &origTracks = m_tracks.front();
  if (origTracks.empty())
    return createTrack(0, deviceId, touchId, ticks, hasPressure, hasTilt);
  int cmp;

  int a = 0;
  cmp = trackCompare(*origTracks[a], deviceId, touchId);
  if (cmp == 0) return origTracks[a];
  if (cmp < 0) return createTrack(a, deviceId, touchId, ticks, hasPressure, hasTilt);

  int b = (int)origTracks.size() - 1;
  cmp = trackCompare(*origTracks[b], deviceId, touchId);
  if (cmp == 0) return origTracks[b];
  if (cmp > 0) return createTrack(b+1, deviceId, touchId, ticks, hasPressure, hasTilt);

  // binary search: tracks[a] < tracks[c] < tracks[b]
  while(true) {
    int c = (a + b)/2;
    if (a == c) break;
    cmp = trackCompare(*origTracks[c], deviceId, touchId);
    if (cmp < 0) b = c; else
      if (cmp > 0) a = c; else
        return origTracks[c];
  }
  return createTrack(b, deviceId, touchId, ticks, hasPressure, hasTilt);
}


void
TInputManager::addTrackPoint(
  const TTrackP& track,
  const TPointD &position,
  double pressure,
  const TPointD &tilt,
  double time,
  bool final )
{
  track->push_back(
    TTrackPoint(
      position,
      pressure,
      tilt,
      (double)track->size(),
      time,
      0.0, // length will calculated inside of TTrack::push_back
      final ),
    true );
}


void
TInputManager::touchTracks(bool finish) {
  for(TTrackList::const_iterator i = m_tracks.front().begin(); i != m_tracks.front().end(); ++i) {
    if (!(*i)->finished() && (*i)->size() > 0) {
      const TTrackPoint &p = (*i)->back();
      addTrackPoint(*i, p.position, p.pressure, p.tilt, fixTicks(m_lastTicks)*TToolTimer::step, finish);
    }
  }
}


void
TInputManager::modifierActivate(const TInputModifierP &modifier) {
  modifier->setManager(this);
  modifier->activate();
}


void
TInputManager::modifierDeactivate(const TInputModifierP &modifier) {
  modifier->deactivate();
  modifier->setManager(NULL);
}


void
TInputManager::processTracks() {
  paintTracks();
  if (m_handler) {
    TRectD bounds = calcDrawBounds();
    if (!bounds.isEmpty()) {
      m_handler->inputInvalidateRect(m_prevBounds + bounds);
      m_nextBounds += bounds;
    }
  }
}


void
TInputManager::finishTracks() {
  touchTracks(true);
  processTracks();
}


void
TInputManager::reset() {
  // forget about handler busy state
  // assuime it was already reset by outside
  m_started = false;

  // reset tracks
  for(int i = 0; i < (int)m_tracks.size(); ++i)
    m_tracks[i].clear();
}


void
TInputManager::setHandler(TInputHandler *handler) {
  if (m_handler == handler) return;
  finishTracks();
  m_handler = handler;
}


int
TInputManager::findModifier(const TInputModifierP &modifier) const {
  for(int i = 0; i < getModifiersCount(); ++i)
    if (getModifier(i) == modifier)
      return i;
  return -1;
}


void
TInputManager::insertModifier(int index, const TInputModifierP &modifier) {
  if (findModifier(modifier) >= 0) return;
  finishTracks();
  m_modifiers.insert(m_modifiers.begin() + index, modifier);
  m_tracks.insert(m_tracks.begin() + index + 1, TTrackList());
  m_hovers.insert(m_hovers.begin() + index + 1, THoverList());
  modifierActivate(modifier);
}


void
TInputManager::removeModifier(int index) {
  if (index >= 0 && index < getModifiersCount()) {
    finishTracks();
    modifierDeactivate(m_modifiers[index]);
    m_modifiers.erase(m_modifiers.begin() + index);
    m_tracks.erase(m_tracks.begin() + index + 1);
    m_hovers.erase(m_hovers.begin() + index + 1);
  }
}


void
TInputManager::clearModifiers() {
  while(getModifiersCount() > 0)
    removeModifier(getModifiersCount() - 1);
}


void
TInputManager::trackEvent(
  TInputState::DeviceId deviceId,
  TInputState::TouchId touchId,
  const TPointD &position,
  const double pressure,
  const TPointD &tilt,
  bool hasPressure,
  bool hasTilt,
  bool final,
  TTimerTicks ticks )
{
  ticks = fixTicks(ticks);
  TTrackP track = getTrack(deviceId, touchId, ticks, hasPressure, hasTilt);
  if (!track->finished()) {
    ticks = fixTicks(ticks);
    double time = (double)(ticks - track->ticks())*TToolTimer::step - track->rootTimeOffset;
    addTrackPoint(
      track,
      position,
      pressure,
      tilt,
      time,
      final );
  }
}


bool
TInputManager::keyEvent(
  bool press,
  TInputState::Key key,
  TTimerTicks ticks,
  QKeyEvent *event )
{
  bool wasPressed = state.isKeyPressed(key);
  ticks = fixTicks(ticks);
  state.keyEvent(press, key, ticks);
  processTracks();
  bool result = m_handler && m_handler->inputKeyEvent(press, key, event, *this);
  if (wasPressed != press) {
    touchTracks();
    processTracks();
    //hoverEvent(getInputHovers());
  }
  return result;
}


void
TInputManager::buttonEvent(
  bool press,
  TInputState::DeviceId deviceId,
  TInputState::Button button,
  TTimerTicks ticks )
{
  bool wasPressed = state.isButtonPressed(deviceId, button);
  ticks = fixTicks(ticks);
  state.buttonEvent(press, deviceId, button, ticks);
  processTracks();
  if (m_handler) m_handler->inputButtonEvent(press, deviceId, button, *this);
  if (wasPressed != press) {
    touchTracks();
    processTracks();
    //hoverEvent(getInputHovers());
  }
}


void
TInputManager::hoverEvent(const THoverList &hovers) {
  if (&m_hovers[0] != &hovers)
    m_hovers[0] = hovers;
  for(int i = 0; i < (int)m_modifiers.size(); ++i) {
    m_hovers[i+1].clear();
    m_modifiers[i]->modifyHovers(m_hovers[i], m_hovers[i+1]);
  }
  if (m_handler) {
    TRectD bounds = calcDrawBounds();
    if (!bounds.isEmpty()) {
      m_handler->inputInvalidateRect(m_prevBounds + bounds);
      m_nextBounds += bounds;
    }
    m_handler->inputHoverEvent(*this);
  }
}


TRectD
TInputManager::calcDrawBounds() {
  if (debugInputManager)
    return TConsts::infiniteRectD;

  TRectD bounds;
  for(int i = 0; i < (int)m_modifiers.size(); ++i)
    bounds += m_modifiers[i]->calcDrawBounds(m_tracks[i], m_hovers[i]);

  if (drawPreview) {
    for(TTrackList::const_iterator ti = getOutputTracks().begin(); ti != getOutputTracks().end(); ++ti) {
      TTrack &track = **ti;
      int start = std::max(0, track.fixedSize() - track.fixedPointsAdded);
      if (start + 1 < track.size())
        for(int i = start + 1; i < track.size(); ++i)
          bounds += boundingBox(track[i-1].position, track[i].position);
    }
  }

  if (!bounds.isEmpty())
    bounds.enlarge(4.0);
  
  return bounds;
}


void
TInputManager::draw() {
  m_prevBounds = m_nextBounds;
  m_nextBounds = TRectD();
  
  // paint not fixed parts of tracks
  if (drawPreview) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    tglEnableBlending();
    tglEnableLineSmooth(true, 1.0);
    double pixelSize = sqrt(tglGetPixelSize2());
    double colorBlack[4] = { 0.0, 0.0, 0.0, 0.5 };
    double colorWhite[4] = { 1.0, 1.0, 1.0, 0.5 };
    for(TTrackList::const_iterator ti = getOutputTracks().begin(); ti != getOutputTracks().end(); ++ti) {
      TTrack &track = **ti;
      int start = std::max(0, track.fixedSize() - track.fixedPointsAdded);
      const TPointD *a = &track[start].position;
      for(int i = start + 1; i < track.size(); ++i) {
        const TPointD *b = &track[i].position;
        TPointD d = *b - *a;
        double k = norm2(d);
        if (k > TConsts::epsilon*TConsts::epsilon) {
          k = 0.5*pixelSize/sqrt(k);
          d = TPointD(-k*d.y, k*d.x);
          glColor4dv(colorWhite);
          tglDrawSegment(*a - d, *b - d);
          glColor4dv(colorBlack);
          tglDrawSegment(*a + d, *b + d);
          a = b;
        }
      }
    }
    glPopAttrib();
  }
  
  // paint all tracks modifications for debug
  if (debugInputManager) {
    double pixelSize = sqrt(tglGetPixelSize2());
    double color[4] = { 0.0, 0.0, 0.0, 0.5 };
    int cnt = m_tracks.size();
    for(int li = 0; li < cnt; ++li) {
      HSV2RGB(240 + li*120.0/(cnt-1), 1, 1, &color[0], &color[1], &color[2]);
      glColor4dv(color);
      for(TTrackList::const_iterator ti = m_tracks[li].begin(); ti != m_tracks[li].end(); ++ti) {
        assert(*ti);
        const TTrack &track = **ti;
        int radius = 0;
        const TPointD *a = &track[0].position;
        for(int i = 0; i < track.size(); ++i) {
          const TPointD *b = &track[i].position;
          
          if (i) {
            TPointD d = *b - *a;
            double k = norm2(d);
            if (k > 4*pixelSize*pixelSize) {
              tglDrawSegment(*a, *b);
              radius = 0;
              a = b;
            } else {
              ++radius;
            }
          }
          
          if (radius && li == 0)
            tglDrawCircle(*b, radius*pixelSize*3);
        }
      }
    }
  }

  // paint modifiers
  for(int i = 0; i < (int)m_modifiers.size(); ++i)
    m_modifiers[i]->draw(m_tracks[i], m_hovers[i]);
}

TInputState::TouchId
TInputManager::genTouchId()
  { return ++m_lastTouchId; }
