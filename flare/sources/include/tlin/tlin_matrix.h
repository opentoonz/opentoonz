#pragma once

#ifndef TLIN_MATRIX_H
#define TLIN_MATRIX_H

#include "assert.h"

#include "tlin_basicops.h"

namespace tlin {

//----------------------------------------------------------------------------

/*!
  The tlin::matrix class represents matrices in tlin-compatible algorithms.
  This implementation both serves as a reference to other would-be matrix types,
  specifying the core methods they must provide to work in place of a
  tcg::matrix;
  plus, it provides the naive, universal dense-format of a matrix type.
  Please, observe that a tlin::matrix is stored in column-major order, unlike
  common
  C matrices, since it has to comply with the usual Fortran-style matrices
  supported
  by every BLAS implementation. In practice, this means that the values() array
  stores \b columns in consecutive data blocks.
*/

template <typename T>
class matrix {
  int m_rows;
  int m_cols;

  T *m_entries;

public:
  matrix() : m_rows(0), m_cols(0), m_entries(0) {}
  matrix(int rows, int cols)
      : m_rows(rows), m_cols(cols), m_entries(new T[rows * cols]) {
    memset(m_entries, 0, m_rows * m_cols * sizeof(T));
  }
  ~matrix() { delete[] m_entries; }

  matrix(const matrix &mat)
      : m_rows(mat.m_rows)
      , m_cols(mat.m_cols)
      , m_entries(new T[m_rows * m_cols]) {
    memcpy(m_entries, mat.m_entries, m_rows * m_cols * sizeof(T));
  }

  matrix &operator=(const matrix &mat) {
    if (m_rows != mat.m_rows || m_cols != mat.m_cols) {
      delete[] m_entries;
      m_rows = mat.m_rows, m_cols = mat.m_cols,
      m_entries = new T[m_rows * m_cols];
    }

    memcpy(m_entries, mat.m_entries, m_rows * m_cols * sizeof(T));
    return *this;
  }

  int rows() const { return m_rows; }
  int cols() const { return m_cols; }

  T &at(int row, int col) { return m_entries[m_rows * col + row]; }
  const T &get(int row, int col) const { return m_entries[m_rows * col + row]; }
  const T &operator()(int row, int col) const { return get(row, col); }

  //-----------------------------------------------------------------------------

  //  Dense-specific methods
  matrix(int rows, int cols, T val)
      : m_rows(rows), m_cols(cols), m_entries(new T[rows * cols]) {
    fill(val);
  }

  T *values() { return m_entries; }
  const T *values() const { return m_entries; }

  void fill(const T &val) {
    memset(m_entries, val, m_rows * m_cols * sizeof(T));
  }
};

//-------------------------------------------------------------------------

//  The Standard matrix data type in tlin is double

typedef tlin::matrix<double> mat;

}  // namespace tlin

#endif  // TLIN_MATRIX_H
