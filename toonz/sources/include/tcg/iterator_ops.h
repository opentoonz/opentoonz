#pragma once

#ifndef TCG_ITERATOR_OPS_H
#define TCG_ITERATOR_OPS_H

// tcg includes
#include "traits.h"
#include "ptr.h"

// STD includes
#include <iterator>

namespace tcg {

//****************************************************************************
//    Traits
//****************************************************************************

template <typename It>
struct iterator_traits : public std::iterator_traits<It> {
  typedef It inheritable_iterator_type;
};

template <typename T>
struct iterator_traits<T *> : public std::iterator_traits<T *> {
  typedef ptr<T> inheritable_iterator_type;
};

//****************************************************************************
//    Derived Iterator  definition
//****************************************************************************

template <typename It, typename Der,
          typename iterator_tag =
              typename std::iterator_traits<It>::iterator_category>
struct derived_iterator
    : public tcg::iterator_traits<It>::inheritable_iterator_type {
  typedef typename tcg::iterator_traits<It>::inheritable_iterator_type
      base_iterator;

public:
  derived_iterator() : base_iterator() {}
  derived_iterator(const base_iterator &it) : base_iterator(it) {}

  Der &operator++() {
    base_iterator::operator++();
    return static_cast<Der &>(*this);
  }
  Der operator++(int) {
    return Der(base_iterator::operator++(0), static_cast<Der &>(*this));
  }
};

template <typename It, typename Der>
struct derived_iterator<It, Der, std::bidirectional_iterator_tag>
    : public derived_iterator<It, Der, std::forward_iterator_tag> {
  typedef typename tcg::iterator_traits<It>::inheritable_iterator_type
      base_iterator;

public:
  derived_iterator() : _iter() {}
  derived_iterator(const base_iterator &it) : _iter(it) {}

  Der &operator--() {
    base_iterator::operator--();
    return static_cast<Der &>(*this);
  }
  Der operator--(int) {
    return Der(base_iterator::operator--(0), static_cast<Der &>(*this));
  }

private:
  typedef derived_iterator<It, Der, std::forward_iterator_tag> _iter;
};

template <typename It, typename Der>
struct derived_iterator<It, Der, std::random_access_iterator_tag>
    : public derived_iterator<It, Der, std::bidirectional_iterator_tag> {
  typedef typename tcg::iterator_traits<It>::inheritable_iterator_type
      base_iterator;
  typedef typename base_iterator::difference_type difference_type;

public:
  derived_iterator() : _iter() {}
  derived_iterator(const base_iterator &it) : _iter(it) {}

  Der operator+(difference_type d) const {
    return Der(static_cast<const base_iterator &>(*this) + d,
               static_cast<const Der &>(*this));
  }
  Der &operator+=(difference_type d) {
    static_cast<base_iterator &>(*this) += d;
    return static_cast<Der &>(*this);
  }

  Der operator-(difference_type d) const {
    return Der(static_cast<const base_iterator &>(*this) - d,
               static_cast<const Der &>(*this));
  }
  Der &operator-=(difference_type d) {
    static_cast<base_iterator &>(*this) -= d;
    return static_cast<Der &>(*this);
  }

  difference_type operator-(const Der &other) const {
    return static_cast<const base_iterator &>(*this) -
           static_cast<const base_iterator &>(other);
  }

private:
  typedef derived_iterator<It, Der, std::bidirectional_iterator_tag> _iter;
};

//****************************************************************************
//    Cast Iterator  definition
//****************************************************************************

/*!
  A cast iterator is a utility iterator wrapper that can be used to access
  an iterator's data through a supplied functor intermediary, proving to be
  especially useful when converting data from a container to another with
  minimal effort.
*/

template <typename It, typename Func,
          typename Val = typename traits<
              typename function_traits<Func>::ret_type>::referenced_type,
          typename Ref = typename choose_if_match<
              typename function_traits<Func>::ret_type &,
              typename traits<Val>::reference_type>::type,
          typename Ptr = typename choose_if_match<
              Ref, void, typename traits<Val>::pointer_type>::type>
class cast_iterator
    : public derived_iterator<It, cast_iterator<It, Func, Val, Ref, Ptr>> {
  typedef derived_iterator<It, cast_iterator> iterator;
  typedef typename iterator::base_iterator base_iterator;
  typedef Func function;
  typedef typename function_traits<Func>::ret_type ret_type;

public:
  typedef Ref reference;
  typedef Ptr pointer;
  typedef Val value_type;

public:
  cast_iterator() : iterator(), m_func() {}
  cast_iterator(const Func &func) : iterator(), m_func(func) {}

  cast_iterator(const base_iterator &it) : iterator(it), m_func() {}
  cast_iterator(const base_iterator &it, const Func &func)
      : iterator(it), m_func(func) {}

  cast_iterator(const base_iterator &it, const cast_iterator &other)
      : iterator(it), m_func(other.m_func) {}

  ret_type operator*() { return m_func(iterator::operator*()); }
  pointer operator->() { return ptr(0); }

private:
  Func m_func;

private:
  template <typename T>
  pointer ptr(T,
              typename tcg::enable_if<tcg::type_mismatch<pointer, void>::value,
                                      T>::type = 0) const {
    return &operator*();
  }

  void ptr(char) const {}
};

//==========================================================================

//  Utility maker function

template <typename It, typename Func>
inline cast_iterator<It, Func> make_cast_it(const It &it, Func func) {
  return cast_iterator<It, Func>(it, func);
}

//***********************************************************************
//    Step Iterator class
//***********************************************************************

/*!
  The Step Iterator class is a simple random access iterator wrapper which
  moves by a fixed number of items.

\warning The size of the container referenced by the wrapped iterator should
  always be a multiple of the specified step.
*/

template <typename RanIt>
class step_iterator : public std::iterator<
                          std::random_access_iterator_tag,
                          typename std::iterator_traits<RanIt>::value_type,
                          typename std::iterator_traits<RanIt>::difference_type,
                          typename std::iterator_traits<RanIt>::pointer,
                          typename std::iterator_traits<RanIt>::reference> {
  RanIt m_it;
  typename step_iterator::difference_type m_step;

public:
  step_iterator() {}
  step_iterator(const RanIt &it, typename step_iterator::difference_type step)
      : m_it(it), m_step(step) {}

  step_iterator &operator++() {
    m_it += m_step;
    return *this;
  }

  step_iterator &operator--() {
    m_it -= m_step;
    return *this;
  }

  step_iterator operator++(int) {
    step_iterator it(*this);
    operator++();
    return it;
  }

  step_iterator operator--(int) {
    step_iterator it(*this);
    operator--();
    return it;
  }

  step_iterator &operator+=(
      const typename step_iterator::difference_type &val) {
    m_it += val * m_step;
    return *this;
  }

  step_iterator &operator-=(
      const typename step_iterator::difference_type &val) {
    m_it -= val * m_step;
    return *this;
  }

  typename step_iterator::difference_type operator-(
      const step_iterator &it) const {
    return (m_it - it.m_it) / m_step;
  }

  step_iterator operator+(
      const typename step_iterator::difference_type &val) const {
    step_iterator it(*this);
    it += val;
    return it;
  }

  step_iterator operator-(
      const typename step_iterator::difference_type &val) const {
    step_iterator it(*this);
    it -= val;
    return it;
  }

  typename step_iterator::reference operator*() const { return *m_it; }
  typename step_iterator::pointer operator->() const {
    return m_it.operator->();
  }

  const RanIt &it() const { return m_it; }
  int step() const { return m_step; }

  bool operator==(const step_iterator &it) const { return m_it == it.m_it; }
  bool operator!=(const step_iterator &it) const { return !operator==(it); }

  bool operator<(const step_iterator &it) const { return m_it < it.m_it; }
  bool operator>(const step_iterator &it) const { return m_it > it.m_it; }
  bool operator<=(const step_iterator &it) const { return m_it <= it.m_it; }
  bool operator>=(const step_iterator &it) const { return m_it >= it.m_it; }
};

}  // namespace tcg

#endif  // TCG_ITERATOR_OPS_H
