#pragma once

#ifndef BORDERS_EXTRACTOR_HPP
#define BORDERS_EXTRACTOR_HPP

// Toonz includes
#include "raster_edge_iterator.h"

// tcg includes
#include "tcg/tcg_traits.h"
#include "tcg/tcg_containers_reader.h"
#include "tcg/tcg_hash.h"
#include "tcg/tcg_misc.h"

#include "borders_extractor.h"

namespace TRop {
namespace borders {

//*********************************************************************************************************
//    Private stuff
//*********************************************************************************************************

template <typename PixelSelector>
class _DummyReader {
public:
  void openContainer(const RasterEdgeIterator<PixelSelector> &) {}
  void addElement(const RasterEdgeIterator<PixelSelector> &) {}
  void closeContainer() {}
};

//*********************************************************************************************************
//    Borders Extraction procedure
//*********************************************************************************************************

inline void _signEdge(RunsMapP &runsMap, int x, int y0, int y1,
                      UCHAR increasingSign, UCHAR decreasingSign) {
  for (; y0 < y1; ++y0) runsMap->runHeader(x, y0) |= increasingSign;

  if (y0 > y1) {
    --x;
    do {
      --y0;
      runsMap->runHeader(x, y0) |= decreasingSign;
    } while (y0 > y1);
  }
}

//---------------------------------------------------------------------------------------------

template <typename Pixel, typename PixelSelector, typename ContainerReader>
void _readBorder(const TRasterPT<Pixel> &rin, const PixelSelector &selector,
                 RunsMapP &runsMap, int x, int y, bool counter,
                 ContainerReader &reader) {
  typedef typename PixelSelector::value_type value_type;

  UCHAR increasingSign = _BORDER_LEFT, decreasingSign = _BORDER_RIGHT;
  if (!counter)
    increasingSign |= _HIERARCHY_INCREASE,
        decreasingSign |= _HIERARCHY_DECREASE;

  // First, read the border entirely, while erasing the border from
  // the runsMap.
  RasterEdgeIterator<PixelSelector> it(rin, selector, TPoint(x, y),
                                       counter ? TPoint(1, 0) : TPoint(0, 1));
  //++it;   //As we could be in the middle of a straight edge, increment to get
  // a corner

  TPoint start(it.pos()), startDir(it.dir());
  reader.openContainer(it);

  TPoint oldPos(start);
  for (++it; it.pos() != start || it.dir() != startDir; ++it) {
    const TPoint &currPos(it.pos());
    reader.addElement(it);

    // Sign the corresponding (vertical) edge
    _signEdge(runsMap, oldPos.x, oldPos.y, currPos.y, increasingSign,
              decreasingSign);

    oldPos = currPos;
  }

  _signEdge(runsMap, oldPos.x, oldPos.y, it.pos().y, increasingSign,
            decreasingSign);

  reader.closeContainer();
}

//---------------------------------------------------------------------------------------------

template <typename Pixel, typename PixelSelector, typename ContainerReader>
void readBorders(const TRasterPT<Pixel> &rin, const PixelSelector &selector,
                 ContainerReader &reader, RunsMapP *rasterRunsMap) {
  typedef TRasterPT<Pixel> RasterTypeP;
  typedef _DummyReader<PixelSelector> DummyReader;

  // First, extract the run-length representation for rin
  RunsMapP runsMap;
  if (rasterRunsMap && *rasterRunsMap) {
    // If it was supplied, use it
    runsMap = *rasterRunsMap;
    runsMap->lock();
  } else {
    // In case, build it anew
    runsMap = RunsMapP(rin->getLx(), rin->getLy());

    runsMap->lock();
    buildRunsMap(runsMap, rin, selector);
  }

  if (rasterRunsMap)
    // Return the runsMap if requested
    *rasterRunsMap = runsMap;

  // Build a fake reader for internal borders
  DummyReader dummyReader;

  // Now, use it to extract borders - iterate through runs and, whenever
  // one is found with opaque color (ie not transparent), extract its
  // associated border. The border is erased internally after the read.
  int lx = rin->getLx(), ly = rin->getLy();

  int hierarchyLevel = 0;

  int x, y;
  for (y = 0; y < ly; ++y) {
    Pixel *lineStart     = rin->pixels(y), *pix;
    TPixelGR8 *runsStart = runsMap->pixels(y), *run;

    UCHAR nextHeader, prevHeader = 0;
    for (x = 0, pix = lineStart, run = runsStart; x < lx;) {
      nextHeader = run->value;

      if (hierarchyLevel) {
        if (prevHeader & _BORDER_RIGHT) {
          if (prevHeader & _HIERARCHY_DECREASE) --hierarchyLevel;
        } else  // Every right border in a region should be signed. Do so now.
          _readBorder(rin, selector, runsMap, x, y, true, dummyReader);
      }

      if (hierarchyLevel) {
        if (nextHeader & _BORDER_LEFT) {
          if (nextHeader & _HIERARCHY_INCREASE) ++hierarchyLevel;
        } else {
          ++hierarchyLevel;
          _readBorder(rin, selector, runsMap, x, y, false, reader);
        }
      } else {
        if (!(selector.transparent(
                *pix)))  // External transparent region - do not extract
        {
          ++hierarchyLevel;
          if (!(nextHeader & _BORDER_LEFT))
            _readBorder(rin, selector, runsMap, x, y, false, reader);
        }
      }

      // Increment variables
      x += runsMap->runLength(x, y), pix = lineStart + x, run = runsStart + x;
      prevHeader = (run - 1)->value;
    }

    assert(x == lx);

    if (hierarchyLevel) {
      assert((prevHeader & _BORDER_RIGHT) &&
             (prevHeader & _HIERARCHY_DECREASE));
      --hierarchyLevel;
    }

    assert(!hierarchyLevel);
  }

  runsMap->unlock();
}

//*********************************************************************************************************
//    New Mesh Extraction procedure
//*********************************************************************************************************

enum {
  _PROCESSED                  = 0x1,
  _HIERARCHY_UP               = 0x2,
  _HIERARCHY_DN               = 0x4,
  _PROCESSED_AND_HIERARCHY_UP = (_PROCESSED | _HIERARCHY_UP)
};

//-------------------------------------------------------------------

template <typename RasterEdgeIter>
inline bool _isVertex(
    const RasterEdgeIter &it,
    const typename RasterEdgeIter::value_type &oldOtherColor) {
  return (it.otherColor() != oldOtherColor) ||
         (it.turn() == it.adherence() &&
          (!(it.turn() & RasterEdgeIter::AMBIGUOUS)) &&
          it.elbowColor() != oldOtherColor);
}

//-------------------------------------------------------------------

inline size_t _pointHash(const TPoint &point) { return point.x ^ point.y; }

//-------------------------------------------------------------------

template <typename RasterEdgeIter>
struct _ExternalEdgeSigner {
  static inline void signAndIncrement(RunsMapP &runsMap, RasterEdgeIter &it) {
    if (it.dir().y > 0) {
      TPoint pos = it.pos();
      int newY   = (++it).pos().y;

      for (; pos.y != newY; ++pos.y)
        runsMap->runHeader(pos.x, pos.y) |= _PROCESSED_AND_HIERARCHY_UP;
    } else if (it.dir().y < 0) {
      TPoint pos = it.pos();
      int newY   = (++it).pos().y;

      TPixelGR8 *pix = runsMap->pixels(pos.y - 1) + pos.x;

      for (; pos.y != newY; --pos.y, --pix) {
        pix->value |= _PROCESSED;
        (--pix)->value |= _HIERARCHY_DN;
      }
    } else
      ++it;
  }
};

//-------------------------------------------------------------------

template <typename RasterEdgeIter>
struct _InternalEdgeSigner {
  static inline void signAndIncrement(RunsMapP &runsMap, RasterEdgeIter &it) {
    if (it.dir().y) {
      TPoint pos = it.pos();
      int newY   = (++it).pos().y;
      int dir    = it.dir().y;

      TPixelGR8 *pix =
          runsMap->pixels((it.dir().y > 0) ? pos.y : pos.y - 1) + pos.x;

      for (; pos.y != newY; pos.y += dir, pix += dir) {
        pix->value |= _PROCESSED_AND_HIERARCHY_UP;
        (--pix)->value |= _HIERARCHY_DN;
      }
    } else
      ++it;
  }
};

//-------------------------------------------------------------------

template <typename RasterEdgeIter, typename Mesh, typename ContainersReader,
          typename EdgeSigner>
int _readEdge(RasterEdgeIter &it, const RasterEdgeIter &end, RunsMapP runsMap,
              int &vIdx, Mesh &mesh, tcg::hash<TPoint, int> &pointsHash,
              ContainersReader &reader) {
  typedef tcg::container_reader_traits<ContainersReader,
                                       typename Mesh::edge_type>
      edge_output;

  typename Mesh::edge_type ed;

  ed.addVertex(vIdx);
  ed.direction(0) = it.dir();

  edge_output::openContainer(reader, it);

  typename RasterEdgeIter::value_type oldOtherColor = it.otherColor();
  do {
    EdgeSigner::signAndIncrement(runsMap, it);
    edge_output::addElement(reader, it);

  } while ((it != end) && !_isVertex(it, oldOtherColor));

  // Identify the newly found vertex. If it's a brand new one, add it
  tcg::hash<TPoint, int>::iterator ht = pointsHash.find(it.pos());
  vIdx = (ht == pointsHash.end())
             ? pointsHash[it.pos()] =
                   mesh.addVertex(typename Mesh::vertex_type(it.pos()))
             : ht.m_idx;

  ed.addVertex(vIdx);
  ed.direction(1) = (it.turn() == RasterEdgeIter::STRAIGHT)
                        ? -it.dir()
                        : (it.turn() == RasterEdgeIter::LEFT)
                              ? tcg::point_ops::ortLeft(it.dir())
                              : tcg::point_ops::ortRight(it.dir());

  int eIdx = mesh.addEdge(ed);
  edge_output::closeContainer(reader, &mesh, eIdx);

  return eIdx;
}

//---------------------------------------------------------------------------------------------

template <typename RasterEdgeIter, typename Mesh, typename ContainersReader>
void _readMeshes(const RasterEdgeIter &begin, RunsMapP &runsMap,
                 ContainersReader &reader) {
  typedef tcg::container_reader_traits<ContainersReader,
                                       typename Mesh::face_type>
      face_output;

  // Iterate it clockwise. Process lines with vertical displacement. In each
  // line, search
  // for unprocessed raster edges.

  // Use hierarchy signs in the runsMap to understand the search scope in this
  // sub-region.
  int hierarchyLevel = 0;

  RasterEdgeIter it(begin);
  do {
    if (it.dir().y > 0) {
      // Process line
      TPoint pos           = it.pos();
      const TPixelGR8 *pix = runsMap->pixels(pos.y) + pos.x;

      hierarchyLevel = 0;
      assert((pix->value & _PROCESSED) && (pix->value & _HIERARCHY_UP));

      do {
        // Iterate through the line. Extract a mesh each time an unprocessed
        // raster edge is found.
        if (!(pix->value & _PROCESSED)) {
          assert(hierarchyLevel == 1);

          Mesh *meshPtr = new Mesh;
          _readMesh(it.raster(), it.selector(), runsMap, pos.x, pos.y, *meshPtr,
                    reader);

          face_output::addElement(reader, meshPtr);
        }

        if (pix->value & _HIERARCHY_UP) ++hierarchyLevel;

        TUINT32 l = runsMap->runLength(pos.x, pos.y);
        pos.x += l;
        pix += l;

        if ((pix - 1)->value & _HIERARCHY_DN) --hierarchyLevel;

      } while (hierarchyLevel > 0);
    }

    ++it;

  } while (it != begin);
}

//-------------------------------------------------------------------

template <typename RasterEdgeIter, typename Mesh, typename ContainersReader>
void _readBorder(const RasterEdgeIter &begin, RunsMapP runsMap, int vIdx,
                 Mesh &mesh, tcg::hash<TPoint, int> &pointsHash,
                 ContainersReader &reader) {
  typedef typename Mesh::face_type face_type;
  typedef typename Mesh::edge_type edge_type;
  typedef tcg::container_reader_traits<ContainersReader,
                                       typename Mesh::face_type>
      face_output;

  // As long as we don't get back to the initial iterator, extract edges
  RasterEdgeIter it(begin);

  // Create the face to be extracted at the right of processed border, and add
  // it to
  // the mesh. Observe that insertion is made manually in the mesh's faces list.
  // This prevents the mesh from automatically link edges to the face.
  face_type fc;

  do {
    // Retrieve current vertex
    typename Mesh::vertex_type &vx = mesh.vertex(vIdx);

    // Search in it the edge corresponding to current iterating direction
    int e, edgesCount = vx.edgesCount(), eIdx = -1, side = -1;
    for (e = 0; e < edgesCount; ++e) {
      edge_type &ed = mesh.edge(vx.edge(e));
      side          = ed.vertex(0) == vIdx ? 0 : 1;

      if (ed.direction(side) == it.dir()) {
        eIdx = ed.getIndex();
        break;
      }
    }

    if (e == edgesCount) {
      // In case the edge was not found, it needs to be extracted now.
      eIdx = _readEdge<RasterEdgeIter, Mesh, ContainersReader,
                       _InternalEdgeSigner<RasterEdgeIter>>(
          it, begin, runsMap, vIdx, mesh, pointsHash, reader);
    } else {
      // The edge was already extracted. We just need to update the iterator
      // then.
      const edge_type &ed = mesh.edge(eIdx);

      vIdx                      = ed.vertex(1 - side);
      const TPoint &oppositePos = mesh.vertex(vIdx).P();
      const TPoint &oppositeDir = ed.direction(1 - side);

      // We need to perform the last ++it in the already extracted edge since we
      // need
      // to give it the incoming direction.
      it.setEdge(oppositePos + oppositeDir, -oppositeDir), ++it;
    }

    fc.addEdge(eIdx);

  } while (it != begin);

  // The face has now been described (from the mesh viewpoint). Add it to the
  // mesh.
  int fIdx = mesh.addFace(fc);

  // We still need to extract its sub-meshes content now.
  face_output::openContainer(reader, &mesh, fIdx, begin.rightColor());

  _readMeshes<RasterEdgeIter, Mesh, ContainersReader>(begin, runsMap, reader);

  face_output::closeContainer(reader);
}

//-------------------------------------------------------------------

template <typename PixelSelector, typename Mesh, typename ContainersReader>
void _readMesh(const TRasterPT<typename PixelSelector::pixel_type> &rin,
               const PixelSelector &selector, RunsMapP &runsMap, int x, int y,
               Mesh &mesh, ContainersReader &reader) {
  typedef typename Mesh::vertex_type vertex_type;
  typedef typename Mesh::edge_type edge_type;
  typedef tcg::container_reader_traits<ContainersReader, edge_type> edge_output;
  typedef typename PixelSelector::value_type value_type;
  typedef RasterEdgeIterator<PixelSelector> raster_edge_iterator;

  // Iterate silently until a vertex is encountered (or until the initial point
  // is found)
  raster_edge_iterator it(rin, selector, TPoint(x, y), TPoint(0, 1)), begin(it);
  it.setAdherence(raster_edge_iterator::LEFT);

  value_type beginColor = begin.rightColor();
  for (++it; it != begin && !_isVertex(it, beginColor); ++it)
    ;

  // Use a hash to keep track of found vertices
  tcg::hash<TPoint, int> pointsHash(&_pointHash);

  int vIdx = pointsHash[it.pos()] = mesh.addVertex(it.pos());

  // The outer edges are extracted first in clockwise orientation.
  begin = it;
  do {
    _readEdge<raster_edge_iterator, Mesh, ContainersReader,
              _ExternalEdgeSigner<raster_edge_iterator>>(
        it, begin, runsMap, vIdx, mesh, pointsHash, reader);

  } while (it != begin);

  // Then, their associated faces are extracted.
  it.setAdherence(raster_edge_iterator::RIGHT);

  int e, outerEdgesCount = mesh.edgesCount();
  for (e = 0; e < outerEdgesCount; ++e) {
    const edge_type &ed = mesh.edge(e);
    if (ed.face(0) < 0) {
      vIdx                  = ed.vertex(0);
      const vertex_type &vx = mesh.vertex(vIdx);

      it.setEdge(vx.P(), ed.direction(0));
      _readBorder(it, runsMap, vIdx, mesh, pointsHash, reader);
    }
  }

  // Edges following those must have either side associated with a face.
  // Which must be extracted too.
  for (e = outerEdgesCount; e < mesh.edgesCount(); ++e) {
    const edge_type &ed = mesh.edge(e);
    if (ed.face(1) < 0) {
      vIdx                  = ed.vertex(1);
      const vertex_type &vx = mesh.vertex(vIdx);

      it.setEdge(vx.P(), ed.direction(1));
      _readBorder(it, runsMap, vIdx, mesh, pointsHash, reader);
    }
  }
}

//---------------------------------------------------------------------------------------------

template <typename PixelSelector, typename Mesh, typename ContainersReader>
void readMeshes(const TRasterPT<typename PixelSelector::pixel_type> &rin,
                const PixelSelector &selector, ContainersReader &reader,
                RunsMapP *rasterRunsMap) {
  typedef typename PixelSelector::pixel_type pixel_type;
  typedef TRasterPT<pixel_type> RasterTypeP;
  typedef Mesh mesh_type;
  typedef tcg::container_reader_traits<ContainersReader,
                                       typename Mesh::face_type>
      face_output;

  // First, extract the run-length representation for rin
  RunsMapP runsMap;
  if (rasterRunsMap && *rasterRunsMap) {
    // If a runsmap was supplied, use it
    runsMap = *rasterRunsMap;
    runsMap->lock();

    assert((runsMap->getLx() == rin->getLx() + 1) &&
           (runsMap->getLy() == rin->getLy()));
  } else {
    // In case, build it anew
    runsMap = RunsMapP(rin->getLx() + 1, rin->getLy());

    // Observe the +1 on the x-axis. One additional pixel is currently required
    // on the right
    // side of the runsmap - for ease of use in the algorithm.

    runsMap->lock();
    buildRunsMap(runsMap, rin, selector);
  }

  if (rasterRunsMap)
    // Return the runsMap if requested
    *rasterRunsMap = runsMap;

  face_output::openContainer(reader, 0, -1, selector.transparent());

  // Now, use it to extract borders - iterate through runs and, whenever
  // one is found with opaque color (ie not transparent), extract its
  // associated border. The border is erased internally after the read.
  int lx = rin->getLx(), ly = rin->getLy();

  int x, y;
  for (y = 0; y < ly; ++y) {
    // Process each row

    pixel_type *lineStart = rin->pixels(y), *pix;
    TPixelGR8 *runsStart  = runsMap->pixels(y), *run;

    UCHAR nextHeader, prevHeader = 0;
    for (x = 0, pix = lineStart, run = runsStart; x < lx;) {
      nextHeader = run->value;

      if (!(selector.transparent(*pix) || nextHeader & _PROCESSED)) {
        mesh_type *meshPtr = new mesh_type;

        // Read the mesh. All its internal sub-meshes are read as well.
        _readMesh(rin, selector, runsMap, x, y, *meshPtr, reader);
        face_output::addElement(reader, meshPtr);
      }

      // Increment variables
      x += runsMap->runLength(x, y), pix = lineStart + x, run = runsStart + x;
      prevHeader = (run - 1)->value;
    }

    assert(x == lx);
  }

  face_output::closeContainer(reader);

  runsMap->unlock();
}
}
}  // namespace TRop::borders

#endif  // BORDERS_EXTRACTOR_HPP
