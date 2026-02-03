#pragma once

#ifndef TLIN_VECTOR_H
#define TLIN_VECTOR_H

//----------------------------------------------------------------------------

/*!
  The Vector class represents a vector in tlin-compatible algorithms.
*/

template <typename T>
class vector {
public:
  vector(int size);
  ~vector();

  vector(const vector &);
  vector &operator=(const vector &);

  int size() const;

  T &operator[](int i);
  const T &operator[](int i) const;
};

#endif  // TLIN_MATRIX_H
