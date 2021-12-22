#pragma once

#ifndef TCG_POLYLINE_OPS_HPP
#define TCG_POLYLINE_OPS_HPP

// tcg includes
#include "../polyline_ops.h"
#include "../iterator_ops.h"
#include "../sequence_ops.h"
#include "../point_ops.h"

// STD includes
#include <assert.h>

namespace tcg {
namespace polyline_ops {

using tcg::point_ops::operator/;

//***********************************************************************************
//    Standard Deviation Evaluator
//***********************************************************************************

template <typename RanIt>
StandardDeviationEvaluator<RanIt>::StandardDeviationEvaluator(
    const RanIt &begin, const RanIt &end)
    : m_begin(begin), m_end(end) {
  // Let m_sum[i] and m_sum2[i] be respectively the sums of vertex coordinates
  //(relative to begin is sufficient) from 0 to i, and the sums of their
  // squares;
  // m_sumsMix contain sums of xy terms.

  diff_type i, n = m_end - m_begin;
  diff_type n2 = n * 2;

  m_sums_x.resize(n);
  m_sums_y.resize(n);
  m_sums2_x.resize(n);
  m_sums2_y.resize(n);
  m_sums_xy.resize(n);

  m_sums_x[0] = m_sums_y[0] = m_sums2_x[0] = m_sums2_y[0] = m_sums_xy[0] = 0.0;

  // Build sums following the path

  point_type posToBegin;
  i = 0;

  iterator_type a = m_begin;
  for (a = m_begin, ++a; a != m_end; ++a, ++i) {
    posToBegin = point_type(a->x - m_begin->x, a->y - m_begin->y);

    m_sums_x[i + 1]  = m_sums_x[i] + posToBegin.x;
    m_sums_y[i + 1]  = m_sums_y[i] + posToBegin.y;
    m_sums2_x[i + 1] = m_sums2_x[i] + sq(posToBegin.x);
    m_sums2_y[i + 1] = m_sums2_y[i] + sq(posToBegin.y);
    m_sums_xy[i + 1] = m_sums_xy[i] + posToBegin.x * posToBegin.y;
  }
}

//------------------------------------------------------------------------------------

template <typename RanIt>
typename StandardDeviationEvaluator<RanIt>::penalty_type
StandardDeviationEvaluator<RanIt>::penalty(const iterator_type &a,
                                           const iterator_type &b) {
  diff_type aIdx = a - m_begin, bIdx = b - m_begin;
  point_type v(b->x - a->x, b->y - a->y),
      a_(a->x - m_begin->x, a->y - m_begin->y);

  double n      = b - a;  // Needs to be of higher precision than diff_type
  double sumX   = m_sums_x[bIdx] - m_sums_x[aIdx];
  double sumY   = m_sums_y[bIdx] - m_sums_y[aIdx];
  double sum2X  = m_sums2_x[bIdx] - m_sums2_x[aIdx];
  double sum2Y  = m_sums2_y[bIdx] - m_sums2_y[aIdx];
  double sumMix = m_sums_xy[bIdx] - m_sums_xy[aIdx];

  if (bIdx < aIdx) {
    int count = m_end - m_begin, count_1 = count - 1;

    n += count;
    sumX += m_sums_x[count_1];
    sumY += m_sums_y[count_1];
    sum2X += m_sums2_x[count_1];
    sum2Y += m_sums2_y[count_1];
    sumMix += m_sums_xy[count_1];
  }

  double A = sum2Y - 2.0 * sumY * a_.y + n * sq(a_.y);
  double B = sum2X - 2.0 * sumX * a_.x + n * sq(a_.x);
  double C = sumMix - sumX * a_.y - sumY * a_.x + n * a_.x * a_.y;

  return sqrt((v.x * v.x * A + v.y * v.y * B - 2 * v.x * v.y * C) / n);
}

//***********************************************************************************
//    Quadratics approximation Evaluator
//***********************************************************************************

template <typename Point>
class _QuadraticsEdgeEvaluator {
public:
  typedef Point point_type;
  typedef typename tcg::point_traits<point_type>::value_type value_type;
  typedef typename std::vector<Point>::iterator cp_iterator;
  typedef typename tcg::step_iterator<cp_iterator> quad_iterator;
  typedef double penalty_type;

private:
  quad_iterator m_begin, m_end;
  penalty_type m_tol;

public:
  _QuadraticsEdgeEvaluator(const quad_iterator &begin, const quad_iterator &end,
                           penalty_type tol);

  quad_iterator furthestFrom(const quad_iterator &a);
  penalty_type penalty(const quad_iterator &a, const quad_iterator &b);
};

//---------------------------------------------------------------------------

template <typename Point>
_QuadraticsEdgeEvaluator<Point>::_QuadraticsEdgeEvaluator(
    const quad_iterator &begin, const quad_iterator &end, penalty_type tol)
    : m_begin(begin), m_end(end), m_tol(tol) {}

//---------------------------------------------------------------------------

template <typename Point>
typename _QuadraticsEdgeEvaluator<Point>::quad_iterator
_QuadraticsEdgeEvaluator<Point>::furthestFrom(const quad_iterator &at) {
  const point_type &A  = *at;
  const point_type &A1 = *(at.it() + 1);

  // Build at (opposite) side
  int atSide_        = -tcg::numeric_ops::sign(cross(A - A1, *(at + 1) - A1));
  bool atSideNotZero = (atSide_ != 0);

  quad_iterator bt,
      last = this->m_end - 1;          // Don't do the last (it's a dummy quad)
  for (bt = at + 1; bt != last; ++bt)  // Always allow 1 step
  {
    // Trying to reach (bt + 1) from at
    const point_type &C  = *(bt + 1);
    const point_type &C1 = *(bt.it() + 1);

    // Ensure that bt is not a corner
    if (std::abs(tcg::point_ops::cross(*(bt.it() - 1) - *bt, *(bt.it() + 1) - *bt)) >
        1e-3)
      break;

    // Ensure there is no sign inversion
    int btSide =
        tcg::numeric_ops::sign(tcg::point_ops::cross(*bt - C1, C - C1));
    if (atSideNotZero && btSide != 0 && btSide == atSide_) break;

    // Build the approximating new quad if any
    value_type s, t;
    tcg::point_ops::intersectionSegCoords(A, A1, C, C1, s, t, 1e-4);
    if (s == tcg::numeric_ops::NaN<value_type>()) {
      // A-A1 and C1-C are parallel. There are 2 cases:
      if ((A1 - A) * (C - C1) >= 0)
        // Either we're still on a straight line
        continue;
      else
        // Or, we just can't build the new quad
        break;
    }

    point_type B(A + s * (A1 - A));

    point_type A_B(A - B);
    point_type AC_2B(A_B + C - B);

    // Now, for each quadratic between at and bt, build the 'distance' from our
    // new
    // approximating quad (ABC)

    quad_iterator qt, end = bt + 1;
    for (qt = at; qt != end; ++qt) {
      const point_type &Q_A(*qt);
      const point_type &Q_B(*(qt.it() + 1));
      const point_type &Q_C(*(qt + 1));

      // Check the distance of Q_B from the ABC tangent whose direction
      // is the same as Q'_B - ie, Q_A -> Q_C.

      point_type dir(Q_C - Q_A);
      value_type dirNorm = tcg::point_ops::norm(dir);
      if (dirNorm < 1e-4) break;

      dir = dir / dirNorm;

      value_type den = tcg::point_ops::cross(AC_2B, dir);
      if (den < 1e-4 && den > -1e-4) break;

      value_type t = tcg::point_ops::cross(A_B, dir) / den;
      if (t < 0.0 || t > 1.0) break;

      value_type t1 = 1.0 - t;
      point_type P(sq(t1) * A + 2.0 * t * t1 * B + sq(t) * C);
      point_type Q(0.25 * Q_A + 0.5 * Q_B + 0.25 * Q_C);

      if (tcg::point_ops::lineDist(Q, P, dir) > m_tol) break;

      value_type pos = ((P - Q_A) * dir);
      if (pos < 0.0 || pos > dirNorm) break;
      /*if(pos < -m_tol || pos > dirNorm + m_tol)     //Should this be relaxed
too?
break;*/

      if (qt == bt) continue;

      // Check the distance of Q_C from the ABC tangent whose direction
      // is the same as Q'_C.

      dir = tcg::point_ops::direction(Q_B, Q_C);

      den = tcg::point_ops::cross(AC_2B, dir);
      if (den < 1e-4 && den > -1e-4) break;

      t = tcg::point_ops::cross(A_B, dir) / den;
      if (t < 0.0 || t > 1.0) break;

      t1 = 1.0 - t;
      P  = sq(t1) * A + 2.0 * t * t1 * B + sq(t) * C;

      if (tcg::point_ops::lineDist(Q_C, P, dir) > m_tol) break;
    }

    if (qt != end) break;  // Constraints were violated
  }

  return std::min(bt, this->m_end - 1);
}

//---------------------------------------------------------------------------

template <typename Point>
typename _QuadraticsEdgeEvaluator<Point>::penalty_type
_QuadraticsEdgeEvaluator<Point>::penalty(const quad_iterator &at,
                                         const quad_iterator &bt) {
  if (bt == at + 1) return 0.0;

  penalty_type penalty = 0.0;

  const point_type &A(*at);
  const point_type &A1(*(at.it() + 1));
  const point_type &C(*bt);
  const point_type &C1(*(bt.it() - 1));

  // Build B
  value_type s, t;
  tcg::point_ops::intersectionSegCoords(A, A1, C, C1, s, t, 1e-4);
  if (s == tcg::numeric_ops::NaN<value_type>()) return 0.0;

  point_type B(A + s * (A1 - A));

  // Iterate and build penalties
  point_type A_B(A - B);
  point_type AC_2B(A_B + C - B);

  quad_iterator qt, bt_1 = bt - 1;
  for (qt = at; qt != bt; ++qt) {
    const point_type &Q_A(*qt);
    const point_type &Q_B(*(qt.it() + 1));
    const point_type &Q_C(*(qt + 1));

    // point_type dir(tcg::point_ops::direction(Q_A, Q_C));
    point_type dir(Q_C - Q_A);
    dir = dir / tcg::point_ops::norm(dir);

    value_type t =
        tcg::point_ops::cross(A_B, dir) / tcg::point_ops::cross(AC_2B, dir);
    assert(t >= 0.0 && t <= 1.0);

    value_type t1 = 1.0 - t;
    point_type P(sq(t1) * A + 2.0 * t * t1 * B + sq(t) * C);
    point_type Q(0.25 * Q_A + 0.5 * Q_B + 0.25 * Q_C);

    penalty += tcg::point_ops::lineDist(Q, P, dir);

    if (qt == bt_1) continue;

    dir = tcg::point_ops::direction(Q_B, Q_C);

    t = tcg::point_ops::cross(A_B, dir) / tcg::point_ops::cross(AC_2B, dir);
    assert(t >= 0.0 && t <= 1.0);

    t1 = 1.0 - t;
    P  = sq(t1) * A + 2.0 * t * t1 * B + sq(t) * C;

    penalty += tcg::point_ops::lineDist(Q_C, P, dir);
  }

  return penalty;
}

//***********************************************************************************
//    Conversion to Quadratics functions
//***********************************************************************************

template <typename cps_reader>
class _QuadReader {
public:
  typedef typename cps_reader::value_type point_type;
  typedef typename tcg::point_traits<point_type>::value_type value_type;
  typedef typename std::vector<point_type>::iterator cps_iterator;
  typedef typename tcg::step_iterator<cps_iterator> quad_iterator;

private:
  cps_reader &m_reader;
  quad_iterator m_it;

public:
  _QuadReader(cps_reader &reader) : m_reader(reader) {}

  void openContainer(const quad_iterator &it) {
    m_reader.openContainer(*it);
    m_it = it;
  }

  void addElement(const quad_iterator &it) {
    if (it == m_it + 1) {
      m_reader.addElement(*(it.it() - 1));
      m_reader.addElement(*it);
    } else {
      const point_type &A(*m_it);
      const point_type &A1(*(m_it.it() + 1));
      const point_type &C(*it);
      const point_type &C1(*(it.it() - 1));

      // Build B
      value_type s, t;
      tcg::point_ops::intersectionSegCoords(A, A1, C, C1, s, t, 1e-4);
      point_type B((s == tcg::numeric_ops::NaN<value_type>())
                       ? 0.5 * (A + C)
                       : A + s * (A1 - A));

      m_reader.addElement(B);
      m_reader.addElement(C);
    }

    m_it = it;
  }

  void closeContainer() { m_reader.closeContainer(); }
};

//---------------------------------------------------------------------------

template <typename iter_type, typename Reader, typename tripleToQuadsFunc>
void _naiveQuadraticConversion(const iter_type &begin, const iter_type &end,
                               Reader &reader,
                               tripleToQuadsFunc &tripleToQuadsF) {
  typedef typename std::iterator_traits<iter_type>::value_type point_type;

  point_type a, c;
  iter_type it, jt;
  iter_type last(end);
  --last;

  if (*begin != *last) {
    reader.openContainer();
    reader.addElement(*begin);

    ++(it = begin);
    a = 0.5 * (*begin + *it);

    reader.addElement(0.5 * (*begin + a));
    reader.addElement(a);

    // Work out each quadratic
    for (++(jt = it); jt != end; it = jt, ++jt) {
      c = 0.5 * (*it + *jt);
      tripleToQuadsF(a, it, c, reader);
      a = c;
    }

    reader.addElement(0.5 * (a + *it));
    reader.addElement(*it);
  } else {
    ++(it = begin);
    point_type first = a = 0.5 * (*begin + *it);

    reader.openContainer();
    reader.addElement(a);

    for (++(jt = it); jt != end; it = jt, ++jt) {
      c = 0.5 * (*it + *jt);
      tripleToQuadsF(a, it, c, reader);
      a = c;
    }

    tripleToQuadsF(a, last, first, reader);
  }

  reader.closeContainer();
}

//---------------------------------------------------------------------------

template <typename iter_type, typename containers_reader, typename toQuadsFunc>
void toQuadratics(iter_type begin, iter_type end, containers_reader &output,
                  toQuadsFunc &toQuadsF, double reductionTol) {
  typedef typename std::iterator_traits<iter_type>::difference_type diff_type;
  typedef typename std::iterator_traits<iter_type>::value_type point_type;
  typedef typename tcg::point_traits<point_type>::value_type value_type;

  if (begin == end) return;

  diff_type count = std::distance(begin, end);
  if (count < 2) {
    // Single point - add 2 points on top of it and quit.
    output.openContainer(*begin);
    output.addElement(*begin), output.addElement(*begin);
    output.closeContainer();
    return;
  }

  if (count == 2) {
    // Segment case
    iter_type it = begin;
    ++it;

    output.openContainer(*begin);
    output.addElement(0.5 * (*begin + *it)), output.addElement(*it);
    output.closeContainer();
    return;
  }

  // Build an intermediate vector of points containing the naive quadratic
  // conversion.

  std::vector<point_type> cps;
  tcg::sequential_reader<std::vector<point_type>> cpsReader(&cps);

  _naiveQuadraticConversion(begin, end, cpsReader, toQuadsF);

  if (reductionTol <= 0) {
    output.openContainer(*cps.begin());

    // Directly output the naive conversion
    typename std::vector<point_type>::iterator it, end = cps.end();
    for (it = ++cps.begin(); it != end; ++it) output.addElement(*it);
    output.closeContainer();

    return;
  }

  // Resize the cps to cover a multiple of 2
  cps.resize(cps.size() + 2 - (cps.size() % 2));

  // Now, launch the quadratics reduction procedure
  tcg::step_iterator<typename std::vector<point_type>::iterator> bt(cps.begin(),
                                                                    2),
      et(cps.end(), 2);

  _QuadraticsEdgeEvaluator<point_type> eval(bt, et, reductionTol);
  _QuadReader<containers_reader> quadReader(output);

  bool ret = tcg::sequence_ops::minimalPath(bt, et, eval, quadReader);
  assert(ret);
}
}
}  // namespace tcg::polyline_ops

#endif  // TCG_POLYLINE_OPS_HPP
