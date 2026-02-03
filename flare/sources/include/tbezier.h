#pragma once

//-----------------------------------------------------------------------------
//  tbezier.h: contains useful function to manipulate bezier polynomious
//-----------------------------------------------------------------------------
#ifndef TBEZIER_H
#define TBEZIER_H
#include <iterator>

//-----------------------------------------------------------------------------

/*!
  Compute the forward differences.
 */
template <class T>
void forwardDifferences(const std::vector<T> &coeff, std::vector<T> &diffs) {
  diffs.clear();

  std::copy(coeff.begin(), coeff.end(), std::back_inserter(diffs));

  typename std::vector<T>::iterator vectIt;

  const UINT degree = coeff.size();

  for (UINT r = 0; r < degree; ++r) {
    vectIt = diffs.begin();
    std::advance(vectIt, r);
    std::adjacent_difference(vectIt, diffs.end(), vectIt);
  }
}

//-----------------------------------------------------------------------------

/*!
  Converts Bezier form to power (polynomial) form.
  \par  bez coefficients in Bezier form.
  \ret  coeff coefficients of power form.
  \note coeffs are ordered from smaller b[0] to greater b[size-1] .
 */
template <class T>
void bezier2poly(const std::vector<T> &bez, std::vector<T> &coeff) {
  forwardDifferences(bez, coeff);  // compute forward differences
                                   // and store them in coeff.
  int degree = bez.size() - 1;

  double i_factorial = 1, n_r = 1;

  coeff[0] = bez[0];

  for (int i = 1; i < degree; i++) {
    i_factorial = i_factorial * i;
    n_r         = n_r * (degree - i + 1);
    i_factorial = 1.0 / i_factorial;
    coeff[i]    = (n_r * i_factorial) * coeff[i];
  }
}

//-----------------------------------------------------------------------------

/*!
  Converts power (polynomial) form to Bezier form.
  \par  coeff coefficients of power form.
  \ret  bez coefficients in Bezier form.
  \note coeffs are ordered from smaller b[0] to greater b[size-1].
 */
template <class T>
void poly2bezier(const std::vector<T> &poly, std::vector<T> &bez) {
  UINT n = poly.size();

  bez.clear();
  std::copy(poly.begin(), poly.end(), std::back_inserter(bez));

  double c, d, e;

  for (UINT j = 1; j < n; j++) {
    c = 1.0 / (n - j);
    d = 1;
    e = c;
    for (UINT i = n - 1; i >= j; i--) {
      bez[i] = d * bez[i] + e * bez[i - 1];
      d -= c;
      e += c;
    }
  }
}

#endif  // TBEZIER_H
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
