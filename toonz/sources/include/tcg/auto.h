#pragma once

#ifndef TCG_AUTO_H
#define TCG_AUTO_H

#include "base.h"
#include "traits.h"

/* \file    auto.h

  \brief    This file contains template classes able to perform special
  operations upon
            instance destruction.

  \details  These classes can be useful to enforce block-scoped operations at a
  block's
            entry point, considering that a block end can be far away, or the
  function
            could return abruptly at several different points.
*/

namespace tcg {

//*******************************************************************************
//    tcg::auto_type  definition
//*******************************************************************************

struct _auto_type {
  mutable bool m_destruct;

public:
  _auto_type(bool destruct) : m_destruct(destruct) {}
  _auto_type(const _auto_type &other) : m_destruct(other.m_destruct) {
    other.m_destruct = false;
  }
  _auto_type &operator=(const _auto_type &other) {
    m_destruct = other.m_destruct, other.m_destruct = false;
    return *this;
  }
};

typedef const _auto_type &auto_type;

//*******************************************************************************
//    tcg::auto_func  definition
//*******************************************************************************

template <typename Op>
struct auto_zerary : public _auto_type {
  Op m_op;

public:
  auto_zerary(bool destruct = true) : _auto_type(destruct) {}
  ~auto_zerary() {
    if (this->m_destruct) m_op();
  }
};

//--------------------------------------------------------------------

template <typename Op, typename T = typename function_traits<Op>::arg1_type>
struct auto_unary : public _auto_type {
  T m_arg1;
  Op m_op;

public:
  auto_unary(bool destruct = true) : _auto_type(destruct) {}
  auto_unary(Op op, T arg, bool destruct = true)
      : _auto_type(destruct), m_arg1(arg), m_op(op) {}
  ~auto_unary() {
    if (this->m_destruct) m_op(m_arg1);
  }
};

//--------------------------------------------------------------------

template <typename Op, typename T1 = typename function_traits<Op>::arg1_type,
          typename T2 = typename function_traits<Op>::arg2_type>
struct auto_binary : public _auto_type {
  T1 m_arg1;
  T2 m_arg2;
  Op m_op;

public:
  auto_binary(bool destruct = true) : _auto_type(destruct) {}
  auto_binary(Op op, T1 arg1, T2 arg2, bool destruct = true)
      : _auto_type(destruct), m_arg1(arg1), m_arg2(arg2), m_op(op) {}
  ~auto_binary() {
    if (this->m_destruct) m_op(m_arg1, m_arg2);
  }
};

//*******************************************************************************
//    Helper functions
//*******************************************************************************

template <typename Op>
auto_zerary<Op> make_auto(Op op, bool destruct = true) {
  return auto_zerary<Op>(op, destruct);
}

template <typename Op, typename T>
auto_unary<Op> make_auto(Op op, T &arg1, bool destruct = true) {
  return auto_unary<Op>(op, arg1, destruct);
}

template <typename Op, typename T>
auto_unary<Op> make_auto(Op op, const T &arg1, bool destruct = true) {
  return auto_unary<Op>(op, arg1, destruct);
}

template <typename Op, typename T1, typename T2>
auto_binary<Op> make_auto(Op op, T1 &arg1, T2 &arg2, bool destruct = true) {
  return auto_binary<Op>(op, arg1, arg2, destruct);
}

template <typename Op, typename T1, typename T2>
auto_binary<Op> make_auto(Op op, const T1 &arg1, T2 &arg2,
                          bool destruct = true) {
  return auto_binary<Op>(op, arg1, arg2, destruct);
}

template <typename Op, typename T1, typename T2>
auto_binary<Op> make_auto(Op op, T1 &arg1, const T2 &arg2,
                          bool destruct = true) {
  return auto_binary<Op>(op, arg1, arg2, destruct);
}

template <typename Op, typename T1, typename T2>
auto_binary<Op> make_auto(Op op, const T1 &arg1, const T2 &arg2,
                          bool destruct = true) {
  return auto_binary<Op>(op, arg1, arg2, destruct);
}

//*******************************************************************************
//    tcg::auto_reset  definition
//*******************************************************************************

template <typename T, T val>
class auto_reset {
  typedef T var_type;

public:
  var_type &m_var;

public:
  auto_reset(var_type &var) : m_var(var) {}
  ~auto_reset() { m_var = val; }

private:
  auto_reset(const auto_reset &);
  auto_reset &operator=(const auto_reset &);
};

//*******************************************************************************
//    tcg::auto_backup  definition
//*******************************************************************************

template <typename T>
struct auto_backup {
  typedef T var_type;

public:
  var_type m_backup;
  var_type *m_original;

public:
  auto_backup() : m_original() {}
  auto_backup(var_type &original) : m_original(&original), m_backup(original) {}
  auto_backup(var_type *original) : m_original(original) {
    if (m_original) m_backup = *m_original;
  }
  ~auto_backup() {
    if (m_original) *m_original = m_backup;
  }

  void reset(T &original) {
    m_original = &original;
    m_backup   = original;
  }
  void reset(T *original) {
    m_original               = original;
    if (m_original) m_backup = *original;
  }

  T *release() {
    T *original = m_original;
    m_original  = 0;
    return original;
  }

private:
  auto_backup(const auto_backup &);
  auto_backup &operator=(const auto_backup &);
};

}  // namespace tcg

#endif  // TCG_AUTO_H
