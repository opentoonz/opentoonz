#pragma once

#ifndef TCG_NUMERIC_OPS_H
#define TCG_NUMERIC_OPS_H

// tcg includes
#include "traits.h"
#include "macros.h"

// STD includes
#include <cmath>
#include <limits>

/*!
  \file     tcg_numeric_ops.h

  \brief    This file contains small function snippets to manipulate scalars
            or numerical objects.
*/

//***************************************************************************
//    Numerical Operations
//***************************************************************************

namespace tcg {

/*!
  Contains small function snippets to manipulate scalars
  or numerical objects.
*/
namespace numeric_ops {

template <typename Scalar>
// inline Scalar NaN() {return (std::numeric_limits<Scalar>::quiet_NaN)();}
inline Scalar NaN() {
  return (std::numeric_limits<Scalar>::max)();
}

//-------------------------------------------------------------------------------------------

/*!
  Calculates the sign of a scalar.
  \remark  This function returns the \a integral values \p 1 in case the input
  value is
           \a positive, \p -1 in case it's negative, and \p 0 in case it's \a
  exactly \p 0.
  \return  The sign of the input scalar.
*/
template <typename Scalar>
inline int sign(Scalar val, Scalar tol = 0) {
  return (val > tol) ? 1 : (val < -tol) ? -1 : 0;
}

//-------------------------------------------------------------------------------------------

//! Computes the nearest integer not greater in magnitude than the given value
template <typename Scalar>
Scalar trunc(Scalar val) {
  return (val < 0) ? std::ceil(val) : std::floor(val);
}

//! Computes the nearest integer \b strictly not greater in magnitude than the
//! given value
template <typename Scalar>
Scalar truncStrict(Scalar val) {
  return (val < 0) ? std::floor(val + 1) : std::ceil(val - 1);
}

//! Computes the nearest integer not lesser in magnitude than the given value
template <typename Scalar>
Scalar grow(Scalar val) {
  return (val < 0) ? std::floor(val) : std::ceil(val);
}

//! Computes the nearest integer \b strictly not lesser in magnitude than the
//! given value
template <typename Scalar>
Scalar growStrict(Scalar val) {
  return (val < 0) ? std::ceil(val - 1) : std::floor(val + 1);
}

//-------------------------------------------------------------------------------------------

template <typename Scalar>
inline bool areNear(Scalar a, Scalar b, Scalar tolerance) {
  return (std::abs(b - a) < tolerance);
}

//-------------------------------------------------------------------------------------------

template <typename Scalar>
inline typename std::enable_if<!tcg::is_floating_point<Scalar>::value,
                                Scalar>::type
mod(Scalar val, Scalar mod) {
  Scalar m = val % mod;
  return (m >= 0) ? m : m + mod;
}

template <typename Scalar>
inline
    typename std::enable_if<tcg::is_floating_point<Scalar>::value, Scalar>::type
    mod(Scalar val, Scalar mod) {
  Scalar m = fmod(val, mod);
  return (m >= 0) ? m : m + mod;
}

//-------------------------------------------------------------------------------------------

template <typename Scalar>
inline Scalar mod(Scalar val, Scalar a, Scalar b) {
  return a + mod(val - a, b - a);
}

//-------------------------------------------------------------------------------------------

//! Returns the modular shift value with minimal abs:  <TT>mod(val2, m) =
//! mod(val1 + shift, m)</TT>
template <typename Scalar>
inline Scalar modShift(Scalar val1, Scalar val2, Scalar m) {
  Scalar shift1 = mod(val2 - val1, m), shift2 = m - shift1;
  return (shift2 < shift1) ? -shift2 : shift1;
}

//-------------------------------------------------------------------------------------------

//! Returns the modular shift value with minimal abs:  <TT>mod(val2, a, b) =
//! mod(val1 + shift, a, b)</TT>
template <typename Scalar>
inline Scalar modShift(Scalar val1, Scalar val2, Scalar a, Scalar b) {
  return modShift(val1 - a, val2 - a, b - a);
}

//-------------------------------------------------------------------------------------------

/*!
  Returns the \a quotient associated to the remainder calculated with mod().
  \return  Integral quotient of the division of \p val by \p d.
*/
template <typename Scalar>
inline typename std::enable_if<!tcg::is_floating_point<Scalar>::value,
                                Scalar>::type
div(Scalar val, Scalar d) {
  TCG_STATIC_ASSERT(-3 / 5 == 0);
  TCG_STATIC_ASSERT(3 / -5 == 0);

  return (val < 0 || d < 0) ? (val / d) - 1 : val / d;
}

/*!
  Returns the \a quotient associated to the remainder calculated with mod().
  \return  Integral quotient of the division of \p val by \p d.
*/
template <typename Scalar>
inline
    typename std::enable_if<tcg::is_floating_point<Scalar>::value, Scalar>::type
    div(Scalar val, Scalar d) {
  return std::floor(val / d);
}

//-------------------------------------------------------------------------------------------

/*!
  Returns the \a quotient associated to the remainder calculated with mod().
  \return  Integral quotient of the division of \p val by <TT>(b-a)</TT>.
*/
template <typename Scalar>
inline Scalar div(Scalar val, Scalar a, Scalar b) {
  return div(val - a, b - a);
}

//-------------------------------------------------------------------------------------------

//! Linear interpolation of values \p v0 and \p v1 with parameter \p t.
template <typename T, typename Scalar>
inline T lerp(const T &v0, const T &v1, Scalar t) {
  return (1 - t) * v0 + t * v1;
}

//-------------------------------------------------------------------------------------------

/*!
  \brief Computes the second degree Bezier curve of control points \p c0, \p c1
  and \p c2
         at parameter \p t.
*/
template <typename T, typename Scalar>
inline T bezier(const T &c0, const T &c1, const T &c2, Scalar t) {
  Scalar one_t = 1 - t, t_one_t = t * one_t;  // 3  Scalar-Scalar products
  return (one_t * one_t) * c0 + (t_one_t + t_one_t) * c1 +
         (t * t) * c2;  // 3  Scalar-T products

  // return (c0 * one_t + c1 * t) * one_t + (c1 * one_t + c2 * t) * t;
  // // 6 Scalar-T products
}

//-------------------------------------------------------------------------------------------

//! \deprecated
template <typename UScalar>
inline UScalar GE_2Power(UScalar val) {
  if (!val) return 0;
  --val;

  UScalar i;
  for (i = 0; val; ++i) val = val >> 1;

  return 1 << i;
}
}
}  // namespace tcg::numeric_ops

#endif  // TCG_NUMERIC_OPS_H
