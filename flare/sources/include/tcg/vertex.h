#pragma once

#ifndef TCG_VERTEX_H
#define TCG_VERTEX_H

// tcg includes
#include "list.h"
#include "point.h"

namespace tcg {

template <typename V>
struct vertex_traits {
  typedef typename V::point_type point_type;

  typedef typename V::edges_const_iterator edges_const_iterator;
  typedef typename V::edges_iterator edges_iterator;
};

//----------------------------------------------------------------------

template <typename Point>
class Vertex {
protected:
  Point m_p;
  int m_index;
  tcg::list<int> m_edgeList;

public:
  typedef Point point_type;

  typedef tcg::list<int>::const_iterator edges_const_iterator;
  typedef tcg::list<int>::iterator edges_iterator;

public:
  Vertex() : m_index(-1) {}
  Vertex(const point_type &p) : m_p(p), m_index(-1) {}
  ~Vertex() {}

  void setIndex(int idx) { m_index = idx; }
  int getIndex() const { return m_index; }

  const tcg::list<int> &edges() const { return m_edgeList; }
  tcg::list<int> &edges() { return m_edgeList; }

  int edgesCount() const { return int(m_edgeList.size()); }
  int edge(int e) const { return int(m_edgeList[e]); }

  void addEdge(int e) { m_edgeList.push_back(e); }
  edges_iterator eraseEdge(edges_iterator it) { return m_edgeList.erase(it); }

  edges_const_iterator edgesBegin() const { return m_edgeList.begin(); }
  edges_const_iterator edgesEnd() const { return m_edgeList.end(); }
  edges_iterator edgesBegin() { return m_edgeList.begin(); }
  edges_iterator edgesEnd() { return m_edgeList.end(); }

  void setP(const point_type &p) { m_p = p; }
  const point_type &P() const { return m_p; }
  point_type &P() { return m_p; }

  bool operator==(const Vertex &v) const { return (m_p == v.m_p); }
  bool operator!=(const Vertex &v) const { return (m_p != v.m_p); }
};

//----------------------------------------------------------------------

template <typename P>
struct point_traits<Vertex<P>> {
  typedef Vertex<P> point_type;
  typedef typename point_traits<P>::value_type value_type;
  typedef typename point_traits<P>::float_type float_type;

  static inline value_type x(const point_type &p) { return p.P().x; }
  static inline value_type y(const point_type &p) { return p.P().y; }
};

}  // namespace tcg

#endif  // TCG_VERTEX_H
