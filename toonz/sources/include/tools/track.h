#pragma once

#ifndef TRACK_INCLUDED
#define TRACK_INCLUDED

// TnzTools includes
#include <tools/inputstate.h>

// TnzCore includes
#include <tcommon.h>
#include <tgeometry.h>

// Qt headers
#include <Qt>

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

//===================================================================

//    Forward Declarations

class TTrack;
class TTrackPoint;
class TTrackTangent;
class TTrackHandler;
class TTrackToolHandler;
class TTrackModifier;

typedef TSmartPointerT<TTrack> TTrackP;
typedef TSmartPointerT<TTrackHandler> TTrackHandlerP;
typedef TSmartPointerT<TTrackToolHandler> TTrackToolHandlerP;
typedef TSmartPointerT<TTrackModifier> TTrackModifierP;

typedef std::vector<TTrackPoint> TTrackPointList;
typedef std::vector<TTrackTangent> TTrackTangentList;
typedef std::vector<TTrackP> TTrackList;

//===================================================================

//*****************************************************************************************
//    export template implementations for win32
//*****************************************************************************************

#ifdef _WIN32
template class DVAPI TSmartPointerT<TTrack>;
template class DVAPI TSmartPointerT<TTrackHandler>;
template class DVAPI TSmartPointerT<TTrackToolHandler>;
template class DVAPI TSmartPointerT<TTrackModifier>;
#endif

//*****************************************************************************************
//    TTrackPoint definition
//*****************************************************************************************

class DVAPI TTrackPoint {
public:
  TPointD position;
  double pressure;
  TPointD tilt;

  TPointD worldPosition;
  TPointD screenPosition;

  double originalIndex;
  double time;
  double length;

  bool final;

  explicit TTrackPoint(const TPointD &position = TPointD(),
                       double pressure = 0.5, const TPointD &tilt = TPointD(),
                       const TPointD &worldPosition  = TPointD(),
                       const TPointD &screenPosition = TPointD(),
                       double originalIndex = 0.0, double time = 0.0,
                       double length = 0.0, bool final = false)
      : position(position)
      , pressure(pressure)
      , tilt(tilt)
      , worldPosition(worldPosition)
      , screenPosition(screenPosition)
      , originalIndex(originalIndex)
      , time(time)
      , length(length)
      , final(final) {}
};

//*****************************************************************************************
//    TTrackTangent definition
//*****************************************************************************************

class DVAPI TTrackTangent {
public:
  TPointD position;
  double pressure;
  TPointD tilt;

  TPointD worldPosition;
  TPointD screenPosition;

  inline explicit TTrackTangent(const TPointD &position       = TPointD(),
                                double pressure               = 0.0,
                                const TPointD &tilt           = TPointD(),
                                const TPointD &worldPosition  = TPointD(),
                                const TPointD &screenPosition = TPointD())
      : position(position)
      , pressure(pressure)
      , tilt(tilt)
      , worldPosition(worldPosition)
      , screenPosition(screenPosition) {}
};

//*****************************************************************************************
//    TTrackHandler definition
//*****************************************************************************************

class DVAPI TTrackHandler : public TSmartObject {
public:
  const TTrack &original;
  std::vector<TTrackP> tracks;
  TTrackHandler(const TTrack &original) : original(original) {}
};

//*****************************************************************************************
//    TTrackToolHandler definition
//*****************************************************************************************

class DVAPI TTrackToolHandler : public TSmartObject {};

//*****************************************************************************************
//    TTrackModifier definition
//*****************************************************************************************

class DVAPI TTrackModifier : public TSmartObject {
public:
  TTrackHandler &handler;
  const TTrack &original;
  const double timeOffset;

  explicit TTrackModifier(TTrackHandler &handler, double timeOffset = 0.0)
      : handler(handler), original(handler.original), timeOffset(timeOffset) {}
  virtual TTrackPoint calcPoint(double originalIndex);
};

//*****************************************************************************************
//    TTrack definition
//*****************************************************************************************

class DVAPI TTrack : public TSmartObject {
public:
  typedef long long Id;

private:
  static Id m_lastId;

public:
  const Id id;
  const TInputState::DeviceId deviceId;
  const TInputState::TouchId touchId;
  const TInputState::KeyHistory::Holder keyHistory;
  const TInputState::ButtonHistory::Holder buttonHistory;
  const bool hasPressure;
  const bool hasTilt;
  const TTrackModifierP modifier;

  mutable TTrackHandlerP handler;
  mutable TTrackToolHandlerP toolHandler;
  mutable int pointsRemoved;
  mutable int pointsAdded;

private:
  TTrackPointList m_points;
  const TTrackPoint m_none;

public:
  explicit TTrack(TInputState::DeviceId deviceId = TInputState::DeviceId(),
                  TInputState::TouchId touchId   = TInputState::TouchId(),
                  const TInputState::KeyHistory::Holder &keyHistory =
                      TInputState::KeyHistory::Holder(),
                  const TInputState::ButtonHistory::Holder &buttonHistory =
                      TInputState::ButtonHistory::Holder(),
                  bool hasPressure = false, bool hasTilt = false);

  explicit TTrack(const TTrackModifierP &modifier);

  inline const TTrack *original() const {
    return modifier ? &modifier->original : NULL;
  }
  inline double timeOffset() const {
    return modifier ? modifier->timeOffset : 0.0;
  }
  inline TTimerTicks ticks() const { return keyHistory.ticks(); }
  inline bool changed() const { return pointsAdded != 0 || pointsRemoved != 0; }

  const TTrack *root() const;
  int level() const;

  inline int clampIndex(int index) const {
    return std::min(std::max(index, 0), size() - 1);
  }
  inline int floorIndexNoClamp(double index) const {
    return (int)floor(index + TConsts::epsilon);
  }
  inline int floorIndex(double index) const {
    return clampIndex(floorIndexNoClamp(index));
  }
  inline int ceilIndexNoClamp(double index) const {
    return (int)ceil(index - TConsts::epsilon);
  }
  inline int ceilIndex(double index) const {
    return clampIndex(ceilIndexNoClamp(index));
  }

  int floorIndex(double index, double *outFrac) const;

  inline const TTrackPoint &floorPoint(double index,
                                       double *outFrac = NULL) const {
    return point(floorIndex(index, outFrac));
  }
  inline const TTrackPoint &ceilPoint(double index) const {
    return point(ceilIndex(index));
  }

  inline const TTrackPoint &point(int index) const {
    return empty() ? m_none : m_points[clampIndex(index)];
  }

  inline int size() const { return (int)m_points.size(); }
  inline bool empty() const { return m_points.empty(); }
  inline const TTrackPoint &front() const { return point(0); }
  inline const TTrackPoint &back() const { return point(size() - 1); }
  inline bool finished() const { return !m_points.empty() && back().final; }
  inline const TTrackPoint &operator[](int index) const { return point(index); }
  inline const TTrackPointList &points() const { return m_points; }

  inline void resetRemoved() const { pointsRemoved = 0; }
  inline void resetAdded() const { pointsAdded = 0; }
  inline void resetChanges() const {
    resetRemoved();
    resetAdded();
  }

  void push_back(const TTrackPoint &point);
  void pop_back(int count = 1);

  inline void truncate(int count) { pop_back(size() - count); }

  inline const TTrackPoint &current() const {
    return point(size() - pointsAdded);
  }
  inline const TTrackPoint &previous() const {
    return point(size() - pointsAdded - 1);
  }
  inline const TTrackPoint &next() const {
    return point(size() - pointsAdded + 1);
  }

  inline TInputState::KeyState::Holder getKeyState(double time) const {
    return keyHistory.get(time);
  }
  inline TInputState::KeyState::Holder getCurrentKeyState() const {
    return getKeyState(current().time);
  }
  inline TInputState::ButtonState::Holder getButtonState(double time) const {
    return buttonHistory.get(time);
  }
  inline TInputState::ButtonState::Holder getCurrentButtonState() const {
    return getButtonState(current().time);
  }

private:
  template <double TTrackPoint::*Field>
  double binarySearch(double value) const {
    // points_[a].value <= value < points_[b].value
    if (m_points.empty()) return 0.0;
    int a     = 0;
    double aa = m_points[a].*Field;
    if (value - aa <= 0.5 * TConsts::epsilon) return (double)a;
    int b     = (int)m_points.size() - 1;
    double bb = m_points[b].*Field;
    if (bb - value <= 0.5 * TConsts::epsilon) return (double)b;
    while (true) {
      int c = (a + b) / 2;
      if (a == c) break;
      double cc = m_points[c].*Field;
      if (cc - value > 0.5 * TConsts::epsilon) {
        b  = c;
        bb = cc;
      } else {
        a  = c;
        aa = cc;
      }
    }
    return bb - aa >= 0.5 * TConsts::epsilon
               ? (double)a + (value - aa) / (bb - aa)
               : (double)a;
  }

public:
  inline double indexByOriginalIndex(double originalIndex) const {
    return binarySearch<&TTrackPoint::originalIndex>(originalIndex);
  }
  inline double indexByTime(double time) const {
    return binarySearch<&TTrackPoint::time>(time);
  }
  inline double indexByLength(double length) const {
    return binarySearch<&TTrackPoint::length>(length);
  }

  inline double originalIndexByIndex(double index) const {
    double frac;
    const TTrackPoint &p0 = floorPoint(index, &frac);
    const TTrackPoint &p1 = ceilPoint(index);
    return interpolationLinear(p0.originalIndex, p1.originalIndex, frac);
  }
  inline double timeByIndex(double index) const {
    double frac;
    const TTrackPoint &p0 = floorPoint(index, &frac);
    const TTrackPoint &p1 = ceilPoint(index);
    return interpolationLinear(p0.time, p1.time, frac);
  }
  inline double lengthByIndex(double index) const {
    double frac;
    const TTrackPoint &p0 = floorPoint(index, &frac);
    const TTrackPoint &p1 = ceilPoint(index);
    return interpolationLinear(p0.length, p1.length, frac);
  }

  TTrackPoint calcPoint(double index) const;
  TPointD calcTangent(double index, double distance = 0.1) const;
  double rootIndexByIndex(double index) const;
  TTrackPoint calcRootPoint(double index) const;

  inline TTrackPoint interpolateLinear(double index) const {
    double frac;
    const TTrackPoint &p0 = floorPoint(index, &frac);
    const TTrackPoint &p1 = ceilPoint(index);
    return interpolationLinear(p0, p1, frac);
  }

  template <typename T>
  static inline T interpolationLinear(const T &p0, const T &p1, double l) {
    return p0 * (1.0 - l) + p1 * l;
  }

  template <typename T>
  static T interpolationSpline(const T &p0, const T &p1, const T &t0,
                               const T &t1, double l) {
    double ll  = l * l;
    double lll = ll * l;
    return p0 * (2.0 * lll - 3.0 * ll + 1.0) + p1 * (-2.0 * lll + 3.0 * ll) +
           t0 * (lll - 2.0 * ll + l) + t1 * (lll - 1.0 * ll);
  }

  static inline TTrackPoint interpolationLinear(const TTrackPoint &p0,
                                                const TTrackPoint &p1,
                                                double l) {
    if (l <= TConsts::epsilon) return p0;
    if (l >= 1.0 - TConsts::epsilon) return p1;
    return TTrackPoint(
        interpolationLinear(p0.position, p1.position, l),
        interpolationLinear(p0.pressure, p1.pressure, l),
        interpolationLinear(p0.tilt, p1.tilt, l),
        interpolationLinear(p0.worldPosition, p1.worldPosition, l),
        interpolationLinear(p0.screenPosition, p1.screenPosition, l),
        interpolationLinear(p0.originalIndex, p1.originalIndex, l),
        interpolationLinear(p0.time, p1.time, l),
        interpolationLinear(p0.length, p1.length, l), p0.final && p1.final);
  }

  static inline TTrackPoint interpolationSpline(const TTrackPoint &p0,
                                                const TTrackPoint &p1,
                                                const TTrackTangent &t0,
                                                const TTrackTangent &t1,
                                                double l) {
    if (l <= TConsts::epsilon) return p0;
    if (l >= 1.0 - TConsts::epsilon) return p1;
    return TTrackPoint(
        interpolationSpline(p0.position, p1.position, t0.position, t1.position,
                            l),
        interpolationLinear(p0.pressure, p1.pressure, l),
        // interpolationSpline(p0.pressure      , p1.pressure      , t0.pressure
        // , t1.pressure      , l),
        interpolationLinear(p0.tilt, p1.tilt, l),
        // interpolationSpline(p0.tilt          , p1.tilt          , t0.tilt
        // , t1.tilt          , l),
        interpolationLinear(p0.worldPosition, p1.worldPosition, l),
        // interpolationSpline(p0.worldPosition , p1.worldPosition ,
        // t0.worldPosition , t1.worldPosition , l),
        interpolationLinear(p0.screenPosition, p1.screenPosition, l),
        // interpolationSpline(p0.screenPosition, p1.screenPosition,
        // t0.screenPosition, t1.screenPosition, l),
        interpolationLinear(p0.originalIndex, p1.originalIndex, l),
        interpolationLinear(p0.time, p1.time, l),
        interpolationLinear(p0.length, p1.length, l), p0.final && p1.final);
  }
};

#endif
