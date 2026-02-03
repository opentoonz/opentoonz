#pragma once

#ifndef TCG_EDGE_H
#define TCG_EDGE_H

// STD includes
#include "assert.h"

namespace tcg {

template <typename E>
struct edge_traits {
  typedef typename E::vertices_const_iterator vertices_const_iterator;
  typedef typename E::vertices_iterator vertices_iterator;

  typedef typename E::faces_const_iterator faces_const_iterator;
  typedef typename E::faces_iterator faces_iterator;
};

//-------------------------------------------------------------------------------

class Edge {
public:
  typedef const int *vertices_const_iterator;
  typedef int *vertices_iterator;

  typedef const int *faces_const_iterator;
  typedef int *faces_iterator;

protected:
  int m_v[2];
  int m_f[2];
  int m_index;

public:
  Edge() : m_index(-1) {
    m_v[0] = -1, m_v[1] = -1;
    m_f[0] = -1, m_f[1] = -1;
  }
  Edge(int v1, int v2) : m_index(-1) {
    m_v[0] = v1, m_v[1] = v2;
    m_f[0] = -1, m_f[1] = -1;
  }
  ~Edge() {}

  void setIndex(int idx) { m_index = idx; }
  int getIndex() const { return m_index; }

  int vertex(int i) const { return m_v[i]; }
  int verticesCount() const { return m_v[0] < 0 ? 0 : m_v[1] < 0 ? 1 : 2; }
  int otherVertex(int v) const { return m_v[0] == v ? m_v[1] : m_v[0]; }

  void addVertex(int v) {
    assert(verticesCount() < 2);
    m_v[verticesCount()] = v;
  }
  void setVertex(int i, int v) { assert(i < verticesCount()), m_v[i] = v; }
  vertices_iterator eraseVertex(vertices_iterator it) {
    *std::copy(it + 1, verticesEnd(), it) = -1;
    return it;
  }

  vertices_const_iterator verticesBegin() const { return m_v; }
  vertices_const_iterator verticesEnd() const { return m_v + verticesCount(); }
  vertices_iterator verticesBegin() { return m_v; }
  vertices_iterator verticesEnd() { return m_v + verticesCount(); }

  int face(int i) const { return m_f[i]; }
  int facesCount() const { return m_f[0] < 0 ? 0 : m_f[1] < 0 ? 1 : 2; }
  int otherFace(int f) const { return m_f[0] == f ? m_f[1] : m_f[0]; }

  void addFace(int f) {
    assert(facesCount() < 2);
    m_f[facesCount()] = f;
  }
  void setFace(int i, int f) { assert(i < facesCount()), m_f[i] = f; }
  faces_iterator eraseFace(faces_iterator it) {
    *std::copy(it + 1, facesEnd(), it) = -1;
    return it;
  }

  faces_const_iterator facesBegin() const { return m_f; }
  faces_const_iterator facesEnd() const { return m_f + facesCount(); }
  faces_iterator facesBegin() { return m_f; }
  faces_iterator facesEnd() { return m_f + facesCount(); }
};

}  // namespace tcg

#endif  // TCG_EDGE_H
