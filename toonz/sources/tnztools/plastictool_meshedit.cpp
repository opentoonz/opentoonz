

// TnzCore includes
#include "tmeshimage.h"
#include "tgl.h"
#include "tundo.h"

// TnzExt includes
#include "ext/plasticdeformerstorage.h"

// tcg includes
#include "tcg/tcg_macros.h"
#include "tcg/tcg_point_ops.h"

#include <unordered_set>
#include <unordered_map>

using namespace tcg::bgl;

#include <boost/graph/breadth_first_search.hpp>

// STD includes
#include <stack>

#include "plastictool.h"

using namespace PlasticToolLocals;

//****************************************************************************************
//    Local namespace  stuff
//****************************************************************************************

namespace {

typedef PlasticTool::MeshIndex MeshIndex;
typedef TTextureMesh::vertex_type vertex_type;
typedef TTextureMesh::edge_type edge_type;
typedef TTextureMesh::face_type face_type;

//------------------------------------------------------------------------

bool borderEdge(const TTextureMesh &mesh, int e) {
  return (mesh.edge(e).facesCount() < 2);
}

bool borderVertex(const TTextureMesh &mesh, int v) {
  const TTextureVertex &vx = mesh.vertex(v);

  tcg::vertex_traits<TTextureVertex>::edges_const_iterator et,
      eEnd(vx.edgesEnd());
  for (et = vx.edgesBegin(); et != eEnd; ++et) {
    if (borderEdge(mesh, *et)) return true;
  }

  return false;
}

//============================================================================

bool testSwapEdge(const TTextureMesh &mesh, int e) {
  return (mesh.edge(e).facesCount() == 2);
}

//------------------------------------------------------------------------

bool testCollapseEdge(const TTextureMesh &mesh, int e) {
  struct Locals {
    const TTextureMesh &m_mesh;
    int m_e;
    const TTextureMesh::edge_type &m_ed;

    bool testTrianglesCount() {
      // There must be at least one remanining triangle
      return (m_mesh.facesCount() > m_ed.facesCount());
    }

    bool testBoundary() {
      // Must not join two non-adjacent boundary vertices
      return (!borderVertex(m_mesh, m_ed.vertex(0)) ||
              !borderVertex(m_mesh, m_ed.vertex(1)) || borderEdge(m_mesh, m_e));
    }

    bool testAdjacency() {
      // See TriMesh<>::collapseEdge()

      // Retrieve allowed adjacent vertices
      int f, fCount = m_ed.facesCount();
      int allowedV[6], *avt, *avEnd = allowedV + 3 * fCount;

      for (f = 0, avt = allowedV; f != fCount; ++f, avt += 3)
        m_mesh.faceVertices(m_ed.face(f), avt[0], avt[1], avt[2]);

      // Test adjacent vertices
      int v0 = m_ed.vertex(0), v1 = m_ed.vertex(1);

      const vertex_type &vx0 = m_mesh.vertex(v0);

      tcg::vertex_traits<vertex_type>::edges_const_iterator et,
          eEnd = vx0.edgesEnd();
      for (et = vx0.edgesBegin(); et != eEnd; ++et) {
        int otherV = m_mesh.edge(*et).otherVertex(v0);

        if (m_mesh.edgeInciding(v1, otherV) >= 0) {
          // Adjacent vertex - must be found in the allowed list
          if (std::find(allowedV, avEnd, otherV) == avEnd) return false;
        }
      }

      return true;
    }
  } locals = {mesh, e, mesh.edge(e)};

  return (locals.testTrianglesCount() && locals.testBoundary() &&
          locals.testAdjacency());
}

}  // namespace

//****************************************************************************************
//    PlasticToolLocals  stuff
//****************************************************************************************

namespace PlasticToolLocals {

struct Closer {
  const TTextureMesh &m_mesh;
  TPointD m_pos;

  double dist2(const TTextureMesh::vertex_type &a) {
    return tcg::point_ops::dist2<TPointD>(a.P(), m_pos);
  }

  double dist2(const TTextureMesh::edge_type &a) {
    const TTextureMesh::vertex_type &avx0 = m_mesh.vertex(a.vertex(0)),
                                    &avx1 = m_mesh.vertex(a.vertex(1));

    return sq(tcg::point_ops::segDist<TPointD>(avx0.P(), avx1.P(), m_pos));
  }

  bool operator()(const TTextureMesh::vertex_type &a,
                  const TTextureMesh::vertex_type &b) {
    return (dist2(a) < dist2(b));
  }

  bool operator()(const TTextureMesh::edge_type &a,
                  const TTextureMesh::edge_type &b) {
    return (dist2(a) < dist2(b));
  }
};

//==============================================================================

static std::pair<double, int> closestVertex(const TTextureMesh &mesh,
                                            const TPointD &pos) {
  Closer closer = {mesh, pos};
  int vIdx      = int(
      std::min_element(mesh.vertices().begin(), mesh.vertices().end(), closer)
          .index());

  return std::make_pair(closer.dist2(mesh.vertex(vIdx)), vIdx);
}

//------------------------------------------------------------------------

static std::pair<double, int> closestEdge(const TTextureMesh &mesh,
                                          const TPointD &pos) {
  Closer closer = {mesh, pos};
  int eIdx =
      int(std::min_element(mesh.edges().begin(), mesh.edges().end(), closer)
              .index());

  return std::make_pair(closer.dist2(mesh.edge(eIdx)), eIdx);
}

//------------------------------------------------------------------------

std::pair<double, MeshIndex> closestVertex(const TMeshImage &mi,
                                           const TPointD &pos) {
  std::pair<double, MeshIndex> closest((std::numeric_limits<double>::max)(),
                                       MeshIndex());

  const TMeshImage::meshes_container &meshes = mi.meshes();

  TMeshImage::meshes_container::const_iterator mt, mEnd = meshes.end();
  for (mt = meshes.begin(); mt != mEnd; ++mt) {
    const std::pair<double, int> &candidateIdx = closestVertex(**mt, pos);

    std::pair<double, MeshIndex> candidate(
        candidateIdx.first,
        MeshIndex(mt - meshes.begin(), candidateIdx.second));

    if (candidate < closest) closest = candidate;
  }

  return closest;
}

//------------------------------------------------------------------------

std::pair<double, MeshIndex> closestEdge(const TMeshImage &mi,
                                         const TPointD &pos) {
  std::pair<double, MeshIndex> closest((std::numeric_limits<double>::max)(),
                                       MeshIndex());

  const TMeshImage::meshes_container &meshes = mi.meshes();

  TMeshImage::meshes_container::const_iterator mt, mEnd = meshes.end();
  for (mt = meshes.begin(); mt != mEnd; ++mt) {
    const std::pair<double, int> &candidateIdx = closestEdge(**mt, pos);

    std::pair<double, MeshIndex> candidate(
        candidateIdx.first,
        MeshIndex(mt - meshes.begin(), candidateIdx.second));

    if (candidate < closest) closest = candidate;
  }

  return closest;
}

}  // namespace

//****************************************************************************************
//    Cut Mesh  operation
//****************************************************************************************

namespace {

struct EdgeCut {
  int m_vIdx;  //!< Vertex index to cut from.
  int m_eIdx;  //!< Edge index to cut.

  EdgeCut(int vIdx, int eIdx) : m_vIdx(vIdx), m_eIdx(eIdx) {}
};

struct VertexOccurrence {
  int m_count;               //!< Number of times a vertex occurs.
  int m_adjacentEdgeIdx[2];  //!< Edge indexes of which a vertex is endpoint.
};

//============================================================================

bool buildEdgeCuts(const TMeshImage &mi,
                   const PlasticTool::MeshSelection &edgesSelection,
                   int &meshIdx, std::vector<EdgeCut> &edgeCuts) {
  typedef PlasticTool::MeshSelection::objects_container edges_container;
  typedef PlasticTool::MeshIndex MeshIndex;
  typedef std::unordered_map<int, VertexOccurrence> VertexOccurrencesMap;

  struct locals {
    static bool differentMesh(const MeshIndex &a, const MeshIndex &b) {
      return (a.m_meshIdx != b.m_meshIdx);
    }

    static int testSingleMesh(const edges_container &edges) {
      assert(!edges.empty());
      return (std::find_if(edges.begin(), edges.end(),
                           [&edges](const MeshIndex &x) {
                             return differentMesh(x, edges.front());
                           }) == edges.end())
                 ? edges.front().m_meshIdx
                 : -1;
    }

    static bool testNoBoundaryEdge(const TTextureMesh &mesh,
                                   const edges_container &edges) {
      edges_container::const_iterator et, eEnd = edges.end();
      for (et = edges.begin(); et != eEnd; ++et)
        if (::borderEdge(mesh, et->m_idx)) return false;

      return true;
    }

    static bool buildVertexOccurrences(
        const TTextureMesh &mesh, const edges_container &edges,
        VertexOccurrencesMap &vertexOccurrences) {
      // Calculate vertex occurrences as edge endpoints
      edges_container::const_iterator et, eEnd = edges.end();
      for (et = edges.begin(); et != eEnd; ++et) {
        const edge_type &ed = mesh.edge(et->m_idx);
        int v0 = ed.vertex(0), v1 = ed.vertex(1);

        VertexOccurrence &vo0 = vertexOccurrences[v0],
                         &vo1 = vertexOccurrences[v1];

        if (vo0.m_count > 1 || vo1.m_count > 1) return false;

        vo0.m_adjacentEdgeIdx[vo0.m_count++] =
            vo1.m_adjacentEdgeIdx[vo1.m_count++] = et->m_idx;
      }

      return true;
    }

    static bool buildEdgeCuts(const TTextureMesh &mesh,
                              const edges_container &edges,
                              std::vector<EdgeCut> &edgeCuts) {
      VertexOccurrencesMap vertexOccurrences;
      if (!buildVertexOccurrences(mesh, edges, vertexOccurrences)) return false;

      // Build endpoints (exactly 2)
      int endPoints[2];
      int epCount = 0;

      VertexOccurrencesMap::iterator ot, oEnd = vertexOccurrences.end();
      for (ot = vertexOccurrences.begin(); ot != oEnd; ++ot) {
        if (ot->second.m_count == 1) {
          if (epCount > 1) return false;

          endPoints[epCount++] = ot->first;
        }
      }

      if (epCount != 2) return false;

      // Pick the first endpoint on the boundary, if any (otherwise, just pick
      // one)
      int *ept, *epEnd = endPoints + 2;

      ept = std::find_if(endPoints, epEnd,
                         [&mesh](int v) { return borderVertex(mesh, v); });
      if (ept == epEnd) {
        // There is no boundary endpoint
        if (edges.size() < 2)  // We should not cut the mesh on a
          return false;        // single edge - no vertex to duplicate!

        ept = endPoints;
      }

      // Build the edge cuts list, expanding the edges selection from
      // the chosen endpoint

      edgeCuts.push_back(EdgeCut(  // Build the first EdgeCut separately
          *ept, vertexOccurrences[*ept].m_adjacentEdgeIdx[0]));

      int e, eCount = int(edges.size());  // Build the remaining ones
      for (e = 1; e != eCount; ++e) {
        const EdgeCut &lastCut = edgeCuts.back();

        int vIdx = mesh.edge(lastCut.m_eIdx).otherVertex(lastCut.m_vIdx);

        const int(&adjEdges)[2] = vertexOccurrences[vIdx].m_adjacentEdgeIdx;
        int eIdx = (adjEdges[0] == lastCut.m_eIdx) ? adjEdges[1] : adjEdges[0];

        edgeCuts.push_back(EdgeCut(vIdx, eIdx));
      }

      return true;
    }
  };

  const edges_container &edges = edgesSelection.objects();

  // Trivial early bailouts
  if (edges.empty()) return false;

  // Selected edges must lie on the same mesh
  meshIdx = locals::testSingleMesh(edges);
  if (meshIdx < 0) return false;

  const TTextureMesh &mesh = *mi.meshes()[meshIdx];

  // No selected edge must be on the boundary
  return (locals::testNoBoundaryEdge(mesh, edges) &&
          locals::buildEdgeCuts(mesh, edges, edgeCuts));
}

//------------------------------------------------------------------------

inline bool testCutMesh(const TMeshImage &mi,
                        const PlasticTool::MeshSelection &edgesSelection) {
  std::vector<EdgeCut> edgeCuts;
  int meshIdx;

  return buildEdgeCuts(mi, edgesSelection, meshIdx, edgeCuts);
}

//------------------------------------------------------------------------

void slitMesh(TTextureMesh &mesh,
              int e)  //! Opens a slit along the specified edge index.
{
  TTextureMesh::edge_type &ed = mesh.edge(e);

  assert(ed.facesCount() == 2);

  // Duplicate the edge and pass one face to the duplicate
  TTextureMesh::edge_type edDup(ed.vertex(0), ed.vertex(1));

  int f = ed.face(1);

  edDup.addFace(f);
  ed.eraseFace(ed.facesBegin() + 1);

  int eDup = mesh.addEdge(edDup);

  // Alter the face to host the duplicate
  TTextureMesh::face_type &fc = mesh.face(f);

  (fc.edge(0) == e)
      ? fc.setEdge(0, eDup)
      : (fc.edge(1) == e) ? fc.setEdge(1, eDup) : fc.setEdge(2, eDup);
}

//------------------------------------------------------------------------

/*!
  \brief    Duplicates a mesh edge-vertex pair (the 'cut') and separates their
            connections to adjacent mesh primitives.

  \remark   The starting vertex is supposed to be on the mesh boundary.
  \remark   Edges with a single neighbouring face can be duplicated, too.
*/
void cutEdge(TTextureMesh &mesh, const EdgeCut &edgeCut) {
  struct locals {
    static void transferEdge(TTextureMesh &mesh, int e, int vFrom, int vTo) {
      edge_type &ed = mesh.edge(e);

      vertex_type &vxFrom = mesh.vertex(vFrom), &vxTo = mesh.vertex(vTo);

      (ed.vertex(0) == vFrom) ? ed.setVertex(0, vTo) : ed.setVertex(1, vTo);

      vxTo.addEdge(e);
      vxFrom.eraseEdge(
          std::find(vxFrom.edges().begin(), vxFrom.edges().end(), e));
    }

    static void transferFace(TTextureMesh &mesh, int eFrom, int eTo) {
      edge_type &edFrom = mesh.edge(eFrom), &edTo = mesh.edge(eTo);

      int f = mesh.edge(eFrom).face(1);
      {
        face_type &fc = mesh.face(f);

        (fc.edge(0) == eFrom)
            ? fc.setEdge(0, eTo)
            : (fc.edge(1) == eFrom) ? fc.setEdge(1, eTo) : fc.setEdge(2, eTo);

        edTo.addFace(f);
        edFrom.eraseFace(edFrom.facesBegin() + 1);
      }
    }

  };  // locals

  int vOrig = edgeCut.m_vIdx, eOrig = edgeCut.m_eIdx;

  // Create a new vertex at the same position of the original
  int vDup = mesh.addVertex(vertex_type(mesh.vertex(vOrig).P()));

  int e = eOrig;
  if (mesh.edge(e).facesCount() == 2) {
    // Duplicate the cut edge
    e = mesh.addEdge(edge_type(vDup, mesh.edge(eOrig).otherVertex(vOrig)));

    // Transfer one face from the original to the duplicate
    locals::transferFace(mesh, eOrig, e);
  } else {
    // Transfer the original edge to the duplicate vertex
    locals::transferEdge(mesh, eOrig, vOrig, vDup);
  }

  // Edges adjacent to the original vertex that are also adjacent
  // to the transferred face above must be transferred too
  int f = mesh.edge(e).face(0);

  while (f >= 0) {
    // Retrieve the next edge to transfer
    int otherE = mesh.otherFaceEdge(f, mesh.edge(e).otherVertex(vDup));

    // NOTE: Not "mesh.edgeInciding(vOrig, mesh.otherFaceVertex(f, e))" in the
    // calculation
    //       of otherE. This is required since by transferring each edge at a
    //       time,
    //       we're 'breaking' faces up - f is adjacent to both vOrig AND vDup!
    //
    //       The chosen calculation, instead, just asks for the one edge which
    //       does
    //       not have a specific vertex in common to the 2 other edges in the
    //       face.

    locals::transferEdge(mesh, otherE, vOrig, vDup);

    // Update e and f
    e = otherE;
    f = mesh.edge(otherE).otherFace(f);
  }
}

//------------------------------------------------------------------------
}
namespace locals_ {      // Need to use a named namespace due to
                         // a known gcc 4.2 bug with compiler-generated
struct VertexesRecorder  // copy constructors.
{
  std::unordered_set<int> &m_examinedVertexes;

public:
  typedef boost::on_examine_vertex event_filter;

public:
  VertexesRecorder(std::unordered_set<int> &examinedVertexes)
      : m_examinedVertexes(examinedVertexes) {}

  void operator()(int v, const TTextureMesh &) { m_examinedVertexes.insert(v); }
};
}
namespace {  //

void splitUnconnectedMesh(TMeshImage &mi, int meshIdx) {
  struct locals {
    static void buildConnectedComponent(const TTextureMesh &mesh,
                                        std::unordered_set<int> &vertexes) {
      // Prepare BFS algorithm
      std::unique_ptr<UCHAR[]> colorMapP(new UCHAR[mesh.vertices().nodesCount()]());

      locals_::VertexesRecorder vertexesRecorder(vertexes);
      std::stack<int> verticesQueue;

      // Launch it
      boost::breadth_first_visit(
          mesh, int(mesh.vertices().begin().index()), verticesQueue,
          boost::make_bfs_visitor(vertexesRecorder), colorMapP.get());
    }
  };  // locals

  // Retrieve the list of vertexes in the first connected component
  TTextureMesh &origMesh = *mi.meshes()[meshIdx];

  std::unordered_set<int> firstComponent;
  locals::buildConnectedComponent(origMesh, firstComponent);

  if (firstComponent.size() == origMesh.verticesCount()) return;

  // There are (exactly) 2 connected components. Just duplicate the mesh
  // and keep/delete found vertexes.
  TTextureMeshP dupMeshPtr(new TTextureMesh(origMesh));
  TTextureMesh &dupMesh = *dupMeshPtr;

  TTextureMesh::vertices_container &vertices = origMesh.vertices();

  TTextureMesh::vertices_container::iterator vt, vEnd = vertices.end();
  for (vt = vertices.begin(); vt != vEnd;) {
    int v = int(vt.index());
    ++vt;

    if (firstComponent.count(v))
      dupMesh.removeVertex(v);
    else
      origMesh.removeVertex(v);
  }

  dupMesh.squeeze();
  origMesh.squeeze();

  mi.meshes().push_back(dupMeshPtr);
}

//------------------------------------------------------------------------

void splitMesh(TMeshImage &mi, int meshIdx, int lastBoundaryVertex) {
  // Retrieve a cutting edge with a single adjacent face - cutting that
  // will just duplicate the vertex and separate the mesh in 2 connected
  // components
  TTextureMesh &mesh = *mi.meshes()[meshIdx];

  int e;
  {
    const vertex_type &lbVx = mesh.vertex(lastBoundaryVertex);

    vertex_type::edges_const_iterator et =
        std::find_if(lbVx.edgesBegin(), lbVx.edgesEnd(),
                     [&mesh](int e) { return borderEdge(mesh, e); });
    assert(et != lbVx.edgesEnd());

    e = *et;
  }

  cutEdge(mesh, EdgeCut(lastBoundaryVertex, e));

  // At this point, separate the 2 resulting connected components
  // in 2 separate meshes (if necessary)
  splitUnconnectedMesh(mi, meshIdx);
}

//------------------------------------------------------------------------

bool cutMesh(TMeshImage &mi, const PlasticTool::MeshSelection &edgesSelection) {
  struct locals {
    static int lastVertex(const TTextureMesh &mesh,
                          const std::vector<EdgeCut> &edgeCuts) {
      return mesh.edge(edgeCuts.back().m_eIdx)
          .otherVertex(edgeCuts.back().m_vIdx);
    }

    static int lastBoundaryVertex(const TTextureMesh &mesh,
                                  const std::vector<EdgeCut> &edgeCuts) {
      int v = lastVertex(mesh, edgeCuts);
      return ::borderVertex(mesh, v) ? v : -1;
    }

  };  // locals

  std::vector<EdgeCut> edgeCuts;
  int meshIdx;

  if (!::buildEdgeCuts(mi, edgesSelection, meshIdx, edgeCuts)) return false;

  TTextureMesh &mesh = *mi.meshes()[meshIdx];

  int lastBoundaryVertex = locals::lastBoundaryVertex(mesh, edgeCuts);

  // Slit the mesh on the first edge, in case the cuts do not start
  // on the mesh boundary
  std::vector<EdgeCut>::iterator ecBegin = edgeCuts.begin();

  if (!::borderVertex(mesh, ecBegin->m_vIdx)) {
    ::slitMesh(mesh, ecBegin->m_eIdx);
    ++ecBegin;
  }

  // Cut edges, in the order specified by edgeCuts
  std::for_each(ecBegin, edgeCuts.end(), [&mesh](const EdgeCut &edgeCut) {
    return cutEdge(mesh, edgeCut);
  });

  // Finally, the mesh could have been split in 2 - we need to separate
  // the pieces if needed
  if (lastBoundaryVertex >= 0) splitMesh(mi, meshIdx, lastBoundaryVertex);

  return true;
}

}  // namespace

//****************************************************************************************
//    Undo  definitions
//****************************************************************************************

namespace {

class MoveVertexUndo_Mesh final : public TUndo {
  int m_row, m_col;  //!< Xsheet coordinates

  std::vector<MeshIndex> m_vIdxs;     //!< Moved vertices
  std::vector<TPointD> m_origVxsPos;  //!< Original vertex positions
  TPointD m_posShift;                 //!< Vertex positions shift

public:
  MoveVertexUndo_Mesh(const std::vector<MeshIndex> &vIdxs,
                      const std::vector<TPointD> &origVxsPos,
                      const TPointD &posShift)
      : m_row(::row())
      , m_col(::column())
      , m_vIdxs(vIdxs)
      , m_origVxsPos(origVxsPos)
      , m_posShift(posShift) {
    assert(m_vIdxs.size() == m_origVxsPos.size());
  }

  int getSize() const override {
    return int(sizeof(*this) +
               m_vIdxs.size() * (sizeof(int) + 2 * sizeof(TPointD)));
  }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.setMeshVertexesSelection(m_vIdxs);
    l_plasticTool.moveVertex_mesh(m_origVxsPos, m_posShift);

    l_plasticTool.invalidate();
    l_plasticTool
        .notifyImageChanged();  // IMPORTANT: In particular, sets the level's
  }                             //            dirty flag, so Toonz knows it has
                                //            to be saved!
  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.setMeshVertexesSelection(m_vIdxs);
    l_plasticTool.moveVertex_mesh(m_origVxsPos, TPointD());

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }
};

//==============================================================================

class SwapEdgeUndo final : public TUndo {
  int m_row, m_col;             //!< Xsheet coordinates
  mutable MeshIndex m_edgeIdx;  //!< Edge index

public:
  SwapEdgeUndo(const MeshIndex &edgeIdx)
      : m_row(::row()), m_col(::column()), m_edgeIdx(edgeIdx) {}

  int getSize() const override { return sizeof(*this); }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const TMeshImageP &mi = TMeshImageP(TTool::getImage(true));
    assert(mi);

    // Perform swap
    TTextureMesh &mesh = *mi->meshes()[m_edgeIdx.m_meshIdx];

    m_edgeIdx.m_idx = mesh.swapEdge(m_edgeIdx.m_idx);
    assert(m_edgeIdx.m_idx >= 0);

    // Invalidate any deformer associated with mi
    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    // Update tool selection
    l_plasticTool.setMeshEdgesSelection(m_edgeIdx);

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }

  void undo() const override { redo(); }  // Operation is idempotent (indices
                                          // are perfectly restored, too)
};

//==============================================================================

class TTextureMeshUndo : public TUndo {
protected:
  int m_row, m_col;  //!< Xsheet coordinates

  int m_meshIdx;  //!< Mesh index in the image at stored xsheet coords
  mutable TTextureMesh m_origMesh;  //!< Copy of the original mesh

public:
  TTextureMeshUndo(int meshIdx)
      : m_row(::row()), m_col(::column()), m_meshIdx(meshIdx) {}

  // Let's say 1MB each - storing the mesh is costly
  int getSize() const override { return 1 << 20; }

  TMeshImageP getMeshImage() const {
    const TMeshImageP &mi = TMeshImageP(TTool::getImage(true));
    assert(mi);

    return mi;
  }
};

//==============================================================================

class CollapseEdgeUndo final : public TTextureMeshUndo {
  int m_eIdx;  //!< Collapsed edge index

public:
  CollapseEdgeUndo(const MeshIndex &edgeIdx)
      : TTextureMeshUndo(edgeIdx.m_meshIdx), m_eIdx(edgeIdx.m_idx) {}

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const TMeshImageP &mi = getMeshImage();

    // Store the original mesh
    TTextureMesh &mesh = *mi->meshes()[m_meshIdx];
    m_origMesh         = mesh;

    // Collapse
    mesh.collapseEdge(m_eIdx);
    mesh.squeeze();

    // Invalidate any cached deformer associated with the modified mesh image
    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    // Refresh the tool
    l_plasticTool.clearMeshSelections();

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const TMeshImageP &mi = getMeshImage();

    // Restore the original mesh
    TTextureMesh &mesh = *mi->meshes()[m_meshIdx];
    mesh               = m_origMesh;

    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    // Restore selection
    l_plasticTool.setMeshEdgesSelection(MeshIndex(m_meshIdx, m_eIdx));

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }
};

//==============================================================================

class SplitEdgeUndo final : public TTextureMeshUndo {
  int m_eIdx;  //!< Split edge index

public:
  SplitEdgeUndo(const MeshIndex &edgeIdx)
      : TTextureMeshUndo(edgeIdx.m_meshIdx), m_eIdx(edgeIdx.m_idx) {}

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const TMeshImageP &mi = getMeshImage();

    // Store the original mesh
    TTextureMesh &mesh = *mi->meshes()[m_meshIdx];
    m_origMesh         = mesh;

    // Split
    mesh.splitEdge(m_eIdx);
    // mesh.squeeze();                                                     //
    // There should be no need to squeeze

    assert(mesh.vertices().size() == mesh.vertices().nodesCount());  //
    assert(mesh.edges().size() == mesh.edges().nodesCount());        //
    assert(mesh.faces().size() == mesh.faces().nodesCount());        //

    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    l_plasticTool.clearMeshSelections();

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const TMeshImageP &mi = getMeshImage();

    // Restore the original mesh
    TTextureMesh &mesh = *mi->meshes()[m_meshIdx];
    mesh               = m_origMesh;

    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    // Restore selection
    l_plasticTool.setMeshEdgesSelection(MeshIndex(m_meshIdx, m_eIdx));

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }
};

//==============================================================================

class CutEdgesUndo final : public TUndo {
  int m_row, m_col;         //!< Xsheet coordinates
  TMeshImageP m_origImage;  //!< Clone of the original image

  PlasticTool::MeshSelection m_edgesSelection;  //!< Selection to operate on

public:
  CutEdgesUndo(const PlasticTool::MeshSelection &edgesSelection)
      : m_row(::row())
      , m_col(::column())
      , m_origImage(TTool::getImage(false)->cloneImage())
      , m_edgesSelection(edgesSelection) {}

  int getSize() const override { return 1 << 20; }

  bool do_() const {
    TMeshImageP mi = TTool::getImage(true);

    if (::cutMesh(*mi, m_edgesSelection)) {
      PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

      l_plasticTool.clearMeshSelections();

      l_plasticTool.invalidate();
      l_plasticTool.notifyImageChanged();

      return true;
    }

    return false;
  }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    bool ret = do_();
    (void)ret;
    assert(ret);
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    TMeshImageP mi = TTool::getImage(true);

    // Restore the original image
    *mi = *m_origImage;

    PlasticDeformerStorage::instance()->releaseMeshData(mi.getPointer());

    // Restore selection
    l_plasticTool.setMeshEdgesSelection(m_edgesSelection);

    l_plasticTool.invalidate();
    l_plasticTool.notifyImageChanged();
  }
};

}  // namespace

//****************************************************************************************
//    PlasticTool  functions
//****************************************************************************************

void PlasticTool::storeMeshImage() {
  TMeshImageP mi = getImage(false);

  if (mi != m_mi) {
    m_mi = mi;
    clearMeshSelections();
  }
}

//------------------------------------------------------------------------

void PlasticTool::setMeshSelection(MeshSelection &target,
                                   const MeshSelection &newSel) {
  if (newSel.isEmpty()) {
    target.selectNone();
    target.makeNotCurrent();

    return;
  }

  target.setObjects(newSel.objects());

  target.notifyView();
  target.makeCurrent();
}

//------------------------------------------------------------------------

void PlasticTool::toggleMeshSelection(MeshSelection &target,
                                      const MeshSelection &addition) {
  typedef MeshSelection::objects_container objects_container;

  const objects_container &storedIdxs = target.objects();
  const objects_container &addedIdxs  = addition.objects();

  // Build new selection
  objects_container selectedIdxs;

  if (target.contains(addition)) {
    std::set_difference(storedIdxs.begin(), storedIdxs.end(), addedIdxs.begin(),
                        addedIdxs.end(), std::back_inserter(selectedIdxs));
  } else {
    std::set_union(storedIdxs.begin(), storedIdxs.end(), addedIdxs.begin(),
                   addedIdxs.end(), std::back_inserter(selectedIdxs));
  }

  setMeshSelection(target, selectedIdxs);
}

//------------------------------------------------------------------------

void PlasticTool::clearMeshSelections() {
  m_mvHigh = m_meHigh = MeshIndex();

  m_mvSel.selectNone();
  m_mvSel.makeNotCurrent();

  m_meSel.selectNone();
  m_meSel.makeNotCurrent();
}

//------------------------------------------------------------------------

void PlasticTool::setMeshVertexesSelection(const MeshSelection &vSel) {
  setMeshSelection(m_meSel, MeshSelection()), setMeshSelection(m_mvSel, vSel);
}

//------------------------------------------------------------------------

void PlasticTool::toggleMeshVertexesSelection(const MeshSelection &vSel) {
  setMeshSelection(m_meSel, MeshSelection()),
      toggleMeshSelection(m_mvSel, vSel);
}

//------------------------------------------------------------------------

void PlasticTool::setMeshEdgesSelection(const MeshSelection &eSel) {
  setMeshSelection(m_meSel, eSel), setMeshSelection(m_mvSel, MeshSelection());
}

//------------------------------------------------------------------------

void PlasticTool::toggleMeshEdgesSelection(const MeshSelection &eSel) {
  toggleMeshSelection(m_meSel, eSel),
      setMeshSelection(m_mvSel, MeshSelection());
}

//------------------------------------------------------------------------

void PlasticTool::mouseMove_mesh(const TPointD &pos, const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;  // Needs to be done now - ensures m_pos is valid

  m_mvHigh = MeshIndex();  // Reset highlighted primitives

  if (m_mi) {
    // Look for nearest primitive
    std::pair<double, MeshIndex> closestVertex = ::closestVertex(*m_mi, pos),
                                 closestEdge = ::closestEdge(*m_mi, pos);

    // Discriminate on fixed metric
    const double hDistSq = sq(getPixelSize() * MESH_HIGHLIGHT_DISTANCE);

    m_mvHigh = m_meHigh = MeshIndex();

    if (closestEdge.first < hDistSq) m_meHigh = closestEdge.second;

    if (closestVertex.first < hDistSq)
      m_mvHigh = closestVertex.second, m_meHigh = MeshIndex();
  }

  assert(!(m_mvHigh &&
           m_meHigh));  // Vertex and edge highlights are mutually exclusive

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_mesh(const TPointD &pos,
                                      const TMouseEvent &me) {
  struct Locals {
    PlasticTool *m_this;

    void updateSelection(MeshSelection &sel, const MeshIndex &idx,
                         const TMouseEvent &me) {
      if (idx) {
        MeshSelection newSel(idx);

        if (me.isCtrlPressed())
          m_this->toggleMeshSelection(sel, newSel);
        else if (!sel.contains(newSel))
          m_this->setMeshSelection(sel, newSel);
      } else
        m_this->setMeshSelection(sel, MeshSelection());
    }
  } locals = {this};

  // Track mouse position
  m_pressedPos = m_pos = pos;

  // Update selection
  locals.updateSelection(m_mvSel, m_mvHigh, me);
  locals.updateSelection(m_meSel, m_meHigh, me);

  // Store original vertex positions
  if (!m_mvSel.isEmpty()) {
    std::vector<TPointD> v;
    for (auto const &e : m_mvSel.objects()) {
      v.push_back(m_mi->meshes()[e.m_meshIdx]->vertex(e.m_idx).P());
    }
    m_pressedVxsPos = std::move(v);
  }

  // Redraw selections
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDrag_mesh(const TPointD &pos,
                                      const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;

  if (!m_mvSel.isEmpty()) {
    moveVertex_mesh(m_pressedVxsPos, pos - m_pressedPos);
    invalidate();
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp_mesh(const TPointD &pos, const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;

  if (m_dragged && !m_mvSel.isEmpty()) {
    TUndoManager::manager()->add(new MoveVertexUndo_Mesh(
        m_mvSel.objects(), m_pressedVxsPos, pos - m_pressedPos));

    invalidate();
    notifyImageChanged();  // Sets the level's dirty flag      -.-'
  }
}

//------------------------------------------------------------------------

void PlasticTool::addContextMenuActions_mesh(QMenu *menu) {
  bool ret = true;

  if (!m_meSel.isEmpty()) {
    if (m_meSel.hasSingleObject()) {
      const MeshIndex &mIdx    = m_meSel.objects().front();
      const TTextureMesh &mesh = *m_mi->meshes()[mIdx.m_meshIdx];

      if (::testSwapEdge(mesh, mIdx.m_idx)) {
        QAction *swapEdge = menu->addAction(tr("Swap Edge"));
        ret = ret && connect(swapEdge, SIGNAL(triggered()), &l_plasticTool,
                             SLOT(swapEdge_mesh_undo()));
      }

      if (::testCollapseEdge(mesh, mIdx.m_idx)) {
        QAction *collapseEdge = menu->addAction(tr("Collapse Edge"));
        ret = ret && connect(collapseEdge, SIGNAL(triggered()), &l_plasticTool,
                             SLOT(collapseEdge_mesh_undo()));
      }

      QAction *splitEdge = menu->addAction(tr("Split Edge"));
      ret = ret && connect(splitEdge, SIGNAL(triggered()), &l_plasticTool,
                           SLOT(splitEdge_mesh_undo()));
    }

    if (::testCutMesh(*m_mi, m_meSel)) {
      QAction *cutEdges = menu->addAction(tr("Cut Mesh"));
      ret = ret && connect(cutEdges, SIGNAL(triggered()), &l_plasticTool,
                           SLOT(cutEdges_mesh_undo()));
    }

    menu->addSeparator();
  }

  assert(ret);
}

//------------------------------------------------------------------------

void PlasticTool::moveVertex_mesh(const std::vector<TPointD> &origVxsPos,
                                  const TPointD &posShift) {
  if (m_mvSel.isEmpty() || !m_mi) return;

  assert(origVxsPos.size() == m_mvSel.objects().size());

  // Move selected vertices
  TMeshImageP mi = getImage(true);
  assert(m_mi == mi);

  int v, vCount = int(m_mvSel.objects().size());
  for (v = 0; v != vCount; ++v) {
    const MeshIndex &meshIndex = m_mvSel.objects()[v];

    TTextureMesh &mesh               = *mi->meshes()[meshIndex.m_meshIdx];
    mesh.vertex(meshIndex.m_idx).P() = origVxsPos[v] + posShift;
  }

  // Mesh must be recompiled
  PlasticDeformerStorage::instance()->invalidateMeshImage(
      mi.getPointer(), PlasticDeformerStorage::MESH);
}

//------------------------------------------------------------------------

void PlasticTool::swapEdge_mesh_undo() {
  if (!(m_mi && m_meSel.hasSingleObject())) return;

  // Test current edge swapability
  {
    const MeshIndex &eIdx    = m_meSel.objects().front();
    const TTextureMesh &mesh = *m_mi->meshes()[eIdx.m_meshIdx];

    if (!::testSwapEdge(mesh, eIdx.m_idx)) return;
  }

  // Perform operation
  std::unique_ptr<TUndo> undo(new SwapEdgeUndo(m_meSel.objects().front()));
  undo->redo();

  TUndoManager::manager()->add(undo.release());
}

//------------------------------------------------------------------------

void PlasticTool::collapseEdge_mesh_undo() {
  if (!(m_mi && m_meSel.hasSingleObject())) return;

  // Test collapsibility of current edge
  {
    const MeshIndex &eIdx    = m_meSel.objects().front();
    const TTextureMesh &mesh = *m_mi->meshes()[eIdx.m_meshIdx];

    if (!::testCollapseEdge(mesh, eIdx.m_idx)) return;
  }

  // Perform operation
  std::unique_ptr<TUndo> undo(new CollapseEdgeUndo(m_meSel.objects().front()));
  undo->redo();

  TUndoManager::manager()->add(undo.release());
}

//------------------------------------------------------------------------

void PlasticTool::splitEdge_mesh_undo() {
  if (!(m_mi && m_meSel.hasSingleObject())) return;

  std::unique_ptr<TUndo> undo(new SplitEdgeUndo(m_meSel.objects().front()));
  undo->redo();

  TUndoManager::manager()->add(undo.release());
}

//------------------------------------------------------------------------

void PlasticTool::cutEdges_mesh_undo() {
  if (!m_mi) return;

  std::unique_ptr<CutEdgesUndo> undo(new CutEdgesUndo(m_meSel.objects()));

  if (undo->do_()) TUndoManager::manager()->add(undo.release());
}

//------------------------------------------------------------------------

void PlasticTool::draw_mesh() {
  struct Locals {
    PlasticTool *m_this;
    double m_pixelSize;

    void drawLine(const TPointD &a, const TPointD &b) {
      glVertex2d(a.x, a.y);
      glVertex2d(b.x, b.y);
    }

    void drawVertexSelections() {
      typedef MeshSelection::objects_container objects_container;
      const objects_container &objects = m_this->m_mvSel.objects();

      glColor3ub(255, 0, 0);  // Red
      glLineWidth(1.0f);

      const double hSize = MESH_SELECTED_HANDLE_SIZE * m_pixelSize;

      objects_container::const_iterator vt, vEnd = objects.end();
      for (vt = objects.begin(); vt != vEnd; ++vt) {
        const TTextureVertex &vx =
            m_this->m_mi->meshes()[vt->m_meshIdx]->vertex(vt->m_idx);

        ::drawFullSquare(vx.P(), hSize);
      }
    }

    void drawEdgeSelections() {
      typedef MeshSelection::objects_container objects_container;
      const objects_container &objects = m_this->m_meSel.objects();

      glColor3ub(0, 0, 255);  // Blue
      glLineWidth(2.0f);

      glBegin(GL_LINES);

      objects_container::const_iterator et, eEnd = objects.end();
      for (et = objects.begin(); et != eEnd; ++et) {
        const TTextureVertex
            &vx0 =
                m_this->m_mi->meshes()[et->m_meshIdx]->edgeVertex(et->m_idx, 0),
            &vx1 =
                m_this->m_mi->meshes()[et->m_meshIdx]->edgeVertex(et->m_idx, 1);

        drawLine(vx0.P(), vx1.P());
      }

      glEnd();
    }

    void drawVertexHighlights() {
      if (m_this->m_mvHigh) {
        const MeshIndex &vHigh = m_this->m_mvHigh;

        const TTextureMesh::vertex_type &vx =
            m_this->m_mi->meshes()[vHigh.m_meshIdx]->vertex(vHigh.m_idx);

        glColor3ub(255, 0, 0);  // Red
        glLineWidth(1.0f);

        const double hSize = MESH_HIGHLIGHTED_HANDLE_SIZE * m_pixelSize;

        ::drawSquare(vx.P(), hSize);
      }
    }

    void drawEdgeHighlights() {
      if (m_this->m_meHigh) {
        const MeshIndex &eHigh = m_this->m_meHigh;

        const TTextureMesh::vertex_type
            &vx0 = m_this->m_mi->meshes()[eHigh.m_meshIdx]->edgeVertex(
                eHigh.m_idx, 0),
            &vx1 = m_this->m_mi->meshes()[eHigh.m_meshIdx]->edgeVertex(
                eHigh.m_idx, 1);

        {
          glPushAttrib(GL_LINE_BIT);

          glEnable(GL_LINE_STIPPLE);
          glLineStipple(1, 0xCCCC);

          glColor3ub(0, 0, 255);  // Blue
          glLineWidth(1.0f);

          glBegin(GL_LINES);
          drawLine(vx0.P(), vx1.P());
          glEnd();

          glPopAttrib();
        }
      }
    }

  } locals = {this, getPixelSize()};

  // The selected mesh image is already drawn by the stage

  // Draw additional overlays
  if (m_mi) {
    locals.drawVertexSelections();
    locals.drawEdgeSelections();

    locals.drawVertexHighlights();
    locals.drawEdgeHighlights();
  }
}
