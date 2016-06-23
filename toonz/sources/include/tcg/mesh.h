// #pragma once // could not use by INCLUDE_HPP

#ifndef MESH_H
#define MESH_H

// tcg includes
#include "list.h"

namespace tcg {

//********************************************************************************
//    Polygon Mesh template class
//********************************************************************************

/*
  \brief    The mesh class models entities composed of vertices, edges and face
            using an index-based random access approach.

  \details  This mesh implementation uses 3 separate tcg::list to provide the
            fundamental index-based access to components, one per component
  type.

            Said component containers are an explicit requirement of the mesh
            class, and direct access is therefore provided. However, use of
            (mutating) direct accessors should be restricted to special cases,
            since direct components manipulation is \a nontrivial - use the
            appropriate \p add and \p remove methods to manipulate single
            components.

  \sa       Classes tcg::Vertex, tcg::Edge and tcg::Face  for
  tcg::Mesh-compatible
            VEF models.
*/

template <typename V, typename E, typename F>
class Mesh {
public:
  typedef V vertex_type;
  typedef E edge_type;
  typedef F face_type;

  typedef list<V> vertices_container;
  typedef list<E> edges_container;
  typedef list<F> faces_container;

protected:
  vertices_container m_vertices;
  edges_container m_edges;
  faces_container m_faces;

public:
  Mesh() {}
  ~Mesh() {}

  bool empty() const { return m_vertices.empty(); }
  void clear() {
    m_vertices.clear();
    m_edges.clear();
    m_faces.clear();
  }

  int verticesCount() const { return int(m_vertices.size()); }
  int edgesCount() const { return int(m_edges.size()); }
  int facesCount() const { return int(m_faces.size()); }

  const vertices_container &vertices() const { return m_vertices; }
  vertices_container &vertices() { return m_vertices; }

  const edges_container &edges() const { return m_edges; }
  edges_container &edges() { return m_edges; }

  const faces_container &faces() const { return m_faces; }
  faces_container &faces() { return m_faces; }

  int addVertex(const V &v) {
    int idx = int(m_vertices.push_back(v));
    m_vertices[idx].setIndex(idx);
    return idx;
  }
  int addEdge(const E &e);
  int addFace(const F &f);

  void removeVertex(int v);  //!< Removes the <TT>v</TT>-th vertex from the
                             //! mesh.  \warning  Any adjacent edge or face will
  //! be removed, too.  \param v  Index of the vertex
  //! to be removed.
  void removeEdge(int e);  //!< Removes the <TT>e</TT>-th edge from the mesh.
                           //!\warning  Any adjacent face will be removed, too.
                           //!\param e  Index of the edge to be removed.
  void removeFace(int f);  //!< Removes the <TT>f</TT>-th face from the mesh.
                           //!\param f  Index of the face to be removed.

  const V &vertex(int v) const { return m_vertices[v]; }
  V &vertex(int v) {
    return m_vertices[v];
  }  //!< Returns the <TT>v</TT>-th mesh vertex.   \param v  Index of the vertex
     //! to be returned.  \return  See description.

  const E &edge(int e) const { return m_edges[e]; }
  E &edge(int e) {
    return m_edges[e];
  }  //!< Returns the <TT>e</TT>-th mesh edge.   \param e  Index of the edge to
     //! be returned.  \return  See description.

  const F &face(int f) const { return m_faces[f]; }
  F &face(int f) {
    return m_faces[f];
  }  //!< Returns the <TT>f</TT>-th mesh face.   \param f  Index of the face to
     //! be returned.  \return  See description.

  const V &edgeVertex(int e, int i) const { return vertex(edge(e).vertex(i)); }
  V &edgeVertex(int e, int i)  //!  \param e  Host edge index.  \param i  Vertex
                               //!  index in e.  \return See description.
  {
    return vertex(edge(e).vertex(i));
  }  //!< Returns the <TT>i</TT>-th vertex in the edge of index \p e.

  const F &edgeFace(int e, int i) const { return face(edge(e).face(i)); }
  F &edgeFace(int e, int i)  //!  \param e  Host edge index.  \param i  Face
                             //!  index in e.  \return See description.
  {
    return face(edge(e).face(i));
  }  //!< Returns the <TT>i</TT>-th face in the edge of index \p e.

  const V &otherEdgeVertex(int e, int v) const {
    return vertex(edge(e).otherVertex(v));
  }
  V &otherEdgeVertex(int e, int v)  //!  \param e  Host edge index.  \param v
                                    //!  Index of the adjacent vertex to \p e
                                    //!  we're not interested in.  \return See
                                    //!  description.
  {
    return vertex(edge(e).otherVertex(v));
  }  //!< Retrieves the vertex adjacent to \p e whose index is \a not \p v.

  const F &otherEdgeFace(int e, int f) const {
    return face(edge(e).otherFace(f));
  }
  F &otherEdgeFace(int e, int f)  //!  \param e  Host edge index.  \param f
                                  //!  Index of the adjacent face to \p e we're
                                  //!  not interested in.  \return See
                                  //!  description.
  {
    return face(edge(e).otherFace(f));
  }  //!< Retrieves the face adjacent to \p e whose index is \a not \p f.

  /*!
\remark   Index \p n is arbitrary. Use it to traverse all edges inciding \p v1
and \p v2:
          \code for(int n=0; mesh.edgeInciding(v1, v2, n) > 0; ++n) ... \endcode
*/
  int edgeInciding(int v1, int v2, int n = 0) const;  //!< \brief Returns the
                                                      //! edge index of the
  //!<TT>n</TT>-th edge
  //! inciding
  //!  \p v1 and \p v2, or \p -1 if the required edge could not be found.
  //!  \param v1  First edge endpoint.  \param v2  Second edge endpoint.  \param
  //!  n  Index in the sequence of all edges inciding \p v1 and \p v2.  \return
  //!  See description.

  //! \remark  All indices and iterators will be \a invalidated.
  void
  squeeze();  //!< \brief Eliminates unused list nodes in the representation of
              //!  vertices, edges and faces.
};

//********************************************************************************
//    Triangular Mesh template class
//********************************************************************************

template <typename V, typename E, typename F>
class TriMesh : public Mesh<V, E, F> {
protected:
  using Mesh<V, E, F>::m_vertices;
  using Mesh<V, E, F>::m_edges;
  using Mesh<V, E, F>::m_faces;

public:
  TriMesh() {}
  TriMesh(int verticesHint);
  ~TriMesh() {}

  int addFace(V &v1, V &v2, V &v3);
  int addFace(int v1, int v2, int v3) {
    return addFace(Mesh<V, E, F>::vertex(v1), Mesh<V, E, F>::vertex(v2),
                   Mesh<V, E, F>::vertex(v3));
  }

  int otherFaceVertex(int f, int e) const;
  int otherFaceVertex(int f, int v1, int v2) const {
    return otherFaceVertex(f, Mesh<V, E, F>::edgeInciding(v1, v2));
  }

  int otherFaceEdge(int f, int v) const;

  void faceVertices(int f, int &v1, int &v2, int &v3) const {
    const E &ed = Mesh<V, E, F>::edge(Mesh<V, E, F>::face(f).edge(0));
    v1          = ed.vertex(0);
    v2          = ed.vertex(1);
    v3          = otherFaceVertex(f, ed.getIndex());
  }

  /*!
\details    This function selects an edge with \a two adjacent faces, and swaps
          its endpoints with their otherFaceVertex().

\remark     This function is idempotent - swapEdge(swapEdge(e)) has no effect
          on the mesh (assuming swapEdge(e) is not \p -1). In particular,
indices
          should remain the same.

\note       In current implementation, the result is the <I>very same</I> input
          edge index.

\return     The swapped edge, or \p -1 if the supplied edge did not have \a two
          adjacent faces.
*/
  int swapEdge(
      int e);  //!< Swaps the specified edge in the \a two adjacent faces.

  /*!
\details    Specifically, this function removes <TT>edgeVertex(e, 1)</TT> and
redirects its edges to
          <TT>edgeVertex(e, 0)</TT>. One edge per adjacent face (other than \p
e) will be removed
          (which can be thought as 'merged' with the remaining one), and each
adjacent face will be
          removed.

\note       This function removes <I>at most</I> 1 vertex, 3 edges and 2 faces,
total.

\warning    The user is respondible for ensuring that every vertex adjacent to
\a both the
          collapsed edge's endpoints is \a also adjacent to one of the edge's \a
faces.
          If not, the output configuration would be ill-formed. This function
will
          \a assert in this case, and result in <B>undefined behavior</B>.

\return     The remaining vertex index - specifically, <TT>edgeVertex(e,
0)</TT>.
*/
  int collapseEdge(int e);  //!< Collapses the specified edge.

  /*!
\details    This function inserts a new vertex at the midpoint of the specified
edge, and
          splits any adjacent face in two.

\return     The inserted vertex index.
*/
  int splitEdge(int e);  //!< \brief Splits the specified edge, inserting a new
                         //!  vertex at the middle.
};

}  // namespace tcg

#endif  // MESH_H

//----------------------------------------------------------------------------------------

#ifdef INCLUDE_HPP
#include "hpp/mesh.hpp"
#endif
