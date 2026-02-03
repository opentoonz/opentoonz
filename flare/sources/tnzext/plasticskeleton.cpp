

// TnzCore includes
#include "tstream.h"

// TnzExt includes
#include "ext/plasticskeletondeformation.h"

// STL includes
#include <set>

// tcg includes
#include "tcg/tcg_misc.h"
#include "tcg/tcg_pool.h"

#include "ext/plasticskeleton.h"

PERSIST_IDENTIFIER(PlasticSkeletonVertex, "PlasticSkeletonVertex")
PERSIST_IDENTIFIER(PlasticSkeleton, "PlasticSkeleton")

DEFINE_CLASS_CODE(PlasticSkeleton, 122)

//************************************************************************************
//    PlasticSkeletonVertex  implementation
//************************************************************************************

PlasticSkeletonVertex::PlasticSkeletonVertex()
    : tcg::Vertex<TPointD>()
    , m_number(-1)
    , m_parent(-1)
    , m_minAngle(-(std::numeric_limits<double>::max)())
    , m_maxAngle((std::numeric_limits<double>::max)())
    , m_interpolate(true) {}

//-------------------------------------------------------------------------------

PlasticSkeletonVertex::PlasticSkeletonVertex(const TPointD &pos)
    : tcg::Vertex<TPointD>(pos)
    , m_number(-1)
    , m_parent(-1)
    , m_minAngle(-(std::numeric_limits<double>::max)())
    , m_maxAngle((std::numeric_limits<double>::max)())
    , m_interpolate(true) {}

//-------------------------------------------------------------------------------

PlasticSkeletonVertex::operator PlasticHandle() const {
  PlasticHandle result(P());
  result.m_interpolate = m_interpolate;
  return result;
}

//-------------------------------------------------------------------------------

void PlasticSkeletonVertex::saveData(TOStream &os) {
  os.child("name") << m_name;
  os.child("number") << m_number;
  os.child("pos") << this->P().x << this->P().y;
  os.child("interpolate") << (int)this->m_interpolate;

  if (m_minAngle != -(std::numeric_limits<double>::max)())
    os.child("minAngle") << m_minAngle;

  if (m_maxAngle != (std::numeric_limits<double>::max)())
    os.child("maxAngle") << m_maxAngle;
}

//-------------------------------------------------------------------------------

void PlasticSkeletonVertex::loadData(TIStream &is) {
  int val;

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "name")
      is >> m_name, is.matchEndTag();
    else if (tagName == "number")
      is >> m_number, is.matchEndTag();
    else if (tagName == "pos")
      is >> this->P().x >> this->P().y, is.matchEndTag();
    else if (tagName == "interpolate")
      is >> val, m_interpolate = (bool)val, is.matchEndTag();
    else if (tagName == "minAngle")
      is >> m_minAngle, is.matchEndTag();
    else if (tagName == "maxAngle")
      is >> m_maxAngle, is.matchEndTag();
    else
      is.skipCurrentTag();
  }
}

//************************************************************************************
//    PlasticSkeleton::Imp  definition
//************************************************************************************

class PlasticSkeleton::Imp {
public:
  std::set<PlasticSkeletonDeformation *> m_deformations;  //!< Registered
                                                          //! deformations for
  //! this skeleton (not
  //! owned)
  tcg::indices_pool<int>
      m_numbersPool;  //!< Vertex numbers pool (used for naming vertices only)

public:
  Imp() {}
  Imp(const Imp &other);
  Imp &operator=(const Imp &other);

  Imp(Imp &&other);
  Imp &operator=(Imp &&other);
};

//===============================================================================

PlasticSkeleton::Imp::Imp(const Imp &other)
    : m_numbersPool(other.m_numbersPool) {}

//-------------------------------------------------------------------------------

PlasticSkeleton::Imp &PlasticSkeleton::Imp::operator=(const Imp &other) {
  m_numbersPool = other.m_numbersPool;
  return *this;
}

//-------------------------------------------------------------------------------
PlasticSkeleton::Imp::Imp(Imp &&other)
    : m_numbersPool(std::move(other.m_numbersPool)) {}

//-------------------------------------------------------------------------------

PlasticSkeleton::Imp &PlasticSkeleton::Imp::operator=(Imp &&other) {
  m_numbersPool = std::move(other.m_numbersPool);
  return *this;
}

//************************************************************************************
//    PlasticSkeleton  implementation
//************************************************************************************

PlasticSkeleton::PlasticSkeleton() : m_imp(new Imp) {}

//------------------------------------------------------------------

PlasticSkeleton::~PlasticSkeleton() {}

//------------------------------------------------------------------

PlasticSkeleton::PlasticSkeleton(const PlasticSkeleton &other)
    : mesh_type(other), m_imp(new Imp(*other.m_imp)) {}

//------------------------------------------------------------------

PlasticSkeleton &PlasticSkeleton::operator=(const PlasticSkeleton &other) {
  assert(m_imp->m_deformations.empty());

  mesh_type::operator=(other);
  *m_imp             = *other.m_imp;

  return *this;
}

//------------------------------------------------------------------
PlasticSkeleton::PlasticSkeleton(PlasticSkeleton &&other)
    : mesh_type(std::forward<mesh_type>(other))
    , m_imp(new Imp(std::move(*other.m_imp))) {}

//------------------------------------------------------------------

PlasticSkeleton &PlasticSkeleton::operator=(PlasticSkeleton &&other) {
  assert(m_imp->m_deformations.empty());

  mesh_type::operator=(std::forward<mesh_type>(other));
  *m_imp             = std::move(*other.m_imp);

  return *this;
}

//------------------------------------------------------------------

void PlasticSkeleton::addListener(PlasticSkeletonDeformation *deformation) {
  m_imp->m_deformations.insert(deformation);
}

//------------------------------------------------------------------

void PlasticSkeleton::removeListener(PlasticSkeletonDeformation *deformation) {
  m_imp->m_deformations.erase(deformation);
}

//------------------------------------------------------------------

bool PlasticSkeleton::setVertexName(int v, const QString &newName) {
  assert(!newName.isEmpty());

  if (m_vertices[v].m_name == newName) return true;

  // Traverse the vertices list - if the same name already exists, return false
  tcg::list<vertex_type>::iterator vt, vEnd(m_vertices.end());
  for (vt = m_vertices.begin(); vt != vEnd; ++vt)
    if (vt.m_idx != v &&
        vt->m_name == newName)  // v should be skipped in the check
      return false;

  // Notify deformations before changing the name
  std::set<PlasticSkeletonDeformation *>::iterator dt,
      dEnd(m_imp->m_deformations.end());
  for (dt = m_imp->m_deformations.begin(); dt != dEnd; ++dt)
    (*dt)->vertexNameChange(this, v, newName);

  m_vertices[v].m_name = newName;

  return true;
}

//------------------------------------------------------------------

void PlasticSkeleton::moveVertex(int v, const TPointD &pos) {
  mesh_type::vertex(v).P() = pos;  // Apply new position
}

//------------------------------------------------------------------

int PlasticSkeleton::addVertex(const PlasticSkeletonVertex &vx, int parent) {
  // Add the vertex
  int v = mesh_type::addVertex(vx);

  // Retrieve a vertex index
  PlasticSkeletonVertex &vx_ = vertex(v);

  vx_.m_number = m_imp->m_numbersPool.acquire();

  // Assign a name to the vertex in case none was given
  QString name(vx.name());
  if (name.isEmpty())
    name = (v == 0) ? QString("Root")
                    : "Vertex " +
                          QString::number(vx_.m_number).rightJustified(3, '_');

  // Ensure the name is unique
  while (!setVertexName(v, name)) name += "_";

  if (parent >= 0) {
    // Link it to the parent
    mesh_type::addEdge(
        edge_type(parent, v));  // Observe that parent is always v0
    vx_.m_parent = parent;
    assert(parent != v);
  }

  // Notify deformations
  std::set<PlasticSkeletonDeformation *>::iterator dt,
      dEnd(m_imp->m_deformations.end());
  for (dt = m_imp->m_deformations.begin(); dt != dEnd; ++dt)
    (*dt)->addVertex(this, v);

  return v;
}

//-------------------------------------------------------------------------------

int PlasticSkeleton::insertVertex(const PlasticSkeletonVertex &vx, int parent,
                                  const std::vector<int> &children) {
  assert(parent >= 0);

  if (children.empty()) return addVertex(vx, parent);

  // Add the vertex
  int v = mesh_type::addVertex(vx);

  // Retrieve a vertex index
  PlasticSkeletonVertex &vx_ = vertex(v);

  vx_.m_number = m_imp->m_numbersPool.acquire();

  // Assign a name to the vertex in case none was given
  QString name(vx.name());
  if (name.isEmpty())
    name = "Vertex " + QString::number(vx_.m_number).rightJustified(3, '_');

  // Ensure the name is unique
  while (!setVertexName(v, name)) name += "_";

  // Link it to the parent
  {
    PlasticSkeletonVertex &vx_ = vertex(v);

    mesh_type::addEdge(
        edge_type(parent, v));  // Observe that parent is always v0
    vx_.m_parent = parent;
    assert(parent != v);
  }

  // Link it to children
  int c, cCount = int(children.size());
  for (c = 0; c != cCount; ++c) {
    int vChild                     = children[c];
    PlasticSkeletonVertex &vxChild = vertex(vChild);

    assert(vxChild.parent() == parent);

    // Remove the edge and substitute it with a new one
    int e = edgeInciding(parent, vChild);
    mesh_type::removeEdge(e);

    mesh_type::addEdge(edge_type(v, vChild));
    vxChild.m_parent = v;
    assert(v != vChild);
  }

  // Notify deformations
  std::set<PlasticSkeletonDeformation *>::iterator dt,
      dEnd(m_imp->m_deformations.end());
  for (dt = m_imp->m_deformations.begin(); dt != dEnd; ++dt)
    (*dt)->insertVertex(this, v);

  return v;
}

//-------------------------------------------------------------------------------

int PlasticSkeleton::insertVertex(const PlasticSkeletonVertex &vx, int e) {
  const edge_type &ed = edge(e);
  return insertVertex(vx, ed.vertex(0), std::vector<int>(1, ed.vertex(1)));
}

//-------------------------------------------------------------------------------

void PlasticSkeleton::removeVertex(int v) {
  int vNumber;
  {
    // Reparent all of v's children to its parent's children, first. This is
    // needed
    // to ensure that deformations update vertex deformations correctly.

    PlasticSkeletonVertex vx(
        vertex(v));  // Note the COPY - not referencing. It's best since
    // we'll be iterating and erasing vx's edges at the same time
    int parent = vx.m_parent;
    if (parent < 0) {
      // Root case - clear the whole skeleton
      clear();  // Should ensure that the next inserted vertex has index 0
      return;
    }

    // Add edges from parent to vx's children
    vertex_type::edges_iterator et, eEnd = vx.edgesEnd();
    for (et = vx.edgesBegin(); et != eEnd; ++et) {
      int vChild = edge(*et).vertex(1);
      if (vChild == v) continue;

      mesh_type::removeEdge(*et);

      mesh_type::addEdge(edge_type(parent, vChild));
      vertex(vChild).m_parent = parent;
      assert(vChild != parent);
    }

    vNumber = vx.m_number;
  }

  // Notify deformations BEFORE removing the vertex, so the vertex deformations
  // are still accessible.
  std::set<PlasticSkeletonDeformation *>::iterator dt,
      dEnd(m_imp->m_deformations.end());
  for (dt = m_imp->m_deformations.begin(); dt != dEnd; ++dt)
    (*dt)->deleteVertex(this, v);

  // Then, erase v. This already ensures that edges connected with v are
  // destroyed.
  mesh_type::removeVertex(v);

  m_imp->m_numbersPool.release(vNumber);
}

//-------------------------------------------------------------------------------

void PlasticSkeleton::clear() {
  mesh_type::clear();
  m_imp->m_numbersPool.clear();

  // Notify deformations
  std::set<PlasticSkeletonDeformation *>::iterator dt,
      dEnd(m_imp->m_deformations.end());
  for (dt = m_imp->m_deformations.begin(); dt != dEnd; ++dt) (*dt)->clear(this);
}

//-------------------------------------------------------------------------------

void PlasticSkeleton::squeeze() {
  // Squeeze associated deformations first
  int v, e;

  // Update indices
  tcg::list<vertex_type>::iterator vt, vEnd(m_vertices.end());
  for (v = 0, vt = m_vertices.begin(); vt != vEnd; ++vt, ++v) vt->setIndex(v);

  tcg::list<edge_type>::iterator et, eEnd(m_edges.end());
  for (e = 0, et = m_edges.begin(); et != eEnd; ++et, ++e) et->setIndex(e);

  // Update stored indices
  for (et = m_edges.begin(); et != eEnd; ++et) {
    edge_type &ed = *et;
    ed.setVertex(0, vertex(ed.vertex(0)).getIndex());
    ed.setVertex(1, vertex(ed.vertex(1)).getIndex());
  }

  for (vt = m_vertices.begin(); vt != vEnd; ++vt) {
    vertex_type &vx = *vt;

    if (vt->m_parent >= 0) vt->m_parent = vertex(vt->m_parent).getIndex();

    vertex_type::edges_iterator vet, veEnd(vx.edgesEnd());
    for (vet = vx.edgesBegin(); vet != veEnd; ++vet)
      *vet = edge(*vet).getIndex();
  }

  // Finally, rebuild the actual containers
  if (!m_edges.empty()) {
    tcg::list<edge_type> temp(m_edges.begin(), m_edges.end());
    std::swap(m_edges, temp);
  }

  if (!m_vertices.empty()) {
    tcg::list<vertex_type> temp(m_vertices.begin(), m_vertices.end());
    std::swap(m_vertices, temp);
  }
}

//-------------------------------------------------------------------------------

void PlasticSkeleton::saveData(TOStream &os) {
  // NOTE: Primitives saved by INDEX iteration is NOT COINCIDENTAL - since
  // the lists' internal linking could have been altered to mismatch the
  // natural indexing referred to by primitives' data.

  if (m_vertices.size() != m_vertices.nodesCount() ||
      m_edges.size() != m_edges.nodesCount()) {
    // Ensure that there are no holes in the representation
    PlasticSkeleton skel(*this);

    skel.squeeze();
    skel.saveData(os);

    return;
  }

  // Save vertices
  os.openChild("V");
  {
    int vCount = int(m_vertices.size());
    os << vCount;

    for (int v = 0; v != vCount; ++v) os.child("Vertex") << m_vertices[v];
  }
  os.closeChild();

  // Save edges
  os.openChild("E");
  {
    int eCount = int(m_edges.size());
    os << eCount;

    for (int e = 0; e != eCount; ++e) {
      PlasticSkeleton::edge_type &ed = m_edges[e];
      os << ed.vertex(0) << ed.vertex(1);
    }
  }
  os.closeChild();
}

//-------------------------------------------------------------------------------

void PlasticSkeleton::loadData(TIStream &is) {
  // Ensure the skeleton is clean
  clear();

  // Load vertices
  std::string str;
  int i, size;

  while (is.openChild(str)) {
    if (str == "V") {
      is >> size;

      std::vector<int> acquiredVertexNumbers;
      acquiredVertexNumbers.reserve(size);

      m_vertices.reserve(size);

      for (i = 0; i < size; ++i) {
        if (is.openChild(str) && (str == "Vertex")) {
          vertex_type vx;

          is >> vx;
          int idx = mesh_type::addVertex(vx);

          if (vx.m_number < 0) vertex(idx).m_number = vx.m_number = idx + 1;

          acquiredVertexNumbers.push_back(vx.m_number);

          is.matchEndTag();
        } else
          assert(false), is.skipCurrentTag();
      }

      m_imp->m_numbersPool = tcg::indices_pool<int>(
          acquiredVertexNumbers.begin(), acquiredVertexNumbers.end());

      is.matchEndTag();
    } else if (str == "E") {
      is >> size;

      m_edges.reserve(size);
      int v0, v1;

      for (i = 0; i < size; ++i) {
        is >> v0 >> v1;

        addEdge(edge_type(v0, v1));
        vertex(v1).m_parent = v0;
      }

      is.matchEndTag();
    } else
      assert(false), is.skipCurrentTag();
  }
}

//************************************************************************************
//    PlasticSkeleton  utility functions
//************************************************************************************

int PlasticSkeleton::closestVertex(const TPointD &pos, double *dist) const {
  // Traverse vertices
  double d2, minDist2 = (std::numeric_limits<double>::max)();
  int v = -1;

  tcg::list<vertex_type>::const_iterator vt, vEnd(m_vertices.end());
  for (vt = m_vertices.begin(); vt != vEnd; ++vt) {
    d2                          = tcg::point_ops::dist2(pos, vt->P());
    if (d2 < minDist2) minDist2 = d2, v = int(vt.m_idx);
  }

  if (dist && v >= 0) *dist = sqrt(minDist2);

  return v;
}

//-------------------------------------------------------------------------------

int PlasticSkeleton::closestEdge(const TPointD &pos, double *dist) const {
  // Traverse edges
  double d, minDist = (std::numeric_limits<double>::max)();
  int e = -1;

  tcg::list<edge_type>::const_iterator et, eEnd(m_edges.end());
  for (et = m_edges.begin(); et != eEnd; ++et) {
    const TPointD &vp0 = vertex(et->vertex(0)).P(),
                  &vp1 = vertex(et->vertex(1)).P();

    d                        = tcg::point_ops::segDist(vp0, vp1, pos);
    if (d < minDist) minDist = d, e = int(et.m_idx);
  }

  if (dist && e >= 0) *dist = minDist;

  return e;
}

//-------------------------------------------------------------------------------

std::vector<PlasticHandle> PlasticSkeleton::verticesToHandles() const {
  std::vector<PlasticHandle> v;
  for (auto const &e : m_vertices) {
    v.push_back(e);
  }
  return v;
}
