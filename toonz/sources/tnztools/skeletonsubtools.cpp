

#include "skeletonsubtools.h"
#include "skeletontool.h"
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "toonz/tscenehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"

#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/tpinnedrangeset.h"
#include "toonz/stage.h"
#include "toonz/hook.h"
#include "toonz/txshcell.h"
#include "toonz/skeleton.h"

#include "toonz/stageobjectutil.h"
#include "tgl.h"
#include "tconvert.h"

#include <tsystem.h>
#include <math.h>

using namespace SkeletonSubtools;

//============================================================
//
// DragCenterTool
//
//------------------------------------------------------------

DragCenterTool::DragCenterTool(SkeletonTool *tool)
    : DragTool(tool)
    , m_objId(TTool::getApplication()->getCurrentObject()->getObjectId())
    , m_frame(TTool::getApplication()->getCurrentFrame()->getFrame()) {}

//------------------------------------------------------------

void DragCenterTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  m_center = m_oldCenter = xsh->getCenter(m_objId, m_frame);
  m_firstPos             = pos;
  m_affine               = xsh->getPlacement(m_objId, m_frame).inv() *
             xsh->getParentPlacement(m_objId, m_frame);
  m_affine.a13 = m_affine.a23 = 0;
}

//------------------------------------------------------------

void DragCenterTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  double factor = 1.0 / Stage::inch;
  m_center      = m_oldCenter + (m_affine * (pos - m_firstPos)) * factor;
  TTool::getApplication()->getCurrentXsheet()->getXsheet()->setCenter(
      m_objId, m_frame, m_center);
}

//------------------------------------------------------------

void DragCenterTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  UndoStageObjectCenterMove *undo =
      new UndoStageObjectCenterMove(m_objId, m_frame, m_oldCenter, m_center);
  TTool::Application *app = TTool::getApplication();
  undo->setObjectHandle(app->getCurrentObject());
  undo->setXsheetHandle(app->getCurrentXsheet());
  TUndoManager::manager()->add(undo);
}

//============================================================
//
// DragChannelTool
//
//------------------------------------------------------------

DragChannelTool::DragChannelTool(SkeletonTool *tool, TStageObject::Channel a0)
    : DragTool(tool) {
  TTool::Application *app = TTool::getApplication();
  m_before.setFrameHandle(app->getCurrentFrame());
  m_before.setObjectHandle(app->getCurrentObject());
  m_before.setXsheetHandle(app->getCurrentXsheet());
  m_before.add(a0);
  if (tool->isGlobalKeyframesEnabled()) {
    m_before.add(TStageObject::T_Angle);
    m_before.add(TStageObject::T_X);
    m_before.add(TStageObject::T_Y);
    m_before.add(TStageObject::T_Z);
    m_before.add(TStageObject::T_SO);
    m_before.add(TStageObject::T_ScaleX);
    m_before.add(TStageObject::T_ScaleY);
    m_before.add(TStageObject::T_Scale);
    m_before.add(TStageObject::T_Path);
    m_before.add(TStageObject::T_ShearX);
    m_before.add(TStageObject::T_ShearY);
  }
  m_after   = m_before;
  m_dragged = false;
}

//------------------------------------------------------------

DragChannelTool::DragChannelTool(SkeletonTool *tool, TStageObject::Channel a0,
                                 TStageObject::Channel a1)
    : DragTool(tool) {
  TTool::Application *app = TTool::getApplication();
  m_before.setFrameHandle(app->getCurrentFrame());
  m_before.setObjectHandle(app->getCurrentObject());
  m_before.setXsheetHandle(app->getCurrentXsheet());
  m_before.add(a0);
  m_before.add(a1);
  if (tool->isGlobalKeyframesEnabled()) {
    m_before.add(TStageObject::T_Angle);
    m_before.add(TStageObject::T_X);
    m_before.add(TStageObject::T_Y);
    m_before.add(TStageObject::T_Z);
    m_before.add(TStageObject::T_SO);
    m_before.add(TStageObject::T_ScaleX);
    m_before.add(TStageObject::T_ScaleY);
    m_before.add(TStageObject::T_Scale);
    m_before.add(TStageObject::T_Path);
    m_before.add(TStageObject::T_ShearX);
    m_before.add(TStageObject::T_ShearY);
  }
  m_after   = m_before;
  m_dragged = false;
}

//------------------------------------------------------------

void DragChannelTool::start() {
  m_before.updateValues();
  m_after = m_before;
}

//------------------------------------------------------------

TPointD DragChannelTool::getCenter() {
  TTool::Application *app = TTool::getApplication();
  TStageObjectId objId    = app->getCurrentObject()->getObjectId();
  int frame               = app->getCurrentFrame()->getFrame();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  return xsh->getParentPlacement(objId, frame).inv() *
         xsh->getPlacement(objId, frame) *
         (Stage::inch * xsh->getCenter(objId, frame));
}

//------------------------------------------------------------

void DragChannelTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (m_dragged) {
    if (getTool()->isGlobalKeyframesEnabled()) m_after.setGlobalKeyframe();

    TTool::Application *app   = TTool::getApplication();
    UndoStageObjectMove *undo = new UndoStageObjectMove(m_before, m_after);
    undo->setObjectHandle(app->getCurrentObject());
    TUndoManager::manager()->add(undo);
    app->getCurrentScene()->setDirtyFlag(true);
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentObject()->notifyObjectIdChanged(
        false);  // Keyframes navigator reads this
  }
  m_dragged = false;
}

//============================================================
//
// DragPositionTool
//
//------------------------------------------------------------

DragPositionTool::DragPositionTool(SkeletonTool *tool)
    : DragChannelTool(tool, TStageObject::T_X, TStageObject::T_Y) {}

//------------------------------------------------------------

void DragPositionTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  start();
  m_firstPos  = pos;
  m_firstDrag = true;
}

//------------------------------------------------------------

void DragPositionTool::leftButtonDrag(const TPointD &pos,
                                      const TMouseEvent &e) {
  TPointD delta = pos - m_firstPos;
  if (m_firstDrag && (delta.x > 2.0 || delta.y > 2.0)) {
    m_firstPos  = pos;
    delta       = TPointD(0.0, 0.0);
    m_firstDrag = false;
  }
  if (e.isShiftPressed()) {
    if (fabs(delta.x) > fabs(delta.y))
      delta.y = 0;
    else
      delta.x = 0;
  }
  double factor = 1.0 / Stage::inch;
  setValues(getOldValue(0) + delta.x * factor,
            getOldValue(1) + delta.y * factor);
  m_dragged = true;
}

//============================================================
//
// DragRotationTool
//
//------------------------------------------------------------

DragRotationTool::DragRotationTool(SkeletonTool *tool, bool snapped)
    : DragChannelTool(tool, TStageObject::T_Angle), m_snapped(snapped) {}

//------------------------------------------------------------

void DragRotationTool::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  m_lastPos = pos;
  m_center  = getCenter();
  start();
}

//------------------------------------------------------------

void DragRotationTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &) {
  TPointD a = m_center - m_lastPos;
  TPointD b = m_center - pos;

  double a0 = norm2(pos - m_lastPos);
  if (a0 < 2 && !m_dragged) return;

  double a2 = norm2(a), b2 = norm2(b);
  const double eps = 1e-1;
  if (a2 < eps || b2 < eps) return;

  double dang = asin(cross(a, b) / sqrt(a2 * b2)) * M_180_PI;

  if (m_snapped) {
    if (fabs(dang) < 2) return;
    m_snapped = false;
  }
  setValue(getValue(0) + dang);
  m_dragged = true;

  m_lastPos = pos;
}

//============================================================
//
// ParentChangeTool
//
//------------------------------------------------------------

ParentChangeTool::ParentChangeTool(SkeletonTool *tool, TTool::Viewer *viewer)
    : DragTool(tool)
    , m_viewer(viewer)
    , m_index(-1)
    , m_snapped(true)
    , m_pixelSize(tool->getPixelSize()) {}

//------------------------------------------------------------

void ParentChangeTool::leftButtonDown(const TPointD &pos,
                                      const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  // colonna, id e frame corrente
  int currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
  int currentFrame       = app->getCurrentFrame()->getFrame();
  TStageObjectId id      = TStageObjectId::ColumnId(currentColumnIndex);
  TStageObject *obj      = xsh->getStageObject(id);
  obj->getCenterAndOffset(m_oldCenter, m_oldOffset);

  // trovo il centro della colonna corrente
  TAffine aff    = xsh->getPlacement(id, currentFrame);
  TPointD center = Stage::inch * xsh->getCenter(id, currentFrame);
  m_center       = aff * center;
  // mi serve in coordinate assolute (per questo non uso direttamente pos)
  m_lastPos  = m_viewer->winToWorld(e.m_pos);
  m_lastPos2 = pos;
  // if(m_mode==1) getTool()->setParentProbe(pos);

  // cerco centri e hooks di tutte le altre colonne (non discendenti)
  int nextName = 10000;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshCell cell = xsh->getCell(currentFrame, col);
    if (cell.isEmpty()) continue;
    TStageObjectId colId = TStageObjectId::ColumnId(col);

    // se colId e' un discendente di id non e' un candidato a diventare
    // il padre di id.
    TStageObjectId tmpId = colId;
    while (tmpId.isColumn() && tmpId != id)
      tmpId = xsh->getStageObjectParent(tmpId);
    if (tmpId == id) continue;

    // registro alcune informazioni relative a colId:
    // placement, bbox, centro, hooks, name, ecc.
    Element element;
    element.m_columnIndex = col;
    TImageP img           = cell.getImage(false);
    if (img) element.m_bbox = img->getBBox();
    element.m_aff = xsh->getPlacement(colId, currentFrame);
    Peer peer;
    peer.m_columnIndex = col;
    peer.m_handle      = 0;
    peer.m_name        = nextName++;
    peer.m_pos =
        element.m_aff * (Stage::inch * xsh->getCenter(colId, currentFrame));
    element.m_peers.push_back(peer);
    TXshLevel *xl = cell.m_level.getPointer();
    if (xl) {
      HookSet *hookSet     = xl->getHookSet();
      TStageObject *pegbar = xsh->getStageObject(colId);
      if (hookSet && pegbar) {
        for (int i = 0; i < hookSet->getHookCount(); i++) {
          if (!hookSet->getHook(i)) continue;
          peer.m_handle = 1 + i;
          peer.m_pos =
              element.m_aff * hookSet->getHook(i)->getAPos(cell.m_frameId);
          peer.m_name = nextName++;
          element.m_peers.push_back(peer);
        }
      }
    }
    m_elements[col] = element;
  }

  // m_newParentCenter = TPointD();
  m_firstWinPos = e.m_pos;

  m_affine = xsh->getParentPlacement(id, currentFrame).inv();
  m_objId  = id;
}

//------------------------------------------------------------

void ParentChangeTool::leftButtonDrag(const TPointD &pos,
                                      const TMouseEvent &e) {
  if (m_snapped && norm2(e.m_pos - m_firstWinPos) > 200) {
    TTool::Application *app = TTool::getApplication();
    int currentColumnIndex  = app->getCurrentColumn()->getColumnIndex();
    TStageObjectId objId    = TStageObjectId::ColumnId(currentColumnIndex);
    TStageObjectCmd::setParent(objId, TStageObjectId::NoneId, "",
                               app->getCurrentXsheet());
    m_snapped = false;
  }

  if (m_snapped) return;
  getTool()->setParentProbe(getTool()->getCurrentColumnParentMatrix() * pos);

  m_index = m_viewer->posToColumnIndex(e.m_pos, 5.0, false);

  m_lastPos = m_viewer->winToWorld(e.m_pos);

  m_handle = -1;
  if (m_index >= 0) {
    double minDist2 = 0;
    for (int i = 0; i < (int)m_elements[m_index].m_peers.size(); i++) {
      TPointD targetPos = m_elements[m_index].m_peers[i].m_pos;
      double dist2      = norm2(targetPos - m_lastPos);
      if (m_handle < 0 || dist2 < minDist2) {
        minDist2 = dist2;
        m_handle = m_elements[m_index].m_peers[i].m_handle;
      }
    }
  }
}

//------------------------------------------------------------

void ParentChangeTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  int currentColumnIndex  = app->getCurrentColumn()->getColumnIndex();
  int currentFrame        = app->getCurrentFrame()->getFrame();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  TStageObjectId objId    = TStageObjectId::ColumnId(currentColumnIndex);
  getTool()->resetParentProbe();

  if (m_snapped) {
    xsh->getStageObject(objId)->setCenterAndOffset(m_oldCenter, m_oldOffset);
    return;
  }

  if (getTool()->getMagicLinkCount() > 0) {
    MagicLink magicLink = getTool()->getMagicLink(0);
    HookData h0         = magicLink.m_h0;
    HookData h1         = magicLink.m_h1;
    TStageObject *obj   = xsh->getStageObject(objId);

    int parentColumnIndex    = h1.m_columnIndex;
    TStageObjectId parentId  = TStageObjectId::ColumnId(parentColumnIndex);
    std::string parentHandle = h1.getHandle();
    std::string handle       = "";
    if (h0.m_columnIndex < 0) {
      handle = obj->getHandle();
    } else {
      handle = h0.getHandle();
    }

    // TUndoManager *undoManager = TUndoManager::manager();
    // undoManager->beginBlock();
    // TStageObjectCmd::resetPosition(id, app->getCurrentXsheet());
    TStageObjectCmd::setHandle(objId, handle, app->getCurrentXsheet());
    TStageObjectCmd::setParent(objId, parentId, parentHandle,
                               app->getCurrentXsheet());
    // undoManager->endBlock();
  } else {
    TStageObjectCmd::setParent(objId, TStageObjectId::NoneId, "",
                               app->getCurrentXsheet());
    xsh->getStageObject(objId)->setCenterAndOffset(m_oldCenter, m_oldOffset);
  }
}

//------------------------------------------------------------

void ParentChangeTool::draw() { getTool()->drawHooks(); }

//============================================================
//
// IKTool helper functions
//
//------------------------------------------------------------

// This needs some clarification.
namespace {
class Graph {
  // local variables defined below are:
  // Nodes m_nodes;
  // std::map<int, int> m_leaves;

public:
  typedef std::set<int> Links;
  typedef Links::const_iterator LinkIter;

  // the second part of the nodes map is a set if ints
  typedef std::map<int, Links> Nodes;
  typedef Nodes::const_iterator NodeIter;
  typedef std::map<int, int> LeaveTable;
  typedef LeaveTable::const_iterator LeaveIter;

  Graph() {}

  int touch(int id) {
    // insert an empty node if not found
    if (m_nodes.count(id) == 0) m_nodes[id] = Links();
    return id;
  }

  // check if a node is found in m_nodes
  bool isNode(int id) const { return m_nodes.count(id) > 0; }

  int getNodeCount() const { return (int)m_nodes.size(); }

  void link(int a, int b) {
    touch(a);
    touch(b);
    m_nodes[a].insert(b);
    m_nodes[b].insert(a);
  }
  bool areLinked(int a, int b) const {
    NodeIter it = m_nodes.find(a);
    return it == m_nodes.end() ? false : it->second.count(b);
  }

  const Links &getLinks(int id) const {
    static const Links empty;
    NodeIter it = m_nodes.find(id);
    return it == m_nodes.end() ? empty : it->second;
  }

  int getLinkCount(int id) const { return (int)getLinks(id).size(); }

  int getFirstLink(int id) const {
    const Links &links = getLinks(id);
    return links.empty() ? -1 : *(links.begin());
  }

  NodeIter begin() const { return m_nodes.begin(); }
  NodeIter end() const { return m_nodes.end(); }

  // Wht is a leave?
  // m_leaves is a std map of int int

  bool isLeave(int id) const { return m_leaves.count(id); }

  enum LeaveType {
    PINNED     = 0x1,
    TEMP       = 0x2,
    CHILD_END  = 0x4,
    PARENT_END = 0x8
  };

  int getLeaveType(int id) const {
    LeaveIter it = m_leaves.find(id);
    return it == m_leaves.end() ? 0 : it->second;
  }

  void setLeaveType(int id, int type) { m_leaves[id] = type; }

  // This gets rid of a node.
  void remove(int id) {
    NodeIter it = m_nodes.find(id);
    if (it != m_nodes.end()) {
      // iterate over the links
      for (LinkIter j = it->second.begin(); j != it->second.end(); ++j)
        m_nodes[*j].erase(id);
      m_nodes.erase(it->first);
    }
  }
  void clear() {
    m_nodes.clear();
    m_leaves.clear();
  }

private:
  Nodes m_nodes;
  std::map<int, int> m_leaves;
};

//------------------------------------------------------------

bool hasPinned(const Skeleton::Bone *bone, const Skeleton::Bone *prevBone) {
  if (!bone) return false;
  bool isHandle = prevBone == 0;
  bool isChild  = prevBone != 0 && prevBone == bone->getParent();

  if (bone->getPinnedStatus() != Skeleton::Bone::FREE) return true;

  if (bone->getParent() && bone->getParent() != prevBone &&
      hasPinned(bone->getParent(), bone)) {
    return true;
  }

  for (int i = 0; i < bone->getChildCount(); i++) {
    if (bone->getChild(i) != prevBone) {
      if (hasPinned(bone->getChild(i), bone)) {
        return true;
      }
    }
  }

  return false;
}

// create the tree from the skeleton. the tree contains the active chain only
// i.e. the nodes bounded by the pinned nodes and the handle

bool addToActiveChain(Graph &tree, const Skeleton::Bone *bone,
                      const Skeleton::Bone *prevBone) {
  if (!bone) return false;

  // The handle is what you grabbed
  bool isHandle = prevBone == 0;

  bool isChild     = prevBone != 0 && prevBone == bone->getParent();
  bool isParent    = prevBone != 0 && prevBone->getParent() == bone;
  int pinnedStatus = bone->getPinnedStatus();
  bool isFree      = pinnedStatus == Skeleton::Bone::FREE;
  // bool isPinned = pinnedStatus == Skeleton::Bone::PINNED;
  bool isTempPinned = pinnedStatus == Skeleton::Bone::TEMP_PINNED;

  bool propagate = false;

  // Go up the chain from what you grabbed and add bones
  if (!isChild && isFree) {
    if (bone->getParent()) {
      propagate |= addToActiveChain(tree, bone->getParent(), bone);
    }
  }

  std::vector<int> children;
  // Once you reach the top parent, add the other children
  if (isHandle || isFree) {
    for (int i = 0; i < bone->getChildCount(); i++) {
      if (bone->getChild(i) != prevBone) {
        propagate |= addToActiveChain(tree, bone->getChild(i), bone);
      }
    }
  }

  bool insert = false;

  if (isHandle)  // the handle must be added anyway
    insert = true;

  else if (isChild)  // add child if it's pinned or if some gran-child has been
                     // added
    insert = !isFree || propagate;

  else if (isTempPinned) {
    // parent temp pinned are normally added, but if another branch is pinned we
    // are not
    // able to handle the configuration: then consider it as pinned
    bool locked = false;
    // if(bone->getParent()) locked |= hasPinned(bone->getParent(), bone);
    for (int i = 0; i < bone->getChildCount() && !locked; i++)
      if (bone->getChild(i) != prevBone)
        locked |= hasPinned(bone->getChild(i), bone);
    insert = !locked;
  } else  // add parent if free and grand-parent or some other child has been
          // added; don't add pinned parent
    insert = isFree && propagate;

  if (insert) {
    int index = tree.touch(bone->getColumnIndex());
    if (!isFree)
      tree.setLeaveType(index, isTempPinned ? 2 : 1);
    else if (!isChild && bone->getParent() &&
             !tree.isNode(bone->getParent()->getColumnIndex())) {
      // the parent has not been added. it was locked => this node act as pinned
      tree.setLeaveType(index, 2);
    }

    if (prevBone) tree.link(prevBone->getColumnIndex(), index);
  }

  return isHandle || !isFree || propagate;
}

//------------------------------------------------------------

}  // namespace

//============================================================
//
// IKToolUndo
//
//------------------------------------------------------------

namespace SkeletonSubtools {

class IKToolUndo final : public TUndo {
  struct Node {
    TStageObjectId m_id;
    double m_oldAngle, m_newAngle;
    bool m_wasKeyframe;
  };
  std::vector<Node> m_nodes;
  TStageObjectId m_firstFootId;
  TAffine m_oldFootPlacement, m_newFootPlacement;
  int m_frame;

public:
  IKToolUndo() {}

  void setFirstFootId(const TStageObjectId &firstFootId) {
    m_firstFootId = firstFootId;
  }
  void setFirstFootOldPlacement(const TAffine &oldPlacement) {
    m_oldFootPlacement = oldPlacement;
  }
  void setFirstFootNewPlacement(const TAffine &newPlacement) {
    m_newFootPlacement = newPlacement;
  }

  void addNode(const TStageObjectId &id) {
    m_nodes.push_back(Node());
    m_nodes.back().m_id = id;
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    int frame    = TTool::getApplication()->getCurrentFrame()->getFrame();
    TDoubleParam *param =
        xsh->getStageObject(id)->getParam(TStageObject::T_Angle);
    m_nodes.back().m_oldAngle    = param->getValue(frame);
    m_nodes.back().m_wasKeyframe = param->isKeyframe(frame);
  }

  void onAdd() override {
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    m_frame      = TTool::getApplication()->getCurrentFrame()->getFrame();
    for (int i = 0; i < (int)m_nodes.size(); i++) {
      TDoubleParam *param =
          xsh->getStageObject(m_nodes[i].m_id)->getParam(TStageObject::T_Angle);
      m_nodes[i].m_newAngle = param->getValue(m_frame);
    }
  }

  void setFootPlacement(const TAffine &placement) const {
    if (!m_firstFootId.isColumn()) return;
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    TStageObject *obj         = xsh->getStageObject(m_firstFootId);
    TPinnedRangeSet *rangeSet = obj->getPinnedRangeSet();
    rangeSet->setPlacement(placement);
    while (obj->getParent().isColumn())
      obj = xsh->getStageObject(obj->getParent());
    obj->invalidate();
  }

  void undo() const override {
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    for (int i = 0; i < (int)m_nodes.size(); i++) {
      TDoubleParam *param =
          xsh->getStageObject(m_nodes[i].m_id)->getParam(TStageObject::T_Angle);
      if (m_nodes[i].m_wasKeyframe)
        param->setValue(m_frame, m_nodes[i].m_oldAngle);
      else
        param->deleteKeyframe(m_frame);
    }
    if (m_firstFootId.isColumn()) setFootPlacement(m_oldFootPlacement);

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
  }

  void redo() const override {
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    for (int i = 0; i < (int)m_nodes.size(); i++) {
      TDoubleParam *param =
          xsh->getStageObject(m_nodes[i].m_id)->getParam(TStageObject::T_Angle);
      param->setValue(m_frame, m_nodes[i].m_newAngle);
    }
    if (m_firstFootId.isColumn()) setFootPlacement(m_newFootPlacement);

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
  }

  int getSize() const override {
    return sizeof(*this) + (int)m_nodes.size() * sizeof(Node);
  }
};
}  // namespace SkeletonSubtools

//============================================================
//
// IKTool
//
//------------------------------------------------------------

IKTool::IKTool(SkeletonTool *tool, TTool::Viewer *viewer, Skeleton *skeleton,
               int columnIndex)
    : DragTool(tool)
    , m_viewer(viewer)
    , m_skeleton(skeleton)
    , m_pos()
    , m_columnIndex(columnIndex)
    , m_valid(false)
    , m_frameOnNewPin(false)
    , m_foot(0)
    , m_firstFoot(0)
    , m_undo(0) {}

//------------------------------------------------------------

IKTool::~IKTool() { delete m_skeleton; }

//------------------------------------------------------------

bool IKTool::isParentOf(int columnIndex, int childColumnIndex) const {
  Skeleton::Bone *bone = m_skeleton->getBoneByColumnIndex(columnIndex);
  Skeleton::Bone *childBone =
      m_skeleton->getBoneByColumnIndex(childColumnIndex);
  return bone && childBone && childBone->getParent() == bone;
}

//------------------------------------------------------------
// This is a beast.

void IKTool::initEngine(const TPointD &pos) {
  m_valid = false;
  m_engine.clear();
  m_joints.clear();

  // build the active chain (bounded by m_columnIndex and pinned nodes)
  Graph chain;
  addToActiveChain(chain, m_skeleton->getBoneByColumnIndex(m_columnIndex), 0);

  // mark leaves as child_end or parent_end: a leave is child/parent_end iff
  // all its link are parent/child
  for (Graph::NodeIter it = chain.begin(); it != chain.end(); ++it)
    if (chain.getLeaveType(it->first) & (Graph::PINNED | Graph::TEMP)) {
      int type = 0;
      for (Graph::LinkIter j = it->second.begin(); j != it->second.end(); ++j) {
        int f =
            isParentOf(*j, it->first) ? Graph::CHILD_END : Graph::PARENT_END;
        if (type == 0)
          type = f;
        else if (type != f) {
          type = 0;
          break;
        }
      }
      chain.setLeaveType(it->first, chain.getLeaveType(it->first) | type);
    }

  // search the foot (i.e. the pinned node). if there are no pinned node find
  // the first temp-pinned
  int foot = -1;
  for (Graph::NodeIter it = chain.begin(); it != chain.end(); ++it)
    if (chain.isLeave(it->first)) {
      if ((chain.getLeaveType(it->first) & Graph::PINNED) != 0 || foot < 0)
        foot = it->first;
    }
  if (foot < 0 || !chain.isNode(foot)) return;

  // If the chain descend from the foot then the whole foot should be still
  // (e.g. no rotation)
  // => chop the foot
  if (chain.getLinkCount(foot) == 1 &&
      (chain.getLeaveType(foot) & Graph::PINNED)) {
    int nextNode = chain.getFirstLink(foot);
    if (isParentOf(foot, nextNode)) {
      chain.remove(foot);
      foot = nextNode;
      chain.setLeaveType(foot, 3);
    }
  }

  // if the handle is a terminal node (i.e. just one link) and the next node is
  // a child move the handle to the next node
  int handle = m_columnIndex;
  if (chain.getLinkCount(handle) == 1) {
    int nextNode = chain.getFirstLink(handle);
    if (isParentOf(handle, nextNode)) {
      chain.remove(handle);
      handle = nextNode;
    }
  }

  // "reverse" the tree to suit the IKEngine convention
  std::vector<std::pair<int, Skeleton::Bone *>> stack;
  std::vector<int> done;
  stack.push_back(std::make_pair(foot, (Skeleton::Bone *)0));
  while (!stack.empty()) {
    Skeleton::Bone *bone = m_skeleton->getBoneByColumnIndex(stack.back().first);
    Skeleton::Bone *prev = stack.back().second;
    Skeleton::Bone *prev2 = prev;
    stack.pop_back();
    for (;;) {
      int id = bone->getColumnIndex();
      done.push_back(id);
      double sign = 1;
      if (prev) {
        if (prev->getParent() == bone) sign = -1;
      } else if (done.size() == 1) {
        if (chain.getLeaveType(id) & Graph::CHILD_END) sign = -1;
      } else {
        if (chain.getLeaveType(id) & Graph::PARENT_END) sign = -1;
      }

      Graph::Links links = chain.getLinks(bone->getColumnIndex());
      for (int i = 0; i < (int)done.size(); i++) links.erase(done[i]);
      if (links.size() == 0) {
        // terminal point
        m_joints.push_back(Joint(bone, prev, sign));
        break;
      } else {
        Graph::LinkIter it   = links.begin();
        Skeleton::Bone *next = m_skeleton->getBoneByColumnIndex(*it++);
        for (; it != links.end(); ++it)
          stack.push_back(std::make_pair(*it, prev));
        if (links.size() > 1 ||
            (prev && next &&
             (prev->getParent() == bone && next->getParent() == bone))) {
          bone = next;
        } else {
          m_joints.push_back(Joint(bone, prev, sign));
          prev = bone;
          bone = next;
        }
      }
    }
  }
  // if(m_joints.size()>=2 && m_joints[1].m_prevBone == m_joints[0].m_bone)
  //  m_joints[0].m_sign = m_joints[1].m_sign;

  // feed the engine
  m_engine.setRoot(m_joints[0].m_bone->getCenter());
  for (int i = 1; i < (int)m_joints.size(); i++) {
    int parent = -1;
    for (int j = 0; j < i; j++)
      if (m_joints[j].m_bone == m_joints[i].m_prevBone) {
        parent = j;
        break;
      }
    if (parent < 0) {
      assert(0);
      break;
    }
    m_engine.addJoint(m_joints[i].m_bone->getCenter(), parent);
    if (m_joints[i].m_bone->getPinnedStatus() != Skeleton::Bone::FREE)
      m_engine.lock(i);
  }

  // add the handle
  bool handleFound = false;
  for (int i = 0; i < (int)m_joints.size(); i++)
    if (m_joints[i].m_bone->getColumnIndex() == handle) {
      m_engine.addJoint(pos, i);
      handleFound = true;
      break;
    }
  if (!handleFound) return;

  setAngleOffsets();
  computeIHateIK();

  m_valid = true;
}

//------------------------------------------------------------

// This sets foot placement data
// and checks if the current frame is the start
// of a new pinned center
// The name of this function should be changed once it
// actually works.  Until then, the name is fitting.
void IKTool::computeIHateIK() {
  std::vector<TStageObject *> objs;
  for (int i = 0; i < m_skeleton->getBoneCount(); i++)
    objs.push_back(m_skeleton->getBone(i)->getStageObject());
  int n     = (int)objs.size();
  int frame = TTool::getApplication()->getCurrentFrame()->getFrame();
  m_foot = m_firstFoot = 0;
  m_frameOnNewPin      = false;

  // this just finds the first pin
  int i;
  for (i = 0; i < n && !objs[i]->getPinnedRangeSet()->isPinned(frame); i++) {
  }
  if (i == n) return;

  // this makes m_foot to be the current pin
  m_foot = objs[i];

  // check to see if this frame is the start of a new pin
  const TPinnedRangeSet::Range *range =
      m_foot->getPinnedRangeSet()->getRange(frame);
  if (!range || range->first != frame) return;

  m_frameOnNewPin = true;
  int firstFrame  = frame - 1;
  m_firstFoot     = m_foot;

  // the frame is the start of a new pinned frame, find the previous pin
  for (;;) {
    for (i = 0; i < n && !objs[i]->getPinnedRangeSet()->isPinned(firstFrame);
         i++) {
    }

    if (i == n) break;
    m_firstFoot = objs[i];
    range       = m_firstFoot->getPinnedRangeSet()->getRange(firstFrame);
    if (!range) break;
    firstFrame = range->first - 1;
    if (firstFrame < 0) break;
  }

  m_footPlacement      = m_foot->getPlacement(frame);
  m_firstFootPlacement = m_firstFoot->getPinnedRangeSet()->getPlacement();
}

//------------------------------------------------------------

void IKTool::setAngleOffsets() {
  int frame = TTool::getApplication()->getCurrentFrame()->getFrame();
  for (int i = 0; i < (int)m_joints.size(); i++) {
    double angle = m_joints[i].m_bone->getStageObject()->getParam(
        TStageObject::T_Angle, frame);
    double theta0             = angle * M_PI_180;
    double theta1             = m_joints[i].m_sign * m_engine.getJointAngle(i);
    m_joints[i].m_angleOffset = theta1 - theta0;
  }
}

//------------------------------------------------------------

void IKTool::storeOldValues() {
  for (int i = 0; i < (int)m_joints.size(); i++) {
    TStageObjectValues values(m_joints[i].m_bone->getStageObject()->getId(),
                              TStageObject::T_Angle);
    if (getTool()->isGlobalKeyframesEnabled()) {
      values.add(TStageObject::T_X);
      values.add(TStageObject::T_Y);
      values.add(TStageObject::T_Z);
      values.add(TStageObject::T_SO);
      values.add(TStageObject::T_ScaleX);
      values.add(TStageObject::T_ScaleY);
      values.add(TStageObject::T_Scale);
      values.add(TStageObject::T_Path);
      values.add(TStageObject::T_ShearX);
      values.add(TStageObject::T_ShearY);
    }
    TTool::Application *app = TTool::getApplication();
    values.setFrameHandle(app->getCurrentFrame());
    values.setXsheetHandle(app->getCurrentXsheet());
    values.updateValues();
    m_joints[i].m_oldValues = values;
  }
}
//------------------------------------------------------------
// This is called on movement of the mouse with button down
void IKTool::apply() {
  if (!m_valid) return;
  TStageObject *rootObj = m_skeleton->getRootBone()->getStageObject();
  if (!m_undo) {
    m_undo = new IKToolUndo();
    for (int i = 0; i < (int)m_joints.size(); i++)
      m_undo->addNode(m_joints[i].m_bone->getStageObject()->getId());

    if (m_frameOnNewPin && m_firstFoot) {
      m_undo->setFirstFootId(m_firstFoot->getId());
      m_undo->setFirstFootOldPlacement(
          m_firstFoot->getPinnedRangeSet()->getPlacement());
    }
  }

  int frame = TTool::getApplication()->getCurrentFrame()->getFrame();
  for (int i = 0; i < (int)m_joints.size(); i++) {
    TStageObject *obj   = m_joints[i].m_bone->getStageObject();
    TDoubleParam *param = obj->getParam(TStageObject::T_Angle);
    double theta        = m_joints[i].m_sign * m_engine.getJointAngle(i) -
                   m_joints[i].m_angleOffset;
    theta *= M_180_PI;
    double oldTheta = param->getValue(frame);
    double delta    = theta - oldTheta;
    if (fabs(delta) > 180) theta += (theta < oldTheta ? 1 : -1) * 360;
    param->setValue(frame, theta);
  }
  m_skeleton->getRootBone()->getStageObject()->invalidate();
  if (m_frameOnNewPin) {
    TStageObject *rootObj = m_skeleton->getRootBone()->getStageObject();
    rootObj->setStatus(TStageObject::XY);
    rootObj->invalidate();
    TAffine rootBasePlacement = rootObj->getPlacement(frame);
    rootObj->setStatus(TStageObject::IK);
    rootObj->invalidate();

    TPinnedRangeSet *rangeSet = m_firstFoot->getPinnedRangeSet();

    TAffine oldFootPlacement = m_foot->getPlacement(frame);
    TAffine relativeOldFootPlacement =
        rootBasePlacement.inv() * oldFootPlacement;
    TAffine correction = rootBasePlacement.inv() * m_footPlacement *
                         oldFootPlacement.inv() * rootBasePlacement;

    rangeSet->setPlacement(correction * rangeSet->getPlacement());
    rootObj->invalidate();

    TAffine currentFootPlacement = m_foot->getPlacement(frame);
    TAffine check                = m_footPlacement.inv() * currentFootPlacement;

    assert(check.isIdentity(0.01));
  }
}

//------------------------------------------------------------

void IKTool::leftButtonDown(const TPointD &p, const TMouseEvent &e) {
  TPointD pos = m_viewer->winToWorld(e.m_pos);
  m_pos       = pos;
  initEngine(pos);
  storeOldValues();
}

//------------------------------------------------------------

void IKTool::leftButtonDrag(const TPointD &p, const TMouseEvent &e) {
  if (!m_valid) return;
  if (m_engine.getJointCount() > 0) {
    TPointD tmp = m_viewer->winToWorld(e.m_pos);
    m_engine.drag(tmp);
    apply();
  }
}
//------------------------------------------------------------

void IKTool::leftButtonUp(const TPointD &p, const TMouseEvent &e) {
  if (m_undo) {
    if (m_frameOnNewPin && m_firstFoot)
      m_undo->setFirstFootNewPlacement(
          m_firstFoot->getPinnedRangeSet()->getPlacement());
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }

  m_valid = false;
  m_engine.clear();
}

//------------------------------------------------------------

void IKTool::draw() {
  int err = glGetError();
  assert(err == GL_NO_ERROR);
  TTool::Application *app = TTool::getApplication();
  int frame               = app->getCurrentFrame()->getFrame();

  double unit =
      TTool::getApplication()->getCurrentTool()->getTool()->getPixelSize();

  if (m_engine.getJointCount() > 0) {
    glColor3d(1, 0, 1);
    for (int i = 0; i < m_engine.getJointCount(); i++) {
      TPointD pa = m_engine.getJoint(i);
      tglDrawDisk(pa, 5 * unit);
      if (i > 0) {
        int j = m_engine.getJointParent(i);
        tglDrawSegment(pa, m_engine.getJoint(j));
      }
    }
  }
}

//============================================================
//
// ChangeDrawingTool
//
//------------------------------------------------------------

class ChangeDrawingUndo final : public TUndo {
  int m_row, m_col;
  TFrameId m_oldFid, m_newFid;

public:
  ChangeDrawingUndo(int row, int col) : m_row(row), m_col(col) {
    m_oldFid = getDrawing();
  }

  TFrameId getDrawing() const {
    TTool::Application *app = TTool::getApplication();
    TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
    TXshCell cell           = xsh->getCell(m_row, m_col);
    return cell.m_frameId;
  }

  void setDrawing(const TFrameId &fid) const {
    TTool::Application *app = TTool::getApplication();
    TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
    TXshCell cell           = xsh->getCell(m_row, m_col);
    cell.m_frameId          = fid;
    xsh->setCell(m_row, m_col, cell);
    TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(m_col));
    pegbar->setOffset(pegbar->getOffset());
    // solo per invalidare l'imp. TODO da fare meglio
    // l'idea e' che cambiando drawing corrente, se ci sono hook la posizione
    // della pegbar (o delle pegbar figlie) puo' cambiare
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void onAdd() override { m_newFid = getDrawing(); }
  void undo() const override { setDrawing(m_oldFid); }
  void redo() const override { setDrawing(m_newFid); }
  int getSize() const override { return sizeof(*this); }
  TFrameId getOldDrawing() const { return m_oldFid; }
};

//------------------------------------------------------------

ChangeDrawingTool::ChangeDrawingTool(SkeletonTool *tool, int dir)
    : DragTool(tool), m_oldY(0), m_dir(dir), m_undo(0) {}

//------------------------------------------------------------

void ChangeDrawingTool::leftButtonDown(const TPointD &, const TMouseEvent &e) {
  m_oldY                  = e.m_pos.y;
  TTool::Application *app = TTool::getApplication();
  int row                 = app->getCurrentFrame()->getFrame();
  int col                 = app->getCurrentColumn()->getColumnIndex();
  m_undo                  = new ChangeDrawingUndo(row, col);
  if (m_dir > 0)
    changeDrawing(1);
  else if (m_dir < 0)
    changeDrawing(-1);
}

//------------------------------------------------------------

void ChangeDrawingTool::leftButtonDrag(const TPointD &, const TMouseEvent &e) {
  if (m_dir != 0) return;
  int delta     = e.m_pos.y - m_oldY;
  int h         = 5;
  int increment = delta > 0 ? delta / h : -(-delta) / h;
  if (increment == 0) return;
  changeDrawing(-increment);
  m_oldY += h * increment;
}

//------------------------------------------------------------

void ChangeDrawingTool::leftButtonUp(const TPointD &, const TMouseEvent &e) {
  ChangeDrawingUndo *u = dynamic_cast<ChangeDrawingUndo *>(m_undo);
  if (u) {
    if (u->getOldDrawing() != u->getDrawing())
      TUndoManager::manager()->add(u);
    else
      delete u;
    m_undo = 0;
  }
}

//------------------------------------------------------------

// cambia il frame della cella corrente, scorrendo (in maniera
// ciclica) i drawings del livello
// ritorna true se ci riesce
bool ChangeDrawingTool::changeDrawing(int delta) {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  int row                 = app->getCurrentFrame()->getFrame();
  int col                 = app->getCurrentColumn()->getColumnIndex();
  TXshCell cell           = xsh->getCell(row, col);
  if (!cell.m_level || !cell.m_level->getSimpleLevel()) return false;
  std::vector<TFrameId> fids;
  cell.m_level->getSimpleLevel()->getFids(fids);
  int n = fids.size();
  if (n < 2) return false;
  std::vector<TFrameId>::iterator it;
  it = std::find(fids.begin(), fids.end(), cell.m_frameId);
  if (it == fids.end()) return false;
  int index = std::distance(fids.begin(), it);
  while (delta < 0) delta += n;
  index = (index + delta) % n;

  ChangeDrawingUndo *u = dynamic_cast<ChangeDrawingUndo *>(m_undo);
  if (u) u->setDrawing(fids[index]);
  return true;
}

//============================================================

CommandHandler::CommandHandler() : m_skeleton(0), m_tempPinnedSet(0) {}

//------------------------------------------------------------

CommandHandler::~CommandHandler() { delete m_skeleton; }

//------------------------------------------------------------

void CommandHandler::setSkeleton(Skeleton *skeleton) {
  if (m_skeleton != skeleton) {
    delete m_skeleton;
    m_skeleton = skeleton;
  }
}

//------------------------------------------------------------

void CommandHandler::clearPinnedRanges() {
  if (m_skeleton) {
    TTool::Application *app = TTool::getApplication();
    m_skeleton->clearAllPinnedRanges();
    app->getCurrentScene()->setDirtyFlag(true);
    app->getCurrentXsheet()->notifyXsheetChanged();
    m_skeleton->getRootBone()->getStageObject()->setStatus(TStageObject::XY);
    delete m_skeleton;
    m_skeleton = 0;
  }
  if (m_tempPinnedSet) m_tempPinnedSet->clear();
}
