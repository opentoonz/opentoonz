#pragma once

#ifndef TZEROFINDER_H
#define TZEROFINDER_H

#include <cmath>
#include <assert.h>

enum { FZ_OK = 0, FZ_FTOL = 1, FZ_XTOL = 2, FZ_MAX_ITER = 3, FZ_NOT_ZERO = 4 };

/*

//Daniele
//  Use the one at the end instead. This may overshoot - plus, it does not
//  bisecate the interval... (I suppose it has never been used much :) )

template< class  unaryOp        ,
          class  unaryOpDerivate>
bool  findZero_Newton ( double          x0      ,
                        double          x1      ,
                        unaryOp         f       ,
                        unaryOpDerivate fd1     ,
                        double          xtol    ,
                        double          ftol    ,
                        int             maxIter ,
                        double          &x      ,
                        int             &err    )
{
  double  f_x_n_plus_1  = 0.0   ;
  double  f_x_n         = f(x1) ;
  double  f_x_n_minus_1 = f(x0) ;

  if( f_x_n*f_x_n_minus_1 > 0 )
  {
    err = FZ_NOT_ZERO;
    return  false;
  }

  int i;
  err=FZ_OK;

  if( f_x_n         == 0  ||
      f_x_n_minus_1 == 0    )
  {
    x   = (f_x_n == 0.0) ? x1 : x0;
    err = FZ_OK;
    return true;
  }

  double  x_n_plus_1  ;
  double  x_n         ;
  double  x_n_minus_1 ;

  x_n         = x1;
  x_n_minus_1 = x0;

  for(i=0;i<maxIter;++i)
  {
    // the firts time x_n = x1
    double  fd1_x_n = fd1(x_n);
    assert( fd1_x_n != 0.0 );

    x_n_plus_1 = x_n - f_x_n / fd1_x_n;
    if( fabs(x_n - x_n_plus_1) < xtol )
    {
      err=FZ_XTOL;
      break;
    }

    f_x_n_plus_1 = f(x_n_plus_1);
    if( fabs(f_x_n_plus_1) < ftol )
    {
      err=FZ_FTOL;
      break;
    }

    x_n_minus_1   = x_n;
    x_n           = x_n_plus_1;
    f_x_n         = f_x_n_plus_1;
  }

  x=x_n_plus_1;
  err = (i == maxIter ? FZ_MAX_ITER : err);
  return true;
}
*/

//-----------------------------------------------------------------------------

/*!

  Bisection algorithm.
  Get an object and a method of object class.
  x1 and x2 are extremes of test and xtol
  is the accuracy.

    Return:
    the minimum or,
    -2  if min it's unreacheable.
    -1  if min does not exist
    Remark:
    Max iterations numbers is fixed to 100 in in maxIter.
    This value fix the accuracy to 2e-100.

      From Numerical Recipes.
*/

template <class unaryOp>
bool findZero_bisection(double x0, double x1, unaryOp f, double xtol,
                        int maxIter, double &x, int &err) {
  int i;
  err = FZ_OK;
  double dx, fx0, fmid, xmid, rtb;

  fx0  = f(x0);
  fmid = f(x1);

  if (fx0 * fmid > 0.0) {
    err = FZ_NOT_ZERO;
    return false;
  }

  if (fx0 == 0 || fmid == 0) {
    x   = (fmid == 0.0) ? x1 : x0;
    err = FZ_OK;
    return true;
  }

  rtb = fx0 < 0.0 ? (dx = x1 - x0, x0) : (dx = x0 - x1, x1);

  for (i = 1; i <= maxIter; i++) {
    fmid = f(xmid        = rtb + (dx *= 0.5));
    if (fmid <= 0.0) rtb = xmid;
    if (fabs(dx) < xtol || fmid == 0.0) {
      x   = rtb;
      err = FZ_XTOL;
      break;
    }
  }
  err = (i == maxIter ? FZ_MAX_ITER : err);
  return true;
}

template <class unaryOp>
bool findZero_secant(double x0, double x1, unaryOp f, double xtol, double ftol,
                     int maxIter, double &x, int &err) {
  double f_x_n_plus_1 = 0.0, f_x_n = f(x1), f_x_n_minus_1 = f(x0);

  if (f_x_n * f_x_n_minus_1 > 0) {
    err = FZ_NOT_ZERO;
    return false;
  }

  int i;
  err = FZ_OK;

  if (f_x_n == 0 || f_x_n_minus_1 == 0) {
    x = (f_x_n == 0.0) ? x1 : x0;
    return true;
  }

  double x_n_plus_1 = x1, x_n = x1, x_n_minus_1 = x0;

  for (i = 0; i < maxIter; ++i) {
    double den = f_x_n - f_x_n_minus_1;
    assert(den != 0.0);
    if (den == 0.0) {
      x_n = x_n_plus_1;
      err = FZ_FTOL;
      break;
    }

    x_n_plus_1 = x_n - (x_n - x_n_minus_1) * f_x_n / den;
    if (fabs(x_n - x_n_plus_1) < xtol) {
      err = FZ_XTOL;
      break;
    }

    f_x_n_plus_1 = f(x_n_plus_1);
    if (fabs(f_x_n_plus_1) < ftol) {
      err = FZ_FTOL;
      break;
    }

    x_n_minus_1   = x_n;
    x_n           = x_n_plus_1;
    f_x_n_minus_1 = f_x_n;
    f_x_n         = f_x_n_plus_1;
  }

  x   = x_n_plus_1;
  err = (i == maxIter ? FZ_MAX_ITER : err);
  return true;
}

template <class unaryOp1, class unaryOp2>
bool findZero_Newton(double x0, double x1, unaryOp1 f, unaryOp2 f1, double xtol,
                     double ftol, int maxIter, double &x, int &err) {
  double f_0   = f(x0);
  double f_1   = f(x1);
  double f_x_n = f_0, f_x_n_plus_1 = 0.0;

  bool s_0 = (f_0 > 0);
  bool s_1 = (f_1 > 0);
  bool s_n;

  if (s_0 && s_1) {
    err = FZ_NOT_ZERO;
    return false;
  }

  int i;
  err = FZ_OK;

  if (f_0 == 0 || f_1 == 0) {
    x = (f_1 == 0.0) ? x1 : x0;
    return true;
  }

  double x_n = x1, x_n_plus_1;

  for (i = 0; i < maxIter; ++i) {
    double den                 = f1(x_n);
    if (den > 1e-2) x_n_plus_1 = x_n - f_x_n / den;

    if (den <= 1e-2 || x0 > x_n_plus_1 || x1 < x_n_plus_1)
      x_n_plus_1 = 0.5 * (x0 + x1);

    if (fabs(x_n - x_n_plus_1) < xtol) {
      err = FZ_XTOL;
      break;
    }

    f_x_n_plus_1 = f(x_n_plus_1);
    if (fabs(f_x_n_plus_1) < ftol) {
      err = FZ_FTOL;
      break;
    }

    x_n   = x_n_plus_1;
    f_x_n = f_x_n_plus_1;
    s_n   = (f_x_n > 0);

    if (s_n == s_0)
      x0 = x_n;
    else
      x1 = x_n;
  }

  x   = x_n_plus_1;
  err = (i == maxIter ? FZ_MAX_ITER : err);
  return true;
}

#endif
