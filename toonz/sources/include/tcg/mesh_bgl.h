#pragma once

#ifndef TCG_MESH_BGL_H
#define TCG_MESH_BGL_H

// TCG includes
#include "mesh.h"

// Boost includes
#include <boost/graph/graph_traits.hpp>

/*
  \file     tcg_mesh_bgl.h

  \brief    This file contains TCG adapters to Boost's Graph Library (BGL).
*/

//************************************************************************************
//    boost::graph_traits to tcg::Mesh concept type
//************************************************************************************

namespace boost {

template <typename V, typename E, typename F>
struct graph_traits<typename tcg::Mesh<V, E, F>> {
  // Preliminar typedefs
  typedef typename tcg::Mesh<V, E, F> mesh_type;
  typedef typename mesh_type::vertex_type vertex_type;
  typedef typename mesh_type::edge_type edge_type;

  typedef typename vertex_type::edges_const_iterator edge_const_iterator;

  // V/E descriptors
  typedef int vertex_descriptor;

  struct edge_descriptor  // NOTE: Can't use std::pair due to std conflicts
  {
    int m_e, m_src;

    edge_descriptor() : m_e(-1), m_src(-1) {}
    edge_descriptor(int e, int src) : m_e(e), m_src(src) {}

    bool operator==(const edge_descriptor &e_d) {
      return m_e == e_d.m_e;
    }  // Undirected
    bool operator!=(const edge_descriptor &e_d) { return !operator==(e_d); }
  };

  // Iterators
  struct out_edge_iterator : public edge_const_iterator {
    int m_src;

    out_edge_iterator() : edge_const_iterator(), m_src(-1) {}
    out_edge_iterator(const edge_const_iterator &it, int src)
        : edge_const_iterator(it), m_src(src) {}

    edge_descriptor operator*() const {
      return edge_descriptor(edge_const_iterator::operator*(), m_src);
    }
  };

  typedef typename tcg::list<vertex_type>::const_iterator vertex_iterator;
  typedef typename tcg::list<edge_type>::const_iterator edge_iterator;

  // Categories
  typedef undirected_tag directed_category;
  typedef allow_parallel_edge_tag edge_parallel_category;

  typedef struct our_category : public bidirectional_graph_tag,
                                public vertex_list_graph_tag,
                                public edge_list_graph_tag {
  } traversal_category;

  // Sizes
  typedef int vertices_size_type;
  typedef int edges_size_type;
  typedef int degree_size_type;

  // Functions
  static inline vertex_descriptor null_vertex() { return -1; }
};

template <typename V, typename E, typename F>
struct graph_traits<typename tcg::TriMesh<V, E, F>>
    : public graph_traits<typename tcg::Mesh<V, E, F>> {};

}  // namespace boost

//************************************************************************************
//    tcg::bgl  helpers to edge data
//************************************************************************************

namespace tcg {
namespace bgl {

template <typename Mesh>
inline int source(
    const typename boost::graph_traits<Mesh>::edge_descriptor &edge_descr,
    const Mesh &mesh) {
  return edge_descr.m_src;
}

//---------------------------------------------------------------------------------------

template <typename Mesh>
inline int target(
    const typename boost::graph_traits<Mesh>::edge_descriptor &edge_descr,
    const Mesh &mesh) {
  const typename Mesh::edge_type &ed = mesh.edge(edge_descr.m_e);
  return (ed.vertex(0) == edge_descr.m_src) ? ed.vertex(1) : ed.vertex(0);
}

//---------------------------------------------------------------------------------------

template <typename Mesh>
inline std::pair<typename boost::graph_traits<Mesh>::out_edge_iterator,
                 typename boost::graph_traits<Mesh>::out_edge_iterator>
out_edges(int v, const Mesh &mesh) {
  typedef
      typename boost::graph_traits<Mesh>::out_edge_iterator out_edge_iterator;

  return make_pair(out_edge_iterator(mesh.vertex(v).edgesBegin(), v),
                   out_edge_iterator(mesh.vertex(v).edgesEnd(), v));
}

//---------------------------------------------------------------------------------------

template <typename Mesh>
inline int out_degree(int v, const Mesh &mesh) {
  return mesh.vertex(v).edgesCount();
}
}
}  // namespace tcg::bgl

#endif  // TCG_MESH_BGL_H
