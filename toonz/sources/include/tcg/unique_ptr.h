#pragma once

#ifndef TCG_UNIQUE_PTR_H
#define TCG_UNIQUE_PTR_H

// tcg includes
#include "traits.h"
#include "base.h"
#include "deleter_types.h"

// STD includes
#include "assert.h"
#include <utility>

namespace tcg {

//**********************************************************************************
//    unique_ptr  definition
//**********************************************************************************

/*!
  \brief    The unique_ptr class is a C++03 compatibility class that implements
            a \a noncopyable smart pointer, similarly to \p boost::scoped_ptr,
            but accepting a custom deleter type as template parameter.

  \details  \par  Properties
            This class provides the following features:
<UL>
              <LI>It's noncopyable, like \p boost::scoped_ptr.</LI>
              <LI>Unlike \p boost::scoped_ptr, it provides the release()
method.</LI>
              <LI>Arrays are valid template types - so <TT>tcg::unique_ptr<int
[]></TT>
                  is accepted and works as expected.</LI>
              <LI>Like \p std::unique_ptr, it accepts custom \a inheritable
deallocators.</LI>
</UL>
            \par  Incomplete types
            Like \p boost::scoped_ptr, incomplete types are accepted as template
            parameter, \a provided their definition is completed by the time the
destructor
            is invoked. In particular, pointers to incomplete types \b must be
stored in
            classes whose destructor is defined in an implementation file - and
<I>remember
            that compiler-generated destructors are always implicitly
inlined</I>.

            \par  Type erasure
            Unlike shared_ptr, this class does not employ <I>type erasure</I>
            on the deleter object - which is an explicit template argument by
            default. This means that the pointed object type must be \b complete
            at the moment the \p unique_ptr is destroyed, \a except if the
            supplied deleter is type-erased on its own.

  \remark   The C++11 class \p std::unique_ptr should be preferred, if possible.
*/

template <typename T, typename D = typename tcg::deleter<T>>
class unique_ptr : private D  // Empty Base Optimization
{
public:
  typedef typename tcg::traits<T>::element_type element_type;
  typedef typename tcg::traits<element_type>::pointer_type ptr_type;
  typedef typename tcg::traits<element_type>::reference_type ref_type;

public:
  explicit unique_ptr(ptr_type ptr = ptr_type())  // Explicit unary constructors
      : m_ptr(ptr) {}
  explicit unique_ptr(D d) : m_ptr(), D(d) {}  //
  unique_ptr(ptr_type ptr, D d) : m_ptr(ptr), D(d) {}

  ~unique_ptr() { D::operator()(m_ptr); }

  friend void swap(unique_ptr &a, unique_ptr &b) {
    using std::swap;

    swap(static_cast<D &>(a), static_cast<D &>(b));
    swap(a.m_ptr, b.m_ptr);
  }

  // Explicitly disabled (safe) conversion to bool - although
  // std::unique_ptr could support it, that would just add overhead.
  // It's also not compatible with other C++03 smart pointers.

  // typedef ptr_type unique_ptr::*                                 bool_type;

  // operator bool_type() const                                     // Safe bool
  // idiom
  //  { return m_ptr ? &unique_ptr::m_ptr : 0; }                   // additional
  //  branching!

  ptr_type operator->() const { return m_ptr; }
  ref_type operator*() const { return *m_ptr; }
  ref_type operator[](size_t idx) const { return m_ptr[idx]; }

  void reset(ptr_type ptr = ptr_type()) {
    D::operator()(m_ptr);
    m_ptr = ptr;
  }

  void reset(ptr_type ptr, D d) {
    reset(ptr);
    D::operator=(d);
  }

  ptr_type release() {
    ptr_type ptr = m_ptr;
    m_ptr        = ptr_type();
    return ptr;
  }

  const ptr_type get() const { return m_ptr; }
  ptr_type get() { return m_ptr; }

private:
  ptr_type m_ptr;
};

}  // namespace tcg

#endif  // TCG_UNIQUE_PTR_H
