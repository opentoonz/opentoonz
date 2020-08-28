#pragma once

#ifndef KEYSTATE_INCLUDED
#define KEYSTATE_INCLUDED

// TnzTools includes
#include <tools/tooltimer.h>

// TnzCore includes
#include <tcommon.h>
#include <tsmartpointer.h>

// std includes
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

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

//*****************************************************************************************
//    TKeyState definition
//*****************************************************************************************

template <typename T>
class TKeyStateT : public TSmartObject {
public:
  typedef T Type;
  typedef TSmartPointerT<TKeyStateT> Pointer;

  struct Holder {
    Pointer state;
    TTimerTicks ticks;
    double timeOffset;

    explicit Holder(const Pointer &state = Pointer(), TTimerTicks ticks = 0,
                    double timeOffset = 0.0)
        : state(state), ticks(ticks), timeOffset(timeOffset) {}

    Pointer find(const Type &value) const {
      return state ? state->find(value) : Pointer();
    }
    bool isEmpty() const { return !state || state->isEmpty(); }
    bool isPressed(const Type &value) const { return find(value); }
    double howLongPressed(const Type &value) {
      return howLongPressed(find(value), ticks, timeOffset);
    }

    static double howLongPressed(const Pointer &state, long ticks,
                                 double timeOffset) {
      return state ? std::max(
                         TToolTimer::step,
                         (ticks - state->ticks) * TToolTimer::step + timeOffset)
                   : 0.0;
    }
  };

  static const Type &none() {
    static Type none;
    return none;
  }

  static const Pointer &empty() {
    static Pointer empty = new TKeyStateT();
    return empty;
  }

  const Pointer previous;
  const TTimerTicks ticks;
  const Type value;

private:
  TKeyStateT(const Pointer &previous, TTimerTicks ticks, const Type &value)
      : previous(previous), ticks(ticks), value(value) {}

  Pointer makeChainWithout(const Pointer &state) {
    return state == this || !previous
               ? previous
               : Pointer(new TKeyStateT(previous->makeChainWithout(state),
                                        ticks, value));
  }

public:
  TKeyStateT() : ticks(0), value(none()) {}

  Pointer find(const Type &value) {
    return value == none() ? Pointer() : value == this->value
                                             ? this
                                             : previous ? previous->find(value)
                                                        : Pointer();
  }

  Pointer change(bool press, const Type &value, TTimerTicks ticks) {
    if (value == none()) return Pointer(this);
    if (ticks <= this->ticks) ticks = this->ticks + 1;

    Pointer p = find(value);
    if (press) {
      if (p) return Pointer(this);
      return Pointer(new TKeyStateT((isEmpty() ? Pointer() : Pointer(this)),
                                    ticks, value));
    }

    if (!p) return Pointer(this);
    Pointer chain = makeChainWithout(p);
    return chain ? chain : Pointer(new TKeyStateT());
  }

  Pointer released_all(TTimerTicks)
    { return isEmpty() ? Pointer(this) : Pointer(new TKeyStateT()); }
  
  bool isEmpty()
    { return value == none() && (!previous || previous->isEmpty()); }
  bool isPressed(const Type &value)
    { return find(value); }

  Pointer pressed(const Type &value, TTimerTicks ticks)
    { return change(true, value, ticks); }
  Pointer released(const Type &value, TTimerTicks ticks)
    { return change(false, value, ticks); }

  // low-level functions for debug
  const Type& get_last_pressed_value() const
    { return value; }
  const TTimerTicks& get_last_pressed_ticks() const
    { return ticks; }
  const Pointer& get_previous_pressed_state() const
    { return previous; }
};

//*****************************************************************************************
//    TKeyHistory definition
//*****************************************************************************************

template <typename T>
class TKeyHistoryT : public TSmartObject {
public:
  typedef T Type;
  typedef TSmartPointerT<TKeyHistoryT> Pointer;
  typedef TKeyStateT<Type> State;
  typedef typename TKeyStateT<Type>::Pointer StatePointer;
  typedef typename TKeyStateT<Type>::Holder StateHolder;
  
  typedef std::map<TTimerTicks, StatePointer> StateMap;
  typedef std::multiset<TTimerTicks> LockSet;

  class Holder {
  private:
    Pointer m_history;
    TTimerTicks m_ticks;
    double m_timeOffset;
    TTimerTicks m_heldTicks;

  public:
    Holder() : m_ticks(), m_timeOffset(), m_heldTicks() {}
    Holder(const Pointer &history, TTimerTicks ticks, double timeOffset = 0.0)
        : m_ticks(), m_timeOffset(), m_heldTicks() {
      set(history, ticks, timeOffset);
    }
    Holder(const Holder &other) : m_ticks(), m_timeOffset(), m_heldTicks() {
      set(other);
    }
    ~Holder() { reset(); }

    Holder &operator=(const Holder &other) {
      set(other);
      return *this;
    }

    void set(const Pointer &history, TTimerTicks ticks,
             double timeOffset = 0.0) {
      if (m_history) m_history->releaseTicks(m_heldTicks);
      m_history    = history;
      m_ticks      = ticks;
      m_timeOffset = timeOffset;
      m_heldTicks  = (m_history ? m_history->holdTicks(m_ticks) : 0);
    }
    void set(const Holder &other) {
      set(other.history(), other.ticks(), other.timeOffset());
    }
    void reset() { set(Pointer(), 0); }

    Pointer history() const { return m_history; }
    TTimerTicks ticks() const { return m_ticks; }
    double timeOffset() const { return m_timeOffset; }

    Holder offset(double timeOffset) const {
      return fabs(timeOffset) < TToolTimer::epsilon
                 ? *this
                 : Holder(m_history, m_ticks, m_timeOffset + timeOffset);
    }

    StateHolder get(double time) const {
      TTimerTicks dticks = (TTimerTicks)ceil(
          TToolTimer::frequency * (time + m_timeOffset - TConsts::epsilon));
      StatePointer state = m_history->get(m_ticks + dticks);
      return StateHolder(state, m_ticks, m_timeOffset + time);
    }
  };

private:
  StateMap m_states;
  LockSet m_locks;

  void autoRemove() {
    TTimerTicks ticks =
        m_locks.empty() ? m_states.rbegin()->first : *m_locks.begin();
    while (true) {
      typename std::map<TTimerTicks, StatePointer>::iterator i =
          m_states.begin();
      ++i;
      if (i == m_states.end() || (!i->second->isEmpty() && i->first >= ticks))
        break;
      m_states.erase(i);
    }
  }

  TTimerTicks holdTicks(TTimerTicks ticks) {
    return *m_locks.insert(std::max(ticks, m_states.begin()->first));
  }
  void releaseTicks(TTimerTicks heldTicks) {
    m_locks.erase(heldTicks);
    autoRemove();
  }

  StatePointer get(TTimerTicks ticks) {
    typename std::map<TTimerTicks, StatePointer>::iterator i =
        m_states.upper_bound(ticks);
    return i == m_states.begin() ? i->second : (--i)->second;
  }

public:
  TKeyHistoryT() { m_states[TTimerTicks()] = StatePointer(new State()); }

  StatePointer current() const { return m_states.rbegin()->second; }

  StatePointer change(bool press, Type value, TTimerTicks ticks) {
    StatePointer state = current()->change(press, value, ticks);
    if (state != current() && ticks > m_states.rbegin()->first)
      m_states[ticks] = state;
    autoRemove();
    return current();
  }
  
  StatePointer released_all(TTimerTicks ticks) {
    StatePointer state = current()->released_all(ticks);
    if (state != current() && ticks > m_states.rbegin()->first)
      m_states[ticks] = state;
    autoRemove();
    return current();
  }

  StatePointer pressed(Type value, TTimerTicks ticks)
    { return change(true, value, ticks); }
  StatePointer released(Type value, TTimerTicks ticks)
    { return change(false, value, ticks); }
  
  // low-level functions for debug
  const StateMap& get_stored_states() const
    { return m_states; }
  const LockSet& get_locks() const
    { return m_locks; }
};

#endif
