#pragma once

#ifndef TCG_WRAP_H
#define TCG_WRAP_H

// tcg includes
#include "tcg/tcg_point.h"

// Toonz includes
#include "tgeometry.h"

//*************************************************************************************
//    Toonz wrapper for tcg
//*************************************************************************************

namespace tcg {

template <>
struct point_traits<TPoint> {
  typedef TPoint point_type;
  typedef int value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

template <>
struct point_traits<TPointD> {
  typedef TPointD point_type;
  typedef double value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

template <>
struct point_traits<TThickPoint> {
  typedef TThickPoint point_type;
  typedef double value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

}  // namespace tcg

#endif  // TCG_WRAP_H
