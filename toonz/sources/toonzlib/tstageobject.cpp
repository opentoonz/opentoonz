

#include "toonz/tstageobject.h"

// TnzLib includes
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/txsheet.h"
#include "toonz/observer.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshcell.h"
#include "toonz/stage.h"
#include "toonz/tcamera.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/tpinnedrangeset.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"
#include "ext/plasticdeformerstorage.h"

// TnzCore includes
#include "tstream.h"
#include "tstroke.h"
#include "tconvert.h"
#include "tundo.h"
#include "tconst.h"

// Qt includes
#include <QMetaObject>

// STD includes
#include <fstream>
#include <set>

using namespace std;

//
// Per un problema su alcune macchine, solo Release.
// il problema si verifica ruotando gli oggetti sul camera stand
//

#ifdef _MSC_VER
#pragma optimize("", off)
#endif
static TAffine makeRotation(double ang) { return TRotation(ang); }
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

DEFINE_CLASS_CODE(TStageObject, 102)

//************************************************************************************************
//    Local namespace
//************************************************************************************************

namespace {

enum StageObjectType {
  NONE   = 0,
  CAMERA = 1,
  TABLE  = 2,
  PEGBAR = 5,
  COLUMN = 6
};

const int StageObjectTypeShift = 28;
const int StageObjectMaxIndex  = ((1 << StageObjectTypeShift) - 1);
const int StageObjectIndexMask = ((1 << StageObjectTypeShift) - 1);

}  // namespace

//************************************************************************************************
//    TStageObjectParams  implementation
//************************************************************************************************

TStageObjectParams::TStageObjectParams() : m_spline(0), m_noScaleZ(0) {}

//-----------------------------------------------------------------------------

TStageObjectParams::TStageObjectParams(TStageObjectParams *data)
    : m_id(data->m_id)
    , m_parentId(data->m_parentId)
    , m_children(data->m_children)
    , m_keyframes(data->m_keyframes)
    , m_cycleEnabled(data->m_cycleEnabled)
    , m_spline(data->m_spline)
    , m_status(data->m_status)
    , m_handle(data->m_handle)
    , m_parentHandle(data->m_parentHandle)
    , m_x(data->m_x)
    , m_y(data->m_y)
    , m_z(data->m_z)
    , m_so(data->m_so)
    , m_rot(data->m_rot)
    , m_scalex(data->m_scalex)
    , m_scaley(data->m_scaley)
    , m_scale(data->m_scale)
    , m_posPath(data->m_posPath)
    , m_shearx(data->m_shearx)
    , m_sheary(data->m_sheary)
    , m_skeletonDeformation(data->m_skeletonDeformation)
    , m_noScaleZ(data->m_noScaleZ)
    , m_center(data->m_center)
    , m_offset(data->m_offset)
    , m_name(data->m_name)
    , m_isOpened(data->m_isOpened)
    , m_pinnedRangeSet(data->m_pinnedRangeSet->clone()) {}

//---------------------------------------------------------------------------

TStageObjectParams::~TStageObjectParams() { delete m_pinnedRangeSet; }

//---------------------------------------------------------------------------

TStageObjectParams *TStageObjectParams::clone() {
  return new TStageObjectParams(this);
}

//===========================================================================
// Utility Functions
//---------------------------------------------------------------------------

TStageObjectId toStageObjectId(string s) {
  if (s == "None")
    return TStageObjectId::NoneId;
  else if (s == "Table")
    return TStageObjectId::TableId;
  else if (isInt(s)) {
    TStageObjectId id;
    id.setCode(std::stoi(s));
    return id;
  } else if (s.length() > 3) {
    if (s.substr(0, 3) == "Col")
      return TStageObjectId::ColumnId(std::stoi(s.substr(3)) - 1);
    else if (s.substr(0, 3) == "Peg")
      return TStageObjectId::PegbarId(std::stoi(s.substr(3)) - 1);
    else if (s.length() > 6 && s.substr(0, 6) == "Camera")
      return TStageObjectId::CameraId(std::stoi(s.substr(6)) - 1);
  }
  return TStageObjectId::NoneId;
}

//-----------------------------------------------------------------------------

ostream &operator<<(ostream &out, const TStageObjectId &id) {
  return out << id.toString();
}

//************************************************************************************************
//    TStageObjectId  implementation
//************************************************************************************************

TStageObjectId::TStageObjectId() : m_id(NONE << StageObjectTypeShift) {}

//-----------------------------------------------------------------------------

TStageObjectId::~TStageObjectId() {}

//-----------------------------------------------------------------------------

bool TStageObjectId::isCamera() const {
  return m_id >> StageObjectTypeShift == CAMERA;
}

//-----------------------------------------------------------------------------

bool TStageObjectId::isTable() const {
  return m_id >> StageObjectTypeShift == TABLE;
}

//-----------------------------------------------------------------------------

bool TStageObjectId::isPegbar() const {
  return m_id >> StageObjectTypeShift == PEGBAR;
}

//-----------------------------------------------------------------------------

bool TStageObjectId::isColumn() const {
  return m_id >> StageObjectTypeShift == COLUMN;
}

//-----------------------------------------------------------------------------

const TStageObjectId TStageObjectId::NoneId(NONE << StageObjectTypeShift);

//-----------------------------------------------------------------------------

const TStageObjectId TStageObjectId::TableId(TABLE << StageObjectTypeShift);

//-----------------------------------------------------------------------------

const TStageObjectId TStageObjectId::CameraId(int index) {
  assert(0 <= index && index <= StageObjectMaxIndex);
  return TStageObjectId(CAMERA << StageObjectTypeShift | index);
}

//-----------------------------------------------------------------------------

const TStageObjectId TStageObjectId::PegbarId(int index) {
  assert(0 <= index && index <= StageObjectMaxIndex);
  return TStageObjectId(PEGBAR << StageObjectTypeShift | index);
}

//-----------------------------------------------------------------------------

const TStageObjectId TStageObjectId::ColumnId(int index) {
  assert(0 <= index && index <= StageObjectMaxIndex);
  return TStageObjectId(COLUMN << StageObjectTypeShift | index);
}

//-----------------------------------------------------------------------------

string TStageObjectId::toString() const {
  int index = m_id & StageObjectIndexMask;
  string shortName;
  switch (m_id >> StageObjectTypeShift) {
  case NONE:
    shortName = "None";
    break;
  case CAMERA:
    shortName = "Camera" + std::to_string(index + 1);
    break;
  case TABLE:
    shortName = "Table";
    break;
  case PEGBAR:
    shortName = "Peg" + std::to_string(index + 1);
    break;
  case COLUMN:
    shortName = "Col" + std::to_string(index + 1);
    break;
  default:
    shortName = "BadPegbar";
  }
  return shortName;
}

//-----------------------------------------------------------------------------

int TStageObjectId::getIndex() const { return m_id & StageObjectIndexMask; }

//=============================================================================
namespace {  // TStageObject Utility
//-----------------------------------------------------------------------------

TDoubleKeyframe withFrame(TDoubleKeyframe k, double frame) {
  k.m_frame = frame;
  return k;
}

//-----------------------------------------------------------------------------

bool setKeyframe(const TDoubleParamP &param, const TDoubleKeyframe &kf,
                 int frame, const double &easeIn, const double &easeOut) {
  if (!kf.m_isKeyframe) return false;

  TDoubleKeyframe kfCopy = kf;

  kfCopy.m_frame = frame;
  if (easeIn >= 0.0) kfCopy.m_speedIn = TPointD(-easeIn, kfCopy.m_speedIn.y);
  if (easeOut >= 0.0) kfCopy.m_speedOut = TPointD(easeOut, kfCopy.m_speedOut.y);

  param->setKeyframe(kfCopy);
  return true;
}

//-----------------------------------------------------------------------------

void setkey(const TDoubleParamP &param, int frame) {
  KeyframeSetter setter(param.getPointer(), -1, false);
  setter.createKeyframe(frame);
}

//-----------------------------------------------------------------------------

void updateUnit(TDoubleParam *param) {
  for (int i = 0; i < param->getKeyframeCount(); i++) {
    TDoubleKeyframe k = param->getKeyframe(i);
    k.m_value /= Stage::inch;
    param->setKeyframe(i, k);
  }
}

//-----------------------------------------------------------------------------

string convertTo4InchCenterUnits(string handle) {
  // per convenzione un handle del tipo 'a'..'z' utilizza
  // la vecchia convenzione per i centri (4 inch invece di 8)
  if (handle.length() == 1 && 'A' <= handle[0] && handle[0] <= 'Z' &&
      handle[0] != 'B')
    return string(1, handle[0] + 'a' - 'A');
  else
    return handle;
}

//-----------------------------------------------------------------------------

TPointD updateDagPosition(const TPointD &pos, const VersionNumber &tnzVersion) {
  if (tnzVersion < VersionNumber(1, 16)) return TConst::nowhere;
  return pos;
}

//-----------------------------------------------------------------------------

// Update the passed stage object keyframe and keyframe type specification with
// a new keyframe. Returns true if specifications match, false otherwise.
bool touchEaseAndCompare(const TDoubleKeyframe &kf,
                         TStageObject::Keyframe &stageKf,
                         TDoubleKeyframe::Type &type) {
  bool initialization = (type == TDoubleKeyframe::None);

  if (initialization) type = kf.m_type;

  if (kf.m_type != type || (kf.m_type != TDoubleKeyframe::SpeedInOut &&
                            kf.m_type != TDoubleKeyframe::EaseInOut &&
                            (kf.m_prevType != TDoubleKeyframe::None &&
                             kf.m_prevType != TDoubleKeyframe::SpeedInOut &&
                             kf.m_prevType != TDoubleKeyframe::EaseInOut))) {
    stageKf.m_easeIn  = -1.0;
    stageKf.m_easeOut = -1.0;

    return false;
  }

  double easeIn = -kf.m_speedIn.x;
  if (initialization)
    stageKf.m_easeIn = easeIn;
  else if (stageKf.m_easeIn != easeIn)
    stageKf.m_easeIn = -1.0;

  double easeOut = kf.m_speedOut.x;
  if (initialization)
    stageKf.m_easeOut = easeOut;
  else if (stageKf.m_easeOut != easeOut)
    stageKf.m_easeOut = -1.0;

  return true;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//************************************************************************************************
//    TStageObject::LazyData  implementation
//************************************************************************************************

TStageObject::LazyData::LazyData() : m_time(-1.0) {}

//************************************************************************************************
//    TStageObject  implementation
//************************************************************************************************

TStageObject::TStageObject(TStageObjectTree *tree, TStageObjectId id)
    : m_tree(tree)
    , m_id(id)
    , m_parent(0)
    , m_name("")
    , m_isOpened(false)
    , m_spline(0)
    , m_status(XY)
    , m_x(new TDoubleParam())
    , m_y(new TDoubleParam())
    , m_z(new TDoubleParam())
    , m_so(new TDoubleParam())
    , m_rot(new TDoubleParam())
    , m_scalex(new TDoubleParam(1.0))
    , m_scaley(new TDoubleParam(1.0))
    , m_scale(new TDoubleParam(1.0))
    , m_posPath(new TDoubleParam())
    , m_shearx(new TDoubleParam())
    , m_sheary(new TDoubleParam())
    , m_center()
    , m_offset()
    , m_cycleEnabled(false)
    , m_handle("B")
    , m_parentHandle("B")
    , m_dagNodePos(TConst::nowhere)
    , m_camera(0)
    , m_locked(false)
    , m_noScaleZ(0)
    , m_pinnedRangeSet(0)
    , m_ikflag(0)
    , m_groupSelector(-1) {
  // NOTA: per le unita' di misura controlla anche tooloptions.cpp
  m_x->setName("W_X");
  m_x->setMeasureName("length.x");
  m_x->addObserver(this);

  m_y->setName("W_Y");
  m_y->setMeasureName("length.y");
  m_y->addObserver(this);

  m_z->setName("W_Z");
  m_z->setMeasureName(id.isCamera() ? "zdepth.cam" : "zdepth");
  m_z->addObserver(this);

  m_so->setName("W_SO");
  m_so->addObserver(this);

  m_rot->setName("W_Rotation");
  m_rot->setMeasureName("angle");
  m_rot->addObserver(this);

  m_scalex->setName("W_ScaleH");
  m_scalex->setMeasureName("scale");
  m_scalex->addObserver(this);

  m_scaley->setName("W_ScaleV");
  m_scaley->setMeasureName("scale");
  m_scaley->addObserver(this);

  m_scale->setName("W_Scale");
  m_scale->setMeasureName("scale");
  m_scale->addObserver(this);

  m_shearx->setName("W_ShearH");
  m_shearx->setMeasureName("shear");
  m_shearx->addObserver(this);

  m_sheary->setName("W_ShearV");
  m_sheary->setMeasureName("shear");
  m_sheary->addObserver(this);

  m_posPath->setName("posPath");
  m_posPath->setMeasureName("percentage2");
  m_posPath->addObserver(this);

  m_tree->setGrammar(m_x);
  m_tree->setGrammar(m_y);
  m_tree->setGrammar(m_z);
  m_tree->setGrammar(m_so);
  m_tree->setGrammar(m_rot);
  m_tree->setGrammar(m_scalex);
  m_tree->setGrammar(m_scaley);
  m_tree->setGrammar(m_scale);
  m_tree->setGrammar(m_shearx);
  m_tree->setGrammar(m_sheary);
  m_tree->setGrammar(m_posPath);

  if (id.isCamera()) m_camera = new TCamera();

  m_pinnedRangeSet = new TPinnedRangeSet();
}

//-----------------------------------------------------------------------------

TStageObject::~TStageObject() {
  if (m_spline) {
    if (m_posPath) m_spline->removeParam(m_posPath.getPointer());
    m_spline->release();
  }

  if (m_x) m_x->removeObserver(this);
  if (m_y) m_y->removeObserver(this);
  if (m_z) m_z->removeObserver(this);
  if (m_so) m_so->removeObserver(this);
  if (m_rot) m_rot->removeObserver(this);
  if (m_scalex) m_scalex->removeObserver(this);
  if (m_scaley) m_scaley->removeObserver(this);
  if (m_scale) m_scale->removeObserver(this);
  if (m_shearx) m_shearx->removeObserver(this);
  if (m_sheary) m_sheary->removeObserver(this);
  if (m_posPath) m_posPath->removeObserver(this);

  if (m_skeletonDeformation) {
    PlasticDeformerStorage::instance()->releaseDeformationData(
        m_skeletonDeformation.getPointer());
    m_skeletonDeformation->removeObserver(this);
  }

  delete m_camera;
  delete m_pinnedRangeSet;
}

//-----------------------------------------------------------------------------

const TStageObject::LazyData &TStageObject::lazyData() const {
  return m_lazyData([this](LazyData &ld) { this->update(ld); });
}

//-----------------------------------------------------------------------------

TStageObject::LazyData &TStageObject::lazyData() {
  return const_cast<LazyData &>(
      static_cast<const TStageObject &>(*this).lazyData());
}

//-----------------------------------------------------------------------------

void TStageObject::update(LazyData &ld) const {
  if (ld.m_time >= 0.0) invalidate(ld);

  updateKeyframes(ld);
}

//-----------------------------------------------------------------------------

void TStageObject::onChange(const class TParamChange &c) {
  // Rationale: Since a stage object holds many parameters (or par references),
  // it may receive multiple notifications at the same time - but it should
  // actually refresh its data only ONCE for 'em all.

  // Even worse, the stage object may be notified for EACH touched key of just
  // one of its parameters. This means this function gets called A LOT.

  // Thus, we're just SCHEDULING for a data refresh. The actual refresh happens
  // whenever the scheduled data is accessed.

  if (c.m_keyframeChanged)
    m_lazyData.invalidate();  // Both invalidate placement AND keyframes
  else
    invalidate();  // Invalidate placement only
}

//-----------------------------------------------------------------------------

TStageObjectId TStageObject::getId() const { return m_id; }

//-----------------------------------------------------------------------------

double TStageObject::paramsTime(double t) const {
  const KeyframeMap &keyframes = lazyData().m_keyframes;

  if (m_cycleEnabled && keyframes.size() > 1) {
    int firstT = keyframes.begin()->first;
    if (t <= firstT) return t;
    int lastT  = keyframes.rbegin()->first;
    int tRange = lastT - firstT + 1;
    assert(tRange > 0);
    int it    = tfloor(t);
    double ft = t - it;
    return firstT + ((it - firstT) % tRange) + ft;
  } else
    return t;
}

//-----------------------------------------------------------------------------

void TStageObject::setName(const std::string &name) {
  m_name = (name == m_id.toString()) ? std::string() : name;
}

//-----------------------------------------------------------------------------

string TStageObject::getName() const {
  if (m_name != "") return m_name;
  if (!m_id.isColumn()) return m_id.toString();
  return "Col" + std::to_string(m_id.getIndex() + 1);
}

//-----------------------------------------------------------------------------

string TStageObject::getFullName() const {
  string name = getName();
  if (m_id.isColumn()) {
    if (name.find("Col") == 0 && name.length() > 3 &&
        name.find_first_not_of("0123456789", 3) == string::npos)
      return name;
    else
      return name + " (" + std::to_string(m_id.getIndex() + 1) + ")";
  } else
    return name;
}

//-----------------------------------------------------------------------------

void TStageObject::setParent(const TStageObjectId &parentId) {
  assert(m_tree);
  TStageObject *newParent = 0;
  if (parentId != TStageObjectId::NoneId) {
    newParent = m_tree->getStageObject(parentId);
    assert(newParent);

    // cerco di evitare i cicli
    TStageObject *p = newParent;
    while (p->m_parent) {
      if (p->m_parent->getId() == getId()) return;
      p = p->m_parent;
    }
  } else {
    if (!m_id.isCamera() && !m_id.isTable()) {
      newParent = m_tree->getStageObject(TStageObjectId::TableId);
      assert(newParent);
    }
  }

  if (m_parent) m_parent->m_children.remove(this);

  m_parent = newParent;
  if (m_parent) m_parent->m_children.insert(m_parent->m_children.end(), this);
  invalidate();

#ifndef NDEBUG
  if (m_id.isCamera()) {
  } else if (m_id.isTable()) {
    assert(m_parent == 0);
  } else if (m_id.isColumn()) {
    assert(m_parent &&
           (m_parent->m_id.isTable() || m_parent->m_id.isColumn() ||
            m_parent->m_id.isPegbar() || m_parent->m_id.isCamera()));
  } else if (m_id.isPegbar()) {
    assert(m_parent && (m_parent->m_id.isTable() || m_parent->m_id.isCamera() ||
                        m_parent->m_id.isPegbar()));
  } else {
    assert(0);
  }
#endif
}

//-----------------------------------------------------------------------------

void TStageObject::detachFromParent() {
  if (m_parent) m_parent->m_children.remove(this);
  m_parent = 0;
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::attachChildrenToParent(const TStageObjectId &parentId) {
  while (!m_children.empty()) {
    TStageObject *son = *m_children.begin();
    if (son) son->setParent(parentId);
  }
}

//-----------------------------------------------------------------------------

TStageObjectId TStageObject::getParent() const {
  return m_parent ? m_parent->m_id : TStageObjectId();
}

//-----------------------------------------------------------------------------

bool TStageObject::isAncestor(TStageObject *stageObject) const {
  if (!stageObject) return false;
  if (stageObject == (TStageObject *)m_parent)
    return true;
  else if (m_parent == 0)
    return false;
  else
    return m_parent->isAncestor(stageObject);
}

//-----------------------------------------------------------------------------

TStageObjectSpline *TStageObject::getSpline() const { return m_spline; }

//-----------------------------------------------------------------------------

void TStageObject::doSetSpline(TStageObjectSpline *spline) {
  TDoubleParam *param = m_posPath.getPointer();
  bool uppkEnabled    = isUppkEnabled();
  if (!spline) {
    if (uppkEnabled && m_spline) m_spline->removeParam(param);
    if (m_spline) m_spline->release();
    m_spline = 0;
    enablePath(false);
  } else {
    if (m_spline != spline) {
      if (m_spline && uppkEnabled) m_spline->removeParam(param);
      if (m_spline) m_spline->release();
      m_spline = spline;
      m_spline->addRef();
      if (m_spline && uppkEnabled) m_spline->addParam(param);
    }
    if (!isPathEnabled()) enablePath(true);
  }
}

//-----------------------------------------------------------------------------

void TStageObject::setSpline(TStageObjectSpline *spline) {
  doSetSpline(spline);
  TNotifier::instance()->notify(TXsheetChange());
  TNotifier::instance()->notify(TStageChange());
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::setStatus(Status status) {
  if (m_status == status) return;
  bool oldPathEnabled = isPathEnabled();
  bool oldUppkEnabled = isUppkEnabled();
  m_status            = status;
  bool pathEnabled    = isPathEnabled();
  bool uppkEnabled    = isUppkEnabled();

  if (pathEnabled) {
    if (!m_spline)
      doSetSpline(m_tree->createSpline());
    else if (oldUppkEnabled != uppkEnabled) {
      TDoubleParam *param = getParam(T_Path);
      if (uppkEnabled)
        m_spline->addParam(param);
      else
        m_spline->removeParam(param);
    }
  } else {
    doSetSpline(0);
  }
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::enableAim(bool enabled) {
  setStatus((Status)((m_status & ~STATUS_MASK) | (enabled ? PATH_AIM : PATH)));
}

//-----------------------------------------------------------------------------

void TStageObject::enablePath(bool enabled) {
  if (isPathEnabled() != enabled) setStatus(enabled ? PATH : XY);
}

//-----------------------------------------------------------------------------

void TStageObject::enableUppk(bool enabled) {
  assert(isPathEnabled());
  setStatus((Status)((m_status & ~UPPK_MASK) | (enabled ? UPPK_MASK : 0)));
}

//-----------------------------------------------------------------------------

void TStageObject::setCenter(double frame, const TPointD &center) {
  TPointD c = center - getHandlePos(m_handle, (int)frame);

  TAffine aff   = computeLocalPlacement(frame);
  TPointD delta = aff * c - aff * m_center;
  m_center      = c;
  m_offset += delta;
  invalidate();
}

//-----------------------------------------------------------------------------

TPointD TStageObject::getCenter(double frame) const {
  return m_center + getHandlePos(m_handle, (int)frame);
}

//-----------------------------------------------------------------------------

TPointD TStageObject::getOffset() const { return m_offset; }

//-----------------------------------------------------------------------------

void TStageObject::setOffset(const TPointD &off) {
  m_offset = off;
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::getCenterAndOffset(TPointD &center, TPointD &offset) const {
  center = m_center;
  offset = m_offset;
}

//-----------------------------------------------------------------------------

void TStageObject::setCenterAndOffset(const TPointD &center,
                                      const TPointD &offset) {
  m_center = center;
  m_offset = offset;
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::setHandle(const std::string &s) {
  m_handle = s;
  if (!s.empty() && s[0] == 'H') m_offset = m_center = TPointD();

  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::setParentHandle(const std::string &s) {
  m_parentHandle = s;
  invalidate();
}

//-----------------------------------------------------------------------------

TPointD TStageObject::getHandlePos(string handle, int row) const {
  double unit = 8;
  if (handle == "")
    return TPointD();
  else if (handle.length() > 1 && handle[0] == 'H')
    return m_tree->getHandlePos(m_id, handle, row);
  else if (handle.length() == 1 && 'A' <= handle[0] && handle[0] <= 'Z')
    return TPointD(unit * (handle[0] - 'B'), 0);
  else if (handle.length() == 1 && 'a' <= handle[0] && handle[0] <= 'z')
    return TPointD(0.5 * unit * (handle[0] - 'b'), 0);
  else
    return TPointD(0, 0);
}

//-----------------------------------------------------------------------------

bool TStageObject::isKeyframe(int frame) const {
  const KeyframeMap &keyframes = lazyData().m_keyframes;
  return keyframes.find(frame) != keyframes.end();
}

//-----------------------------------------------------------------------------

bool TStageObject::is52FullKeyframe(int frame) const {
  return m_rot->isKeyframe(frame) && m_x->isKeyframe(frame) &&
         m_y->isKeyframe(frame) && m_z->isKeyframe(frame) &&
         m_posPath->isKeyframe(frame) && m_scalex->isKeyframe(frame) &&
         m_scaley->isKeyframe(frame) && m_shearx->isKeyframe(frame) &&
         m_sheary->isKeyframe(frame);
}

//-----------------------------------------------------------------------------

bool TStageObject::isFullKeyframe(int frame) const {
  return m_rot->isKeyframe(frame) && m_x->isKeyframe(frame) &&
         m_y->isKeyframe(frame) && m_z->isKeyframe(frame) &&
         m_so->isKeyframe(frame) && m_posPath->isKeyframe(frame) &&
         m_scalex->isKeyframe(frame) && m_scaley->isKeyframe(frame) &&
         m_scale->isKeyframe(frame) && m_shearx->isKeyframe(frame) &&
         m_sheary->isKeyframe(frame);
}

//-----------------------------------------------------------------------------

TStageObject::Keyframe TStageObject::getKeyframe(int frame) const {
  const KeyframeMap &keyframes = lazyData().m_keyframes;

  std::map<int, TStageObject::Keyframe>::const_iterator it;
  it = keyframes.find(frame);
  if (it == keyframes.end()) {
    TStageObject::Keyframe k;
    k.m_channels[TStageObject::T_Angle]  = m_rot->getValue(frame);
    k.m_channels[TStageObject::T_X]      = m_x->getValue(frame);
    k.m_channels[TStageObject::T_Y]      = m_y->getValue(frame);
    k.m_channels[TStageObject::T_Z]      = m_z->getValue(frame);
    k.m_channels[TStageObject::T_SO]     = m_so->getValue(frame);
    k.m_channels[TStageObject::T_ScaleX] = m_scalex->getValue(frame);
    k.m_channels[TStageObject::T_ScaleY] = m_scaley->getValue(frame);
    k.m_channels[TStageObject::T_Scale]  = m_scale->getValue(frame);
    k.m_channels[TStageObject::T_Path]   = m_posPath->getValue(frame);
    k.m_channels[TStageObject::T_ShearX] = m_shearx->getValue(frame);
    k.m_channels[TStageObject::T_ShearY] = m_sheary->getValue(frame);

    if (m_skeletonDeformation)
      m_skeletonDeformation->getKeyframeAt(frame, k.m_skeletonKeyframe);

    k.m_isKeyframe = false;
    return k;
  } else
    return it->second;
}

//-----------------------------------------------------------------------------

void TStageObject::setKeyframeWithoutUndo(int frame,
                                          const TStageObject::Keyframe &k) {
  KeyframeMap &keyframes = lazyData().m_keyframes;

  bool keyWasSet = false;
  keyWasSet = ::setKeyframe(m_rot, k.m_channels[TStageObject::T_Angle], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_x, k.m_channels[TStageObject::T_X], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_y, k.m_channels[TStageObject::T_Y], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_z, k.m_channels[TStageObject::T_Z], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_so, k.m_channels[TStageObject::T_SO], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_posPath, k.m_channels[TStageObject::T_Path],
                            frame, k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_scalex, k.m_channels[TStageObject::T_ScaleX],
                            frame, k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_scaley, k.m_channels[TStageObject::T_ScaleY],
                            frame, k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_scale, k.m_channels[TStageObject::T_Scale], frame,
                            k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_shearx, k.m_channels[TStageObject::T_ShearX],
                            frame, k.m_easeIn, k.m_easeOut) ||
              keyWasSet;
  keyWasSet = ::setKeyframe(m_sheary, k.m_channels[TStageObject::T_ShearY],
                            frame, k.m_easeIn, k.m_easeOut) ||
              keyWasSet;

  if (m_skeletonDeformation)
    keyWasSet = m_skeletonDeformation->setKeyframe(k.m_skeletonKeyframe, frame,
                                                   k.m_easeIn, k.m_easeOut) ||
                keyWasSet;

  if (keyWasSet) keyframes[frame] = k;

  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::setKeyframeWithoutUndo(int frame) {
  if (isFullKeyframe(frame)) return;

  setkey(m_x, frame);
  setkey(m_y, frame);
  setkey(m_z, frame);
  setkey(m_so, frame);
  setkey(m_posPath, frame);
  setkey(m_rot, frame);
  setkey(m_scalex, frame);
  setkey(m_scaley, frame);
  setkey(m_scale, frame);
  setkey(m_shearx, frame);
  setkey(m_sheary, frame);

  // Plastic keys are currently not *created* by xsheet commands.

  /*if(m_skeletonDeformation)
{
const PlasticSkeleton* skeleton = m_skeletonDeformation->skeleton();
const tcg::list<PlasticSkeleton::vertex_type>& vertices = skeleton->vertices();

tcg::list<PlasticSkeleton::vertex_type>::const_iterator vt,
vEnd(vertices.end());
for(vt = vertices.begin(); vt != vertices.end(); ++vt)
{
SkVD* vd = m_skeletonDeformation->vertexDeformation(vt->name());
assert(vd);

for(int p=0; p<SkVD::PARAMS_COUNT; ++p)
  setkey(vd->m_params[p], frame);
}
}*/
}

//-----------------------------------------------------------------------------

void TStageObject::removeKeyframeWithoutUndo(int frame) {
  KeyframeMap &keyframes = lazyData().m_keyframes;
  double &time           = lazyData().m_time;

  if (!isKeyframe(frame)) return;

  keyframes.erase(frame);
  m_rot->deleteKeyframe(frame);
  m_x->deleteKeyframe(frame);
  m_y->deleteKeyframe(frame);
  m_z->deleteKeyframe(frame);
  m_so->deleteKeyframe(frame);
  m_scalex->deleteKeyframe(frame);
  m_scaley->deleteKeyframe(frame);
  m_scale->deleteKeyframe(frame);
  m_posPath->deleteKeyframe(frame);
  m_shearx->deleteKeyframe(frame);
  m_sheary->deleteKeyframe(frame);

  if (m_skeletonDeformation) m_skeletonDeformation->deleteKeyframe(frame);

  time = -1;
  if ((int)keyframes.size() < 2) m_cycleEnabled = false;

  invalidate();
}

//-----------------------------------------------------------------------------

bool TStageObject::moveKeyframe(int dst, int src) {
  assert(dst != src);
  if (isKeyframe(dst) || !isKeyframe(src)) return false;
  setKeyframeWithoutUndo(dst, getKeyframe(src));
  removeKeyframeWithoutUndo(src);
  assert(isKeyframe(dst));
  assert(!isKeyframe(src));
  invalidate();
  return true;
}

//-----------------------------------------------------------------------------

bool TStageObject::canMoveKeyframes(std::set<int> &frames, int delta) {
  if (delta == 0) return false;
  std::set<int>::iterator it;
  for (it = frames.begin(); it != frames.end(); ++it) {
    int f = *it;
    if (!isKeyframe(f)) return false;
    f += delta;
    if (f < 0) return false;
    if (frames.find(f) == frames.end() && isKeyframe(f)) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

bool TStageObject::moveKeyframes(std::set<int> &frames, int delta) {
  if (!canMoveKeyframes(frames, delta)) return false;
  if (delta < 0) {
    std::set<int>::iterator it;
    for (it = frames.begin(); it != frames.end(); ++it) {
      bool ret = moveKeyframe(*it + delta, *it);
      assert(ret);
    }
  } else {
    std::set<int>::reverse_iterator ti;
    for (ti = frames.rbegin(); ti != frames.rend(); ++ti) {
      bool ret = moveKeyframe(*ti + delta, *ti);
      assert(ret);
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

void TStageObject::getKeyframes(KeyframeMap &keyframes) const {
  keyframes = lazyData().m_keyframes;
}

//-----------------------------------------------------------------------------

bool TStageObject::getKeyframeRange(int &r0, int &r1) const {
  const KeyframeMap &keyframes = lazyData().m_keyframes;

  if (keyframes.empty()) {
    r0 = 0;
    r1 = -1;
    return false;
  }
  r0 = keyframes.begin()->first;
  r1 = keyframes.rbegin()->first;

  return true;
}

//-----------------------------------------------------------------------------

bool TStageObject::getKeyframeSpan(int row, int &r0, double &ease0, int &r1,
                                   double &ease1) const {
  const KeyframeMap &keyframes = lazyData().m_keyframes;

  KeyframeMap::const_iterator it = keyframes.lower_bound(row);
  if (it == keyframes.end() || it == keyframes.begin() || it->first == row) {
    r0    = 0;
    r1    = -1;
    ease0 = ease1 = 0;
    return false;
  }
  r1    = it->first;
  ease1 = it->second.m_easeIn;
  --it;
  r0    = it->first;
  ease0 = it->second.m_easeOut;

  return true;
}

//-----------------------------------------------------------------------------

void TStageObject::setParam(Channel type, double frame, double val) {
  assert(0);
}

//-----------------------------------------------------------------------------

double TStageObject::getParam(Channel type, double frame) const {
  switch (type) {
  case T_Angle:
    return m_rot->getValue(frame);
  case T_X:
    return m_x->getValue(frame);
  case T_Y:
    return m_y->getValue(frame);
  case T_Z:
    return m_z->getValue(frame);
  case T_SO:
    return m_so->getValue(frame);
  case T_ScaleX:
    return m_scalex->getValue(frame);
  case T_ScaleY:
    return m_scaley->getValue(frame);
  case T_Scale:
    return m_scale->getValue(frame);
  case T_Path:
    return m_posPath->getValue(frame);
  case T_ShearX:
    return m_shearx->getValue(frame);
  case T_ShearY:
    return m_sheary->getValue(frame);

  default:
    assert(false);
    return 0;
  }
}

//-----------------------------------------------------------------------------

TDoubleParam *TStageObject::getParam(Channel channel) const {
  switch (channel) {
  case T_X:
    return m_x.getPointer();
  case T_Y:
    return m_y.getPointer();
  case T_Z:
    return m_z.getPointer();
  case T_SO:
    return m_so.getPointer();
  case T_Angle:
    return m_rot.getPointer();
  case T_Path:
    return m_posPath.getPointer();
  case T_ScaleX:
    return m_scalex.getPointer();
  case T_ScaleY:
    return m_scaley.getPointer();
  case T_Scale:
    return m_scale.getPointer();
  case T_ShearX:
    return m_shearx.getPointer();
  case T_ShearY:
    return m_sheary.getPointer();
  default:
    return 0;
  }
}

//-----------------------------------------------------------------------------

PlasticSkeletonDeformationP TStageObject::getPlasticSkeletonDeformation()
    const {
  return m_skeletonDeformation;
}

//-----------------------------------------------------------------------------

void TStageObject::setPlasticSkeletonDeformation(
    const PlasticSkeletonDeformationP &sd) {
  if (m_skeletonDeformation == sd) return;

  if (m_skeletonDeformation) {
    PlasticDeformerStorage::instance()->releaseDeformationData(
        m_skeletonDeformation.getPointer());

    m_skeletonDeformation->setGrammar(0);
    m_skeletonDeformation->removeObserver(this);
  }

  m_skeletonDeformation = sd;

  if (m_skeletonDeformation) {
    m_skeletonDeformation->setGrammar(m_tree->getGrammar());
    m_skeletonDeformation->addObserver(this);
  }
}

//-----------------------------------------------------------------------------

TStageObject *TStageObject::clone() {
  assert(m_tree);
  TStageObjectId pegId    = getId();
  TStageObjectId newPegId = TStageObjectId::NoneId;
  int newIndex            = pegId.getIndex();
  if (pegId.isCamera()) {
    while ((m_tree->getStageObject(TStageObjectId::CameraId(newIndex), false) !=
            0L) ||
           (pegId.getIndex() == newIndex))
      ++newIndex;
    newPegId = TStageObjectId::CameraId(newIndex);
  } else if (pegId.isColumn()) {
    while ((m_tree->getStageObject(TStageObjectId::ColumnId(newIndex), false) !=
            0L) ||
           (pegId.getIndex() == newIndex))
      ++newIndex;
    newPegId = TStageObjectId::ColumnId(newIndex);
  } else if (pegId.isPegbar()) {
    while ((m_tree->getStageObject(TStageObjectId::PegbarId(newIndex), false) !=
            0L) ||
           (pegId.getIndex() == newIndex))
      ++newIndex;
    newPegId = TStageObjectId::PegbarId(newIndex);
  } else {
    assert(!"Unknown stage object type");
    return 0L;
  }
  assert(newPegId != TStageObjectId::NoneId);
  assert(newIndex >= 0);
  assert(newPegId.getIndex() != pegId.getIndex());

  // aggiunge lo stage object clonato nel stage object tree
  TStageObject *clonedPeg = m_tree->getStageObject(newPegId, true);
  TStageObject *cloned    = clonedPeg;
  assert(cloned);
  assert(cloned->m_tree == m_tree);

  KeyframeMap &keyframes       = lazyData().m_keyframes;
  KeyframeMap &clonedKeyframes = cloned->lazyData().m_keyframes;

  std::map<int, TStageObject::Keyframe>::iterator itKf = keyframes.begin();
  for (; itKf != keyframes.end(); ++itKf) {
    clonedKeyframes.insert(make_pair(itKf->first, itKf->second));
  }
  cloned->m_cycleEnabled    = m_cycleEnabled;
  cloned->lazyData().m_time = lazyData().m_time;
  cloned->m_localPlacement  = m_localPlacement;
  cloned->m_absPlacement    = m_absPlacement;
  cloned->m_status          = m_status;
  cloned->doSetSpline(m_spline);

  cloned->m_x       = static_cast<TDoubleParam *>(m_x->clone());
  cloned->m_y       = static_cast<TDoubleParam *>(m_y->clone());
  cloned->m_z       = static_cast<TDoubleParam *>(m_z->clone());
  cloned->m_so      = static_cast<TDoubleParam *>(m_so->clone());
  cloned->m_rot     = static_cast<TDoubleParam *>(m_rot->clone());
  cloned->m_scalex  = static_cast<TDoubleParam *>(m_scalex->clone());
  cloned->m_scaley  = static_cast<TDoubleParam *>(m_scaley->clone());
  cloned->m_scale   = static_cast<TDoubleParam *>(m_scale->clone());
  cloned->m_posPath = static_cast<TDoubleParam *>(m_posPath->clone());
  cloned->m_shearx  = static_cast<TDoubleParam *>(m_shearx->clone());
  cloned->m_sheary  = static_cast<TDoubleParam *>(m_sheary->clone());

  if (m_skeletonDeformation)
    cloned->m_skeletonDeformation =
        new PlasticSkeletonDeformation(*m_skeletonDeformation);

  cloned->m_noScaleZ   = m_noScaleZ;
  cloned->m_center     = m_center;
  cloned->m_offset     = m_offset;
  cloned->m_name       = m_name;
  cloned->m_dagNodePos = m_dagNodePos;

  return cloned;
}

//-----------------------------------------------------------------------------

bool TStageObject::isCycleEnabled() const { return m_cycleEnabled; }

//-----------------------------------------------------------------------------

void TStageObject::enableCycle(bool on) { m_cycleEnabled = on; }

//-----------------------------------------------------------------------------

TStageObject *TStageObject::findRoot(double frame) const {
  if (!m_parent) return NULL;

  TStageObject *parent = m_parent;
  while (parent->m_parent && parent->lazyData().m_time != frame)
    parent = parent->m_parent;

  return parent;
}

//-----------------------------------------------------------------------------

TStageObject *TStageObject::getPinnedDescendant(int frame) {
  if (getPinnedRangeSet()->isPinned(frame)) return this;
  for (list<TStageObject *>::iterator it = m_children.begin();
       it != m_children.end(); ++it) {
    TStageObject *child = *it;
    if (child->getPinnedRangeSet()->isPinned(frame)) return child;
    TStageObject *pinned = child->getPinnedDescendant(frame);
    if (pinned) return pinned;
  }
  return 0;
}

//-----------------------------------------------------------------------------

TAffine TStageObject::computeIkRootOffset(int t) {
  if (m_ikflag > 0) return TAffine();

  // get normal movement (which will be left-multiplied to the IK-part)
  setStatus(XY);
  invalidate();
  TAffine basePlacement = getPlacement(t);
  setStatus(IK);
  invalidate();

  TStageObject *foot = getPinnedDescendant(t);
  if (foot == 0) {
    foot = this;
    setStatus(XY);
  }

  m_ikflag++;
  invalidate();

  TAffine placement                   = foot->getPlacement(t).inv();
  int t0                              = 0;
  const TPinnedRangeSet::Range *range = foot->getPinnedRangeSet()->getRange(t);
  if (range) t0 = range->first;
  while (t0 > 0) {
    TStageObject *oldFoot = getPinnedDescendant(t0 - 1);
    if (oldFoot == 0) break;  // oldFoot = this;
    assert(oldFoot != foot);
    TAffine changeFootAff =
        oldFoot->getPlacement(t0).inv() * foot->getPlacement(t0);
    placement = changeFootAff * placement;
    foot      = oldFoot;
    range     = oldFoot->getPinnedRangeSet()->getRange(t0 - 1);
    t0        = 0;
    if (range) t0 = range->first;
  }
  m_ikflag--;
  invalidate();

  placement = foot->getPinnedRangeSet()->getPlacement() * placement;

  return basePlacement * placement;
}

//-----------------------------------------------------------------------------
/*
inline double getPosPathAtCP(const TStroke* path, int cpIndex)
{
  int n = path->getControlPointCount();
  return path->getLengthAtControlPoint(tcrop(cpIndex*4,0,n-1));
}

//-----------------------------------------------------------------------------
*/

TAffine TStageObject::computeLocalPlacement(double frame) {
  frame = paramsTime(frame);

  if (lazyData().m_time != frame) {
    double sc  = m_scale->getValue(frame);
    double sx  = sc * m_scalex->getValue(frame);
    double sy  = sc * m_scaley->getValue(frame);
    double ang = m_rot->getValue(frame);
    double shx = m_shearx->getValue(frame);
    double shy = m_sheary->getValue(frame);

    TPointD position;
    double posPath = 0;
    switch (m_status & STATUS_MASK) {
    case XY:
      position.x = m_x->getValue(frame) * Stage::inch;
      position.y = m_y->getValue(frame) * Stage::inch;
      break;
    case PATH:
      assert(m_spline);
      assert(m_spline->getStroke());
      posPath = m_spline->getStroke()->getLength() *
                m_posPath->getValue(frame) * 0.01;
      position = m_spline->getStroke()->getPointAtLength(posPath);
      break;
    case PATH_AIM:
      assert(m_spline);
      assert(m_spline->getStroke());
      posPath = m_spline->getStroke()->getLength() *
                m_posPath->getValue(frame) * 0.01;

      position = m_spline->getStroke()->getPointAtLength(posPath);
      if (m_spline->getStroke()->getLength() > 1e-5)
        ang +=
            rad2degree(atan(m_spline->getStroke()->getSpeedAtLength(posPath)));
      break;
    case IK:
      return computeIkRootOffset(frame);
      break;
    }

    TAffine shear(1, shx, 0, shy, 1, 0);

    TPointD handlePos = getHandlePos(m_handle, (int)frame);
    TPointD center    = (m_center + handlePos) * Stage::inch;

    TPointD pos = m_offset;
    if (m_parent) pos += m_parent->getHandlePos(m_parentHandle, (int)frame);
    pos = pos * Stage::inch + position;

    m_localPlacement = TTranslation(pos) * makeRotation(ang) * shear *
                       TScale(sx, sy) * TTranslation(-center);
  }

  return m_localPlacement;
}

//-----------------------------------------------------------------------------

TAffine TStageObject::getPlacement(double t) {
  double &time = lazyData().m_time;

  if (time == t) return m_absPlacement;
  if (time != -1) {
    if (!m_parent)
      invalidate();
    else
      findRoot(t)->invalidate();
  }

  double tt = paramsTime(t);

  TAffine place;
  if (m_parent)
    place = m_parent->getPlacement(t) * computeLocalPlacement(tt);
  else
    place = computeLocalPlacement(tt);
  m_absPlacement = place;
  time           = t;
  return place;
}

//-----------------------------------------------------------------------------

double TStageObject::getZ(double t) {
  double tt = paramsTime(t);
  if (m_parent)
    return m_parent->getZ(t) + m_z->getValue(tt);
  else
    return m_z->getValue(tt);
}

//-----------------------------------------------------------------------------

double TStageObject::getSO(double t) {
  double tt = paramsTime(t);
  if (m_parent)
    return m_parent->getSO(t) + m_so->getValue(tt);
  else
    return m_so->getValue(tt);
}

//-----------------------------------------------------------------------------

double TStageObject::getGlobalNoScaleZ() const {
  return m_parent ? m_parent->getGlobalNoScaleZ() + m_noScaleZ : m_noScaleZ;
}

//-----------------------------------------------------------------------------

double TStageObject::getNoScaleZ() const { return m_noScaleZ; }

//-----------------------------------------------------------------------------

void TStageObject::setNoScaleZ(double noScaleZ) {
  if (m_noScaleZ == noScaleZ) return;
  m_noScaleZ = noScaleZ;
  invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::invalidate(LazyData &ld) const {
  // Since this is an invalidation function, access to the invalidable data
  // should
  // not trigger a data update
  ld.m_time = -1;

  std::list<TStageObject *>::const_iterator cit = m_children.begin();
  for (; cit != m_children.end(); ++cit) (*cit)->invalidate();
}

//-----------------------------------------------------------------------------

void TStageObject::invalidate() { invalidate(m_lazyData(tcg::direct_access)); }

//-----------------------------------------------------------------------------

TAffine TStageObject::getParentPlacement(double t) const {
  return m_parent ? m_parent->getPlacement(t) : TAffine();
}

//-----------------------------------------------------------------------------

// Rebuild the keyframes map
void TStageObject::updateKeyframes(LazyData &ld) const {
  KeyframeMap &keyframes = ld.m_keyframes;

  // Clear the map
  keyframes.clear();

  // Gather all sensible parameters in a vector
  std::vector<TDoubleParam *> params;

  if (m_status == XY) {
    params.push_back(m_x.getPointer());
    params.push_back(m_y.getPointer());
  } else {
    params.push_back(m_posPath.getPointer());
  }

  params.push_back(m_z.getPointer());
  params.push_back(m_so.getPointer());
  params.push_back(m_rot.getPointer());
  params.push_back(m_scalex.getPointer());
  params.push_back(m_scaley.getPointer());
  params.push_back(m_scale.getPointer());
  params.push_back(m_shearx.getPointer());
  params.push_back(m_sheary.getPointer());

  if (m_skeletonDeformation) {
    params.push_back(m_skeletonDeformation->skeletonIdsParam().getPointer());

    // Add Plastic Skeleton params too
    SkD::vd_iterator vdt, vdEnd;
    m_skeletonDeformation->vertexDeformations(vdt, vdEnd);

    for (; vdt != vdEnd; ++vdt) {
      const std::pair<const QString *, SkVD *> &vd = *vdt;

      for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
        params.push_back(vd.second->m_params[p].getPointer());
    }
  }

  // Scan each parameter for a key frame - add each in a set
  std::set<int> frames;

  int p, pCount = params.size();
  for (p = 0; p < pCount; ++p) {
    TDoubleParam *param = params[p];

    int k, kCount = param->getKeyframeCount();
    for (k = 0; k < kCount; ++k) {
      const TDoubleKeyframe &kf = param->getKeyframe(k);
      frames.insert((int)kf.m_frame);
    }
  }

  // Traverse said set and build a TStageObject keyframe for each found instant

  std::set<int>::iterator ft, fEnd(frames.end());
  for (ft = frames.begin(); ft != fEnd; ++ft) {
    int f = *ft;

    // Add values
    TStageObject::Keyframe stageKf;
    stageKf.m_channels[TStageObject::T_Angle]  = m_rot->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_X]      = m_x->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_Y]      = m_y->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_Z]      = m_z->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_SO]     = m_so->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_ScaleX] = m_scalex->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_ScaleY] = m_scaley->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_Scale]  = m_scale->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_Path]   = m_posPath->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_ShearX] = m_shearx->getKeyframeAt(f);
    stageKf.m_channels[TStageObject::T_ShearY] = m_sheary->getKeyframeAt(f);

    if (m_skeletonDeformation)
      m_skeletonDeformation->getKeyframeAt(f, stageKf.m_skeletonKeyframe);

    stageKf.m_isKeyframe = true;

    // Build the stage keyframe's global ease factors
    stageKf.m_easeIn  = -1;
    stageKf.m_easeOut = -1;

    TDoubleKeyframe::Type type = TDoubleKeyframe::None;
    bool easeOk                = true;

    // Start from analyzing standard channels
    for (int c = 0; easeOk && c < T_ChannelCount; ++c) {
      const TDoubleKeyframe &kf = stageKf.m_channels[c];
      if (!kf.m_isKeyframe) continue;

      easeOk = touchEaseAndCompare(kf, stageKf, type);
    }

    // Finally, check the skeleton deformation keyframes
    const std::map<QString, SkVD::Keyframe> &vdfs =
        stageKf.m_skeletonKeyframe.m_vertexKeyframes;

    std::map<QString, SkVD::Keyframe>::const_iterator vdft, vdfEnd(vdfs.end());
    for (vdft = vdfs.begin(); easeOk && vdft != vdfEnd; ++vdft) {
      for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
        const TDoubleKeyframe &kf = vdft->second.m_keyframes[p];
        if (!kf.m_isKeyframe) continue;

        easeOk = touchEaseAndCompare(kf, stageKf, type);
      }
    }

    keyframes[f] = stageKf;
  }
}

//-----------------------------------------------------------------------------

void TStageObject::updateKeyframes() {
  return updateKeyframes(m_lazyData(tcg::direct_access));
}

//-----------------------------------------------------------------------------

void TStageObject::saveData(TOStream &os) {
  TStageObjectId parentId = getParent();
  std::map<std::string, string> attr;
  attr["id"]           = parentId.toString();
  attr["handle"]       = m_handle;
  attr["parentHandle"] = m_parentHandle;
  os.openChild("parent", attr);
  os.closeChild();
  if (m_name != "") os.child("name") << m_name;
  os.child("isOpened") << (int)m_isOpened;
  if (isGrouped()) {
    os.openChild("groupIds");
    int i;
    for (i = 0; i < m_groupId.size(); i++) os << m_groupId[i];
    os.closeChild();
    os.openChild("groupNames");
    for (i = 0; i < m_groupName.size(); i++) os << m_groupName[i];
    os.closeChild();
  }

  m_pinnedRangeSet->saveData(os);

  os.child("center") << m_center.x << m_center.y << -m_offset.x << -m_offset.y;
  os.child("status") << (int)m_status;
  if (m_noScaleZ != 0) os.child("noScaleZ") << m_noScaleZ;

  if (m_spline) os.child("splinep") << m_spline;

  if (!m_x->isDefault()) os.child("x") << *m_x;
  if (!m_y->isDefault()) os.child("y") << *m_y;
  if (!m_z->isDefault()) os.child("z") << *m_z;
  if (!m_so->isDefault()) os.child("so") << *m_so;
  if (!m_scalex->isDefault()) os.child("sx") << *m_scalex;
  if (!m_scaley->isDefault()) os.child("sy") << *m_scaley;
  if (!m_scale->isDefault()) os.child("sc") << *m_scale;
  if (!m_rot->isDefault()) os.child("rot") << *m_rot;
  if (!m_posPath->isDefault()) os.child("pos") << *m_posPath;
  if (!m_shearx->isDefault()) os.child("shx") << *m_shearx;
  if (!m_sheary->isDefault()) os.child("shy") << *m_sheary;
  if (m_cycleEnabled) os.child("cycle") << 1;

  if (m_skeletonDeformation) os.child("plasticSD") << *m_skeletonDeformation;

  os.child("nodePos") << m_dagNodePos.x << m_dagNodePos.y;

  if (getId().isCamera()) {
    os.openChild("camera");
    m_camera->saveData(os);
    os.closeChild();

    /*TDimensionD size = m_camera->getSize();
TDimension res = m_camera->getRes();
bool isXPrevalence = m_camera->isXPrevalence();
os.child("cameraSize") << size.lx << size.ly;
os.child("cameraRes") << res.lx << res.ly;
os.child("xPrevalence") << (int)isXPrevalence;*/
  }
}

//-----------------------------------------------------------------------------

void TStageObject::loadData(TIStream &is) {
  VersionNumber tnzVersion = is.getVersion();
  string tagName;
  QList<int> groupIds;
  QList<wstring> groupNames;

  KeyframeMap &keyframes = lazyData().m_keyframes;

  while (is.matchTag(tagName)) {
    if (tagName == "parent") {
      string parentIdStr  = is.getTagAttribute("id");
      string handle       = is.getTagAttribute("handle");
      string parentHandle = is.getTagAttribute("parentHandle");
      if (tnzVersion < VersionNumber(1, 15)) {
        handle       = convertTo4InchCenterUnits(handle);
        parentHandle = convertTo4InchCenterUnits(parentHandle);
      }
      if (parentIdStr == "")  // vecchio formato
        is >> parentIdStr;
      else {
      }

      TStageObjectId parentId = toStageObjectId(parentIdStr);
      if (m_id != TStageObjectId::NoneId &&
          parentId != TStageObjectId::NoneId) {
        assert(parentId != m_id);
        setParent(parentId);
      }
      if (handle != "") setHandle(handle);
      if (parentHandle != "") setParentHandle(parentHandle);
    } else if (tagName == "center") {
      is >> m_center.x >> m_center.y >> m_offset.x >> m_offset.y;
      m_offset = -m_offset;
    } else if (tagName == "name")
      is >> m_name;
    else if (tagName == "x")
      is >> *m_x;
    else if (tagName == "y")
      is >> *m_y;
    else if (tagName == "z")
      is >> *m_z;
    else if (tagName == "so")
      is >> *m_so;
    else if (tagName == "rot")
      is >> *m_rot;
    else if (tagName == "sx")
      is >> *m_scalex;
    else if (tagName == "sy")
      is >> *m_scaley;
    else if (tagName == "sc")
      is >> *m_scale;
    else if (tagName == "shx")
      is >> *m_shearx;
    else if (tagName == "shy")
      is >> *m_sheary;
    else if (tagName == "pos")
      is >> *m_posPath;
    else if (tagName == "posCP") {
      TDoubleParam posCP;
      is >> posCP;
    }  // Ghibli 6.2 release. cfr
    else if (tagName == "noScaleZ")
      is >> m_noScaleZ;
    else if (tagName == "isOpened") {
      int v = 0;
      is >> v;
      m_isOpened = (bool)v;
    } else if (tagName == "pinnedStatus") {
      m_pinnedRangeSet->loadData(is);
    } else if (tagName == "status") {
      int v = 0;
      is >> v;
      m_status = (Status)v;
    } else if (tagName == "spline") {
      TStageObjectSpline *spline = m_tree->createSpline();
      is >> *spline;
      m_spline = spline;
      m_spline->addRef();
    } else if (tagName == "splinep") {
      TPersist *p = 0;
      is >> p;
      m_spline = dynamic_cast<TStageObjectSpline *>(p);
      m_tree->insertSpline(m_spline);
      m_spline->addRef();
    } else if (tagName == "cycle") {
      int dummy;
      is >> dummy;
      m_cycleEnabled = true;
    } else if (tagName == "nodePos") {
      TPointD pos;
      is >> pos.x >> pos.y;
      m_dagNodePos = updateDagPosition(pos, tnzVersion);
      // is >> m_dagNodePos.x >> m_dagNodePos.y;
    }
    //***** camera parameters till toonz 6.0 ********
    else if (tagName == "cameraSize") {
      TDimensionD size(0, 0);
      is >> size.lx >> size.ly;
      if (m_camera) m_camera->setSize(size);
    } else if (tagName == "cameraRes") {
      TDimension res(0, 0);
      is >> res.lx >> res.ly;
      if (m_camera) m_camera->setRes(res);
    }
    //***** camera parameters toonz 6.1 ******** november 2009
    else if (tagName == "camera")
      m_camera->loadData(is);
    else if (tagName == "groupIds") {
      int groupId;
      while (!is.eos()) {
        is >> groupId;
        groupIds.push_back(groupId);
      }
    } else if (tagName == "groupNames") {
      wstring groupName;
      while (!is.eos()) {
        is >> groupName;
        groupNames.push_back(groupName);
      }
    } else if (tagName == "plasticSD") {
      PlasticSkeletonDeformation *sd = new PlasticSkeletonDeformation;
      is >> *sd;
      setPlasticSkeletonDeformation(sd);
    } else
      throw TException("TStageObject::loadData. unexpected tag: " + tagName);
    is.matchEndTag();
  }
  if (!groupIds.isEmpty()) {
    assert(groupIds.size() == groupNames.size());
    int i;
    for (i = 0; i < groupIds.size(); i++) {
      setGroupId(groupIds[i]);
      setGroupName(groupNames[i]);
    }
  }
  if (tnzVersion < VersionNumber(1, 13)) {
    updateUnit(m_y.getPointer());
    updateUnit(m_x.getPointer());
  }
  if (tnzVersion < VersionNumber(1, 14)) {
    double factor = 1.0 / Stage::inch;
    m_center      = m_center * factor;
    m_offset      = m_offset * factor;
  }
  if (tnzVersion < VersionNumber(1, 16) && !keyframes.empty()) {
    std::map<int, TStageObject::Keyframe>::iterator itKf = keyframes.begin();
    std::set<int> keyframeIndexSet;
    for (; itKf != keyframes.end(); itKf++)
      if (is52FullKeyframe(itKf->first)) keyframeIndexSet.insert(itKf->first);
    std::set<int>::iterator itKfInd = keyframeIndexSet.begin();
    for (; itKfInd != keyframeIndexSet.end(); itKfInd++)
      setkey(m_scale, *itKfInd);
  }
  // Calling updateKeyframes() here may cause failure to load expressions
  // especially if it refers to a curve which is to be loaded AFTER this
  // function while loading scene process. So I will skip it assuming that
  // updateKeyframes() will be called when it is actually needed.
  // updateKeyframes();
  if (m_spline != 0 && isUppkEnabled())
    m_spline->addParam(m_posPath.getPointer());
  invalidate();
}

//-----------------------------------------------------------------------------

TStageObjectParams *TStageObject::getParams() const {
  TStageObjectParams *data = new TStageObjectParams();
  data->m_name             = m_name;
  data->m_center           = m_center;
  data->m_noScaleZ         = m_noScaleZ;

  data->m_id       = m_id;
  data->m_parentId = getParent();
  data->m_offset   = m_offset;
  data->m_status   = m_status;

  data->m_x       = m_x;
  data->m_y       = m_y;
  data->m_z       = m_z;
  data->m_so      = m_so;
  data->m_rot     = m_rot;
  data->m_scalex  = m_scalex;
  data->m_scaley  = m_scaley;
  data->m_scale   = m_scale;
  data->m_posPath = m_posPath;
  data->m_shearx  = m_shearx;
  data->m_sheary  = m_sheary;

  data->m_skeletonDeformation = m_skeletonDeformation;

  data->m_cycleEnabled = m_cycleEnabled;
  data->m_spline       = m_spline;

  data->m_handle       = m_handle;
  data->m_parentHandle = m_parentHandle;
  if (m_pinnedRangeSet) data->m_pinnedRangeSet = m_pinnedRangeSet->clone();

  return data;
}

//-----------------------------------------------------------------------------

void TStageObject::assignParams(const TStageObjectParams *src,
                                bool doParametersClone) {
  m_name     = src->m_name;
  m_center   = src->m_center;
  m_noScaleZ = src->m_noScaleZ;
  m_offset   = src->m_offset;
  m_status   = src->m_status;
  if (m_spline) m_spline->release();
  m_spline = src->m_spline;
  if (m_spline) m_spline->addRef();

  if (doParametersClone) {
    m_x->copy(src->m_x.getPointer());
    m_y->copy(src->m_y.getPointer());
    m_z->copy(src->m_z.getPointer());
    m_so->copy(src->m_so.getPointer());
    m_rot->copy(src->m_rot.getPointer());
    m_scalex->copy(src->m_scalex.getPointer());
    m_scaley->copy(src->m_scaley.getPointer());
    m_scale->copy(src->m_scale.getPointer());
    m_posPath->copy(src->m_posPath.getPointer());
    m_shearx->copy(src->m_shearx.getPointer());
    m_sheary->copy(src->m_sheary.getPointer());

    if (src->m_skeletonDeformation)
      setPlasticSkeletonDeformation(
          new PlasticSkeletonDeformation(*src->m_skeletonDeformation));
  } else {
    m_x = src->m_x.getPointer();
    m_x->addObserver(this);
    m_y = src->m_y.getPointer();
    m_y->addObserver(this);
    m_z = src->m_z.getPointer();
    m_z->addObserver(this);
    m_so = src->m_so.getPointer();
    m_so->addObserver(this);
    m_rot = src->m_rot.getPointer();
    m_rot->addObserver(this);
    m_scalex = src->m_scalex.getPointer();
    m_scalex->addObserver(this);
    m_scaley = src->m_scaley.getPointer();
    m_scaley->addObserver(this);
    m_scale = src->m_scale.getPointer();
    m_scale->addObserver(this);
    m_posPath = src->m_posPath.getPointer();
    m_posPath->addObserver(this);
    m_shearx = src->m_shearx.getPointer();
    m_shearx->addObserver(this);
    m_sheary = src->m_sheary.getPointer();
    m_sheary->addObserver(this);

    m_skeletonDeformation = src->m_skeletonDeformation;
    if (m_skeletonDeformation) m_skeletonDeformation->addObserver(this);
  }

  m_handle       = src->m_handle;
  m_parentHandle = src->m_parentHandle;

  m_cycleEnabled = src->m_cycleEnabled;
  if (m_pinnedRangeSet) *m_pinnedRangeSet = *src->m_pinnedRangeSet;
  updateKeyframes();
  if (m_spline && isUppkEnabled()) m_spline->addParam(m_posPath.getPointer());
  invalidate();
}

//-----------------------------------------------------------------------------
//
// objectZ e' la posizione dell'oggetto lungo l'asse Z
//   il default e' 0. valori grandi rappresentano oggetti piu' vicini alla
//   camera
//   la distanza iniziale fra tavolo e camera e' 1000
//
// cameraZ e' la posizione della camera lungo l'asse Z
//   il default e' 0. valori negativi indicano che la camera si avvicina al
//   tavolo
//   (che raggiungerebbe per un valore di -1000)
//

bool TStageObject::perspective(TAffine &aff, const TAffine &cameraAff,
                               double cameraZ, const TAffine &objectAff,
                               double objectZ, double objectNoScaleZ) {
  TPointD cameraPos(cameraAff.a13, cameraAff.a23);
  TPointD objectPos(objectAff.a13, objectAff.a23);

  const double focus     = 1000;
  const double nearPlane = 1;

  /*--
   * dzはカメラとオブジェクトの間の距離、focus(=1000)はPegbarをZ軸方向に動かさなかった場合のデフォルト距離
   * --*/
  double dz = focus + cameraZ - objectZ;
  if (dz < nearPlane) {
    aff = TAffine();
    return false;
  }

  double noScaleSc = 1 - objectNoScaleZ / focus;
  aff = cameraAff * TScale((focus + cameraZ) / dz) * cameraAff.inv() *
        objectAff * TScale(noScaleSc);

  return true;
}

//-----------------------------------------------------------------------------

int TStageObject::setGroupId(int value) {
  m_groupSelector++;
  m_groupId.insert(m_groupSelector, value);
  return m_groupSelector;
}

//-----------------------------------------------------------------------------

void TStageObject::setGroupId(int value, int position) {
  assert(position >= 0 && position <= m_groupId.size());
  m_groupId.insert(position, value);
  if (m_groupSelector + 1 >= position) m_groupSelector++;
}

//-----------------------------------------------------------------------------

int TStageObject::getGroupId() {
  return m_groupId.isEmpty() || m_groupSelector < 0 ||
                 m_groupSelector >= m_groupId.size()
             ? 0
             : m_groupId[m_groupSelector];
}

//-----------------------------------------------------------------------------

QStack<int> TStageObject::getGroupIdStack() { return m_groupId; }

//-----------------------------------------------------------------------------

void TStageObject::removeGroupId(int position) {
  if (!isGrouped()) return;
  assert(position >= 0 && position <= m_groupId.size());
  m_groupId.remove(position);
  if (m_groupSelector + 1 >= position && m_groupSelector > -1)
    m_groupSelector--;
}

//-----------------------------------------------------------------------------

int TStageObject::removeGroupId() {
  m_groupId.remove(m_groupSelector);
  if (m_groupSelector > -1) m_groupSelector--;
  return m_groupSelector + 1;
}

//-----------------------------------------------------------------------------

bool TStageObject::isGrouped() { return !m_groupId.isEmpty(); }

//-----------------------------------------------------------------------------

bool TStageObject::isContainedInGroup(int groupId) {
  return m_groupId.contains(groupId);
}

//-----------------------------------------------------------------------------

void TStageObject::setGroupName(const wstring &name, int position) {
  int groupSelector = position < 0 ? m_groupSelector : position;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.insert(groupSelector, name);
}

//-----------------------------------------------------------------------------

wstring TStageObject::getGroupName(bool fromEditor) {
  int groupSelector = fromEditor ? m_groupSelector + 1 : m_groupSelector;
  return m_groupName.isEmpty() || groupSelector < 0 ||
                 groupSelector >= m_groupName.size()
             ? L""
             : m_groupName[groupSelector];
}

//-----------------------------------------------------------------------------

QStack<wstring> TStageObject::getGroupNameStack() { return m_groupName; }

//-----------------------------------------------------------------------------

int TStageObject::removeGroupName(bool fromEditor) {
  int groupSelector = fromEditor ? m_groupSelector + 1 : m_groupSelector;
  if (!isGrouped()) return -1;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.remove(groupSelector);
  return groupSelector;
}

//-----------------------------------------------------------------------------

void TStageObject::removeGroupName(int position) {
  int groupSelector = position < 0 ? m_groupSelector : position;
  assert(groupSelector >= 0 && groupSelector <= m_groupName.size());
  m_groupName.remove(groupSelector);
}

//-----------------------------------------------------------------------------

void TStageObject::removeFromAllGroup() {
  m_groupId.clear();
  m_groupName.clear();
  m_groupSelector = -1;
}

//-----------------------------------------------------------------------------

void TStageObject::editGroup() { m_groupSelector--; }

//-----------------------------------------------------------------------------

bool TStageObject::isGroupEditing() {
  return isGrouped() && m_groupSelector == -1;
}

//-----------------------------------------------------------------------------

void TStageObject::closeEditingGroup(int groupId) {
  if (!m_groupId.contains(groupId)) return;
  m_groupSelector = 0;
  while (m_groupId[m_groupSelector] != groupId &&
         m_groupSelector < m_groupId.size())
    m_groupSelector++;
}

//-----------------------------------------------------------------------------

int TStageObject::getEditingGroupId() {
  if (!isGrouped() || m_groupSelector + 1 >= m_groupId.size()) return -1;
  return m_groupId[m_groupSelector + 1];
}

//-----------------------------------------------------------------------------

wstring TStageObject::getEditingGroupName() {
  if (!isGrouped() || m_groupSelector + 1 >= m_groupName.size()) return L"";
  return m_groupName[m_groupSelector + 1];
}
