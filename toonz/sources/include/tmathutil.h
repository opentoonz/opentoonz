#pragma once

#ifndef TMATH_UTIL_H
#define TMATH_UTIL_H

//#include "tcommon.h"
#include "texception.h"

#include <numeric>

#ifndef __sgi
#include <cmath>
#else
#include <math.h>
#endif

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

enum TMathError { INFINITE_SOLUTIONS = -1 };

//-----------------------------------------------------------------------------

class DVAPI TMathException final : public TException {
  TString m_msg;

public:
  TMathException(std::string msg);
  virtual ~TMathException() {}

  TString getMessage() const override { return m_msg; }
};

//-----------------------------------------------------------------------------

/*!
  Decompose a square matrix [A] dim(A)=[n x n]  using the LU method.
  A is a matrix stored for row.
  \par A array of coeff
  \par n size of matrix
  \par indx array with permutation from pivoting
  \par d +1/-1 and depend wheter the number of row interchanges (even or odd)
  \note If error a TException is thrown.
  \include mathutils_ex2.cpp
 */
DVAPI void tLUDecomposition(double *A, int n, int *indx, double &d);

//-----------------------------------------------------------------------------

/*!
  Use the back-substitution method to solve the linear
  system A*x=b.
  \par A matrix exited from tLUDecomposition
  \par n size of matrix
  \par indx array exited from tLUDecomposition
  \par b in input is array of element
  \ret b in output contains result
  \note A is the square matrix [n x n] returned from tLUDecomposition routine.
  \sa tLUDecomposition
 */
DVAPI void tbackSubstitution(double *A, int n, int *indx, double *b);

//-----------------------------------------------------------------------------

/*!
  Solve the system A*x=b.
  It`s needed to insert b in res.
  A and res are modifyed in the routine.
  \par A matrix
  \par n size of matrix
  \note A is square [n x n]
  \include mathutils_ex1.cpp
 */
DVAPI void tsolveSistem(double *A, int n, double *res);

//-----------------------------------------------------------------------------

/*!
  Solve the system A*x=b.
  It`s needed to insert b in res.
  A and res are modifyed in the routine.
  \note A is square [n x n]
  \include mathutils_ex1.cpp
 */
inline void tsolveSistem(std::vector<double> &A, std::vector<double> &res) {
  assert(res.size() * res.size() == A.size());
  tsolveSistem(&A[0], res.size(), &res[0]);
}

//-----------------------------------------------------------------------------

/*!
  Find determinant of square matrix A
  \par A is matrix to test
  \par n size of matrix
 */
DVAPI double tdet(double *A, int n);

//-----------------------------------------------------------------------------

/*!
  Find determinant of square matrix A [n x n], using information
  from tLUDecomposition decomposition.
  \par LUa output matrix from tLUDecomposition
  \par n size of matrix
  \par d parameter from tLUDecomposition
  \sa tLUDecomposition
 */
DVAPI double tdet(double *LUa, int n, double d);

//-----------------------------------------------------------------------------

/*!
  Find real root of a polynomious
  \par poly is a vector with coeff of polynomious in crescent order.
  \ret sol is the vector of solution and contain real found solution.
 */
DVAPI int rootFinding(const std::vector<double> &poly,
                      std::vector<double> &sol);

//-----------------------------------------------------------------------------

/*!
  Check if val is nearest to epsilon.
  \par val value to test
  \par eps tolerance required
  \ret true if value is nearest to zero
 */
inline bool isAlmostZero(double val, double eps = TConsts::epsilon) {
  return -eps < val && val < eps;
}

//-----------------------------------------------------------------------------

/*!
  Find number of roots of a poly in interval min max.
  \par order degree of polynomious
  \par coeff of poly in crescent order
  \par lesser extreme
  \par greater extreme
  \ret number of root
 */
DVAPI int numberOfRootsInInterval(int order, const double *polyH, double min,
                                  double max);

//-----------------------------------------------------------------------------

/*!
  Find the real root of the poly a*x^3+b*x^2+c*x+d in [0,1]
  \par a coeff of x^3
  \par b coeff of x^2
  \par c coeff of x^1
  \par d coeff of x^0
  \ret the real root of the poly, or 1.0 if the root is not in [0,1]
*/
DVAPI double cubicRoot(double a, double b, double c, double d);

//-----------------------------------------------------------------------------

/*!
  Find the root of the poly a*x^2+b*x+c
  \par a coeff of x^2
  \par b coeff of x^1
  \par c coeff of x^0
  \ret the root of the poly, or 1.0 if it does not exist
*/
DVAPI double quadraticRoot(double a, double b, double c);

//-----------------------------------------------------------------------------

#endif  // #ifndef TMATH_UTIL_H
//-----------------------------------------------------------------------------
// End Of File
//-----------------------------------------------------------------------------
