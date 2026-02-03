#pragma once

#ifndef INPUTMANAGER_INCLUDED
#define INPUTMANAGER_INCLUDED

// TnzTools includes
#include <tools/tooltimer.h>
#include <tools/inputstate.h>
#include <tools/track.h>

// TnzCore includes
#include <tcommon.h>
#include <tgeometry.h>
#include <tsmartpointer.h>

// Qt includes
#include <QObject>
#include <QKeyEvent>

// std includes
#include <vector>
#include <algorithm>


#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif


//====================================================

//  Forward declarations

class TTool;
class TInputModifier;
class TInputManager;

typedef TSmartPointerT<TInputModifier> TInputModifierP;

//===================================================================


//*****************************************************************************************
//    TInputModifier definition
//*****************************************************************************************

class DVAPI TInputModifier: public TSmartObject {
private:
  TInputManager *m_manager;

public:
  typedef std::vector<TInputModifierP> List;

  TInputManager* getManager() const
    { return m_manager; }
  void setManager(TInputManager *manager);
  virtual void onSetManager() { }

  virtual void activate() { }

  virtual void modifyTrack(
    const TTrack &track,
    TTrackList &outTracks );
  virtual void modifyTracks(
    const TTrackList &tracks,
    TTrackList &outTracks );

  virtual void modifyHover(
    const TPointD &hover,
    THoverList &outHovers );
  virtual void modifyHovers(
    const THoverList &hovers,
    THoverList &outHovers );

  virtual TRectD calcDrawBoundsHover(const TPointD &hover) { return TRectD(); }
  virtual TRectD calcDrawBoundsTrack(const TTrack &track) { return TRectD(); }
  virtual TRectD calcDrawBounds(const TTrackList &tracks, const THoverList &hovers);

  virtual void drawTrack(const TTrack &track) { }
  virtual void drawHover(const TPointD &hover) { }
  virtual void drawTracks(const TTrackList &tracks);
  virtual void drawHovers(const THoverList &hovers);
  virtual void draw(const TTrackList &tracks, const THoverList &hovers);

  virtual void deactivate() { }
};


//*****************************************************************************************
//    TInputHandler definition
//*****************************************************************************************

class TInputHandler {
public:
  virtual void inputLeftButtonDown(const TTrackPoint&, const TTrack&) { }
  virtual void inputLeftButtonDrag(const TTrackPoint&, const TTrack&) { }
  virtual void inputLeftButtonUp(const TTrackPoint&, const TTrack&) { }
  virtual void inputMouseMove(const TPointD&, const TInputState&) { }
  virtual void inputRightButtonDown(const TPointD&, const TInputState&) { }
  virtual bool inputKeyDown(QKeyEvent *) { return false; }

  virtual void inputSetBusy(bool) { }
  
  virtual bool inputKeyEvent(
    bool press,
    TInputState::Key key,
    QKeyEvent *event,
    const TInputManager &manager );
  
  virtual void inputButtonEvent(
    bool press,
    TInputState::DeviceId device,
    TInputState::Button button,
    const TInputManager &manager );
  
  virtual void inputHoverEvent(const TInputManager &manager);

  virtual void inputPaintTracksBegin() { }
  virtual void inputPaintTrackPoint(const TTrackPoint &point, const TTrack &track, bool firstTrack, bool preview);
  virtual void inputPaintTracksEnd() { }
  virtual void inputPaintTracks(const TTrackList &tracks);

  virtual void inputInvalidateRect(const TRectD &bounds) { }
  
  virtual TTool* inputGetTool() { return nullptr; };
};


//*****************************************************************************************
//    TInputManager definition
//*****************************************************************************************

class DVAPI TInputManager {
private:
  TTimerTicks m_lastTicks;
  TInputHandler *m_handler;
  TInputModifier::List m_modifiers;
  std::vector<TTrackList> m_tracks;
  std::vector<THoverList> m_hovers;
  TRectD m_prevBounds;
  TRectD m_nextBounds;
  bool m_started;

  static TInputState::TouchId m_lastTouchId;


public:
  TInputState state;
  bool drawPreview;


public:
  TInputManager();

private:
  inline TTimerTicks fixTicks(TTimerTicks ticks) {
    if (ticks <= m_lastTicks) ticks = m_lastTicks + 1;
    return m_lastTicks = ticks;
  }
  
  void paintTracks();

  int trackCompare(
    const TTrack &track,
    TInputState::DeviceId deviceId,
    TInputState::TouchId touchId ) const;
  const TTrackP& createTrack(
    int index,
    TInputState::DeviceId deviceId,
    TInputState::TouchId touchId,
    TTimerTicks ticks,
    bool hasPressure,
    bool hasTilt );
  const TTrackP& getTrack(
    TInputState::DeviceId deviceId,
    TInputState::TouchId touchId,
    TTimerTicks ticks,
    bool hasPressure,
    bool hasTilt );
  void addTrackPoint(
    const TTrackP& track,
    const TPointD &position,
    double pressure,
    const TPointD &tilt,
    double time,
    bool final );
  void touchTracks(bool finish = false);

  void modifierActivate(const TInputModifierP &modifier);
  void modifierDeactivate(const TInputModifierP &modifier);

public:
  inline const TTrackList& getInputTracks() const
    { return m_tracks.front(); }
  inline const TTrackList& getOutputTracks() const
    { return m_tracks.back(); }

  inline const THoverList& getInputHovers() const
    { return m_hovers.front(); }
  inline const THoverList& getOutputHovers() const
    { return m_hovers.back(); }

  void processTracks();
  void finishTracks();
  void reset();

  TInputHandler* getHandler() const
    { return m_handler; }
  void setHandler(TInputHandler *handler);

  int getModifiersCount() const
    { return (int)m_modifiers.size(); }
  const TInputModifierP& getModifier(int index) const
    { return m_modifiers[index]; }
  int findModifier(const TInputModifierP &modifier) const;
  void insertModifier(int index, const TInputModifierP &modifier);
  void addModifier(const TInputModifierP &modifier)
    { insertModifier(getModifiersCount(), modifier); }
  void addModifiers(const TInputModifier::List &modifiers) {
    for(TInputModifier::List::const_iterator i = modifiers.begin(); i != modifiers.end(); ++i)
      addModifier(*i);
  }
  void removeModifier(int index);
  void removeModifier(const TInputModifierP &modifier)
    { removeModifier(findModifier(modifier)); }
  void clearModifiers();

  void trackEvent(
    TInputState::DeviceId deviceId,
    TInputState::TouchId touchId,
    const TPointD &position,
    const double pressure,
    const TPointD &tilt,
    bool hasPressure,
    bool hasTilt,
    bool final,
    TTimerTicks ticks );
  bool keyEvent(
    bool press,
    TInputState::Key key,
    TTimerTicks ticks,
    QKeyEvent *event );
  void buttonEvent(
    bool press,
    TInputState::DeviceId deviceId,
    TInputState::Button button,
    TTimerTicks ticks);
  void hoverEvent(const THoverList &hovers);

  TRectD calcDrawBounds();
  void draw();

  static TInputState::TouchId genTouchId();
};


//*****************************************************************************************
//    export template implementations for win32
//*****************************************************************************************

#ifdef _WIN32
template class DVAPI TSmartPointerT<TInputModifier>;
#endif


#endif
