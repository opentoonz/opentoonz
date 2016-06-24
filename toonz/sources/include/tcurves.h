#pragma once

#ifndef T_CURVES_INCLUDED
#define T_CURVES_INCLUDED

#include "tgeometry.h"

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
/*!
  TSegment Class to manage a segment
 */
class DVAPI TSegment {
protected:
  TPointD m_c0, m_c1;

public:
  TSegment() : m_c0(), m_c1(){};

  //! p0,p1  are the two control points
  TSegment(const TPointD &p0, const TPointD &p1) : m_c0(p0), m_c1(p1 - p0) {}

  TSegment(double x0, double y0, double x1, double y1)
      : m_c0(x0, y0), m_c1(x1 - x0, y1 - y0) {}

  TSegment(const TSegment &src) : m_c0(src.m_c0), m_c1(src.m_c1) {}

  //! Return the point of segment at parameter \b t.
  TPointD getPoint(double t) const { return m_c0 + t * m_c1; };

  //! Return speed of segment.
  TPointD getSpeed(double = 0) const { return m_c1; };

  //! Return first control point \b P0.
  TPointD getP0() const { return m_c0; }
  //! Return second control point \b P1.
  TPointD getP1() const { return m_c0 + m_c1; }

  //! Set the value of first control point \b P0 to \b p.
  void setP0(const TPointD &p) {
    m_c1 += m_c0 - p;
    m_c0 = p;
  }
  //! Set the value of second control point \b P1 to \b p.
  void setP1(const TPointD &p) { m_c1 = p - m_c0; }

  bool operator==(const TSegment &c) const {
    return m_c0 == c.m_c0 && m_c1 == c.m_c1;
  };

  bool operator!=(const TSegment &c) const { return !operator==(c); };

  //! Return rect that contains the segment.
  TRectD getBBox() const { return TRectD(getP0(), getP1()); }

  //! Return the length of segment
  double getLength() const { return norm(m_c1); }

  //! Return true if segment is a point
  bool isPoint(double err2 = TConsts::epsilon) const {
    return norm2(m_c1) < err2;
  }
};

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TSegment &segment);

//-----------------------------------------------------------------------------

//!\relates TAffine, TSegment
inline TSegment operator*(const TAffine &aff, const TSegment &seg) {
  return TSegment(aff * seg.getP0(), aff * seg.getP1());
}

//=============================================================================
/*!
   TQuadratic class to manage a quadratic
 */
class DVAPI TQuadratic {
protected:
  TPointD m_p0, m_p1, m_p2;

public:
  TQuadratic() : m_p0(), m_p1(), m_p2() {}

  //! p0,p1,p2 are the three control points
  TQuadratic(const TPointD &p0, const TPointD &p1, const TPointD &p2)
      : m_p0(p0), m_p1(p1), m_p2(p2) {}

  TQuadratic(const TQuadratic &src)
      : m_p0(src.m_p0), m_p1(src.m_p1), m_p2(src.m_p2) {}

  TQuadratic &operator=(const TQuadratic &src) {
    m_p0 = src.m_p0;
    m_p1 = src.m_p1;
    m_p2 = src.m_p2;
    return *this;
  }

  //! Return the point of quadratic at parameter \b t.
  TPointD getPoint(double t) const;
  double getX(double t) const;
  double getY(double t) const;

  //! Return speed of quadratic at parameter \b t.
  TPointD getSpeed(double t) const {
    return 2 * ((t - 1) * m_p0 + (1.0 - 2.0 * t) * m_p1 + t * m_p2);
  }

  //! Return the y value of quadratic speed at parameter \b t.
  double getSpeedY(double t) const {
    return 2 * ((t - 1) * m_p0.y + (1.0 - 2.0 * t) * m_p1.y + t * m_p2.y);
  }

  //! Return acceleration of quadratic.
  TPointD getAcceleration(double = 0) const {
    return 2.0 * (m_p0 + m_p2 - 2.0 * m_p1);
  }

  //! Return curvature of quadratic at parameter \b t.
  /*! Calcolo della curvatura per una Quadratica.
  Vedi Farin pag.176 per la spiegazione della formula
  usata.
*/
  double getCurvature(double t) const;

  //! Return first control point \b P0.
  TPointD getP0() const { return m_p0; }
  //! Return second control point \b P1.
  TPointD getP1() const { return m_p1; }
  //! Return third control point \b P2.
  TPointD getP2() const { return m_p2; }

  //! Set the value of first control point \b P0 to \b p.
  void setP0(const TPointD &p) { m_p0 = p; }
  //! Set the value of second control point \b P1 to \b p.
  void setP1(const TPointD &p) { m_p1 = p; }
  //! Set the value of third control point \b P2 to \b p.
  void setP2(const TPointD &p) { m_p2 = p; }

  bool operator==(const TQuadratic &c) const {
    return m_p0 == c.m_p0 && m_p1 == c.m_p1 && m_p2 == c.m_p2;
  }

  bool operator!=(const TQuadratic &c) const { return !operator==(c); }

  //! Return a parameter that correspond to the point \b p in the quadratic.
  double getT(const TPointD &p) const;

  int getX(double y, double &x0, double &x1) const;

  int getY(double x, double &y0, double &y1) const;

  /*!
N.B. if t==0 o t==1 return a quadratic with all points equal
*/
  void split(double t, TQuadratic &first, TQuadratic &second) const;

  //! Return rect that contains the quadratic.
  TRectD getBBox() const;

  /*!
Return length of an arc between parameters t0 t1.
N.B. Length returned is always positive.
\note t0 and t1 are clamped to [0,1] interval
\note if t0>=t1 returns 0
*/
  double getLength(double t0, double t1) const;

  double getLength(double t1 = 1) const { return getLength(0, t1); }

  double getApproximateLength(double t0, double t1, double error) const;

  void reverse() {
    TPointD app;
    app  = m_p0;
    m_p0 = m_p2;
    m_p2 = app;
  }
};

//-------------------------------------------------------

//!\relates TAffine, TQuadratic
inline TQuadratic operator*(const TAffine &aff, const TQuadratic &curve) {
  TQuadratic quad;
  quad.setP0(aff * curve.getP0());
  quad.setP1(aff * curve.getP1());
  quad.setP2(aff * curve.getP2());
  return quad;
}

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TQuadratic &curve);

//-----------------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out, const TQuadratic *curve) {
  assert(curve);
  return out << *curve;
}

//=============================================================================
/*!
   TCubic class to manage a cubic
 */
class DVAPI TCubic {
protected:
  TPointD m_p0, m_p1, m_p2, m_p3;

public:
  TCubic() : m_p0(), m_p1(), m_p2(), m_p3() {}

  //! p0,p1,p2,p3 are the four control points
  TCubic(const TPointD &p0, const TPointD &p1, const TPointD &p2,
         const TPointD &p3)
      : m_p0(p0), m_p1(p1), m_p2(p2), m_p3(p3) {}

  TCubic(const TCubic &src)
      : m_p0(src.m_p0), m_p1(src.m_p1), m_p2(src.m_p2), m_p3(src.m_p3) {}

  TCubic &operator=(const TCubic &src) {
    m_p0 = src.m_p0;
    m_p1 = src.m_p1;
    m_p2 = src.m_p2;
    m_p3 = src.m_p3;
    return *this;
  }

  //! Return first control point \b P0.
  TPointD getP0() const { return m_p0; }
  //! Return second control point \b P1.
  TPointD getP1() const { return m_p1; }
  //! Return third control point \b P2.
  TPointD getP2() const { return m_p2; }
  //! Return fourth control point \b P3.
  TPointD getP3() const { return m_p3; }

  //! Set the value of first control point \b P0 to \b p.
  void setP0(const TPointD &p0) { m_p0 = p0; }
  //! Set the value of second control point \b P1 to \b p.
  void setP1(const TPointD &p1) { m_p1 = p1; }
  //! Set the value of third control point \b P2 to \b p.
  void setP2(const TPointD &p2) { m_p2 = p2; }
  //! Set the value of fourth control point \b P3 to \b p.
  void setP3(const TPointD &p3) { m_p3 = p3; }

  bool operator==(const TCubic &c) const {
    return m_p0 == c.m_p0 && m_p1 == c.m_p1 && m_p2 == c.m_p2 && m_p3 == c.m_p3;
  }

  bool operator!=(const TCubic &c) const { return !operator==(c); }

  //! Return rect that contains the cubic.
  TRectD getBBox() const {
    return TRectD(std::min({m_p0.x, m_p1.x, m_p2.x, m_p3.x}),
                  std::min({m_p0.y, m_p1.y, m_p2.y, m_p3.y}),
                  std::max({m_p0.x, m_p1.x, m_p2.x, m_p3.x}),
                  std::max({m_p0.y, m_p1.y, m_p2.y, m_p3.y}));
  };

  //! Return the point of cubic at parameter \b t.
  TPointD getPoint(double t) const;

  //! Return speed of cubic at parameter \b t.
  TPointD getSpeed(double t) const;

  //! Return acceleration of cubic at parameter \b t.
  TPointD getAcceleration(double t) const {
    return 6.0 *
           ((m_p2 - 2 * m_p1 + m_p0) * (1 - t) + (m_p3 - 2 * m_p2 + m_p1) * t);
  };

  /*!
Return length of an arc between parameters t0 t1.
N.B. Length returned is always positive.
\note t0 and t1 are clamped to [0,1] interval
\note if t0>=t1 returns 0
*/
  double getLength(double t0, double t1) const;

  inline double getLength(double t1 = 1.0) const { return getLength(0.0, t1); }

  /*!
N.B. if t==0 o t==1 return a quadratic with all points equal
*/
  void split(double t, TCubic &first, TCubic &second) const;
};

//-----------------------------------------------------------------------------

//!\relates TAffine, TCubic
inline TCubic operator*(const TAffine &aff, const TCubic &curve) {
  TCubic out;
  out.setP0(aff * curve.getP0());
  out.setP1(aff * curve.getP1());
  out.setP2(aff * curve.getP2());
  out.setP3(aff * curve.getP3());
  return out;
}

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TCubic &curve);

//-----------------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out, const TCubic *curve) {
  assert(curve);
  return out << *curve;
}

//=============================================================================
/*!
  TSegment Class to manage a segment with thickness
  \!relates TSegment
 */
class DVAPI TThickSegment final : public TSegment {
protected:
  double m_thickP0;
  double m_thickP1;

public:
  //! m_thickP0, m_thickP1 are thickness of segment control points
  TThickSegment() : TSegment(), m_thickP0(0), m_thickP1(0) {}

  TThickSegment(const TThickSegment &thickSegment)
      : TSegment(thickSegment)
      , m_thickP0(thickSegment.m_thickP0)
      , m_thickP1(thickSegment.m_thickP1) {}

  TThickSegment(const TSegment &seg)
      : TSegment(seg), m_thickP0(0), m_thickP1(0) {}

  TThickSegment(const TPointD &p0, double thickP0, const TPointD &p1,
                double thickP1)
      : TSegment(p0, p1), m_thickP0(thickP0), m_thickP1(thickP1) {}

  TThickSegment(const TThickPoint &thickP0, const TThickPoint &thickP1)
      : TSegment(thickP0, thickP1)
      , m_thickP0(thickP0.thick)
      , m_thickP1(thickP1.thick) {}

  TThickSegment &operator=(const TThickSegment &other) {
    m_c0.x    = other.m_c0.x;
    m_c1.x    = other.m_c1.x;
    m_c0.y    = other.m_c0.y;
    m_c1.y    = other.m_c1.y;
    m_thickP0 = other.m_thickP0;
    m_thickP1 = other.m_thickP1;
    return *this;
  }

  //! Set the value of first control point \b P0 to \b TThickPoint \b p0.
  void setThickP0(const TThickPoint &p0) {
    m_c0.x    = p0.x;
    m_c0.y    = p0.y;
    m_thickP0 = p0.thick;
  }
  //! Set the value of second control point \b P1 to \b TThickPoint \b p1.
  void setThickP1(const TThickPoint &p1) {
    m_c1.x    = p1.x - m_c0.x;
    m_c1.y    = p1.y - m_c0.y;
    m_thickP1 = p1.thick;
  }

  //! Return first control point \b P0.
  TThickPoint getThickP0() const { return TThickPoint(m_c0, m_thickP0); }
  //! Return second control point \b P1.
  TThickPoint getThickP1() const { return TThickPoint(m_c0 + m_c1, m_thickP1); }

  //! Return the \b TThickPoint of segment at parameter \b t.
  TThickPoint getThickPoint(double t) const {
    return TThickPoint(m_c0 + t * m_c1, (1 - t) * m_thickP0 + t * m_thickP1);
  }
};

//-----------------------------------------------------------------------------

inline TThickSegment operator*(const TAffine &aff, const TThickSegment &ts) {
  TThickSegment out(ts);
  out.setP0(aff * ts.getP0());
  out.setP1(aff * ts.getP1());
  return out;
}

//-----------------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TThickSegment &segment);

//-----------------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out,
                                const TThickSegment *segment) {
  assert(segment);
  return out << *segment;
}

//=============================================================================

/*!
  Class TThickQuadratic: manage a curve with thick
  \!relates TQuadratic
 */
class DVAPI TThickQuadratic final : public TQuadratic {
protected:
  double m_thickP0;
  double m_thickP1;
  double m_thickP2;

public:
  TThickQuadratic();

  //! thickP0, thickP1, thickP2 are thickness of quadratic control points
  TThickQuadratic(const TPointD &p0, double thickP0, const TPointD &p1,
                  double thickP1, const TPointD &p2, double thickP2);

  TThickQuadratic(const TThickPoint &p0, const TThickPoint &p1,
                  const TThickPoint &p2);

  TThickQuadratic(const TThickQuadratic &thickQuadratic);

  TThickQuadratic(const TQuadratic &quadratic);

  //! Set the value of first control point \b P0 to \b TThickPoint \b p.
  void setThickP0(const TThickPoint &p);
  //! Set the value of second control point \b P1 to \b TThickPoint \b p.
  void setThickP1(const TThickPoint &p);
  //! Set the value of third control point \b P2 to \b TThickPoint \b p.
  void setThickP2(const TThickPoint &p);

  //! Return the \b TThickPoint of quadratic at parameter \b t.
  TThickPoint getThickPoint(double t) const;

  //! Return first control point \b P0.
  TThickPoint getThickP0() const { return TThickPoint(m_p0, m_thickP0); }
  //! Return second control point \b P1.
  TThickPoint getThickP1() const { return TThickPoint(m_p1, m_thickP1); }
  //! Return third control point \b P2.
  TThickPoint getThickP2() const { return TThickPoint(m_p2, m_thickP2); }

  void split(double t, TThickQuadratic &first, TThickQuadratic &second) const;

  TRectD getBBox() const;
};

//---------------------------------------------------------------------

inline TThickQuadratic operator*(const TAffine &aff,
                                 const TThickQuadratic &tq) {
  TThickQuadratic out(tq);
  out.setP0(aff * tq.getP0());
  out.setP1(aff * tq.getP1());
  out.setP2(aff * tq.getP2());

  return out;
}

//---------------------------------------------------------------------

inline TThickQuadratic transformQuad(const TAffine &aff,
                                     const TThickQuadratic &tq,
                                     bool doChangeThickness = false) {
  if (!doChangeThickness) return aff * tq;

  TThickQuadratic out(tq);
  double det = aff.det();
  det        = (det < 0) ? sqrt(-det) : sqrt(det);

  out.setThickP0(TThickPoint(aff * tq.getP0(), tq.getThickP0().thick * det));
  out.setThickP1(TThickPoint(aff * tq.getP1(), tq.getThickP1().thick * det));
  out.setThickP2(TThickPoint(aff * tq.getP2(), tq.getThickP2().thick * det));
  return out;
}

//---------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TThickQuadratic &tq);

//---------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out, const TThickQuadratic *tq) {
  assert(tq);
  return out << *tq;
}

//=============================================================================

/*!
  Class TThickCubic: manage a cubic with thick
  \!relates TCubic
 */
class DVAPI TThickCubic final : public TCubic {
protected:
  double m_thickP0;
  double m_thickP1;
  double m_thickP2;
  double m_thickP3;

public:
  TThickCubic();

  //! thickP0, thickP1, thickP2, thickP3 are thickness of cubic control points
  TThickCubic(const TPointD &p0, double thickP0, const TPointD &p1,
              double thickP1, const TPointD &p2, double thickP2,
              const TPointD &p3, double thickP3);

  TThickCubic(const TThickPoint &p0, const TThickPoint &p1,
              const TThickPoint &p2, const TThickPoint &p3);

  //  tonino ***************************************************************
  TThickCubic(const T3DPointD &p0, const T3DPointD &p1, const T3DPointD &p2,
              const T3DPointD &p3);

  //  tonino ***************************************************************

  TThickCubic(const TThickCubic &thickCubic);

  TThickCubic(const TCubic &cubic);

  //! Set the value of first control point \b P0 to \b TThickPoint \b p.
  void setThickP0(const TThickPoint &p);
  //! Set the value of second control point \b P1 to \b TThickPoint \b p.
  void setThickP1(const TThickPoint &p);
  //! Set the value of third control point \b P2 to \b TThickPoint \b p.
  void setThickP2(const TThickPoint &p);
  //! Set the value of fourth control point \b P3 to \b TThickPoint \b p.
  void setThickP3(const TThickPoint &p);

  //! Return the \b TThickPoint of cubic at parameter \b t.
  TThickPoint getThickPoint(double t) const;

  //! Return first control point \b P0.
  TThickPoint getThickP0() const { return TThickPoint(m_p0, m_thickP0); }
  //! Return second control point \b P1.
  TThickPoint getThickP1() const { return TThickPoint(m_p1, m_thickP1); }
  //! Return third control point \b P2.
  TThickPoint getThickP2() const { return TThickPoint(m_p2, m_thickP2); }
  //! Return fourth control point \b P3.
  TThickPoint getThickP3() const { return TThickPoint(m_p3, m_thickP3); }

  void split(double t, TThickCubic &first, TThickCubic &second) const;
};

//---------------------------------------------------------------------

inline TThickCubic operator*(const TAffine &aff, const TThickCubic &tc) {
  TThickCubic out(tc);
  out.setP0(aff * tc.getP0());
  out.setP1(aff * tc.getP1());
  out.setP2(aff * tc.getP2());
  out.setP3(aff * tc.getP3());
  return out;
}

//---------------------------------------------------------------------

DVAPI std::ostream &operator<<(std::ostream &out, const TThickCubic &tc);

//---------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &out, const TThickCubic *tc) {
  assert(tc);
  return out << *tc;
}

//=====================================================================

#endif  //__T_CURVES_INCLUDED
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
