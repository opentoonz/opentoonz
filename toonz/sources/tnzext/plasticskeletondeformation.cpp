

// TnzCore includes
#include "tstream.h"

// TnzBase includes
#include "tdoublekeyframe.h"
#include "tparamchange.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticdeformerstorage.h"

// tcg includes
#include "tcg/tcg_misc.h"

// STL includes
#include <memory>
#include <set>
#include <map>

// Boost includes
#include <boost/bimap.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include "ext/plasticskeletondeformation.h"

PERSIST_IDENTIFIER(PlasticSkeletonVertexDeformation,
                   "PlasticSkeletonVertexDeformation")
PERSIST_IDENTIFIER(PlasticSkeletonDeformation, "PlasticSkeletonDeformation")

DEFINE_CLASS_CODE(PlasticSkeletonDeformation, 121)

//**************************************************************************************
//    Boost-related  stuff
//**************************************************************************************

using namespace boost::multi_index;

namespace {

typedef boost::bimap<int, PlasticSkeletonP> SkeletonSet;

//======================================================================

struct VDKey {
  QString m_name;
  int m_hookNumber;

  mutable std::map<int, int>
      m_vIndices;  //!< Skeleton index to Vertex index map
  mutable SkVD m_vd;
};

//----------------------------------------------------------------------

typedef boost::multi_index_container<
    VDKey,
    indexed_by<

        ordered_unique<tag<QString>, member<VDKey, QString, &VDKey::m_name>>,
        ordered_unique<tag<int>, member<VDKey, int, &VDKey::m_hookNumber>>

        >>
    SkVDSet;

//----------------------------------------------------------------------

typedef SkVDSet::index<int>::type SkVDByHookNumber;

}  // namespace

//**************************************************************************************
//    Local  namespace
//**************************************************************************************

namespace {

static const char *parNames[SkVD::PARAMS_COUNT] = {"Angle", "Distance", "SO"};
static const char *parMeasures[SkVD::PARAMS_COUNT] = {"angle", "fxLength", ""};

//------------------------------------------------------------------

// Extract the angle between edges vParent->v and vGrandParent->vParent
double buildAngle(const PlasticSkeleton &skeleton, int v) {
  const PlasticSkeletonVertex &vx = skeleton.vertex(v);
  int vParent                     = vx.parent();

  assert(vx.parent() >= 0);

  const PlasticSkeletonVertex &vxParent = skeleton.vertex(vParent);
  int vGrandParent                      = vxParent.parent();

  // Build reference orientation
  TPointD dir(1.0, 0.0);  // Standard horizontal axis if no grandParent
  if (vGrandParent >= 0) {
    // Relative orientation
    const PlasticSkeletonVertex &vxGrandParent = skeleton.vertex(vGrandParent);
    dir = vxParent.P() - vxGrandParent.P();
  }

  return tcg::point_ops::angle(dir, vx.P() - vxParent.P()) * M_180_PI;
}

}  // namespace

//**************************************************************************************
//    PlasticSkeletonVertex  implementation
//**************************************************************************************

SkVD::Keyframe SkVD::getKeyframe(double frame) const {
  Keyframe kf;
  for (int p          = 0; p < PARAMS_COUNT; ++p)
    kf.m_keyframes[p] = m_params[p]->getKeyframeAt(frame);

  return kf;
}

//------------------------------------------------------------------

void SkVD::setKeyframe(double frame) {
  for (int p = 0; p < PARAMS_COUNT; ++p)
    m_params[p]->setKeyframe(m_params[p]->getKeyframeAt(frame));
}

//------------------------------------------------------------------

bool SkVD::setKeyframe(const SkVD::Keyframe &values) {
  bool keyWasSet = false;
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (values.m_keyframes[p].m_isKeyframe) {
      m_params[p]->setKeyframe(values.m_keyframes[p]);
      keyWasSet = true;
    }

  return keyWasSet;
}

//------------------------------------------------------------------

bool SkVD::setKeyframe(const SkVD::Keyframe &values, double frame,
                       double easeIn, double easeOut) {
  bool keyWasSet = false;

  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (values.m_keyframes[p].m_isKeyframe) {
      TDoubleKeyframe kf(values.m_keyframes[p]);
      kf.m_frame = frame;

      if (easeIn >= 0.0) kf.m_speedIn   = TPointD(-easeIn, kf.m_speedIn.y);
      if (easeOut >= 0.0) kf.m_speedOut = TPointD(easeOut, kf.m_speedOut.y);

      m_params[p]->setKeyframe(kf);
      keyWasSet = true;
    }

  return keyWasSet;
}

//------------------------------------------------------------------

bool SkVD::isKeyframe(double frame) const {
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (m_params[p]->isKeyframe(frame)) return true;

  return false;
}

//------------------------------------------------------------------

bool SkVD::isFullKeyframe(double frame) const {
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (!m_params[p]->isKeyframe(frame)) return false;

  return true;
}

//------------------------------------------------------------------

void SkVD::deleteKeyframe(double frame) {
  for (int p = 0; p < PARAMS_COUNT; ++p) m_params[p]->deleteKeyframe(frame);
}

//------------------------------------------------------------------

void SkVD::saveData(TOStream &os) {
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (!m_params[p]->isDefault()) os.child(::parNames[p]) << *m_params[p];
}

//------------------------------------------------------------------

void SkVD::loadData(TIStream &is) {
  std::string tagName;

  while (is.matchTag(tagName)) {
    int p;
    for (p = 0; p < PARAMS_COUNT; ++p) {
      if (tagName == parNames[p]) {
        is >> *m_params[p], is.matchEndTag();
        break;
      }
    }

    if (p >= PARAMS_COUNT) is.skipCurrentTag();
  }
}

//**************************************************************************************
//    PlasticSkeletonDeformation::Imp  definition
//**************************************************************************************

class PlasticSkeletonDeformation::Imp final : public TParamObserver {
public:
  PlasticSkeletonDeformation *m_back;  //!< Back-pointer to the interface class

  SkeletonSet m_skeletons;  //!< Skeletons owned by the deformation
  SkVDSet m_vds;            //!< Container of vertex deformations

  TDoubleParamP m_skelIdsParam;  //!< Curve of skeleton ids by xsheet frame

  std::set<TParamObserver *>
      m_observers;  //!< Set of the deformation's observers

  TSyntax::Grammar
      *m_grammar;  //!< The params' grammar. Weird though - it's a VERY
                   //!< occult requirement to TDoubleParams...

  // NOTE: There \a is a deformation even for a skeleton's root node. This is
  // now required due to the
  // onwership of \a multiple skeletons at once. However, its angle and distance
  // params will be unused.

public:
  Imp(PlasticSkeletonDeformation *back);
  ~Imp();

  Imp(PlasticSkeletonDeformation *back, const Imp &other);
  Imp &operator=(const Imp &other);

  PlasticSkeleton &skeleton(int skelId) const;
  SkVD &vertexDeformation(const QString &name) const;

  void attach(int skeletonId, PlasticSkeleton *skeleton);
  void detach(int skeletonId);

  void attachVertex(const QString &name, int skelId, int v);
  void detachVertex(const QString &name, int skelId, int v);
  void rebindVertex(const QString &name, int skelId, const QString &newName);

  void touchParams(SkVD &vd);

  //! Applies stored vertex deformations to the skeleton branch starting at v
  void updateBranchPositions(const PlasticSkeleton &originalSkeleton,
                             PlasticSkeleton &deformedSkeleton, double frame,
                             int v);

  void onChange(
      const TParamChange &change) override;  // Passes param notifications to
                                             // external observers

private:
  // Not directly copy-constructible
  Imp(const Imp &other);
};

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::Imp(PlasticSkeletonDeformation *back)
    : m_back(back), m_skelIdsParam(1.0), m_grammar() {
  m_skelIdsParam->setName("Skeleton Id");
  m_skelIdsParam->addObserver(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::~Imp() {
  m_skelIdsParam->removeObserver(this);

  SkVDSet::iterator dt, dEnd(m_vds.end());
  for (dt = m_vds.begin(); dt != dEnd; ++dt)
    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
      dt->m_vd.m_params[p]->removeObserver(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::Imp(PlasticSkeletonDeformation *back,
                                     const Imp &other)
    : m_back(back), m_skelIdsParam(other.m_skelIdsParam->clone()), m_grammar() {
  m_skelIdsParam->setGrammar(m_grammar);
  m_skelIdsParam->addObserver(this);

  // Clone the skeletons
  SkeletonSet::const_iterator st, sEnd(other.m_skeletons.end());
  for (st = other.m_skeletons.begin(); st != sEnd; ++st)
    m_skeletons.insert(SkeletonSet::value_type(
        st->get_left(), new PlasticSkeleton(*st->get_right())));

  // Clone each parameters curve
  SkVD vd;

  SkVDSet::const_iterator dt, dEnd(other.m_vds.end());
  for (dt = other.m_vds.begin(); dt != dEnd; ++dt) {
    VDKey vdKey = {dt->m_name, dt->m_hookNumber, dt->m_vIndices};

    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
      TDoubleParamP &param = vdKey.m_vd.m_params[p];

      param = TDoubleParamP(dt->m_vd.m_params[p]->clone());

      param->setGrammar(m_grammar);
      param->addObserver(this);
    }

    m_vds.insert(vdKey);
  }
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp &PlasticSkeletonDeformation::Imp::operator=(
    const Imp &other) {
  *m_skelIdsParam = *other.m_skelIdsParam;
  m_skelIdsParam->setGrammar(m_grammar);

  // Take in all curves whose name matches one of the stored ones

  // Traverse known curves
  SkVDSet::iterator dt, dEnd(m_vds.end());
  SkVDSet::const_iterator st, sEnd(other.m_vds.end());

  for (dt = m_vds.begin(); dt != dEnd; ++dt) {
    // Search a corresponding curve in the input ones
    st = other.m_vds.find(dt->m_name);
    if (st != sEnd)
      for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
        TDoubleParam &param = *dt->m_vd.m_params[p];

        param = *st->m_vd.m_params[p];
        param.setGrammar(m_grammar);
      }
  }

  return *this;
}

//------------------------------------------------------------------

PlasticSkeleton &PlasticSkeletonDeformation::Imp::skeleton(int skelId) const {
  SkeletonSet::left_map::const_iterator st(m_skeletons.left.find(skelId));
  assert(st != m_skeletons.left.end());

  return *st->second;
}

//------------------------------------------------------------------

SkVD &PlasticSkeletonDeformation::Imp::vertexDeformation(
    const QString &name) const {
  SkVDSet::const_iterator vdt(m_vds.find(name));
  assert(vdt != m_vds.end());

  return vdt->m_vd;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::attach(int skeletonId,
                                             PlasticSkeleton *skeleton) {
  assert(skeleton);

  // Store the skeleton - acquires shared ownership
  assert(m_skeletons.left.find(skeletonId) == m_skeletons.left.end());

  m_skeletons.insert(SkeletonSet::value_type(skeletonId, skeleton));

  // Deal with vertex deformations and parameter defaults
  tcg::list<PlasticSkeleton::vertex_type> &vertices = skeleton->vertices();
  if (!vertices.empty()) {
    tcg::list<PlasticSkeletonVertex>::iterator vt, vEnd(vertices.end());
    for (vt = vertices.begin(); vt != vEnd; ++vt)
      attachVertex(vt->name(), skeletonId, vt.index());
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::detach(int skeletonId) {
  // First, detach all vertices
  tcg::list<PlasticSkeleton::vertex_type> &vertices =
      skeleton(skeletonId).vertices();
  if (!vertices.empty()) {
    tcg::list<PlasticSkeletonVertex>::iterator vt, vEnd(vertices.end());
    for (vt = vertices.begin(); vt != vEnd; ++vt)
      detachVertex(vt->name(), skeletonId, vt.index());
  }

  // Then, release the skeleton itself
  m_skeletons.left.erase(skeletonId);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::attachVertex(const QString &name,
                                                   int skelId, int v) {
  struct locals {
    static int newHookNumber(Imp *imp) {
      int h = 1;

      SkVDByHookNumber::iterator vdt, vdEnd(imp->m_vds.get<int>().end());
      for (vdt = imp->m_vds.get<int>().begin();
           vdt != vdEnd && vdt->m_hookNumber == h; ++vdt, ++h)
        ;

      return h;
    }

  };  // locals

  // Insert a new VD if necessary
  SkVDSet::iterator vdt(m_vds.find(name));
  if (vdt == m_vds.end()) {
    VDKey vdKey = {name, locals::newHookNumber(this)};
    touchParams(vdKey.m_vd);

    vdt = m_vds.insert(vdKey).first;
  }

  // Register (skelId, v) on the vd
  vdt->m_vIndices.insert(std::make_pair(skelId, v));
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::detachVertex(const QString &name,
                                                   int skelId, int v) {
  SkVDSet::iterator vdt = m_vds.find(name);
  assert(vdt != m_vds.end());

  // Unregister skelId
  int count = vdt->m_vIndices.erase(skelId);
  assert(count > 0);

  if (vdt->m_vIndices.empty()) {
    // De-register as vdt's observer, and release it from stored vds
    SkVD &vd = vdt->m_vd;

    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
      vd.m_params[p]->removeObserver(this);

    m_vds.erase(vdt);
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::rebindVertex(const QString &name,
                                                   int skelId,
                                                   const QString &newName) {
  if (name == newName) return;

  SkVDSet::iterator oldVdt = m_vds.find(name);
  if (oldVdt == m_vds.end()) return;  // We get here when creating a new vertex

  std::map<int, int>::iterator vit = oldVdt->m_vIndices.find(skelId);
  assert(vit != oldVdt->m_vIndices.end());

  int v = vit->second;

  SkVDSet::iterator newVdt = m_vds.find(newName);
  if (newVdt != m_vds.end()) {
    detachVertex(name, skelId, v);
    attachVertex(newName, skelId, v);
  } else {
    // Creating a new vd entry
    if (oldVdt->m_vIndices.size() == 1) {
      // The old entry should be removed - it is actually *purely* renamed
      VDKey vdKey(*oldVdt);
      vdKey.m_name = newName;

      m_vds.erase(name);
      m_vds.insert(vdKey);
    } else {
      // The old entry remains - and data must be copied from there
      detachVertex(name, skelId, v);
      attachVertex(newName, skelId, v);

      newVdt = m_vds.find(newName);  // Fetch the newly created vd

      // Copy the existing vd into the new one
      SkVD &oldVd = oldVdt->m_vd, &newVd = newVdt->m_vd;

      int p, pCount = SkVD::PARAMS_COUNT;
      for (p = 0; p != pCount; ++p) *newVd.m_params[p] = *oldVd.m_params[p];
    }
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::touchParams(SkVD &vd) {
  for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
    if (vd.m_params[p]) continue;

    TDoubleParam *param = new TDoubleParam;
    param->setName(parNames[p]);
    param->setMeasureName(parMeasures[p]);
    param->setGrammar(m_grammar);

    vd.m_params[p] = param;

    param->addObserver(this);
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::onChange(const TParamChange &change) {
  // Since the deformation was changed, any associated deformer
  // must be invalidated (at the animation-deform level only)
  PlasticDeformerStorage::instance()->invalidateDeformation(
      m_back, PlasticDeformerStorage::NONE);

  // Propagate notification to this object's observers
  std::set<TParamObserver *>::iterator ot, oEnd(m_observers.end());
  for (ot = m_observers.begin(); ot != oEnd; ++ot) (*ot)->onChange(change);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::updateBranchPositions(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v) {
  struct locals {
    static void buildParentDirection(const PlasticSkeleton &skel, int v,
                                     TPointD &dir) {
      assert(v >= 0);

      const PlasticSkeletonVertex &vx = skel.vertex(v);

      int vParent = vx.parent();
      if (vParent < 0) return;  // dir remains as passed

      const TPointD &dir_ =
          tcg::point_ops::direction(skel.vertex(vParent).P(), vx.P(), 1e-4);

      if (dir_ != tcg::point_ops::NaP<TPointD>())
        dir = dir_;
      else
        buildParentDirection(skel, vParent, dir);
    }
  };  // locals

  PlasticSkeletonVertex &dvx = deformedSkeleton.vertex(v);
  int vParent                = dvx.parent();
  if (vParent >= 0) {
    // Rebuild vertex position
    const TPointD &ovxPos       = originalSkeleton.vertex(v).P();
    const TPointD &ovxParentPos = originalSkeleton.vertex(vParent).P();

    // Start by getting the polar reference
    TPointD oDir(1.0, 0.0), dDir(1.0, 0.0);

    locals::buildParentDirection(originalSkeleton, vParent, oDir);
    locals::buildParentDirection(deformedSkeleton, vParent, dDir);

    // Now, rebuild vx's position
    const SkVD &vd = m_vds.find(dvx.name())->m_vd;

    double a = tcg::point_ops::angle(oDir, ovxPos - ovxParentPos) * M_180_PI;
    double d = tcg::point_ops::dist(ovxParentPos, ovxPos);

    double aDelta = vd.m_params[SkVD::ANGLE]->getValue(frame);
    double dDelta = vd.m_params[SkVD::DISTANCE]->getValue(frame);

    dvx.P() = deformedSkeleton.vertex(vParent).P() +
              (d + dDelta) * (TRotation(a + aDelta) * dDir);
  }

  // Finally, update children positions
  PlasticSkeleton::vertex_type::edges_iterator et, eEnd(dvx.edgesEnd());
  for (et = dvx.edgesBegin(); et != eEnd; ++et) {
    int vChild = deformedSkeleton.edge(*et).vertex(1);
    if (vChild == v) continue;

    updateBranchPositions(originalSkeleton, deformedSkeleton, frame, vChild);
  }
}

//**************************************************************************************
//    PlasticSkeletonDeformation  implementation
//**************************************************************************************

PlasticSkeletonDeformation::PlasticSkeletonDeformation()
    : m_imp(new Imp(this)) {}

//------------------------------------------------------------------

PlasticSkeletonDeformation::PlasticSkeletonDeformation(
    const PlasticSkeletonDeformation &other)
    : TSmartObject(m_classCode), m_imp(new Imp(this, *other.m_imp)) {
  // Register deformation
  SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
  for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st)
    st->get_right()->addListener(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::~PlasticSkeletonDeformation() {
  // Unregister deformation
  SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
  for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st)
    st->get_right()->removeListener(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation &PlasticSkeletonDeformation::operator=(
    const PlasticSkeletonDeformation &other) {
  // The meaning of operator= is DIFFERENT from that implemented in the copy
  // constructor.
  // Skeletons are NOT cloned.

  *m_imp = *other.m_imp;
  return *this;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::empty() const {
  return m_imp->m_skeletons.empty();
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonsCount() const {
  return m_imp->m_skeletons.size();
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::skeletonIds(skelId_iterator &begin,
                                             skelId_iterator &end) const {
  auto const f = [](const SkeletonSet::left_map::value_type &val) {
    return val.first;
  };

  begin = boost::make_transform_iterator(m_imp->m_skeletons.left.begin(), f);
  end   = boost::make_transform_iterator(m_imp->m_skeletons.left.end(), f);
}

//------------------------------------------------------------------

TDoubleParamP PlasticSkeletonDeformation::skeletonIdsParam() const {
  return m_imp->m_skelIdsParam;
}

//------------------------------------------------------------------

PlasticSkeletonP PlasticSkeletonDeformation::skeleton(double frame) const {
  return skeleton(skeletonId(frame));
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonId(double frame) const {
  return m_imp->m_skelIdsParam->getValue(frame);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::attach(int skeletonId,
                                        PlasticSkeleton *skeleton) {
  m_imp->attach(skeletonId, skeleton);

  skeleton->addListener(this);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::detach(int skeletonId) {
  SkeletonSet::left_map::iterator st(m_imp->m_skeletons.left.find(skeletonId));
  if (st != m_imp->m_skeletons.left.end()) {
    st->second->removeListener(this);
    m_imp->detach(skeletonId);
  }
}

//------------------------------------------------------------------

PlasticSkeletonP PlasticSkeletonDeformation::skeleton(int skeletonId) const {
  SkeletonSet::left_map::const_iterator st =
      m_imp->m_skeletons.left.find(skeletonId);
  return (st == m_imp->m_skeletons.left.end()) ? PlasticSkeletonP()
                                               : st->second;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonId(PlasticSkeleton *skeleton) const {
  SkeletonSet::right_map::const_iterator st =
      m_imp->m_skeletons.right.find(skeleton);
  return (st == m_imp->m_skeletons.right.end())
             ? -(std::numeric_limits<int>::max)()
             : st->second;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::vertexDeformationsCount() const {
  return m_imp->m_vds.size();
}

//------------------------------------------------------------------

SkVD *PlasticSkeletonDeformation::vertexDeformation(
    const QString &vxName) const {
  SkVDSet::const_iterator vdt = m_imp->m_vds.find(vxName);
  return (vdt == m_imp->m_vds.end()) ? (SkVD *)0 : &vdt->m_vd;
}

//------------------------------------------------------------------

SkVD *PlasticSkeletonDeformation::vertexDeformation(int skelId, int v) const {
  const QString &name = skeleton(skelId)->vertex(v).name();
  return vertexDeformation(name);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vertexDeformations(vd_iterator &begin,
                                                    vd_iterator &end) const {
  auto const f = [](const VDKey &vdKey) {
    return std::make_pair(&vdKey.m_name, &vdKey.m_vd);
  };

  begin = boost::make_transform_iterator(m_imp->m_vds.begin(), f);
  end   = boost::make_transform_iterator(m_imp->m_vds.end(), f);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vdSkeletonVertices(const QString &vertexName,
                                                    vx_iterator &begin,
                                                    vx_iterator &end) const {
  auto const f = [](const std::map<int, int>::value_type &val) {
    return std::make_pair(val.first, val.second);
  };

  SkVDSet::const_iterator nt(m_imp->m_vds.find(vertexName));

  if (nt == m_imp->m_vds.end()) {
    begin = vx_iterator();
    end   = vx_iterator();
  } else {
    begin = boost::make_transform_iterator(nt->m_vIndices.begin(), f);
    end   = boost::make_transform_iterator(nt->m_vIndices.end(), f);
  }
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::hookNumber(const QString &name) const {
  SkVDSet::const_iterator nt(m_imp->m_vds.find(name));
  return (nt == m_imp->m_vds.end()) ? -1 : nt->m_hookNumber;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::hookNumber(int skelId, int v) const {
  const QString &name = skeleton(skelId)->vertex(v).name();
  return hookNumber(name);
}

//------------------------------------------------------------------

QString PlasticSkeletonDeformation::vertexName(int hookNumber) const {
  const SkVDByHookNumber &vds = m_imp->m_vds.get<int>();

  SkVDByHookNumber::const_iterator ht(vds.find(hookNumber));
  return (ht == vds.end()) ? QString() : ht->m_name;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::vertexIndex(int hookNumber, int skelId) const {
  const SkVDByHookNumber &vds = m_imp->m_vds.get<int>();

  SkVDByHookNumber::const_iterator ht(vds.find(hookNumber));
  if (ht == vds.end()) return -1;

  std::map<int, int>::const_iterator st(ht->m_vIndices.find(skelId));
  return (st == ht->m_vIndices.end()) ? -1 : st->second;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::getKeyframeAt(double frame,
                                               SkDKey &keyframe) const {
  keyframe.m_skelIdKeyframe = m_imp->m_skelIdsParam->getKeyframeAt(frame);
  keyframe.m_vertexKeyframes.clear();

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    keyframe.m_vertexKeyframes.insert(
        std::make_pair(dt->m_name, dt->m_vd.getKeyframe(frame)));
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setKeyframe(double frame) {
  m_imp->m_skelIdsParam->setKeyframe(frame);

  SkVDSet::iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt) dt->m_vd.setKeyframe(frame);
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::setKeyframe(const SkDKey &keyframe) {
  bool keyWasSet = false;

  if (keyframe.m_skelIdKeyframe.m_isKeyframe) {
    m_imp->m_skelIdsParam->setKeyframe(keyframe.m_skelIdKeyframe);
    keyWasSet = true;
  }

  const std::map<QString, SkVD::Keyframe> &vdKeys = keyframe.m_vertexKeyframes;

  // Iterate the keyframe's vertex deformations
  std::map<QString, SkVD::Keyframe>::const_iterator kt, kEnd(vdKeys.end());
  for (kt = vdKeys.begin(); kt != vdKeys.end(); ++kt) {
    // Search for a corresponding vertex deformation among stored ones
    SkVDSet::iterator vdt = m_imp->m_vds.find(kt->first);
    if (vdt != m_imp->m_vds.end()) {
      // Set the corresponding keyframe
      keyWasSet = vdt->m_vd.setKeyframe(kt->second) || keyWasSet;
    }
  }

  return keyWasSet;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::setKeyframe(const SkDKey &keyframe,
                                             double frame, double easeIn,
                                             double easeOut) {
  bool keyWasSet = false;

  if (keyframe.m_skelIdKeyframe.m_isKeyframe) {
    TDoubleKeyframe kf(keyframe.m_skelIdKeyframe);
    kf.m_frame = frame;

    m_imp->m_skelIdsParam->setKeyframe(kf);
    keyWasSet = true;
  }

  const std::map<QString, SkVD::Keyframe> &vdKeys = keyframe.m_vertexKeyframes;

  // Iterate the keyframe's vertex deformations
  std::map<QString, SkVD::Keyframe>::const_iterator kt, kEnd(vdKeys.end());
  for (kt = vdKeys.begin(); kt != vdKeys.end(); ++kt) {
    // Search for a corresponding vertex deformation among stored ones
    SkVDSet::iterator vdt = m_imp->m_vds.find(kt->first);
    if (vdt != m_imp->m_vds.end()) {
      // Set the corresponding keyframe
      keyWasSet = vdt->m_vd.setKeyframe(kt->second, frame, easeIn, easeOut) ||
                  keyWasSet;
    }
  }

  return keyWasSet;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::isKeyframe(double frame) const {
  if (m_imp->m_skelIdsParam->isKeyframe(frame)) return true;

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    if (dt->m_vd.isKeyframe(frame)) return true;

  return false;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::isFullKeyframe(double frame) const {
  if (!m_imp->m_skelIdsParam->isKeyframe(frame)) return false;

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    if (!dt->m_vd.isFullKeyframe(frame)) return false;

  return true;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::deleteKeyframe(double frame) {
  m_imp->m_skelIdsParam->deleteKeyframe(frame);

  SkVDSet::iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    dt->m_vd.deleteKeyframe(frame);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::storeDeformedSkeleton(
    int skelId, double frame, PlasticSkeleton &skeleton) const {
  // Copy the un-deformed skeleton to the output one
  const PlasticSkeletonP &origSkel = this->skeleton(skelId);
  skeleton                         = origSkel ? *origSkel : PlasticSkeleton();

  // Update the skeleton to the specified frame
  if (!skeleton.vertices().empty())
    m_imp->updateBranchPositions(*origSkel, skeleton, frame,
                                 skeleton.vertices().begin().m_idx);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::updatePosition(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v, const TPointD &pos) {
  const PlasticSkeletonVertex &vx = deformedSkeleton.vertex(v);
  int vParent                     = vx.parent();

  const TPointD &vParentPos = deformedSkeleton.vertex(vParent).P();
  const TPointD &vPos       = deformedSkeleton.vertex(v).P();

  SkVD &vd = m_imp->m_vds.find(vx.name())->m_vd;

  // NOTE: The following aDelta calculation should be done as a true difference
  // - this is still ok and spares
  // access to v's grandParent...

  double aDelta = tcg::point_ops::angle(vPos - vParentPos, pos - vParentPos) *
                  M_180_PI,
         dDelta = tcg::point_ops::dist(vParentPos, pos) -
                  tcg::point_ops::dist(vParentPos, vPos),

         a = tcrop(vd.m_params[SkVD::ANGLE]->getValue(frame) + aDelta,
                   vx.m_minAngle, vx.m_maxAngle),
         d = vd.m_params[SkVD::DISTANCE]->getValue(frame) + dDelta;

  vd.m_params[SkVD::ANGLE]->setValue(frame, a);
  vd.m_params[SkVD::DISTANCE]->setValue(frame, d);

  m_imp->updateBranchPositions(originalSkeleton, deformedSkeleton, frame, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::updateAngle(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v, const TPointD &pos) {
  const PlasticSkeletonVertex &vx = deformedSkeleton.vertex(v);
  int vParent                     = vx.parent();

  const TPointD &vParentPos = deformedSkeleton.vertex(vParent).P();

  // No need to access the grandParent, we're making the diff against the old vx
  // position

  SkVD &vd = m_imp->m_vds.find(vx.name())->m_vd;

  double aDelta = tcg::point_ops::angle(vx.P() - vParentPos, pos - vParentPos) *
                  M_180_PI,
         a = tcrop(vd.m_params[SkVD::ANGLE]->getValue(frame) + aDelta,
                   vx.m_minAngle, vx.m_maxAngle);

  vd.m_params[SkVD::ANGLE]->setValue(frame, a);

  m_imp->updateBranchPositions(originalSkeleton, deformedSkeleton, frame, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::addVertex(PlasticSkeleton *skel, int v) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->attachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::insertVertex(PlasticSkeleton *skel, int v) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->attachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::deleteVertex(PlasticSkeleton *skel, int v) {
  assert(v > 0);  // Root should not be deleted

  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  // Remove the vertex deformation associated with v
  m_imp->detachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vertexNameChange(PlasticSkeleton *skel, int v,
                                                  const QString &newName) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->rebindVertex(skel->vertex(v).name(), skelId, newName);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::clear(PlasticSkeleton *skel) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->detach(skelId);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::addObserver(TParamObserver *observer) {
  m_imp->m_observers.insert(observer);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::removeObserver(TParamObserver *observer) {
  m_imp->m_observers.erase(observer);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setGrammar(TSyntax::Grammar *grammar) {
  SkVDSet::iterator vdt, vdEnd(m_imp->m_vds.end());
  for (vdt = m_imp->m_vds.begin(); vdt != vdEnd; ++vdt) {
    SkVD &vd = vdt->m_vd;

    for (int c = 0; c != SkVD::PARAMS_COUNT; ++c)
      vd.m_params[c]->setGrammar(grammar);
  }

  m_imp->m_skelIdsParam->setGrammar(grammar);

  m_imp->m_grammar = grammar;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::saveData(TOStream &os) {
  // Save skeleton vertex deformations
  os.openChild("VertexDeforms");  // These are saved *before* skeletons
  {                               // ON PURPOSE - in loadData(), we'll
    SkVDSet::iterator vdt,
        vdEnd(m_imp->m_vds.end());  // attach() skeletons to a completely
    for (vdt = m_imp->m_vds.begin(); vdt != vdEnd;
         ++vdt)  // rebuilt set of vertex deformations.
    {
      os.child("Name") << vdt->m_name;
      os.child("Hook") << vdt->m_hookNumber;
      os.child("VD") << vdt->m_vd;
    }
  }
  os.closeChild();

  // Save skeleton ids param
  os.child("SkelIdsParam") << *m_imp->m_skelIdsParam;

  // Save skeletons
  os.openChild("Skeletons");
  {
    SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
    for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st) {
      os.child("SkelId") << st->get_left();
      os.child("Skeleton") << *st->get_right();
    }
  }
  os.closeChild();
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::loadData(TIStream &is) {
  if (is.getVersion() < VersionNumber(1, 21)) {
    loadData_prerelease(is);
    return;
  }

  int skeletonId;
  PlasticSkeleton *skeleton;

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "VertexDeforms") {
      VDKey vKey;

      while (is.openChild(tagName)) {
        if (tagName == "Name")
          is >> vKey.m_name, is.matchEndTag();
        else if (tagName == "Hook")
          is >> vKey.m_hookNumber, is.matchEndTag();
        else if (tagName == "VD") {
          m_imp->touchParams(vKey.m_vd);
          is >> vKey.m_vd, is.matchEndTag();

          m_imp->m_vds.insert(vKey);

          vKey = VDKey();
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else if (tagName == "SkelIdsParam")
      is >> *m_imp->m_skelIdsParam, is.matchEndTag();
    else if (tagName == "Skeletons") {
      while (is.openChild(tagName)) {
        if (tagName == "SkelId")
          is >> skeletonId, is.matchEndTag();
        else if (tagName == "Skeleton") {
          skeleton = new PlasticSkeleton;
          is >> *skeleton, is.matchEndTag();

          attach(skeletonId, skeleton);
          skeletonId = 0, skeleton = 0;
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::loadData_prerelease(TIStream &is) {
  struct locals {
    static void buildParentDirection(const PlasticSkeleton &skel, int v,
                                     TPointD &dir) {
      assert(v >= 0);

      const PlasticSkeletonVertex &vx = skel.vertex(v);

      int vParent = vx.parent();
      if (vParent < 0) return;  // dir remains as passed

      const TPointD &dir_ =
          tcg::point_ops::direction(skel.vertex(vParent).P(), vx.P(), 1e-4);

      if (dir_ != tcg::point_ops::NaP<TPointD>())
        dir = dir_;
      else
        buildParentDirection(skel, vParent, dir);
    }

    //------------------------------------------------------------------------------

    static void adjust(SkD &sd, int v) {
      PlasticSkeleton &skeleton = *sd.skeleton(1);

      PlasticSkeletonVertex &vx = skeleton.vertex(v);
      int vParent               = vx.parent();
      if (vParent >= 0) {
        // Rebuild vertex position
        const TPointD &vxPos       = skeleton.vertex(v).P();
        const TPointD &vxParentPos = skeleton.vertex(vParent).P();

        // Start by getting the polar reference
        TPointD dir(1.0, 0.0);

        buildParentDirection(skeleton, vParent, dir);

        // Now, rebuild vx's position
        SkVD &vd = sd.m_imp->m_vds.find(vx.name())->m_vd;

        double a = tcg::point_ops::angle(dir, vxPos - vxParentPos) * M_180_PI;
        double d = tcg::point_ops::dist(vxParentPos, vxPos);

        {
          TDoubleParamP param(vd.m_params[SkVD::ANGLE]);
          param->setDefaultValue(0.0);

          int k, kCount = param->getKeyframeCount();
          for (k = 0; k != kCount; ++k) {
            TDoubleKeyframe kf(param->getKeyframe(k));
            kf.m_value -= a;
            param->setKeyframe(k, kf);
          }
        }

        {
          TDoubleParamP param(vd.m_params[SkVD::DISTANCE]);
          param->setDefaultValue(0.0);

          int k, kCount = param->getKeyframeCount();
          for (k = 0; k != kCount; ++k) {
            TDoubleKeyframe kf(param->getKeyframe(k));
            kf.m_value -= d;
            param->setKeyframe(k, kf);
          }
        }
      }

      // Finally, update children positions
      PlasticSkeleton::vertex_type::edges_iterator et, eEnd(vx.edgesEnd());
      for (et = vx.edgesBegin(); et != eEnd; ++et) {
        int vChild = skeleton.edge(*et).vertex(1);
        if (vChild == v) continue;

        adjust(sd, vChild);
      }
    }
  };  // locals

  PlasticSkeletonP skeleton(new PlasticSkeleton);

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "Skeleton")
      is >> *skeleton, is.matchEndTag();
    else if (tagName == "VertexDeforms") {
      while (is.openChild(tagName)) {
        if (tagName == "VD") {
          VDKey vKey;
          m_imp->touchParams(vKey.m_vd);

          is >> vKey.m_name, is >> vKey.m_vd;
          is.closeChild();

          // Rebuild vKey's data from skeleton
          int v, vCount = skeleton->verticesCount();
          for (v = 0; v != vCount; ++v)
            if (skeleton->vertex(v).name() == vKey.m_name) break;

          assert(v < vCount);
          vKey.m_hookNumber = skeleton->vertex(v).number();

          m_imp->m_vds.insert(vKey);
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }

  attach(1, skeleton.getPointer());

  // SkVD params are now intended as deltas. Adjusting...
  locals::adjust(*this, 0);
}
