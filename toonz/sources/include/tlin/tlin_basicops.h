#pragma once

#ifndef TLIN_BASICOPS_H
#define TLIN_BASICOPS_H

namespace tlin {

//------------------------------------------------------------------------------

template <typename _IN1, typename _IN2, typename _OUT>
void mult(const _IN1 &, const _IN2 &, _OUT &);

//------------------------------------------------------------------------------

template <typename _IN1, typename _IN2, typename _OUT>
void sum(const _IN1 &, const _IN2 &, _OUT &);

//------------------------------------------------------------------------------

template <typename _IN1, typename _IN2, typename _OUT>
void sub(const _IN1 &, const _IN2 &, _OUT &);

//------------------------------------------------------------------------------

template <typename _T>
void transpose(const _T &);

}  // namespace tlin

#endif  // TLIN_BASICOPS_H
