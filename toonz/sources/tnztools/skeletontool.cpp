

// TnzCore includes
#include "tstroke.h"
#include "tgl.h"

// TnzBase includes
#include "tenv.h"

// TnzLib includes
#include "toonz/tstageobjectcmd.h"
#include "toonz/toonzimageutils.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/toonzscene.h"
#include "toonz/stage.h"
#include "toonz/txshcell.h"
#include "toonz/dpiscale.h"
#include "toonz/skeleton.h"
#include "toonz/tscenehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tpinnedrangeset.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/stageobjectutil.h"

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/selectioncommandids.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolutils.h"

// Qt includes
#include <QCoreApplication>  // Qt translation support
#include <QPainter>
#include <QGLWidget>  // for QGLWidget::convertToGLFormat
#include <QPainterPath>
#include <QString>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>

#include "skeletonsubtools.h"
#include "skeletontool.h"

using namespace SkeletonSubtools;

TEnv::IntVar SkeletonGlobalKeyFrame("SkeletonToolGlobalKeyFrame", 0);
TEnv::IntVar SkeletonInverseKinematics("SkeletonToolInverseKinematics", 0);

#define BUILD_SKELETON L"Build Skeleton"
#define ANIMATE L"Animate"
#define INVERSE_KINEMATICS L"Inverse Kinematics"

const double alpha = 0.4;

using SkeletonSubtools::HookData;

//============================================================

inline std::string removeTrailingH(std::string handle) {
  if (handle.find("H") == 0)
    return handle.substr(1);
  else
    return handle;
}

//============================================================
//
// util
//
//------------------------------------------------------------

// return true iff column ancestorIndex is column descentIndex or its parent or
// the parent of the parent, etc.
static bool isAncestorOf(int ancestorIndex, int descendentIndex) {
  TStageObjectId ancestorId   = TStageObjectId::ColumnId(ancestorIndex);
  TStageObjectId descendentId = TStageObjectId::ColumnId(descendentIndex);
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  while (descendentId != ancestorId && descendentId.isColumn())
    descendentId = xsh->getStageObjectParent(descendentId);
  return descendentId == ancestorId;
}

//------------------------------------------------------------

static void getHooks(std::vector<HookData> &hooks, TXsheet *xsh, int row,
                     int col, TPointD dpiScale) {
  // nota. hook position is in the coordinate system of the parent object.
  // a inch is Stage::inch

  TXshCell cell = xsh->getCell(row, col);
  if (!cell.m_level) return;

  TStageObjectId columnId = TStageObjectId::ColumnId(col);
  // handle is the current handle (pivot) of the column. It can be H1,H2,... or
  // A,B,C,...
  std::string handle =
      xsh->getStageObject(TStageObjectId::ColumnId(col))->getHandle();
  bool handleIsHook = handle.find("H") == 0;
  TAffine aff       = xsh->getPlacement(columnId, row);

  // imageDpiAff represent the scale factor between the image and the camera
  // derived by dpi match (i.e. ignoring geometric transformation)
  // cameraPos = imageDpiAff * imagePos
  TAffine imageDpiAff;
  if (cell.m_level->getSimpleLevel())
    imageDpiAff =
        getDpiAffine(cell.m_level->getSimpleLevel(), cell.m_frameId, true);

  // center (inches)
  TPointD center           = xsh->getCenter(columnId, row);  // getHooks
  if (handleIsHook) center = TPointD(0, 0);

  // add the hook #0 (i.e. the regular center)
  hooks.push_back(HookData(xsh, col, 0, aff * TScale(Stage::inch) * center));

  // add the regular hooks
  HookSet *hookSet = cell.m_level->getHookSet();
  if (hookSet && hookSet->getHookCount() > 0) {
    for (int j = 0; j < hookSet->getHookCount(); j++) {
      Hook *hook = hookSet->getHook(j);
      if (hook && !hook->isEmpty()) {
        TPointD pos = hook->getAPos(cell.m_frameId);
        pos         = aff * imageDpiAff * pos;
        hooks.push_back(HookData(xsh, col, j + 1, pos));
      }
    }
  }
}

//-------------------------------------------------------------------

static void getConnectedColumns(std::set<int> &connectedColumns, TXsheet *xsh,
                                int col) {
  TStageObjectId id;
  // insert col and all column ancestors
  id = TStageObjectId::ColumnId(col);
  do {
    connectedColumns.insert(id.getIndex());
    id = xsh->getStageObjectParent(id);
  } while (id.isColumn());
  // for each column
  for (int i = 0; i < xsh->getColumnCount(); i++) {
    id = TStageObjectId::ColumnId(i);
    std::vector<TStageObjectId> stack;
    // find a column ancestor already connected; put all the column ancestors in
    // stack
    while (id.isColumn() && connectedColumns.count(id.getIndex()) == 0) {
      stack.push_back(id);
      id = xsh->getStageObjectParent(id);
    }
    if (id.isColumn()) {
      // the stack is connected
      for (int j = 0; j < (int)stack.size(); j++)
        connectedColumns.insert(stack[j].getIndex());
    }
  }
}

static bool canShowBone(Skeleton::Bone *bone, TXsheet *xsh, int row) {
  TStageObjectId id = bone->getStageObject()->getId();
  if (!xsh->getCell(row, id.getIndex()).isEmpty() &&
      xsh->getColumn(id.getIndex())->isCamstandVisible())
    return true;
  int i;
  for (i = 0; i < bone->getChildCount(); i++) {
    if (canShowBone(bone->getChild(i), xsh, row)) return true;
  }
  return false;
}

//============================================================

HookData::HookData(TXsheet *xsh, int columnIndex, int hookId,
                   const TPointD &pos)
    : m_columnIndex(columnIndex)
    , m_hookId(hookId)
    , m_pos(pos)
    , m_isPivot(false) {
  std::string handle =
      xsh->getStageObject(TStageObjectId::ColumnId(columnIndex))->getHandle();
  if (m_hookId == 0) {
    // if the current handle is a hook the the hook#0 should be the default
    // center, i.e. B
    if (handle.find("H") == 0)
      m_name = "B";
    else {
      m_name    = handle;
      m_isPivot = true;
    }
  } else {
    m_name    = std::to_string(m_hookId);
    m_isPivot = "H" + m_name == handle;
  }
}

//============================================================

enum ToolDevice {
  TD_None        = -1,
  TD_Translation = 1,
  TD_Rotation,
  TD_Center,
  TD_ChangeParent,
  TD_ChangeDrawing,
  TD_IncrementDrawing,
  TD_DecrementDrawing,
  TD_InverseKinematics,

  TD_Hook            = 10000,
  TD_LockStageObject = 20000,
  TD_MagicLink       = 30000,

  TD_Test = 100000
};

//============================================================
//
// SkeletonTool
//
//------------------------------------------------------------

SkeletonTool skeletonTool;

//-------------------------------------------------------------------

SkeletonTool::SkeletonTool()
    : TTool("T_Skeleton")
    , m_active(false)
    , m_device(TD_None)
    , m_mode("Mode:")
    , m_showOnlyActiveSkeleton("Show Only Active Skeleton", false)
    , m_globalKeyframes("Global Key", false)
    , m_dragTool(0)
    , m_firstTime(true)
    , m_currentFrame(-1)
    , m_parentProbe()
    , m_parentProbeEnabled(false)
    , m_otherColumn(-1)
    , m_otherColumnBBoxAff()
    , m_otherColumnBBox()
    , m_labelPos(0, 0)
    , m_label("") {
  bind(TTool::CommonLevels);
  m_prop.bind(m_mode);
  m_prop.bind(m_globalKeyframes);
  m_prop.bind(m_showOnlyActiveSkeleton);
  m_mode.setId("SkeletonMode");
  m_globalKeyframes.setId("GlobalKey");
  m_showOnlyActiveSkeleton.setId("ShowOnlyActiveSkeleton");

  m_mode.addValue(BUILD_SKELETON);
  m_mode.addValue(ANIMATE);
  m_mode.addValue(INVERSE_KINEMATICS);

  m_commandHandler = new CommandHandler();
  m_commandHandler->setTempPinnedSet(&m_temporaryPinnedColumns);
}

//-------------------------------------------------------------------

SkeletonTool::~SkeletonTool() { delete m_dragTool; }

//-------------------------------------------------------------------

bool SkeletonTool::onPropertyChanged(std::string propertyName) {
  SkeletonGlobalKeyFrame = (int)(m_globalKeyframes.getValue());
  // SkeletonInverseKinematics=(int)(m_ikEnabled.getValue());
  invalidate();
  return false;
}

//-------------------------------------------------------------------

bool SkeletonTool::isGlobalKeyframesEnabled() const {
  return m_globalKeyframes.getValue();
}

//-------------------------------------------------------------------

void SkeletonTool::updateTranslation() {
  // m_ikEnabled.setQStringName(tr("Inverse Kinematics"));
  m_showOnlyActiveSkeleton.setQStringName(tr("Show Only Active Skeleton"));
  m_globalKeyframes.setQStringName(tr("Global Key"));
  m_mode.setQStringName(tr("Mode:"));
}

//-------------------------------------------------------------------

bool SkeletonTool::doesApply() const {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  assert(xsh);
  TStageObjectId objId = app->getCurrentObject()->getObjectId();
  if (objId.isColumn()) {
    TXshColumn *column = xsh->getColumn(objId.getIndex());
    if (column && column->getSoundColumn()) return false;
  }
  return true;
}

//-------------------------------------------------------------------

void SkeletonTool::mouseMove(const TPointD &, const TMouseEvent &e) {
  int selectedDevice = pick(e.m_pos);
  if (selectedDevice != m_device) {
    m_device = selectedDevice;
    invalidate();
  }
}

//-------------------------------------------------------------------

void SkeletonTool::leftButtonDown(const TPointD &ppos, const TMouseEvent &e) {
  m_otherColumn        = -1;
  m_otherColumnBBox    = TRectD();
  m_otherColumnBBoxAff = TAffine();
  m_labelPos           = TPointD(0, 0);
  m_label              = "";

  TUndoManager::manager()->beginBlock();
  if (!doesApply()) return;

  assert(m_dragTool == 0);
  m_dragTool = 0;

  TTool::Application *app = TTool::getApplication();
  int currentColumnIndex  = app->getCurrentColumn()->getColumnIndex();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  TPointD pos             = ppos;

  int selectedDevice = pick(e.m_pos);

  // cambio drawing
  if (selectedDevice == TD_ChangeDrawing ||
      selectedDevice == TD_IncrementDrawing ||
      selectedDevice == TD_DecrementDrawing) {
    int d = 0;
    if (selectedDevice == TD_IncrementDrawing)
      d = 1;
    else if (selectedDevice == TD_DecrementDrawing)
      d        = -1;
    m_dragTool = new ChangeDrawingTool(this, d);
    m_dragTool->leftButtonDown(ppos, e);
    return;
  }

  // click su un hook: attacca la colonna corrente tramite quell'hook
  if (TD_Hook <= selectedDevice && selectedDevice < TD_Hook + 50) {
    TXsheet *xsh         = app->getCurrentXsheet()->getXsheet();
    TStageObjectId objId = TStageObjectId::ColumnId(currentColumnIndex);
    TPointD p0           = getCurrentColumnMatrix() * TPointD(0, 0);
    HookData hook(xsh, currentColumnIndex, selectedDevice - TD_Hook, p0);
    TStageObjectCmd::setHandle(objId, hook.getHandle(),
                               app->getCurrentXsheet());
    app->getCurrentXsheet()->notifyXsheetChanged();
    invalidate();
    return;
  }

  // magic link
  if (TD_MagicLink <= selectedDevice &&
      selectedDevice < TD_MagicLink + (int)m_magicLinks.size()) {
    magicLink(selectedDevice - TD_MagicLink);
    app->getCurrentXsheet()->notifyXsheetChanged();
    return;
  }

  m_device          = selectedDevice;
  bool justSelected = false;

  if (m_device < 0) {
    // nessun gadget cliccato. Eventualmente seleziono la colonna
    std::vector<int> columnIndexes;
    getViewer()->posToColumnIndexes(e.m_pos, columnIndexes, getPixelSize() * 5,
                                    false);
    if (!columnIndexes.empty()) {
      int columnIndex;
      columnIndex = columnIndexes.back();

      if (columnIndex >= 0 && columnIndex != currentColumnIndex) {
        if (!isColumnLocked(columnIndex)) {
          pos = getMatrix() * pos;
          app->getCurrentColumn()->setColumnIndex(columnIndex);
          updateMatrix();
          currentColumnIndex = columnIndex;
          justSelected       = true;
          pos                = getMatrix().inv() * pos;
        } else {
          m_label    = "Column is locked";
          m_labelPos = pos;
        }
      }
    }
  }

  if (m_device < 0) {
    if (m_mode.getValue() == INVERSE_KINEMATICS)
      m_device = TD_InverseKinematics;
    else if (m_mode.getValue() == ANIMATE)
      m_device = TD_Rotation;
  }

  // lock/unlock: modalita IK
  if (TD_LockStageObject <= m_device && m_device < TD_LockStageObject + 1000) {
    int columnIndex = m_device - TD_LockStageObject;
    int frame       = app->getCurrentFrame()->getFrame();
    togglePinnedStatus(columnIndex, frame, e.isShiftPressed());
    invalidate();
    m_dragTool = 0;
    return;
  }

  switch (m_device) {
  case TD_Center:
    m_dragTool = new DragCenterTool(this);
    break;
  case TD_Translation:
    m_dragTool = new DragPositionTool(this);
    break;
  case TD_Rotation:
    m_dragTool = new DragRotationTool(this, justSelected);
    break;
  case TD_ChangeParent:
    m_dragTool = new ParentChangeTool(this, getViewer());
    break;
  case TD_InverseKinematics: {
    Skeleton *skeleton = new Skeleton();
    buildSkeleton(*skeleton, currentColumnIndex);
    m_dragTool = new IKTool(this, getViewer(), skeleton, currentColumnIndex);
    break;
  }
  }

  if (m_dragTool) {
    m_dragTool->leftButtonDown(pos, e);
    invalidate();
  }
}

//-------------------------------------------------------------------

void SkeletonTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (m_dragTool) {
    m_dragTool->leftButtonDrag(pos, e);
    invalidate();
  }
}

//-------------------------------------------------------------------

void SkeletonTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  m_label    = "";
  m_labelPos = TPointD(0, 0);

  if (m_dragTool) {
    m_dragTool->leftButtonUp(pos, e);
    delete m_dragTool;
    m_dragTool = 0;

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(
        false);  // Keyframes navigator reads this
  }
  if (m_device == TD_IncrementDrawing || m_device == TD_DecrementDrawing ||
      m_device == TD_ChangeDrawing)
    m_device = pick(e.m_pos);
  else
    m_device = -1;
  invalidate();
  TUndoManager::manager()->endBlock();
}

//-------------------------------------------------------------------

bool SkeletonTool::keyDown(QKeyEvent *event) {
  ChangeDrawingTool tool(this, 0);
  switch (event->key()) {
  case Qt::Key_Up:
    tool.changeDrawing(1);
    break;
  case Qt::Key_Down:
    tool.changeDrawing(-1);
    break;
  default:
    return false;
    break;
  }
  invalidate();
  return true;
}

//-------------------------------------------------------------------

class TogglePinnedStatusUndo final : public TUndo {
  SkeletonTool *m_tool;
  std::set<int> m_oldTemp, m_newTemp;
  int m_columnIndex, m_oldColumnIndex;
  std::pair<int, int> m_newRange, m_oldRange;
  TAffine m_oldPlacement, m_newPlacement;
  std::vector<std::pair<TStageObjectId, TStageObject::Keyframe>> m_keyframes;
  int m_frame;

public:
  TogglePinnedStatusUndo(SkeletonTool *tool, int frame)
      : m_tool(tool)
      , m_oldTemp()
      , m_newTemp()
      , m_columnIndex(-1)
      , m_oldColumnIndex(-1)
      , m_newRange(0, -1)
      , m_oldRange(0, -1)
      , m_frame(frame) {}

  void addBoneId(const TStageObjectId &id) {
    TStageObject *stageObject = getXsheet()->getStageObject(id);
    if (stageObject) {
      TStageObject::Keyframe k = stageObject->getKeyframe(m_frame);
      m_keyframes.push_back(std::make_pair(id, k));
    }
  }

  void setOldRange(int columnIndex, int first, int second,
                   const TAffine &placement) {
    m_oldColumnIndex = columnIndex;
    m_oldRange       = std::make_pair(first, second);
    m_oldPlacement   = placement;
  }
  void setNewRange(int columnIndex, int first, int second,
                   const TAffine &placement) {
    m_columnIndex  = columnIndex;
    m_newRange     = std::make_pair(first, second);
    m_newPlacement = placement;
  }
  void setOldTemp(const std::set<int> &oldTemp) { m_oldTemp = oldTemp; }
  void setNewTemp(const std::set<int> &newTemp) { m_newTemp = newTemp; }

  TStageObject *getStageObject(int columnIndex) const {
    return TTool::getApplication()
        ->getCurrentXsheet()
        ->getXsheet()
        ->getStageObject(TStageObjectId::ColumnId(columnIndex));
  }

  TPinnedRangeSet *getRangeSet(int columnIndex) const {
    return getStageObject(columnIndex)->getPinnedRangeSet();
  }

  TXsheet *getXsheet() const {
    return TTool::getApplication()->getCurrentXsheet()->getXsheet();
  }

  void notify() const {
    m_tool->invalidate();
    TXsheet *xsh         = getXsheet();
    int index            = m_columnIndex;
    if (index < 0) index = m_oldColumnIndex;
    if (index >= 0) {
      TStageObjectId id = TStageObjectId::ColumnId(index);
      TStageObjectId parentId;
      while (parentId = xsh->getStageObjectParent(id), parentId.isColumn())
        id = parentId;
      xsh->getStageObject(id)->invalidate();
      TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
      TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
    }
  }

  void undo() const override {
    m_tool->setTemporaryPinnedColumns(m_oldTemp);

    if (m_columnIndex >= 0)
      getRangeSet(m_columnIndex)
          ->removeRange(m_newRange.first, m_newRange.second);
    if (m_oldColumnIndex >= 0) {
      TPinnedRangeSet *rangeSet = getRangeSet(m_oldColumnIndex);
      rangeSet->setRange(m_oldRange.first, m_oldRange.second);
      rangeSet->setPlacement(m_oldPlacement);
    }
    TXsheet *xsh = getXsheet();
    for (int i = 0; i < (int)m_keyframes.size(); i++) {
      TStageObject *stageObject =
          getXsheet()->getStageObject(m_keyframes[i].first);
      if (!stageObject) continue;
      stageObject->removeKeyframeWithoutUndo(m_frame);
      if (m_keyframes[i].second.m_isKeyframe)
        stageObject->setKeyframeWithoutUndo(m_frame, m_keyframes[i].second);
    }

    notify();
  }

  void redo() const override {
    TXsheet *xsh = getXsheet();
    for (int i = 0; i < (int)m_keyframes.size(); i++) {
      TStageObject *stageObject =
          getXsheet()->getStageObject(m_keyframes[i].first);
      if (stageObject) stageObject->setKeyframeWithoutUndo(m_frame);
    }
    m_tool->setTemporaryPinnedColumns(m_newTemp);

    if (m_oldColumnIndex >= 0)
      getRangeSet(m_oldColumnIndex)
          ->removeRange(m_oldRange.first, m_oldRange.second);
    if (m_columnIndex >= 0) {
      TPinnedRangeSet *rangeSet = getRangeSet(m_columnIndex);
      rangeSet->setRange(m_newRange.first, m_newRange.second);
      rangeSet->setPlacement(m_newPlacement);
    }
    notify();
  }
  int getSize() const override { return sizeof *this; }
};

//-------------------------------------------------------------------

void SkeletonTool::togglePinnedStatus(int columnIndex, int frame,
                                      bool shiftPressed) {
  Skeleton skeleton;
  buildSkeleton(skeleton, columnIndex);
  if (!skeleton.getRootBone() || !skeleton.getRootBone()->getStageObject())
    return;
  Skeleton::Bone *bone = skeleton.getBoneByColumnIndex(columnIndex);
  assert(bone);
  if (!bone) return;

  TogglePinnedStatusUndo *undo = new TogglePinnedStatusUndo(this, frame);
  for (int i = 0; i < skeleton.getBoneCount(); i++) {
    TStageObject *obj = skeleton.getBone(i)->getStageObject();
    if (obj) {
      undo->addBoneId(obj->getId());
      obj->setKeyframeWithoutUndo(frame);
    }
  }

  getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  getApplication()->getCurrentObject()->notifyObjectIdChanged(false);

  undo->setOldTemp(m_temporaryPinnedColumns);
  bool isTemporaryPinned = m_temporaryPinnedColumns.count(columnIndex) > 0;
  if (shiftPressed || isTemporaryPinned) {
    if (isTemporaryPinned)
      m_temporaryPinnedColumns.erase(columnIndex);
    else
      m_temporaryPinnedColumns.insert(columnIndex);
  } else {
    TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    TAffine placement =
        xsh->getPlacement(bone->getStageObject()->getId(), frame);

    TStageObjectId rootId = skeleton.getRootBone()->getStageObject()->getId();
    TAffine rootPlacement = xsh->getPlacement(rootId, frame);

    int pinnedStatus = bone->getPinnedStatus();
    if (pinnedStatus != Skeleton::Bone::PINNED) {
      int oldPinned = -1;
      for (int i = 0; i < skeleton.getBoneCount(); i++) {
        TStageObject *obj = skeleton.getBone(i)->getStageObject();
        if (obj->getPinnedRangeSet()->isPinned(frame)) {
          oldPinned = i;
          break;
        }
      }

      int lastFrame = 1000000;
      if (oldPinned >= 0) {
        assert(skeleton.getBone(oldPinned) != bone);
        TStageObject *obj = skeleton.getBone(oldPinned)->getStageObject();
        const TPinnedRangeSet::Range *range =
            obj->getPinnedRangeSet()->getRange(frame);
        assert(range && range->first <= frame && frame <= range->second);
        lastFrame                 = range->second;
        TPinnedRangeSet *rangeSet = obj->getPinnedRangeSet();
        rangeSet->removeRange(frame, range->second);
        obj->invalidate();
        undo->setOldRange(oldPinned, frame, range->second,
                          rangeSet->getPlacement());
      } else {
        for (int i = 0; i < skeleton.getBoneCount(); i++) {
          TStageObject *obj = skeleton.getBone(i)->getStageObject();
          const TPinnedRangeSet::Range *range =
              obj->getPinnedRangeSet()->getNextRange(frame);
          if (range) {
            assert(range->first > frame);
            if (range->first - 1 < lastFrame) lastFrame = range->first - 1;
          }
        }
      }

      TStageObject *obj         = bone->getStageObject();
      TPinnedRangeSet *rangeSet = obj->getPinnedRangeSet();
      rangeSet->setRange(frame, lastFrame);
      if (frame == 0) {
        // this code should be moved elsewhere, possibly in the stageobject
        // implementation
        // the idea is to remove the normal
        TStageObject *rootObj = skeleton.getRootBone()->getStageObject();
        rootObj->setStatus(TStageObject::XY);
        placement = rootObj->getPlacement(0).inv() * placement;
        rootObj->setStatus(TStageObject::IK);
        rangeSet->setPlacement(placement);
        rootObj->invalidate();
      }
      undo->setNewRange(bone->getColumnIndex(), frame, lastFrame,
                        rangeSet->getPlacement());
    }
  }
  undo->setNewTemp(m_temporaryPinnedColumns);
  TUndoManager::manager()->add(undo);
}

//-------------------------------------------------------------------

void SkeletonTool::drawSkeleton(const Skeleton &skeleton, int row) {
  bool buildingSkeleton = m_mode.getValue() == BUILD_SKELETON;
  bool ikEnabled        = m_mode.getValue() == INVERSE_KINEMATICS;

  TXsheet *xsh = getXsheet();
  std::vector<int> showBoneIndex;
  int i;
  for (i = 0; i < skeleton.getBoneCount(); i++) {
    Skeleton::Bone *bone = skeleton.getBone(i);
    TStageObjectId id    = bone->getStageObject()->getId();
    bool canShow         = canShowBone(bone, xsh, row);
    if (!canShow) continue;
    showBoneIndex.push_back(i);
  }

  bool changingParent = dynamic_cast<ParentChangeTool *>(m_dragTool) != 0;
  TStageObjectId currentObjectId =
      TTool::getApplication()->getCurrentObject()->getObjectId();
  std::string currentHandle = xsh->getStageObject(currentObjectId)->getHandle();

  for (i = 0; i < (int)showBoneIndex.size(); i++) {
    Skeleton::Bone *bone = skeleton.getBone(showBoneIndex[i]);
    TStageObjectId id    = bone->getStageObject()->getId();
    bool isCurrent       = id == currentObjectId;
    if (isCurrent && buildingSkeleton && m_parentProbeEnabled) {
      if (!m_magicLinks.empty()) {
        drawBone(bone->getCenter(), m_magicLinks[0].m_h1.m_pos, false);
      }
      drawBone(bone->getCenter(), m_parentProbe, true);
    } else if (ikEnabled) {
      if (bone->getParent())
        drawIKBone(bone->getCenter(), bone->getParent()->getCenter());
    } else if (bone->getParent() || isCurrent) {
      double pixelSize = getPixelSize();
      TPointD a        = bone->getCenter();
      TPointD b, pm;
      if (bone->getParent()) {
        b  = bone->getParent()->getCenter();
        pm = (a + b) * 0.5;
      } else {
        pm = b = a + TPointD(0, 60) * pixelSize;
      }
      bool boneIsVisible = false;
      if (buildingSkeleton) {
        boneIsVisible = true;
        if (bone->isSelected())
          drawBone(a, b, true);
        else if (!m_showOnlyActiveSkeleton.getValue())
          drawBone(a, b, false);
        else
          boneIsVisible = false;
      }
      if (boneIsVisible && isCurrent) {
        // draw change parent gadget
        double r = pixelSize * 5;
        if (isPicking()) {
          // int code = TD_ResetParent +
          // bone->getStageObject()->getId().getIndex();
          glPushName(TD_ChangeParent);
          tglDrawDisk(pm, r);
          glPopName();
        } else {
          if (m_device == TD_ChangeParent) {
            glColor4d(0.47 * alpha, 0.6 * alpha, 0.65 * alpha, alpha);
            r *= 1.5;
          } else
            glColor4d(0.37 * alpha, 0.5 * alpha, 0.55 * alpha, alpha);
          glRectd(pm.x - r, pm.y - r, pm.x + r, pm.y + r);
          glColor3d(0, 0, 0);
          tglDrawRect(pm.x - r, pm.y - r, pm.x + r, pm.y + r);
        }
      }
    }
  }
  for (i = 0; i < (int)showBoneIndex.size(); i++) {
    Skeleton::Bone *bone = skeleton.getBone(showBoneIndex[i]);
    if (!m_showOnlyActiveSkeleton.getValue() || bone->isSelected())
      drawJoint(bone->getCenter(),
                currentObjectId == bone->getStageObject()->getId() &&
                    currentHandle.find("H") != 0);
  }
}

//-------------------------------------------------------------------

void SkeletonTool::getImageBoundingBox(TRectD &bbox, TAffine &aff, int frame,
                                       int columnIndex) {
  TAffine columnAff =
      getXsheet()->getPlacement(TStageObjectId::ColumnId(columnIndex), frame);
  // TAffine affine = getColumnMatrix(columnIndex);
  TXshCell cell    = getXsheet()->getCell(frame, columnIndex);
  TImageP image    = cell.getImage(false);
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  if (ti) {
    TAffine imageDpiAff;
    if (cell.m_level->getSimpleLevel())
      imageDpiAff =
          getDpiAffine(cell.m_level->getSimpleLevel(), cell.m_frameId, true);
    aff  = columnAff * imageDpiAff;
    bbox = ToonzImageUtils::convertRasterToWorld(convert(ti->getBBox()), ti) *
           ti->getSubsampling();
    ToolUtils::drawRect(bbox * ti->getSubsampling(), TPixel32(200, 200, 200),
                        0x5555);
  } else if (vi) {
    bbox = vi->getBBox();
    aff  = columnAff;
  } else {
    bbox = TRectD();
    aff  = TAffine();
  }
}

//-------------------------------------------------------------------

void SkeletonTool::drawLevelBoundingBox(int frame, int columnIndex) {
  TAffine affine   = getCurrentColumnMatrix();
  TXshCell cell    = getXsheet()->getCell(frame, columnIndex);
  TImageP image    = cell.getImage(false);
  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  glPushMatrix();
  if (affine != getMatrix()) tglMultMatrix(getMatrix().inv() * affine);
  if (ti) {
    TPointD dpiScale = getViewer()->getDpiScale();
    glScaled(dpiScale.x, dpiScale.y, 1);
    TRectD bbox =
        ToonzImageUtils::convertRasterToWorld(convert(ti->getBBox()), ti);
    ToolUtils::drawRect(bbox * ti->getSubsampling(), TPixel32(200, 200, 200),
                        0x5555);
  }
  if (vi) {
    TRectD bbox = vi->getBBox();
    ToolUtils::drawRect(bbox, TPixel32(200, 200, 200), 0x5555);
  }
  glPopMatrix();
}

//-------------------------------------------------------------------

void SkeletonTool::drawIKJoint(const Skeleton::Bone *bone) {
  TPointD pos     = bone->getCenter();
  const double r0 = 6 * getPixelSize(), r1 = r0 / 3;
  int code = TD_LockStageObject + bone->getColumnIndex();
  glPushName(code);
  glColor3d(0.8, 0.5, 0.05);
  if (bone->getPinnedStatus() != Skeleton::Bone::FREE) {
    if (bone->getPinnedStatus() == Skeleton::Bone::TEMP_PINNED) {
      double r1 = r0 * 0.60;
      glColor3d(60.0 / 255.0, 250.0 / 255.0, 1.0);
      glRectd(pos.x - r0, pos.y - r0, pos.x + r0, pos.y + r0);

      glColor3d(0.78, 0.62, 0);
      glRectd(pos.x - r1, pos.y - r1, pos.x + r1, pos.y + r1);

      glColor3d(0.2, 0.1, 0.05);
      tglDrawRect(pos.x - r0, pos.y - r0, pos.x + r0, pos.y + r0);
      tglDrawRect(pos.x - r1, pos.y - r1, pos.x + r1, pos.y + r1);
    } else {
      glColor3d(0, 175.0 / 255.0, 1.0);
      glRectd(pos.x - r0, pos.y - r0, pos.x + r0, pos.y + r0);
      glColor3d(0.2, 0.1, 0.05);
      tglDrawRect(pos.x - r0, pos.y - r0, pos.x + r0, pos.y + r0);
    }
  } else {
    if (bone->isSelected())
      glColor3d(1, 0.78, 0.19);
    else
      glColor3d(0.78, 0.62, 0);
    tglDrawDisk(pos, r0);
    glColor3d(0.2, 0.1, 0.05);
    tglDrawCircle(pos, r0);
  }

  if (m_device == code) {
    glColor3d(0.9, 0.1, 0.1);
    tglFillRect(pos.x - r1, pos.y - r1, pos.x + r1, pos.y + r1);
  } else {
    glColor3d(0.2, 0.1, 0.05);
    const double r3 = 2 * getPixelSize();
    tglDrawCircle(pos, r3);
  }
  glPopName();
}

//-------------------------------------------------------------------

void SkeletonTool::drawJoint(const TPointD &pos, bool current) {
  const double alpha = 0.8, ialpha = 1 - alpha;
  double r0 = 4 * getPixelSize();
  if (current) {
    glPushName(TD_Center);
    if (m_device == TD_Center) {
      glColor4d(0.9 * alpha, 0.8 * alpha, 0.2 * alpha, alpha);
      r0 *= 1.5;
    } else {
      glColor4d(((255.0 / 255.0) - ialpha) / alpha,
                ((200.0 / 255.0) - ialpha) / alpha,
                ((48.0 / 255.0) - ialpha) / alpha, alpha);
    }
    tglDrawDisk(pos, r0);
    glColor3d(0.2, 0.1, 0.05);
    tglDrawCircle(pos, r0);
    glPopName();
  } else {
    // in build skeleton center is clickable, but only the current one
    if (m_mode.getValue() == BUILD_SKELETON)
      glColor4d(0.60 * alpha, 0.60 * alpha, 0.60 * alpha, alpha);
    else
      glColor4d(0.78 * alpha, 0.62 * alpha, 0 * alpha, alpha);
    tglDrawDisk(pos, r0);
    glColor3d(0.2, 0.1, 0.05);
    tglDrawCircle(pos, r0);
  }
}

//-------------------------------------------------------------------

void SkeletonTool::drawBone(const TPointD &a, const TPointD &b, bool selected) {
  const double alpha = 0.8;
  // se sono troppo vicini niente da fare
  TPointD delta = b - a;
  if (norm2(delta) < 0.001) return;
  TPointD u = getPixelSize() * 2.5 * normalize(rotate90(delta));
  if (selected)
    glColor4d(0.9 * alpha, 0.9 * alpha, 0.9 * alpha, alpha);
  else
    glColor4d(0.58 * alpha, 0.58 * alpha, 0.58 * alpha, alpha);
  glBegin(GL_POLYGON);
  tglVertex(a + u);
  tglVertex(b);
  tglVertex(a - u);
  glEnd();
  glColor3d(0.2, 0.3, 0.35);
  glBegin(GL_LINE_STRIP);
  tglVertex(a + u);
  tglVertex(b);
  tglVertex(a - u);
  glEnd();
}

//-------------------------------------------------------------------

void SkeletonTool::drawIKBone(const TPointD &a, const TPointD &b) {
  // se sono troppo vicini niente da fare
  TPointD delta = b - a;
  if (norm2(delta) < 0.001) return;
  TPointD u = getPixelSize() * 2.5 * normalize(rotate90(delta));
  glColor3d(0.58, 0.58, 0.58);
  glBegin(GL_POLYGON);
  tglVertex(a + u);
  tglVertex(b + u);
  tglVertex(b - u);
  tglVertex(a - u);
  glEnd();
  glColor3d(0.2, 0.3, 0.35);
  glBegin(GL_LINES);
  tglVertex(a + u);
  tglVertex(b + u);
  tglVertex(a - u);
  tglVertex(b - u);
  glEnd();
}

//-------------------------------------------------------------------

void SkeletonTool::computeMagicLinks() {
  // TODO: spostare qui il calcolo dei magic link
}

//-------------------------------------------------------------------

void SkeletonTool::drawHooks() {
  // camera stand reference system
  // glColor3d(0,0,1);
  // tglDrawRect(3,3,97,97);

  // glColor3d(0,1,1);
  // tglDrawRect(0,100,Stage::inch,110);

  QTime time;
  time.start();

  m_magicLinks.clear();
  computeMagicLinks();

  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  int row                 = app->getCurrentFrame()->getFrame();
  int col                 = app->getCurrentColumn()->getColumnIndex();
  TPointD dpiScale        = getViewer()->getDpiScale();

  // glColor3d(1,0,1);
  // tglDrawRect(-100*dpiScale.x, -100*dpiScale.y, 0,0);

  // I should not show hooks of columns already connected with the current one
  // ("linking" to those hooks would create evil loops in the hierarchy)
  std::set<int> connectedColumns;
  getConnectedColumns(connectedColumns, xsh, col);

  std::vector<HookData> currentColumnHooks;
  std::vector<HookData> otherColumnsHooks;

  // fill currentColumnHooks
  // detect otherColumn (i.e. the active column during a click&drag operator
  int otherColumn = -1;
  if (m_parentProbeEnabled) {
    // qDebug("  parent probe enabled");
    // currentColumnHooks <- current column & current handle
    int hookId = 0;
    std::string handle =
        xsh->getStageObject(TStageObjectId::ColumnId(col))->getHandle();
    if (handle.find("H") == 0) {
      int j = 1;
      while (j < (int)handle.size() && '0' <= handle[j] && handle[j] <= '9') {
        hookId = hookId * 10 + (int)handle[j] - '0';
        j++;
      }
    }

    currentColumnHooks.push_back(HookData(xsh, col, hookId, m_parentProbe));

    // otherColumn = "picked" column not connected
    TPointD parentProbePos = getViewer()->worldToPos(m_parentProbe);
    std::vector<int> indexes;
    getViewer()->posToColumnIndexes(parentProbePos, indexes,
                                    getPixelSize() * 10, false);
    for (int i = (int)indexes.size() - 1; i >= 0; i--) {
      if (connectedColumns.count(indexes[i]) == 0) {
        otherColumn = indexes[i];
        break;
      }
    }
    // if the mouse is not on a stroke, I'm using the "old" m_otherColumn, if
    // any.
    // (this is needed because of the hooks put inside of unfilled regions)
    if (otherColumn < 0 && m_otherColumn >= 0) {
      if (m_otherColumnBBox.contains(m_otherColumnBBoxAff.inv() *
                                     m_parentProbe))
        otherColumn = m_otherColumn;
      else
        m_otherColumn = -1;
    }
  } else {
    // parent probe not enabled: get all hooks of current column. other column =
    // -1
    getHooks(currentColumnHooks, xsh, row, col, TPointD(1, 1));  // dpiScale);
  }

  // other columns hooks <- all hooks of all unlinked columns
  for (int i = 0; i < xsh->getColumnCount(); i++)
    if (xsh->getColumn(i)->isCamstandVisible())
      if (connectedColumns.count(i) == 0)
        getHooks(otherColumnsHooks, xsh, row, i, TPointD(1, 1));  // dpiScale);

  /*
qDebug("  time=%dms", time.elapsed());
qDebug("  %d hooks (current column)", currentColumnHooks.size());
for(int i=0;i<(int)currentColumnHooks.size();i++)
qDebug("
%d,%d",currentColumnHooks[i].m_columnIndex,currentColumnHooks[i].m_hookId);
qDebug("  %d hooks (other columns)", otherColumnsHooks.size());
for(int i=0;i<(int)otherColumnsHooks.size();i++)
qDebug("
%d,%d",otherColumnsHooks[i].m_columnIndex,otherColumnsHooks[i].m_hookId);
*/

  std::vector<TRectD> balloons;

  // draw current column hooks
  for (int i = 0; i < (int)currentColumnHooks.size(); i++) {
    const HookData &hook = currentColumnHooks[i];
    if (hook.m_name == "") continue;  // should not happen
    int code    = TD_Hook + hook.m_hookId;
    TPointD pos = hook.m_pos;
    ToolUtils::drawHook(pos, ToolUtils::OtherLevelHook);
    glPushName(code);
    TPixel32 color(200, 220, 205, 200);
    if (hook.m_isPivot)
      color = TPixel32(200, 200, 10, 200);
    else if (code == m_device)
      color = TPixel32(185, 255, 255);
    ToolUtils::drawBalloon(pos, hook.m_name, color, TPoint(20, 20),
                           getPixelSize(), isPicking(), &balloons);
    glPopName();
  }

  if (m_parentProbeEnabled) {
    for (int i = 0; i < (int)otherColumnsHooks.size(); i++)
      ToolUtils::drawHook(otherColumnsHooks[i].m_pos,
                          ToolUtils::OtherLevelHook);
  }

  // search for magic links
  double minDist2       = 0;
  double snapRadius2    = 100 * getPixelSize() * getPixelSize();
  double snapRadius2bis = 100;

  if (!m_parentProbeEnabled) {
    // "static" magic links: no parent probe
    for (int i = 0; i < (int)currentColumnHooks.size(); i++) {
      for (int j = 0; j < (int)otherColumnsHooks.size(); j++) {
        double dist2 =
            norm2(currentColumnHooks[i].m_pos - otherColumnsHooks[j].m_pos);
        if (currentColumnHooks[i].m_hookId == 0 ||
            otherColumnsHooks[j].m_hookId == 0)
          continue;
        if (dist2 < snapRadius2bis) {
          m_magicLinks.push_back(
              MagicLink(currentColumnHooks[i], otherColumnsHooks[j], dist2));

          qDebug("  magic link_a %d (%d,%d) %d (%d,%d); dist=%f", i,
                 currentColumnHooks[i].m_columnIndex,
                 currentColumnHooks[i].m_hookId, j,
                 otherColumnsHooks[j].m_columnIndex,
                 otherColumnsHooks[j].m_hookId, dist2);
        }
      }
    }
  }

  if (m_parentProbeEnabled) {
    // search for the closest hook of the picked column
    int i = -1, j = -1;
    double minDist2 = snapRadius2;
    for (i = 0; i < (int)otherColumnsHooks.size(); i++) {
      double dist2 = tdistance2(otherColumnsHooks[i].m_pos, m_parentProbe);
      if (dist2 < minDist2) {
        j           = i;
        minDist2    = dist2;
        otherColumn = otherColumnsHooks[i].m_columnIndex;
      }
    }
  }

  if (m_parentProbeEnabled && otherColumn >= 0)  //  && m_magicLinks.empty()
  {
    // "dynamic" magic links: probing and picking a column

    // show image bounding box
    m_otherColumn = otherColumn;
    getImageBoundingBox(m_otherColumnBBox, m_otherColumnBBoxAff, row,
                        otherColumn);
    if (!m_otherColumnBBox.isEmpty()) {
      glPushMatrix();
      tglMultMatrix(m_otherColumnBBoxAff);
      ToolUtils::drawRect(m_otherColumnBBox, TPixel32(188, 202, 191), 0xF0F0);
      // tglDrawRect(img->getBBox());
      glPopMatrix();
    }

    /*
TXshCell cell = xsh->getCell(row, otherColumn);
//TAffine aff = xsh->getPlacement(TStageObjectId::ColumnId(otherColumn),row);
//m_otherColumnAff = aff;
TImageP img = cell.getImage(false);
if(img)
{
getImageBoundingBox(m_otherColumnBBox, m_otherColumnBBoxAff, row, otherColumn);
//glColor3d(188.0/255.0, 202.0/255.0, 191.0/255.0);
glPushMatrix();
tglMultMatrix(aff * imageDpiAff);
ToolUtils::drawRect(img->getBBox(), TPixel32(188,202,191), 0xF0F0);
//tglDrawRect(img->getBBox());
glPopMatrix();
}
*/
    // search for the closest hook of the picked column
    int i = -1, j = -1;
    double minDist2 = snapRadius2;
    for (i = 0; i < (int)otherColumnsHooks.size(); i++)
      if (otherColumnsHooks[i].m_columnIndex == otherColumn) {
        double dist2 = tdistance2(otherColumnsHooks[i].m_pos, m_parentProbe);
        if (j < 0)
          j = i;
        else if (dist2 < minDist2 || otherColumnsHooks[i].m_hookId == 0) {
          j        = i;
          minDist2 = dist2;
        }
      }
    // visualize a balloon for the closest hook
    if (0 <= j && j < (int)otherColumnsHooks.size()) {
      // I want to get a specific color when overlaying the label on white
      int alfa = 100, ialfa = 255 - alfa;
      TPixel32 color(255 * (188 - ialfa) / alfa, 255 * (202 - ialfa) / alfa,
                     255 * (191 - ialfa) / alfa, alfa);
      ToolUtils::drawBalloon(otherColumnsHooks[j].m_pos,
                             otherColumnsHooks[j].m_name,  // getHandle(),
                             color, TPoint(20, 20), getPixelSize(), false,
                             &balloons);

      HookData baseHook = currentColumnHooks[0];
      baseHook.m_pos    = otherColumnsHooks[j].m_pos;

      // this is also a magic link
      m_magicLinks.push_back(MagicLink(baseHook, otherColumnsHooks[j], 10000));
    }
  }

  // visualize all the magic links

  for (int i = 0; i < (int)m_magicLinks.size(); i++) {
    const MagicLink &magicLink = m_magicLinks[i];
    const HookData &h1         = magicLink.m_h1;
    std::string name;
    name = (m_parentProbeEnabled ? "Linking " : "Link ") +
           removeTrailingH(magicLink.m_h0.getHandle()) + " to Col " +
           std::to_string(h1.m_columnIndex + 1) + "/" +
           removeTrailingH(h1.getHandle());

    int code = TD_MagicLink + i;
    glPushName(code);
    TPixel32 color(100, 255, 100, 100);
    if (code == m_device) color = TPixel32(185, 255, 255);
    ToolUtils::drawBalloon(magicLink.m_h0.m_pos, name, color, TPoint(20, -20),
                           getPixelSize(), isPicking(), &balloons);
    glPopName();
  }
}

//-------------------------------------------------------------------

void SkeletonTool::drawDrawingBrowser(const TXshCell &cell,
                                      const TPointD &center) {
  if (!cell.m_level || cell.m_level->getFrameCount() <= 1) return;
  double pixelSize = getPixelSize();

  std::string name = ::to_string(cell.m_level->getName()) + "." +
                     std::to_string(cell.m_frameId.getNumber());

  QString qText = QString::fromStdString(name);
  QFont font("Arial", 10);  // ,QFont::Bold);
  QFontMetrics fm(font);
  QSize textSize   = fm.boundingRect(qText).size();
  int arrowHeight  = 10;
  int minTextWidth = 2 * arrowHeight + 5;
  if (textSize.width() < minTextWidth) textSize.setWidth(minTextWidth);
  QSize totalSize(textSize.width(), textSize.height() + 2 * arrowHeight);
  TPointD p = center + TPointD(30, -arrowHeight) * pixelSize;
  QRect textRect(0, arrowHeight, textSize.width(), textSize.height());

  assert(glGetError() == 0);

  if (isPicking()) {
    double x0 = p.x, x1 = p.x + totalSize.width() * pixelSize;
    double y0 = p.y, y3 = p.y + totalSize.height() * pixelSize;
    double y1 = y0 + arrowHeight * pixelSize;
    double y2 = y3 - arrowHeight * pixelSize;
    double x  = (x0 + x1) * 0.5;
    double d  = arrowHeight * pixelSize;

    glColor3d(0, 1, 0);
    glPushName(TD_ChangeDrawing);
    glRectd(x0, y1, x1, y2);
    glPopName();
    glPushName(TD_IncrementDrawing);
    glBegin(GL_POLYGON);
    glVertex2d(x, y0);
    glVertex2d(x + d, y0 + d);
    glVertex2d(x - d, y0 + d);
    glEnd();
    glPopName();
    glPushName(TD_DecrementDrawing);
    glBegin(GL_POLYGON);
    glVertex2d(x, y3);
    glVertex2d(x + d, y3 - d);
    glVertex2d(x - d, y3 - d);
    glEnd();
    glPopName();
    return;
  } else {
    assert(glGetError() == 0);
    bool active = m_device == TD_ChangeDrawing ||
                  m_device == TD_IncrementDrawing ||
                  m_device == TD_DecrementDrawing;
    QImage img(totalSize.width(), totalSize.height(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter imgPainter(&img);
    imgPainter.setRenderHints(QPainter::Antialiasing |
                              QPainter::TextAntialiasing);

    // imgPainter.setPen(Qt::black);
    // imgPainter.drawRect(0,0,totalSize.width(),totalSize.height());

    imgPainter.setPen(Qt::NoPen);
    imgPainter.setBrush(QColor(200, 200, 200, 200));
    imgPainter.drawRect(textRect);
    imgPainter.setPen(active ? Qt::red : Qt::black);
    imgPainter.setBrush(Qt::NoBrush);
    imgPainter.setFont(font);
    imgPainter.drawText(textRect, Qt::AlignCenter, qText);

    if (active) {
      int x = textRect.center().x();
      int d = arrowHeight - 4;
      int y = 0;
      QPainterPath upArrow;
      upArrow.moveTo(x, y);
      upArrow.lineTo(x + d, y + d);
      upArrow.lineTo(x - d, y + d);
      upArrow.lineTo(x, y);
      y = totalSize.height() - 1;
      QPainterPath dnArrow;
      dnArrow.moveTo(x, y);
      dnArrow.lineTo(x + d, y - d);
      dnArrow.lineTo(x - d, y - d);
      dnArrow.lineTo(x, y);

      imgPainter.setPen(Qt::NoPen);
      imgPainter.setBrush(m_device == TD_DecrementDrawing
                              ? QColor(255, 0, 0)
                              : QColor(200, 100, 100));
      imgPainter.drawPath(upArrow);
      imgPainter.setBrush(m_device == TD_IncrementDrawing
                              ? QColor(255, 0, 0)
                              : QColor(200, 100, 100));
      imgPainter.drawPath(dnArrow);
    }

    QImage texture = QGLWidget::convertToGLFormat(img);

    glRasterPos2f(p.x, p.y);
    // glBitmap(0,0,0,0,  0,-size.height()+(y+delta.y),  NULL); //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawPixels(texture.width(), texture.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                 texture.bits());
    glDisable(GL_BLEND);
    glColor3d(0, 0, 0);

    assert(glGetError() == 0);
  }
}

//-------------------------------------------------------------------

void SkeletonTool::drawMainGadget(const TPointD &center) {
  assert(glGetError() == GL_NO_ERROR);

  double r  = 10 * getPixelSize();
  double cx = center.x + r * 1.1;
  double cy = center.y - r * 1.1;

  glColor3d(1, 0, 0);

  if (isPicking()) {
    glPushName(TD_Translation);
    tglDrawDisk(TPointD(cx, cy), getPixelSize() * 9);
    glPopName();
    return;
  }

  QImage img(19, 19, QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  QPainter p(&img);
  // p.setRenderHints(QPainter::Antialiasing|QPainter::TextAntialiasing);

  QPainterPath pp;
  int dx = 1, dy = 0;
  for (int i = 0; i < 4; i++) {
    int x = 9 + dx * 8;
    int y = 9 + dy * 8;
    pp.moveTo(9, 9);
    pp.lineTo(x, y);
    pp.lineTo(x - 2 * dx - 2 * dy, y - 2 * dy + 2 * dx);
    pp.moveTo(x, y);
    pp.lineTo(x - 2 * dx + 2 * dy, y - 2 * dy - 2 * dx);
    int d = dx;
    dx    = -dy;
    dy    = d;
  }

  p.setPen(QPen(Qt::white, 3));
  p.drawPath(pp);
  p.setPen(Qt::black);
  p.drawPath(pp);

  p.setBrush(QColor(54, 213, 54));
  p.drawRect(6, 6, 6, 6);
  QImage texture = QGLWidget::convertToGLFormat(img);
  // texture.save("c:\\urka.png");

  glRasterPos2f(center.x + r * 1.1, center.y - r * 1.1);
  glBitmap(0, 0, 0, 0, -9, -9, NULL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDrawPixels(texture.width(), texture.height(), GL_RGBA, GL_UNSIGNED_BYTE,
               texture.bits());
  glDisable(GL_BLEND);
  glColor3d(0, 0, 0);

  assert(glGetError() == GL_NO_ERROR);
}

//-------------------------------------------------------------------

void SkeletonTool::draw() {
  // parent object reference system
  // glColor3d(1,0,0);
  // tglDrawRect(0,0,100,100);

  if (m_label != "")
    ToolUtils::drawBalloon(m_labelPos, m_label, TPixel32::Red, TPoint(20, -20),
                           getPixelSize(), false);

  bool ikEnabled = m_mode.getValue() == INVERSE_KINEMATICS;
  assert(glGetError() == GL_NO_ERROR);

  // l'xsheet, oggetto (e relativo placement), frame corrente
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = getXsheet();
  assert(xsh);
  TStageObjectId objId = app->getCurrentObject()->getObjectId();
  // se l'oggetto corrente non e' una colonna non disegno nulla
  if (!objId.isColumn()) return;

  TStageObject *pegbar = xsh->getStageObject(objId);
  int col              = objId.getIndex();

  int frame = app->getCurrentFrame()->getFrame();
  if (m_currentFrame != frame) m_temporaryPinnedColumns.clear();

  TAffine aff = getMatrix();
  // puo' suggere che il placement degeneri (es.: c'e' uno h-scale = 0%)
  if (fabs(aff.det()) < 0.00001) return;

  // m_unit = getPixelSize() * sqrt(fabs(aff.det()));

  if (!ikEnabled) drawLevelBoundingBox(frame, col);

  glPushMatrix();
  tglMultMatrix(aff.inv());

  // camera stand reference system
  // glColor3d(0,1,0);
  // tglDrawRect(0,0,100,100);

  bool changingParent = dynamic_cast<ParentChangeTool *>(m_dragTool) != 0;

  // !changingParent &&
  if (m_mode.getValue() == BUILD_SKELETON &&
      !xsh->getStageObjectParent(objId).isColumn()) {
    if (!changingParent) drawHooks();
  }

  Skeleton skeleton;
  buildSkeleton(skeleton, col);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  drawSkeleton(skeleton, frame);
  glDisable(GL_BLEND);

  TXshCell cell            = xsh->getCell(frame, objId.getIndex());
  Skeleton::Bone *rootBone = skeleton.getRootBone();
  for (int i = 0; i < skeleton.getBoneCount(); i++) {
    Skeleton::Bone *bone     = skeleton.getBone(i);
    TStageObjectId currentId = bone->getStageObject()->getId();
    bool isCurrent           = (currentId == objId);
    TPointD pos              = bone->getCenter();
    if (isCurrent && m_mode.getValue() != BUILD_SKELETON) {
      drawDrawingBrowser(cell, pos);
    }

    bool isActiveChain = bone->isSelected();

    glColor3d(0, 1, 0);
    if (ikEnabled) {
      drawIKJoint(bone);
    } else {
      TPointD pos = bone->getCenter();
      if (isCurrent && m_mode.getValue() == ANIMATE) {
        drawMainGadget(pos);
      }
    }
  }
  m_currentFrame = frame;

  if (m_dragTool) m_dragTool->draw();
  glPopMatrix();
}

//===================================================================

void SkeletonTool::onActivate() {
  TTool::Application *app = TTool::getApplication();
  if (m_firstTime) {
    m_globalKeyframes.setValue(SkeletonGlobalKeyFrame ? 1 : 0);
    m_mode.setValue(BUILD_SKELETON);
    // m_ikEnabled.setValue(SkeletonInverseKinematics ? 1 : 0);
    m_firstTime = false;
  }
  TStageObjectId objId = app->getCurrentObject()->getObjectId();
  if (objId == TStageObjectId::NoneId) {
    int index = app->getCurrentColumn()->getColumnIndex();
    objId     = TStageObjectId::ColumnId(index);
  }
  //  app->editStageObject(objId);
}

//===================================================================

void SkeletonTool::onDeactivate() {
  // m_selection.selectNone();
  // TSelection::setCurrent(0);
}

//-------------------------------------------------------------------

SkeletonSubtools::MagicLink SkeletonTool::getMagicLink(int index) const {
  assert(0 <= index && index < (int)m_magicLinks.size());
  return m_magicLinks[index];
}

//-------------------------------------------------------------------

void SkeletonTool::magicLink(int index) {
  if (index < 0 || index >= (int)m_magicLinks.size()) return;
  HookData h0             = m_magicLinks[index].m_h0;
  HookData h1             = m_magicLinks[index].m_h1;
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  int columnIndex         = app->getCurrentColumn()->getColumnIndex();
  TStageObjectId id       = TStageObjectId::ColumnId(columnIndex);
  TStageObject *obj       = xsh->getStageObject(id);

  int parentColumnIndex    = h1.m_columnIndex;
  TStageObjectId parentId  = TStageObjectId::ColumnId(parentColumnIndex);
  std::string parentHandle = h1.getHandle();

  std::string handle = "";
  if (h0.m_columnIndex < 0) {
    handle = obj->getHandle();
  } else {
    handle = h0.getHandle();
  }

  // TUndoManager *undoManager = TUndoManager::manager();
  // undoManager->beginBlock();
  TStageObjectCmd::setHandle(id, handle, app->getCurrentXsheet());
  TStageObjectCmd::setParent(id, parentId, parentHandle,
                             app->getCurrentXsheet());
  // undoManager->endBlock();
}

//-------------------------------------------------------------------

int SkeletonTool::getCursorId() const {
  switch (m_device) {
  case TD_None:
    if (m_mode.getValue() == BUILD_SKELETON)
      break;
    else
      return ToolCursor::RotCursor;
  case TD_Translation:
    return ToolCursor::MoveCursor;
  case TD_Rotation:
    return ToolCursor::RotCursor;
  default:
    return ToolCursor::StrokeSelectCursor;
  }
  return ToolCursor::StrokeSelectCursor;
}

//-------------------------------------------------------------------

void SkeletonTool::addContextMenuItems(QMenu *menu) {
  bool ikEnabled = m_mode.getValue() == INVERSE_KINEMATICS;

  if (ikEnabled) {
    Skeleton *skeleton = new Skeleton();
    buildSkeleton(
        *skeleton,
        TTool::getApplication()->getCurrentColumn()->getColumnIndex());
    if (skeleton->hasPinnedRanges() || skeleton->isIKEnabled()) {
      m_commandHandler->setSkeleton(skeleton);
      QAction *rp = menu->addAction(tr("Reset Pinned Center"));
      menu->addSeparator();
      bool ret = QObject::connect(rp, SIGNAL(triggered()), m_commandHandler,
                                  SLOT(clearPinnedRanges()));
      assert(ret);
    } else
      delete skeleton;
  }
}

//-------------------------------------------------------------------

void SkeletonTool::buildSkeleton(Skeleton &skeleton, int columnIndex) {
  int frame = TTool::getApplication()->getCurrentFrame()->getFrame();
  skeleton.build(getXsheet(), frame, columnIndex, m_temporaryPinnedColumns);
}
