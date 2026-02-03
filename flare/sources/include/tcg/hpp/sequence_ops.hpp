#pragma once

#ifndef TCG_SEQUENCE_OPS_HPP
#define TCG_SEQUENCE_OPS_HPP

#include "../sequence_ops.h"

#ifdef min
#undef min
#endif

namespace tcg {
namespace sequence_ops {

//***********************************************************************************
//    Minimal Path Functions
//***********************************************************************************

template <typename ranit_type, typename edge_eval, typename containers_reader>
bool minimalPath(ranit_type begin, ranit_type end, edge_eval &eval,
                 containers_reader &output) {
  typedef typename ranit_type::difference_type diff_type;
  typedef typename edge_eval::penalty_type penalty_type;

  ranit_type a, b;
  diff_type i, j, m, n = end - begin, last = n - 1;

  // Precache the longest edge possible from each vertex, imposing that furthest
  // nodes have increasing indices.

  std::vector<diff_type> furthest(n);

  diff_type currFurthest = furthest[last] = last;
  for (i = last - 1; i >= 0; --i) {
    currFurthest = furthest[i] =
        std::min(eval.furthestFrom(begin + i) - begin, currFurthest);
    if (currFurthest == i)
      return false;  // There exists no path from start to end - quit
  }

  // Iterate from begin to end, using the maximum step allowed. The number of
  // iterations is the number of output edges.

  for (i = 0, m = 0; i < last; i = furthest[i], ++m)
    ;

  // Also, build the iteration sequence. It will define the upper bounds for the
  // k-th vertex of the output.

  std::vector<diff_type> upperBound(m + 1);

  for (i = 0, j = 0; i <= m; j = furthest[j], ++i) upperBound[i] = j;

  // Now, the body of the algorithm

  std::vector<penalty_type> minPenaltyToEnd(n);
  std::vector<diff_type> minPenaltyNext(last);
  diff_type aIdx, bIdx;
  penalty_type newPenalty;

  minPenaltyToEnd[last] = 0;

  diff_type nextLowerBound;
  for (j = m - 1, nextLowerBound = last; j >= 0; --j) {
    // Build the minimal penalty to end (also storing the next iterator
    // leading to it) from each vertex of the polygon, assuming the minimal
    // number of edges from the vertex to end.

    // The j-th polygon vertex must lie in the [lowerBound, upperBound[j]]
    // interval, whereas the (j+1)-th will be in [nextLowerBound,
    // upperBound[j+1]].

    // Please, observe that we always have upperBound[j] < nextLowerBound due
    // to the minimal edge count constraint.

    for (aIdx = upperBound[j]; aIdx >= 0 && furthest[aIdx] >= nextLowerBound;
         --aIdx) {
      a = begin + aIdx;

      // Search the vertex next to a which minimizes the penalty to end - and
      // store it.
      minPenaltyToEnd[aIdx] = (std::numeric_limits<penalty_type>::max)();

      for (bIdx = nextLowerBound, b = begin + nextLowerBound;
           furthest[aIdx] >= bIdx; ++b, ++bIdx) {
        assert(minPenaltyToEnd[bIdx] <
               (std::numeric_limits<penalty_type>::max)());
        newPenalty = eval.penalty(a, b) + minPenaltyToEnd[bIdx];
        if (newPenalty < minPenaltyToEnd[aIdx]) {
          minPenaltyToEnd[aIdx] = newPenalty;
          minPenaltyNext[aIdx]  = bIdx;
        }
      }
    }

    // Update
    nextLowerBound = aIdx;
    ++nextLowerBound;
  }

  // Finally, build the output sequence
  output.openContainer(begin);
  for (i = minPenaltyNext[0], j = 1; j < m; i = minPenaltyNext[i], ++j)
    output.addElement(begin + i);
  output.addElement(begin + last);
  output.closeContainer();

  return true;
}
}
}  // namespace tcg::sequence_ops

#endif  // TCG_SEQUENCE_OPS_HPP
