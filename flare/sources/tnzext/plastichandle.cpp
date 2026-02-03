

// TnzCore includes
#include "tmeshimage.h"

// tcg includes
#include "tcg/tcg_misc.h"
#include "tcg/tcg_mesh_bgl.h"

using namespace tcg::bgl;

// Boost includes
#include <boost/graph/properties.hpp>
#include <boost/graph/breadth_first_search.hpp>

// STD includes
#include <queue>

#include "ext/plastichandle.h"

#include <cstring>

//***********************************************************************************************
//    Distance building
//***********************************************************************************************

namespace {

typedef tcg::Mesh<TTextureVertex, tcg::Edge, tcg::FaceN<3>> Graph;

//----------------------------------------------------------------------------------

struct DistanceGreater {
  float *m_distances;

public:
  DistanceGreater(float *distances) : m_distances(distances) {}

  bool operator()(int a, int b) { return m_distances[a] > m_distances[b]; }
};

}  // namespace

//==================================================================================

namespace local {

struct BFS_DistanceBuilder {
  typedef boost::graph_traits<Graph>::edge_descriptor edge_descr;

  float *m_distances;  //!< (NOT owned) Distances from selected vertex
  UCHAR *m_colormap;   //!< (NOT owned) Graph BFS colormap

public:
  BFS_DistanceBuilder(float *distances, UCHAR *colormap)
      : m_distances(distances), m_colormap(colormap) {}

  void tree_edge(const edge_descr &e, const Graph &g) {
    int v, v0 = -1, v1 = tcg::bgl::target(e, g);
    double d, dMin = (std::numeric_limits<double>::max)();

    assert(m_colormap[v1] == boost::white_color);

    // Search for the distance-nearest found vertex
    const Graph::vertex_type &vx1 = g.vertex(v1);

    Graph::vertex_type::edges_const_iterator et, eEnd(vx1.edgesEnd());
    for (et = vx1.edgesBegin(); et != eEnd; ++et) {
      v = g.edge(*et).otherVertex(v1);
      if (m_colormap[v] != boost::white_color) {
        d                = tcg::point_ops::dist(g.vertex(v).P(), vx1.P());
        if (d < dMin) v0 = v, dMin = d;
      }
    }

    assert(v0 >= 0);

    // Just add to the distance from that vertex
    m_distances[v1] = m_distances[v0] + dMin;
  }

  // Unused bfs methods

  void initialize_vertex(int v, const Graph &g) {}
  void discover_vertex(int v, const Graph &g) {}
  void examine_vertex(int v, const Graph &g) {}
  void finish_vertex(int v, const Graph &g) {}

  void examine_edge(const edge_descr &e, const Graph &g) {}
  void non_tree_edge(const edge_descr &e, const Graph &g) {}
  void gray_target(const edge_descr &e, const Graph &g) {}
  void black_target(const edge_descr &e, const Graph &g) {}
};

}  // namespace local

//==================================================================================

bool buildDistances(float *distances, const TTextureMesh &mesh,
                    const TPointD &pos, int *faceHint) {
  using namespace local;

  // First off, find the face containing the specified vertex
  int localF = -1, &f = faceHint ? *faceHint : localF;

  if (f < 0 || f >= mesh.facesCount() || !mesh.faceContains(f, pos))
    f = mesh.faceContaining(pos);

  if (f < 0) return false;

  int vCount = mesh.verticesCount();

  // Prepare the interpolation builder
  UCHAR *colorMap = (UCHAR *)calloc(vCount, sizeof(UCHAR));
  BFS_DistanceBuilder visitor(distances, colorMap);

  DistanceGreater gr(distances);
  std::priority_queue<int, std::vector<int>, DistanceGreater> verticesQueue(gr);

  int v0, v1, v2;
  mesh.faceVertices(f, v0, v1, v2);

  // Prepare BFS data
  distances[v0] = distances[v1] = distances[v2] = 0.0f;

  verticesQueue.push(v0), colorMap[v0] = boost::gray_color;
  verticesQueue.push(v1), colorMap[v1] = boost::gray_color;

  // Launch BFS algorithm
  boost::breadth_first_visit((const Graph &)mesh, v2, verticesQueue, visitor,
                             colorMap);

  free(colorMap);
  return true;
}

//----------------------------------------------------------------------------------

void buildSO(double *so, const TTextureMesh &mesh,
             const std::vector<PlasticHandle> &handles, int *faceHints) {
  int v, vCount = mesh.verticesCount();

  // Build the interpolant function data
  const TRectD &bbox = mesh.getBBox();

  const double len = std::max(bbox.getLx(), bbox.getLy()), val = 1e-8;
  const double k = -log(val) / len;

  // Allocate / initialize arrays
  float *distances = (float *)malloc(vCount * sizeof(float));
  double *wSums    = (double *)calloc(vCount, sizeof(double));

  memset(so, 0, vCount * sizeof(double));

  // Iterate handles - for each, add the corresponding interpolant
  int h, hCount = handles.size();
  for (h = 0; h != hCount; ++h) {
    const PlasticHandle &handle = handles[h];

    if (!buildDistances(distances, mesh, handle.m_pos,
                        faceHints ? &faceHints[h] : 0))
      continue;

    for (v = 0; v != vCount; ++v) {
      double w = fabs(distances[v]);

      wSums[v] += w = exp(-k * w) / (1e-3 + w);
      so[v] += w * handle.m_so;
    }
  }

  // Normalize so values by the wSums
  for (v = 0; v != vCount; ++v)
    if (wSums[v] != 0.0) so[v] /= wSums[v];

  // Cleanup
  free(wSums);
  free(distances);
}
