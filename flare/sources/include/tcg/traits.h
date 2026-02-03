#pragma once

#ifndef TCG_TRAITS_H
#define TCG_TRAITS_H

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

}  // namespace tcg

#endif  // TCG_TRAITS_H
