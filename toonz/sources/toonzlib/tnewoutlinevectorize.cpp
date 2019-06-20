

// Toonz includes
#include "tgeometry.h"
#include "tpalette.h"
#include "tstroke.h"
#include "tregion.h"
#include "tcolorutils.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "trop.h"
#include "trop_borders.h"
#include "tstrokeutil.h"

// template includes
#define INCLUDE_HPP

// tcg includes
#include "tcg_wrap.h"
#include "tcg/tcg_vertex.h"
#include "tcg/tcg_edge.h"
#include "tcg/tcg_face.h"
#include "tcg/tcg_mesh.h"
#include "tcg/tcg_hash.h"
#include "tcg/tcg_polylineops.h"
#include "tcg/tcg_sequence_ops.h"
#include "tcg/tcg_cyclic.h"

// Toonz includes
#include "../common/trop/raster_edge_evaluator.hpp"

#undef INCLUDE_HPP

// STL includes
#include <set>

#include "toonz/tcenterlinevectorizer.h"

//************************************************************************
//    Local namespace stuff
//************************************************************************

namespace {

//===========================================================
//    Colors stuff
//===========================================================

template <typename Pix>
Pix transparent();

template <>
inline TPixel32 transparent<TPixel32>() {
  return TPixel32::Transparent;
}
template <>
inline TPixelGR16 transparent<TPixelGR16>() {
  return TPixelGR16::Black;
}

//===========================================================
//    Mesh stuff
//===========================================================

class Edge final : public tcg::Edge {
  TPoint m_dirs[2];
  TStroke *m_s;

public:
  Edge() : tcg::Edge(), m_s(0) {}
  Edge(int v1, const TPoint &dir1, int v2, const TPoint &dir2)
      : tcg::Edge(v1, v2), m_s(0) {
    m_dirs[0] = dir1, m_dirs[1] = dir2;
  }

  const TPoint &direction(int i) const { return m_dirs[i]; }
  TPoint &direction(int i) { return m_dirs[i]; }

  const TStroke *stroke() const { return m_s; }
  void setStroke(TStroke *s) { m_s = s; }
};

//-------------------------------------------------------------------

typedef tcg::Mesh<tcg::Vertex<TPoint>, Edge, tcg::Face> LocalMesh;

//-------------------------------------------------------------------

int hashPoint(const TPoint &point) { return point.x ^ point.y; }

typedef int (*PointHashType)(const TPoint &);

//-------------------------------------------------------------------

size_t hashStroke(const TStroke *stroke) { return (size_t)stroke; }

typedef size_t (*StrokeHashType)(const TStroke *);

//===========================================================
//    Vertex Adjustment
//===========================================================

struct Sums {
  double m_sums_x, m_sums_y;
  double m_sums2_x, m_sums2_y;
  double m_sums_xy;
};

//-------------------------------------------------------------------

struct SumsBuilder {
  const std::vector<double> &m_sums_x, &m_sums_y, &m_sums2_x, &m_sums2_y,
      &m_sums_xy;

  SumsBuilder(const std::vector<double> &sums_x,
              const std::vector<double> &sums_y,
              const std::vector<double> &sums2_x,
              const std::vector<double> &sums2_y,
              const std::vector<double> &sums_xy)
      : m_sums_x(sums_x)
      , m_sums_y(sums_y)
      , m_sums2_x(sums2_x)
      , m_sums2_y(sums2_y)
      , m_sums_xy(sums_xy) {}

  void build(Sums &sums, int idx0, int idx1) const {
    sums.m_sums_x  = m_sums_x[idx1] - m_sums_x[idx0];
    sums.m_sums_y  = m_sums_y[idx1] - m_sums_y[idx0];
    sums.m_sums2_x = m_sums2_x[idx1] - m_sums2_x[idx0];
    sums.m_sums2_y = m_sums2_y[idx1] - m_sums2_y[idx0];
    sums.m_sums_xy = m_sums_xy[idx1] - m_sums_xy[idx0];
  }
};

//-------------------------------------------------------------------

template <typename P1_type, typename P2_type>
void adjustVertex(const TPointD &origin, TPointD &point, P1_type p0,
                  Sums &sums0, int n0, P2_type p1, Sums &sums1, int n1) {
  // Find the 2 best-fit lines
  TPointD v0, v1;

  tcg::point_ops::bestFit(p0, v0, sums0.m_sums_x, sums0.m_sums_y,
                          sums0.m_sums2_x, sums0.m_sums2_y, sums0.m_sums_xy,
                          n0);

  tcg::point_ops::bestFit(p1, v1, sums1.m_sums_x, sums1.m_sums_y,
                          sums1.m_sums2_x, sums1.m_sums2_y, sums1.m_sums_xy,
                          n1);

  // Get the intersecting point
  double s, t;
  tcg::point_ops::intersectionCoords(p0, v0, p1, v1, s, t, 1e-3);

  if (s == tcg::numeric_ops::NaN<double>()) return;

  // Adjust vertex - inside a 0.5 radius disc
  TPointD newPoint(origin + p0 + s * v0), diff(newPoint - point);

  double distance = norm(diff);
  if (distance < 0.5)
    point = newPoint;
  else
    point += (0.5 / distance) * diff;
}

//-------------------------------------------------------------------

void adjustVertices(const TPointD &origin, std::vector<TPointD> &points,
                    const std::vector<int> &indices,
                    const std::vector<double> &sums_x,
                    const std::vector<double> &sums_y,
                    const std::vector<double> &sums2_x,
                    const std::vector<double> &sums2_y,
                    const std::vector<double> &sums_xy) {
  int i, last = points.size() - 1;

  SumsBuilder sumsBuilder(sums_x, sums_y, sums2_x, sums2_y, sums_xy);
  Sums sums0, sums1;
  int prev0, prev1, next0, next1;

  TPointD a0, a1;

  if (points.front() == points.back()) {
    prev0 = indices[last - 1] - 1, prev1 = indices[last],
    next0 = indices[0] - 1, next1 = indices[1];

    sumsBuilder.build(sums0, prev0, prev1);
    sumsBuilder.build(sums1, next0, next1);

    adjustVertex<TPointD &, TPointD &>(origin, points[0], a0, sums0,
                                       prev1 - prev0, a1, sums1, next1 - next0);

    points[last] = points[0];

    for (i = 1; i < last; ++i) {
      prev0 = indices[i - 1] - 1, prev1 = indices[i], next0 = indices[i] - 1,
      next1 = indices[i + 1];

      sumsBuilder.build(sums0, prev0, prev1);
      sumsBuilder.build(sums1, next0, next1);

      adjustVertex<TPointD &, TPointD &>(origin, points[i], a0, sums0,
                                         prev1 - prev0, a1, sums1,
                                         next1 - next0);
    }
  } else {
    prev0 = indices[0], prev1 = indices[1], next0 = indices[1] - 1,
    next1 = indices[2];

    sumsBuilder.build(sums0, prev0, prev1);
    sumsBuilder.build(sums1, next0, next1);

    a0 = points[0];

    adjustVertex<const TPointD &, TPointD &>(origin, points[1], a0, sums0,
                                             prev1 - prev0 + 1, a1, sums1,
                                             next1 - next0);

    int end = last - 1;
    for (i = 2; i < end; ++i) {
      prev0 = indices[i - 1] - 1, prev1 = indices[i], next0 = indices[i] - 1,
      next1 = indices[i + 1];

      sumsBuilder.build(sums0, prev0, prev1);
      sumsBuilder.build(sums1, next0, next1);

      adjustVertex<TPointD &, TPointD &>(origin, points[i], a0, sums0,
                                         prev1 - prev0, a1, sums1,
                                         next1 - next0);
    }

    prev0 = indices[end - 1], prev1 = indices[end], next0 = indices[end] - 1,
    next1 = indices[last];

    sumsBuilder.build(sums0, prev0, prev1);
    sumsBuilder.build(sums1, next0, next1);

    a1 = points[last];

    adjustVertex<const TPointD &, TPointD &>(
        origin, points[1], a1, sums1, next1 - next0, a0, sums0, prev1 - prev0);
  }
}

//************************************************************************
//    Borders Reader declaration
//************************************************************************

template <typename RanIt>
class PolylineReader {
  const NewOutlineConfiguration &m_conf;
  double m_adherenceTol, m_angleTol, m_relativeTol, m_mergeTol;

  TVectorImageP m_vi;

  std::vector<TPointD> m_points;
  std::vector<int> m_indices;

  const RasterEdgeEvaluator<RanIt> *m_eval;

public:
  PolylineReader(TVectorImageP vi, const NewOutlineConfiguration &conf)
      : m_vi(vi)
      , m_conf(conf)
      , m_adherenceTol(2.0 * (1.0 - m_conf.m_adherenceTol))
      , m_angleTol(cos(M_PI * m_conf.m_angleTol))
      , m_relativeTol(conf.m_relativeTol)
      , m_mergeTol(m_conf.m_mergeTol) {}

  void setEvaluator(const RasterEdgeEvaluator<RanIt> *eval) { m_eval = eval; }

  void openContainer(const RanIt &it);
  void addElement(const RanIt &it);
  void openContainer(const TPoint &p) { addElement(p); }
  void addElement(const TPoint &p) { m_points.push_back(TPointD(p.x, p.y)); }
  void closeContainer();
};

//************************************************************************
//    Borders Reader declaration
//************************************************************************

template <typename Pix>
class BordersReader final : public TRop::borders::BordersReader {
public:
  typedef Pix pixel_type;

  typedef TPoint point_type;
  typedef tcg::hash<TPoint, int, PointHashType> points_hash;

  typedef typename std::pair<pixel_type, pixel_type> stroke_colors_type;
  typedef
      typename tcg::hash<const TStroke *, stroke_colors_type, StrokeHashType>
          stroke_colors_hash;

  typedef typename std::vector<TPoint>::iterator point_iterator;
  typedef typename tcg::cyclic_iterator<point_iterator> cyclic_point_iterator;

private:
  const TRasterPT<pixel_type> &m_ras;
  int m_lx, m_ly, m_wrap;

  LocalMesh *m_mesh;
  PolylineReader<point_iterator> m_polylineReader;
  PolylineReader<cyclic_point_iterator> m_loopReader;
  TVectorImageP m_vi;

  points_hash m_vHash;

  pixel_type m_innerColor, m_outerColor;
  stroke_colors_hash m_scHash;

  // Current data
  TPoint m_pos;
  pixel_type *m_pix;
  std::vector<TPoint> m_points;

  // Last vertex data
  TPoint m_dir;
  int m_vIdx;
  int m_nDirections;

  // First vertex data
  TPoint m_firstPos, m_firstDir, m_firstOppDir;
  int m_firstVIdx;
  int m_firstNDirections;
  std::vector<TPoint> m_firstPoints;

public:
  BordersReader(const TRasterPT<pixel_type> &ras, LocalMesh *mesh,
                TVectorImageP vi, const NewOutlineConfiguration &conf)
      : m_mesh(mesh)
      , m_polylineReader(vi, conf)
      , m_loopReader(vi, conf)
      , m_vi(vi)
      , m_vHash(&hashPoint)
      , m_scHash(&hashStroke)
      , m_ras(ras)
      , m_lx(ras->getLx())
      , m_ly(ras->getLy())
      , m_wrap(ras->getWrap()) {}

  void openContainer(const TPoint &pos, const TPoint &dir,
                     const pixel_type &innerColor,
                     const pixel_type &outerColor) override;
  void addElement(const TPoint &pos, const TPoint &dir,
                  const pixel_type &outerColor) override;
  void closeContainer() override;

  int surroundingEdges();

  int touchVertex(const TPoint &pos);
  void touchEdge(int v0, const TPoint &d0, int nd0, int v1, const TPoint &d1,
                 int nd1);

  const stroke_colors_hash &scHash() const { return m_scHash; }
};

}  // namespace

//************************************************************************
//    BordersReader implementation
//************************************************************************

template <typename Pix>
int ::BordersReader<Pix>::surroundingEdges() {
  static const Pix transp(transparent<Pix>());

  Pix ll((m_pos.x > 0 && m_pos.y > 0) ? *(m_pix - m_wrap - 1) : transp);
  Pix lr((m_pos.x < m_lx && m_pos.y > 0) ? *(m_pix - m_wrap) : transp);
  Pix ul((m_pos.x > 0 && m_pos.y < m_ly) ? *(m_pix - 1) : transp);
  Pix ur((m_pos.x < m_lx && m_pos.y < m_ly) ? *(m_pix) : transp);

  if (ll == ur || lr == ul) return 2;

  int nEquals =
      (int)(ll == lr) + (int)(lr == ur) + (int)(ur == ul) + (int)(ul == ll);
  return 4 - nEquals;
}

//-------------------------------------------------------------------

template <typename Pix>
int ::BordersReader<Pix>::touchVertex(const TPoint &pos) {
  points_hash::iterator it = m_vHash.find(pos);
  if (it == m_vHash.end())
    // No vertex found, add it now.
    it = m_vHash.insert(m_pos, m_mesh->addVertex(LocalMesh::vertex_type(pos)));

  return it->m_val;
}

//-------------------------------------------------------------------

template <typename Pix>
void ::BordersReader<Pix>::touchEdge(int v0, const TPoint &d0, int nd0, int v1,
                                     const TPoint &d1, int nd1) {
  typedef tcg::vertex_traits<LocalMesh::vertex_type>::edges_const_iterator
      edge_const_it;

  // Ensure that an associated edge is present, in case it should
  const LocalMesh::vertex_type &vx0 = m_mesh->vertex(v0);
  const LocalMesh::vertex_type &vx1 = m_mesh->vertex(v1);

  // Peek each vertex edge, for the one with the right direction
  int e;

  edge_const_it it, end = vx1.edgesEnd();
  for (it = vx1.edgesBegin(); it != end; ++it) {
    Edge &ed = m_mesh->edge(*it);
    int side = (int)(ed.vertex(1) == v1);

    if (ed.direction(side) == d1) {
      e = ed.getIndex();
      break;
    }
  }

  // If none was found, the edge must be added
  if (it == end) {
    e = m_mesh->addEdge(Edge(v0, d0, v1, d1));

    // Also, insert it in the output vector image
    if (m_points.size() == 2) {
      m_polylineReader.openContainer(m_points[0]);
      m_polylineReader.addElement(m_points[1]);
      m_polylineReader.closeContainer();
    } else {
      if (m_points.front() == m_points.back()) {
        point_iterator b = m_points.begin(), e = m_points.end() - 1;
        cyclic_point_iterator beginC(b, b, e, 0), endC(b + 1, b, e, 1);

        RasterEdgeEvaluator<cyclic_point_iterator> eval(
            beginC - 1, endC + 1, 1.0, (std::numeric_limits<double>::max)());
        m_loopReader.setEvaluator(&eval);

        tcg::sequence_ops::minimalPath(beginC, endC, eval, m_loopReader);
      } else {
        RasterEdgeEvaluator<point_iterator> eval(
            m_points.begin(), m_points.end(), 1.0,
            (std::numeric_limits<double>::max)());
        m_polylineReader.setEvaluator(&eval);

        tcg::sequence_ops::minimalPath(m_points.begin(), m_points.end(), eval,
                                       m_polylineReader);
      }
    }

    Edge &ed = m_mesh->edge(e);
    ed.setStroke(m_vi->getStroke(m_vi->getStrokeCount() - 1));

    // Also, associate the extracted colors to the built stroke.
    stroke_colors_type &colors = m_scHash[ed.stroke()];
    colors.first               = m_outerColor;
    colors.second              = m_innerColor;
  }

  // Finally, if the number of each vertex's incident edges has been reached,
  // erase the corresponding hash entry.
  /*{
if(nd0 == vx0.edgesCount())
m_vHash.erase(vx0.P());

if(nd1 == vx1.edgesCount())
m_vHash.erase(vx1.P());
}*/
}

//-------------------------------------------------------------------

template <typename Pix>
void ::BordersReader<Pix>::openContainer(const TPoint &pos, const TPoint &dir,
                                         const pixel_type &innerColor,
                                         const pixel_type &outerColor) {
  // Store the associated color if not already present
  m_innerColor = innerColor;
  m_outerColor = outerColor;

  // Build the initial pixel
  m_pos = pos;
  m_pix = m_ras->pixels(0) + m_ras->getWrap() * m_pos.y + m_pos.x;
  m_points.push_back(m_pos);

  m_dir         = dir;
  m_vIdx        = -1;
  m_nDirections = surroundingEdges();

  m_firstPos         = m_pos;
  m_firstDir         = m_dir;
  m_firstVIdx        = -1;
  m_firstNDirections = m_nDirections;
  m_firstOppDir      = TPoint(1, 0);

  if (m_nDirections > 2) {
    // Found mesh vertex. Retrieve the associated vertex
    m_vIdx        = touchVertex(m_pos);
    m_firstVIdx   = m_vIdx;
    m_firstPoints = m_points;
  }
}

//-------------------------------------------------------------------

template <typename Pix>
void ::BordersReader<Pix>::addElement(const TPoint &pos, const TPoint &dir,
                                      const Pix &outerColor) {
  // Build opposite direction
  bool horizontal = (pos.y == m_pos.y);
  TPoint oppDir   = horizontal ? TPoint((pos.x > m_pos.x) ? -1 : 1, 0)
                             : TPoint(0, (pos.y > m_pos.y) ? -1 : 1);

  // Update position
  m_pix += horizontal ? (pos.x - m_pos.x) : (pos.y - m_pos.y) * m_wrap;
  m_pos = pos;
  m_points.push_back(m_pos);

  // Check the new pos
  int nDirections = surroundingEdges();
  if (nDirections > 2) {
    // Found mesh vertex. First, check the hash for an associated vertex
    int vIdx = touchVertex(m_pos);

    // Ensure that an associated edge is present, in case it should
    if (m_vIdx >= 0)
      touchEdge(m_vIdx, m_dir, m_nDirections, vIdx, oppDir, nDirections);
    else {
      m_firstPos         = m_pos;
      m_firstDir         = dir;
      m_firstOppDir      = oppDir;
      m_firstVIdx        = vIdx;
      m_firstNDirections = nDirections;
      m_firstPoints      = m_points;
    }

    m_dir         = dir;
    m_vIdx        = vIdx;
    m_nDirections = nDirections;
    m_outerColor  = outerColor;

    m_points.clear();
    m_points.push_back(m_pos);
  }
}

//-------------------------------------------------------------------

template <typename Pix>
void ::BordersReader<Pix>::closeContainer() {
  // If no vertex was found, build one on the first position.
  if (m_firstVIdx < 0) {
    // Add a vertex at the first position.
    m_firstVIdx = m_vIdx = touchVertex(m_firstPos);
    m_dir                = m_firstDir;
    m_nDirections        = m_firstNDirections;
    m_firstPoints.push_back(m_firstPos);
  }

  // Connect the last vertex to the first.
  m_points.insert(m_points.end(), m_firstPoints.begin(), m_firstPoints.end());
  touchEdge(m_vIdx, m_dir, m_nDirections, m_firstVIdx, m_firstOppDir,
            m_firstNDirections);

  m_points.clear();
  m_firstPoints.clear();
}

//===================================================================

template <typename RanIt>
void ::PolylineReader<RanIt>::openContainer(const RanIt &it) {
  const TPoint &p = *it;
  m_points.push_back(TPointD(p.x, p.y));
  m_indices.push_back(it - m_eval->begin());
}

//-------------------------------------------------------------------

template <typename RanIt>
void ::PolylineReader<RanIt>::addElement(const RanIt &it) {
  const TPoint &p = *it;
  m_points.push_back(TPointD(p.x, p.y));
  m_indices.push_back(it - m_eval->begin());
}

//-------------------------------------------------------------------

template <typename RanIt>
void ::PolylineReader<RanIt>::closeContainer() {
  if (!m_indices.empty()) {
    const TPoint &origI(*m_eval->begin());
    TPointD origin(origI.x, origI.y);

    ::adjustVertices(origin, m_points, m_indices, m_eval->sums_x(),
                     m_eval->sums_y(), m_eval->sums2_x(), m_eval->sums2_y(),
                     m_eval->sums_xy());
  }

  std::vector<TThickPoint> cps;
  polylineToQuadratics(m_points, cps, m_adherenceTol, m_angleTol, m_relativeTol,
                       0.75, m_mergeTol);

  m_vi->addStroke(new TStroke(cps));

  m_points.clear();
  m_indices.clear();
}

//************************************************************************
//    Palette functions
//************************************************************************

namespace {

void discretizeColors(TRaster32P &ras, TPalette *palette, int nColors,
                      TPixel32 transparentColor) {
  // Extract the palette
  std::set<TPixel32> colors;
  TColorUtils::buildPalette(colors, ras, nColors);
  colors.erase(TPixel::Black);  // Black is automatically inserted by TPalette's
                                // constructor

  std::set<TPixel32>::const_iterator it = colors.begin();
  for (; it != colors.end(); ++it) palette->getPage(0)->addStyle(*it);

  // Flatten ras to the specified palette
  TPixel32 *pix, *line, *lineEnd;
  int y, lx = ras->getLx(), ly = ras->getLy();
  for (y = 0; y < ly; ++y) {
    line = ras->pixels(y), lineEnd = line + lx;
    for (pix = line; pix < lineEnd; ++pix)
      *pix = (*pix == transparentColor)
                 ? TPixel32::Transparent
                 : palette->getStyle(palette->getClosestStyle(*pix))
                       ->getMainColor();
  }
}

//===================================================================

void copyCM(TRasterGR16P &dst, const TRasterCM32P &src, int toneTol) {
  assert(dst->getLx() == src->getLx() && dst->getLy() == src->getLy());

  int y, lx = src->getLx(), ly = src->getLy();
  TPixelCM32 *pixIn, *lineInEnd;
  TPixelGR16 *pixOut;
  for (y = 0; y < ly; ++y) {
    pixIn = src->pixels(y), lineInEnd = pixIn + lx;
    pixOut = dst->pixels(y);
    for (; pixIn < lineInEnd; ++pixIn, ++pixOut)
      pixOut->value =
          (pixIn->getTone() < toneTol) ? pixIn->getInk() : pixIn->getPaint();
  }
}

//===================================================================

typedef BordersReader<TPixel32>::stroke_colors_type stroke_colors_typeRGBM;
typedef BordersReader<TPixel32>::stroke_colors_hash stroke_colors_hashRGBM;

typedef BordersReader<TPixelGR16>::stroke_colors_type stroke_colors_typeCM;
typedef BordersReader<TPixelGR16>::stroke_colors_hash stroke_colors_hashCM;

//===================================================================

void buildColorsRGBM(TRegion *r, const stroke_colors_hashRGBM &scHash,
                     const TPaletteP palette) {
  // Build r's color
  UINT i, edgeCount = r->getEdgeCount();
  for (i = 0; i < edgeCount; ++i) {
    TEdge *ed = r->getEdge(i);

    stroke_colors_hashRGBM::const_iterator it = scHash.find(ed->m_s);
    if (it == scHash.end()) continue;

    const stroke_colors_typeRGBM &colors = it->m_val;

    int style;
    if (ed->m_w0 < ed->m_w1) {
      style = palette->getClosestStyle(colors.first);
      ed->setStyle(style);
      ed->m_s->setStyle(style ? style
                              : palette->getClosestStyle(colors.second));
    } else {
      style = palette->getClosestStyle(colors.second);
      ed->setStyle(style);
      ed->m_s->setStyle(style ? style : palette->getClosestStyle(colors.first));
    }
  }

  // Build the color for its sub-regions
  int j, rCount = r->getSubregionCount();
  for (j = 0; j < rCount; ++j)
    buildColorsRGBM(r->getSubregion(j), scHash, palette);
}

//-------------------------------------------------------------------

void buildColorsRGBM(TVectorImageP vi, const stroke_colors_hashRGBM &scHash) {
  // For every region, find its color
  int i, rCount = vi->getRegionCount();
  for (i = 0; i < rCount; ++i)
    buildColorsRGBM(vi->getRegion(i), scHash, vi->getPalette());
}

//-------------------------------------------------------------------

void buildColorsCM(TRegion *r, const stroke_colors_hashCM &scHash) {
  // Build r's color
  UINT i, edgeCount = r->getEdgeCount();
  for (i = 0; i < edgeCount; ++i) {
    TEdge *ed = r->getEdge(i);

    stroke_colors_hashCM::const_iterator it = scHash.find(ed->m_s);
    if (it == scHash.end()) continue;

    const stroke_colors_typeCM &colors = it->m_val;
    if (ed->m_w0 < ed->m_w1)
      ed->setStyle(colors.first.value);
    else
      ed->setStyle(colors.second.value);

    ed->m_s->setStyle(colors.first.value ? colors.first.value
                                         : colors.second.value);
  }

  // Build the color for its sub-regions
  int j, rCount = r->getSubregionCount();
  for (j = 0; j < rCount; ++j) buildColorsCM(r->getSubregion(j), scHash);
}

//-------------------------------------------------------------------

void buildColorsCM(TVectorImageP vi, const stroke_colors_hashCM &scHash) {
  // For every region, find its color
  int i, rCount = vi->getRegionCount();
  for (i = 0; i < rCount; ++i) buildColorsCM(vi->getRegion(i), scHash);
}

}  // namespace

//************************************************************************
//    Main functions
//************************************************************************

namespace {

void outlineVectorize(TVectorImageP &vi, const TRasterImageP &ri,
                      const NewOutlineConfiguration &conf, TPalette *palette) {
  // Make a copy of ri's raster - a 32-bit raster
  TRasterP ras(ri->getRaster());
  TRaster32P ras32(ras->getSize());

  TRop::copy(ras32, ras);

  // Build palette color and discretize the raster
  discretizeColors(ras32, palette, conf.m_maxColors, conf.m_transparentColor);

  // Perform despeckling
  if (conf.m_despeckling > 0)
    TRop::majorityDespeckle(ras32, conf.m_despeckling);

  // Examinate the discretized raster. Build a mesh structure representing the
  // image's
  // colors geometry. Build strokes as mesh edges are extracted.
  LocalMesh mesh;
  BordersReader<TPixel32> reader(ras32, &mesh, vi, conf);

  TRop::borders::readBorders_simple(ras32, reader, false);

  // Build regions
  vi->transform(conf.m_affine);
  vi->findRegions();

  if (!conf.m_leaveUnpainted)
    // Finally, build region colors.
    buildColorsRGBM(vi, reader.scHash());
}

//-------------------------------------------------------------------

void outlineVectorize(TVectorImageP &vi, const TToonzImageP &ti,
                      const NewOutlineConfiguration &conf, TPalette *palette) {
  TRasterCM32P ras(ti->getRaster());
  TRasterGR16P rasGR16(ras->getSize());

  ::copyCM(rasGR16, ras, conf.m_toneTol);

  // Perform despeckling
  if (conf.m_despeckling > 0)
    TRop::majorityDespeckle(rasGR16, conf.m_despeckling);

  LocalMesh mesh;
  BordersReader<TPixelGR16> reader(rasGR16, &mesh, vi, conf);

  TRop::borders::readBorders_simple(rasGR16, reader, TPixelGR16::Black, false);

  vi->transform(conf.m_affine);
  vi->findRegions();

  if (!conf.m_leaveUnpainted) buildColorsCM(vi, reader.scHash());
}

}  // namespace

//-------------------------------------------------------------------

TVectorImageP VectorizerCore::newOutlineVectorize(
    const TImageP &image, const NewOutlineConfiguration &conf,
    TPalette *palette) {
  TVectorImageP output(new TVectorImage);
  output->setPalette(palette);

  TRasterImageP ri(image);
  TToonzImageP ti(image);

  // Deal with palette (observe that if image is colormap, the input palette is
  // directly copied to output)
  if (ri)
    ::outlineVectorize(output, ri, conf, palette);
  else if (ti)
    ::outlineVectorize(output, ti, conf, palette);
  else
    assert(false);

  return output;
}
