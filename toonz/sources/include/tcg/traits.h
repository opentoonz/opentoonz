#pragma once

#ifndef TCG_TRAITS_H
#define TCG_TRAITS_H

// tcg includes
#include "sfinae.h"

// STD includes
#include <iterator>

//--------------------------------------------------------------------------

namespace tcg {

//****************************************************************************
//    TCG traits for generic type concepts
//****************************************************************************

template <typename T>
struct traits {
  typedef T *pointer_type;
  typedef T pointed_type;
  typedef T &reference_type;
  typedef T referenced_type;
  typedef T element_type;
};

template <typename T>
struct traits<T *> {
  typedef T **pointer_type;
  typedef T pointed_type;
  typedef T *&reference_type;
  typedef T *referenced_type;
  typedef T *element_type;
};

template <typename T>
struct traits<T[]> : public traits<T *> {
  typedef T element_type;
};

template <typename T>
struct traits<T &> : public traits<T> {};

template <>
struct traits<void> {
  typedef void *pointer_type;
  typedef void pointed_type;
  typedef void reference_type;
  typedef void referenced_type;
};

//****************************************************************************
//    Qualifier removers
//****************************************************************************

template <typename T>
struct remove_const {
  typedef T type;
};
template <typename T>
struct remove_const<const T> {
  typedef T type;
};

template <typename T>
struct remove_ref {
  typedef T type;
};
template <typename T>
struct remove_ref<T &> {
  typedef T type;
};

template <typename T>
struct remove_cref {
  typedef typename remove_const<typename remove_ref<T>::type>::type type;
};

//****************************************************************************
//    TCG traits for function types
//****************************************************************************

template <typename Func>
class function_traits {
  template <typename F, bool>
  struct result;

  template <typename F>
  struct result<F, true> {
    typedef typename F::result_type type;
  };

  template <typename F>
  struct result<F, false> {
    typedef struct { } type; };

  template <typename Q>
  static typename enable_if_exists<typename Q::result_type, char>::type
  result_fun(Q *);
  static double result_fun(...);

  template <typename F, bool>
  struct argument;

  template <typename F>
  struct argument<F, true> {
    typedef typename F::argument_type type;
  };

  template <typename F>
  struct argument<F, false> {
    typedef void type;
  };
  template <typename Q>

  static typename enable_if_exists<typename Q::argument_type, char>::type
  argument_fun(Q *);
  static double argument_fun(...);

  template <typename F, bool>
  struct first_arg;

  template <typename F>
  struct first_arg<F, true> {
    typedef typename F::first_argument_type type;
  };

  template <typename F>
  struct first_arg<F, false> {
    typedef void type;
  };
  template <typename Q>

  static typename enable_if_exists<typename Q::first_argument_type, char>::type
  first_arg_fun(Q *);
  static double first_arg_fun(...);

  template <typename F, bool>
  struct second_arg;

  template <typename F>
  struct second_arg<F, true> {
    typedef typename F::second_argument_type type;
  };

  template <typename F>
  struct second_arg<F, false> {
    typedef void type;
  };

  template <typename Q>
  static typename enable_if_exists<typename Q::second_argument_type, char>::type
  second_arg_fun(Q *);
  static double second_arg_fun(...);

public:
  enum {
    has_result =
        (sizeof(result_fun(typename tcg::traits<Func>::pointer_type())) ==
         sizeof(char)),
    has_argument =
        (sizeof(argument_fun(typename tcg::traits<Func>::pointer_type())) ==
         sizeof(char)),
    has_first_arg =
        (sizeof(first_arg_fun(typename tcg::traits<Func>::pointer_type())) ==
         sizeof(char)),
    has_second_arg =
        (sizeof(second_arg_fun(typename tcg::traits<Func>::pointer_type())) ==
         sizeof(char))
  };

  typedef typename result<Func, has_result>::type ret_type;
  typedef typename argument<Func, has_argument>::type arg_type;
  typedef typename first_arg<Func, has_first_arg>::type arg1_type;
  typedef typename second_arg<Func, has_second_arg>::type arg2_type;
};

//-----------------------------------------------------------------------

template <typename Ret>
struct function_traits<Ret()> {
  enum {
    has_result     = true,
    has_argument   = false,
    has_first_arg  = false,
    has_second_arg = false
  };

  typedef Ret ret_type;
  typedef void arg_type;
  typedef void arg1_type;
  typedef void arg2_type;
};

template <typename Ret>
struct function_traits<Ret (*)()> : public function_traits<Ret()> {};

template <typename Ret>
struct function_traits<Ret (&)()> : public function_traits<Ret()> {};

//-----------------------------------------------------------------------

template <typename Ret, typename Arg>
struct function_traits<Ret(Arg)> {
  enum {
    has_result     = true,
    has_argument   = true,
    has_first_arg  = false,
    has_second_arg = false
  };

  typedef Ret ret_type;
  typedef Arg arg_type;
  typedef void arg1_type;
  typedef void arg2_type;
};

template <typename Ret, typename Arg>
struct function_traits<Ret (*)(Arg)> : public function_traits<Ret(Arg)> {};

template <typename Ret, typename Arg>
struct function_traits<Ret (&)(Arg)> : public function_traits<Ret(Arg)> {};

//-----------------------------------------------------------------------

template <typename Ret, typename Arg1, typename Arg2>
struct function_traits<Ret(Arg1, Arg2)> {
  enum { has_result = true, has_first_arg = true, has_second_arg = true };

  typedef Ret ret_type;
  typedef Arg1 arg1_type;
  typedef Arg2 arg2_type;
};

template <typename Ret, typename Arg1, typename Arg2>
struct function_traits<Ret (*)(Arg1, Arg2)>
    : public function_traits<Ret(Arg1, Arg2)> {};

template <typename Ret, typename Arg1, typename Arg2>
struct function_traits<Ret (&)(Arg1, Arg2)>
    : public function_traits<Ret(Arg1, Arg2)> {};

//******************************************************************************
//    TCG traits for output container readers
//******************************************************************************

template <typename Reader, typename OutputData = typename Reader::value_type>
struct container_reader_traits {
  typedef Reader reader_type;
  typedef OutputData value_type;

  static void openContainer(reader_type &reader) { reader.openContainer(); }
  static void addElement(reader_type &reader, const value_type &data) {
    reader.addElement(data);
  }
  static void closeContainer(reader_type &reader) { reader.closeContainer(); }
};

//************************************************************************************
//    Notable Test  traits
//************************************************************************************

template <typename T>
struct is_floating_point {
  enum { value = false };
};

template <>
struct is_floating_point<float> {
  enum { value = true };
};

template <>
struct is_floating_point<double> {
  enum { value = true };
};

template <>
struct is_floating_point<long double> {
  enum { value = true };
};

//-----------------------------------------------------------------------

template <typename T>
struct is_function {
  enum { value = function_traits<T>::has_result };
};

template <typename T>
struct is_functor {
  template <typename Q>
  static typename enable_if_exists<typename Q::result_type, char>::type result(
      Q *);

  static double result(...);

  enum {
    value = (sizeof(result(typename tcg::traits<T>::pointer_type())) ==
             sizeof(char))
  };
};

}  // namespace tcg

#endif  // TCG_TRAITS_H
