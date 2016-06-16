

#ifndef TCG_TRIANGULATE_H
#define TCG_TRIANGULATE_H

// tcg includes
#include "tcg_mesh.h"

namespace tcg {

namespace TriMeshStuff {
/*!
    \brief    Traits class providing useful accessor to vertex data
              in a form compatible with the GLU tessellator.

    \details  Vertex types usable with the GLU tessellator must have:
                \li A vertex position member, in the form of a
                    <TT>double[3]</TT> array containing its <TT>(x,y,z)</TT>
                    coordinates.
                \li An index member to identify the vertex.
  */

template <typename vertex_type>
struct glu_vertex_traits {
  static inline double (&vertex3d(vertex_type &vx))[3] { return vx.m_pos; }

  static inline int &index(vertex_type &vx) { return vx.m_idx; }
};

//------------------------------------------------------------------------------------

template <typename TriMesh_type>
struct ActionEvaluator {
  enum Action { NONE, SWAP, COLLAPSE, SPLIT };

  /*!
Fill in the 3-array actionSequence with the actions you would want performed
first on edge e of given mesh. Provided the actions are feasible, they will be
performed in the returned order.
*/
  virtual void actionSort(const TriMesh_type &mesh, int e,
                          Action *actionSequence) = 0;
};

//------------------------------------------------------------------------------------

template <typename TriMesh_type>
struct DefaultEvaluator : public ActionEvaluator<TriMesh_type> {
  double m_collapseValue;
  double m_splitValue;

  /*!
Sorts actions to achieve near-targetLength edge lengths.
Split and collapse actions follow the rules:

  <li> Collapse if  length < collapseValue <\li>
  <li> Split if     length > splitValue <\li>

\warning The simplification procedure may loop on certain configurations where
collapses and splits alternate repeatedly.
*/
  DefaultEvaluator(double collapseValue, double splitValue)
      : m_collapseValue(collapseValue), m_splitValue(splitValue) {}

  void actionSort(
      const TriMesh_type &mesh, int e,
      typename ActionEvaluator<TriMesh_type>::Action *actionSequence);
};

}  // namespace TriMeshStuff

//==============================================================================================

//! Uses the glu tessellator to build a triangular mesh from a specified tribe
//! of polygons.
/*!
  The algorithm accepts a tribe (that is, a container of containers, where each
  container class stores
  <I> pointer type objects <\I> to sub-containers) of polygons, and reads out a
  sequence of meshes with
  triangular faces.

  Each family of the tribe should contain a list of polygons representing a
  connected component
  to triangulate, where its first element is the external border, and the
  following ones are the internals.

  The output meshes are the tessellation of each family in the tribe.
*/
template <typename ForIt, typename ContainersReader>
void gluTriangulate(ForIt polygonsTribeBegin, ForIt polygonsTribeEnd,
                    ContainersReader &meshes_reader);

template <typename TriMesh_type>
void refineMesh(
    TriMesh_type &mesh, TriMeshStuff::ActionEvaluator<TriMesh_type> &eval,
    unsigned long maxActions = (std::numeric_limits<unsigned long>::max)());

}  // namespace tcg

#endif  // TCG_TRIANGULATE_H

#ifdef INCLUDE_HPP
#include "hpp/triangulate.hpp"
#endif
