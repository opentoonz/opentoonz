#pragma once

#ifndef TCG_POINT_H
#define TCG_POINT_H

//*************************************************************
//    tcg Generic Point Class
//*************************************************************

namespace tcg {

/*!
  The Point class models a point in a bidimensional vector space. It has 2
  members,
  x and y, representing its coordinates, and a constructor prototype:

    Point_class(const value_type& x, const value_type& y);
*/
template <typename T>
struct PointT {
  typedef T value_type;
  value_type x, y;

  PointT() : x(0), y(0) {}
  PointT(const value_type &x_, const value_type &y_) : x(x_), y(y_) {}

  bool operator==(const PointT &other) const {
    return (x == other.x) && (y == other.y);
  }
  bool operator!=(const PointT &other) const { return !operator==(other); }
};

//******************************************************************************
//    Common typedefs
//******************************************************************************

typedef PointT<int> Point;
typedef PointT<int> PointI;
typedef PointT<double> PointD;

//*************************************************************
//    tcg Generic Point Traits
//*************************************************************

template <typename P>
struct point_traits {
  typedef P point_type;
  typedef typename P::value_type value_type;
  typedef typename P::value_type float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

template <>
struct point_traits<PointI> {
  typedef PointI point_type;
  typedef PointI::value_type value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

}  // namespace tcg

#endif  // TCG_POINT_H
