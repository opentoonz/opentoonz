#pragma once

#ifndef TCG_DELETER_TYPES_H
#define TCG_DELETER_TYPES_H

// tcg includes
#include "traits.h"

/*
  \file     deleter_types.h

  \brief    This file contains implementations of some useful deleter functors
            and deleter concepts.
*/

namespace tcg {

//*********************************************************************************
//    Deleter  objects
//*********************************************************************************

template <typename T>
struct deleter {
  void operator()(T *ptr) const { delete ptr; }
};

template <typename T>
struct deleter<T[]> {
  void operator()(T *ptr) const { delete[] ptr; }
};

template <typename T>
struct dtor {
  void operator()(T *ptr) const { ptr->~T(); }
};

template <typename T>
struct dtor<T[]> {
  int m_count;

  dtor(int count) : m_count(count) {}

  void operator()(T *ptr) const {
    for (int t = 0; t != m_count; ++t) ptr[t].~T();
  }
};

struct freer {
  void operator()(void *ptr) const { free(ptr); }
};

/*!
  The Deleter concept can be used to destroy instances of incomplete types.

  Deleter objects are useful in all cases where a type-erasure concept needs
  to support specialization on incomplete types.
*/

template <typename T>
class deleter_concept {
public:
  typedef typename tcg::traits<T>::pointer_type pointer_type;

public:
  virtual ~deleter_concept() {}

  virtual deleter_concept *clone() const    = 0;
  virtual void operator()(pointer_type ptr) = 0;
};

//--------------------------------------------------------------------------------

template <typename T>
class deleter_model : public deleter_concept<T> {
public:
  deleter_concept<T> *clone() const { return new deleter_model<T>(*this); }
  void operator()(T *ptr) { delete ptr; }
};

template <typename T>
class deleter_model<T[]> : public deleter_concept<T[]> {
public:
  deleter_concept<T[]> *clone() const { return new deleter_model<T[]>(*this); }
  void operator()(T *ptr) { delete[] ptr; }
};

}  // namespace tcg

#endif  // TCG_DELETER_TYPES_H
