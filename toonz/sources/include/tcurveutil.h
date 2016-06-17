#pragma once

#ifndef TCURVES_UTIL_INCLUDED
#define TCURVES_UTIL_INCLUDED

//#include "tutil.h"
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
// forwards declarations

class TSegment;
class TQuadratic;
class TThickQuadratic;

//=============================================================================

/*! area (orientata) del trapeziode limitato dalla curva
    e dall'asse delle ascisse. L'area e' positiva se p(0),...p(t)...,p(1),p(0)
      viene percorso in senso antiorario
DVAPI double getArea(const TQuadratic &curve);
*/

/*! Returns true if the min distance between \b point an \b segment is less o
 * equal to \b distance
*/
DVAPI bool isCloseToSegment(const TPointD &point, const TSegment &segment,
                            double distance);

/*!
  Compute min distance between a segment and a point
  */
DVAPI double tdistance(const TSegment &segment, const TPointD &point);
inline double tdistance(const TPointD &point, const TSegment &segment) {
  return tdistance(segment, point);
}

/*!
  Compute intersection between segments;
  return the number of intersections (0/1/2/-1) and add them
  (as a param couple) to the vector 'intersections'
  \note
    if the segment intersections is larger than one point
    (i.e. the segments share a sub-segment) return 2 and
    in vector there are extremes of sub-segment.
 */

DVAPI int intersect(const TPointD &seg1p0, const TPointD &seg1p1,
                    const TPointD &seg2p0, const TPointD &seg2p1,
                    std::vector<DoublePair> &intersections);

DVAPI int intersect(const TSegment &first, const TSegment &second,
                    std::vector<DoublePair> &intersections);

/*!
  Compute intersection between quadratics;
  return the number of intersections (0-4) and add them
  (as a param couple) to the vector 'intersections'
 */
DVAPI int intersect(const TQuadratic &q0, const TQuadratic &q1,
                    std::vector<DoublePair> &intersections,
                    bool checksegments = true);

/*!
  Compute intersection between and a segment;
  return the number of intersections [0,2] and add them
  (as a param couple) to the vector 'intersections'.
  Remark:
    In pair "first" is for the first object and "second"
    its for the second.
 */
DVAPI int intersect(const TQuadratic &q, const TSegment &s,
                    std::vector<DoublePair> &intersections,
                    bool firstQuad = true);

inline int intersect(const TSegment &s, const TQuadratic &q,
                     std::vector<DoublePair> &intersections) {
  return intersect(q, s, intersections, false);
}

template <class T>
void split(const T &tq, const std::vector<double> &pars, std::vector<T *> &v) {
  if (pars.empty()) return;

  T *q1, q2;

  UINT i;

  q1 = new T();
  tq.split(pars[0], *q1, q2);
  v.push_back(q1);

  for (i = 1; i < pars.size(); ++i) {
    double newPar = (pars[i] - pars[i - 1]) / (1.0 - pars[i - 1]);

    q1 = new T();
    q2.split(newPar, *q1, q2);
    v.push_back(q1);
  }

  v.push_back(new T(q2));
}

template <class T>
void split(const T &tq, double w0, double w1, T &qOut) {
  T q2;

  assert(w0 <= w1);

  if ((w1 - w0 == 0.0) && w0 == 1.0) {
    tq.split(w0, q2, qOut);
    return;
  }

  tq.split(w0, qOut, q2);

  double newPar = (w1 - w0) / (1.0 - w0);

  q2.split(newPar, qOut, q2);
}

DVAPI double computeStep(const TQuadratic &quad, double pixelSize);

DVAPI double computeStep(const TThickQuadratic &quad, double pixelSize);

//=============================================================================

/*!
  TQuadraticLengthEvaulator is an explicit length builder that for a specified
  quadratic.

  The purpose of a dedicated evaluator for the length of a quadratic is that of
  minimizing its computational cost.
  Both assigning a quadratic to the evaluator and retrieving its length up
  to a given parameter cost 1 sqrt and 1 log.
*/
class TQuadraticLengthEvaluator {
  double m_c, m_e, m_f, m_sqrt_a_div_2, m_tRef, m_primitive_0;
  bool m_constantSpeed, m_noSpeed0, m_squareIntegrand;

public:
  TQuadraticLengthEvaluator() {}
  TQuadraticLengthEvaluator(const TQuadratic &quad) { setQuad(quad); }

  void setQuad(const TQuadratic &quad);
  double getLengthAt(double t) const;
};

#endif
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
