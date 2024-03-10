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

inline double logNormalDistribuitionUnscaled(double x, double x0, double w)
  { return exp(-0.5*pow(log(x/x0)/w, 2.0))/x; }

inline double logNormalDistribuition(double x, double x0, double w)
  { return logNormalDistribuitionUnscaled(x, x0, w)/(w*sqrt(2.0*M_PI)); }

//=============================================================================

template <class T> class T3DPointT;
template <class T> class T4DPointT;

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

  inline TPointT() : x(0), y(0){};
  inline TPointT(T x, T y) : x(x), y(y){};
  inline explicit TPointT(const T3DPointT<T> &p);
  inline explicit TPointT(const T4DPointT<T> &p);

  inline TPointT& operator+=(const TPointT &a)
    { return x += a.x, y += a.y, *this; };
  inline TPointT& operator-=(const TPointT &a)
    { return x -= a.x, y -= a.y, *this; };
  inline TPointT operator+(const TPointT &a) const
    { return TPointT(x + a.x, y + a.y); };
  inline TPointT operator-(const TPointT &a) const
    { return TPointT(x - a.x, y - a.y); };
  inline TPointT operator-() const
    { return TPointT(-x, -y); };
  
  //! Scalar(dot) Product
  inline T operator*(const TPointT &a) const
    { return x*a.x + y*a.y; }

  inline TPointT operator*=(T a)
    { return x *= a, y *= a, *this; }
  inline TPointT operator*(T a) const
    { return TPointT(x*a, y*a); }
  friend inline TPointT operator*(T a, const TPointT &b)
    { return TPointT(a*b.x, a*b.y); }

  inline TPointT operator/(T a) const
    { return TPointT(x/a, y/a); }
  inline TPointT operator/=(T a)
    { return x /= a, y /= a, *this; }

  inline bool operator==(const TPointT &a) const
    { return x == a.x && y == a.y; }
  inline bool operator!=(const TPointT &a) const
    { return !(*this == a); }
  
  friend inline std::ostream &operator<<(std::ostream &out, const TPointT &p)
    { return out << "(" << p.x << ", " << p.y << ")"; }
  
  /*! Rotate a point 90 degrees (counterclockwise).
  \param p a point.
  \return the rotated point
  \sa rotate270 */
  friend inline TPointT rotate90(const TPointT &p) // 90 counterclockwise
    { return TPointT(-p.y, p.x); }
  
  /*! Rotate a point 270 degrees (clockwise).
  \param p a point.
  \return the rotated point
  \sa rotate90 */
  friend inline TPointT rotate270(const TPointT &p)  // 90 clockwise
    { return TPointT(p.y, -p.x); }

  //! This helper function returns the square of the absolute value of the point
  friend inline T norm2(const TPointT &a)
    { return a*a; }
  
  //! This helper function returns the square of the distance between two points
  friend inline T tdistance2(const TPointT &a, const TPointT &b)
    { return norm2(a - b); }

  //! the cross product
  friend inline T cross(const TPointT &a, const TPointT &b)
    { return a.x*b.y - a.y*b.x; }
};

template <>
inline bool TPointT<double>::operator==(const TPointT<double> &a) const
  { return tdistance2(*this, a) <= TConsts::epsilon * TConsts::epsilon; }


//-----------------------------------------------------------------------------

typedef TPointT<int> TPoint, TPointI;
typedef TPointT<double> TPointD;

#ifdef _WIN32
template class DVAPI TPointT<int>;
template class DVAPI TPointT<double>;
#endif


//-----------------------------------------------------------------------------

/*!
\relates TPointT
This helper function converts a TPoint (TPointT<int>) into a TPointD
*/
inline TPointD convert(const TPoint &p)
  { return TPointD(p.x, p.y); }

/*!
\relates TPointT
This helper function converts a TPointD (TPointT<double>) into a TPoint
*/
inline TPoint convert(const TPointD &p)
  { return TPoint(tround(p.x), tround(p.y)); }


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
  assert(n);
  return p*(1/n);
}

/*!
\relates TPointT
This helper function returns the normalized version of the specified point
or zero if it is not possible
*/
inline TPointD normalizeOrZero(const TPointD &p) {
  double n = norm2(p);
  return fabs(n) > TConsts::epsilon*TConsts::epsilon ? p*(1/sqrt(n)) : TPointD();
}

/*!
\relates TPointT
This helper function returns the distance between two points
*/
inline double tdistance(const TPointD &p1, const TPointD &p2)
  { return norm(p2 - p1); }

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

  inline T3DPointT() : x(), y(), z() {}
  inline T3DPointT(T x, T y, T z) : x(x), y(y), z(z) {}
  inline T3DPointT(const TPointT<T> &p, T z) : x(p.x), y(p.y), z(z) {}
  inline explicit T3DPointT(const T4DPointT<T> &p);

  inline TPointT<T>& xy() { return *(TPointT<T>*)this; }
  inline const TPointT<T>& xy() const { return *(const TPointT<T>*)this; }

  inline T3DPointT &operator+=(const T3DPointT &a)
    { return x += a.x, y += a.y, z += a.z, *this; }
  inline T3DPointT &operator-=(const T3DPointT &a)
    { return x -= a.x, y -= a.y, z -= a.z, *this; }
  inline T3DPointT operator+(const T3DPointT &a) const
    { return T3DPointT(x + a.x, y + a.y, z + a.z); }
  inline T3DPointT operator-(const T3DPointT &a) const
    { return T3DPointT(x - a.x, y - a.y, z - a.z); }
  inline T3DPointT operator-() const
    { return T3DPointT(-x, -y, -z); }

  //! Scalar(dot) Product
  inline T operator*(const T3DPointT &a) const
    { return x*a.x + y*a.y + z*a.z; }

  inline T3DPointT operator*=(T a)
    { return x *= a, y *= a, z *= a, *this; }
  inline T3DPointT operator*(T a) const
    { return T3DPointT(x*a, y*a, z*a); }
  friend inline T3DPointT operator*(T a, const T3DPointT &b)
    { return T3DPointT(a*b.x, a*b.y, a*b.z); }

  inline T3DPointT operator/(T a) const
    { return T3DPointT(x/a, y/a, z/a); }
  inline T3DPointT operator/=(T a)
    { return x /= a, y /= a, z /= a, *this; }

  inline bool operator==(const T3DPointT &a) const
    { return x == a.x && y == a.y && z == a.z; }
  inline bool operator!=(const T3DPointT &a) const
    { return !(*this == a); }
  
  friend inline std::ostream &operator<<(std::ostream &out, const T3DPointT &p)
    { return out << "(" << p.x << ", " << p.y << ", " << p.z << ")"; }
  
  //! This helper function returns the square of the absolute value of the point
  friend inline T norm2(const T3DPointT &a)
    { return a*a; }
  
  //! This helper function returns the square of the distance between two points
  friend inline T tdistance2(const T3DPointT &a, const T3DPointT &b)
    { return norm2(a - b); }

  //! the cross product
  friend inline T3DPointT cross(const T3DPointT &a, const T3DPointT &b) {
    return T3DPointT( a.y*b.z - b.y*a.z,
                      a.z*b.x - b.z*a.x,
                      a.x*b.y - b.x*a.y);
  }
};

template <>
inline bool T3DPointT<double>::operator==(const T3DPointT<double> &a) const
  { return tdistance2(*this, a) <= TConsts::epsilon * TConsts::epsilon; }

template <class T>
inline TPointT<T>::TPointT(const T3DPointT<T> &p) : x(p.x), y(p.y) {};

//=============================================================================

typedef T3DPointT<int> T3DPoint, T3DPointI;
typedef T3DPointT<double> T3DPointD;

#ifdef _WIN32
template class DVAPI T3DPointT<int>;
template class DVAPI T3DPointT<double>;
#endif

//-----------------------------------------------------------------------------

inline T3DPointD convert(const T3DPoint &p)
  { return T3DPointD(p.x, p.y, p.z); }
inline T3DPoint convert(const T3DPointD &p)
  { return T3DPoint(tround(p.x), tround(p.y), tround(p.z)); }

inline double norm(const T3DPointD &p)
  { return std::sqrt(norm2(p)); }

inline T3DPointD normalize(const T3DPointD &p) {
  double n = norm(p);
  assert(n);
  return p*(1/n);
}

inline T3DPointD normalizeOrZero(const T3DPointD &p) {
  double n = norm2(p);
  return fabs(n) > TConsts::epsilon*TConsts::epsilon ? p*(1/sqrt(n)) : T3DPointD();
}

inline double tdistance(const T3DPointD &p1, const T3DPointD &p2)
  { return norm(p2 - p1); }

//=============================================================================

template <class T>
class DVAPI T4DPointT {
public:
  T x, y, z, w;

  inline T4DPointT() : x(), y(), z(), w() {}
  inline T4DPointT(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}
  inline T4DPointT(const TPointT<T> &p, T z, T w) : x(p.x), y(p.y), z(z), w(w) {}
  inline T4DPointT(const T3DPointT<T> &p, T w) : x(p.x), y(p.y), z(p.z), w(w) {}

  inline TPointT<T>& xy() { return *(TPointT<T>*)this; }
  inline T3DPointT<T>& xyz() { return *(T3DPointT<T>*)this; }

  inline const TPointT<T>& xy() const { return *(const TPointT<T>*)this; }
  inline const T3DPointT<T>& xyz() const { return *(const T3DPointT<T>*)this; }
  
  inline bool operator==(const T4DPointT &p) const
    { return x == p.x && y == p.y && z == p.z && w == p.w; }
  inline bool operator!=(const T4DPointT &p) const
    { return !(*this == p); }
  
  friend inline std::ostream &operator<<(std::ostream &out, const T4DPointT &p)
    { return out << "(" << p.x << ", " << p.y << ", " << p.z << ", " << p.w << ")"; }
};

template <>
inline bool T4DPointT<double>::operator==(const T4DPointT<double> &a) const {
  T4DPointT<double> d(x - a.x, y - a.y, z - a.z, w - a.w);
  return d.x*d.x + d.y*d.y + d.z*d.z + d.w*d.w
      <= TConsts::epsilon * TConsts::epsilon;
}

template <class T>
inline TPointT<T>::TPointT(const T4DPointT<T> &p) : x(p.x), y(p.y) {};
template <class T>
inline T3DPointT<T>::T3DPointT(const T4DPointT<T> &p) : x(p.x), y(p.y), z(p.z) {};

//=============================================================================

typedef T4DPointT<int> T4DPoint, T4DPointI;
typedef T4DPointT<double> T4DPointD;

#ifdef _WIN32
template class DVAPI T4DPointT<int>;
template class DVAPI T4DPointT<double>;
#endif

//-----------------------------------------------------------------------------

//!\relates T4DPointT

inline T4DPointD convert(const T4DPoint &p)
  { return T4DPointD(p.x, p.y, p.z, p.w); }
inline T4DPoint convert(const T4DPointD &p)
  { return T4DPoint(tround(p.x), tround(p.y), tround(p.z), tround(p.w)); }

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

  bool isZero(double err = 1.e-8) const;

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
          Transform point without translation
  */
  TPointD transformDirection(const TPointD &p) const;

  /*!
          Retruns the transformed box of the bounding box.
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

  inline static TAffine identity()
    { return TAffine(); }
  inline static TAffine zero()
    { return TAffine(0, 0, 0, 0, 0, 0); }

  inline static TAffine translation(double x, double y)
    { return TAffine(1, 0, x, 0, 1, y); }
  inline static TAffine translation(const TPointD &p)
    { return translation(p.x, p.y); }

  inline static TAffine scale(double sx, double sy)
    { return TAffine(sx, 0, 0, 0, sy, 0); }
  inline static TAffine scale(double s)
    { return scale(s, s); }
  inline static TAffine scale(const TPointD &center, double sx, double sy)
    { return translation(center)*scale(sx, sy)*translation(-center); }
  inline static TAffine scale(const TPointD &center, double s)
    { return scale(center, s, s); }

  static TAffine rotation(double angle);
  inline static TAffine rotation(const TPointD &center, double angle)
    { return translation(center)*rotation(angle)*translation(-center); }

  inline static TAffine shear(double sx, double sy)
    { return TAffine(1, sx, 0, sy, 1, 0); }
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


//=============================================================================

//! This class performs basic manipulations of affine transformations in 2D space.
//! with ability of perspective transform
//! the matrix is transposed to TAffine and equal to OpenGL cells order

class DVAPI TAffine3 {
public:
  union {
    struct {
      double a11, a12, a13;
      double a21, a22, a23;
      double a31, a32, a33;
    };
    double m[3][3];
    double a[9];
  };

  inline TAffine3():
    a11(1.0), a12(0.0), a13(0.0),
    a21(0.0), a22(1.0), a23(0.0),
    a31(0.0), a32(0.0), a33(1.0) { }

  inline explicit TAffine3(const TAffine &a):
    a11(a.a11), a12(a.a21), a13(0.0),
    a21(a.a12), a22(a.a22), a23(0.0),
    a31(a.a13), a32(a.a23), a33(1.0) { }

  inline TAffine3(
    const T3DPointD &rowX,
    const T3DPointD &rowY,
    const T3DPointD &rowZ
  ):
    a11(rowX.x), a12(rowX.y), a13(rowX.z),
    a21(rowY.x), a22(rowY.y), a23(rowY.z),
    a31(rowZ.x), a32(rowZ.y), a33(rowZ.z) { }

  inline T3DPointD& row(int index)
    { return *(T3DPointD*)(m[index]); }
  inline const T3DPointD& row(int index) const
    { return *(const T3DPointD*)(m[index]); }

  inline T3DPointD& rowX() { return row(0); }
  inline T3DPointD& rowY() { return row(1); }
  inline T3DPointD& rowZ() { return row(2); }

  inline const T3DPointD& rowX() const { return row(0); }
  inline const T3DPointD& rowY() const { return row(1); }
  inline const T3DPointD& rowZ() const { return row(2); }

  T3DPointD operator*(const T3DPointD &b) const;
  TAffine3 operator*(const TAffine3 &b) const;
  TAffine3 operator*=(const TAffine3 &b);

  TAffine3 inv() const;

  TAffine get2d() const;

  inline static TAffine3 identity() { return TAffine3(); }
  static TAffine3 translation2d(double x, double y);
  static TAffine3 scale2d(double x, double y);
  static TAffine3 rotation2d(double angle);
};


//=============================================================================

//! This class performs basic manipulations of affine transformations in 3D space.
//! the matrix is transposed to TAffine and equal to OpenGL

class DVAPI TAffine4 {
public:
  union {
    struct {
      double a11, a12, a13, a14;
      double a21, a22, a23, a24;
      double a31, a32, a33, a34;
      double a41, a42, a43, a44;
    };
    double m[4][4];
    double a[16];
  };

  inline TAffine4():
    a11(1.0), a12(0.0), a13(0.0), a14(0.0),
    a21(0.0), a22(1.0), a23(0.0), a24(0.0),
    a31(0.0), a32(0.0), a33(1.0), a34(0.0),
    a41(0.0), a42(0.0), a43(0.0), a44(1.0) { }

  inline explicit TAffine4(const TAffine &a):
    a11(a.a11), a12(a.a21), a13(0.0), a14(0.0),
    a21(a.a12), a22(a.a22), a23(0.0), a24(0.0),
    a31( 0.0 ), a32( 0.0 ), a33(1.0), a34(0.0),
    a41(a.a13), a42(a.a23), a43(0.0), a44(1.0) { }

  inline TAffine4(
    const T4DPointD &rowX,
    const T4DPointD &rowY,
    const T4DPointD &rowZ,
    const T4DPointD &rowW
  ):
    a11(rowX.x), a12(rowX.y), a13(rowX.z), a14(rowX.w),
    a21(rowY.x), a22(rowY.y), a23(rowY.z), a24(rowY.w),
    a31(rowZ.x), a32(rowZ.y), a33(rowZ.z), a34(rowZ.w),
    a41(rowW.x), a42(rowW.y), a43(rowW.z), a44(rowW.w) { }

  inline T4DPointD& row(int index)
    { return *(T4DPointD*)(m[index]); }
  inline const T4DPointD& row(int index) const
    { return *(const T4DPointD*)(m[index]); }

  inline T4DPointD& rowX() { return row(0); }
  inline T4DPointD& rowY() { return row(1); }
  inline T4DPointD& rowZ() { return row(2); }
  inline T4DPointD& rowW() { return row(3); }

  inline const T4DPointD& rowX() const { return row(0); }
  inline const T4DPointD& rowY() const { return row(1); }
  inline const T4DPointD& rowZ() const { return row(2); }
  inline const T4DPointD& rowW() const { return row(3); }

  T4DPointD operator*(const T4DPointD &b) const;
  TAffine4 operator*(const TAffine4 &b) const;
  TAffine4 operator*=(const TAffine4 &b);

  TAffine4 inv() const;

  TAffine get2d(double z = 0.0) const;
  TAffine3 get2dPersp(double z = 0.0) const;

  inline static TAffine4 identity() { return TAffine4(); }
  static TAffine4 translation(double x, double y, double z);
  static TAffine4 scale(double x, double y, double z);
  static TAffine4 rotation(double x, double y, double z, double angle);
  static TAffine4 rotationX(double angle);
  static TAffine4 rotationY(double angle);
  static TAffine4 rotationZ(double angle);
  static TAffine4 perspective(double near, double far, double tangent);
};


//=============================================================================

//! This class performs binary manipulations with angle ranges

typedef unsigned int TAngleI;

class DVAPI TAngleRangeSet {
public:
  typedef TAngleI Type;
  typedef std::vector<Type> List;

  static const Type min = Type();
  static const Type max = Type() - Type(1);
  static const Type half = ((Type() - Type(1)) >> 1) + Type(1);

  static Type fromDouble(double a)
    { return Type(round((a/M_2PI + 0.5)*max)); }
  static double toDouble(Type a)
    { return ((double)a/(double)max - 0.5)*M_2PI; }
  static List::const_iterator empty_iterator()
    { static List list; return list.end(); }
  
  struct Range {
    Type a0, a1;
    Range(): a0(), a1() { }
    Range(Type a0, Type a1): a0(a0), a1(a1) { }
    inline bool isEmpty() const { return a0 == a1; }
    inline Range flip() const { return Range(a1, a0); }
  };

  struct Iterator {
  private:
    bool m_flip;
    List::const_iterator m_prebegin;
    List::const_iterator m_begin;
    List::const_iterator m_end;
    List::const_iterator m_current;
    bool m_lapped;

  public:
    inline Iterator(): m_flip(), m_lapped(true)
      { reset(); }
    inline explicit Iterator(const List &list, bool flip = false, bool reverse = false)
      { set(list, flip, reverse); }
    inline explicit Iterator(const TAngleRangeSet &ranges, bool flip = false, bool reverse = false)
      { set(ranges, flip, reverse); }

    inline Iterator& set(bool full) {
      m_flip = full; m_lapped = !m_flip;
      m_current = m_prebegin = m_begin = m_end = empty_iterator();
      return *this;
    }

    inline Iterator& reset()
      { return set(false); }

    inline Iterator& set(const List &list, bool flip = false, bool reverse = false) {
      assert(list.size()%2 == 0);
      if (list.empty()) {
        set(flip);
      } else {
        m_flip = flip;
        m_lapped = false;
        if (flip) {
          m_prebegin = list.end() - 1;
          m_begin = list.begin();
          m_end = m_prebegin - 1;
        } else {
          m_prebegin = list.begin();
          m_begin = m_prebegin + 1;
          m_end = list.end() - 1;
        }
      }
      m_current = reverse ? m_end : m_begin;
      return *this;
    }

    inline Iterator& set(const TAngleRangeSet &ranges, bool flip = false, bool reverse = false)
      { return set(ranges.angles(), ranges.isFlipped() != flip, reverse); }

    inline const Type a0() const
      { return valid() ? *(m_current == m_begin ? m_prebegin : m_current - 1) : Type(); }
    inline const Type a1() const
      { return valid() ? *m_current : Type(); }
    inline double d0() const
      { return toDouble(a0()); }
    inline double d1() const
      { return toDouble(a1()); }
    inline double d1greater() const {
      return !valid() ? (m_flip ? M_PI : -M_PI)
           : m_current == m_begin && m_prebegin > m_begin
           ? toDouble(*m_current) + M_2PI : toDouble(*m_current);
    }
    inline Range range() const
      { return Range(a0(), a1()); }
    inline int size() const
      { return (int)(m_end - m_begin)/2 + 1; }
    inline int index() const
      { return (int)(m_current - m_begin)/2; }
    inline int reverseIndex() const
      { int i = index(); return i == 0 ? 0 : size() - i; }
    inline bool lapped() const
      { return m_lapped; }
    inline bool valid() const
      { return m_prebegin != m_begin; }
    inline bool isFull() const
      { return !valid() && m_flip; }
    inline bool isEmpty() const
      { return !valid() && !m_flip; }

    inline operator bool() const
      { return !m_lapped; }

    inline Iterator& operator++() {
      if (!valid()) { m_lapped = true; return *this; }
      m_lapped = (m_current == m_end);
      if (m_lapped) m_current = m_begin; else m_current += 2;
      return *this;
    }

    inline Iterator& operator--() {
      if (!valid()) { m_lapped = true; return *this; }
      m_lapped = (m_current == m_end);
      if (m_lapped) m_current = m_end; else m_current -= 2;
      return *this;
    }

    inline Iterator& operator += (int i) {
      if (i == 0) { m_lapped = isEmpty(); return *this; }
      if (!valid()) { m_lapped = true; return *this; }
      int ii = index();
      int s = size();
      if (ii + i >= 0 && ii + i < s) {
        m_current += i*2;
        m_lapped = false;
      } else {
        m_current = m_begin + ((ii + s + i%s)%s)*2;
        m_lapped = true;
      }
      return *this;
    }

    inline int operator-(const Iterator &i) const {
      assert(m_flip == i.m_flip && m_begin == i.m_begin && m_end == i.m_end && m_prebegin == i.m_prebegin);
      int ii = (int)(m_current - i.m_current);
      return ii < 0 ? ii + size() : ii;
    }

    inline Iterator operator++() const
      { Iterator copy(*this); ++(*this); return copy; }
    inline Iterator operator--() const
      { Iterator copy(*this); --(*this); return copy; }
    inline Iterator& operator -= (int i)
      { return (*this) += -i; }
    inline Iterator operator+(int i) const
      { Iterator ii(*this); return ii += i; }
    inline Iterator operator-(int i) const
      { Iterator ii(*this); return ii -= i; }
  };

private:
  bool m_flip;
  List m_angles;

  int find(Type a) const;
  void insert(Type a);
  void doAdd(Type a0, Type a1);

public:
  inline explicit TAngleRangeSet(bool fill = false): m_flip(fill) { }
  inline TAngleRangeSet(const TAngleRangeSet &x, bool flip = false):
      m_flip(x.isFlipped() != flip), m_angles(x.angles()) { }

  inline const List& angles() const { return m_angles; }
  inline bool isFlipped() const { return m_flip; }
  inline bool isEmpty() const { return !m_flip && m_angles.empty(); }
  inline bool isFull() const { return m_flip && m_angles.empty(); }

  bool contains(Type a) const;
  bool check() const;

  inline void clear() { m_flip = false; m_angles.clear(); }
  inline void fill() { m_flip = true; m_angles.clear(); }
  inline void invert() { m_flip = !m_flip; }

  void set(Type a0, Type a1);
  void set(const TAngleRangeSet &x, bool flip = false);

  //! also known as 'xor'
  void invert(Type a0, Type a1);
  inline void invert(const Range &x) { invert(x.a0, x.a1); }
  void invert(const TAngleRangeSet &x);

  void add(Type a0, Type a1);
  inline void add(const Range &x) { add(x.a0, x.a1); }
  void add(const TAngleRangeSet &x);

  void subtract(Type a0, Type a1);
  inline void subtract(const Range &x) { subtract(x.a0, x.a1); }
  void subtract(const TAngleRangeSet &x);

  void intersect(Type a0, Type a1);
  inline void intersect(const Range &x) { intersect(x.a0, x.a1); }
  void intersect(const TAngleRangeSet &x);
};


#endif  //  __T_GEOMETRY_INCLUDED__
