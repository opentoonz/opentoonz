#pragma once

#ifndef TCG_BASE_H
#define TCG_BASE_H

namespace tcg {

/*!
  \file   tcg_base.h

  \brief  This file contains the implementation of some handy base classes
          that can be inherited from.
*/

//========================================================================

/*!
  \brief  The empty_type can be used as default template parameter in cases
          where the template parameter may be omitted.
*/
struct empty_type {};

//------------------------------------------------------------------------

/*!
  \brief  The noncopyable class can be inherited to forbid access to the
          copy constructor and assignment operator.

  \note   A template parameter is provided to permit the empty base
          class optimization. In case this optimization is not needed,
          please use boost::noncopyable instead.
*/

template <typename B = empty_type>
struct noncopyable : public B {
  noncopyable() {}
  // noncopyable(const B& b) : B(b) {}                 // Would introduce
  // additional copies
  // along the inheritance chain. Not worth it.
protected:
  ~noncopyable() {}  //!< Protected destructor since the class
                     //!  is intended for nonvirtual inheritance.
private:
  noncopyable(const noncopyable &);  //!< Non-accessible copy constructor.
  noncopyable &operator=(
      const noncopyable &);  //!< Non-accessible assignment operator.
};

//------------------------------------------------------------------------

/*!
  \brief    The polymorphic class just implements an empty class
            with a virtual destructor.

  \details  It can be useful in certain occasions:

              \li Explicitly marks derived classes as polymporphic, and
                  spares the need to write a virtual destructor.
              \li It's noncopyable, disabling value semantics.
              \li Provides a common base class to polymorphic hierarchies.
              \li Enables lightweight type erasure without resorting to
                  a wrapper class like boost::any, assuming you have
                  enough control of the involved class to add a base class.
*/

class polymorphic : noncopyable<>  // Noncopyable to prevent slicing
{
protected:
  polymorphic() {}  //!< Protected constructor to ensure that the
                    //!  class is only used as base class.
public:
  virtual ~polymorphic() {}  //!< A virtual destructor as every good base
                             //!  class must have.
};

//------------------------------------------------------------------------

/*!
  The tcg::safe_bool template class is a simple implementation
  of the classic Safe Bool Idiom, meant to be used inside tcg.

  For a better implementation, you should consider looking into
  Boost.Spirit.Safe_Bool.
*/

template <typename T, typename B = empty_type>  // B is used for base class
// chaining, to deal with
class safe_bool : public B  // the empty class optimization
{
  class dummy {};
  struct detail {
    dummy *member;
  };

public:
  typedef dummy *detail::*bool_type;

public:
  safe_bool() {}
  // safe_bool(const B& b) : B(b) {}         // Would introduce additional
  // copies
  // along the inheritance chain. Not worth it.
  operator bool_type() const {
    return static_cast<const T *>(this)->operator_bool() ? &detail::member : 0;
  }

protected:
  ~safe_bool() {}  //!< Protected destructor since the class
                   //!  is intended for nonvirtual inheritance.
private:
  bool operator==(const safe_bool &);
  bool operator!=(const safe_bool &);
};

}  // namespace tcg

#endif  // TCG_BASE_H
