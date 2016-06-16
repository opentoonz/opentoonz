// #pragma once // could not use by INCLUDE_HPP

#ifndef RASTER_EDGE_EVALUATOR_H
#define RASTER_EDGE_EVALUATOR_H

#include "tcg/tcg_sequence_ops.h"
#include "tcg/tcg_polylineops.h"

//*************************************************************************
//    Standard Raster Edge Evaluator
//*************************************************************************

/*!
  \brief This class implements an evaluator for tcg's sequential reduction
algorithm
  that can be used to simplify the borders extracted from a raster image under
  a specified tolerance factor. This is typically used as a step in
polygonal-based
  image vectorization processes.

\sa The tcg::sequence_ops::minimalPath() function.
*/
template <typename RanIt>
class RasterEdgeEvaluator
    : public tcg::polyline_ops::StandardDeviationEvaluator<RanIt> {
  double m_tolerance;  //!< Maximal distance of an edge from one of the
                       //!< points it approximates, in the Manhattan metric
  double m_maxLength;  //!< Maximal length of an acceptable edge length,
                       //!< in the standard metric
public:
  typedef typename tcg::polyline_ops::StandardDeviationEvaluator<
      RanIt>::iterator_type iterator_type;
  typedef
      typename tcg::polyline_ops::StandardDeviationEvaluator<RanIt>::point_type
          point_type;
  typedef typename tcg::polyline_ops::StandardDeviationEvaluator<
      RanIt>::penalty_type penalty_type;

public:
  RasterEdgeEvaluator(const iterator_type &begin, const iterator_type &end,
                      double tolerance, double maxLength);

  iterator_type furthestFrom(const iterator_type &it);
  penalty_type penalty(const iterator_type &a, const iterator_type &b);
};

#endif  // RASTER_EDGE_EVALUATOR_H

#ifdef INCLUDE_HPP
#include "raster_edge_evaluator.hpp"
#endif
