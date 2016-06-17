#pragma once

#ifndef TCG_FUNCTION_TYPES_H
#define TCG_FUNCTION_TYPES_H

// tcg includes
#include "traits.h"

// STD includes
#include <functional>

/*!
  \file     function_types.h

  \brief    This file contains some template class types useful to convert
  functions
            into functors <I> at compile-time <\I>, with minimal memory
  overhead.
*/

namespace tcg {

//************************************************************************************
//    Function Objects  definition
//************************************************************************************

template <typename Func, Func f>
struct function;

//---------------------------------------------------------------------------

template <typename Ret, Ret (*f)()>
struct function<Ret (*)(), f> : public std::unary_function<void, Ret> {
  Ret operator()() const { return f(); }
};

//---------------------------------------------------------------------------

template <typename Ret, typename Arg, Ret (*f)(Arg)>
struct function<Ret (*)(Arg), f> : public std::unary_function<Arg, Ret> {
  Ret operator()(Arg arg) const { return f(arg); }
};

//---------------------------------------------------------------------------

template <typename Ret, typename Arg1, typename Arg2, Ret (*f)(Arg1, Arg2)>
struct function<Ret (*)(Arg1, Arg2), f>
    : public std::binary_function<Arg1, Arg2, Ret> {
  Ret operator()(Arg1 arg1, Arg2 arg2) const { return f(arg1, arg2); }
};

//************************************************************************************
//    Member Function Objects  definition
//************************************************************************************

template <typename C, typename Ret, Ret (C::*f)()>
struct function<Ret (C::*)(), f> : public std::unary_function<C &, Ret> {
  Ret operator()(C &c) const { return (c.*f)(); }
};

//---------------------------------------------------------------------------

template <typename C, typename Ret, Ret (C::*f)() const>
struct function<Ret (C::*)() const, f>
    : public std::unary_function<const C &, Ret> {
  Ret operator()(const C &c) const { return (c.*f)(); }
};

//---------------------------------------------------------------------------

template <typename C, typename Ret, typename Arg, Ret (C::*f)(Arg)>
struct function<Ret (C::*)(Arg), f>
    : public std::binary_function<C &, Arg, Ret> {
  Ret operator()(C &c, Arg arg) const { return (c.*f)(arg); }
};

//---------------------------------------------------------------------------

template <typename C, typename Ret, typename Arg, Ret (C::*f)(Arg) const>
struct function<Ret (C::*)(Arg) const, f>
    : public std::binary_function<const C &, Arg, Ret> {
  Ret operator()(const C &c, Arg arg) const { return (c.*f)(arg); }
};

//************************************************************************************
//    Binder functors
//************************************************************************************

/*!
  \brief    Binary function object binder generalizing \p std::binder1st with
            an explicitly specifiable bound type.

  \note     Unlike std::binder1st, this binder accepts functors whose arguments
            are reference types.

  \sa       Helper function tcg::bind1st().

  \remark   This class currently <I>adds an additional arguments copy</I>.
            Be warned of this when using heavyweight argument types.
*/

template <class Func, typename Arg = typename function_traits<Func>::arg1_type>
class binder1st
    : public std::unary_function<
          typename function_traits<Func>::arg2_type,  // Forward function types
          typename function_traits<Func>::ret_type>   //
{
public:
  binder1st(const Func &func, Arg arg1) : m_func(func), m_arg1(arg1) {}

  typename function_traits<Func>::ret_type operator()(
      typename function_traits<Func>::arg2_type arg2)
      const  // NOTE: Double copy
  {
    return m_func(m_arg1, arg2);
  }

protected:
  Func m_func;
  Arg m_arg1;
};

//---------------------------------------------------------------------------

template <class Func, typename Arg = typename function_traits<Func>::arg2_type>
class binder2nd
    : public std::unary_function<typename function_traits<Func>::arg1_type,
                                 typename function_traits<Func>::ret_type> {
public:
  binder2nd(const Func &func, Arg arg2) : m_func(func), m_arg2(arg2) {}

  typename function_traits<Func>::ret_type operator()(
      typename function_traits<Func>::arg1_type arg1) const {
    return m_func(arg1, m_arg2);
  }

protected:
  Func m_func;
  Arg m_arg2;
};

//---------------------------------------------------------------------------

/*!
  \brief    Helper function for the creation of binder objects with automatic
            type deduction.

  \warning  Unlike std::bind1st, the bound argument type is the one <I>declared
            by the function object</I>. This is typically a commodity, but
            be aware of the subtle implications:

              \li Only parameters copied by value result in a copied
                  bound argument.
              \li Avoid temporaries as bound reference arguments.

  \sa       Class tcg::binder1st.
*/

template <typename Func, typename Arg>
inline binder1st<Func> bind1st(const Func &func, const Arg &arg1) {
  return binder1st<Func>(func, arg1);
}

template <typename Func, typename Arg>
inline binder1st<Func> bind1st(const Func &func, Arg &arg1) {
  return binder1st<Func>(func, arg1);
}

//---------------------------------------------------------------------------

template <typename Func, typename Arg>
inline binder2nd<Func> bind2nd(const Func &func, const Arg &arg2) {
  return binder2nd<Func>(func, arg2);
}

template <typename Func, typename Arg>
inline binder2nd<Func> bind2nd(const Func &func, Arg &arg2) {
  return binder2nd<Func>(func, arg2);
}

}  // namespace tcg

#endif  // TCG_FUNCTION_TYPES_H
