#pragma once

#ifndef TCG_LIST_H
#define TCG_LIST_H

// tcg includes
#include "alignment.h"
#include "macros.h"

// STD includes
#include <cstddef>  // size_t
#include <new>      // placement new
#include <vector>

#include "assert.h"

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

#if (TCG_RVALUES_SUPPORT > 0)

// Perfect forwarding macros
#define TCG_FORWARD_TYPE(TheType) TheType##__ &&
#define TCG_FORWARD_TEMPLATE(TheType) template <typename TheType##__>
#define TCG_FORWARD(TheType, Val) std::forward<TheType##__>(Val)

#else

// Common const reference forwarding macros
#define TCG_FORWARD_TYPE(TheType) const TheType &
#define TCG_FORWARD_TEMPLATE(TheType)
#define TCG_FORWARD(TheType, Val) Val

#endif

/*! \file     tcg_list.h

    \brief    This file contains the implementation of an <I>indexed list</I>,
              a list class based on a random access sequential container.
*/

//************************************************************************************
//    Preliminaries
//************************************************************************************

namespace tcg {

static const size_t _neg = -1, _invalid = -2;

}  // namespace tcg

//************************************************************************************
//    Indexed list node  definition
//************************************************************************************

namespace tcg {

/*!
  \brief    Internal node class used in the implementation of a tcg::list_base.
*/

template <typename T>
struct _list_node {
  TCG_STATIC_ASSERT(sizeof(T) == sizeof(aligned_buffer<T>));

  aligned_buffer<T> m_val;
  size_t m_prev, m_next;

  TCG_DEBUG(T &m_t;)  // Provided for debuggers' convenience

public:
  _list_node() : m_prev(_neg), m_next(_invalid) TCG_DEBUG2(, m_t(value())) {}
  _list_node(const T &val)
      : m_prev(_neg), m_next(_neg) TCG_DEBUG2(, m_t(value())) {
    constructValue(val);
  }
  ~_list_node() {
    if (isValid()) destroyValue();
  }

  _list_node(const _list_node &other)
      : m_prev(other.m_prev), m_next(other.m_next) TCG_DEBUG2(, m_t(value())) {
    if (other.isValid()) constructValue(other.value());
  }

  _list_node &operator=(const _list_node &other) {
    if (isValid())
      if (other.isValid())
        value() = other.value();
      else
        destroyValue();
    else
      constructValue(other.value());

    m_prev = other.m_prev, m_next = other.m_next;
    return *this;
  }

  const T &value() const { return reinterpret_cast<const T &>(m_val.m_buf); }
  T &value() { return reinterpret_cast<T &>(m_val.m_buf); }

  bool isValid() const { return (m_next != _invalid); }
  void invalidate() {
    assert(isValid());
    destroyValue();
    m_next = _invalid;
  }

#if (TCG_RVALUES_SUPPORT > 0)

  _list_node(T &&val) : m_prev(_neg), m_next(_neg) TCG_DEBUG2(, m_t(value())) {
    constructValue(std::move(val));
  }

  _list_node(_list_node &&other)
      : m_prev(other.m_prev), m_next(other.m_next) TCG_DEBUG2(, m_t(value())) {
    if (other.isValid())
      constructValue(std::move(other.value())), other.invalidate();
  }

  _list_node &operator=(_list_node &&other) {
    if (isValid())
      if (other.isValid())
        value() = other.value();
      else
        destroyValue();
    else
      constructValue(std::move(other.value()));

    value() = std::move(other.value());
    m_prev = other.m_prev, m_next = other.m_next;
    return *this;
  }

#endif

public:
  void constructValue(const T &val) { new (m_val.m_buf) T(val); }
  void destroyValue() { value().~T(); }

#if (TCG_RVALUES_SUPPORT > 0)
  void constructValue(T &&val) { new (m_val.m_buf) T(std::move(val)); }
#endif
};

//************************************************************************************
//    Base indexed list  definition
//************************************************************************************

/*!
  \brief    Base class for list-like containers based on <TT>tcg::list</TT>'s
            indexed list implementation.

  \details  This class implements a barebone container class which manages
            a collection of nodes stored in an iternal random access container.

            Nodes management is limited to the \a acquisition and \a release
            of individual nodes (see the buyNode() and sellNode() functions).

            Links between list nodes must be managed externally.

  \sa       Template class \p tcg::list for an explanation of the indexed list
            container concept.
*/

template <typename T>
class list_base {
public:
  typedef T value_type;
  typedef ::size_t size_t;

  static const size_t _neg = -1;

protected:
  typedef _list_node<T> list_node;

  std::vector<list_node> m_vector;

  size_t m_size;
  size_t m_clearedHead;

public:
  struct const_iterator
      : public std::iterator<std::bidirectional_iterator_tag, const T> {
    const list_base *m_list;
    size_t m_idx;

    TCG_DEBUG(const list_node *m_node;)  // Provided for debuggers' convenience

  public:
    typedef const T value_type;
    typedef const T *pointer;
    typedef const T &reference;

  public:
    const_iterator() {}
    const_iterator(const list_base *list, size_t idx)
        : m_list(list), m_idx(idx) {
      TCG_DEBUG(m_node = (m_idx == _neg) ? 0 : &m_list->m_vector[m_idx]);
    }

    size_t index() const { return m_idx; }

    reference operator*() const { return (*m_list)[m_idx]; }
    pointer operator->() const { return &(*m_list)[m_idx]; }

    const_iterator operator++(int) {
      const_iterator temp(*this);
      operator++();
      return temp;
    }
    const_iterator &operator++() {
      m_idx = m_list->m_vector[m_idx].m_next;
      TCG_DEBUG(m_node = (m_idx == _neg) ? 0 : &m_list->m_vector[m_idx]);
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator temp(*this);
      operator--();
      return temp;
    }
    const_iterator &operator--() {
      m_idx = m_list->m_vector[m_idx].m_prev;
      TCG_DEBUG(m_node = (m_idx == _neg) ? 0 : &m_list->m_vector[m_idx]);
      return *this;
    }

    bool operator==(const const_iterator &it) const {
      return (m_idx == it.m_idx);
    }
    bool operator!=(const const_iterator &it) const {
      return (m_idx != it.m_idx);
    }
  };

  struct iterator : public const_iterator {
  public:
    typedef T value_type;
    typedef T *pointer;
    typedef T &reference;

  public:
    iterator() {}
    iterator(list_base *list, size_t idx) : const_iterator(list, idx) {}

    reference operator*() const {
      return const_cast<reference>(const_iterator::operator*());
    }
    pointer operator->() const {
      return const_cast<pointer>(const_iterator::operator->());
    }

    iterator operator++(int) {
      iterator temp(*this);
      operator++();
      return temp;
    }
    iterator &operator++() {
      const_iterator::operator++();
      return *this;
    }

    iterator operator--(int) {
      iterator temp(*this);
      operator--();
      return temp;
    }
    iterator &operator--() {
      const_iterator::operator--();
      return *this;
    }
  };

  friend struct iterator;
  friend struct const_iterator;

public:
  list_base() : m_size(0), m_clearedHead(_neg) {}

  list_base(size_t count, const T &val)
      : m_vector(count, list_node(val)), m_size(count), m_clearedHead(_neg) {
    assert(m_size >= 0);
    for (size_t i        = 0; i < m_size; ++i)
      m_vector[i].m_prev = i - 1, m_vector[i].m_next = i + 1;
    if (m_size > 0) m_vector[m_size - 1].m_next = _neg;
  }

  template <typename InIt>
  list_base(InIt begin, InIt end)
      : m_vector(begin, end), m_size(m_vector.size()), m_clearedHead(_neg) {
    assert(m_size >= 0);
    for (size_t i        = 0; i < m_size; ++i)
      m_vector[i].m_prev = i - 1, m_vector[i].m_next = i + 1;
    if (m_size > 0) m_vector[m_size - 1].m_next = _neg;
  }

  //-----------------------------------------------------------------------------------------

  list_base(const list_base &other)
      : m_vector(other.m_vector)
      , m_size(other.m_size)
      , m_clearedHead(other.m_clearedHead) {}

  list_base &operator=(const list_base &other) {
    // Note: not using the copy-and-swap idiom. We want to attempt preservation
    // of m_vector's buffer in case its current capacity is sufficient.

    m_size        = other.m_size;
    m_clearedHead = other.m_clearedHead;

    // A clear must be performed before the actual =. It's necessary to prevent
    // copying to
    // objects that were already destroyed - but are still in the vector as
    // zombies.
    // In this case, the value-to-value vector::operator= would assume that the
    // assigned-to object is NOT a zombie - and would attempt its release, which
    // is UB!
    m_vector.clear();

    m_vector = other.m_vector;
    return *this;
  }

#if (TCG_RVALUES_SUPPORT > 0)

  list_base(list_base &&other)
      : m_vector(std::move(other.m_vector))
      , m_size(other.m_size)
      , m_clearedHead(other.m_clearedHead) {}

  list_base &operator=(list_base &&other) {
    swap(*this, std::forward<list_base>(other));
    return *this;
  }

#endif

  T &operator[](size_t idx) {
    assert(idx < m_vector.size() && isValid(idx));
    return m_vector[idx].value();
  }
  const T &operator[](size_t idx) const {
    assert(idx < m_vector.size() && isValid(idx));
    return m_vector[idx].value();
  }

  bool isValid(size_t idx) const { return m_vector[idx].isValid(); }

  size_t size() const { return m_size; }
  size_t nodesCount() const { return m_vector.size(); }

  void reserve(size_t size) { m_vector.reserve(size); }
  size_t capacity() const { return m_vector.capacity(); }

  void clear() {
    m_size        = 0;
    m_clearedHead = _neg;
    m_vector.clear();
  }

  bool empty() const { return m_size == 0; }

  void _swap(list_base &a, list_base &b) {
    std::swap(a.m_size, b.m_size);
    std::swap(a.m_clearedHead, b.m_clearedHead);
    a.m_vector.swap(b.m_vector);
  }

#if (TCG_RVALUES_SUPPORT > 0)

  void _swap(list_base &a, list_base &&b) {
    a.m_size        = b.m_size;
    a.m_clearedHead = b.m_clearedHead;
    a.m_vector.swap(b.m_vector);
  }

  void _swap(list_base &&a, list_base &b) { swap(b, std::move(a)); }

#endif

  //-----------------------------------------------------------------------------------------

  //! Constructs from a range of (index, value) pairs
  template <typename ForIt>
  list_base(ForIt begin, ForIt end, size_t nodesCount) {
    // Initialize the vector with invalid nodes up to the required nodes count
    m_vector.resize(nodesCount, list_node());

    // Iterate the range and add corresponding value pairs
    size_t lastIdx = _neg;

    for (; begin != end; ++begin) {
      size_t idx = (*begin).first;

      assert(idx >= 0);

      // Further resize the vector with holes if necessary
      if (idx >= m_vector.size()) {
        // The nodesCount hint was not sufficient. Thus, we have to readjust the
        // correct minimal
        // allocation size. Let's do it now with a further single pass toward
        // the end.
        size_t newSize = m_vector.size();

        for (ForIt it = begin; it != end; ++it)
          newSize = std::max(newSize, size_t((*it).first) + 1);

        m_vector.resize(newSize, list_node());
      }

      // Place current node in the vector
      list_node &node = m_vector[idx];
      new (&node) list_node((*begin).second);

      // Link the node
      node.m_prev                                   = lastIdx;
      if (lastIdx != _neg) m_vector[lastIdx].m_next = idx;

      lastIdx = idx;
    }

    // Finally, make up a consistent links chain for holes
    lastIdx = _neg;

    size_t n, nCount = m_vector.size();
    for (n = 0; n != nCount; ++n) {
      list_node &node = m_vector[n];

      if (!node.isValid()) node.m_prev = lastIdx, lastIdx = n;
    }

    m_clearedHead = lastIdx;
  }

  //-----------------------------------------------------------------------------------------

protected:
  TCG_FORWARD_TEMPLATE(T)
  size_t buyNode(TCG_FORWARD_TYPE(T) val) {
    size_t nodeIdx;
    list_node *node;

    ++m_size;

    if (m_clearedHead != _neg) {
      assert(m_clearedHead < m_vector.size());

      // Take the last cleared element
      nodeIdx = m_clearedHead;
      node    = &m_vector[nodeIdx];

      m_clearedHead = node->m_prev;
    } else {
      m_vector.push_back(list_node());

      nodeIdx = m_vector.size() - 1;
      node    = &m_vector[nodeIdx];
    }

    // Construct the node's value
    node->constructValue(TCG_FORWARD(T, val));

    return nodeIdx;
  }

  //-----------------------------------------------------------------------------------------

  void sellNode(size_t idx) {
    assert(isValid(idx));

    list_node &node = m_vector[idx];

    // Relink neighbours
    if (node.m_prev != _neg) m_vector[node.m_prev].m_next = node.m_next;
    if (node.m_next != _neg) m_vector[node.m_next].m_prev = node.m_prev;

    // Explicitly invoke the node's destructor
    node.invalidate();

    // Now, the node is a zombie - still, use its indices data to remember the
    // list
    // of unused nodes

    // Remember the last cleared element in element.m_prev
    node.m_prev   = m_clearedHead;
    m_clearedHead = idx;

    --m_size;
  }

  //-----------------------------------------------------------------------------------------

  void sellNodes(size_t begin, size_t end) {
    for (size_t i = begin; i != end; i = m_vector[i].m_next) sellNode(i);
  }

  //-----------------------------------------------------------------------------------------

  void move(size_t afterDst, size_t &dstBegin, size_t &dstRBegin,
            size_t srcBegin, size_t srcEnd, size_t srcRBegin) {
    if (srcBegin == _neg || srcBegin == afterDst) return;

    // afterDst-1 -> srcBegin
    {
      size_t beforeDst =
          (afterDst == _neg) ? dstRBegin : m_vector[afterDst].m_prev;

      m_vector[srcBegin].m_prev = beforeDst;

      if (beforeDst != _neg)
        m_vector[beforeDst].m_next = srcBegin;
      else
        dstBegin = srcBegin;
    }

    // srcEnd-1 -> afterDst
    {
      size_t srcLast = (srcEnd == _neg) ? srcRBegin : m_vector[srcEnd].m_prev;

      m_vector[srcLast].m_next = afterDst;  // NOTE: srcLast cannot be _neg

      if (afterDst != _neg)
        m_vector[afterDst].m_prev = srcLast;
      else
        dstRBegin = srcLast;
    }
  }
};

//************************************************************************************
//    Indexed list  definition
//************************************************************************************

/*!
  \brief    Double linked list container class with indexed access to elements.

  \details  This class is the result of implementing a double linked list using
a
            dynamic random access sequential container as the underlying pool
for
            the list's nodes.

            This is somewhat different than supplying a custom allocator to an
existing
            list implementation, like \p std::list, since:

              <UL>
              <LI>  No memory model of the underlying container is actually
                    required.</LI>
              <LI>  The underlying container provides explicit access to nodes
                    using their associated <I>insertion indexes</I>, so \p
std::list
                    could not be used as-is.</LI>
              </UL>
\n
            The nodes pool (see \p list_base) is an internal \a lazy container
that
            saves released nodes \a without actually deallocating them (of
course,
            their content is properly destroyed). They are rather stored into an
            internal \a stack of released nodes, ready to be reacquired.
              \par Features
              \li  Insertion indexes are automatically generated keys with the
same
                   access time of the undelying container (typically super-fast
                   \p O(1)).
              \li  Stable indexes and iterators with respect to <I>every
operation</I>.
              \li  Copying an indexed list \a preserves the original indexes.
              \li  Indexes are <I>process independent</I> - they can be saved
                   on file and restored later (although you probably want to
                   \a squeeze the list before saving).

              \par Use cases
            This class is surprisingly useful for <I>cross-referencing</I>.
            A graph data structure, for example, can be implemented using a pair
of
            indexed lists for vertexes and edges.

              \par Known issues
            The lazy nature of this container class may require \a squeezing the
list's
            content at strategic places in your code, as noted before.
            Beware that composite cross-referencing may be hard to track \a
precisely
            during this particular operation.

  \remark   Current implementation explicitly uses an \p std::vector class as
the
            underlying random access container. Future improvements will make
the
            container class a template parameter.

  \sa       Template class \p tcg::Mesh for a notable use case.
*/

template <typename T>
class list : public list_base<T> {
  size_t m_begin, m_rbegin;

protected:
  typedef typename list_base<T>::list_node list_node;
  using list_base<T>::m_vector;

public:
  struct iterator : public list_base<T>::iterator {
    iterator() {}
    iterator(list *l, size_t idx) : list_base<T>::iterator(l, idx) {}

    iterator &operator++() {
      list_base<T>::iterator::operator++();
      return *this;
    }
    iterator &operator--() {
      if (this->m_idx == _neg)
        operator=(static_cast<list *>(this->m_list)->last());
      else
        list_base<T>::iterator::operator--();

      return *this;
    }

    iterator operator++(int) {
      iterator temp(*this);
      operator++();
      return temp;
    }
    iterator operator--(int) {
      iterator temp(*this);
      operator--();
      return temp;
    }
  };

  struct const_iterator : public list_base<T>::const_iterator {
    const_iterator() {}
    const_iterator(const list *l, size_t idx)
        : list_base<T>::const_iterator(l, idx) {}

    const_iterator &operator++() {
      list_base<T>::const_iterator::operator++();
      return *this;
    }
    const_iterator &operator--() {
      if (this->m_idx == _neg)
        operator=(static_cast<const list *>(this->m_list)->last());
      else
        list_base<T>::const_iterator::operator--();

      return *this;
    }

    const_iterator operator++(int) {
      const_iterator temp(*this);
      operator++();
      return temp;
    }
    const_iterator operator--(int) {
      const_iterator temp(*this);
      operator--();
      return temp;
    }
  };

public:
  list() : m_begin(_neg), m_rbegin(_neg) {}
  list(size_t count, const T &val)
      : list_base<T>(count, val), m_begin(0), m_rbegin(count - 1) {}

  template <typename InIt>
  list(InIt begin, InIt end)
      : list_base<T>(begin, end)
      , m_begin(list_base<T>::m_size ? 0 : _neg)
      , m_rbegin(list_base<T>::m_size - 1) {}

  //! Constructs from a range of (index, value) pairs
  template <typename BidIt>
  list(BidIt begin, BidIt end, size_t nodesCount)
      : list_base<T>(begin, end, nodesCount) {
    if (begin != end) m_begin = (*begin).first, m_rbegin = (*--end).first;
  }

  //-----------------------------------------------------------------------------------------

  list(const list &other)
      : list_base<T>(other), m_begin(other.m_begin), m_rbegin(other.m_rbegin) {}

  /*!
\warning  \p tcg::list has a different copy behavior from other container
classes, since
        it is intended to preserve the source's node indices in the process.

        This means that no value-to-value assignment can take place, and thus
<TT>operator=</TT>'s
        behavior is equivalent to first clearing it, and then copy-constructing
each value.

        This typically leads to performance penalties with respect to other
container classes that
        implement value-to-value assignments, like \p std::vector in case its
capacity is not exceeded.
*/
  list &operator=(const list &other) {
    m_begin = other.m_begin, m_rbegin = other.m_rbegin;
    list_base<T>::operator=(other);
    return *this;
  }

#if (TCG_RVALUES_SUPPORT > 0)

  list(list &&other)
      : list_base<T>(std::forward<list_base<T>>(other))
      , m_begin(other.m_begin)
      , m_rbegin(other.m_rbegin) {}

  list &operator=(list &&other) {
    swap(*this, std::forward<list>(other));
    return *this;
  }

#endif

  //-----------------------------------------------------------------------------------------

  TCG_FORWARD_TEMPLATE(T)
  size_t insert(size_t idx, TCG_FORWARD_TYPE(T) val) {
    assert(idx == _neg || list_base<T>::isValid(idx));

    size_t nodeIdx  = list_base<T>::buyNode(TCG_FORWARD(T, val));
    list_node &node = m_vector[nodeIdx];

    // Update links
    if (idx != _neg) {
      // node has a valid next
      list_node &next = m_vector[idx];

      node.m_prev                                           = next.m_prev;
      next.m_prev                                           = nodeIdx;
      node.m_next                                           = idx;
      if (node.m_prev != _neg) m_vector[node.m_prev].m_next = nodeIdx;
    } else {
      node.m_next                                     = _neg;
      node.m_prev                                     = m_rbegin;
      if (m_rbegin != _neg) m_vector[m_rbegin].m_next = nodeIdx;
      m_rbegin                                        = nodeIdx;
    }

    if (idx == m_begin) m_begin = nodeIdx;

    return nodeIdx;
  }

  template <typename Iter>
  size_t insert(size_t idx, Iter begin, Iter end) {
    if (begin == end) return idx;

    assert(idx == _neg || list_base<T>::isValid(idx));

    size_t res = insert(idx, *begin);

    for (++begin; begin != end; ++begin) insert(idx, *begin);

    return res;
  }

  size_t erase(size_t idx) {
    assert(list_base<T>::isValid(idx));

    if (idx == m_begin) m_begin   = m_vector[idx].m_next;
    if (idx == m_rbegin) m_rbegin = m_vector[idx].m_prev;

    size_t result = m_vector[idx].m_next;
    list_base<T>::sellNode(idx);

    return result;
  }

  size_t erase(size_t begin, size_t end) {
    assert(list_base<T>::isValid(begin));

    if (begin == m_begin) m_begin = end;
    if (end == m_rbegin) m_rbegin = m_vector[begin].m_prev;

    list_base<T>::sellNodes(begin, end);

    return end;
  }

  void clear() {
    list_base<T>::clear();
    m_begin = m_rbegin = _neg;
  }

  void swap(list &other) {
    _swap(*this, other);
    std::swap(m_begin, other.m_begin);
    std::swap(m_rbegin, other.m_rbegin);
  }

  void swap(list &a, list &b) { a.swap(b); }

#if (TCG_RVALUES_SUPPORT > 0)

  void swap(list &&other) {
    this->_swap(*this, std::move(other));
    m_begin  = other.m_begin;
    m_rbegin = other.m_rbegin;
  }

  void swap(list &a, list &&b) { a.swap(std::move(b)); }

  void swap(list &&a, list &b) { b.swap(std::move(a)); }

#endif

  //-----------------------------------------------------------------------------------------

  /*!
\details  This function is \p tcg::list's counterpart to \p std::list::splice().
        It moves the specified range to a different part of the list, \a without
        touching any element elements - just by altering node links.

\note     Unlike \p std::list::splice(), the list must be the same.
*/
  void move(size_t afterDst, size_t srcBegin,
            size_t srcEnd)  //! Moves range <TT>[srcBegin, srcEnd)</TT> right
                            //! before afterDst.      \param afterDst  Index
                            //! right after the destination position. \param
                            //! srcBegin  Start index of the moved range. \param
                            //! srcEnd  End index of the moved range.
  {
    return move(afterDst, m_begin, m_rbegin, srcBegin, srcEnd, m_rbegin);
  }

  /*!
\sa       The equivalent function move(size_t, size_t, size_t).
*/
  void move(iterator afterDst, iterator srcBegin,
            iterator srcEnd)  //! Moves range <TT>[srcBegin, srcEnd)</TT> right
                              //! before afterDst.      \param afterDst
                              //! Iterator right after the destination position.
                              //! \param srcBegin  Start of the moved range.
                              //! \param srcEnd  End of the moved range.
  {
    assert(afterDst.m_list == this && srcBegin.m_list == this &&
           srcEnd.m_list == this);

    return move(afterDst.m_idx, srcBegin.m_idx, srcEnd.m_idx);
  }

  //-----------------------------------------------------------------------------------------

  iterator insert(iterator it, const T &val) {
    return iterator(this, insert(it.m_idx, val));
  }
  iterator erase(iterator it) { return iterator(this, erase(it.m_idx)); }

  template <typename Iter>
  iterator insert(iterator it, Iter begin, Iter end) {
    return iterator(this, insert(it.m_idx, begin, end));
  }
  iterator erase(iterator begin, iterator end) {
    return iterator(this, erase(begin.m_idx, end.m_idx));
  }

  size_t push_front(const T &val) { return insert(m_begin, val); }
  size_t push_back(const T &val) { return insert(_neg, val); }

  void pop_front() { erase(m_begin); }
  void pop_back() { erase(m_rbegin); }

  T &front() { return m_vector[m_begin].value(); }
  const T &front() const { return m_vector[m_begin].value(); }

  T &back() { return m_vector[m_rbegin].value(); }
  const T &back() const { return m_vector[m_rbegin].value(); }

  iterator begin() { return iterator(this, m_begin); }
  const_iterator begin() const { return const_iterator(this, m_begin); }

  iterator last() { return iterator(this, m_rbegin); }
  const_iterator last() const { return const_iterator(this, m_rbegin); }

  iterator end() { return iterator(this, _neg); }
  const_iterator end() const { return const_iterator(this, _neg); }

#if (TCG_RVALUES_SUPPORT > 0)

  iterator insert(iterator it, T &&val) {
    return iterator(this, insert<T>(it.m_idx, std::forward<T>(val)));
  }
  size_t push_front(T &&val) {
    return insert<T>(m_begin, std::forward<T>(val));
  }
  size_t push_back(T &&val) { return insert<T>(_neg, std::forward<T>(val)); }

#endif
};

}  // namespace tcg

#endif  // TCG_LIST_H
