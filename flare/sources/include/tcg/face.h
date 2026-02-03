#pragma once

#ifndef TCG_FACE_H
#define TCG_FACE_H

// STD includes
#include "assert.h"

namespace tcg {

template <typename F>
struct face_traits {
  typedef typename F::edges_const_iterator edges_const_iterator;
  typedef typename F::edges_iterator edges_iterator;
};

//-------------------------------------------------------------------------------

class Face {
protected:
  int m_index;
  tcg::list<int> m_edges;

public:
  typedef tcg::list<int>::const_iterator edges_const_iterator;
  typedef tcg::list<int>::iterator edges_iterator;

public:
  Face() : m_index(-1) {}
  ~Face() {}

  void setIndex(int idx) { m_index = idx; }
  int getIndex() const { return m_index; }

  int edge(int e) const { return m_edges[e]; }
  int &edge(int e) { return m_edges[e]; }
  int edgesCount() const { return (int)m_edges.size(); }

  void addEdge(int idx) { m_edges.push_back(idx); }
  edges_iterator eraseEdge(const edges_iterator &it) {
    return m_edges.erase(it);
  }

  edges_const_iterator edgesBegin() const { return m_edges.begin(); }
  edges_const_iterator edgesEnd() const { return m_edges.end(); }
  edges_iterator edgesBegin() { return m_edges.begin(); }
  edges_iterator edgesEnd() { return m_edges.end(); }
};

//-------------------------------------------------------------------------------

template <int N>
class FaceN {
public:
  typedef const int *edges_const_iterator;
  typedef int *edges_iterator;

protected:
  int m_e[N], m_count;
  int m_index;

public:
  FaceN() : m_index(-1), m_count(0) { std::fill(m_e, m_e + N, -1); }
  FaceN(int (&edges)[N]) : m_index(-1), m_count(0) {
    std::copy(edges, edges + N, m_e), m_count = N;
  }
  ~FaceN() {}

  void setIndex(int idx) { m_index = idx; }
  int getIndex() const { return m_index; }

  int edge(int e) const {
    assert(e < m_count);
    return m_e[e];
  }
  int &edge(int e) {
    assert(e < m_count);
    return m_e[e];
  }
  int edgesCount() const { return m_count; }

  void addEdge(int idx) {
    assert(m_count < N);
    m_e[m_count++] = idx;
  }
  void setEdge(int e, int idx) {
    assert(e < m_count);
    m_e[e] = idx;
  }
  edges_iterator eraseEdge(const edges_iterator &it) {
    std::copy(it + 1, edgesEnd(), it);
    m_e[--m_count] = -1;
    return it;
  }

  edges_const_iterator edgesBegin() const { return m_e; }
  edges_const_iterator edgesEnd() const { return m_e + m_count; }
  edges_iterator edgesBegin() { return m_e; }
  edges_iterator edgesEnd() { return m_e + m_count; }
};

}  // namespace tcg

#endif  // TCG_FACE_H
