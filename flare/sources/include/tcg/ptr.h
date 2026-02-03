#pragma once

#ifndef TCG_PTR_WRAPPER
#define TCG_PTR_WRAPPER

// STD includes
#include <iterator>

namespace tcg {

/*!
  \brief    The ptr_wrapper class implements the basic functions necessary
            to wrap a generic pointer object. The use case for this class
            is to allow pointer inheritance.
*/

template <typename T>
class ptr {
public:
  typedef T *ptr_type;

  typedef typename std::iterator_traits<ptr_type>::iterator_category
      iterator_category;
  typedef typename std::iterator_traits<ptr_type>::value_type value_type;
  typedef
      typename std::iterator_traits<ptr_type>::difference_type difference_type;
  typedef typename std::iterator_traits<ptr_type>::pointer pointer;
  typedef typename std::iterator_traits<ptr_type>::reference reference;

public:
  explicit ptr(ptr_type p = ptr_type()) : m_ptr(p) {}

  operator bool() const { return m_ptr; }  // There should be no need to use
                                           // the Safe Bool idiom
  bool operator==(const ptr &other) const { return (m_ptr == other.m_ptr); }
  bool operator!=(const ptr &other) const { return (m_ptr != other.m_ptr); }

  bool operator<(const ptr &other) const { return (m_ptr < other.m_ptr); }
  bool operator>(const ptr &other) const { return (m_ptr > other.m_ptr); }
  bool operator<=(const ptr &other) const { return (m_ptr <= other.m_ptr); }
  bool operator>=(const ptr &other) const { return (m_ptr >= other.m_ptr); }

  ptr &operator++() {
    ++m_ptr;
    return *this;
  }
  ptr operator++(int) { return ptr(m_ptr++, *this); }

  ptr &operator--() {
    --m_ptr;
    return *this;
  }
  ptr operator--(int) { return ptr(m_ptr--, *this); }

  ptr operator+(difference_type d) const { return ptr(m_ptr + d, *this); }
  ptr &operator+=(difference_type d) {
    m_ptr += d;
    return *this;
  }

  ptr operator-(difference_type d) const { return ptr(m_ptr - d, *this); }
  ptr &operator-=(difference_type d) {
    m_ptr -= d;
    return *this;
  }

  difference_type operator-(const ptr &other) const {
    return m_ptr - other.m_ptr;
  }

  pointer operator->() const { return m_ptr; }
  reference operator*() const { return *m_ptr; }
  reference operator[](difference_type d) const { return m_ptr[d]; }

protected:
  ptr_type m_ptr;
};

//=======================================================

template <typename T>
ptr<T> make_ptr(T *p) {
  return ptr<T>(p);
}

}  // namespace tcg

#endif  // TCG_PTR_WRAPPER
