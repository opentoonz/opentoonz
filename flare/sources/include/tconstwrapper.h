#pragma once

#ifndef TCONSTWRAPPER_INCLUDED
#define TCONSTWRAPPER_INCLUDED

#include <cstddef>
#include <cassert>
#include <vector>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================


template<typename T, typename TT>
class TConstArrayWrapperT {
public:
  typedef T value_type;
  typedef TT src_value_type;

  class reverse_iterator;

  class iterator {
  private:
    const src_value_type *pointer;
    friend class TConstArrayWrapperT<value_type, src_value_type>;
    friend class reverse_iterator;

    explicit iterator(const src_value_type *pointer): pointer(pointer) { }
  public:
    iterator(): pointer() { }
    iterator(const iterator &other): pointer(other.pointer) { }
    explicit iterator(const reverse_iterator &other): pointer(other.pointer) { }

    iterator& operator=(const iterator &other) const { pointer = other.pointer; return *this; }

    bool operator==(const iterator &other) const { return pointer == other.pointer; }
    bool operator!=(const iterator &other) const { return pointer != other.pointer; }
    bool operator< (const iterator &other) const { return pointer <  other.pointer; }
    bool operator> (const iterator &other) const { return pointer >  other.pointer; }
    bool operator<=(const iterator &other) const { return pointer <= other.pointer; }
    bool operator>=(const iterator &other) const { return pointer >= other.pointer; }
    ptrdiff_t operator-(const iterator &other) const { return pointer - other.pointer; }
    iterator operator+(ptrdiff_t diff) const { return iterator(pointer + diff); }
    iterator operator-(ptrdiff_t diff) const { return iterator(pointer - diff); }

    iterator& operator++()   { ++pointer; return *this; }
    iterator& operator--()   { --pointer; return *this; }
    iterator operator++(int) { ++pointer; return iterator(pointer-1); }
    iterator operator--(int) { --pointer; return iterator(pointer+1); }
    iterator operator+=(ptrdiff_t diff) { pointer += diff; return *this; }
    iterator operator-=(ptrdiff_t diff) { pointer -= diff; return *this; }

    const value_type* operator->() const { assert(pointer); return cast(pointer); }
    const value_type& operator*()  const { assert(pointer); return *cast(pointer); }
  };

  class reverse_iterator {
  private:
    const src_value_type *pointer;
    friend class TConstArrayWrapperT<value_type, src_value_type>;

    explicit reverse_iterator(const src_value_type *pointer): pointer(pointer) { }
  public:
    reverse_iterator(): pointer() { }
    reverse_iterator(const reverse_iterator &other): pointer(other.pointer) { }
    explicit reverse_iterator(const iterator &other): pointer(other.pointer) { }

    reverse_iterator& operator=(const reverse_iterator &other) const { pointer = other.pointer; return *this; }

    bool operator==(const reverse_iterator &other) const { return pointer == other.pointer; }
    bool operator!=(const reverse_iterator &other) const { return pointer != other.pointer; }
    bool operator< (const reverse_iterator &other) const { return pointer >  other.pointer; }
    bool operator> (const reverse_iterator &other) const { return pointer <  other.pointer; }
    bool operator<=(const reverse_iterator &other) const { return pointer >= other.pointer; }
    bool operator>=(const reverse_iterator &other) const { return pointer <= other.pointer; }
    ptrdiff_t operator-(const reverse_iterator &other) const { return other.pointer - pointer; }
    reverse_iterator operator+(ptrdiff_t diff) const { return reverse_iterator(pointer - diff); }
    reverse_iterator operator-(ptrdiff_t diff) const { return reverse_iterator(pointer + diff); }

    reverse_iterator& operator++()   { --pointer; return *this; }
    reverse_iterator& operator--()   { ++pointer; return *this; }
    reverse_iterator operator++(int) { --pointer; return reverse_iterator(pointer+1); }
    reverse_iterator operator--(int) { ++pointer; return reverse_iterator(pointer-1); }
    reverse_iterator operator+=(ptrdiff_t diff) { pointer -= diff; return *this; }
    reverse_iterator operator-=(ptrdiff_t diff) { pointer += diff; return *this; }

    const value_type* operator->() const { assert(pointer); return cast(pointer); }
    const value_type& operator*()  const { assert(pointer); return *cast(pointer); }
  };

private:
  const src_value_type *m_begin;
  const src_value_type *m_end;
  const src_value_type *m_rbegin;
  const src_value_type *m_rend;
  size_t m_size;

  static const value_type* cast(const src_value_type *pointer)
    { return static_cast<const value_type*>(pointer); }

public:
  TConstArrayWrapperT():
    m_begin(), m_end(), m_rbegin(), m_rend(), m_size() { }
  TConstArrayWrapperT(const src_value_type *data, size_t size):
    m_begin(), m_end(), m_rbegin(), m_rend(), m_size() { set(data, size); }
  TConstArrayWrapperT(const TConstArrayWrapperT &other):
    m_begin(), m_end(), m_rbegin(), m_rend(), m_size() { *this = other; }
  explicit TConstArrayWrapperT(const std::vector<TT> &v):
    m_begin(), m_end(), m_rbegin(), m_rend(), m_size() { set(v); }

  void reset() {
    m_begin  = 0;
    m_end    = 0;
    m_rbegin = 0;
    m_rend   = 0;
    m_size   = 0;
  }

  void set(const src_value_type *data, size_t size) {
    assert((data != 0) == (size != 0));
    if (!data) reset();
    m_begin  = data;
    m_end    = data + size;
    m_rbegin = m_end - 1;
    m_rend   = m_begin - 1;
    m_size   = size;
  }

  void set(const std::vector<TT> &v)
    { if (v.empty()) reset(); else set(&v.front(), v.size()); }

  TConstArrayWrapperT& operator=(const TConstArrayWrapperT &other)
    { set(other.m_begin, other.m_size); return *this; }

  size_t size() const { return m_size; }
  bool empty() const { return m_size != 0; }

  iterator begin() const
    { return iterator(m_begin); }
  iterator end() const
    { return iterator(m_end); }
  reverse_iterator rbegin() const
    { return reverse_iterator(m_rbegin); }
  reverse_iterator rend() const
    { return reverse_iterator(m_rend); }
  const value_type& at (size_t index) const
    { assert(index < m_size); return *cast(m_begin + index); }
  const value_type& operator[] (size_t index) const
    { return at(index); }
  const value_type& front() const
    { assert(!empty()); return *cast(m_begin); }
  const value_type& back() const
    { assert(!empty()); return *cast(m_rbegin); }
};

#endif
