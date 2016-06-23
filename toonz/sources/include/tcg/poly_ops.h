#pragma once

#ifndef TCG_POLY_OPS_H
#define TCG_POLY_OPS_H

// tcg includes
#include "macros.h"

/*!
  \file     tcg_poly_ops.h

  \brief    This file contains useful functions for operating with polynomials.
*/

//*******************************************************************************
//    Polynomial Operations
//*******************************************************************************

namespace tcg {

//! Contains useful functions for operating with polynomials.
namespace poly_ops {

/*!
  \brief    Evaluates a polynomial using Horner's algorithm
  \return   The value of the input polynomial at the specified parameter
*/
template <typename Scalar>
Scalar evaluate(const Scalar poly[],  //!< Coefficients of the input polynomial,
                                      //! indexed by degree
                int deg,              //!< Degree of the polynomial function
                Scalar x)  //!< Parameter the polynomial will be evaluated on
{
  // ((poly[deg] * x) + poly[deg-1]) * x + poly[deg - 2] + ...

  Scalar value = poly[deg];
  for (int d = deg - 1; d >= 0; --d) value = value * x + poly[d];

  return value;
}

//-------------------------------------------------------------------------------------------

/*!
  \brief  Reduces the degree of the input polynomial, discarding all leading
  coefficients
          whose absolute value is below the specified tolerance threshold.
*/
template <typename Scalar>
void reduceDegree(
    const Scalar poly[],  //!< Input polynomial to be reduced.
    int &deg,             //!< Input/output polynomial degree.
    Scalar tolerance  //!< Coefficients threshold to reduce the polynomial with.
    ) {
  while ((deg > 0) && (std::abs(poly[deg]) < tolerance)) --deg;
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Adds two polynomials and returns the sum.
  \remark   The supplied polynomials can actually be the same.
*/
template <typename A, typename B, typename C, int deg>
void add(const A (&poly1)[deg],  //!< First polynomial addendum.
         const B (&poly2)[deg],  //!< Second polynomial addendum.
         C (&result)[deg])       //!< Resulting sum.
{
  for (int d = 0; d != deg; ++d) result[d] = poly1[d] + poly2[d];
}

//-------------------------------------------------------------------------------------------

/*!
  \brief  Subtracts two polynomials /p poly1 and \p poly2 and returns the
          difference <TT>poly1 - poly2</TT>.
*/
template <typename A, typename B, typename C, int deg>
void sub(const A (&poly1)[deg],  //!< First polynomial addendum.
         const B (&poly2)[deg],  //!< Second polynomial addendum.
         C (&result)[deg])       //!< Resulting difference.
{
  for (int d = 0; d != deg; ++d) result[d] = poly1[d] - poly2[d];
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Multiplies two polynomials into a polynomial whose degree is the
            \a sum of the multiplicands' degrees.

  \warning  The resulting polynomial is currently <B>not allowed</B> to be one
            of the multiplicands.
*/
template <typename A, typename B, typename C, int deg1, int deg2, int degR>
void mul(const A (&poly1)[deg1],  //!< First multiplicand.
         const B (&poly2)[deg2],  //!< Second multiplicand.
         C (&result)[degR])       //!< Resulting polynomial.
{
  TCG_STATIC_ASSERT(degR == deg1 + deg2 - 1);

  for (int a = 0; a != deg1; ++a) {
    for (int b = 0; b != deg2; ++b) result[a + b] += poly1[a] * poly2[b];
  }
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Calculates the chaining <TT>poly1 o poly2</TT> of two given
  polynomial
            \p poly1 and \p poly2.

  \warning  The resulting polynomial is currently <B>not allowed</B> to be one
            of the multiplicands.
  \warning  This function is still \b untested.
*/
template <typename A, typename B, typename C, int deg1, int deg2, int degR>
void chain(const A (&poly1)[deg1],  //!< First polynomial.
           const B (&poly2)[deg2],  //!< Second polynomial.
           C (&result)[degR])       //!< Resulting polynomial.
{
  for (int a = 0; a != deg1; ++a) {
    B pow[degR][2] = {{}};

    // Build poly2 powers
    {
      std::copy(poly2, poly2 + deg2, pow[1]);

      for (int p = 1; p < a; ++p) poly_mul(pow[p % 2], poly2, pow[(p + 1) % 2]);
    }

    B(&pow_add)[degR] = pow[a % 2];

    // multiply by poly1[a]
    C addendum[degR];

    for (int c = 0; c != degR; ++c) addendum[c] = poly1[c] * pow_add[c];

    poly_add(addendum, result);
  }
}

//-------------------------------------------------------------------------------------------

template <typename A, typename B, int deg, int degR>
void derivative(const A (&poly)[deg],  //!< Polynomial to be derived.
                B (&result)[degR])     //!< Resulting derivative polynomial.
{
  TCG_STATIC_ASSERT(degR == deg - 1);

  for (int c = 1; c != deg; ++c) result[c - 1] = c * poly[c];
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Solves the 1st degree equation: $c[1] t + c[0] = 0$
  \return   The number of solutions found under the specified divide-by
  tolerance
*/
template <typename Scalar>
inline unsigned int solve_1(
    Scalar c[2],  //!< Polynomial coefficients array
    Scalar s[1],  //!< Solutions array
    Scalar tol =
        0)  //!< Leading coefficient tolerance, the equation has no solution
            //!  if the leading coefficient is below this threshold
{
  if (std::abs(c[1]) <= tol) return 0;

  s[0] = -c[0] / c[1];
  return 1;
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Solves the 2nd degree equation: $c[2] t^2 + c[1] t + c[0] = 0$
  \return   The number of #real# solutions found under the specified divide-by
  tolerance

  \remark   The returned solutions are sorted, with $s[0] <= s[1]$
*/
template <typename Scalar>
unsigned int solve_2(
    Scalar c[3],  //!< Polynomial coefficients array
    Scalar s[2],  //!< Solutions array
    Scalar tol =
        0)  //!< Leading coefficient tolerance, the equation has no solution
            //!  if the leading coefficient is below this threshold
{
  if (std::abs(c[2]) <= tol)
    return solve_1(c, s, tol);  // Reduces to first degree

  Scalar nc[2] = {
      c[0] / c[2],
      c[1] / (c[2] + c[2])};  // NOTE: nc[1] gets further divided by 2

  Scalar delta = nc[1] * nc[1] - nc[0];
  if (delta < 0) return 0;

  delta = sqrt(delta);

  s[0] = -delta - nc[1];
  s[1] = delta - nc[1];

  return 2;
}

//-------------------------------------------------------------------------------------------

/*!
  \brief    Solves the 3rd degree equation: $c[3]t^3 + c[2] t^2 + c[1] t + c[0]
  = 0$
  \return   The number of #real# solutions found under the specified divide-by
  tolerance

  \remark   The returned solutions are sorted, with $s[0] <= s[1] <= s[2]$
*/
template <typename Scalar>
unsigned int solve_3(Scalar c[4],     //!< Polynomial coefficients array
                     Scalar s[3],     //!< Solutions array
                     Scalar tol = 0)  //!< Leading coefficient tolerance, the
                                      //! equation is reduced to 2nd degree
//!  if the leading coefficient is below this threshold
{
  if (std::abs(c[3]) <= tol)
    return solve_2(c, s, tol);  // Reduces to second degree

  Scalar nc[3] = {c[0] / c[3], c[1] / c[3], c[2] / c[3]};

  Scalar b2     = nc[2] * nc[2];
  Scalar p      = nc[1] - b2 / 3;
  Scalar q      = nc[2] * (b2 + b2 - 9 * nc[1]) / 27 + nc[0];
  Scalar p3     = p * p * p;
  Scalar d      = q * q + 4 * p3 / 27;
  Scalar offset = -nc[2] / 3;

  if (d >= 0) {
    // Single solution
    Scalar z = sqrt(d);
    Scalar u = (-q + z) / 2;
    Scalar v = (-q - z) / 2;

    u = (u < 0) ? -pow(-u, 1 / Scalar(3)) : pow(u, 1 / Scalar(3));
    v = (v < 0) ? -pow(-v, 1 / Scalar(3)) : pow(v, 1 / Scalar(3));

    s[0] = offset + u + v;
    return 1;
  }

  assert(p3 < 0);

  Scalar u = sqrt(-p / Scalar(3));
  Scalar v = acos(-sqrt(-27 / p3) * q / Scalar(2)) / Scalar(3);
  Scalar m = cos(v), n = sin(v) * 1.7320508075688772935274463415059;  // sqrt(3)

  s[0] = offset - u * (n + m);
  s[1] = offset + u * (n - m);
  s[2] = offset + u * (m + m);

  assert(s[0] <= s[1] && s[1] <= s[2]);

  return 3;
}
}
}  // namespace tcg::poly_ops

#endif  // TCG_POLY_OPS_H
