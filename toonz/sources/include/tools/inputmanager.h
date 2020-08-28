#pragma once

#ifndef INPUTMANAGER_INCLUDED
#define INPUTMANAGER_INCLUDED

// TnzTools includes
#include <tools/tooltimer.h>
#include <tools/inputstate.h>
#include <tools/track.h>

// TnzCore includes
#include <tcommon.h>
#include <tsmartpointer.h>
#include <tgeometry.h>

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

class TApplication;
class TTool;
class TToolViewer;
class TInputModifier;
class TInputManager;

typedef TSmartPointerT<TInputModifier> TInputModifierP;

//===================================================================

//*****************************************************************************************
//    TInputSavePoint definition
//*****************************************************************************************

class DVAPI TInputSavePoint {
public:
  class DVAPI Holder {
  private:
    TInputSavePoint *m_savePoint;
    bool m_lock = false;

  public:
    inline explicit Holder(TInputSavePoint *savePoint = NULL, bool lock = true)
        : m_savePoint(), m_lock() {
      set(savePoint, lock);
    }
    inline Holder(const Holder &other) : m_savePoint(), m_lock() {
      *this = other;
    }
    inline ~Holder() { reset(); }

    inline Holder &operator=(const Holder &other) {
      set(other.m_savePoint, other.m_lock);
      return *this;
    }

    inline operator bool() const { return assigned(); }

    inline void set(TInputSavePoint *savePoint, bool lock) {
      if (m_savePoint != savePoint) {
        if (m_savePoint) {
          if (m_lock) m_savePoint->unlock();
          m_savePoint->release();
        }
        m_savePoint = savePoint;
        m_lock      = lock;
        if (m_savePoint) {
          m_savePoint->hold();
          if (m_lock) savePoint->lock();
        }
      } else if (m_lock != lock) {
        if (m_savePoint) {
          if (lock)
            m_savePoint->lock();
          else
            m_savePoint->unlock();
        }
        m_lock = lock;
      }
    }

    inline void reset() { set(NULL, false); }
    inline void setLock(bool lock) { set(m_savePoint, lock); }
    inline void lock() { setLock(true); }
    inline void unlock() { setLock(false); }

    inline TInputSavePoint *savePoint() const { return m_savePoint; }
    inline bool assigned() const { return savePoint(); }
    inline bool locked() const { return m_savePoint && m_lock; }
    inline bool available() const {
      return m_savePoint && m_savePoint->available;
    }
    inline bool isFree() const { return !m_savePoint || m_savePoint->isFree(); }
  };

  typedef std::vector<Holder> List;

private:
  int m_refCount;
  int m_lockCount;

  inline void hold() { ++m_refCount; }
  inline void release() {
    if ((--m_refCount) <= 0) delete this;
  }
  inline void lock() { ++m_lockCount; }
  inline void unlock() { --m_lockCount; }

public:
  bool available;

  inline explicit TInputSavePoint(bool available = false)
      : m_refCount(), m_lockCount(), available(available) {}
  inline bool isFree() const { return m_lockCount <= 0; }

  static inline Holder create(bool available = false) {
    return Holder(new TInputSavePoint(available));
  }
};

//*****************************************************************************************
//    TInputModifier definition
//*****************************************************************************************

class DVAPI TInputModifier : public TSmartObject {
private:
  TInputManager *m_manager;

public:
  typedef std::vector<TInputModifierP> List;

  TInputManager *getManager() const { return m_manager; }
  void setManager(TInputManager *manager);
  virtual void onSetManager() {}

  virtual void activate() {}

  virtual void modifyTrack(const TTrack &track,
                           const TInputSavePoint::Holder &savePoint,
                           TTrackList &outTracks);
  virtual void modifyTracks(const TTrackList &tracks,
                            const TInputSavePoint::Holder &savePoint,
                            TTrackList &outTracks);

  virtual void modifyHover(const TPointD &hover, THoverList &outHovers);
  virtual void modifyHovers(const THoverList &hovers, THoverList &outHovers);

  virtual TRectD calcDrawBoundsHover(const TPointD &hover) { return TRectD(); }
  virtual TRectD calcDrawBoundsTrack(const TTrack &track) { return TRectD(); }
  virtual TRectD calcDrawBounds(const TTrackList &tracks,
                                const THoverList &hovers);

  virtual void drawTrack(const TTrack &track) {}
  virtual void drawHover(const TPointD &hover) {}
  virtual void drawTracks(const TTrackList &tracks);
  virtual void drawHovers(const THoverList &hovers);
  virtual void draw(const TTrackList &tracks, const THoverList &hovers);

  virtual void deactivate() {}
};

//*****************************************************************************************
//    TInputManager definition
//*****************************************************************************************

class DVAPI TInputManager : public QObject {
  Q_OBJECT
public:
  class TrackHandler : public TTrackHandler {
  public:
    std::vector<int> saves;
    TrackHandler(TTrack &original, int keysCount = 0)
        : TTrackHandler(original), saves(keysCount, 0) {}
  };

private:
  TToolViewer *m_viewer;
  TInputModifier::List m_modifiers;
  std::vector<TTrackList> m_tracks;
  std::vector<THoverList> m_hovers;
  TInputSavePoint::List m_savePoints;
  int m_started;
  int m_savePointsSent;
  mutable TPointD m_dpiScale;

  static TInputState::TouchId m_lastTouchId;

public:
  TInputState state;

public:
  TInputManager();

private:
  void paintRollbackTo(int saveIndex, TTrackList &subTracks);
  void paintApply(int count, TTrackList &subTracks);
  void paintTracks();

  int trackCompare(const TTrack &track, TInputState::DeviceId deviceId,
                   TInputState::TouchId touchId);
  const TTrackP &createTrack(int index, TInputState::DeviceId deviceId,
                             TInputState::TouchId touchId, TTimerTicks ticks,
                             bool hasPressure, bool hasTilt);
  const TTrackP &getTrack(TInputState::DeviceId deviceId,
                          TInputState::TouchId touchId, bool create = false,
                          TTimerTicks ticks = TTimerTicks(),
                          bool hasPressure = false, bool hasTilt = false);
  void addTrackPoint(const TTrackP &track, const TPointD &position,
                     double pressure, const TPointD &tilt,
                     const TPointD &worldPosition,
                     const TPointD &screenPosition, double time, bool final);
  void touchTrack(const TTrackP &track, bool finish = false);
  void touchTracks(bool finish = false);
  
  void tryTouchTrack(TInputState::DeviceId deviceId,
                     TInputState::TouchId touchId,
                     TPointD last_position);

  void modifierActivate(const TInputModifierP &modifier);
  void modifierDeactivate(const TInputModifierP &modifier);

public:
  inline const TTrackList &getInputTracks() const { return m_tracks.front(); }
  inline const TTrackList &getOutputTracks() const { return m_tracks.back(); }

  inline const THoverList &getInputHovers() const { return m_hovers.front(); }
  inline const THoverList &getOutputHovers() const { return m_hovers.back(); }

  void processTracks();
  void finishTracks();
  void reset();

  TToolViewer *getViewer() const { return m_viewer; }
  void setViewer(TToolViewer *viewer);

  bool isActive() const;

  static TApplication *getApplication();
  static TTool *getTool();

  int getModifiersCount() const { return (int)m_modifiers.size(); }
  const TInputModifierP &getModifier(int index) const {
    return m_modifiers[index];
  }
  int findModifier(const TInputModifierP &modifier) const;
  void insertModifier(int index, const TInputModifierP &modifier);
  void addModifier(const TInputModifierP &modifier) {
    insertModifier(getModifiersCount(), modifier);
  }
  void removeModifier(int index);
  void removeModifier(const TInputModifierP &modifier) {
    removeModifier(findModifier(modifier));
  }
  void clearModifiers();

  void updateDpiScale() const;
  const TPointD &dpiScale() const { return m_dpiScale; }

  TAffine toolToWorld() const;
  TAffine worldToTool() const;
  TAffine worldToScreen() const;
  TAffine screenToWorld() const;

  inline TAffine toolToScreen() const {
    return toolToWorld() * worldToScreen();
  }
  inline TAffine screenToTool() const {
    return screenToWorld() * worldToTool();
  }

  void trackEvent(TInputState::DeviceId deviceId, TInputState::TouchId touchId,
                  const TPointD &screenPosition, const double *pressure,
                  const TPointD *tilt, bool final, TTimerTicks ticks);
  void trackEventFinish(TInputState::DeviceId deviceId,
                        TInputState::TouchId touchId);
  bool keyEvent(bool press, TInputState::Key key, TTimerTicks ticks,
                QKeyEvent *event);
  void buttonEvent(bool press, TInputState::DeviceId deviceId,
                   TInputState::Button button, TTimerTicks ticks);
  void releaseAllEvent(TTimerTicks ticks);
  void hoverEvent(const THoverList &hovers);
  void doubleClickEvent();
  void textEvent(const std::wstring &preedit, const std::wstring &commit,
                 int replacementStart, int replacementLen);
  void enterEvent();
  void leaveEvent();

  TRectD calcDrawBounds();
  void draw();

  static TInputState::TouchId genTouchId();

public slots:
  void onToolSwitched();
};

//*****************************************************************************************
//    export template implementations for win32
//*****************************************************************************************

#ifdef _WIN32
template class DVAPI TSmartPointerT<TInputModifier>;
#endif

#endif
