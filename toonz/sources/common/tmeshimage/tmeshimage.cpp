

// TnzCore includes
#include "tstream.h"

// tcg includes
#include "tcg/tcg_misc.h"

#define INCLUDE_HPP
#include "tmeshimage.h"
#undef INCLUDE_HPP

//******************************************************************************
//    Explicit instantiations
//******************************************************************************

typedef tcg::TriMesh<TTextureVertex, tcg::Edge, tcg::FaceN<3>> TriMesh_base;

//******************************************************************************
//    TTextureMesh  implementation
//******************************************************************************

DEFINE_CLASS_CODE(TTextureMesh, 120)
PERSIST_IDENTIFIER(TTextureMesh, "mesh")

static void static_check() {
  /* input iterator */
  static_assert(
      std::is_same<std::iterator_traits<
                       std::vector<TTextureMesh>::iterator>::iterator_category,
                   std::random_access_iterator_tag>::value == true,
      "random");

  static_assert(
      std::is_base_of<std::input_iterator_tag,
                      std::iterator_traits<std::vector<
                          TTextureMesh>::iterator>::iterator_category>::value ==
          true,
      "input");

  static_assert(
      std::is_base_of<std::forward_iterator_tag,
                      std::iterator_traits<std::vector<
                          TTextureMesh>::iterator>::iterator_category>::value ==
          true,
      "forward");

  static_assert(
      std::is_constructible<TTextureMeshP,
                            std::iterator_traits<std::vector<
                                TTextureMeshP>::iterator>::reference>::value ==
          true,
      "akan");
}

//-----------------------------------------------------------------------

TTextureMesh::TTextureMesh() : TSmartObject(m_classCode) {}

//-----------------------------------------------------------------------

TTextureMesh::TTextureMesh(const TTextureMesh &other)
    : TriMesh_base(other), TSmartObject(m_classCode) {}

//-----------------------------------------------------------------------

TTextureMesh &TTextureMesh::operator=(const TTextureMesh &other) {
  TriMesh_base::operator=(other);
  return *this;
}

//-----------------------------------------------------------------------

bool TTextureMesh::faceContains(int f, const TPointD &p) const {
  int v0, v1, v2;
  this->faceVertices(f, v0, v1, v2);

  const TPointD &p0 = vertex(v0).P();
  const TPointD &p1 = vertex(v1).P();
  const TPointD &p2 = vertex(v2).P();

  bool clockwise = (tcg::point_ops::cross(p2 - p0, p1 - p0) >= 0);
  return ((tcg::point_ops::cross(p - p0, p1 - p0) >= 0) == clockwise) &&
         ((tcg::point_ops::cross(p - p1, p2 - p1) >= 0) == clockwise) &&
         ((tcg::point_ops::cross(p - p2, p0 - p2) >= 0) == clockwise);
}

//-----------------------------------------------------------------------

int TTextureMesh::faceContaining(const TPointD &p) const {
  int f, fCount = facesCount();
  for (f = 0; f < fCount; ++f)
    if (faceContains(f, p)) return f;
  return -1;
}

//-----------------------------------------------------------------------

TRectD TTextureMesh::getBBox() const {
  // TODO: Should be cached...

  const double max = (std::numeric_limits<double>::max)();
  TRectD result(max, max, -max, -max);

  // Iterate all meshes
  assert(m_vertices.size() == m_vertices.nodesCount());

  int v, vCount = int(m_vertices.size());
  for (v = 0; v != vCount; ++v) {
    const TTextureVertex &vx = m_vertices[v];

    result.x0 = std::min(result.x0, vx.P().x);
    result.y0 = std::min(result.y0, vx.P().y);
    result.x1 = std::max(result.x1, vx.P().x);
    result.y1 = std::max(result.y1, vx.P().y);
  }

  return result;
}

//-----------------------------------------------------------------------

void TTextureMesh::saveData(TOStream &os) {
  struct locals {
    static inline bool hasNon1Rigidity(const TTextureMesh &mesh) {
      int v, vCount = int(mesh.verticesCount());
      for (v = 0; v != vCount; ++v)
        if (mesh.vertex(v).P().rigidity != 1.0) return true;
      return false;
    }
  };

  // NOTE: Primitives saved by INDEX iteration is NOT COINCIDENTAL - since
  // the lists' internal linking could have been altered to mismatch the
  // natural indexing referred to by primitives' data.

  if (m_vertices.size() != m_vertices.nodesCount() ||
      m_edges.size() != m_edges.nodesCount() ||
      m_faces.size() != m_faces.nodesCount()) {
    // Ensure the mesh is already squeezed - save a squeezed
    // copy if necessary
    TTextureMesh mesh(*this);

    mesh.squeeze();
    mesh.saveData(os);

    return;
  }

  assert(m_vertices.size() == m_vertices.nodesCount());
  assert(m_edges.size() == m_edges.nodesCount());
  assert(m_faces.size() == m_faces.nodesCount());

  // Store Vertices
  os.openChild("V");
  {
    int vCount = int(m_vertices.size());
    os << vCount;

    for (int v = 0; v != vCount; ++v) {
      TTextureMesh::vertex_type &vx = m_vertices[v];
      os << vx.P().x << vx.P().y;
    }
  }
  os.closeChild();

  // Store Edges
  os.openChild("E");
  {
    int eCount = int(m_edges.size());
    os << eCount;

    for (int e = 0; e != eCount; ++e) {
      TTextureMesh::edge_type &ed = m_edges[e];
      os << ed.vertex(0) << ed.vertex(1);
    }
  }
  os.closeChild();

  // Store Faces
  os.openChild("F");
  {
    int fCount = int(m_faces.size());
    os << fCount;

    for (int f = 0; f != fCount; ++f) {
      TTextureMesh::face_type &fc = m_faces[f];

      int e, eCount = fc.edgesCount();
      for (e = 0; e < eCount; ++e) os << fc.edge(e);
    }
  }
  os.closeChild();

  // Store rigidities
  if (locals::hasNon1Rigidity(*this)) {
    os.openChild("rigidities");
    {
      int vCount = int(m_vertices.size());
      os << vCount;

      for (int v = 0; v != vCount; ++v) os << m_vertices[v].P().rigidity;
    }
    os.closeChild();
  }
}

//-----------------------------------------------------------------------

void TTextureMesh::loadData(TIStream &is) {
  typedef tcg::Mesh<vertex_type, edge_type, face_type> mesh_type;

  std::string str;
  int i, size;

  while (is.openChild(str)) {
    if (str == "V") {
      is >> size;

      m_vertices.reserve(size);
      TTextureMesh::vertex_type v;

      for (i = 0; i < size; ++i) {
        is >> v.P().x >> v.P().y;
        mesh_type::addVertex(v);
      }

      is.closeChild();
    } else if (str == "E") {
      is >> size;

      m_edges.reserve(size);
      int v0, v1;

      for (i = 0; i < size; ++i) {
        is >> v0 >> v1;
        mesh_type::addEdge(TTextureMesh::edge_type(v0, v1));
      }

      is.closeChild();
    } else if (str == "F") {
      is >> size;

      m_faces.reserve(size);

      int e[3];

      for (i = 0; i < size; ++i) {
        is >> e[0] >> e[1] >> e[2];
        mesh_type::addFace(TTextureMesh::face_type(e));
      }

      is.closeChild();
    } else if (str == "rigidities") {
      is >> size;
      size = std::min(size, this->verticesCount());

      for (i = 0; i < size; ++i) is >> m_vertices[i].P().rigidity;

      is.closeChild();
    } else {
      assert(false);
      is.skipCurrentTag();
    }
  }
}

//******************************************************************************
//    TMeshImage::Imp  definition
//******************************************************************************

class TMeshImage::Imp {
public:
  std::vector<TTextureMeshP> m_meshes;  //!< Mesh data
  double m_dpiX, m_dpiY;                //!< Meshes dpi

  Imp() : m_dpiX(), m_dpiY() {}

  Imp(const Imp &other) : m_dpiX(other.m_dpiX), m_dpiY(other.m_dpiY) {
    for (auto const &e : other.m_meshes) {
      m_meshes.push_back(cloneMesh(e));
    }
  }

private:
  static TTextureMeshP cloneMesh(const TTextureMeshP &other) {
    return TTextureMeshP(new TTextureMesh(*other));
  }

  // Not assignable
  Imp &operator=(const Imp &other);
};

//******************************************************************************
//    TMeshImage  implementation
//******************************************************************************

TMeshImage::TMeshImage() : m_imp(new Imp) {}

//-----------------------------------------------------------------------

TMeshImage::TMeshImage(std::shared_ptr<Imp> imp) : m_imp(std::move(imp)) {}

//-----------------------------------------------------------------------

TMeshImage::~TMeshImage() {}

//-----------------------------------------------------------------------

TMeshImage::TMeshImage(const TMeshImage &other)
    : m_imp(new Imp(*other.m_imp)) {}

//-----------------------------------------------------------------------

TMeshImage &TMeshImage::operator=(TMeshImage other) {
  swap(*this, other);
  return *this;
}

//-----------------------------------------------------------------------

TRectD TMeshImage::getBBox() const {
  const double max = (std::numeric_limits<double>::max)();
  TRectD result(max, max, -max, -max);

  // Iterate all meshes
  int m, mCount = int(m_imp->m_meshes.size());
  for (m = 0; m < mCount; ++m) result += m_imp->m_meshes[m]->getBBox();

  return result;
}

//-----------------------------------------------------------------------

TImage *TMeshImage::cloneImage() const { return new TMeshImage(*this); }

//-----------------------------------------------------------------------

void TMeshImage::getDpi(double &dpix, double &dpiy) const {
  dpix = m_imp->m_dpiX, dpiy = m_imp->m_dpiY;
}

//-----------------------------------------------------------------------

void TMeshImage::setDpi(double dpix, double dpiy) {
  m_imp->m_dpiX = dpix, m_imp->m_dpiY = dpiy;
}

//-----------------------------------------------------------------------

const std::vector<TTextureMeshP> &TMeshImage::meshes() const {
  return m_imp->m_meshes;
}

//-----------------------------------------------------------------------

std::vector<TTextureMeshP> &TMeshImage::meshes() { return m_imp->m_meshes; }
