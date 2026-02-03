#pragma once

#ifndef TCG_POLYLINE_OPS
#define TCG_POLYLINE_OPS

// tcg includes
#include "traits.h"
#include "containers_reader.h"
#include "point.h"
#include "point_ops.h"

namespace tcg {
namespace polyline_ops {

//**************************************************************************************
//    Polyline Basic Operations
//**************************************************************************************

/*!
  Computes the length of the polyline between the specified point iterators.
  \return   The input polyline's length
*/

template <typename ForIt>
double length(ForIt begin, ForIt end) {
  typedef typename std::iterator_traits<ForIt>::value_type point_type;

  double result = 0.0;

  for (ForIt jt = begin, it = ++jt; jt != end; it = jt, ++jt)
    result += tcg::point_ops::dist(*it, *jt);

  return result;
}

//-----------------------------------------------------------------------------

/*!
  Computes the area enclosed by the input polyline's \a closure.

  \note    The input polyline is implicitly \a connected at the endpoints
           with a straight segment. This has obviously makes no difference
           if the supplied polyline already had coincident endpoints.

  \return  The area enclosed by the polyline's \a closure.
*/

template <typename ForIt>
double area(ForIt begin, ForIt end) {
  typedef typename std::iterator_traits<ForIt>::value_type point_type;

  double result = 0.0;

  if (begin != end) {
    ForIt jt = begin, it = jt++;

    for (; jt != end; it = jt++)
      result += 0.5 * (tcg::point_traits<point_type>::y(*jt) +
                       tcg::point_traits<point_type>::y(*it)) *
                (tcg::point_traits<point_type>::x(*jt) -
                 tcg::point_traits<point_type>::x(*it));

    result += 0.5 * (tcg::point_traits<point_type>::y(*begin) +
                     tcg::point_traits<point_type>::y(*it)) *
              (tcg::point_traits<point_type>::x(*begin) -
               tcg::point_traits<point_type>::x(*it));
  }

  return result;
}

//**************************************************************************************
//    Quadratic Conversions
//**************************************************************************************

/*!
  Standard direct conversion function used in polyline-to-quadratics conversion.

  Point a has already been inserted in the output; this function must add the
  remaining part of a sequence of quadratics approximating the triplet (a, *bt,
  c).
*/

// Note: typename iter_type::value_type == point_type
template <typename point_type, typename iter_type>
void tripletToQuadratics(
    const point_type &a, const iter_type &bt, const point_type &c,
    tcg::sequential_reader<std::vector<point_type>> &output) {
  // Direct conversion
  output.addElement(*bt);
  output.addElement(c);
}

/*!
  Performs a conversion of the specified polyline into a sequence of quadratics,
then
  applies a quadratics sub-sequence optimal merging algorithm.

  A user-made local triplet-to-quadratics conversion can be supplied to
recognize corners
  or supply a tight starting approximation.

\warning Passed polylines with equal endpoints are interpreted as closed
(circular) polylines;
         in this case, the resulting endpoints of the quadratic sequence will be
displaced to
         the first segment mid-point.
*/

template <typename iter_type, typename containers_reader, typename toQuadsFunc>
void toQuadratics(
    iter_type begin, iter_type end, containers_reader &output,
    toQuadsFunc &toQuads =
        &tripletToQuadratics<typename iter_type::value_type, iter_type>,
    double mergeTol = 1.0);

//**************************************************************************************
//    Standard Polyline Evaluators
//**************************************************************************************

/*!
  Calculates the (weighted) standard deviation of a polyline's sub-paths with
  respect
  to the segment connecting the endpoints.

  \warning For efficiency reasons, the returned value is the actual standard
  deviation, times the endpoints-segment length.
*/

template <typename RanIt>
class StandardDeviationEvaluator {
public:
  typedef RanIt iterator_type;
  typedef typename std::iterator_traits<RanIt>::difference_type diff_type;
  typedef typename std::iterator_traits<RanIt>::value_type point_type;
  typedef typename tcg::point_traits<point_type>::value_type value_type;
  typedef double penalty_type;

protected:
  iterator_type m_begin, m_end;

  std::vector<double> m_sums_x, m_sums_y;  //!< Sums of the points coordinates
  std::vector<double> m_sums2_x,
      m_sums2_y;                  //!< Sums of the points coordinates' squares
  std::vector<double> m_sums_xy;  //!< Sums of the coordinates products

public:
  StandardDeviationEvaluator(const iterator_type &begin,
                             const iterator_type &end);

  penalty_type penalty(const iterator_type &a, const iterator_type &b);

  const iterator_type &begin() const { return m_begin; }
  const iterator_type &end() const { return m_end; }

  const std::vector<double> &sums_x() const { return m_sums_x; }
  const std::vector<double> &sums_y() const { return m_sums_y; }
  const std::vector<double> &sums2_x() const { return m_sums2_x; }
  const std::vector<double> &sums2_y() const { return m_sums2_y; }
  const std::vector<double> &sums_xy() const { return m_sums_xy; }
};
}
}  // namespace tcg::polyline_ops

#ifdef INCLUDE_HPP
#include "hpp/polyline_ops.hpp"
#endif

#endif  // TCG_POLYLINE_OPS
