#pragma once

#ifndef TCG_CYCLIC_H
#define TCG_CYCLIC_H

// STD includes
#include <iterator>

// tcg includes
#include "numeric_ops.h"

namespace tcg {

//************************************************************************
//    Cyclic helpers
//************************************************************************

namespace cyclic_ops {

template <typename Scalar>
inline bool ll(Scalar a, Scalar b, Scalar c) {
  return a <= c ? a < b && b < c : c > b || b > a;
}

template <typename Scalar>
inline bool lel(Scalar a, Scalar b, Scalar c) {
  return a <= c ? a <= b && b < c : c > b || b >= a;
}

template <typename Scalar>
inline bool lle(Scalar a, Scalar b, Scalar c) {
  return a <= c ? a < b && b <= c : c >= b || b > a;
}

template <typename Scalar>
inline bool lele(Scalar a, Scalar b, Scalar c) {
  return a <= c ? a <= b && b <= c : c >= b || b >= a;
}

//-------------------------------------------------------------------------------------------

#ifdef min
#undef min
#endif

template <typename Scalar>
Scalar min(Scalar a, Scalar b, Scalar ref) {
  return lel(ref, a, b) ? a : b;
}

#ifdef max
#undef max
#endif

template <typename Scalar>
Scalar max(Scalar a, Scalar b, Scalar ref) {
  return lel(ref, a, b) ? a : b;
}

//-------------------------------------------------------------------------------------------

template <typename value_type, typename diff_type>
value_type increased(const value_type &val, const value_type &start,
                     diff_type length) {
  return start + numeric_ops::mod(val - start + 1, length);
}

//-------------------------------------------------------------------------------------------

template <typename value_type, typename diff_type>
value_type increased(const value_type &val, const value_type &start,
                     diff_type length, diff_type add) {
  return start + numeric_ops::mod(val - start + add, length);
}

//-------------------------------------------------------------------------------------------

template <typename value_type, typename diff_type>
value_type decreased(const value_type &val, const value_type &start,
                     diff_type length) {
  return start + numeric_ops::mod(val - start - 1, length);
}

//-------------------------------------------------------------------------------------------

template <typename value_type, typename diff_type>
value_type decreased(const value_type &val, const value_type &start,
                     diff_type length, diff_type add) {
  return start + numeric_ops::mod(val - start - add, length);
}

}  // namespace cyclic_ops

//************************************************************************
//    Cyclic Iterators
//************************************************************************

template <typename It,
          typename Cat = typename std::iterator_traits<It>::iterator_category>
class cyclic_iterator;

//------------------------------------------------------------------------------------------

template <typename It>
class cyclic_iterator<It, std::forward_iterator_tag>
    : public std::iterator<std::forward_iterator_tag,
                           typename std::iterator_traits<It>::value_type,
                           typename std::iterator_traits<It>::difference_type,
                           typename std::iterator_traits<It>::pointer,
                           typename std::iterator_traits<It>::reference> {
protected:
  It m_it, m_begin, m_end;
  int m_lap;

public:
  cyclic_iterator() : m_lap(0) {}
  cyclic_iterator(const It &it, const It &begin, const It &end, int lap = 0)
      : m_it(it), m_begin(begin), m_end(end), m_lap(lap) {}

  cyclic_iterator &operator++() {
    if (++m_it == m_end) ++m_lap, m_it = m_begin;
    return *this;
  }

  cyclic_iterator operator++(int) {
    cyclic_iterator it(*this);
    operator++();
    return it;
  }

  typename cyclic_iterator::reference operator*() { return *m_it; }
  typename cyclic_iterator::pointer operator->() { return m_it.operator->(); }

  bool operator==(const cyclic_iterator &it) const {
    return m_it == it.m_it && m_lap == it.m_lap;
  }
  bool operator!=(const cyclic_iterator &it) const { return !operator==(it); }
};

//------------------------------------------------------------------------------------------

template <typename It>
class cyclic_iterator<It, std::bidirectional_iterator_tag>
    : public std::iterator<std::bidirectional_iterator_tag,
                           typename std::iterator_traits<It>::value_type,
                           typename std::iterator_traits<It>::difference_type,
                           typename std::iterator_traits<It>::pointer,
                           typename std::iterator_traits<It>::reference> {
protected:
  It m_it, m_begin, m_end;
  int m_lap;

public:
  cyclic_iterator() : m_lap(0) {}
  cyclic_iterator(const It &it, const It &begin, const It &end, int lap = 0)
      : m_it(it), m_begin(begin), m_end(end), m_lap(lap) {}

  cyclic_iterator &operator++() {
    if (++m_it == m_end) ++m_lap, m_it = m_begin;
    return *this;
  }

  cyclic_iterator &operator--() {
    if (m_it == m_begin) --m_lap, m_it = m_end;
    --m_it;
    return *this;
  }

  cyclic_iterator operator++(int) {
    cyclic_iterator it(*this);
    operator++();
    return it;
  }

  cyclic_iterator operator--(int) {
    cyclic_iterator it(*this);
    operator--();
    return it;
  }

  typename cyclic_iterator::reference operator*() { return *m_it; }
  typename cyclic_iterator::pointer operator->() { return m_it.operator->(); }

  bool operator==(const cyclic_iterator &it) const {
    return m_it == it.m_it && m_lap == it.m_lap;
  }
  bool operator!=(const cyclic_iterator &it) const { return !operator==(it); }
};

//------------------------------------------------------------------------------------------

template <typename It>
class cyclic_iterator<It, std::random_access_iterator_tag>
    : public std::iterator<std::random_access_iterator_tag,
                           typename std::iterator_traits<It>::value_type,
                           typename std::iterator_traits<It>::difference_type,
                           typename std::iterator_traits<It>::pointer,
                           typename std::iterator_traits<It>::reference> {
protected:
  It m_it, m_begin, m_end;
  int m_lap;
  typename cyclic_iterator::difference_type m_count;

public:
  cyclic_iterator() : m_lap(0), m_count(0) {}
  cyclic_iterator(const It &it, const It &begin, const It &end, int lap = 0)
      : m_it(it)
      , m_begin(begin)
      , m_end(end)
      , m_lap(lap)
      , m_count(m_end - m_begin) {}

  cyclic_iterator &operator++() {
    if (++m_it == m_end) ++m_lap, m_it = m_begin;
    return *this;
  }

  cyclic_iterator &operator--() {
    if (m_it == m_begin) --m_lap, m_it = m_end;
    --m_it;
    return *this;
  }

  cyclic_iterator operator++(int) {
    cyclic_iterator it(*this);
    operator++();
    return it;
  }

  cyclic_iterator operator--(int) {
    cyclic_iterator it(*this);
    operator--();
    return it;
  }

  cyclic_iterator &operator+=(
      const typename cyclic_iterator::difference_type &val) {
    m_lap += (val + (m_it - m_begin)) / m_count;
    m_it = cyclic_ops::increased(m_it, m_begin, m_count, val);
    return *this;
  }

  cyclic_iterator &operator-=(
      const typename cyclic_iterator::difference_type &val) {
    m_lap -= (val + (m_end - m_it)) / m_count;
    m_it = cyclic_ops::decreased(m_it, m_begin, m_count, val);
    return *this;
  }

  typename cyclic_iterator::difference_type operator-(
      const cyclic_iterator &it) const {
    assert(m_begin == it.m_begin && m_end == it.m_end);
    return m_it - it.m_it + m_lap * m_count - it.m_lap * it.m_count;
  }

  cyclic_iterator operator+(
      const typename cyclic_iterator::difference_type &val) const {
    cyclic_iterator it(*this);
    it += val;
    return it;
  }

  cyclic_iterator operator-(
      const typename cyclic_iterator::difference_type &val) const {
    cyclic_iterator it(*this);
    it -= val;
    return it;
  }

  typename cyclic_iterator::reference operator*() const { return *m_it; }
  typename cyclic_iterator::pointer operator->() const {
    return m_it.operator->();
  }

  bool operator==(const cyclic_iterator &it) const {
    return m_it == it.m_it && m_lap == it.m_lap;
  }
  bool operator!=(const cyclic_iterator &it) const { return !operator==(it); }

  bool operator<(const cyclic_iterator &it) const {
    return m_lap < it.m_lap || m_it < it.m_it;
  }
  bool operator>(const cyclic_iterator &it) const {
    return m_lap > it.m_lap || m_it > it.m_it;
  }
  bool operator<=(const cyclic_iterator &it) const {
    return m_lap <= it.m_lap || m_it <= it.m_it;
  }
  bool operator>=(const cyclic_iterator &it) const {
    return m_lap >= it.m_lap || m_it >= it.m_it;
  }
};

}  // namespace tcg

#endif  // TCG_CYCLIC_H
