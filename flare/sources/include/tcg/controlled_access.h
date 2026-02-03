#pragma once

#ifndef TCG_CONTROLLED_ACCESS_H
#define TCG_CONTROLLED_ACCESS_H

// tcg includes
#include "base.h"

/*!
  \file     controlled_access.h

  \brief    This file contains the description of a C++ idiom that can be used
            to perform controlled access on an object.

  \details  Controlled access forces users to invoke a function every time an
            object is accessed, at compile-time.

            Some notable cases for this idiom include the "frequent notifier /
  lazy update"
            scenario, where an object is frequently notified for status updates,
  but
            many updates can be spared until the data is finally accessed.
*/

namespace tcg {

enum DirectAccess { direct_access };  //!< Enum used to access variables with
                                      //!  controlled output directly.

//*************************************************************************
//    tcg::controlled_access  definition
//*************************************************************************

/*!
  \brief    The controlled_access template class encapsulate an object that
            can be accessed \b only after a functor has been invoked, enforcing
            access tracking at compile time.

  \details  The accessor functor can be specified by explicit template argument
            or supplying it during access. In case the accessor type is
  specified,
            an instance of the functor will be stored together with the accessed
            variable.

  \todo     Move functor to base class to enable empty class optimization for
            stateless functors.
*/

template <typename Obj, typename Func = tcg::empty_type>
class controlled_access : tcg::noncopyable<> {
  Obj m_obj;    //!< Stored object.
  Func m_func;  //!< Functor to be invoked upon access.

public:
  typedef Obj obj_type;
  typedef Func func_type;

public:
  controlled_access(const func_type &func) : m_obj(), m_func(func) {}
  controlled_access(const obj_type &obj, const func_type &func)
      : m_obj(obj), m_func(func) {}

  const obj_type &operator()(DirectAccess) const { return m_obj; }
  obj_type &operator()(DirectAccess) { return m_obj; }

  const obj_type &operator()() const {
    m_func(m_obj);
    return m_obj;
  }
  obj_type &operator()() {
    m_func(m_obj);
    return m_obj;
  }
};

//------------------------------------------------------------------------------

/*!
  \brief    Explicit specialization advocating no explicit accessor functor -
  the
            accessor must be supplied upon access.
*/

template <typename Obj>
class controlled_access<Obj, tcg::empty_type> : tcg::noncopyable<> {
  Obj m_obj;  //!< Stored object.

public:
  typedef Obj obj_type;

public:
  controlled_access() : m_obj() {}
  controlled_access(const obj_type &obj) : m_obj(obj) {}

  const obj_type &operator()(DirectAccess) const { return m_obj; }
  obj_type &operator()(DirectAccess) { return m_obj; }

  template <typename Func>
  const obj_type &operator()(Func func) const {
    func(m_obj);
    return m_obj;
  }

  template <typename Func>
  obj_type &operator()(Func func) {
    func(m_obj);
    return m_obj;
  }
};

//==============================================================================

/*!
  \brief    The invalidable template class is a common case of controlled
  variable
            access for objects which can be \a invalidated, and thus require a
  validator
            procedure to be invoked strictly before a user access to the object
  can be
            allowed.

  \details  An invalidable instance wraps a \a mutable object and adds a boolean
  to
            signal its current validity state.

            The initial validity of the object can be specified at construction,
  and
            explicitly set to the invalid state through the invalidate() method.

            Access to the wrapped object requires either the invocation a
  validator
            functor through the various operator() overloads, or that the access
  is
            explicitly marked as \a direct by supplying the \p
  tcg::direct_access tag.
*/

template <typename Obj, typename Func = tcg::empty_type>
class invalidable : tcg::noncopyable<> {
  mutable Obj m_obj;       //!< Stored object.
  mutable bool m_invalid;  //!< Data validity status.

  Func m_func;  //!< Functor to be invoked upon access.

public:
  typedef Obj obj_type;
  typedef Func func_type;

public:
  invalidable(const func_type &func, bool invalid = false)
      : m_obj(), m_func(func), m_invalid(invalid) {}
  invalidable(const obj_type &obj, const func_type &func, bool invalid = false)
      : m_obj(obj), m_func(func), m_invalid(invalid) {}

  void invalidate() { m_invalid = true; }
  bool isInvalid() const { return m_invalid; }

  const obj_type &operator()(DirectAccess) const { return m_obj; }
  obj_type &operator()(DirectAccess) { return m_obj; }

  const obj_type &operator()() const {
    if (m_invalid) m_func(m_obj), m_invalid = false;

    return m_obj;
  }

  obj_type &operator()() {
    if (m_invalid) m_func(m_obj), m_invalid = false;

    return m_obj;
  }
};

//------------------------------------------------------------------------------

/*!
  \brief    Explicit specialization advocating no explicit validator functor -
  the
            accessor must be supplied upon access.
*/

template <typename Obj>
class invalidable<Obj, tcg::empty_type> : tcg::noncopyable<> {
  mutable Obj m_obj;       //!< Stored object
  mutable bool m_invalid;  //!< Data validity status

public:
  typedef Obj obj_type;

public:
  invalidable(bool invalid = false) : m_obj(), m_invalid(invalid) {}
  invalidable(const obj_type &obj, bool invalid = false)
      : m_obj(obj), m_invalid(invalid) {}

  void invalidate() { m_invalid = true; }
  bool isInvalid() const { return m_invalid; }

  const obj_type &operator()(DirectAccess) const { return m_obj; }
  obj_type &operator()(DirectAccess) { return m_obj; }

  template <typename Func>
  const obj_type &operator()(Func func) const {
    if (m_invalid) func(m_obj), m_invalid = false;

    return m_obj;
  }

  template <typename Func>
  obj_type &operator()(Func func) {
    if (m_invalid) func(m_obj), m_invalid = false;

    return m_obj;
  }
};

}  // namespace tcg

#endif  // TCG_CONTROLLED_ACCESS_H
