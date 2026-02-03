#pragma once

#ifndef TCG_ALIGNMENT_H
#define TCG_ALIGNMENT_H

#include "macros.h"

/*! \file alignment.h

  This file contains C++ utilities about types alignment.
*/

namespace tcg {

//**************************************************************************
//    Private  stuff
//**************************************************************************

// From http://stackoverflow.com/questions/6959261/how-can-i-simulate-alignast

union _MaxAlign {
  int i;
  long l;
  long long ll;
  long double ld;
  double d;
  void *p;
  void (*pf)();
  _MaxAlign *ps;
};

//---------------------------------------------------------------

template <typename T, bool>
struct _AlignTypeDetail;

template <typename T>
struct _AlignTypeDetail<T, false> {
  typedef T type;
};

template <typename T>
struct _AlignTypeDetail<T, true> {
  typedef char type;
};

template <size_t alignment, typename U>
struct _AlignType {
  typedef typename _AlignTypeDetail<U, (alignment < sizeof(U))>::type type;
};

template <typename T>
struct _Aligner {
  char c;
  T t;
};

//**************************************************************************
//    TCG alignment  unions
//**************************************************************************

template <int alignment>
union aligner_type {
private:
  typename _AlignType<alignment, char>::type c;
  typename _AlignType<alignment, short>::type s;
  typename _AlignType<alignment, int>::type i;
  typename _AlignType<alignment, long>::type l;
  typename _AlignType<alignment, long long>::type ll;
  typename _AlignType<alignment, float>::type f;
  typename _AlignType<alignment, double>::type d;
  typename _AlignType<alignment, long double>::type ld;
  typename _AlignType<alignment, void *>::type pc;
  typename _AlignType<alignment, _MaxAlign *>::type ps;
  typename _AlignType<alignment, void (*)()>::type pf;
};

//---------------------------------------------------------------

template <typename T>
union aligned_buffer {
  typedef aligner_type<sizeof(_Aligner<T>) - sizeof(T)> aligner_type_t;

  aligner_type_t m_aligner;
  char m_buf[sizeof(T)];

private:
  TCG_STATIC_ASSERT(sizeof(_Aligner<T>) - sizeof(T) ==
                    sizeof(_Aligner<aligner_type_t>) - sizeof(aligner_type_t));
};

//**************************************************************************
//    TCG alignment  traits
//**************************************************************************

template <typename T>
struct alignment_traits {
  static const int alignment = sizeof(_Aligner<T>) - sizeof(T);

  typedef aligner_type<alignment> aligner_type_traits;
  typedef aligned_buffer<T> buffer_type;
};

}  // namespace tcg

#endif  // TCG_ALIGNMENT_H
