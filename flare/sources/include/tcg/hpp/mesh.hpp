#pragma once

#ifndef TCG_MESH_HPP
#define TCG_MESH_HPP

// tcg includes
#include "../mesh.h"

namespace tcg {

//************************************************************************
//    Polygon Mesh methods
//************************************************************************

template <typename V, typename E, typename F>
int Mesh<V, E, F>::edgeInciding(int vIdx1, int vIdx2, int n) const {
  const V &v1 = vertex(vIdx1);

  const tcg::list<int> &incidingV1 = v1.edges();
  tcg::list<int>::const_iterator it;

  for (it = incidingV1.begin(); it != incidingV1.end(); ++it) {
    const E &e = edge(*it);
    if (e.otherVertex(vIdx1) == vIdx2 && n-- == 0) break;
  }

  return (it == incidingV1.end()) ? -1 : (*it);
}

//-------------------------------------------------------------------------

template <typename V, typename E, typename F>
int Mesh<V, E, F>::addEdge(const E &ed) {
  int e = int(m_edges.push_back(ed));
  m_edges[e].setIndex(e);

  // Add the edge index to the edge's vertices
  typename edge_traits<E>::vertices_const_iterator it, end(ed.verticesEnd());
  for (it = ed.verticesBegin(); it != end; ++it) m_vertices[*it].addEdge(e);

  return e;
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int Mesh<V, E, F>::addFace(const F &fc) {
  int f = int(m_faces.push_back(fc));
  m_faces[f].setIndex(f);

  // Add the face index to the face's edges
  typename face_traits<F>::edges_const_iterator it, end = fc.edgesEnd();
  for (it = fc.edgesBegin(); it != end; ++it) m_edges[*it].addFace(f);

  return f;
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
void Mesh<V, E, F>::removeVertex(int v) {
  V &vx = vertex(v);

  // As long as there are incident edges, remove them
  while (vx.edgesCount() > 0) removeEdge(vx.edges().front());

  m_vertices.erase(v);
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
void Mesh<V, E, F>::removeEdge(int e) {
  E &ed = edge(e);

  // Remove all the associated faces
  typename edge_traits<E>::faces_iterator ft;
  while ((ft = ed.facesBegin()) !=
         ed.facesEnd())  // current iterator could be erased here!
    removeFace(*ft);

  // Remove the edge from the associated vertices
  typename edge_traits<E>::vertices_iterator vt, vEnd = ed.verticesEnd();
  for (vt = ed.verticesBegin(); vt != vEnd; ++vt) {
    V &vx = vertex(*vt);
    typename vertex_traits<V>::edges_iterator et =
        std::find(vx.edgesBegin(), vx.edgesEnd(), e);

    assert(et != vx.edgesEnd());
    vx.eraseEdge(et);
  }

  m_edges.erase(e);
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
void Mesh<V, E, F>::removeFace(int f) {
  F &fc = face(f);

  // Remove the face from all adjacent edges
  typename face_traits<F>::edges_iterator et, eEnd = fc.edgesEnd();
  for (et = fc.edgesBegin(); et != eEnd; ++et) {
    E &ed = edge(*et);
    typename edge_traits<E>::faces_iterator ft =
        std::find(ed.facesBegin(), ed.facesEnd(), f);

    assert(ft != ed.facesEnd());
    ed.eraseFace(ft);
  }

  m_faces.erase(f);
}

//---------------------------------------------------------------------------------------------

/*!
  \brief    Remaps the mesh indices in a natural order, removing unused cells in
  the internal
            container model, for minimum memory consumption.

  \warning  This is a slow operation, compared to all the others in the Mesh
  class.
*/

template <typename V, typename E, typename F>
void Mesh<V, E, F>::squeeze() {
  // Build new indices for remapping.
  typename tcg::list<F>::iterator it, endI(m_faces.end());
  int i;
  for (i = 0, it = m_faces.begin(); it != endI; ++i, ++it) it->setIndex(i);

  typename tcg::list<E>::iterator jt, endJ(m_edges.end());
  for (i = 0, jt = m_edges.begin(); jt != endJ; ++i, ++jt) jt->setIndex(i);

  typename tcg::list<V>::iterator kt, endK(m_vertices.end());
  for (i = 0, kt = m_vertices.begin(); kt != endK; ++i, ++kt) kt->setIndex(i);

  // Update stored indices
  for (it = m_faces.begin(); it != endI; ++it) {
    F &face = *it;

    typename face_traits<F>::edges_iterator et, eEnd = face.edgesEnd();
    for (et = face.edgesBegin(); et != eEnd; ++et) *et = edge(*et).getIndex();
  }

  for (jt = m_edges.begin(); jt != endJ; ++jt) {
    E &edge = *jt;

    typename edge_traits<E>::vertices_iterator vt, vEnd = edge.verticesEnd();
    for (vt = edge.verticesBegin(); vt != vEnd; ++vt)
      *vt = vertex(*vt).getIndex();

    typename edge_traits<E>::faces_iterator ft, fEnd = edge.facesEnd();
    for (ft = edge.facesBegin(); ft != fEnd; ++ft) *ft = face(*ft).getIndex();
  }

  tcg::list<int>::iterator lt;
  for (kt = m_vertices.begin(); kt != endK; ++kt) {
    V &vertex = *kt;

    typename vertex_traits<V>::edges_iterator et, eEnd = vertex.edgesEnd();
    for (et = vertex.edgesBegin(); et != eEnd; ++et) *et = edge(*et).getIndex();
  }

  // Finally, rebuild the actual containers
  if (!m_faces.empty()) {
    tcg::list<F> temp(m_faces.begin(), m_faces.end());
    std::swap(m_faces, temp);
  }

  if (!m_edges.empty()) {
    tcg::list<E> temp(m_edges.begin(), m_edges.end());
    std::swap(m_edges, temp);
  }

  if (!m_vertices.empty()) {
    tcg::list<V> temp(m_vertices.begin(), m_vertices.end());
    std::swap(m_vertices, temp);
  }
}

//************************************************************************
//    Triangular Mesh methods
//************************************************************************

template <typename V, typename E, typename F>
TriMesh<V, E, F>::TriMesh(int verticesHint) {
  int edgesHint = (3 * verticesHint) / 2;

  m_vertices.reserve(verticesHint);
  m_edges.reserve(edgesHint);
  m_faces.reserve(edgesHint + 1);  // Since V - E + F = 1 for planar graphs (no
                                   // outer face), and vMin == 0
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::addFace(V &vx1, V &vx2, V &vx3) {
  int v1 = vx1.getIndex(), v2 = vx2.getIndex(), v3 = vx3.getIndex();

  // Retrieve the edges having v1, v2, v3 in common
  int e1, e2, e3;

  e1 = this->edgeInciding(v1, v2);
  e2 = this->edgeInciding(v2, v3);
  e3 = this->edgeInciding(v3, v1);

  if (e1 < 0) e1 = this->addEdge(E(v1, v2));
  if (e2 < 0) e2 = this->addEdge(E(v2, v3));
  if (e3 < 0) e3 = this->addEdge(E(v3, v1));

  F fc;
  fc.addEdge(e1), fc.addEdge(e2), fc.addEdge(e3);

  int f = int(m_faces.push_back(fc));

  m_faces[f].setIndex(f);

  E &E1 = this->edge(e1);
  E1.addFace(f);
  E &E2 = this->edge(e2);
  E2.addFace(f);
  E &E3 = this->edge(e3);
  E3.addFace(f);

  return f;
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::otherFaceVertex(int f, int e) const {
  const F &face = Mesh<V, E, F>::face(f);
  const E &otherEdge =
      face.edge(0) == e ? this->edge(face.edge(1)) : this->edge(face.edge(0));

  int v1 = this->edge(e).vertex(0), v2 = this->edge(e).vertex(1),
      v3 = otherEdge.otherVertex(v1);
  return (v3 == v2) ? otherEdge.otherVertex(v2) : v3;
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::otherFaceEdge(int f, int v) const {
  const F &face = Mesh<V, E, F>::face(f);

  {
    const E &ed = this->edge(face.edge(0));
    if (ed.vertex(0) != v && ed.vertex(1) != v) return face.edge(0);
  }

  {
    const E &ed = this->edge(face.edge(1));
    if (ed.vertex(0) != v && ed.vertex(1) != v) return face.edge(1);
  }

  return face.edge(2);
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::swapEdge(int e) {
  E &ed = this->edge(e);

  if (ed.facesCount() < 2) return -1;

  int f1 = ed.face(0), f2 = ed.face(1);

  // Retrieve the 2 vertices not belonging to e in the adjacent faces
  int v1 = ed.vertex(0), v2 = ed.vertex(1);
  int v3 = otherFaceVertex(f1, e), v4 = otherFaceVertex(f2, e);

  assert(this->edgeInciding(v3, v4) < 0);

  // Remove e
  this->removeEdge(e);

  // Insert the new faces
  addFace(v1, v3, v4);  // Inserts edge E(v3, v4)
  addFace(v2, v4, v3);

  return this->edgeInciding(v3, v4);
}

//---------------------------------------------------------------------------------------------

/*

    *---*---*     Common case          *          FORBIDDEN case:
   / \ / x / \                        /|\           note that the collapsed edge
  *---*-x-X---*                      /_*_\          have 3 (possibly more) other
  vertices
   \ / \ x \ /                      *--X--*         with edges inciding both the
  collapsed
    *---*---*                        \   /          edge's extremes.
                                      \ /
                                       *            This cannot be processed,
  since the
                                                    unexpected merged edge would
  either have
                                                    more than 2 adjacent faces,
  or a hole.
*/

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::collapseEdge(int e) {
  E &ed = this->edge(e);

  // First, retrieve ed's adjacent vertices
  int vKeep = ed.vertex(0), vDelete = ed.vertex(1);
  V &vxKeep = this->vertex(vKeep), &vxDelete = this->vertex(vDelete);

  // Then, retrieve the 2 vertices not belonging to e in the adjacent faces
  int f, fCount = ed.facesCount();
  int otherV[2];

  for (f = 0; f != fCount; ++f) otherV[f] = otherFaceVertex(ed.face(f), e);

  // Remove e
  this->removeEdge(e);

  // Merge edges inciding vDelete and otherV with the corresponding inciding
  // vKeep and otherV
  for (f = 0; f != fCount; ++f) {
    int srcE = this->edgeInciding(vDelete, otherV[f]),
        dstE = this->edgeInciding(vKeep, otherV[f]);

    E &srcEd = this->edge(srcE), &dstEd = this->edge(dstE);

    typename edge_traits<E>::faces_iterator ft = srcEd.facesBegin();
    while (ft != srcEd.facesEnd())  // current iterator will be erased
    {
      F &fc = this->face(*ft);

      (fc.edge(0) == srcE)   ? fc.setEdge(0, dstE)
      : (fc.edge(1) == srcE) ? fc.setEdge(1, dstE)
                             : fc.setEdge(2, dstE);

      dstEd.addFace(*ft);
      ft = srcEd.eraseFace(ft);  // here
    }

    this->removeEdge(srcE);
  }

  // Move further edges adjacent to vDelete to vKeep
  typename vertex_traits<V>::edges_iterator et = vxDelete.edgesBegin();
  while (et != vxDelete.edgesEnd())  // current iterator will be erased
  {
    E &ed = this->edge(*et);

// Ensure that there is no remaining edge which would be duplicated
// after vDelete and vKeep merge
/* FIXME: Omitted for now because it is warned that there is no edgeInciding */
#if 0
    assert("Detected vertex adjacent to collapsed edge's endpoints, but not to its faces." &&
           edgeInciding(ed.otherVertex(vDelete), vKeep) < 0);
#endif
    (ed.vertex(0) == vDelete) ? ed.setVertex(0, vKeep) : ed.setVertex(1, vKeep);

    vxKeep.addEdge(*et);
    et = vxDelete.eraseEdge(et);  // here
  }

  // Finally, update vKeep's position and remove vDelete
  vxKeep.P() = 0.5 * (vxKeep.P() + vxDelete.P());
  m_vertices.erase(vDelete);

  return vKeep;
}

//---------------------------------------------------------------------------------------------

template <typename V, typename E, typename F>
int TriMesh<V, E, F>::splitEdge(int e) {
  E &ed = this->edge(e);

  // Build a new vertex on the middle of e
  int v1 = ed.vertex(0), v2 = ed.vertex(1);

  V &vx1 = this->vertex(v1), &vx2 = this->vertex(v2);
  V v(0.5 * (vx1.P() + vx2.P()));

  int vIdx = this->addVertex(v);

  // Retrieve opposite vertices
  int otherV[2];  // NOTE: If ever extended to support edges with
  int f,
      fCount =
          ed.facesCount();  //       MORE than 2 adjacent faces, the new faces
  //       should be inserted BEFORE removing the split
  for (f = 0; f != fCount; ++f)  //       edge.
    otherV[f] = otherFaceVertex(ed.face(f), e);

  // Remove e
  this->removeEdge(e);

  // Add the new edges
  this->addEdge(E(v1, vIdx));
  this->addEdge(E(vIdx, v2));

  // Add the new faces
  for (f = 0; f != fCount; ++f)
    addFace(v1, vIdx, otherV[f]), addFace(vIdx, v2, otherV[f]);

  return vIdx;
}

}  // namespace tcg

#endif  // TCG_MESH_HPP
