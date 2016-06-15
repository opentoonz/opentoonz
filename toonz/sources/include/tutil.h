#pragma once

#ifndef TUTIL_INCLUDED
#define TUTIL_INCLUDED

#include "tcommon.h"
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#endif

//! Type definition for a pair of double.
typedef std::pair<double, double> DoublePair;

//! Type definition for a pair of integer.
typedef std::pair<int, int> IntPair;

//! Square of x.
/*!
  \par x val to square
 */
template <class T>
inline T sq(T x) {
  return x * x;
}

//! Calculates the floor of a value.
/*!
  Return the largest integer that is less than or equal to x.
  \par x val to floor
  \ret largest integer that is less than or equal to x
 */
inline int tfloor(double x) { return ((int)(x) > (x) ? (int)(x)-1 : (int)(x)); }

//! Calculates the ceiling of a value.
/*!
  Return the smallest integer that is greater than or equal to x.
  \par x val to floor
  \ret smallest integer that is greater than or equal to x.
 */
inline int tceil(double x) {
  return ((int)(x) < (x) ? (int)(x) + 1 : (int)(x));
}

//! Check if value is an integer.
/*!
  Return largest integer that is less than or equal to x
  \par x val to test
  \ret true if val is integer
 */
inline bool isInt(double x) { return (int)(x) == (x); }

inline int tfloor(int x, int step) {
  return step * (x >= 0 ? (x / step) : -((-1 - x + step) / step));
}

inline int tceil(int x, int step) {
  return step * (x >= 0 ? ((x + step - 1) / step) : -((-x) / step));
}

inline int intLE(double x) { return tfloor(x); }

inline int intGT(double x) { return tfloor(x) + 1; }

inline int intLT(double x) { return tceil(x) - 1; }

inline int intGE(double x) { return tceil(x); }

//! convert radiant to degree
/*!
  Convert an angle from radiant to angle.
  \par angle in radiant
  \ret angle in degree
 */
inline double rad2degree(double rad) { return rad * M_180_PI; }

//! convert degree to radiant
/*!
  Convert an angle from degree to radiant.
  \par angle in degree
  \ret angle in radiant
 */
inline double degree2rad(double degree) { return degree * M_PI_180; }

//! Sign of argument.
/*!
 Return sign of argument.
 \par arg value to test
 \ret -1 if arg is negative, 1 if arg is positive, 0 if arg is zero
 */
template <class T>
inline int tsign(T arg) {
  return arg < 0 ? -1 : arg > 0 ? 1 : 0;
}

//! Check if two values are very similar.
/*!
  Check if two values are very similar.
  \par a first value
  \par b second value
  \par err max distance from value
  \ret bool if value are very similar.
 */
inline bool areAlmostEqual(double a, double b, double err = TConsts::epsilon) {
  return fabs(a - b) < err;
}

//! Check if two values are very similar.
/*!
  Check if two values are very similar.
  \par a first value
  \par b second value
  \par err max distance from value
  \ret bool if value are very similar.
 */
template <class T>
inline bool areAlmostEqual(const T &a, const T &b,
                           double err = TConsts::epsilon) {
  return tdistance(a, b) < err;
}

struct TDeleteObjectFunctor {
  template <typename T>
  void operator()(T *ptr) {
    delete ptr;
  }
};

//! Clear a container deleting all elements.
/*!
  Clear a container, but before recall delete for all elements.
  \par c container
  \note the code doesn't work with map because it's impossible
        to deduce template
 */
template <class T>
inline void clearPointerContainer(T &c) throw() {
  T tmp;
  std::for_each(c.begin(), c.end(), TDeleteObjectFunctor());
  c.swap(tmp);
}

#endif
