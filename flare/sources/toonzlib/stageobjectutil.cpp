

// TnzBase includes
#include "tdoubleparam.h"

// TnzLib includes
#include "toonz/tstageobject.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/tstageobjecttree.h"

#include "toonz/stageobjectutil.h"

//=============================================================================
// TStageObjectValues
//-----------------------------------------------------------------------------

TStageObjectValues::Channel::Channel(TStageObject::Channel actionId)
    : m_actionId(actionId)
    , m_value(0)  //, m_isKeyframe(false)
{}

TStageObjectValues::TStageObjectValues()
    : m_objectId(TStageObjectId::NoneId), m_frame(-1) {}

//-----------------------------------------------------------------------------

TStageObjectValues::TStageObjectValues(TStageObjectId id,
                                       TStageObject::Channel a0)
    : m_objectId(id), m_frame(-1) {
  add(a0);
}

//-----------------------------------------------------------------------------

TStageObjectValues::TStageObjectValues(TStageObjectId id,
                                       TStageObject::Channel a0,
                                       TStageObject::Channel a1)
    : m_objectId(id), m_frame(-1) {
  add(a0);
  add(a1);
}

//-----------------------------------------------------------------------------

void TStageObjectValues::add(TStageObject::Channel actionId) {
  bool isFound = false;
  std::vector<Channel>::iterator it;
  for (it = m_channels.begin(); it != m_channels.end(); ++it) {
    TStageObjectValues::Channel ch = *it;
    if (ch.m_actionId == actionId) {
      isFound = true;
      break;
    }
  }
  if (!isFound) m_channels.push_back(Channel(actionId));
}

//-----------------------------------------------------------------------------

void TStageObjectValues::updateValues() {
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  if (m_objectId == TStageObjectId::NoneId)
    m_objectId = m_objectHandle->getObjectId();
  m_frame      = m_frameHandle->getFrame();
  std::vector<Channel>::iterator it;

  for (it = m_channels.begin(); it != m_channels.end(); ++it) {
    TDoubleParam *param =
        xsh->getStageObject(m_objectId)->getParam(it->m_actionId);
    it->setValue(param->getValue(m_frame));
  }
}

//-----------------------------------------------------------------------------
namespace {
TStageObject *getAncestor(TStageObjectTree *tree, TStageObject *obj) {
  TStageObjectId parentId = obj->getParent();
  TStageObject *root      = obj;
  while (parentId != TStageObjectId::NoneId && parentId.isColumn()) {
    root     = tree->getStageObject(parentId);
    parentId = root->getParent();
  }
  return root;
}
}
//-----------------------
void TStageObjectValues::applyValues(bool undoEnabled) const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  std::vector<Channel>::const_iterator it;
  for (it = m_channels.begin(); it != m_channels.end(); ++it) {
    TStageObject *pegbar = xsh->getStageObject(m_objectId);

    TDoubleParam *param =
        pegbar->getParam((TStageObject::Channel)it->m_actionId);
    if (!param->isKeyframe(m_frame)) {
      KeyframeSetter setter(param, -1,
                            undoEnabled);  // Deve essere registrato l'undo
      setter.createKeyframe(m_frame);
    }
    int indexKeyframe = param->getClosestKeyframe(m_frame);
    KeyframeSetter setter(param, indexKeyframe,
                          false);  // Non deve essere registrato l'undo
    setter.setValue(it->getValue());
  }
  //--- Permette l'undo per l'interpolazione con la cinematica inversa
  TStageObjectTree *tree = xsh->getStageObjectTree();
  if (tree) {
    TStageObject *root = getAncestor(tree, tree->getStageObject(m_objectId));
    if (root) root->invalidate();
  }
  //---
}

//-----------------------------------------------------------------------------

void TStageObjectValues::Channel::setValue(double value) {
  if (m_actionId == TStageObject::T_ScaleX ||
      m_actionId == TStageObject::T_ScaleY) {
    const double eps             = 1.e-8;
    if (fabs(value) < eps) value = value < 0 ? -eps : eps;
  }
  m_value = value;
}

//-----------------------------------------------------------------------------

void TStageObjectValues::setValue(double v) { m_channels[0].setValue(v); }

//-----------------------------------------------------------------------------

void TStageObjectValues::setValues(double v0, double v1) {
  m_channels[0].setValue(v0);
  m_channels[1].setValue(v1);
}

//-----------------------------------------------------------------------------

double TStageObjectValues::getValue(int index) const {
  assert(0 <= index && index < (int)m_channels.size());
  return m_channels[index].getValue();
}

//-----------------------------------------------------------------------------

void TStageObjectValues::setGlobalKeyframe() {
  TXsheet *xsh              = m_xsheetHandle->getXsheet();
  TStageObject *stageObject = xsh->getStageObject(m_objectId);
  stageObject->setKeyframeWithoutUndo(m_frame);
}

//-----------------------------------------------------------------------------
/*-- HistoryViewerに、動かしたObject/Channel名・フレーム番号の文字列を返す --*/
QString TStageObjectValues::getStringForHistory() {
  /*-- どのチャンネルを動かしたか --*/
  QString channelStr;
  if (m_channels.size() > 1) /*--複数チャンネルを動かした場合--*/
    channelStr = QObject::tr("Move");
  else {
    switch (m_channels.at(0).m_actionId) {
    case TStageObject::T_Angle:
      channelStr = QObject::tr("Edit Rotation");
      break;
    case TStageObject::T_X:
      channelStr = QObject::tr("Move X");
      break;
    case TStageObject::T_Y:
      channelStr = QObject::tr("Move Y");
      break;
    case TStageObject::T_Z:
      channelStr = QObject::tr("Move Z");
      break;
    case TStageObject::T_SO:
      channelStr = QObject::tr("Edit Stack Order");
      break;
    case TStageObject::T_ScaleX:
      channelStr = QObject::tr("Edit Scale W");
      break;
    case TStageObject::T_ScaleY:
      channelStr = QObject::tr("Edit Scale H");
      break;
    case TStageObject::T_Scale:
      channelStr = QObject::tr("Edit Scale");
      break;
    case TStageObject::T_Path:
      channelStr = QObject::tr("Edit PosPath");
      break;
    case TStageObject::T_ShearX:
      channelStr = QObject::tr("Edit Shear X");
      break;
    case TStageObject::T_ShearY:
      channelStr = QObject::tr("Edit Shear Y");
      break;
    default:
      channelStr = QObject::tr("Move");
      break;
    }
  }

  return QObject::tr("%1  %2  Frame : %3")
      .arg(channelStr)
      .arg(QString::fromStdString(m_objectId.toString()))
      .arg(m_frame + 1);
}

//-----------------------------------------------------------------------------

//=============================================================================
// UndoSetKeyFrame
//-----------------------------------------------------------------------------

UndoSetKeyFrame::UndoSetKeyFrame(TStageObjectId objectId, int frame,
                                 TXsheetHandle *xsheetHandle)
    : m_objId(objectId), m_frame(frame), m_xsheetHandle(xsheetHandle) {
  TXsheet *xsh      = m_xsheetHandle->getXsheet();
  TStageObject *obj = xsh->getStageObject(m_objId);

  m_key = obj->getKeyframe(frame);
}

//-----------------------------------------------------------------------------

void UndoSetKeyFrame::undo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();

  assert(xsh->getStageObject(m_objId));

  if (TStageObject *obj = xsh->getStageObject(m_objId)) {
    obj->removeKeyframeWithoutUndo(m_frame);
    if (m_key.m_isKeyframe) obj->setKeyframeWithoutUndo(m_frame, m_key);
  }

  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void UndoSetKeyFrame::redo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();

  assert(xsh->getStageObject(m_objId));

  if (TStageObject *obj = xsh->getStageObject(m_objId))
    obj->setKeyframeWithoutUndo(m_frame);

  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

int UndoSetKeyFrame::getSize() const {
  return 10 << 10;
}  // Gave up exact calculation. Say ~10 kB?

//=============================================================================
// UndoRemoveKeyFrame
//-----------------------------------------------------------------------------

UndoRemoveKeyFrame::UndoRemoveKeyFrame(TStageObjectId objectId, int frame,
                                       TStageObject::Keyframe key,
                                       TXsheetHandle *xsheetHandle)
    : m_objId(objectId)
    , m_frame(frame)
    , m_xsheetHandle(xsheetHandle)
    , m_key(key) {}

//-----------------------------------------------------------------------------

void UndoRemoveKeyFrame::undo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();

  assert(xsh->getStageObject(m_objId));

  if (TStageObject *obj = xsh->getStageObject(m_objId)) {
    obj->setKeyframeWithoutUndo(m_frame);
    obj->setKeyframeWithoutUndo(m_frame, m_key);
  }

  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void UndoRemoveKeyFrame::redo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();

  assert(xsh->getStageObject(m_objId));

  if (TStageObject *obj = xsh->getStageObject(m_objId))
    obj->removeKeyframeWithoutUndo(m_frame);

  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

int UndoRemoveKeyFrame::getSize() const {
  return 10 << 10;
}  // Gave up exact calculation. Say ~10 kB?

//=============================================================================
// UndoStageObjectCenterMove
//-----------------------------------------------------------------------------

UndoStageObjectCenterMove::UndoStageObjectCenterMove(const TStageObjectId &id,
                                                     int frame,
                                                     const TPointD &before,
                                                     const TPointD &after)
    : m_pid(id), m_frame(frame), m_before(before), m_after(after) {}

//-----------------------------------------------------------------------------

void UndoStageObjectCenterMove::undo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  xsh->setCenter(m_pid, m_frame, m_before);
  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void UndoStageObjectCenterMove::redo() const {
  TXsheet *xsh = m_xsheetHandle->getXsheet();
  xsh->setCenter(m_pid, m_frame, m_after);
  m_objectHandle->notifyObjectIdChanged(false);
}

//=============================================================================
// UndoStageObjectMove
//-----------------------------------------------------------------------------

UndoStageObjectMove::UndoStageObjectMove(const TStageObjectValues &before,
                                         const TStageObjectValues &after)
    : m_before(before), m_after(after) {}

//-----------------------------------------------------------------------------

void UndoStageObjectMove::undo() const {
  m_before.applyValues(false);
  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void UndoStageObjectMove::redo() const {
  m_after.applyValues(false);
  m_objectHandle->notifyObjectIdChanged(false);
}

//=============================================================================
// UndoStageObjectPinned
//-----------------------------------------------------------------------------

UndoStageObjectPinned::UndoStageObjectPinned(const TStageObjectId &id,
                                             int frame, const bool &before,
                                             const bool &after)
    : m_pid(id), m_frame(frame), m_before(before), m_after(after) {}

//-----------------------------------------------------------------------------

void UndoStageObjectPinned::undo() const {
  TXsheet *xsh      = m_xsheetHandle->getXsheet();
  TStageObject *obj = xsh->getStageObject(m_pid);
  assert(0);
  // obj->setIsPinned(m_frame,m_before);
  m_objectHandle->notifyObjectIdChanged(false);
}

//-----------------------------------------------------------------------------

void UndoStageObjectPinned::redo() const {
  TXsheet *xsh      = m_xsheetHandle->getXsheet();
  TStageObject *obj = xsh->getStageObject(m_pid);
  assert(0);
  // obj->setIsPinned(m_frame,m_after);
  m_objectHandle->notifyObjectIdChanged(false);
}

//=============================================================================
// insertFrame
//-----------------------------------------------------------------------------
//
// sposto avanti di un frame tutti i keyframes >= frame
//
void insertFrame(TDoubleParam *param, int frame) {
  std::map<int, TDoubleKeyframe> keyframes;

  for (int i = param->getKeyframeCount() - 1; i >= 0; --i) {
    TDoubleKeyframe k = param->getKeyframe(i);
    if (k.m_frame < (double)frame) break;

    k.m_frame += 1.0;
    keyframes.insert(keyframes.begin(), std::make_pair(i, k));
  }

  if (!keyframes.empty()) param->setKeyframes(keyframes);
}

void insertFrame(TStageObject *obj, int frame) {
  // Move standard TStageObject channels
  for (int channel = 0; channel != TStageObject::T_ChannelCount; ++channel)
    insertFrame(obj->getParam((TStageObject::Channel)channel), frame);

  if (const SkDP &sd = obj->getPlasticSkeletonDeformation()) {
    // Move Plastic channels
    insertFrame(sd->skeletonIdsParam().getPointer(), frame);

    PlasticSkeletonDeformation::vd_iterator vt, vEnd;
    sd->vertexDeformations(vt, vEnd);

    for (; vt != vEnd; ++vt) {
      SkVD *vd = (*vt).second;

      for (int p = 0; p != SkVD::PARAMS_COUNT; ++p)
        insertFrame(vd->m_params[p].getPointer(), frame);
    }
  }
}

//=============================================================================
// removeFrame
//-----------------------------------------------------------------------------
//
// cancello l'eventuale keyframe = frame;
// sposto tutti i successivi indietro di un frame
//
void removeFrame(TDoubleParam *param, int frame) {
  param->deleteKeyframe((double)frame);

  std::map<int, TDoubleKeyframe> keyframes;
  int i = 0;
  while (i < param->getKeyframeCount()) {
    TDoubleKeyframe k = param->getKeyframe(i);
    if (k.m_frame < frame) {
      i++;
      continue;
    }
    k.m_frame -= 1;
    keyframes[i] = k;
    i++;
  }
  if (!keyframes.empty()) param->setKeyframes(keyframes);
}

void removeFrame(TStageObject *obj, int frame) {
  for (int channel = 0; channel != TStageObject::T_ChannelCount; ++channel)
    removeFrame(obj->getParam((TStageObject::Channel)channel), frame);

  // Plastic keys can be removed through xsheet commands - do so

  if (const SkDP &sd = obj->getPlasticSkeletonDeformation()) {
    removeFrame(sd->skeletonIdsParam().getPointer(), frame);

    SkD::vd_iterator vdt, vdEnd;
    sd->vertexDeformations(vdt, vdEnd);

    for (; vdt != vdEnd; ++vdt) {
      SkVD *vd = (*vdt).second;

      for (int p = 0; p != SkVD::PARAMS_COUNT; ++p)
        removeFrame(vd->m_params[p].getPointer(), frame);
    }
  }
}
