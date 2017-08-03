

// Local includes
#include "pixelselectors.h"
#include "runsmap.h"

// STD includes
#include <stack>

// tcg includes
#include "tcg/tcg_traits.h"
#include "tcg/tcg_vertex.h"
#include "tcg/tcg_edge.h"
#include "tcg/tcg_face.h"

#define INCLUDE_HPP
#include "tcg/tcg_mesh.h"
#include "raster_edge_iterator.h"
#include "borders_extractor.h"
#undef INCLUDE_HPP

#include "trop_borders.h"

using namespace TRop::borders;

namespace {

//****************************************************************
//    Container Reader for Borders Reader
//****************************************************************

template <typename PixelSelector>
class WrapperReader {
  TRop::borders::BordersReader &m_reader;

public:
  WrapperReader(TRop::borders::BordersReader &reader) : m_reader(reader) {}

  void openContainer(const RasterEdgeIterator<PixelSelector> &it) {
    m_reader.openContainer(it.pos(), it.dir(), it.rightColor(), it.leftColor());
  }
  void addElement(const RasterEdgeIterator<PixelSelector> &it) {
    m_reader.addElement(it.pos(), it.dir(), it.leftColor());
  }
  void closeContainer() { m_reader.closeContainer(); }
};

}  // namespace

//****************************************************************
//    Borders Extractor instantiations
//****************************************************************

namespace TRop {
namespace borders {

//--------------------------------------------------------------------------------

// Standard type instantiations

template void DVAPI readMeshes<TPixel32>(const TRasterPT<TPixel32> &raster,
                                         ImageMeshesReaderT<TPixel32> &reader);
template void DVAPI readMeshes<TPixel64>(const TRasterPT<TPixel64> &raster,
                                         ImageMeshesReaderT<TPixel64> &reader);
template void DVAPI readMeshes<TPixelGR8>(
    const TRasterPT<TPixelGR8> &raster, ImageMeshesReaderT<TPixelGR8> &reader);
template void DVAPI
readMeshes<TPixelGR16>(const TRasterPT<TPixelGR16> &raster,
                       ImageMeshesReaderT<TPixelGR16> &reader);
template void DVAPI
readMeshes<TPixelCM32>(const TRasterPT<TPixelCM32> &raster,
                       ImageMeshesReaderT<TPixelCM32> &reader);

//--------------------------------------------------------------------------------

template <typename Pix>
void readBorders_simple(const TRasterPT<Pix> &raster, BordersReader &reader,
                        bool onlyCorners) {
  typedef PixelSelector<Pix> pixel_selector;

  pixel_selector selector(onlyCorners);
  WrapperReader<pixel_selector> wrapReader(reader);

  raster->lock();
  readBorders(raster, selector, wrapReader);
  raster->unlock();
}

//--------------------------------------------------------------------------------

void readBorders_simple(const TRasterGR8P &raster, BordersReader &reader,
                        const TPixelGR8 &transparencyColor, bool onlyCorners) {
  typedef PixelSelector<TPixelGR8> pixel_selector;

  pixel_selector selector(onlyCorners, transparencyColor);
  WrapperReader<pixel_selector> wrapReader(reader);

  raster->lock();
  readBorders(raster, selector, wrapReader);
  raster->unlock();
}

//--------------------------------------------------------------------------------

void readBorders_simple(const TRasterGR16P &raster, BordersReader &reader,
                        const TPixelGR16 &transparencyColor, bool onlyCorners) {
  typedef PixelSelector<TPixelGR16> pixel_selector;

  pixel_selector selector(onlyCorners, transparencyColor);
  WrapperReader<pixel_selector> wrapReader(reader);

  raster->lock();
  readBorders(raster, selector, wrapReader);
  raster->unlock();
}

//--------------------------------------------------------------------------------

void readBorders_simple(const TRasterCM32P &raster, BordersReader &reader,
                        bool onlyCorners, int toneThreshold) {
  typedef PixelSelector<TPixelCM32> pixel_selector;

  pixel_selector selector(onlyCorners, toneThreshold);
  WrapperReader<pixel_selector> wrapReader(reader);

  raster->lock();
  readBorders(raster, selector, wrapReader);
  raster->unlock();
}

//--------------------------------------------------------------------------------

void readBorders_simple(const TRasterP &raster, BordersReader &reader,
                        bool onlyCorners) {
  TRaster32P ras32(raster);
  if (ras32) {
    readBorders_simple(ras32, reader, onlyCorners);
    return;
  }

  TRaster64P ras64(raster);
  if (ras64) {
    readBorders_simple(ras64, reader, onlyCorners);
    return;
  }

  TRasterCM32P rasCM32(raster);
  if (rasCM32) {
    readBorders_simple(rasCM32, reader, onlyCorners);
    return;
  }

  TRasterGR8P rasGR8(raster);
  if (rasGR8) {
    readBorders_simple(rasGR8, reader, onlyCorners);
    return;
  }

  TRasterGR16P rasGR16(raster);
  if (rasGR16) {
    readBorders_simple(rasGR16, reader, onlyCorners);
    return;
  }
}
}
}  // namespace TRop::borders

//****************************************************************
//    Meshes Extraction (MeshesReader::Imp)
//****************************************************************

namespace TRop {
namespace borders {

class TRop::borders::ImageMeshesReader::Imp {
public:
  Face m_outerFace;
  tcg::list<ImageMeshP> m_meshes;

  std::stack<Face *> m_facesStack;

  int m_facesCount, m_edgesCount;

public:
  Imp() : m_facesCount(), m_edgesCount() {}

  void clear() {
    assert(m_facesStack.empty());

    m_outerFace = Face();
    m_meshes.clear();
    m_facesCount = m_edgesCount = 0;
  }
};

//****************************************************************
//    Meshes Extraction (MeshesReader)
//****************************************************************

ImageMeshesReader::ImageMeshesReader() : m_imp(new Imp) {}

//--------------------------------------------------------------------------------

ImageMeshesReader::~ImageMeshesReader() {}

//--------------------------------------------------------------------------------

void ImageMeshesReader::clear() { m_imp->clear(); }

//--------------------------------------------------------------------------------

const Face &ImageMeshesReader::outerFace() const { return m_imp->m_outerFace; }

//--------------------------------------------------------------------------------

Face &ImageMeshesReader::outerFace() { return m_imp->m_outerFace; }

//--------------------------------------------------------------------------------

const tcg::list<ImageMeshP> &ImageMeshesReader::meshes() const {
  return m_imp->m_meshes;
}

//--------------------------------------------------------------------------------

tcg::list<ImageMeshP> &ImageMeshesReader::meshes() { return m_imp->m_meshes; }

//--------------------------------------------------------------------------------

void ImageMeshesReader::openFace(ImageMesh *mesh, int faceIdx) {
  Face &fc        = mesh ? mesh->face(faceIdx) : m_imp->m_outerFace;
  fc.imageIndex() = m_imp->m_facesCount++;
  m_imp->m_facesStack.push(&fc);
}

//--------------------------------------------------------------------------------

void ImageMeshesReader::addMesh(ImageMesh *mesh) {
  Face &fc = *m_imp->m_facesStack.top();
  fc.meshes().push_back(m_imp->m_meshes.push_back(mesh));
}

//--------------------------------------------------------------------------------

void ImageMeshesReader::closeFace() { m_imp->m_facesStack.pop(); }

//--------------------------------------------------------------------------------

void ImageMeshesReader::closeEdge(ImageMesh *mesh, int edgeIdx) {
  mesh->edge(edgeIdx).imageIndex() = m_imp->m_edgesCount++;
}
}
}  //  namespace TRop::borders

//****************************************************************
//    Meshes Extraction (traits)
//****************************************************************

namespace tcg {

using namespace TRop::borders;

// Reader traits

template <typename Pixel>
struct container_reader_traits<ImageMeshesReaderT<Pixel>,
                               ImageMesh::face_type> {
  typedef ImageMeshesReaderT<Pixel> meshes_reader_type;
  typedef typename meshes_reader_type::value_type value_type;

public:
  inline static void openContainer(meshes_reader_type &reader, ImageMesh *mesh,
                                   int faceIdx, const value_type &colorValue) {
    reader.openFace(mesh, faceIdx, colorValue);
  }
  inline static void addElement(meshes_reader_type &reader, ImageMesh *mesh) {
    reader.addMesh(mesh);
  }
  inline static void closeContainer(meshes_reader_type &reader) {
    reader.closeFace();
  }
};

template <typename Pixel>
struct container_reader_traits<ImageMeshesReaderT<Pixel>,
                               ImageMesh::edge_type> {
  typedef ImageMeshesReaderT<Pixel> meshes_reader_type;
  typedef
      typename meshes_reader_type::raster_edge_iterator raster_edge_iterator;

public:
  inline static void openContainer(meshes_reader_type &reader,
                                   const raster_edge_iterator &it) {
    reader.openEdge(it);
  }
  inline static void addElement(meshes_reader_type &reader,
                                const raster_edge_iterator &it) {
    reader.addVertex(it);
  }
  inline static void closeContainer(meshes_reader_type &reader, ImageMesh *mesh,
                                    int edgeIdx) {
    reader.closeEdge(mesh, edgeIdx);
  }
};

}  // namespace tcg

//****************************************************************
//    Meshes Extraction (functions)
//****************************************************************

namespace TRop {
namespace borders {

template <typename Pix>
void readMeshes(const TRasterPT<Pix> &raster, ImageMeshesReaderT<Pix> &reader) {
  typedef PixelSelector<Pix> pixel_selector;

  reader.clear();

  raster->lock();
  readMeshes<pixel_selector, ImageMesh, ImageMeshesReaderT<Pix>>(
      raster, reader.pixelSelector(), reader);
  raster->unlock();
}
}
}  // namespace TRop::borders
