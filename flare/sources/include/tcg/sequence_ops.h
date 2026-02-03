

#ifndef TCG_SEQUENCE_OPS_H
#define TCG_SEQUENCE_OPS_H

/*!
  \file     sequence_ops.h

  \brief    This file contains algorithms returning sub-sequences of
            some input iterator range.
*/

namespace tcg {
namespace sequence_ops {

//**************************************************************************************
//    Sequence Operations
//**************************************************************************************

/*!
  \brief    Base interface for an evaluator object supported by function
            \p tcg::minimalPath().
*/
template <typename It, typename Pen>
struct EdgeEvaluator {
  typedef It iterator_type;
  typedef Pen penalty_type;

public:
  EdgeEvaluator() {}
  virtual ~EdgeEvaluator() {}

  /*!
\brief    Computes the largest allowed position reachable in a single
        step from given position \a a.
*/
  virtual iterator_type furthestFrom(const iterator_type &a) = 0;

  /*!
\brief    Computes the penalty for the specified input step.
\remark   Supplied input step is ensured to be allowed, as guaranteed
        by function furthestFrom().
*/
  virtual penalty_type penalty(const iterator_type &a,
                               const iterator_type &b) = 0;
};

//------------------------------------------------------------------------------

/*!
  \brief    Builds the sequence's minimal subpath from the specified evaluator.

  \details  This function traverses the input sequence (end excluded) searching
  for the
            minimal allowed path with respect to the number of vertices
  (primarily)
            and the penalty associated to each path edge; returns true whether
  such a
            path could be found, false if \b no path from begin to --end could
  be
            established.

            The minimal path function applies a simplified Bellman optimality
  algorithm
            on a sequence, where graph edges are specified through an edge
  evaluator
            functor, rather than being built in a graph class.

            It works this way:

              \li The minimal number of edges required to traverse the sequence
  can be
                  found by traversing the sequence with the maximum step
  allowed.

              \li Each node have the minimal number of edges required to reach
  the
                  sequence end, the minimal penalty to it, and obviously the
  next node
                  which allow this optimal configuration.

              \li The optimal "edge number-penalty" value from one point sums
  with the
                  one of the next point in the optimal configuration; thus, it
  can be
                  found retroactively starting from the end.

              \li The path retrieved with the maximum step allowed defines the
                  remaining steps to achieve the optimal number of edges,
  starting from
                  the end.

  \remark     This function is currently working only for random access
  iterators.

  \sa         The EdgeEvaluator class for the supported evaluator interface.
*/

template <typename ranit_type, typename edge_eval, typename containers_reader>
bool minimalPath(ranit_type begin, ranit_type end, edge_eval &evaluator,
                 containers_reader &output);
}
}  // namespace tcg::sequence_ops

#endif  // TCG_SEQUENCE_OPS_H

#ifdef INCLUDE_HPP
#include "hpp/sequence_ops.hpp"
#endif
