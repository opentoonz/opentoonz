#pragma once

#ifndef PERMUTED_RANGE_H
#define PERMUTED_RANGE_H

// boost includes
#include <boost/range.hpp>
#include <boost/iterator/permutation_iterator.hpp>

/*!
  \file     permuted_range.h

  \brief    Contains a range creator for boost::permutation_iterator objects.
*/

namespace tcg {

template <typename ElemRng, typename IndxRng>
struct _perm_rng_traits {
  typedef boost::permutation_iterator<
      typename boost::range_iterator<ElemRng>::type,
      typename boost::range_iterator<IndxRng>::type>
      iterator;
  typedef std::pair<iterator, iterator> range;
};

//**********************************************************************************
//    Permuted Range  creator
//**********************************************************************************

template <typename ElementsRng, typename IndexesRng>
typename _perm_rng_traits<ElementsRng, const IndexesRng>::range permuted_range(
    ElementsRng &erng, const IndexesRng &irng) {
  typedef typename _perm_rng_traits<ElementsRng, const IndexesRng>::range range;
  typedef typename _perm_rng_traits<ElementsRng, const IndexesRng>::iterator
      iterator;

  return range(iterator(boost::begin(erng), boost::begin(irng)),
               iterator(boost::begin(erng), boost::end(irng)));
}

}  // namespace tcg

#endif  // PERMUTED_RANGE_H
