// #pragma once // could not use by INCLUDE_HPP

#ifndef TROP_BORDERS_H
#define TROP_BORDERS_H

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"
#include "tgeometry.h"

#include "traster.h"
#include "trastercm.h"

// tcg includes
#include "tcg_wrap.h"
#include "tcg/tcg_vertex.h"
#include "tcg/tcg_edge.h"
#include "tcg/tcg_face.h"
#include "tcg/tcg_mesh.h"

// TRop::borders includes
#include "../common/trop/pixelselectors.h"
#include "../common/trop/raster_edge_iterator.h"
#include "../common/trop/borders_extractor.h"

#undef DVAPI
#undef DVVAR
#ifdef TROP_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================================================================
/*!
  The TRop::borders namespace contains functions and classes that perform
  contours extraction of a specified raster (either greyscale, fullcolor
  or colormap).

  Image contours are currently extracted in a not-hierarchycal fashion
  (ie in succession) and supplied through a reimplementable reader class.
*/
//==============================================================================

namespace TRop {
namespace borders {

// Standard type instantiations

template class DVAPI RasterEdgeIterator<PixelSelector<TPixel32>>;
template class DVAPI RasterEdgeIterator<PixelSelector<TPixel64>>;
template class DVAPI RasterEdgeIterator<PixelSelector<TPixelGR8>>;
template class DVAPI RasterEdgeIterator<PixelSelector<TPixelGR16>>;
template class DVAPI RasterEdgeIterator<PixelSelector<TPixelCM32>>;

//**********************************************************************
//    Borders Extraction Reader  (callbacks container)
//**********************************************************************

/*!
  The BordersReader class defines the supported callbacks fired from the
  borders extraction routines.
*/
class DVAPI BordersReader {
public:
  virtual void openContainer(const TPoint &pos, const TPoint &dir,
                             const TPixel32 &innerColor,
                             const TPixel32 &outerColor) {}
  virtual void openContainer(const TPoint &pos, const TPoint &dir,
                             const TPixel64 &innerColor,
                             const TPixel64 &outerColor) {}
  virtual void openContainer(const TPoint &pos, const TPoint &dir,
                             const TPixelGR8 &innerColor,
                             const TPixelGR8 &outerColor) {}
  virtual void openContainer(const TPoint &pos, const TPoint &dir,
                             const TPixelGR16 &innerColor,
                             const TPixelGR16 &outerColor) {}
  virtual void openContainer(const TPoint &pos, const TPoint &dir,
                             TUINT32 innerColorIdx, TUINT32 outerColorIdx) {
  }  // colormap

  virtual void addElement(const TPoint &pos, const TPoint &dir,
                          const TPixel32 &outerColor) {}
  virtual void addElement(const TPoint &pos, const TPoint &dir,
                          const TPixel64 &outerColor) {}
  virtual void addElement(const TPoint &pos, const TPoint &dir,
                          const TPixelGR8 &outerColor) {}
  virtual void addElement(const TPoint &pos, const TPoint &dir,
                          const TPixelGR16 &outerColor) {}
  virtual void addElement(const TPoint &pos, const TPoint &dir,
                          TUINT32 outerColorIdx) {}

  virtual void closeContainer() {}
};

//**********************************************************************
//    Borders Extraction functions
//**********************************************************************

// Generic readBorders
void DVAPI readBorders_simple(const TRasterP &raster, BordersReader &reader,
                              bool onlyCorners = true);

// Greymap readBorders with transparency color
void DVAPI readBorders_simple(const TRasterGR8P &raster, BordersReader &reader,
                              const TPixelGR8 &transparencyColor,
                              bool onlyCorners = true);
void DVAPI readBorders_simple(const TRasterGR16P &raster, BordersReader &reader,
                              const TPixelGR16 &transparencyColor,
                              bool onlyCorners = true);

// Colormap readBorders with ink/paint antialias chooser
void DVAPI readBorders_simple(const TRasterCM32P &raster, BordersReader &reader,
                              bool onlyCorners = true, int toneThreshold = 128);

//**********************************************************************
//    Mesh Extraction (classes)
//**********************************************************************

typedef tcg::Vertex<TPoint> Vertex;

//--------------------------------------------------------------------------------

class Edge final : public tcg::Edge {
  TPoint m_directions[2];
  int m_imageIndex;

public:
  const TPoint &direction(int i) const { return m_directions[i]; }
  TPoint &direction(int i) { return m_directions[i]; }

  //! The edge index in the overall image extraction procedure. This value can
  //! be used to
  //! univocally associate external data to an image edge.
  int imageIndex() const { return m_imageIndex; }
  int &imageIndex() { return m_imageIndex; }
};

//--------------------------------------------------------------------------------

class Face final : public tcg::Face {
  tcg::list<int> m_meshes;
  int m_imageIndex;

public:
  const tcg::list<int> &meshes() const { return m_meshes; }
  tcg::list<int> &meshes() { return m_meshes; }

  int meshesCount() const { return m_meshes.size(); }
  int mesh(int m) const { return m_meshes[m]; }

  //! The face index in the overall image extraction procedure. This value can
  //! be used to
  //! univocally associate external data to an image face.
  int imageIndex() const { return m_imageIndex; }
  int &imageIndex() { return m_imageIndex; }
};

//--------------------------------------------------------------------------------

class ImageMesh final : public TSmartObject,
                        public tcg::Mesh<Vertex, Edge, Face> {};

//--------------------------------------------------------------------------------
}
}  // namespace TRop::borders

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRop::borders::ImageMesh>;
#endif

namespace TRop {
namespace borders {

typedef TSmartPointerT<ImageMesh> ImageMeshP;

//**********************************************************************
//    Mesh Extraction (reader)
//**********************************************************************

class DVAPI ImageMeshesReader {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  ImageMeshesReader();
  virtual ~ImageMeshesReader();

  const Face &outerFace() const;
  Face &outerFace();

  const tcg::list<ImageMeshP> &meshes() const;
  tcg::list<ImageMeshP> &meshes();

  void clear();

public:
  // Face reader functions. Nested meshes are stored by default in a
  // global list of meshes retrievable with the meshes() accessor.
  // Each Mesh Face will be filled with the list of its sub-meshes in
  // the form of indices to the global list.

  void openFace(ImageMesh *mesh, int faceIdx);
  void addMesh(ImageMesh *mesh);
  void closeFace();

public:
  void closeEdge(ImageMesh *mesh, int edgeIdx);

private:
  // Not implemented

  ImageMeshesReader(const ImageMeshesReader &);
  ImageMeshesReader &operator=(const ImageMeshesReader &);
};

//--------------------------------------------------------------------------------

template <typename Pixel>
class ImageMeshesReaderT : public ImageMeshesReader {
public:
  typedef PixelSelector<Pixel> pixel_selector_type;
  typedef typename pixel_selector_type::value_type value_type;
  typedef RasterEdgeIterator<pixel_selector_type> raster_edge_iterator;

protected:
  pixel_selector_type m_selector;

public:
  ImageMeshesReaderT() : m_selector(false) {}
  ImageMeshesReaderT(const pixel_selector_type &selector)
      : m_selector(selector) {}
  ~ImageMeshesReaderT() {}

  const pixel_selector_type &pixelSelector() const { return m_selector; }

public:
  virtual void openFace(ImageMesh *mesh, int faceIdx,
                        const value_type &colorValue) {
    ImageMeshesReader::openFace(mesh, faceIdx);
  }
  virtual void addMesh(ImageMesh *mesh) { ImageMeshesReader::addMesh(mesh); }
  virtual void closeFace() { ImageMeshesReader::closeFace(); }

public:
  // Edge reader functions. Edges are not stored by default.
  // Users should reimplement these methods to store or process edge
  // data for the required purpose.

  virtual void openEdge(const raster_edge_iterator &it) {}
  virtual void addVertex(const raster_edge_iterator &it) {}
  virtual void closeEdge(ImageMesh *mesh, int edgeIdx) {
    ImageMeshesReader::closeEdge(mesh, edgeIdx);
  }
};

//**********************************************************************
//    Mesh Extraction (functions)
//**********************************************************************

template <typename Pixel>
void readMeshes(const TRasterPT<Pixel> &raster,
                ImageMeshesReaderT<Pixel> &reader);

//--------------------------------------------------------------------------------
}
}  // namespace TRop::borders

#endif  // TROP_BORDERS_H

#ifdef INCLUDE_HPP
#include "../common/trop/borders_extractor.hpp"
#include "../common/trop/raster_edge_iterator.hpp"
#endif
