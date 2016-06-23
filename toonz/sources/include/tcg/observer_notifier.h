#pragma once

#ifndef TCG_OBSERVER_NOTIFIER_H
#define TCG_OBSERVER_NOTIFIER_H

// tcg includes
#include "base.h"

// STD includes
#include <set>

/*!
  \file     tcg_observer_notifier.h

  \brief    This file contains the barebone of an elementary, classical
            implementation of the Observer pattern requiring derivation.

  \details  This is intended as a convenient and lightweight predefined
            implementation when hardwiring the Observer pattern in a
            known class using derivation.

            Observe that this implementation <I>does not deal with the
            actual notifications forwarding</I> - it just manages the
            set of observers/notifiers in a notifier/observer instance.

            No multithreading is taken into consideration.

  \remark   See elsewhere for more advanced implementations.
*/

namespace tcg {

//************************************************************************
//    Observer/notifier  base polymorphic classes
//************************************************************************

class observer_base;
class notifier_base;

//------------------------------------------------------------

class observer_base {
public:
  virtual ~observer_base() {}

  virtual void attach(notifier_base *notifier) = 0;  //!< Adds the specified
  //! notifier to the internal
  //! set of notifiers.
  virtual void detach(notifier_base *notifier) = 0;  //!< Removes the specified
                                                     //! notifier from the
  //! internal set of
  //! notifiers.
};

//------------------------------------------------------------

class notifier_base {
public:
  virtual ~notifier_base() {}

  virtual void attach(observer_base *observer) = 0;  //!< Adds the specified
  //! observer to the internal
  //! set of observers.
  virtual void detach(observer_base *observer) = 0;  //!< Removes the specified
                                                     //! observer from the
  //! internal set of
  //! observers.
};

//************************************************************************
//    Observer/notifier  with a single interacting object
//************************************************************************

template <typename Notifier = notifier_base, typename Base = observer_base>
class observer_single : public noncopyable<Base> {
  Notifier *m_notifier;  // Not owned

public:
  typedef Notifier notifier_type;

public:
  observer_single() : m_notifier() {}
  ~observer_single() {
    if (m_notifier)
      static_cast<notifier_base *>(m_notifier)
          ->detach(
              this);  // The detaching function is most probably \a private in
  }                   // reimplemented notifier classes (like in this observer).
                      // Besides, it would be a virtual function call anyway.
  notifier_type *notifier() const { return m_notifier; }

private:
  void attach(notifier_base *notifier) {
    assert(notifier && !m_notifier);
    m_notifier = static_cast<Notifier *>(notifier);
  }

  void detach(notifier_base *notifier) {
    assert(m_notifier && m_notifier == static_cast<Notifier *>(notifier));
    m_notifier = 0;
  }
};

//------------------------------------------------------------

template <typename Observer = observer_base, typename Base = notifier_base>
class notifier_single : public noncopyable<Base> {
  Observer *m_observer;  // Not owned

public:
  typedef Observer observer_type;

public:
  notifier_single() : m_observer() {}
  ~notifier_single() {
    if (m_observer) static_cast<observer_base *>(m_observer)->detach(this);
  }

  observer_type *observer() const { return m_observer; }

  void addObserver(observer_type *observer) {
    notifier_single::attach(
        observer);  // No reason to go polymorphic here - it's this very class.
    static_cast<observer_base *>(observer)->attach(
        this);  // However, here it's necessary - like above, downcast to
  }             // the very base.

  void removeObserver(observer_type *observer) {
    static_cast<observer_base *>(observer)->detach(this);
    notifier_single::detach(observer);
  }

private:
  void attach(observer_base *observer) {
    assert(observer && !m_observer);
    m_observer = static_cast<Observer *>(observer);
  }

  void detach(observer_base *observer) {
    assert(m_observer && m_observer == static_cast<Observer *>(observer));
    m_observer = 0;
  }
};

//************************************************************************
//    Observer/notifier  with multiple interacting objects
//************************************************************************

template <typename Notifier = notifier_base, typename Base = observer_base,
          typename Set = std::set<Notifier *>>
class observer : public noncopyable<Base> {
  Set m_notifiers;  // Not owned

public:
  typedef Notifier notifier_type;
  typedef Set notifiers_container;

public:
  ~observer() {
    typename Set::iterator nt, nEnd = m_notifiers.end();
    for (nt = m_notifiers.begin(); nt != nEnd; ++nt)
      static_cast<notifier_base *>(*nt)->detach(this);
  }

  const notifiers_container &notifiers() const { return m_notifiers; }

private:
  void attach(notifier_base *notifier) {
    assert(notifier);
    m_notifiers.insert(static_cast<Notifier *>(notifier));
  }

  void detach(notifier_base *notifier) {
    assert(!m_notifiers.empty());
    m_notifiers.erase(static_cast<Notifier *>(notifier));
  }
};

//------------------------------------------------------------

template <typename Observer = observer_base, typename Base = notifier_base,
          typename Set = std::set<Observer *>>
class notifier : public noncopyable<Base> {
  Set m_observers;  // Not owned

public:
  typedef Observer observer_type;
  typedef Set observers_container;

public:
  ~notifier() {
    typename Set::iterator ot, oEnd = m_observers.end();
    for (ot = m_observers.begin(); ot != oEnd; ++ot)
      static_cast<observer_base *>(*ot)->detach(this);
  }

  const observers_container &observers() const { return m_observers; }

  void addObserver(observer_type *observer) {
    notifier::attach(observer);
    static_cast<observer_base *>(observer)->attach(this);
  }

  void removeObserver(observer_type *observer) {
    static_cast<observer_base *>(observer)->detach(this);
    notifier::detach(observer);
  }

private:
  void attach(observer_base *observer) {
    assert(observer);
    m_observers.insert(static_cast<Observer *>(observer));
  }

  void detach(observer_base *observer) {
    assert(!m_observers.empty());
    m_observers.erase(static_cast<Observer *>(observer));
  }
};

}  // namespace tcg

#endif  // TCG_OBSERVER_NOTIFIER_H
