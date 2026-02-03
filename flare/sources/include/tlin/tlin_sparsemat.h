#pragma once

#ifndef TLIN_SPARSEMAT_H
#define TLIN_SPARSEMAT_H

#include "tlin_basicops.h"

#include "tcg/tcg_hash.h"

namespace tlin {

//-------------------------------------------------------------------------------

/*!
  The tlin::sparse_matrix class implements a hash-based format for storing
  sparse
  matrices.

  A tlin::sparse_matrix is a tlin::matrix-compliant class that allows the user
  to
  access the matrix in a purely random fashion, without sacrificing storage
  space or
  efficiency. This is achieved using a single, fast, dynamic hash container that
  maps
  (row, col) entries into their values, the hash function being like:

    (row, col) -> row * nCols + col;    (taken modulo nBuckets)

  Observe that if the number of buckets of the hash map is nCols, the resulting
  representation is similar to a CCS (Harwell-Boeing), with the difference that
  buckets are not sorted by row.
*/

template <typename T>
class sparse_matrix {
public:
  struct IdxFunctor {
    int m_cols;

    IdxFunctor(int cols) : m_cols(cols) {}
    int operator()(const std::pair<int, int> &key) {
      return key.first * m_cols + key.second;
    }
  };

private:
  int m_rows;
  int m_cols;

  tcg::hash<std::pair<int, int>, T, IdxFunctor> m_hash;

public:
  sparse_matrix() : m_rows(0), m_cols(0), m_hash(IdxFunctor(0)) {}
  sparse_matrix(int rows, int cols)
      : m_rows(rows), m_cols(cols), m_hash(cols) {}
  ~sparse_matrix() {}

  sparse_matrix(const sparse_matrix &mat)
      : m_rows(mat.m_rows), m_cols(mat.m_cols), m_hash(mat.m_hash) {}

  sparse_matrix &operator=(const sparse_matrix &mat) {
    m_rows = mat.m_rows, m_cols = mat.m_cols, m_hash = mat.m_hash;
    return *this;
  }

  int rows() const { return m_rows; }
  int cols() const { return m_cols; }

  typedef typename tcg::hash<std::pair<int, int>, T, IdxFunctor> HashMap;
  tcg::hash<std::pair<int, int>, T, IdxFunctor> &entries() { return m_hash; }
  const tcg::hash<std::pair<int, int>, T, IdxFunctor> &entries() const {
    return m_hash;
  }

  //!\warning Assignments of kind: A.at(1,1) = A.at(0,0) are potentially
  //! harmful.
  //! Use A.get(x,y) on the right side instead.
  T &at(int row, int col) { return m_hash.touch(std::make_pair(row, col), 0); }
  const T get(int row, int col) const {
    typename HashMap::const_iterator it = m_hash.find(std::make_pair(row, col));
    return (it == m_hash.end()) ? 0 : it->m_val;
  }
  const T operator()(int row, int col) const { return get(row, col); }

  void clear() { m_hash.clear(); }

  friend void swap(sparse_matrix &a, sparse_matrix &b) {
    using std::swap;

    swap(a.m_rows, b.m_rows), swap(a.m_cols, b.m_cols);
    swap(a.m_hash, b.m_hash);
  }
};

//-------------------------------------------------------------------

//  The Standard matrix data type in tlin is double

typedef tlin::sparse_matrix<double> spmat;
}

#endif  // TLIN_SPARSEMAT_H
