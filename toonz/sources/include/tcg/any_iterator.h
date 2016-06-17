#pragma once

#ifndef TCG_ANY_ITERATOR_H
#define TCG_ANY_ITERATOR_H

#include "base.h"

/*
  \file     any_iterator.h

  \brief    This file contains the implementation of a type-erased iterator
  template class.

  \details  Type erasure is a C++ idiom about allocating an interface
            to an object who is then hidden as the interface's implementation
  detail.

            The cost of using a type-erased object instead of its actual type is
  typically
            measured in a heap access at construction and destruction, and one
  virtual
            function call per method invocation.

            A type-erased iterator can be useful to hide implementation details
  about container
            choices, yet providing an iterator-like interface to access the
  stored data.
*/

#ifndef TCG_RVALUES_SUPPORT
#include <boost/config.hpp>

#ifdef BOOST_NO_RVALUE_REFERENCES
#define TCG_RVALUES_SUPPORT 0
#else
#define TCG_RVALUES_SUPPORT 1
#endif
#endif

namespace tcg {

//****************************************************************************
//    any_iterator_concept (ie the interface)
//****************************************************************************

template <typename Val, typename ValRef, typename ValPtr, typename Dist>
class any_iterator_concept {
public:
  virtual ~any_iterator_concept() {}

  virtual any_iterator_concept *clone() const = 0;

  virtual ValRef operator*() const  = 0;
  virtual ValPtr operator->() const = 0;

  virtual bool operator==(const any_iterator_concept &other) const = 0;
  virtual bool operator!=(const any_iterator_concept &other) const = 0;

  virtual void operator++() = 0;
  virtual any_iterator_concept *operator++(int) {
    assert(false);
    return 0;
  }

  virtual void operator--() { assert(false); }
  virtual any_iterator_concept *operator--(int) {
    assert(false);
    return 0;
  }

  virtual bool operator<(const any_iterator_concept &) const {
    assert(false);
    return false;
  }
  virtual bool operator>(const any_iterator_concept &) const {
    assert(false);
    return false;
  }
  virtual bool operator<=(const any_iterator_concept &) const {
    assert(false);
    return false;
  }
  virtual bool operator>=(const any_iterator_concept &) const {
    assert(false);
    return false;
  }

  virtual any_iterator_concept *operator+(Dist) const {
    assert(false);
    return 0;
  }
  virtual void operator+=(Dist d) { assert(false); }

  virtual any_iterator_concept *operator-(Dist) const {
    assert(false);
    return 0;
  }
  virtual void operator-=(Dist d) { assert(false); }

  virtual Dist operator-(const any_iterator_concept &) const {
    assert(false);
    return 0;
  }

  virtual ValRef operator[](Dist) const {
    assert(false);
    return *(Val *)0;
  }
};

//****************************************************************************
//    any_iterator_model (ie the concrete interface implementations)
//****************************************************************************

template <typename It, typename iterator_cat, typename Val, typename ValRef,
          typename ValPtr, typename Dist>
class any_iterator_model
    : public any_iterator_concept<Val, ValRef, ValPtr, Dist> {
  typedef any_iterator_concept<Val, ValRef, ValPtr, Dist> any_it_concept;

public:
  any_iterator_model() : m_it() {}
  any_iterator_model(const It &it) : m_it(it) {}

  any_it_concept *clone() const {
    return new any_iterator_model<It, iterator_cat, Val, ValRef, ValPtr, Dist>(
        *this);
  }

  ValRef operator*() const { return m_it.operator*(); }
  ValPtr operator->() const { return m_it.operator->(); }

  bool operator==(const any_it_concept &other) const {
    return m_it == static_cast<const any_iterator_model &>(other).m_it;
  }
  bool operator!=(const any_it_concept &other) const {
    return m_it != static_cast<const any_iterator_model &>(other).m_it;
  }

  void operator++() { ++m_it; }
  any_it_concept *operator++(int) {
    return new any_iterator_model<It, iterator_cat, Val, ValRef, ValPtr, Dist>(
        m_it++);
  }

protected:
  It m_it;
};

template <typename It, typename Val, typename ValRef, typename ValPtr,
          typename Dist>
class any_iterator_model<It, std::bidirectional_iterator_tag, Val, ValRef,
                         ValPtr, Dist>
    : public any_iterator_model<It, std::forward_iterator_tag, Val, ValRef,
                                ValPtr, Dist> {
  typedef any_iterator_concept<Val, ValRef, ValPtr, Dist> any_it_concept;

  using any_iterator_model<It, std::forward_iterator_tag, Val, ValRef, ValPtr,
                           Dist>::m_it;

public:
  any_iterator_model() {}
  any_iterator_model(const It &it)
      : any_iterator_model<It, std::forward_iterator_tag, Val, ValRef, ValPtr,
                           Dist>(it) {}

  any_it_concept *clone() const {
    return new any_iterator_model<It, std::bidirectional_iterator_tag, Val,
                                  ValRef, ValPtr, Dist>(*this);
  }

  void operator--() { --m_it; }
  any_it_concept *operator--(int) {
    return new any_iterator_model<It, std::bidirectional_iterator_tag, Val,
                                  ValRef, ValPtr, Dist>(m_it--);
  }
};

template <typename It, typename Val, typename ValRef, typename ValPtr,
          typename Dist>
class any_iterator_model<It, std::random_access_iterator_tag, Val, ValRef,
                         ValPtr, Dist>
    : public any_iterator_model<It, std::bidirectional_iterator_tag, Val,
                                ValRef, ValPtr, Dist> {
  typedef any_iterator_concept<Val, ValRef, ValPtr, Dist> any_it_concept;

  using any_iterator_model<It, std::forward_iterator_tag, Val, ValRef, ValPtr,
                           Dist>::m_it;

public:
  any_iterator_model() {}
  any_iterator_model(const It &it)
      : any_iterator_model<It, std::bidirectional_iterator_tag, Val, ValRef,
                           ValPtr, Dist>(it) {}

  any_it_concept *clone() const {
    return new any_iterator_model<It, std::random_access_iterator_tag, Val,
                                  ValRef, ValPtr, Dist>(*this);
  }

  bool operator<(const any_it_concept &other) const {
    return m_it < static_cast<const any_iterator_model &>(other).m_it;
  }
  bool operator>(const any_it_concept &other) const {
    return m_it > static_cast<const any_iterator_model &>(other).m_it;
  }
  bool operator<=(const any_it_concept &other) const {
    return m_it <= static_cast<const any_iterator_model &>(other).m_it;
  }
  bool operator>=(const any_it_concept &other) const {
    return m_it >= static_cast<const any_iterator_model &>(other).m_it;
  }

  any_it_concept *operator+(Dist d) const {
    return new any_iterator_model<It, std::random_access_iterator_tag, Val,
                                  ValRef, ValPtr, Dist>(m_it + d);
  }
  void operator+=(Dist d) { m_it += d; }

  any_it_concept *operator-(Dist d) const {
    return new any_iterator_model<It, std::random_access_iterator_tag, Val,
                                  ValRef, ValPtr, Dist>(m_it - d);
  }
  void operator-=(Dist d) { m_it -= d; }

  Dist operator-(const any_it_concept &other) const {
    return m_it - static_cast<const any_iterator_model &>(other).m_it;
  }

  ValRef operator[](Dist d) const { return m_it[d]; }
};

//****************************************************************************
//    any_iterator  (ie the wrapper to the interface)
//****************************************************************************

template <typename Val, typename iterator_cat = tcg::empty_type,
          typename ValRef = Val &, typename ValPtr = Val *,
          typename Dist = std::ptrdiff_t>
class any_iterator
    : public std::iterator<iterator_cat, Val, Dist, ValPtr, ValRef> {
  any_iterator_concept<Val, ValRef, ValPtr, Dist> *m_model;

public:
  any_iterator() : m_model(0) {}
  any_iterator(any_iterator_concept<Val, ValRef, ValPtr, Dist> *model)
      : m_model(model) {}

  template <typename It>
  any_iterator(const It &it)
      : m_model(
            new any_iterator_model<It, iterator_cat, Val, ValRef, ValPtr, Dist>(
                it)) {}

  any_iterator(const any_iterator &other) : m_model(other.m_model->clone()) {}
  any_iterator &operator=(any_iterator other) {
    swap(*this, other);
    return *this;
  }

  ~any_iterator() { delete m_model; }

  friend void swap(any_iterator &a, any_iterator &b) {
    std::swap(a.m_model, b.m_model);
  }

  ValRef operator*() const { return m_model->operator*(); }
  ValPtr operator->() const { return m_model->operator->(); }

  bool operator==(const any_iterator &other) const {
    return m_model->operator==(*other.m_model);
  }
  bool operator!=(const any_iterator &other) const {
    return m_model->operator!=(*other.m_model);
  }

  any_iterator &operator++() {
    ++*m_model;
    return *this;
  }
  any_iterator operator++(int) { return any_iterator((*m_model)++); }

  any_iterator &operator--() {
    --*m_model;
    return *this;
  }
  any_iterator operator--(int) { return any_iterator((*m_model)--); }

  bool operator<(const any_iterator &other) const {
    return m_model->operator<(*other.m_model);
  }
  bool operator>(const any_iterator &other) const {
    return m_model->operator>(*other.m_model);
  }
  bool operator<=(const any_iterator &other) const {
    return m_model->operator<=(*other.m_model);
  }
  bool operator>=(const any_iterator &other) const {
    return m_model->operator>=(*other.m_model);
  }

  any_iterator operator+(Dist d) const { return any_iterator((*m_model) + d); }
  any_iterator &operator+=(Dist d) {
    (*m_model) += d;
    return *this;
  }

  any_iterator operator-(Dist d) const { return any_iterator((*m_model) - d); }
  any_iterator &operator-=(Dist d) {
    (*m_model) -= d;
    return *this;
  }

  Dist operator-(const any_iterator &other) const {
    return m_model->operator-(*other.m_model);
  }

  ValRef operator[](Dist d) const { return m_model->operator[](d); }

#if (TCG_RVALUES_SUPPORT > 0)

  any_iterator(any_iterator &&other) : m_model(other.m_model) {
    other.m_model = 0;
  }

#endif
};

//-------------------------------------------------------------------------------

template <typename Val, typename iterator_cat, typename ValRef, typename ValPtr,
          typename Dist>
any_iterator<Val, iterator_cat, ValRef, ValPtr, Dist> operator+(
    Dist d, const any_iterator<Val, iterator_cat, ValRef, ValPtr, Dist> &it) {
  return it + d;
}

//****************************************************************************
//    Additional typedefs to the actual types
//****************************************************************************

template <typename Val>
struct any_iterator<Val, tcg::empty_type> {
  typedef any_iterator<Val, std::input_iterator_tag> input;
  typedef any_iterator<Val, std::output_iterator_tag> output;
  typedef any_iterator<Val, std::forward_iterator_tag> forward;
  typedef any_iterator<Val, std::bidirectional_iterator_tag> bidirectional;
  typedef any_iterator<Val, std::random_access_iterator_tag> random;
};

template <typename Val, typename ValRef = Val &, typename ValPtr = Val *,
          typename Dist = std::ptrdiff_t>
struct any_it {
  typedef any_iterator<Val, std::input_iterator_tag, ValRef, ValPtr, Dist>
      input;
  typedef any_iterator<Val, std::output_iterator_tag, ValRef, ValPtr, Dist>
      output;
  typedef any_iterator<Val, std::forward_iterator_tag, ValRef, ValPtr, Dist>
      forward;
  typedef any_iterator<Val, std::bidirectional_iterator_tag, ValRef, ValPtr,
                       Dist>
      bidirectional;
  typedef any_iterator<Val, std::random_access_iterator_tag, ValRef, ValPtr,
                       Dist>
      random;
};

}  // namespace tcg

#endif  // TCG_ANY_ITERATOR_H
