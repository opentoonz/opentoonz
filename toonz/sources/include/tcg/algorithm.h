#pragma once

#ifndef TCG_ALGORITHM_H
#define TCG_ALGORITHM_H

// tcg includes
#include "traits.h"

// STD includes
#include <functional>

/*!
  \file     algorithm.h

  \brief    This file contains useful algorithms complementary to those
            in the standard \p \<algorithm\> and in \p boost::algorithm.
*/

namespace tcg
{

//***************************************************************************
//    Binary find  algorithms
//***************************************************************************

/*!
  \brief    Performs a binary search for the a value in a <I>sorted,
            random access</I> iterators range, and returns its position.

  \return   The \a first range position whose value is \a equivalent to
            the specified one.
*/
template <typename RanIt, typename T>
RanIt binary_find(RanIt begin,	//!< Start of the sorted range.
				  RanIt end,	  //!< End of the sorted range.
				  const T &value) //!< Value to look up.
{
	RanIt it = std::lower_bound(begin, end, value);
	return (it != end && !(value < *it)) ? it : end;
}

//---------------------------------------------------------------------

/*!
  \brief    Performs a binary search for the a value in a <I>sorted,
            random access</I> iterators range, and returns its position.

  \return   The \a first range position whose value is \a equivalent to
            the specified one.
*/
template <typename RanIt, typename T, typename Compare>
RanIt binary_find(RanIt begin,	//!< Start of the sorted range.
				  RanIt end,	  //!< End of the sorted range.
				  const T &value, //!< Value to look up.
				  Compare comp)   //!< Comparator functor sorting the range.
{
	RanIt it = std::lower_bound(begin, end, value, comp);
	return (it != end && !comp(value, *it)) ? it : end;
}

//***************************************************************************
//    Min/Max iterator range  algorithms
//***************************************************************************

/*!
  \brief    Calculates the minimal transformed element from the
            input iterators range.

  \return   The position of the minimal transform.

  \details  This function is similar to std::min_element(), but
            operating a unary transforming function on dereferenced
            objects.

            Furthermore, the minimal transformed value is cached
            during computation.
*/
template <typename ForIt, typename Func, typename Comp>
ForIt min_transform(ForIt begin, //!< Start of the input iterators range.
					ForIt end,   //!< End of the input iterators range.
					Func func,   //!< The transforming function.
					Comp comp)   //!< The comparator object for transformed values.
{
	typedef typename tcg::function_traits<Func>::ret_type ret_type;
	typedef typename tcg::remove_cref<ret_type>::type value_type;

	if (begin == end)
		return end;

	ForIt minPos = begin;
	value_type minimum = func(*begin);

	for (; begin != end; ++begin) {
		const value_type &candidate = func(*begin);

		if (comp(candidate, minimum))
			minPos = begin, minimum = candidate;
	}

	return minPos;
}

//---------------------------------------------------------------------

/*!
  \brief    Calculates the minimal transformed element from the
            input iterators range.

  \return   The position of the minimal transform.

  \remark   This variation uses \p operator< as comparator for the
            transformed values.
*/
template <typename ForIt, typename Func>
ForIt min_transform(ForIt begin, //!< Start of the input iterators range.
					ForIt end,   //!< End of the input iterators range.
					Func func)   //!< The transforming function.
{
	typedef typename tcg::function_traits<Func>::ret_type ret_type;
	typedef typename tcg::remove_cref<ret_type>::type value_type;

	return min_transform(begin, end, func, std::less<value_type>());
}

//---------------------------------------------------------------------

/*!
  \brief    Calculates the maximal transformed element from the
            input iterators range.

  \return   The position of the maximal transform.

  \sa       See min_transform() for a detailed explanation.
*/
template <typename ForIt, typename Func, typename Comp>
ForIt max_transform(ForIt begin, //!< Start of the input iterators range.
					ForIt end,   //!< End of the input iterators range.
					Func func,   //!< The transforming function.
					Comp comp)   //!< The comparator object for transformed values.
{
	typedef typename tcg::function_traits<Func>::ret_type ret_type;
	typedef typename tcg::remove_cref<ret_type>::type value_type;

	if (begin == end)
		return end;

	ForIt maxPos = begin;
	value_type maximum = func(*begin);

	for (; begin != end; ++begin) {
		const value_type &candidate = func(*begin);

		if (comp(maximum, candidate))
			maxPos = begin, maximum = candidate;
	}

	return maxPos;
}

//---------------------------------------------------------------------

/*!
  \brief    Calculates the maximal transformed element from the
            input iterators range.

  \return   The position of the maximal transform.

  \sa       See min_transform() for a detailed explanation.
*/
template <typename ForIt, typename Func>
ForIt max_transform(ForIt begin, //!< Start of the input iterators range.
					ForIt end,   //!< End of the input iterators range.
					Func func)   //!< The transforming function.
{
	typedef typename tcg::function_traits<Func>::ret_type ret_type;
	typedef typename tcg::remove_cref<ret_type>::type value_type;

	return max_transform(begin, end, func, std::less<value_type>());
}

} // namespace tcg

#endif // TCG_ALGORITHM_H
