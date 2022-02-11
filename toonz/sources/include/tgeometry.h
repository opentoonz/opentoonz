#pragma once

#ifndef T_GEOMETRY_INCLUDED
#define T_GEOMETRY_INCLUDED

#include "tutil.h"
#include <cmath>

#undef DVAPI
#undef DVVAR
#ifdef TGEOMETRY_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
/*
* This is an example of how to use the TPointT, the TRectT and the TAffine
* classes.
*/
/*!  The template class TPointT defines the x- and y-coordinates of a point.
*/
template <class T>
class TPointT {
public:
  T x, y;

  TPointT() : x(0), y(0){};
  TPointT(T _x, T _y) : x(_x), y(_y){};
  TPointT(const TPointT &point) : x(point.x), y(point.y){};
  inline TPointT &operator=(const TPointT &a) {
    x = a.x;
    y = a.y;
    return *this;
  };

  inline TPointT &operator+=(const TPointT &a) {
    x += a.x;
    y += a.y;
    return *this;
  };
  inline TPointT &operator-=(const TPointT &a) {
    x -= a.x;
    y -= a.y;
    return *this;
  };
  inline TPointT operator+(const TPointT &a) const {
    return TPointT(x + a.x, y + a.y);
  };
  inline TPointT operator-(const TPointT &a) const {
    return TPointT(x - a.x, y - a.y);
  };
  inline TPointT operator-() const { return TPointT(-x, -y); };

  bool operator!=(const TPointT &p) const { return x != p.x || y != p.y; }
};

/*! \relates TPointT
* Rotate a point 90 degrees (counterclockwise).
\param p a point.
\return the rotated point
\sa rotate270
*/
template <class T>
inline TPointT<T> rotate90(const TPointT<T> &p)  // 90 counterclockwise
{
  return TPointT<T>(-p.y, p.x);
}
/*! \relates TPointT
* Rotate a point 270 degrees (clockwise).
\param p a point.
\return the rotated point
\sa rotate90
*/
template <class T>
inline TPointT<T> rotate270(const TPointT<T> &p)  // 90 clockwise
{
  return TPointT<T>(p.y, -p.x);
}

/*!
\relates TPointT
*/
template <class T>  // Scalar(dot) Product
inline T operator*(const TPointT<T> &a, const TPointT<T> &b) {
  return a.x * b.x + a.y * b.y;
}

//-----------------------------------------------------------------------------

template <class T>
inline std::ostream &operator<<(std::ostream &out, const TPointT<T> &p) {
  return out << "(" << p.x << ", " << p.y << ")";
}

//-----------------------------------------------------------------------------

typedef TPointT<int> TPoint, TPointI;
typedef TPointT<double> TPointD;

#ifdef _WIN32
template class DVAPI TPointT<int>;
template class DVAPI TPointT<double>;
#endif

template <class T>
inline bool operator==(const TPointT<T> &p0, const TPointT<T> &p1) {
  return p0.x == p1.x && p0.y == p1.y;
}

//-----------------------------------------------------------------------------

//!\relates TPointT
inline TPoint operator*(int a, const TPoint &p) {
  return TPoint(a * p.x, a * p.y);
}

//!\relates TPointT
inline TPoint operator*(const TPoint &p, int a) {
  return TPoint(a * p.x, a * p.y);
}

//!\relates TPointT
inline TPointD operator*(double a, const TPointD &p) {
  return TPointD(a * p.x, a * p.y);
}

//!\relates TPointT
inline TPointD operator*(const TPointD &p, double a) {
  return TPointD(a * p.x, a * p.y);
}

//-----------------------------------------------------------------------------
/*!
\relates TPointT
This helper function returns the square of the absolute value of the specified
point (a TPointI)
*/
inline int norm2(const TPointI &p) { return p.x * p.x + p.y * p.y; }

//-----------------------------------------------------------------------------
/*!
\relates TPointT
This helper function returns the square of the absolute value of the specified
point (a TPointD)
*/
inline double norm2(const TPointD &p) { return p.x * p.x + p.y * p.y; }

/*!
\relates TPointT
This helper function returns the absolute value of the specified point
*/
inline double norm(const TPointD &p) { return std::sqrt(norm2(p)); }

/*!
\relates TPointT
This helper function returns the normalized version of the specified point
*/
inline TPointD normalize(const TPointD &p) {
  double n = norm(p);
  assert(n != 0.0);
  return (1.0 / n) * p;
}

/*!
\relates TPointT
This helper function converts a TPoint (TPointT<int>) into a TPointD
*/
inline TPointD convert(const TPoint &p) { return TPointD(p.x, p.y); }

/*!
\relates TPointT
This helper function converts a TPointD (TPointT<double>) into a TPoint
*/
inline TPoint convert(const TPointD &p) {
  return TPoint(tround(p.x), tround(p.y));
}

/*!
\relates TPointT
This helper function returns the square of the distance between two points
*/
inline double tdistance2(const TPointD &p1, const TPointD &p2) {
  return norm2(p2 - p1);
}

inline bool operator==(const TPointD &p0, const TPointD &p1) {
  return tdistance2(p0, p1) < TConsts::epsilon * TConsts::epsilon;
}

/*!
\relates TPointT
This helper function returns the distance between two points
*/
inline double tdistance(const TPointD &p1, const TPointD &p2) {
  return norm(p2 - p1);
}

/*!
the cross product
\relates TPointT
*/
inline double cross(const TPointD &a, const TPointD &b) {
  return a.x * b.y - a.y * b.x;
}

/*!
the cross product
\relates TPoint
*/
inline int cross(const TPoint &a, const TPoint &b) {
  return a.x * b.y - a.y * b.x;
}

/*!
returns the angle of the point p in polar coordinates
n.b atan(-y) = -pi/2, atan(x) = 0, atan(y) = pi/2, atan(-x) = pi
*/
inline double atan(const TPointD &p) { return atan2(p.y, p.x); }

//=============================================================================

template <class T>
class DVAPI T3DPointT {
public:
  T x, y, z;

  T3DPointT() : x(0), y(0), z(0) {}

  T3DPointT(T _x, T _y, T _z) : x(_x), y(_y), z(_z) {}
  T3DPointT(const TPointT<T> &_p, T _z) : x(_p.x), y(_p.y), z(_z) {}

  T3DPointT(const T3DPointT &_p) : x(_p.x), y(_p.y), z(_p.z) {}

  inline T3DPointT &operator=(const T3DPointT &a) {
    x = a.x;
    y = a.y;
    z = a.z;
    return *this;
  }

  inline T3DPointT &operator+=(const T3DPointT &a) {
    x += a.x;
    y += a.y;
    z += a.z;
    return *this;
  }

  inline T3DPointT &operator-=(const T3DPointT &a) {
    x -= a.x;
    y -= a.y;
    z -= a.z;
    return *this;
  }

  inline T3DPointT operator+(const T3DPointT &a) const {
    return T3DPointT(x + a.x, y + a.y, z + a.z);
  }

  inline T3DPointT operator-(const T3DPointT &a) const {
    return T3DPointT(x - a.x, y - a.y, z - a.z);
  }

  inline T3DPointT operator-() const { return T3DPointT(-x, -y, -z); }

  bool operator==(const T3DPointT &p) const {
    return x == p.x && y == p.y && z == p.z;
  }

  bool operator!=(const T3DPointT &p) const {
    return x != p.x || y != p.y || z != p.z;
  }
};

//=============================================================================

template <class T>
inline std::ostream &operator<<(std::ostream &out, const T3DPointT<T> &p) {
  return out << "(" << p.x << ", " << p.y << ", " << p.z << ")";
}

typedef T3DPointT<int> T3DPoint, T3DPointI;
typedef T3DPointT<double> T3DPointD;

#ifdef _WIN32
template class DVAPI T3DPointT<int>;
template class DVAPI T3DPointT<double>;
#endif

//-----------------------------------------------------------------------------

//!\relates T3DPointT
template <class T>
inline T3DPointT<T> operator*(T a, const T3DPointT<T> &p) {
  return T3DPointT<T>(a * p.x, a * p.y, a * p.z);
}

//!\relates TPointT
template <class T>
inline T3DPointT<T> operator*(const T3DPointT<T> &p, T a) {
  return T3DPointT<T>(a * p.x, a * p.y, a * p.z);
}

//-----------------------------------------------------------------------------
/*!
\relates TPointT
This helper function returns the square of the absolute value of the specified
point (a TPointI)
*/
template <class T>
inline T norm2(const T3DPointT<T> &p) {
  return p.x * p.x + p.y * p.y + p.z * p.z;
}

/*!
*/
template <class T>
inline T norm(const T3DPointT<T> &p) {
  return std::sqrt(norm2(p));
}

/*!
*/
inline T3DPointD normalize(const T3DPointD &p) {
  double n = norm(p);
  assert(n != 0.0);
  return (1.0 / n) * p;
}

/*!
*/
inline T3DPointD convert(const T3DPoint &p) { return T3DPointD(p.x, p.y, p.z); }

/*!
*/
inline T3DPoint convert(const T3DPointD &p) {
  return T3DPoint(tround(p.x), tround(p.y), tround(p.z));
}

//!
template <class T>
inline T tdistance(const T3DPointT<T> &p1, const T3DPointT<T> &p2) {
  return norm<T>(p2 - p1);
}

//!
template <class T>
inline T tdistance2(const T3DPointT<T> &p1, const T3DPointT<T> &p2) {
  return norm2<T>(p2 - p1);
}

//!
template <class T>
inline T3DPointT<T> cross(const T3DPointT<T> &a, const T3DPointT<T> &b) {
  return T3DPointT<T>(a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x,
                      a.x * b.y - b.x * a.y);
}
//=============================================================================
/*!
TThickPoint describe a thick point.
\relates TThickQuadratic, TThickCubic
*/
class DVAPI TThickPoint final : public TPointD {
public:
  double thick;

  TThickPoint() : TPointD(), thick(0) {}

  TThickPoint(double _x, double _y, double _thick = 0)
      : TPointD(_x, _y), thick(_thick) {}

  TThickPoint(const TPointD &_p, double _thick = 0)
      : TPointD(_p.x, _p.y), thick(_thick) {}

  TThickPoint(const T3DPointD &_p) : TPointD(_p.x, _p.y), thick(_p.z) {}

  TThickPoint(const TThickPoint &_p) : TPointD(_p.x, _p.y), thick(_p.thick) {}

  inline TThickPoint &operator=(const TThickPoint &a) {
    x     = a.x;
    y     = a.y;
    thick = a.thick;
    return *this;
  }

  inline TThickPoint &operator+=(const TThickPoint &a) {
    x += a.x;
    y += a.y;
    thick += a.thick;
    return *this;
  }

  inline TThickPoint &operator-=(const TThickPoint &a) {
    x -= a.x;
    y -= a.y;
    thick -= a.thick;
    return *this;
  }

  inline TThickPoint operator+(const TThickPoint &a) const {
    return TThickPoint(x + a.x, y + a.y, thick + a.thick);
  }

  inline TThickPoint operator-(const TThickPoint &a) const {
    return TThickPoint(x - a.x, y - a.y, thick - a.thick);
  }

  inline TThickPoint operator-() const { return TThickPoint(-x, -y, -thick); }

  bool operator==(const TThickPoint &p) const {
    return x == p.x && y == p.y && thick == p.thick;
  }

  bool operator!=(const TThickPoint &p) const {
    return x != p.x || y != p.y || thick != p.thick;
  }
};

inline double operator*(const TThickPoint &a, const TThickPoint &b) {
  return a.x * b.x + a.y * b.y + a.thick * b.thick;
}

inline TThickPoint operator*(double a, const TThickPoint &p) {
  return TThickPoint(a * p.x, a * p.y, a * p.thick);
}

inline TThickPoint operator*(const TThickPoint &p, double a) {
  return TThickPoint(a * p.x, a * p.y, a * p.thick);
}

/*!
\relates TPointD
This helper function converts a TThickPoint into a TPointD
*/
inline TPointD convert(const TThickPoint &p) { return TPointD(p.x, p.y); }

/*!
\relates TThickPoint
This helper function returns the square of the distance between two thick points
(only x and y are used)
*/
inline double tdistance2(const TThickPoint &p1, const TThickPoint &p2) {
  return norm2(convert(p2 - p1));
}
/*!
\relates TThickPoint
This helper function returns the distance between two thick  points
(only x and y are used)
*/
inline double tdistance(const TThickPoint &p1, const TThickPoint &p2) {
  return norm(convert(p2 - p1));
}

inline std::ostream &operator<<(std::ostream &out, const TThickPoint &p) {
  return out << "(" << p.x << ", " << p.y << ", " << p.thick << ")";
}

//=============================================================================
//!	This is a template class representing a generic vector in a plane, i.e.
//! a point.
/*!
                It is a data structure with two objects in it representing
   coordinate of the point and
                the basic operations on it.
        */
template <class T>
class DVAPI TDimensionT {
public:
  T lx, ly;
  /*!
          Constructs a vector of two elements, i.e. a point in a plane.
  */
  TDimensionT() : lx(), ly() {}
  TDimensionT(T _lx, T _ly) : lx(_lx), ly(_ly) {}
  /*!
          Copy constructor.
  */
  TDimensionT(const TDimensionT &d) : lx(d.lx), ly(d.ly) {}
  /*!
          Vector addition.
  */
  TDimensionT &operator+=(TDimensionT a) {
    lx += a.lx;
    ly += a.ly;
    return *this;
  }
  /*!
          Difference of two vectors.
  */
  TDimensionT &operator-=(TDimensionT a) {
    lx -= a.lx;
    ly -= a.ly;
    return *this;
  }
  /*!
          Addition of two vectors.
  */
  TDimensionT operator+(TDimensionT a) const {
    TDimensionT ris(*this);
    return ris += a;
  }
  /*!
          Vector difference.
  */
  TDimensionT operator-(TDimensionT a) const {
    TDimensionT ris(*this);
    return ris -= a;
  }
  /*!
          Compare vectors and returns \e true if are equals element by element.
  */
  bool operator==(const TDimensionT &d) const {
    return lx == d.lx && ly == d.ly;
  }
  /*!
          Compare vectors and returns \e true if are not equals element by
     element.
  */
  bool operator!=(const TDimensionT &d) const { return !operator==(d); }
};

//=============================================================================

typedef TDimensionT<int> TDimension, TDimensionI;
typedef TDimensionT<double> TDimensionD;

//=============================================================================

template <class T>
inline std::ostream &operator<<(std::ostream &out, const TDimensionT<T> &p) {
  return out << "(" << p.lx << ", " << p.ly << ")";
}

#ifdef _WIN32
template class DVAPI TDimensionT<int>;
template class DVAPI TDimensionT<double>;
#endif

//=============================================================================

//! Specifies the corners of a rectangle.
/*!
x0 specifies the x-coordinate of the bottom-left corner of a rectangle.
y0 specifies the y-coordinate of the bottom-left corner of a rectangle.
x1 specifies the x-coordinate of the upper-right corner of a rectangle.
y1 specifies the y-coordinate of the upper-right corner of a rectangle.
*/
template <class T>
class DVAPI TRectT {
public:
  /*! if x0>x1 || y0>y1 then rect is empty
if x0==y1 && y0==y1 and rect is a  TRectD then rect is empty */

  T x0, y0;
  T x1, y1;

  /*! makes an empty rect */
  TRectT();

  TRectT(T _x0, T _y0, T _x1, T _y1) : x0(_x0), y0(_y0), x1(_x1), y1(_y1){};

  TRectT(const TRectT &rect)
      : x0(rect.x0), y0(rect.y0), x1(rect.x1), y1(rect.y1){};

  TRectT(const TPointT<T> &p0, const TPointT<T> &p1)  // non importa l'ordine
      : x0(std::min((T)p0.x, (T)p1.x)),
        y0(std::min((T)p0.y, (T)p1.y)),
        x1(std::max((T)p0.x, (T)p1.x)),
        y1(std::max((T)p0.y, (T)p1.y)){};

  TRectT(const TPointT<T> &bottomLeft, const TDimensionT<T> &d);

  TRectT(const TDimensionT<T> &d);

  void empty();

  /*! TRectD is empty if and only if (x0>x1 || y0>y1) || (x0==y1 && y0==y1);
TRectI  is empty if x0>x1 || y0>y1 */
  bool isEmpty() const;

  T getLx() const;
  T getLy() const;

  TDimensionT<T> getSize() const { return TDimensionT<T>(getLx(), getLy()); };

  TPointT<T> getP00() const { return TPointT<T>(x0, y0); };
  TPointT<T> getP10() const { return TPointT<T>(x1, y0); };
  TPointT<T> getP01() const { return TPointT<T>(x0, y1); };
  TPointT<T> getP11() const { return TPointT<T>(x1, y1); };

  //! Returns the union of two source rectangles.
  //!The union is the smallest rectangle that contains both source rectangles.
  TRectT<T> operator+(const TRectT<T> &rect) const {  // unione
    if (isEmpty())
      return rect;
    else if (rect.isEmpty())
      return *this;
    else
      return TRectT<T>(std::min((T)x0, (T)rect.x0), 
                       std::min((T)y0, (T)rect.y0),
                       std::max((T)x1, (T)rect.x1),
                       std::max((T)y1, (T)rect.y1));
  };
  TRectT<T> &operator+=(const TRectT<T> &rect) {  // unione
    return *this = *this + rect;
  };
  TRectT<T> &operator*=(const TRectT<T> &rect) {  // intersezione
    return *this = *this * rect;
  };

  //!Returns the intersection of two existing rectangles.
  //The intersection is the largest rectangle contained in both existing rectangles.
  TRectT<T> operator*(const TRectT<T> &rect) const {  // intersezione
    if (isEmpty() || rect.isEmpty())
      return TRectT<T>();
    else if (rect.x1 < x0 || x1 < rect.x0 || rect.y1 < y0 || y1 < rect.y0)
      return TRectT<T>();
    else
      return TRectT<T>(std::max((T)x0, (T)rect.x0), std::max((T)y0, (T)rect.y0),
                       std::min((T)x1, (T)rect.x1),
                       std::min((T)y1, (T)rect.y1));
  };

  TRectT<T> &operator+=(const TPointT<T> &p) {  // shift
    x0 += p.x;
    y0 += p.y;
    x1 += p.x;
    y1 += p.y;
    return *this;
  };
  TRectT<T> &operator-=(const TPointT<T> &p) {
    x0 -= p.x;
    y0 -= p.y;
    x1 -= p.x;
    y1 -= p.y;
    return *this;
  };
  TRectT<T> operator+(const TPointT<T> &p) const {
    TRectT<T> ris(*this);
    return ris += p;
  };
  TRectT<T> operator-(const TPointT<T> &p) const {
    TRectT<T> ris(*this);
    return ris -= p;
  };

  bool operator==(const TRectT<T> &r) const {
    return x0 == r.x0 && y0 == r.y0 && x1 == r.x1 && y1 == r.y1;
  };

  bool operator!=(const TRectT<T> &r) const {
    return x0 != r.x0 || y0 != r.y0 || x1 != r.x1 || y1 != r.y1;
  };

  bool contains(const TPointT<T> &p) const {
    return x0 <= p.x && p.x <= x1 && y0 <= p.y && p.y <= y1;
  };

  bool contains(const TRectT<T> &b) const {
    return x0 <= b.x0 && x1 >= b.x1 && y0 <= b.y0 && y1 >= b.y1;
  };

  bool overlaps(const TRectT<T> &b) const {
    return x0 <= b.x1 && x1 >= b.x0 && y0 <= b.y1 && y1 >= b.y0;
  };

  TRectT<T> enlarge(T dx, T dy) const {
    if (isEmpty()) return *this;
    return TRectT<T>(x0 - dx, y0 - dy, x1 + dx, y1 + dy);
  };

  TRectT<T> enlarge(T d) const { return enlarge(d, d); };
  TRectT<T> enlarge(TDimensionT<T> d) const { return enlarge(d.lx, d.ly); };
};

//-----------------------------------------------------------------------------

typedef TRectT<int> TRect, TRectI;
typedef TRectT<double> TRectD;

#ifdef _WIN32
template class DVAPI TRectT<int>;
template class DVAPI TRectT<double>;
#endif

//=============================================================================

// check this, not final version
/*!
\relates TRectT
Convert a TRectD into a TRect
*/

inline TRect convert(const TRectD &r) {
  return TRect((int)(r.x0 + 0.5), (int)(r.y0 + 0.5), (int)(r.x1 + 0.5),
               (int)(r.y1 + 0.5));
}
/*!
\relates TRectT
Convert a TRect into a TRectD
*/
inline TRectD convert(const TRect &r) { return TRectD(r.x0, r.y0, r.x1, r.y1); }

// template?
/*!
\relates TRectT
\relates TPointT
*/
inline TRectD boundingBox(const TPointD &p0, const TPointD &p1) {
  return TRectD(std::min(p0.x, p1.x), std::min(p0.y, p1.y),  // bottom left
                std::max(p0.x, p1.x), std::max(p0.y, p1.y));  // top right
}
/*!
\relates TRectT
\relates TPointT
*/
inline TRectD boundingBox(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2) {
  return TRectD(std::min({p0.x, p1.x, p2.x}), std::min({p0.y, p1.y, p2.y}),
                std::max({p0.x, p1.x, p2.x}), std::max({p0.y, p1.y, p2.y}));
}

/*!
\relates TRectT
\relates TPointT
*/
inline TRectD boundingBox(const TPointD &p0, const TPointD &p1,
                          const TPointD &p2, const TPointD &p3) {
  return TRectD(
      std::min({p0.x, p1.x, p2.x, p3.x}), std::min({p0.y, p1.y, p2.y, p3.y}),
      std::max({p0.x, p1.x, p2.x, p3.x}), std::max({p0.y, p1.y, p2.y, p3.y}));
}

//-----------------------------------------------------------------------------

// TRectT is a rectangle that uses thick points
template <>
inline TRectT<int>::TRectT() : x0(0), y0(0), x1(-1), y1(-1) {}
template <>
inline TRectT<int>::TRectT(const TPointT<int> &bottomLeft,
                           const TDimensionT<int> &d)
    : x0(bottomLeft.x)
    , y0(bottomLeft.y)
    , x1(bottomLeft.x + d.lx - 1)
    , y1(bottomLeft.y + d.ly - 1){};
template <>
inline TRectT<int>::TRectT(const TDimensionT<int> &d)
    : x0(0), y0(0), x1(d.lx - 1), y1(d.ly - 1){};
template <>
inline bool TRectT<int>::isEmpty() const {
  return x0 > x1 || y0 > y1;
}
template <>
inline void TRectT<int>::empty() {
  x0 = y0 = 0;
  x1 = y1 = -1;
}

// Is the adding of one here to account for the thickness?
template <>
inline int TRectT<int>::getLx() const {
  return x1 >= x0 ? x1 - x0 + 1 : 0;
}
template <>
inline int TRectT<int>::getLy() const {
  return y1 >= y0 ? y1 - y0 + 1 : 0;
}

template <>
inline TRectT<double>::TRectT() : x0(0), y0(0), x1(0), y1(0) {}
template <>
inline TRectT<double>::TRectT(const TPointT<double> &bottomLeft,
                              const TDimensionT<double> &d)
    : x0(bottomLeft.x)
    , y0(bottomLeft.y)
    , x1(bottomLeft.x + d.lx)
    , y1(bottomLeft.y + d.ly){};
template <>
inline TRectT<double>::TRectT(const TDimensionT<double> &d)
    : x0(0.0), y0(0.0), x1(d.lx), y1(d.ly){};
template <>
inline bool TRectT<double>::isEmpty() const {
  return (x0 == x1 && y0 == y1) || x0 > x1 || y0 > y1;
}
template <>
inline void TRectT<double>::empty() {
  x0 = y0 = x1 = y1 = 0;
}
template <>
inline double TRectT<double>::getLx() const {
  return x1 >= x0 ? x1 - x0 : 0;
}
template <>
inline double TRectT<double>::getLy() const {
  return y1 >= y0 ? y1 - y0 : 0;
}

//-----------------------------------------------------------------------------

inline TRectD &operator*=(TRectD &rect, double factor) {
  rect.x0 *= factor;
  rect.y0 *= factor;
  rect.x1 *= factor;
  rect.y1 *= factor;
  return rect;
}

//-----------------------------------------------------------------------------

inline TRectD operator*(const TRectD &rect, double factor) {
  TRectD result(rect);
  return result *= factor;
}

//-----------------------------------------------------------------------------

inline TRectD &operator/=(TRectD &rect, double factor) {
  assert(factor != 0.0);
  return rect *= (1.0 / factor);
}

//-----------------------------------------------------------------------------

inline TRectD operator/(const TRectD &rect, double factor) {
  assert(factor != 0.0);
  TRectD result(rect);
  return result *= 1.0 / factor;
}

//-----------------------------------------------------------------------------

template <class T>
inline std::ostream &operator<<(std::ostream &out, const TRectT<T> &r) {
  return out << "(" << r.x0 << "," << r.y0 << ";" << r.x1 << "," << r.y1 << ")";
}

//=============================================================================

namespace TConsts {

extern DVVAR const TPointD napd;
extern DVVAR const TPoint nap;
extern DVVAR const T3DPointD nap3d;
extern DVVAR const TThickPoint natp;
extern DVVAR const TRectD infiniteRectD;
extern DVVAR const TRectI infiniteRectI;
}

//=============================================================================
//! This is the base class for the affine transformations.
/*!
 This class performs basic manipulations of affine transformations.
 An affine transformation is a linear transformation followed by a translation.
 
  [a11, a12, a13]
  [a21, a22, a23]

  a13 and a23 represent translation (moving sideways or up and down)
  the other 4 handle rotation, scale and shear
*/
class DVAPI TAffine {
public:
  double a11, a12, a13;
  double a21, a22, a23;
  /*!
          By default the object is initialized with a null matrix and a null
     translation vector.
  */
  TAffine() : a11(1.0), a12(0.0), a13(0.0), a21(0.0), a22(1.0), a23(0.0){};
  /*!
          Initializes the internal matrix and vector of translation with the
     user values.
  */
  TAffine(double p11, double p12, double p13, double p21, double p22,
          double p23)
      : a11(p11), a12(p12), a13(p13), a21(p21), a22(p22), a23(p23){};
  /*!
          Copy constructor.
  */
  TAffine(const TAffine &a)
      : a11(a.a11)
      , a12(a.a12)
      , a13(a.a13)
      , a21(a.a21)
      , a22(a.a22)
      , a23(a.a23){};
  /*!
          Assignment operator.
*/
  TAffine &operator=(const TAffine &a);
  /*Moved to tgeometry.cpp
{
a11 = a.a11; a12 = a.a12; a13 = a.a13;
a21 = a.a21; a22 = a.a22; a23 = a.a23;
return *this;
};
*/
  /*!
          Matrix multiplication.
          <p>\f$\left(\begin{array}{cc}\bf{A}&\vec{a}\\\vec{0}&1\end{array}\right)
          \left(\begin{array}{cc}\bf{B}&\vec{b}\\\vec{0}&1\end{array}\right)\f$</p>

*/
  TAffine operator*(const TAffine &b) const;
  /*Moved to in tgeometry.cpp
{
return TAffine (
a11 * b.a11 + a12 * b.a21,
a11 * b.a12 + a12 * b.a22,
a11 * b.a13 + a12 * b.a23 + a13,

a21 * b.a11 + a22 * b.a21,
a21 * b.a12 + a22 * b.a22,
a21 * b.a13 + a22 * b.a23 + a23);
};
*/

  TAffine operator*=(const TAffine &b);
  /*Moved to tgeometry.cpp
{
return *this = *this * b;
};
*/
  /*!
          Returns the inverse tansformation as:
          <p>\f$\left(\begin{array}{ccc}\bf{A}^{-1}&-\bf{A}^{-1}&\vec{b}\\\vec{0}&\vec{0}&1\end{array}\right)\f$</p>
  */

  TAffine inv() const;
  /*Moved to tgeometry.cpp
{
if(a12 == 0.0 && a21 == 0.0)
{
assert(a11 != 0.0);
assert(a22 != 0.0);
double inv_a11 = 1.0/a11;
double inv_a22 = 1.0/a22;
return TAffine(inv_a11,0, -a13 * inv_a11,
  0,inv_a22, -a23 * inv_a22);
}
else if(a11 == 0.0 && a22 == 0.0)
{
assert(a12 != 0.0);
assert(a21 != 0.0);
double inv_a21 = 1.0/a21;
double inv_a12 = 1.0/a12;
return TAffine(0, inv_a21, -a23 * inv_a21,
  inv_a12, 0, -a13 * inv_a12);
}
else
{
double d = 1./det();
return TAffine(a22*d,-a12*d, (a12*a23-a22*a13)*d,
  -a21*d, a11*d, (a21*a13-a11*a23)*d);
}
};
*/

  double det() const;
  /*Sposto in tgeometry.cpp{
return a11*a22-a12*a21;
};
*/

  /*!
          Returns \e true if all elements are equals.
  */
  bool operator==(const TAffine &a) const;
  /*Sposto in tgeometry.cpp
{
return a11==a.a11 && a12==a.a12 && a13==a.a13 &&
a21==a.a21 && a22==a.a22 && a23==a.a23;
};
*/
  /*!
          Returns \e true if at least one element is different.
  */

  bool operator!=(const TAffine &a) const;
  /*Sposto in tgeometry.cpp
{
return a11!=a.a11 || a12!=a.a12 || a13!=a.a13 ||
a21!=a.a21 || a22!=a.a22 || a23!=a.a23;
};
*/
  /*!
          Returns \e true if the transformation is an identity,
          i.e in the error limit \e err leaves the vectors unchanged.
  */

  bool isIdentity(double err = 1.e-8) const;
  /*Sposto in tgeometry.cpp
{
return ((a11-1.0)*(a11-1.0)+(a22-1.0)*(a22-1.0)+
a12*a12+a13*a13+a21*a21+a23*a23) < err;
};
*/
  /*!
          Returns \e true if in the error limits \e err \f$\bf{A}\f$ is the
     identity matrix.
  */

  bool isTranslation(double err = 1.e-8) const;
  /*Sposto in tgeometry.cpp
{
return ((a11-1.0)*(a11-1.0)+(a22-1.0)*(a22-1.0)+
a12*a12+a21*a21) < err;
};
*/
  /*!
          Returns \e true if in the error limits the matrix \f$\bf{A}\f$ is of
     the form:
          <p>\f$\left(\begin{array}{cc}a&b\\-b&a\end{array}\right)\f$</p>.
  */

  bool isIsotropic(double err = 1.e-8) const;
  /*Sposto in tgeometry.cpp
  {
    return areAlmostEqual(a11, a22, err) && areAlmostEqual(a12, -a21, err);
  };
  */

  /*!
          Returns the transformed point.
  */
  TPointD operator*(const TPointD &p) const;
  /*Sposto in tgeometry.cpp
{
return TPointD(p.x*a11+p.y*a12+a13, p.x*a21+p.y*a22+a23);
};
*/

  /*!
          Returns the transformed box of the bounding box.
  */
  TRectD operator*(const TRectD &rect) const;

  /*!
          Returns a translated matrix that change the vector (u,v) in (x,y).
  \n	It returns a matrix of the form:
          <p>\f$\left(\begin{array}{ccc}\bf{A}&\vec{x}-\bf{A} \vec{u}\\
          \vec{0}&1\end{array}\right)\f$</p>
  */
  TAffine place(double u, double v, double x, double y) const;

  /*!
          See above.
  */
  TAffine place(const TPointD &pIn, const TPointD &pOut) const;
};

//-----------------------------------------------------------------------------

// template <>
inline bool areAlmostEqual(const TPointD &a, const TPointD &b,
                           double err = TConsts::epsilon) {
  return tdistance2(a, b) < err * err;
}

// template <>
inline bool areAlmostEqual(const TRectD &a, const TRectD &b,
                           double err = TConsts::epsilon) {
  return areAlmostEqual(a.getP00(), b.getP00(), err) &&
         areAlmostEqual(a.getP11(), b.getP11(), err);
}

const TAffine AffI = TAffine();

//-----------------------------------------------------------------------------

class DVAPI TTranslation final : public TAffine {
public:
  TTranslation(){};
  TTranslation(double x, double y) : TAffine(1, 0, x, 0, 1, y){};
  TTranslation(const TPointD &p) : TAffine(1, 0, p.x, 0, 1, p.y){};
};

//-----------------------------------------------------------------------------

class DVAPI TRotation final : public TAffine {
public:
  TRotation(){};

  /*! makes a rotation matrix of  "degrees" degrees counterclockwise
on the origin */
  TRotation(double degrees);
  /*Sposto in tgeometry.cpp
{
double rad, sn, cs;
int idegrees = (int)degrees;
if ((double)idegrees == degrees && idegrees % 90 == 0)
{
switch ((idegrees / 90) & 3)
{
case 0:  sn =  0; cs =  1; break;
case 1:  sn =  1; cs =  0; break;
case 2:  sn =  0; cs = -1; break;
case 3:  sn = -1; cs =  0; break;
default: sn =  0; cs =  0; break;
}
}
else
{
rad = degrees * (TConsts::pi_180);
sn = sin (rad);
cs = cos (rad);
if (sn == 1 || sn == -1)
  cs = 0;
if (cs == 1 || cs == -1)
  sn = 0;
}
a11=cs;a12= -sn;a21= -a12;a22=a11;
};
*/

  /*! makes a rotation matrix of  "degrees" degrees counterclockwise
on the given center */
  TRotation(const TPointD &center, double degrees);
  /*Sposto in tgeometry.cpp
{
TAffine a = TTranslation(center) * TRotation(degrees) * TTranslation(-center);
a11 = a.a11; a12 = a.a12; a13 = a.a13;
a21 = a.a21; a22 = a.a22; a23 = a.a23;
};
*/
};

//-----------------------------------------------------------------------------

class DVAPI TScale final : public TAffine {
public:
  TScale(){};
  TScale(double sx, double sy) : TAffine(sx, 0, 0, 0, sy, 0){};
  TScale(double s) : TAffine(s, 0, 0, 0, s, 0) {}

  TScale(const TPointD &center, double sx, double sy);
  /*Sposto in tgeometry.cpp
{
TAffine a = TTranslation(center) * TScale(sx,sy) * TTranslation(-center);
a11 = a.a11; a12 = a.a12; a13 = a.a13;
a21 = a.a21; a22 = a.a22; a23 = a.a23;
}
*/

  TScale(const TPointD &center, double s);
  /*Sposto in tgeometry.cpp
{
TAffine a = TTranslation(center) * TScale(s) * TTranslation(-center);
a11 = a.a11; a12 = a.a12; a13 = a.a13;
a21 = a.a21; a22 = a.a22; a23 = a.a23;
}
*/
};

//-----------------------------------------------------------------------------

class DVAPI TShear final : public TAffine {
public:
  TShear(){};
  TShear(double sx, double sy) : TAffine(1, sx, 0, sy, 1, 0){};
};

//-----------------------------------------------------------------------------

inline bool areEquals(const TAffine &a, const TAffine &b, double err = 1e-8) {
  return fabs(a.a11 - b.a11) < err && fabs(a.a12 - b.a12) < err &&
         fabs(a.a13 - b.a13) < err && fabs(a.a21 - b.a21) < err &&
         fabs(a.a22 - b.a22) < err && fabs(a.a23 - b.a23) < err;
}

//-----------------------------------------------------------------------------

inline TAffine inv(const TAffine &a) { return a.inv(); }

//-----------------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out, const TAffine &a) {
  return out << "(" << a.a11 << ", " << a.a12 << ", " << a.a13 << ";" << a.a21
             << ", " << a.a22 << ", " << a.a23 << ")";
}

#endif  //  __T_GEOMETRY_INCLUDED__
