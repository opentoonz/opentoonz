#pragma once

#ifndef TCG_TRIANGULATE_HPP
#define TCG_TRIANGULATE_HPP

// tcg includes
#include "../triangulate.h"
#include "../traits.h"
#include "../point_ops.h"

// OS-specific includes
#if defined(_WIN32)
#include "windows.h"
#include <GL/glu.h>
#elif defined(MACOSX)
#include <GLUT/glut.h>
#elif defined(LINUX) || defined(FREEBSD)
#include <GL/glut.h>
#include <cstring>
#endif

#ifndef TCG_GLU_CALLBACK
#ifdef WIN32
#define TCG_GLU_CALLBACK void(CALLBACK *)()
#else
#define TCG_GLU_CALLBACK void (*)()
#endif
#endif

#ifndef CALLBACK
#define CALLBACK
#endif

namespace tcg {

//**************************************************************************************
//    GLU Tessellator Callbacks
//**************************************************************************************

namespace detail {

template <typename mesh_type>
struct CBackData {
  mesh_type *m_mesh;
  int m_triangle[3];
  int m_i;
};

//============================================================================

// NOTE: must be declared with CALLBACK directive
template <typename mesh_type>
void CALLBACK tessBegin(GLenum type, void *polygon_data) {
  assert(type == GL_TRIANGLES);

  CBackData<mesh_type> *data = (CBackData<mesh_type> *)polygon_data;
  data->m_i                  = 0;
}

//----------------------------------------------------------------------------

template <typename mesh_type>
void CALLBACK tessEnd(void *polygon_data) {
  CBackData<mesh_type> *data = (CBackData<mesh_type> *)polygon_data;
  assert(data->m_i == 0);
}

//----------------------------------------------------------------------------

template <typename mesh_type, typename vertex_type>
void CALLBACK tessVertex(void *vertex_data, void *polygon_data) {
  typedef typename mesh_type::vertex_type::point_type point_type;

  CBackData<mesh_type> *data = (CBackData<mesh_type> *)polygon_data;
  vertex_type *vData         = (vertex_type *)vertex_data;

  GLdouble(&pos)[3] =
      TriMeshStuff::glu_vertex_traits<vertex_type>::vertex3d(*vData);
  int &idx = TriMeshStuff::glu_vertex_traits<vertex_type>::index(*vData);

  if (idx < 0) {
    data->m_mesh->addVertex(
        typename mesh_type::vertex_type(point_type(pos[0], pos[1])));
    idx = data->m_mesh->verticesCount() - 1;
  }

  data->m_triangle[data->m_i] = idx;
  data->m_i                   = (data->m_i + 1) % 3;

  if (data->m_i == 0)
    data->m_mesh->addFace(data->m_triangle[0], data->m_triangle[1],
                          data->m_triangle[2]);
}

//----------------------------------------------------------------------------

// Supplied to ensure that triangle primitives are always of type GL_TRIANGLE
template <typename mesh_type>
void CALLBACK edgeFlag(GLboolean flag) {}

}  // namespace tcg::detail

//**************************************************************************************
//    Polygon triangulation
//**************************************************************************************

namespace detail {

template <typename Func>
void gluRegister(GLUtesselator *tess, GLenum which, Func *func) {
  gluTessCallback(tess, which, (TCG_GLU_CALLBACK)func);
}

}  // namespace tcg::detail

//---------------------------------------------------------------------------

template <typename ForIt, typename ContainersReader>
void gluTriangulate(ForIt tribeBegin, ForIt tribeEnd,
                    ContainersReader &meshes_reader) {
  using namespace detail;

  typedef typename tcg::traits<typename ForIt::value_type>::pointed_type
      family_type;
  typedef typename tcg::traits<typename family_type::value_type>::pointed_type
      polygon_type;
  typedef typename polygon_type::value_type vertex_type;

  typedef typename tcg::container_reader_traits<ContainersReader> output;
  typedef
      typename tcg::traits<typename output::value_type>::pointed_type mesh_type;

  GLUtesselator *tess = gluNewTess();  // create a tessellator
  assert(tess);

  // Declare callbacks

  // NOTE: On g++, it seems that at this point each of the callbacks still have
  // undefined type.
  // We use the above gluRegister template to force the compiler to generate
  // these types.

  gluRegister(tess, GLU_TESS_BEGIN_DATA, tessBegin<mesh_type>);
  gluRegister(tess, GLU_TESS_END_DATA, tessEnd<mesh_type>);
  gluRegister(tess, GLU_TESS_VERTEX_DATA, tessVertex<mesh_type, vertex_type>);
  gluRegister(tess, GLU_TESS_EDGE_FLAG, edgeFlag<mesh_type>);

  output::openContainer(meshes_reader);

  // Iterate the tribe. For every polygons family, associate one output mesh
  for (ForIt it = tribeBegin; it != tribeEnd; ++it) {
    // Build the output mesh and initialize stuff
    mesh_type *mesh = new mesh_type;

    CBackData<mesh_type> cbData;  // Callback Data about the mesh
    cbData.m_mesh = mesh;

    // Tessellate the family
    gluTessBeginPolygon(tess, (void *)&cbData);

    typename family_type::iterator ft, fEnd = (*it)->end();
    for (ft = (*it)->begin(); ft != fEnd; ++ft) {
      gluTessBeginContour(tess);

      typename polygon_type::iterator pt, pEnd = (*ft)->end();
      for (pt = (*ft)->begin(); pt != pEnd; ++pt)
        gluTessVertex(
            tess, TriMeshStuff::glu_vertex_traits<vertex_type>::vertex3d(*pt),
            (void *)&*pt);

      gluTessEndContour(tess);
    }

    gluTessEndPolygon(tess);  // Invokes the family tessellation

    output::addElement(meshes_reader, mesh);
  }

  gluDeleteTess(tess);  // delete after tessellation

  output::closeContainer(meshes_reader);
}

//**************************************************************************************
//    Mesh refinement
//**************************************************************************************

namespace detail {

template <typename mesh_type>
inline void touchEdge(std::vector<UCHAR> &buildEdge, mesh_type &mesh, int e) {
  typename mesh_type::edge_type &ed = mesh.edge(e);
  int f1 = ed.face(0), f2 = ed.face(1);

  if (f1 >= 0) {
    typename mesh_type::face_type &f = mesh.face(f1);
    buildEdge[f.edge(0)] = 1, buildEdge[f.edge(1)] = 1,
    buildEdge[f.edge(2)] = 1;
  }
  if (f2 >= 0) {
    typename mesh_type::face_type &f = mesh.face(f2);
    buildEdge[f.edge(0)] = 1, buildEdge[f.edge(1)] = 1,
    buildEdge[f.edge(2)] = 1;
  }
}

//---------------------------------------------------------------------------

template <typename mesh_type>
inline void touchVertex(std::vector<UCHAR> &buildEdge, mesh_type &mesh, int v) {
  // Sign all adjacent edges and adjacent faces' edges to v
  typename mesh_type::vertex_type &vx = mesh.vertex(v);
  const tcg::list<int> &incidentEdges = vx.edges();
  tcg::list<int>::const_iterator it;

  for (it = incidentEdges.begin(); it != incidentEdges.end(); ++it)
    touchEdge(buildEdge, mesh, *it);
}

//================================================================================

template <typename mesh_type>
class BoundaryEdges {
  std::vector<UCHAR> m_boundaryVertices;
  const mesh_type &m_mesh;

public:
  BoundaryEdges(const mesh_type &mesh) : m_mesh(mesh) {
    const tcg::list<typename mesh_type::edge_type> &edges = mesh.edges();
    const tcg::list<typename mesh_type::vertex_type> &vertices =
        mesh.vertices();

    m_boundaryVertices.resize(vertices.nodesCount(), 0);

    typename tcg::list<typename mesh_type::edge_type>::const_iterator it;
    for (it = edges.begin(); it != edges.end(); ++it) {
      if (it->face(0) < 0 || it->face(1) < 0) {
        m_boundaryVertices[it->vertex(0)] = true;
        m_boundaryVertices[it->vertex(1)] = true;
      }
    }
  }
  ~BoundaryEdges() {}

  void setBoundaryVertex(int v) {
    m_boundaryVertices.resize(m_mesh.vertices().nodesCount(), 0);
    m_boundaryVertices[v] = 1;
  }

  bool isBoundaryVertex(int v) const {
    return v < int(m_boundaryVertices.size()) && m_boundaryVertices[v];
  }
  bool isBoundaryEdge(int e) const {
    const typename mesh_type::edge_type &ed = m_mesh.edge(e);
    return ed.face(0) < 0 || ed.face(1) < 0;
  }
};

}  // namespace tcg::detail

//=============================================================================

template <typename mesh_type>
void TriMeshStuff::DefaultEvaluator<mesh_type>::actionSort(
    const mesh_type &mesh, int e,
    typename ActionEvaluator<mesh_type>::Action *actionSequence) {
  typedef ActionEvaluator<mesh_type> ActionEvaluator;

  int count = 0;
  memset(actionSequence, ActionEvaluator::NONE,
         3 * sizeof(typename ActionEvaluator::Action));

  // Try to minimize the edge length deviation in e's neighbourhood
  const typename mesh_type::edge_type &ed = mesh.edge(e);
  int f1 = ed.face(0), f2 = ed.face(1);
  const TPointD *v1, *v2, *v3, *v4;
  double length[6];

  v1 = &mesh.vertex(ed.vertex(0)).P();
  v2 = &mesh.vertex(ed.vertex(1)).P();

  length[0]        = norm(*v2 - *v1);
  double lengthMax = length[0], lengthMin = length[0];

  if (f1 >= 0) {
    v3        = &mesh.vertex(mesh.otherFaceVertex(f1, e)).P();
    length[1] = norm(*v3 - *v1);
    length[2] = norm(*v3 - *v2);
    lengthMax = std::max({lengthMax, length[1], length[2]});
    lengthMin = std::min({lengthMin, length[1], length[2]});
  }

  if (f2 >= 0) {
    v4        = &mesh.vertex(mesh.otherFaceVertex(f2, e)).P();
    length[3] = norm(*v4 - *v1);
    length[4] = norm(*v4 - *v2);
    lengthMax = std::max({lengthMax, length[3], length[4]});
    lengthMin = std::min({lengthMin, length[3], length[4]});
  }

  if (f1 >= 0 && f2 >= 0) {
    // Build the edge lengths
    length[5] = norm(*v4 - *v3);

    // Evaluate swap - take the triangles with least maximum mean boundary edge
    double m1 = (length[0] + length[1] + length[2]) / 3.0;
    double m2 = (length[0] + length[3] + length[4]) / 3.0;
    double m3 = (length[5] + length[1] + length[3]) / 3.0;
    double m4 = (length[5] + length[2] + length[4]) / 3.0;

    if (std::max(m3, m4) < std::max(m1, m2) - 1e-5)
      actionSequence[count++] = ActionEvaluator::SWAP;

    // NOTE: The original swap evaluation was about maximizing the minimal face
    // angle.
    // However, this requires quite some cross products - the above test is
    // sufficiently
    // simple and has a similar behaviour.

    // Evaluate collapse
    if (length[0] < m_collapseValue)
      actionSequence[count++] = ActionEvaluator::COLLAPSE;
  }

  // Evaluate split
  if (length[0] > m_splitValue)
    actionSequence[count++] = ActionEvaluator::SPLIT;
}

//=============================================================================

namespace detail {

template <typename mesh_type>
inline bool testSwap(const mesh_type &mesh, int e) {
  // Retrieve adjacent faces
  const typename mesh_type::edge_type &ed = mesh.edge(e);

  int f1 = ed.face(0), f2 = ed.face(1);
  if (f1 < 0 || f2 < 0) return false;

  // Retrieve the 4 adjacent vertices
  const typename mesh_type::vertex_type &v1 = mesh.vertex(ed.vertex(0));
  const typename mesh_type::vertex_type &v2 = mesh.vertex(ed.vertex(1));
  const typename mesh_type::vertex_type &v3 =
      mesh.vertex(mesh.otherFaceVertex(f1, ed.getIndex()));
  const typename mesh_type::vertex_type &v4 =
      mesh.vertex(mesh.otherFaceVertex(f2, ed.getIndex()));

  // Make sure that vertex v4 lies between the semiplane generated by v3v1 and
  // v3v2
  TPointD a(v1.P() - v3.P()), b(v2.P() - v3.P());

  double normA = norm(a), normB = norm(b);
  if (normA < 1e-5 || normB < 1e-5) return false;

  a = a * (1.0 / normA);
  b = b * (1.0 / normB);

  TPointD c(v4.P() - v1.P()), d(v4.P() - v2.P());

  double normC = norm(c), normD = norm(d);
  if (normC < 1e-5 || normD < 1e-5) return false;

  c = c * (1.0 / normC);
  d = d * (1.0 / normD);

  double crossAC = point_ops::cross(a, c);
  int signAC     = crossAC < -1e-5 ? -1 : crossAC > 1e-5 ? 1 : 0;
  double crossBD = point_ops::cross(b, d);
  int signBD     = crossBD < -1e-5 ? -1 : crossBD > 1e-5 ? 1 : 0;

  return signAC == -signBD;
}

//----------------------------------------------------------------------------

// Tests edge e for admissibility of ad edge collapse. Edge e must not have
// adjacent
// faces with boundary components.
// Furthermore, we must test that faces adjacent to f1 and f2 keep e on the same
// side of
// the line passing through v3v1 and v3v2.
template <typename mesh_type>
inline bool testCollapse(const mesh_type &mesh, int e,
                         const BoundaryEdges<mesh_type> &boundary) {
  // Any face adjacent to e must have no boundary edge
  const typename mesh_type::edge_type &ed = mesh.edge(e);
  int f1 = ed.face(0), f2 = ed.face(1);

  if (f1 < 0 || f2 < 0) return false;

  int v1 = mesh.edge(e).vertex(0), v2 = mesh.edge(e).vertex(1);
  if (boundary.isBoundaryVertex(v1) || boundary.isBoundaryVertex(v2))
    return false;

  // Test faces adjacent to v1 or v2. Since one of their vertices will change,
  // we must make sure that their
  //'side' does not change too.
  int v = mesh.otherFaceVertex(f1, e);
  int l = mesh.edgeInciding(v1, v);
  int f =
      mesh.edge(l).face(0) == f1 ? mesh.edge(l).face(1) : mesh.edge(l).face(0);
  int vNext = mesh.otherFaceVertex(f, l);

  while (f != f2) {
    // Test face f
    if (tsign(point_ops::cross(mesh.vertex(vNext).P() - mesh.vertex(v).P(),
                               mesh.vertex(v2).P() - mesh.vertex(v).P())) !=
        tsign(point_ops::cross(mesh.vertex(vNext).P() - mesh.vertex(v).P(),
                               mesh.vertex(v1).P() - mesh.vertex(v).P())))
      return false;

    // Update vars
    v = vNext;
    l = mesh.edgeInciding(v1, v);
    f = mesh.edge(l).face(0) == f ? mesh.edge(l).face(1) : mesh.edge(l).face(0);
    vNext = mesh.otherFaceVertex(f, l);
  }

  // Same with respect to v2
  v = mesh.otherFaceVertex(f1, e);
  l = mesh.edgeInciding(v2, v);
  f = mesh.edge(l).face(0) == f1 ? mesh.edge(l).face(1) : mesh.edge(l).face(0);
  vNext = mesh.otherFaceVertex(f, l);

  while (f != f2) {
    // Test face f
    if (tsign(point_ops::cross(mesh.vertex(vNext).P() - mesh.vertex(v).P(),
                               mesh.vertex(v2).P() - mesh.vertex(v).P())) !=
        tsign(point_ops::cross(mesh.vertex(vNext).P() - mesh.vertex(v).P(),
                               mesh.vertex(v1).P() - mesh.vertex(v).P())))
      return false;

    // Update vars
    v = vNext;
    l = mesh.edgeInciding(v2, v);
    f = mesh.edge(l).face(0) == f ? mesh.edge(l).face(1) : mesh.edge(l).face(0);
    vNext = mesh.otherFaceVertex(f, l);
  }

  return true;
}

}  // namespace tcg::detail

//----------------------------------------------------------------------------

template <typename mesh_type>
void refineMesh(mesh_type &mesh, TriMeshStuff::ActionEvaluator<mesh_type> &eval,
                unsigned long maxActions) {
  using namespace detail;

  typedef TriMeshStuff::ActionEvaluator<mesh_type> Evaluator;
  typedef typename Evaluator::Action Action;

  // DIAGNOSTICS_TIMER("simplifyMesh");

  // Build boundary edges. They will not be altered by the simplification
  // procedure.
  detail::BoundaryEdges<mesh_type> boundary(mesh);
  Action actions[3], *act, *actEnd = actions + 3;

  tcg::list<Edge> &edges = mesh.edges();
  tcg::list<Edge>::iterator it;

  // DIAGNOSTICS_SET("Simplify | Vertex count (before simplify)",
  // mesh.vertexCount());
  // DIAGNOSTICS_SET("Simplify | Edges count (before simplify)", edges.size());

  // Build a vector of the edges to be analyzed
  std::vector<UCHAR> buildEdge(edges.nodesCount(), 1);
  int touchedIdx;
  bool boundaryEdge;

cycle:

  if (maxActions-- == 0) return;

  // Analyze mesh for possible updates. Perform the first one.
  for (it = edges.begin(); it != edges.end(); ++it) {
    if (!buildEdge[it.m_idx]) continue;

    boundaryEdge = boundary.isBoundaryEdge(it.m_idx);

    eval.actionSort(mesh, it.m_idx, actions);

    for (act = actions; act < actEnd; ++act) {
      // Try to perform the i-th action
      if (*act == Evaluator::NONE)
        break;

      else if (!boundaryEdge && *act == Evaluator::SWAP &&
               testSwap(mesh, it.m_idx)) {
        touchedIdx = mesh.swapEdge(it.m_idx);
        touchEdge(buildEdge, mesh, touchedIdx);

        goto cycle;
      } else if (!boundaryEdge && *act == Evaluator::COLLAPSE &&
                 testCollapse(mesh, it.m_idx, boundary)) {
        touchedIdx = mesh.collapseEdge(it.m_idx);
        touchVertex(buildEdge, mesh, touchedIdx);

        goto cycle;
      } else if (*act == Evaluator::SPLIT) {
        Edge &ed = mesh.edge(it.m_idx);
        touchVertex(buildEdge, mesh, ed.vertex(0));
        touchVertex(buildEdge, mesh, ed.vertex(1));

        touchedIdx = mesh.splitEdge(it.m_idx);
        if (buildEdge.size() < edges.size()) buildEdge.resize(edges.size());

        touchVertex(buildEdge, mesh, touchedIdx);
        if (boundaryEdge) boundary.setBoundaryVertex(touchedIdx);

        goto cycle;
      }
    }

    buildEdge[it.m_idx] = 0;
  }

  // DIAGNOSTICS_SET("Simplify | Vertex count (after simplify)",
  // mesh.vertexCount());
  // DIAGNOSTICS_SET("Simplify | Edges count (after simplify)", edges.size());
}

}  // namespace tcg

#endif  // TCG_TRIANGULATE_HPP
