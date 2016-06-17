#pragma once

#ifndef TCG_POOL_H
#define TCG_POOL_H

// STD includes
#include "assert.h"

#include <vector>
#include <algorithm>
#include <functional>

// Discriminate rvalues support - either use Boost's config.hpp for automatic
// lookup,
// or deal with it manually

#ifndef TCG_RVALUES_SUPPORT
#include <boost/config.hpp>

#ifdef BOOST_NO_RVALUE_REFERENCES
#define TCG_RVALUES_SUPPORT 0
#else
#define TCG_RVALUES_SUPPORT 1
#endif
#endif

/*!
  \file     pool.h

  \brief    This file contains an implementation of the \a pool container
  concept.

  \details  A pool is a container that supports two fundamental operations,
  acquire() and
            release(), whose purpose is that of marking access operations to a
  resource
            that <I>should not be destroyed</I> when the access operation ends.

            Creation and destruction of resources can be a time-consuming task
  that could
            be avoided when it is known that resources could be reused - which
  is the very
            reason why caching can be a useful approach to some problems.

            The classical cache concept is nothing more than that of a standard
  associative
            map, tweaked to accomodate specific commodities - such as managing
  the cache
            size.

            However, sometimes resources could be considered to be effectively
  indentical,
            to the point that assigning a key is merely a way to keep the
  objects
            distinct in the cache. Users are in the condition to require only
  two functions,
            one to acquire() a resource, the other to release() it.

            The Pool concept we are introducing satisfies the following ruleset:

            <UL>
            <LI> The pool is a dynamical container which allocates resources by
  invoking the
                 objects' default constructor. </LI>
            <LI> Objects in the pool are \b not destructed, except when the pool
  is explicitly
                 \a squeezed by the user.</LI>
            <LI> Keys are indices, returned by the pool with an incremental
  ordering. </LI>
            <LI> An object which has been acquired cannot be acquired again
  until it has been
                 released </LI>
            </UL>
*/

namespace tcg {

//****************************************************************************
//    TCG Indices Pool  class
//****************************************************************************

template <typename T = size_t, typename Cont = std::vector<T>>
class indices_pool {
public:
  typedef T value_type;
  typedef Cont container_type;

private:
  T m_start;               //!< First index to be acquired
  T m_size;                //!< Current indices count
  Cont m_releasedIndices;  //!< Priority queue of released indices

public:
  indices_pool(value_type start = 0) : m_start(start), m_size(0) {}
  ~indices_pool() {}

  template <typename iter>
  indices_pool(value_type size, iter releasedBegin, iter releasedEnd,
               value_type start = 0)
      : m_start(start)
      , m_size(size)
      , m_releasedIndices(releasedBegin, releasedEnd) {
    std::make_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
                   std::greater<T>());
  }

  template <typename iter>
  indices_pool(iter acquiredBegin, iter acquiredEnd, value_type start = 0)
      : m_start(start) {
    if (acquiredBegin == acquiredEnd)
      m_size = 0;
    else {
      // Sort the acquired indices
      std::vector<T> acquired(acquiredBegin, acquiredEnd);

      std::sort(acquired.begin(), acquired.end());
      assert(acquired.front() >= m_start);

      // Scan them, adding holes in the acquired sequence to released indices
      m_size = acquired.back() - m_start + 1;

      assert(m_size >= (T)acquired.size());
      m_releasedIndices.reserve(m_size - acquired.size());

      T curIdx = m_start;

      typename std::vector<T>::iterator at, aEnd(acquired.end());
      for (at = acquired.begin(); at != aEnd; ++at, ++curIdx)
        for (; curIdx != *at; ++curIdx) m_releasedIndices.push_back(curIdx);

      std::make_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
                     std::greater<T>());
    }
  }

  value_type size() const { return m_size; }
  value_type releasedSize() const { return m_releasedIndices.size(); }
  value_type acquiredSize() const { return size() - releasedSize(); }

  const container_type &releasedIndices() const { return m_releasedIndices; }

  value_type acquire() {
    if (!m_releasedIndices.empty()) {
      value_type idx = m_releasedIndices.front();

      pop_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
               std::greater<T>());
      m_releasedIndices.pop_back();

      return idx;
    }

    return m_start + m_size++;
  }

  void release(value_type idx) {
    m_releasedIndices.push_back(idx);
    push_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
              std::greater<T>());
  }

  void clear() {
    m_size = 0;
    m_releasedIndices.clear();
  }

  //! Removes pool entries whose index is beyond that of the last acquired one.
  void squeeze() {
    if (m_releasedIndices.empty()) return;

    std::sort_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
                   std::greater<T>());  // O(n logn)

    // Decrease the pool size as long as the back items are unused
    T lastIndex = m_start + m_size - 1;

    typename Cont::iterator ut, uEnd(m_releasedIndices.end());
    for (ut = m_releasedIndices.begin(); ut != uEnd && *ut == lastIndex; ++ut)
      --lastIndex, --m_size;

    if (ut != m_releasedIndices.begin()) m_releasedIndices.swap(Cont(ut, uEnd));

    std::make_heap(m_releasedIndices.begin(), m_releasedIndices.end(),
                   std::greater<T>());  // O(n)
  }

#if (TCG_RVALUES_SUPPORT > 0)

  indices_pool(const indices_pool &other)
      : m_start(other.m_start)
      , m_size(other.m_size)
      , m_releasedIndices(other.m_releasedIndices) {}

  indices_pool &operator=(const indices_pool &other) {
    m_start = other.m_start, m_size = other.m_size,
    m_releasedIndices = other.m_releasedIndices;
    return *this;
  }

  indices_pool(indices_pool &&other)
      : m_start(other.m_start)
      , m_size(other.m_size)
      , m_releasedIndices(
            std::forward<container_type &&>(other.m_releasedIndices)) {}

  indices_pool &operator=(indices_pool &&other) {
    m_start = other.m_start, m_size = other.m_size;
    m_releasedIndices =
        std::forward<container_type &&>(other.m_releasedIndices);
    return *this;
  }

#endif
};

//----------------------------------------------------------------

template <typename T, typename Cont = std::vector<T>>
class pool {
public:
  typedef T value_type;
  typedef Cont container_type;
  typedef size_t size_type;

private:
  Cont m_objects;                       //!< Objects in the pool
  tcg::indices_pool<size_t> m_indices;  //!< Indices Pool representing m_objects

public:
  pool() {}
  ~pool() {}

  const tcg::indices_pool<size_t> &indices() const { return m_indices; }

  size_type size() const {
    assert(m_indices.size() <= m_objects.size());
    return m_objects.size();
  }
  size_type releasedSize() const {
    return m_objects.size() - m_indices.acquiredSize();
  }
  size_type acquiredSize() const { return m_indices.acquiredSize(); }

  const value_type &operator[](size_type idx) const { return m_objects[idx]; }
  value_type &operator[](size_type idx) { return m_objects[idx]; }

  size_type acquire() {
    size_t result = m_indices.acquire();
    if (result >= m_objects.size()) {
      assert(result == m_objects.size());
      m_objects.push_back(T());
    }

    return result;
  }

  void release(size_type idx) { m_indices.release(idx); }

  void releaseAll() { m_indices.clear(); }
  void clear() {
    releaseAll();
    m_objects.clear();
  }

  //! Removes pool entries whose index is beyond that of the last acquired one.
  void squeeze() {
    m_indices.squeeze();
    m_objects.resize(m_indices.size());
  }
};

}  // namespace tcg

#endif  // TCG_POOL_H
