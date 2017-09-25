

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"
#include "tproperty.h"
#include "tdata.h"
#include "tconvert.h"
#include "tgl.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "hookselection.h"
#include "tools/toolhandle.h"

#include "toonzqt/selection.h"

#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/hook.h"
#include "toonz/txshlevel.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzimageutils.h"
#include "toonz/dpiscale.h"
#include "toonz/onionskinmask.h"
#include "toonz/tonionskinmaskhandle.h"
#include <math.h>

// For Qt translation support
#include <QCoreApplication>

#include <QPainter>

using namespace ToolUtils;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// OtherHook
//-----------------------------------------------------------------------------

class OtherHook {
public:
  int m_columnIndex;
  int m_hookIndex;
  TPointD m_hookPos;
  OtherHook(int columnIndex, int hookIndex, const TPointD &hookPos)
      : m_columnIndex(columnIndex)
      , m_hookIndex(hookIndex)
      , m_hookPos(hookPos) {}
};

}  // namespace

//=============================================================================
// HookTool
//-----------------------------------------------------------------------------

class HookTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(HookTool)

  HookSelection m_selection;

  TPointD m_firstPos, m_lastPos;
  int m_hookId, m_hookSide;
  bool m_deselectArmed;
  HookUndo *m_undo;
  std::vector<OtherHook> m_otherHooks;

  TPropertyGroup m_prop;
  TBoolProperty m_snappedActive;

  TPointD m_snappedPos;
  std::string m_snappedReason;
  TRectD m_shapeBBox;
  bool m_snapped, m_hookSetChanged;

  bool m_buttonDown;
  TPointD m_pivotOffset;

  void getOtherHooks(std::vector<OtherHook> &otherHooks);

public:
  HookTool();

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  HookSet *getHookSet() const;

  void updateTranslation() override;

  void draw() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void onActivate() override;
  void onDeactivate() override;
  void onEnter() override;
  void onImageChanged() override {
    m_selection.selectNone();
    m_hookId = -1;
    m_otherHooks.clear();
    getOtherHooks(m_otherHooks);
    invalidate();
  }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override { return ToolCursor::MoveCursor; }

  void onSelectionChanged() { invalidate(); }
  bool select(const TSelection *) { return false; }

  bool pick(int &hookId, int &side, const TPointD &pos);

  bool snap(TPointD &pos, double &range2);

  bool isSnappedActive() const { return m_snappedActive.getValue(); }
  bool isEditingScene() const {
    return getApplication()->getCurrentFrame()->isEditingScene();
  }

  // pivot is the hook the level is attached to.
  // returns -1 if no pivot is defined.
  // Hook1 <==> index=0
  int getPivotIndex() {
    if (!isEditingScene())
      return -1;  // a pivot can be defined only when editing a scene
    std::string handle =
        getXsheet()->getStageObject(getObjectId())->getHandle();
    if (handle.find("H") != 0) return -1;
    return std::stoi(handle.substr(1)) - 1;
  }
  void drawHooks(HookSet *hookSet, const TFrameId &fid, bool isOnion);
  // other hooks are drawn in the current level reference frame
  // when the current matrix changes (e.g. because of a parent change or a
  // handle change)
  // we must recompute the hooks positions
  void updateMatrix() override {
    TTool::updateMatrix();
    m_otherHooks.clear();
    getOtherHooks(m_otherHooks);
  }

} hookTool;

//-----------------------------------------------------------------------------

HookTool::HookTool()
    : TTool("T_Hook")
    , m_hookId(-1)
    , m_hookSide(0)
    , m_deselectArmed(false)
    , m_undo(0)
    , m_snappedActive("Snap", true)  // W_ToolOptions_Snapped
    , m_snappedPos()
    , m_snappedReason("")
    , m_shapeBBox()
    , m_snapped(false)
    , m_hookSetChanged(false)
    , m_buttonDown(false)
    , m_pivotOffset() {
  bind(TTool::CommonLevels);
  m_prop.bind(m_snappedActive);

  m_snappedActive.setId("Snap");
}

//-----------------------------------------------------------------------------

void HookTool::updateTranslation() {
  m_snappedActive.setQStringName(tr("Snap"));
}

//-----------------------------------------------------------------------------

HookSet *HookTool::getHookSet() const {
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return 0;
  return xl->getHookSet();
}

//-----------------------------------------------------------------------------

//
// search for hooks located on other columns (they are used as a reference)
// the position of hooks is computed in the current level reference frame
//
void HookTool::getOtherHooks(std::vector<OtherHook> &otherHooks) {
  if (!getViewer()) return;
  TPointD dpi      = getViewer()->getDpiScale();
  TAffine myAffInv = (getCurrentColumnMatrix() * TScale(dpi.x, dpi.y)).inv();
  TXsheet *xsh     = getXsheet();
  int row          = getFrame();
  int curCol       = getColumnIndex();
  int i;
  for (i = 0; i < xsh->getColumnCount(); i++) {
    if (!xsh->getColumn(i) || !xsh->getColumn(i)->isCamstandVisible()) continue;
    if (i == curCol) continue;
    TXshCell cell = xsh->getCell(row, i);
    if (cell.isEmpty()) continue;
    TAffine aff =
        myAffInv * xsh->getPlacement(TStageObjectId::ColumnId(i), row);
    if (cell.getSimpleLevel())
      aff = aff * getDpiAffine(cell.getSimpleLevel(), cell.m_frameId, true);
    HookSet *hookSet = cell.m_level->getHookSet();
    int j;

    for (j = 0; j < hookSet->getHookCount(); j++) {
      Hook *hook = hookSet->getHook(j);
      if (hook == 0 || hook->isEmpty()) continue;
      TPointD aPos = aff * hook->getAPos(cell.m_frameId);
      TPointD bPos = aff * hook->getBPos(cell.m_frameId);
      otherHooks.push_back(OtherHook(i, j, aPos));
    }
  }
}

//-----------------------------------------------------------------------------

void HookTool::drawHooks(HookSet *hookSet, const TFrameId &fid, bool isOnion) {
  int pivotIndex = getPivotIndex();
  int i;
  for (i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    assert(hook);
    TPointD p0 = hook->getAPos(fid);
    TPointD p1 = hook->getBPos(fid);
    if (pivotIndex == i) {
      // translating the pivot doesn't change its actual position, but just the
      // m_pivotOffset
      // the actual pivot position is changed in the mouseUp event
      p0 += m_pivotOffset;
      p1 += m_pivotOffset;
    }
    bool linked = p0 == p1;
    drawHook(p0, linked ? NormalHook : PassHookA, m_selection.isSelected(i, 1),
             isOnion);
    if (!linked) drawHook(p1, PassHookB, m_selection.isSelected(i, 2), isOnion);
  }
}

//-----------------------------------------------------------------------------

void HookTool::draw() {
  // ToolUtils::drawRect(TRectD(10,10,110,110), TPixel32(255,200,200), 0xFFF0);

  // draw the current image bounding box
  const double v200 = 200.0 / 255.0;
  TImageP image     = getImage(false);
  if (!image) return;

  TToonzImageP ti  = image;
  TVectorImageP vi = image;
  if (ti) {
    TRectD bbox =
        ToonzImageUtils::convertRasterToWorld(convert(ti->getBBox()), ti);
    ToolUtils::drawRect(bbox * ti->getSubsampling(), TPixel32(200, 200, 200),
                        0x5555);
  }
  if (vi) {
    TRectD bbox = vi->getBBox();
    ToolUtils::drawRect(bbox, TPixel32(200, 200, 200), 0x5555);
  }

  // draw hooks
  HookSet *hookSet = getHookSet();
  if (!hookSet) return;

  TTool::Application *app = TTool::getApplication();
  TFrameHandle *fh        = app->getCurrentFrame();
  TFrameId fid            = getCurrentFid();
  TXshSimpleLevel *level  = 0;

  OnionSkinMask osm = app->getCurrentOnionSkin()->getOnionSkinMask();
  std::vector<int> os;

  if (isEditingScene())
    osm.getAll(getFrame(), os);
  else {
    level = app->getCurrentLevel()->getSimpleLevel();
    assert(level);
    osm.getAll(level->guessIndex(fid), os);
  }

  int i;
  if (osm.isEnabled())
    for (i = 0; i < (int)os.size(); i++) {
      if (isEditingScene()) {
        const TXshCell &cell = getXsheet()->getCell(
            os[i], app->getCurrentColumn()->getColumnIndex());
        drawHooks(hookSet, cell.getFrameId(), true);
      } else {
        const TFrameId &fid2 = level->index2fid(os[i]);
        drawHooks(hookSet, fid2, true);
      }
    }

  drawHooks(hookSet, fid, false);

  // TXshCell cell = xsh->getCell(row, i);
  // draw other level hooks
  if (isSnappedActive() && isEditingScene()) {
    for (i = 0; i < (int)m_otherHooks.size(); i++) {
      drawHook(m_otherHooks[i].m_hookPos, OtherLevelHook, false);
    }
  }

  // draw hooks balloons
  std::vector<TRectD> balloons;  // this is used to avoid balloons overlapping
  int pivotIndex = getPivotIndex();
  for (i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    assert(hook);
    TPointD p0  = hook->getAPos(fid);
    TPointD p1  = hook->getBPos(fid);
    bool linked = p0 == p1;
    if (i == pivotIndex) {
      p0 += m_pivotOffset;
      p1 += m_pivotOffset;
    }
    TPixel32 balloonColor(200, 220, 205, 200);
    TPoint balloonOffset(20, 20);
    std::string hookName = std::to_string(i + 1);
    drawBalloon(p0, hookName, balloonColor, balloonOffset, getPixelSize(), false, &balloons);
    if (!linked)
      drawBalloon(p1, hookName, balloonColor, balloonOffset, getPixelSize(), false, &balloons);
  }
  // draw snapped hook balloon
  if (m_snappedReason != "") {
    TPointD pos = m_snappedPos;
    TRectD bbox = m_shapeBBox;
    if (bbox.getLx() > 0 && bbox.getLy() > 0) {
      glColor3d(v200, v200, v200);
      glEnable(GL_LINE_STIPPLE);
      glLineStipple(5, 0xAAAA);
      tglDrawRect(bbox);
      glDisable(GL_LINE_STIPPLE);

      glBegin(GL_LINES);
      glVertex2d(pos.x, bbox.y0);
      glVertex2d(pos.x, bbox.y1);
      glVertex2d(bbox.x0, pos.y);
      glVertex2d(bbox.x1, pos.y);
      glEnd();
      glDisable(GL_LINE_STIPPLE);
    }
    drawBalloon(pos, m_snappedReason, TPixel32(200, 250, 180, 200),
                TPoint(20, 20), getPixelSize(), false, &balloons);
  }
}

//-----------------------------------------------------------------------------

// side : 1=A,2=B,3=A&B
bool HookTool::pick(int &hookId, int &side, const TPointD &pos) {
  HookSet *hookSet = getHookSet();
  if (!hookSet) return false;

  TFrameId fid    = getCurrentFid();
  double minDist2 = 1e8;
  for (int i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    TPointD aPos = hook->getAPos(fid);
    TPointD bPos = hook->getBPos(fid);
    bool linked  = aPos == bPos;
    if (linked) {
      double dist2 = tdistance2(pos, aPos);
      if (dist2 < minDist2) {
        minDist2 = dist2;
        hookId   = hook->getId();
        side     = 3;
      }
    } else {
      double aDist2 = tdistance2(pos, aPos);
      double bDist2 = tdistance2(pos, bPos);
      double dist2  = aDist2;
      int s         = 1;
      if (bDist2 < dist2) {
        dist2 = bDist2;
        s     = 2;
      }
      if (dist2 < minDist2) {
        minDist2 = dist2;
        hookId   = hook->getId();
        side     = s;
      }
    }
  }
  double pixelSize = getPixelSize();
  return minDist2 < 100 * pixelSize * pixelSize;
}

//-----------------------------------------------------------------------------

void HookTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  m_buttonDown = true;
  m_snapped    = false;

  TXshLevel *xl                          = app->getCurrentLevel()->getLevel();
  if (xl && xl->getSimpleLevel()) m_undo = new HookUndo(xl->getSimpleLevel());
  m_selection.setLevel(xl);
  m_firstPos = m_lastPos = pos;
  m_hookId = -1, m_hookSide = 0;
  m_deselectArmed = false;
  if (pick(m_hookId, m_hookSide, pos)) {
    if (m_hookSide == 3)  // ho cliccato su un cerchio-croce
    {
      if (e.isAltPressed())  // con l'alt voglio dividere
      {
        m_selection.selectNone();
        m_selection.select(m_hookId, 2);
      } else if (e.isCtrlPressed()) {
        // se sono tutti e due selezionati li deseleziono
        if (m_selection.isSelected(m_hookId, 1) &&
            m_selection.isSelected(m_hookId, 2)) {
          m_selection.unselect(m_hookId, 1);
          m_selection.unselect(m_hookId, 2);
        } else  // altrimenti li seleziono tutti e due
        {
          m_selection.select(m_hookId, 1);
          m_selection.select(m_hookId, 2);
        }
      } else {
        // senza control ne' shift ne' alt: se ho cliccato su un hook non
        // selezionato
        // deseleziono tutti gli altri e lo seleziono, altrimenti lascio tutto
        // com'e' e al buttonup se non c'e' stato movimento deseleziono gli
        // altri
        if (m_selection.isSelected(m_hookId, 1) ||
            m_selection.isSelected(m_hookId, 2))
          m_deselectArmed = true;
        else {
          m_selection.selectNone();
          m_selection.select(m_hookId, 1);
          m_selection.select(m_hookId, 2);
        }
      }
    } else  // ho cliccato o su un cerchio o su una croce
    {
      if (e.isCtrlPressed())
        m_selection.invertSelection(m_hookId, m_hookSide);
      else {
        m_selection.selectNone();
        m_selection.select(m_hookId, m_hookSide);
      }
    }
  } else {
    // non ho cliccato su nulla: con ctrl non faccio nulla, senza creo un nuovo
    // hook
    if (!e.isCtrlPressed()) {
      m_selection.selectNone();
      TFrameId fid     = getCurrentFid();
      HookSet *hookSet = getHookSet();
      if (hookSet && xl->getSimpleLevel() &&
          !xl->getSimpleLevel()->isReadOnly()) {
        Hook *hook       = hookSet->addHook();
        m_hookSetChanged = true;
        if (hook) {
          TPointD ppos(pos);
          if (m_snappedReason != "") {
            m_snapped = true;
            ppos      = m_snappedPos;
          }
          m_snappedReason = "";
          hook->setAPos(fid, ppos);
          ppos = hook->getAPos(fid);
          m_selection.select(hook->getId(), 1);
          m_selection.select(hook->getId(), 2);
        }
      }
    }
  }
  m_pivotOffset = TPointD();
  m_selection.makeCurrent();
  invalidate();
}

//-----------------------------------------------------------------------------

void HookTool::leftButtonDrag(const TPointD &pp, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (m_snapped) return;

  TFrameId fid = getCurrentFid();

  // cerco di capire se sto muovendo un unico hook "unito"
  TPointD hookPos;
  HookSet *hookSet = getHookSet();
  if (!hookSet) return;

  bool draggingPivot = false;
  int pivotIndex     = getPivotIndex();
  int i, count = 0;
  for (i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    TPointD aPos = hook->getAPos(fid);
    TPointD bPos = hook->getBPos(fid);
    if (m_selection.isSelected(i, 1) && m_selection.isSelected(i, 2) &&
        aPos == bPos) {
      count++;
      hookPos = aPos;
    } else if (m_selection.isSelected(i, 1) || m_selection.isSelected(i, 2)) {
      count = 0;
      break;
    }
    if (i == pivotIndex && m_selection.isSelected(i, 1)) draggingPivot = true;
  }

  TPointD pos(pp);
  TPointD delta = pos - m_lastPos;
  if (e.isShiftPressed()) {
    // se e' pigiato lo shift non snappa e si muove solo in
    // orizzontale/verticale
    TPointD d = pos - m_firstPos;
    if (d.x * d.x > d.y * d.y)
      pos.y = m_firstPos.y;
    else
      pos.x = m_firstPos.x;
    delta   = pos - m_lastPos;
  } else if (count == 1 && isSnappedActive() && !draggingPivot) {
    // snappa
    TPointD oldHookPos = hookPos;
    hookPos += pos - m_lastPos;
    double range2 = getPixelSize() * 20;
    range2 *= range2;
    m_snappedReason = "";
    m_shapeBBox     = TRectD();
    if (snap(hookPos, range2)) {
      delta = hookPos - oldHookPos;
      pos   = delta + m_lastPos;
    }
  } else if (count > 1 && isSnappedActive() && !draggingPivot) {
    TPointD oldHookPos = hookPos;
    // delta = pos - m_lastPos;
    TPointD snappedDelta = delta;
    double range2        = getPixelSize() * 20;
    range2 *= range2;
    m_snappedReason = "";
    m_shapeBBox     = TRectD();

    for (i = 0; i < hookSet->getHookCount(); i++) {
      Hook *hook = hookSet->getHook(i);
      if (hook && !hook->isEmpty()) {
        if (m_selection.isSelected(i, 1)) {
          TPointD p0 = hook->getAPos(fid);
          TPointD p1 = p0 + delta;
          if (snap(p1, range2)) snappedDelta = p1 - p0;
        }
        if (m_selection.isSelected(i, 1)) {
          TPointD p0 = hook->getBPos(fid);
          TPointD p1 = p0 + delta;
          if (snap(p1, range2)) snappedDelta = p1 - p0;
        }
      }
    }

    pos   = m_lastPos + snappedDelta;
    delta = snappedDelta;
  }

  m_lastPos = pos;

  TXsheet *xsh = getXsheet();
  //  std::string handle =
  //  getXsheet()->getStageObject(TStageObjectId::ColumnId())->getHandle();

  // actual movement
  for (i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;

    TPointD aPos   = hook->getAPos(fid);
    TPointD bPos   = hook->getBPos(fid);
    bool aSelected = m_selection.isSelected(i, 1);
    bool bSelected = m_selection.isSelected(i, 2);
    if (pivotIndex == i) {
      if (aSelected) {
        m_pivotOffset += delta;
        if (!bSelected) hook->setBPos(fid, bPos - delta);
      } else if (bSelected)
        hook->setBPos(fid, bPos + delta);
    } else {
      if (aSelected) hook->setAPos(fid, aPos + delta);
      if (bSelected) hook->setBPos(fid, bPos + delta);
    }
    m_hookSetChanged = true;
  }
  getXsheet()->getStageObjectTree()->invalidateAll();
  invalidate();
}

//-----------------------------------------------------------------------------

void HookTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  // note: apparently sometimes (when the user triple-clicks) we receive this
  // event twice
  if (!m_buttonDown) return;
  m_buttonDown = false;

  // if I've moved the pivot hook I have to perform the actual movement in the
  // mouse up event
  int pivotIndex = getPivotIndex();
  if (m_selection.isSelected(pivotIndex, 1) && m_pivotOffset != TPointD()) {
    HookSet *hookSet = getHookSet();
    if (hookSet) {
      Hook *hook = hookSet->getHook(pivotIndex);
      if (hook && !hook->isEmpty()) {
        TFrameId fid = getCurrentFid();
        TPointD aPos = hook->getAPos(fid);
        TPointD bPos = hook->getBPos(fid);
        hook->setAPos(fid, aPos + m_pivotOffset);
        hook->setBPos(fid, bPos + m_pivotOffset);
        getXsheet()->getStageObjectTree()->invalidateAll();
        updateMatrix();
      }
    }
  }

  m_snapped = false;
  // m_selection.selectNone();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  if (!xl || !xl->getSimpleLevel()) return;
  xl->getSimpleLevel()->getProperties()->setDirtyFlag(true);
  TPointD delta = m_lastPos - m_firstPos;
  if (m_deselectArmed && norm2(delta) < 10) {
    m_selection.selectNone();
    m_selection.unselect(m_hookId, 1);
    m_selection.unselect(m_hookId, 2);
  }
  m_deselectArmed = false;
  if (m_undo && m_hookSetChanged)
    TUndoManager::manager()->add(m_undo);
  else {
    delete m_undo;
    m_undo = 0;
  }
  m_hookSetChanged = false;
  m_pivotOffset    = TPointD();
}

//-----------------------------------------------------------------------------

bool HookTool::snap(TPointD &pos, double &range2) {
  bool ret               = false;
  TPointD snappedPos     = pos;
  TVectorImageP vi       = TImageP(getImage(false));
  TStroke *selectedShape = 0;
  TRectD selectedShapeBBox;
  double selectedShapeBBoxArea = 0;
  int i, n;
  n = vi ? (int)vi->getStrokeCount() : 0;
  for (i = 0; i < n; i++) {
    TStroke *stroke = vi->getStroke(i);
    if (!stroke->isSelfLoop()) continue;
    TRectD bbox          = stroke->getBBox();
    TPointD strokeCenter = 0.5 * (bbox.getP00() + bbox.getP11());
    if (bbox.contains(pos)) {
      double dist2 = norm2(pos - strokeCenter);
      if (dist2 < range2) {
        double bboxArea = bbox.getLx() * bbox.getLy();
        if (selectedShape == 0 || selectedShapeBBoxArea > bboxArea) {
          range2                = dist2;
          selectedShape         = stroke;
          selectedShapeBBox     = bbox;
          selectedShapeBBoxArea = bboxArea;
        }
      }
    }
  }

  if (selectedShape) {
    m_shapeBBox = selectedShapeBBox;
    snappedPos =
        0.5 * (selectedShapeBBox.getP00() + selectedShapeBBox.getP11());
    m_snappedPos    = snappedPos;
    m_snappedReason = "shape center";
    ret             = true;
  }

  int k = -1;
  if (isEditingScene()) {
    for (i = 0; i < (int)m_otherHooks.size(); i++) {
      double dist2 = norm2(pos - m_otherHooks[i].m_hookPos);
      if (dist2 < range2) {
        range2 = dist2;
        k      = i;
      }
    }
  }
  if (k >= 0) {
    m_shapeBBox     = TRectD();
    snappedPos      = m_otherHooks[k].m_hookPos;
    m_snappedPos    = snappedPos;
    m_snappedReason = "Col" +
                      std::to_string(m_otherHooks[k].m_columnIndex + 1) + "/" +
                      std::to_string(m_otherHooks[k].m_hookIndex + 1);
    ret = true;
  }
  pos = snappedPos;
  return ret;
}

//-----------------------------------------------------------------------------

void HookTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  std::string oldReason = m_snappedReason;
  TPointD oldPos        = m_snappedPos;

  m_snappedPos    = TPointD();
  m_snappedReason = "";
  m_shapeBBox     = TRectD();

  m_otherHooks.clear();
  getOtherHooks(m_otherHooks);

  int hookId, side;
  if (pick(hookId, side, pos)) {
    if (oldReason != "") invalidate();
    return;
  }

  if (isSnappedActive()) {
    double range2 = getPixelSize() * 20.0;
    range2 *= range2;
    TPointD snappedPos = pos;
    snap(snappedPos, range2);
  }

  if (m_snappedReason != oldReason || m_snappedPos != oldPos) invalidate();
}

//-----------------------------------------------------------------------------

void HookTool::onActivate() {
  // TODO: getApplication()->editImageOrSpline();
  m_otherHooks.clear();
  getOtherHooks(m_otherHooks);
  m_selection.makeCurrent();
}

//-----------------------------------------------------------------------------

void HookTool::onDeactivate() {
  m_selection.selectNone();
  TSelection::setCurrent(0);
}

//-----------------------------------------------------------------------------

void HookTool::onEnter() { m_selection.makeCurrent(); }
