#pragma once

#ifndef TMESHIMAGE_INCLUDED
#define TMESHIMAGE_INCLUDED

#include <memory>

// TnzCore includes
#include "tsmartpointer.h"
#include "tpersist.h"
#include "tgeometry.h"
#include "timage.h"

// tcg includes
#include "tcg_wrap.h"
#include "tcg/tcg_vertex.h"
#include "tcg/tcg_edge.h"
#include "tcg/tcg_face.h"
#include "tcg/tcg_mesh.h"
#include "tcg/tcg_mesh_bgl.h"

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===========================================

//    Forward declarations

//===========================================

//***********************************************************************************
//    TTextureVertex (Textured Mesh Vertex Type)  declaration
//***********************************************************************************

struct RigidPoint final : public TPointD {
  double rigidity;

public:
  RigidPoint() : TPointD(), rigidity(1.0) {}
  explicit RigidPoint(double x, double y, double rigidity_ = 1.0)
      : TPointD(x, y), rigidity(rigidity_) {}
  explicit RigidPoint(const TPointD &point, double rigidity_ = 1.0)
      : TPointD(point), rigidity(rigidity_) {}

  RigidPoint &operator=(const TPointD &p) {
    TPointD::operator=(p);
    return *this;
  }

public:
  RigidPoint operator+(const RigidPoint &other) const {
    return RigidPoint(x + other.x, y + other.y, rigidity + other.rigidity);
  }

  RigidPoint &operator+=(const RigidPoint &other) {
    x += other.x, y += other.y, rigidity += other.rigidity;
    return *this;
  }

  RigidPoint operator-(const RigidPoint &other) const {
    return RigidPoint(x - other.x, y - other.y, rigidity - other.rigidity);
  }

  RigidPoint &operator-=(const RigidPoint &other) {
    x -= other.x, y -= other.y, rigidity -= other.rigidity;
    return *this;
  }

  friend RigidPoint operator*(double a, const RigidPoint &b) {
    return RigidPoint(a * b.x, a * b.y, a * b.rigidity);
  }

  friend RigidPoint operator*(const RigidPoint &a, double b) {
    return RigidPoint(a.x * b, a.y * b, a.rigidity * b);
  }

  RigidPoint &operator*=(double a) {
    x *= a, y *= a, rigidity *= a;
    return *this;
  }

  friend RigidPoint operator*(const TAffine &aff, const RigidPoint &p) {
    return RigidPoint(aff.operator*(p), p.rigidity);
  }
};

//=================================================================================

namespace tcg {

template <>
struct point_traits<RigidPoint> {
  typedef RigidPoint point_type;
  typedef double value_type;
  typedef double float_type;

  inline static value_type x(const point_type &p) { return p.x; }
  inline static value_type y(const point_type &p) { return p.y; }
};

}  // namespace tcg

//-----------------------------------------------------------------------------

template class DVAPI tcg::Vertex<TPointD>;
template class DVAPI tcg::Vertex<RigidPoint>;

typedef tcg::Vertex<RigidPoint> TTextureVertex;

template class DVAPI tcg::Mesh<TTextureVertex, tcg::Edge, tcg::FaceN<3>>;
template class DVAPI tcg::TriMesh<TTextureVertex, tcg::Edge, tcg::FaceN<3>>;

//***********************************************************************************
//    TTextureMesh (Textured Mesh Type)  declaration
//***********************************************************************************

class DVAPI TTextureMesh final
    : public tcg::TriMesh<TTextureVertex, tcg::Edge, tcg::FaceN<3>>,
      public TSmartObject,
      public TPersist {
  PERSIST_DECLARATION(TTextureMesh)
  DECLARE_CLASS_CODE

public:
  TTextureMesh();

  TTextureMesh(const TTextureMesh &);
  TTextureMesh &operator=(const TTextureMesh &);

  int faceContaining(const TPointD &p) const;
  bool faceContains(int f, const TPointD &p) const;

  TRectD getBBox() const;

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TTextureMesh>;
#endif

typedef TSmartPointerT<TTextureMesh> TTextureMeshP;

//-----------------------------------------------------------------------------

namespace boost {

template <>
struct graph_traits<TTextureMesh> final
    : public graph_traits<
          tcg::Mesh<TTextureMesh::vertex_type, TTextureMesh::edge_type,
                    TTextureMesh::face_type>> {};

}  // namespace boost

//***********************************************************************************
//    TMeshImage (Textured Mesh Image)  declaration
//***********************************************************************************

class DVAPI TMeshImage final : public TImage {
  class Imp;
  std::shared_ptr<Imp> m_imp;

public:
  typedef std::vector<TTextureMeshP> meshes_container;

public:
  TMeshImage();
  TMeshImage(std::shared_ptr<Imp> imp);
  ~TMeshImage();

  TMeshImage(const TMeshImage &other);
  TMeshImage &operator=(TMeshImage other);

  TImage::Type getType() const override { return TImage::MESH; }

  TRectD getBBox() const override;
  TImage *cloneImage() const override;

  void getDpi(double &dpix, double &dpiy) const;
  void setDpi(double dpix, double dpiy);

  const meshes_container &meshes() const;
  meshes_container &meshes();

  friend void swap(TMeshImage &a, TMeshImage &b) {
    std::swap(a.m_imp, b.m_imp);
  }
};

//-----------------------------------------------------------------------------

#ifdef _WIN32
template class DVAPI TSmartPointerT<TMeshImage>;
template class DVAPI TDerivedSmartPointerT<TMeshImage, TImage>;
#endif

class DVAPI TMeshImageP final
    : public TDerivedSmartPointerT<TMeshImage, TImage> {
public:
  TMeshImageP() {}
  TMeshImageP(TMeshImage *image) : DerivedSmartPointer(image) {}
  TMeshImageP(TImageP image) : DerivedSmartPointer(image) {}
#if !defined(_WIN32)
  TMeshImageP(TImage *image) : DerivedSmartPointer(TImageP(image)) {}
#endif
  operator TImageP() { return TImageP(m_pointer); }
};

#endif  // TMESHIMAGE_INCLUDED
