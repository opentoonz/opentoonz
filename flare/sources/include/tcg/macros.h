#pragma once

#ifndef TCG_MACROS_H
#define TCG_MACROS_H

/*!
  \file     macros.h

  \brief    This file contains some useful macros that can be used
            with tcg and generic C++ projects.
*/

/*!
  \def      TCG_ASSERT(assertion, failure_expr)

  \brief    The TCG_ASSERT instruction is an assertion that also provides
            a valid failure statement in case the asserted expression fails.

  \details  Assert statements are used to enforce pre- and post-conditions to
            algorithms, in a way that is \b not intended to be broken - in
            other words, after a failed assert statement we should expect
            nothing more than undefined behavior. The assert statement itself
            will signal a failed expression, <I> and then will force the
            program to quit <\I>.

            However, there exist cases in which at the same time:

              \li It is \a known that a pre/post-condition could be possibly
  broken
              \li It should still be considered an error
              \li Yet, a worst-case bail-out policy must be provided

            When this happen, you should:

              \li Feel offended - it should <B>really not<\B> happen. Errors in
                  meeting requirements should never be tolerated
              \li Use TCG_ASSERT. It does not solve things, but is a simple
                  statement and defines a trackable (grep-able) string in the
  project
*/

#define TCG_ASSERT(assertion, failure_expr)                                    \
                                                                               \
  if (!(assertion)) {                                                          \
    assert(assertion);                                                         \
    failure_expr;                                                              \
  } else                                                                       \
    (void)0

/*!
  \def      TCG_JOIN(A, B)
  \brief    Pastes the supplied arguments together, even when the arguments
            are macros themselves (typically seen with __LINE__)
  \sa       Taken from BOOST_JOIN(A, B)
*/
#define TCG_DO_JOIN2(A, B) A##B
#define TCG_DO_JOIN(A, B) TCG_DO_JOIN2(A, B)  // Argument macro expansion here
#define TCG_JOIN(A, B) TCG_DO_JOIN(A, B)

/*!
  \def      TCG_STATIC_ASSERT(expr)

  \brief    Performs a compile-time check of the specified assertion.
  \warning  This macro uses the file's line number to name a hidden structure.
            Please place \b only \b one static assert per line.
*/

// See http://stackoverflow.com/questions/9229601/what-is-in-c-code

#define TCG_STATIC_ASSERT(expr)                                                \
  struct TCG_JOIN(_tcg_static_assert_, __LINE__) {                             \
    int : -int(!(expr));                                                       \
  }

/*!
  \def      TCG_DEBUG(expr)

  \brief    Enables the specified expression in case NDEBUG is not defined,
            thus binding it to the behavior of the standard assert() macro.
*/

#ifdef NDEBUG
#define TCG_DEBUG(expr1)
#define TCG_DEBUG2(expr1, expr2)
#define TCG_DEBUG3(expr1, exrp2, expr3)
#define TCG_DEBUG4(expr1, expr2, expr3, expr4)
#define TCG_DEBUG5(expr1, expr2, expr3, expr4, expr5)
#else
#define TCG_DEBUG(expr1) expr1
#define TCG_DEBUG2(expr1, expr2) expr1, expr2
#define TCG_DEBUG3(expr1, exrp2, expr3) expr1, expr2, expr3
#define TCG_DEBUG4(expr1, expr2, expr3, expr4) expr1, expr2, expr3, expr4
#define TCG_DEBUG5(expr1, expr2, expr3, expr4, expr5)                          \
  expr1, expr2, expr3, expr4, expr5
#endif

#endif  // TCG_MACROS_H
