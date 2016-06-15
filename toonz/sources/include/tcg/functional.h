#pragma once

#ifndef TCG_FUNCTIONAL_H
#define TCG_FUNCTIONAL_H

#include "traits.h"

// std includes
#include <functional>

//**********************************************************************************
//    Logical functor combinators
//**********************************************************************************

namespace tcg {

template <typename Fn1, typename Fn2>
class unary_and
    : public std::unary_function<typename function_traits<Fn1>::arg_type,
                                 bool> {
  Fn1 m_fn1;
  Fn2 m_fn2;

public:
  unary_and(const Fn1 &fn1, const Fn2 &fn2) : m_fn1(fn1), m_fn2(fn2) {}

  bool operator()(const typename function_traits<Fn1>::arg_type &t) const {
    return m_fn1(t) && m_fn2(t);
  }
};

template <typename Fn1, typename Fn2>
unary_and<Fn1, Fn2> and1(const Fn1 &fn1, const Fn2 &fn2) {
  return unary_and<Fn1, Fn2>(fn1, fn2);
}

//----------------------------------------------------------------------------------

template <typename Fn1, typename Fn2>
class unary_or
    : public std::unary_function<typename function_traits<Fn1>::arg_type,
                                 bool> {
  Fn1 m_fn1;
  Fn2 m_fn2;

public:
  unary_or(const Fn1 &fn1, const Fn2 &fn2) : m_fn1(fn1), m_fn2(fn2) {}

  bool operator()(const typename function_traits<Fn1>::arg_type &t) const {
    return m_fn1(t) || m_fn2(t);
  }
};

template <typename Fn1, typename Fn2>
unary_or<Fn1, Fn2> or1(const Fn1 &fn1, const Fn2 &fn2) {
  return unary_or<Fn1, Fn2>(fn1, fn2);
}

}  // namespace tcg

#endif  // TCG_FUNCTIONAL_H
