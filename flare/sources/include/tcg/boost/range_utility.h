#pragma once

#ifndef TCG_BOOST_RANGE_UTILITY_H
#define TCG_BOOST_RANGE_UTILITY_H

// boost includes
#include <boost/range.hpp>

/*!
  \file     range_utility.h

  \brief    Contains utilities to be used with Boost.Range objects.
*/

namespace tcg {

//***************************************************************************
//    Range  utilities
//***************************************************************************

/*!
  \brief    Substitutes a container's content with the one inside
            a \a Boost range.
  \details  The container type requires construction with an
            iterator pair and the swap() method.
*/

template <typename C, typename Rng>
C &substitute(C &c, const Rng &rng) {
  C(boost::begin(rng), boost::end(rng)).swap(c);
  return c;
}

//------------------------------------------------------------------------

/*!
  \brief    Inserts a range inside an associative container.
  \details  The container type requires an insert() method
            accepting a pair of iterators.
  \note     For insertion in a sequential container, use
            <TT>boost::insert</TT>.
*/

template <typename C, typename Rng>
C &insert2(C &c, const Rng &rng) {
  c.insert(boost::begin(rng), boost::end(rng));
  return c;
}

}  // namespace tcg

#endif  // TCG_BOOST_RANGE_UTILITY_H
