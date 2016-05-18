#pragma once

#ifndef ZIPPED_RANGE_H
#define ZIPPED_RANGE_H

// boost includes
#include <boost/range.hpp>
#include <boost/iterator/zip_iterator.hpp>

/*!
  \file     zipped_range.h

  \brief    Contains a range creator for boost::zip_iterator objects.
  \remark   Zipped ranges are currently constrained to a 2-tuple.
*/

namespace tcg
{

template <typename Rng1, typename Rng2>
struct _zip_rng_traits {
	typedef boost::zip_iterator<boost::tuple<
		typename boost::range_iterator<Rng1>::type,
		typename boost::range_iterator<Rng2>::type>> iterator;
	typedef std::pair<iterator, iterator> range;
};

//**********************************************************************************
//    Permuted Range  creator
//**********************************************************************************

template <typename Rng1, typename Rng2>
typename _zip_rng_traits<Rng1, Rng2>::range
zipped_range(Rng1 &rng1, Rng2 &rng2)
{
	typedef typename _zip_rng_traits<Rng1, Rng2>::range range;
	typedef typename _zip_rng_traits<Rng1, Rng2>::iterator iterator;

	return range(iterator(boost::make_tuple(boost::begin(rng1), boost::begin(rng2))),
				 iterator(boost::make_tuple(boost::end(rng1), boost::end(rng2))));
}

} // namespace tcg

#endif // ZIPPED_RANGE_H
