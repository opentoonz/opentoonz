#pragma once

#ifndef TCG_SFINAE_H
#define TCG_SFINAE_H

/*!
  \file     sfinae.h

  \brief    Contains template metafunctions that can be used to enable or
            disable template class members.

  \details  SFINAE (Substitution Failure Is Not An Error) is a C++ \a feature
            that allows the compiler to silently discard failures in template
            function instantiations during function overload resolution.
*/

#if defined(__APPLE_CC__)
#include <type_traits>
#endif

namespace tcg {

template <typename X, typename Y>
struct type_match {
  enum { value = false };
};

template <typename X>
struct type_match<X, X> {
  enum { value = true };
};

//------------------------------------------------------------------------

template <typename X, typename Y>
struct type_mismatch {
  enum { value = true };
};

template <typename X>
struct type_mismatch<X, X> {
  enum { value = false };
};

//========================================================================

template <typename T, typename B>
struct enable_if_exists {
  typedef B type;
};

//========================================================================

template <bool, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> {
  typedef T type;
};

//========================================================================

template <bool, typename T = void>
struct disable_if {
  typedef T type;
};

template <typename T>
struct disable_if<true, T> {};

//========================================================================

template <bool, typename TrueT, typename FalseT = void>
struct choose_if;

template <typename TrueT, typename FalseT>
struct choose_if<true, TrueT, FalseT> {
  typedef TrueT type;
};

template <typename TrueT, typename FalseT>
struct choose_if<false, TrueT, FalseT> {
  typedef FalseT type;
};

//========================================================================

template <typename T, typename MatchT, typename NotMatchedT = void>
struct choose_if_match {
  typedef NotMatchedT type;
};

template <typename MatchT, typename NotMatchedT>
struct choose_if_match<MatchT, MatchT, NotMatchedT> {
  typedef MatchT type;
};

}  // namespace tcg

#endif  // TCG_SFINAE_H
