

// TnzCore includes
#include "trop.h"
#include "trop_borders.h"
#include "tpixelutils.h"

// tcg includes
#include "tcg_wrap.h"

#include "tcg/tcg_point.h"
#include "tcg/tcg_cyclic.h"
#include "tcg/tcg_containers_reader.h"

#define INCLUDE_HPP
#include "tcg/tcg_triangulate.h"
#undef INCLUDE_HPP

// TnzExt includes
#define INCLUDE_HPP
#include "../common/trop/raster_edge_evaluator.h"
#undef INCLUDE_HPP

#include "ext/meshbuilder.h"

//**************************************************************************************
//    Local namespace
//**************************************************************************************

namespace {

struct PolygonVertex {
  double m_pos[3];
  int m_idx;

  PolygonVertex(const TPoint &p) : m_idx(-1) {
    m_pos[0] = p.x, m_pos[1] = p.y, m_pos[2] = 0.0;
  }
};

}  // namespace

//**************************************************************************************
//    tcg  stuff
//**************************************************************************************

namespace tcg {

template <>
struct traits<TTextureMeshP> {
  typedef TTextureMeshP *pointer_type;
  typedef TTextureMeshP *&reference_type;
  typedef TTextureMesh pointed_type;
};

//--------------------------------------------------------------------------

template <>
struct point_traits<PolygonVertex> {
  typedef PolygonVertex point_type;
  typedef double value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.m_pos[0]; }
  inline static value_type y(const point_type &p) { return p.m_pos[1]; }
};

}  // namespace tcg

//**************************************************************************************
//    MeshBuilder Locals
//**************************************************************************************

namespace {

//======================================================================================
//    Thresholding  stuff
//======================================================================================

template <typename Pix>
void thresholdRaster(const TRasterPT<Pix> &ras, TRasterGR8P &out,
                     const Pix &transp) {
  int lx = ras->getLx(), y, ly = ras->getLy();
  for (y = 0; y < ly; ++y) {
    Pix *pix, *line = ras->pixels(y), *lineEnd = line + lx;
    TPixelGR8 *gr, *grLine = out->pixels(y);

    for (pix = line, gr = grLine; pix != lineEnd; ++pix, ++gr)
      gr->value = (pix->m && *pix != transp) ? 0 : 255;
  }
}

//--------------------------------------------------------------------------

template <typename Pix>
void thresholdRasterGr(const TRasterPT<Pix> &ras, TRasterGR8P &out,
                       const Pix &transp) {
  int lx = ras->getLx(), y, ly = ras->getLy();
  for (y = 0; y < ly; ++y) {
    Pix *pix, *line = ras->pixels(y), *lineEnd = line + lx;
    TPixelGR8 *gr, *grLine = out->pixels(y);

    for (pix = line, gr = grLine; pix != lineEnd; ++pix, ++gr)
      gr->value = (*pix != transp) ? 0 : 255;
  }
}

//--------------------------------------------------------------------------

void thresholdRasterCM32(const TRasterCM32P &ras, TRasterGR8P &out) {
  int lx = ras->getLx(), y, ly = ras->getLy();
  for (y = 0; y < ly; ++y) {
    TPixelCM32 *pix, *line = ras->pixels(y), *lineEnd = line + lx;
    TPixelGR8 *gr, *grLine = out->pixels(y);

    for (pix = line, gr = grLine; pix != lineEnd; ++pix, ++gr)
      gr->value = (pix->isPurePaint() && !pix->getPaint()) ? 255 : 0;
  }
}

//--------------------------------------------------------------------------

TRasterGR8P thresholdRaster(const TRasterP &ras,
                            const MeshBuilderOptions &opts) {
  TRasterGR8P binaryRas(ras->getLx(), ras->getLy());

  TRasterCM32P rasCM(ras);
  if (rasCM)
    thresholdRasterCM32(rasCM, binaryRas);
  else
    switch (ras->getPixelSize()) {
    case 1: {
      TRasterGR8P rasGR8(ras);
      thresholdRasterGr(rasGR8, binaryRas,
                        TPixelGR8::from(toPixel32(opts.m_transparentColor)));
      break;
    }

    case 2: {
      TRasterGR16P rasGR16(ras);
      thresholdRasterGr(rasGR16, binaryRas,
                        TPixelGR16::from(opts.m_transparentColor));
      break;
    }

    case 4: {
      TRaster32P ras32(ras);
      thresholdRaster(ras32, binaryRas, toPixel32(opts.m_transparentColor));
      break;
    }

    case 8: {
      TRaster64P ras64(ras);
      thresholdRaster(ras64, binaryRas, opts.m_transparentColor);
      break;
    }

    default:
      assert(false);
    }

  // Build an enlarged ras to preserve borders. 5 pixels should be fine.
  TRasterGR8P result(ras->getLx(), ras->getLy());
  TRop::blur(result, binaryRas, opts.m_margin, 0, 0);

  thresholdRasterGr(result, result, TPixelGR8::White);

  return result;
}

//======================================================================================
//    Borders Extraction  stuff
//======================================================================================

using namespace TRop::borders;

//--------------------------------------------------------------------------

template <typename T>
inline void delete_(T t) {
  delete t;
}

template <typename T>
struct Vector final : public std::vector<T> {
  Vector() : std::vector<T>() {}
  ~Vector() { std::for_each(this->begin(), this->end(), delete_<T>); }
};

//--------------------------------------------------------------------------

typedef std::vector<TPoint> RasterBorder;
typedef std::vector<PolygonVertex> Polygon;
typedef Vector<Polygon *> Family;
typedef Vector<Family *> Tribe;

//--------------------------------------------------------------------------

struct PolygonReader {
  Polygon *m_polygon;

public:
  typedef tcg::cyclic_iterator<RasterBorder::iterator> cyclic_iter;

public:
  PolygonReader() : m_polygon(0) {}

  void openContainer(const cyclic_iter &ct) {
    m_polygon = new Polygon;
    m_polygon->push_back(PolygonVertex(*ct));
  }

  void addElement(const cyclic_iter &ct) {
    m_polygon->push_back(PolygonVertex(*ct));
  }

  void closeContainer() {}
};

//--------------------------------------------------------------------------

class BordersReader final : public ImageMeshesReaderT<TPixelGR8> {
public:
  Vector<RasterBorder *> m_borders;
  RasterBorder *m_current;

public:
  BordersReader()
      : ImageMeshesReaderT<TPixelGR8>(PixelSelector<TPixelGR8>(false))
      , m_current(0) {}

  void openFace(ImageMesh *mesh, int faceIdx, const TPixelGR8 &color) override {
    ImageMeshesReader::openFace(mesh, faceIdx);  // defines imageIndex

    if (mesh) {
      ImageMesh::face_type &fc = mesh->face(faceIdx);
      fc.imageIndex()          = (color.value) ? 0 : 1;  // redefines iI
    }
  }

  //--------------------------------------------------------------------------

  void openEdge(const raster_edge_iterator &it) override {
    m_current = new RasterBorder;
    m_current->push_back(it.pos());
  }

  //--------------------------------------------------------------------------

  void addVertex(const raster_edge_iterator &it) override {
    m_current->push_back(it.pos());
  }

  //--------------------------------------------------------------------------

  void closeEdge(ImageMesh *mesh, int edgeIdx) override {
    ImageMesh::edge_type &ed = mesh->edge(edgeIdx);
    ed.imageIndex()          = m_borders.size();

    m_borders.push_back(m_current);
    m_current = 0;

    ImageMeshesReader::closeEdge(mesh, edgeIdx);
  }
};

//--------------------------------------------------------------------------

Polygon *reduceBorder(RasterBorder *border) {
  typedef RasterBorder::iterator iter;
  typedef tcg::cyclic_iterator<iter> cyclic_iter;

  iter b(border->begin()), e(border->end());

  cyclic_iter cBegin(b, b, e - 1, 0), cEnd(b + 1, b, e - 1, 1);

  RasterEdgeEvaluator<cyclic_iter> eval(cBegin - 1, cEnd + 1, 2.0,
                                        (std::numeric_limits<double>::max)());

  PolygonReader reader;
  tcg::sequence_ops::minimalPath(cBegin, cEnd, eval, reader);

  return reader.m_polygon;
}

//--------------------------------------------------------------------------

void reduceBorders(Tribe *tribe, const ImageMeshesReader &reader,
                   const Vector<RasterBorder *> &borders, int meshIdx,
                   const ImageMesh::face_type &fc) {
  // Traverse the image structure. Each time a black face is encountered, add
  // its associated family to the
  // resulting tribe.

  const tcg::list<ImageMeshP> &meshes = reader.meshes();

  if (meshIdx >= 0 && fc.imageIndex()) {
    // Build a family. Start by extracting the face's contour
    Family *family = new Family;
    tribe->push_back(family);

    const ImageMeshP &mesh = meshes[meshIdx];

    Polygon *outerBorder = reduceBorder(borders[mesh->edge(0).imageIndex()]);
    family->push_back(outerBorder);

    // Then, extract the contours of every sub-mesh
    int m, mCount = fc.meshesCount();
    for (m = 0; m < mCount; ++m) {
      int mIdx                    = fc.mesh(m);
      const ImageMeshP &childMesh = meshes[mIdx];

      Polygon *innerBorder =
          reduceBorder(borders[childMesh->edge(0).imageIndex()]);
      family->push_back(innerBorder);

      reduceBorders(tribe, reader, borders, mIdx, childMesh->face(0));
    }
  }

  // Recursive on the face's sub-mesh faces
  int m, mCount = fc.meshesCount();
  for (m = 0; m < mCount; ++m) {
    int mIdx = fc.mesh(m);
    reduceBorders(tribe, reader, borders, mIdx, meshes[mIdx]->face(0));
  }
}

//--------------------------------------------------------------------------

Tribe *extractBorders(const TRasterGR8P &ras) {
  Tribe *result = new Tribe;

  BordersReader reader;
  TRop::borders::readMeshes(ras, reader);

  reduceBorders(result, reader, reader.m_borders, -1, reader.outerFace());

  return result;
}

//======================================================================================
//    Maximum Edge Length  stuff
//======================================================================================

double buildMinimumEdgeLength(Tribe *tribe, int targetMaxVerticesCount) {
  // Calculate the tribe's total area
  double area = 0.0;

  Tribe::iterator ft, fEnd(tribe->end());
  for (ft = tribe->begin(); ft != fEnd; ++ft) {
    Family *fam = *ft;

    // Add area corresponding to the external border
    area += fabs(
        tcg::polyline_ops::area(fam->front()->begin(), fam->front()->end()));

    // Then, subtract all the internal ones
    Family::iterator pt, pEnd(fam->end());
    for (pt = ++fam->begin(); pt != pEnd; ++pt)
      area -= fabs(tcg::polyline_ops::area((*pt)->begin(), (*pt)->end()));
  }

  // Given the area, find the approximate edge length corresponding to the
  // required
  // vertices count

  // The idea is: given a polygon, its uniform triangular mesh - made of, say,
  // lots of equilateral
  // triangles, has vertices count proportional to ( polygon area / sq(mean mesh
  // edge length) ).

  // Under this assumption, take an equilater triangle as our polygon, and
  // assume that its built
  // mesh is the regular mesh obtained by subdividing it multiple times,
  // Sierpinsky-like.

  // Since it can easily be sheared to a corresponding half-square, it can be
  // SEEN that in
  // this case its vertices count is EXACTLY  (l / e_length)^2 / 2,  l being the
  // large
  // triangle's edge. By extension, if A is the area of the triangle, then we
  // have:

  //    A = l^2 * sqrt(3/2) / 2;      => l = sqrt(2 * K * A),     K = 1.0 /
  //    sqrt(3/2);
  //    V = K * A / e_length^2;       => e_length = sqrt(KA / V);

  // And we just extend this from 'triangle' to 'polygon'.

  return sqrt(area / (sqrt(1.5) * targetMaxVerticesCount));
}

//======================================================================================
//    Mesh Refinement  stuff
//======================================================================================

void refineMeshes(const TMeshImageP &mi, const MeshBuilderOptions &options) {
  std::vector<TTextureMeshP> &meshes = mi->meshes();

  // Refine every mesh to achieve a target mesh density dependent on the image
  // size
  /*TRectD bbox(mi->getBBox());
double targetLength = sqrt(bbox.getLx() * bbox.getLy()) * relativeMeshDensity;*/

  double targetLength = options.m_targetEdgeLength;

  int m, mCount = meshes.size();
  for (m = 0; m < mCount; ++m) {
    const TTextureMeshP &mesh = meshes[m];

    tcg::TriMeshStuff::DefaultEvaluator<TTextureMesh> eval(
        0.0, (std::numeric_limits<double>::max)());

    // First, perform edge swaps alone. This is useful since results from
    // gluTriangulate
    // tend to be unbalanced to vertical - this is a good correction.
    tcg::refineMesh(*mesh, eval);

    // Now, launch a full-scale, finishing simplification
    eval.m_collapseValue = targetLength * 0.6;
    eval.m_splitValue    = targetLength * 1.4;
    tcg::refineMesh(*mesh, eval,
                    20000);  // Max 10000 iterations - to avoid loops

    // Since we stopped at a max number of iterations, separate collapses from
    // splits
    // and simplify until the procedure stops.
    eval.m_collapseValue = 0.0;
    eval.m_splitValue    = 1.4 * targetLength;
    tcg::refineMesh(*mesh, eval);

    eval.m_splitValue    = (std::numeric_limits<double>::max)();
    eval.m_collapseValue = targetLength * 0.6;
    tcg::refineMesh(*mesh, eval);

    // Perform 1000 final iterations with uniform split and collapses
    eval.m_splitValue = targetLength * 1.4;
    tcg::refineMesh(*mesh, eval, 1000);

    // Finally, squeeze the mesh to ensure that VEF containers are tight
    mesh->squeeze();
  }
}

}  // namespace

//**************************************************************************************
//    Mesh Builder  function
//**************************************************************************************

TMeshImageP buildMesh(const TRasterP &ras, const MeshBuilderOptions &options) {
  // Convert the input image to a binary raster
  TRasterGR8P binaryRas = thresholdRaster(ras, options);

  // Extract the image borders
  Tribe *tribe = extractBorders(binaryRas);

  // Calculate maximum edge length
  double minEdgeLength =
      buildMinimumEdgeLength(tribe, options.m_targetMaxVerticesCount);

  MeshBuilderOptions opts(options);
  opts.m_targetEdgeLength = std::max(opts.m_targetEdgeLength, minEdgeLength);

  // Perform tessellation
  TMeshImageP meshImage(new TMeshImage);
  std::vector<TTextureMeshP> &meshes = meshImage->meshes();

  tcg::sequential_reader<std::vector<TTextureMeshP>> reader(&meshes);
  tcg::gluTriangulate(tribe->begin(), tribe->end(), reader);

  delete tribe;

  // Perform meshes refinement
  refineMeshes(meshImage, opts);

  return meshImage;
}
