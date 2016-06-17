#pragma once

#ifndef RASTER_EDGE_EVALUATOR_HPP
#define RASTER_EDGE_EVALUATOR_HPP

#include "raster_edge_evaluator.h"

//*******************************************************************************
//    Raster Edge Evaluator implementation
//*******************************************************************************

template <typename RanIt>
RasterEdgeEvaluator<RanIt>::RasterEdgeEvaluator(const iterator_type &begin,
                                                const iterator_type &end,
                                                double tolerance,
                                                double maxLength)
    : tcg::polyline_ops::StandardDeviationEvaluator<RanIt>(begin, end)
    , m_tolerance(tolerance)
    , m_maxLength(maxLength) {}

//--------------------------------------------------------------------------

template <typename RanIt>
typename RasterEdgeEvaluator<RanIt>::penalty_type
RasterEdgeEvaluator<RanIt>::penalty(const iterator_type &a,
                                    const iterator_type &b) {
  return tcg::point_ops::norm(*b - *a) *
         tcg::polyline_ops::StandardDeviationEvaluator<RanIt>::penalty(a, b);
}

//--------------------------------------------------------------------------

template <typename RanIt>
typename RasterEdgeEvaluator<RanIt>::iterator_type
RasterEdgeEvaluator<RanIt>::furthestFrom(const iterator_type &start) {
  // Build the furthest possible forward difference for every vertex between
  // begin and end.
  point_type displace, oldDisplace;
  point_type leftConstraint, rightConstraint;
  point_type newLeftConstraint, newRightConstraint;
  point_type leftDirConstr, rightDirConstr, dir, oldDir;
  iterator_type it = start, jt;

  const double sqMaxLength = sq(m_maxLength);

  // Initialize search
  leftConstraint = rightConstraint = point_type();
  leftDirConstr = rightDirConstr = point_type();
  oldDir = oldDisplace = point_type();

  if (it != this->m_begin) --it;  // Chop left

  jt = it;
  for (++jt; jt != this->m_end; ++jt) {
    // Retrieve displacement from *it
    displace = point_type(jt->x - it->x, jt->y - it->y);
    dir = point_type(displace.x - oldDisplace.x, displace.y - oldDisplace.y);

    // Max length
    if (oldDir.x != 0 || oldDir.y != 0) {
      if (sq(displace.x) + sq(displace.y) > sqMaxLength) break;
    } else
      leftDirConstr = rightDirConstr = dir;

    // Test displacement against the oldDisplacement. If it's reversing the
    // direction, make it invalid.

    if (cross(oldDir, dir) > 0) leftDirConstr = dir;

    if (cross(oldDir, dir) < 0) rightDirConstr = dir;

    // Test constraints

    /*if(cross(rightDirConstr, leftDirConstr) <= 0 &&
leftDirConstr * rightDirConstr < 0)
break;*/
    if (cross(rightDirConstr, leftDirConstr) < 0) break;

    if (cross(displace, leftConstraint) < 0) break;
    if (cross(displace, rightConstraint) > 0) break;

    if (std::max({displace.x, -displace.x, displace.y, -displace.y}) >
        m_tolerance) {
      // Update m_tolerance constraints
      newLeftConstraint.x =
          displace.x + (displace.y < 0 || (displace.y == 0 && displace.x < 0)
                            ? m_tolerance
                            : -m_tolerance);
      newLeftConstraint.y =
          displace.y + (displace.x > 0 || (displace.x == 0 && displace.y < 0)
                            ? m_tolerance
                            : -m_tolerance);

      if (cross(newLeftConstraint, leftConstraint) >= 0)
        leftConstraint = newLeftConstraint;

      newRightConstraint.x =
          displace.x + (displace.y > 0 || (displace.y == 0 && displace.x < 0)
                            ? m_tolerance
                            : -m_tolerance);

      newRightConstraint.y =
          displace.y + (displace.x < 0 || (displace.x == 0 && displace.y < 0)
                            ? m_tolerance
                            : -m_tolerance);

      if (cross(newRightConstraint, rightConstraint) <= 0)
        rightConstraint = newRightConstraint;
    }

    oldDisplace = displace;
    oldDir      = dir;
  }

  if (jt != this->m_end) --jt;  // Chop Right

  return start +
         std::max(
             (int)std::min(jt - start - 1, this->m_end - this->m_begin - 2), 1);
}

#endif  // RASTER_EDGE_EVALUATOR_HPP
