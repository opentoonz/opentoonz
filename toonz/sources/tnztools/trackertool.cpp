

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tools/cursors.h"

#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/selectioncommandids.h"

#include "toonzqt/selection.h"
#include "tproperty.h"
#include "tdata.h"
#include "tconvert.h"
#include "tgl.h"
#include "tstroke.h"
#include "tvectorimage.h"

#include "toonz/hook.h"
#include "toonz/txshlevel.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/txsheethandle.h"

#include <QMessageBox>
#include <math.h>

// For Qt translation support
#include <QCoreApplication>
#include <QKeyEvent>

using namespace ToolUtils;

class TrackerTool;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------
//=============================================================================
// TrackerRegionSelection
//-----------------------------------------------------------------------------
// Ancora da definire.
// under construction
class TrackerRegionSelection final : public TSelection {
  TXshLevelP m_level;
  std::set<std::pair<int, int>>
      m_objtp;  // objtp: 1=ObjectId 2=Tracker Region Index
  TrackerTool *m_tool;

public:
  TrackerRegionSelection() : m_tool(0) {}

  void setTool(TrackerTool *tool) { m_tool = tool; }

  TSelection *clone() const { return new TrackerRegionSelection(*this); }
  void setLevel(const TXshLevelP &level) { m_level = level; }

  void select(int objectId, int trackerRegionIndex) {
    m_objtp.insert(std::make_pair(objectId, trackerRegionIndex));
  }
  void deselect(int objectId, int trackerRegionIndex) {
    m_objtp.erase(std::make_pair(objectId, trackerRegionIndex));
  }

  bool isSelected(int objectId, int trackerRegionIndex) const {
    return m_objtp.count(std::make_pair(objectId, trackerRegionIndex)) > 0;
  }
  void invertSelection(int objectId, int trackerRegionIndex) {
    if (isSelected(objectId, trackerRegionIndex))
      deselect(objectId, trackerRegionIndex);
    else
      select(objectId, trackerRegionIndex);
  }

  bool isEmpty() const override { return m_objtp.empty(); }

  void selectNone() override { m_objtp.clear(); }

  HookSet *getHookSet() const {
    TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
    // TXshLevel *xl = m_level.getPointer();
    if (!xl) return 0;
    return xl->getHookSet();
  }

  TDataP getData() { return TDataP(); }
  TDataP cutData() { return TDataP(); }

  TDataP clearData() {
    /*
//HookData *data = new HookData();
HookSet *hookSet = getHookSet();
    TFrameId fid = TTool::getApplication()->getCurrentFrame()->getFid();
if(!hookSet) return TDataP();

for(int i=0;i<hookSet->getHookCount();i++)
{
Hook *hook = hookSet->getHook(i);
if(!hook || hook->isEmpty()) continue;
if(isSelected(i,1) && isSelected(i,2))
hookSet->clearHook(hook);
else if(isSelected(i,2))
hook->setBPos(fid, hook->getAPos(fid));
else if(isSelected(i,1))
hook->setAPos(fid, hook->getBPos(fid));
}
    makeCurrent();
return TDataP(); */
  }

  TDataP pasteData(const TDataP &data) { return TDataP(); }

  void resetData(const TDataP &data, bool insert) {}

  bool select(const TSelection *s) {
    if (const TrackerRegionSelection *hs =
            dynamic_cast<const TrackerRegionSelection *>(s)) {
      m_level = hs->m_level;
      m_objtp = hs->m_objtp;
      return true;
    } else
      return false;
  }

  void enableCommands() override;
  void convertToRegion();
};

}  // namespace

//=============================================================================
//  TrackerTool class declaration
//-----------------------------------------------------------------------------

class TrackerTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(TrackerTool)

  TrackerRegionSelection m_selection;
  TPointD m_firstPos, m_lastPos;

  int m_hookSelectedIndex;
  int m_lastHookSelectedIndex;
  bool m_deselectArmed;
  bool m_newObjectAdded;  // serve al buttonUp per sapere se l'ultima tracker
                          // region
                          // aggiunta è un oggetto nuovo oppure no

  TPropertyGroup m_prop;
  TDoubleProperty m_toolSizeWidth;
  TDoubleProperty m_toolSizeHeight;
  TIntProperty m_toolPosX;
  TIntProperty m_toolPosY;
  TRectD m_shapeBBox;

  bool m_buttonDown;
  bool m_dragged;
  bool m_picked;
  TPointD m_pos;  // posizione del mouse
  TPointD m_oldPos;

  int m_what;
  enum {
    Outside,
    Inside,
    P00,
    P01,
    P10,
    P11,
    P1M,
    PM1,
    P0M,
    PM0,
    ADD_OBJECT,
    NormalHook
  };

public:
  TrackerTool();

  ToolType getToolType() const override { return TTool::LevelReadTool; }

  void updateTranslation() override;

  TrackerObjectsSet *getTrackerObjectsSet() const;
  HookSet *getHookSet() const;
  void draw() override;

  void deleteSelectedTrackerRegion();

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  bool keyDown(QKeyEvent *event) override;
  // bool moveCursor(const TPointD &pos){}
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;
  void onDeactivate() override;

  void onImageChanged() override {}
  void reset() override;
  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void onSelectionChanged() { invalidate(); }
  bool onPropertyChanged(std::string propertyName) override;
  bool select(const TSelection *) { return false; }

  bool pick(int &hookIndex, const TPointD &pos);

  int getCursorId() const override;

  // returns true if the pressed key is recognized and processed.
  bool isEventAcceptable(QEvent *e) override;

} trackerTool;

//=============================================================================
//  TrackerTool implementation
//-----------------------------------------------------------------------------

TrackerTool::TrackerTool()
    : TTool("T_Tracker")
    , m_hookSelectedIndex(-1)
    , m_lastHookSelectedIndex(-1)
    , m_deselectArmed(false)
    , m_toolSizeWidth("Width:", 0, 1000, 10)    // W_ToolOptions
    , m_toolSizeHeight("Height:", 0, 1000, 10)  // W_ToolOptions
    , m_toolPosX("X:", -9000, 9000, 10)         // W_ToolOptions
    , m_toolPosY("Y:", -9000, 9000, 10)         // W_ToolOptions
    , m_shapeBBox()
    , m_buttonDown(false)
    , m_dragged(false)
    , m_oldPos(TPointD(0, 0))
    , m_newObjectAdded(false) {
  bind(TTool::CommonLevels);
  m_prop.bind(m_toolSizeWidth);
  m_prop.bind(m_toolSizeHeight);
  m_prop.bind(m_toolPosX);
  m_prop.bind(m_toolPosY);
  m_selection.setTool(this);
}

//-----------------------------------------------------------------------------

void TrackerTool::updateTranslation() {
  m_toolSizeWidth.setQStringName(tr("Width:"));
  m_toolSizeHeight.setQStringName(tr("Height:"));
  m_toolPosX.setQStringName(tr("X:"));
  m_toolPosY.setQStringName(tr("Y:"));
}

//-----------------------------------------------------------------------------

TrackerObjectsSet *TrackerTool::getTrackerObjectsSet() const {
  HookSet *hookSet = getHookSet();
  if (!hookSet) return 0;
  return hookSet->getTrackerObjectsSet();
}
HookSet *TrackerTool::getHookSet() const {
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return 0;
  return xl->getHookSet();
}

//-----------------------------------------------------------------------------

void TrackerTool::draw() {
  HookSet *hookSet = getHookSet();
  if (!hookSet) return;
  if (hookSet->getHookCount() <= m_hookSelectedIndex) m_hookSelectedIndex = -1;

  TFrameId fid = getCurrentFid();
  int selectedObjectId;
  if (m_hookSelectedIndex >= 0 && hookSet->getHook(m_hookSelectedIndex))
    selectedObjectId =
        hookSet->getHook(m_hookSelectedIndex)->getTrackerObjectId();
  else
    selectedObjectId = -1;
  int i              = 0;
  double pixelSize   = getPixelSize();

  std::vector<TRectD> balloons;  // this is used to avoid balloons overlapping
  // draw hooks
  for (i = 0; i < hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    assert(hook);
    // Se l'Hook ha una TrackerRegion allora la disegno
    if (hook->getTrackerObjectId() >= 0) {
      TRectD rect;
      rect = hook->getTrackerRegion(fid);
      TPixel32 textColor(127, 127, 127);
      TPixel32 trackerObjectColor(0, 0, 0);

      if (hook->getTrackerObjectId() == selectedObjectId) {
        if (m_hookSelectedIndex == i) {
          TPixel32 frameColor(127, 127, 127);
          drawSquare(0.5 * (rect.getP01() + rect.getP11()), pixelSize * 4,
                     frameColor);  // scalaY
          drawSquare(0.5 * (rect.getP11() + rect.getP10()), pixelSize * 4,
                     frameColor);                                // scalaX
          drawSquare(rect.getP00(), pixelSize * 4, frameColor);  // scala
          drawSquare(rect.getP10(), pixelSize * 4, frameColor);  // ridimensiona
          trackerObjectColor = TPixel32(183, 227, 0);
          textColor          = TPixel32(155, 213, 219);
        } else {
          textColor          = TPixel32(183, 227, 0);
          trackerObjectColor = TPixel32(155, 213, 219);
        }
      } else
        trackerObjectColor = TPixel32(0, 0, 0);

      tglColor(trackerObjectColor);
      tglDrawRect(rect);
      tglColor(textColor);
      glPushMatrix();
      glTranslated(hook->getPos(fid).x, hook->getPos(fid).y, 0);
      glScaled(pixelSize, pixelSize, 1);
      int objectId     = hook->getTrackerObjectId();
      char *objectChar = (char *)malloc(2);
      objectChar[0]    = (char)(objectId + 65);
      objectChar[1]    = '\0';
      std::string text(objectChar);
      tglDrawText(TPointD(-15, 10), text);
      glPopMatrix();
    }

    TPointD p0  = hook->getAPos(fid);
    TPointD p1  = hook->getBPos(fid);
    bool linked = p0 == p1;
    drawHook(p0, linked ? ToolUtils::NormalHook : ToolUtils::PassHookA,
             m_hookSelectedIndex == i);
    std::string hookName = std::to_string(i + 1);
    TPixel32 balloonColor(200, 220, 205, 200);
    TPoint balloonOffset(20, 20);
    drawBalloon(p0, hookName, balloonColor, balloonOffset, false, &balloons);
    if (!linked) {
      drawHook(p1, PassHookB, m_selection.isSelected(i, 2));
      drawBalloon(p1, hookName, balloonColor, balloonOffset, false, &balloons);
    }
  }
}

//-----------------------------------------------------------------------------

bool TrackerTool::pick(int &hookIndex, const TPointD &pos) {
  double minDistance = -1;
  m_what             = Outside;

  HookSet *hookSet = getHookSet();
  if (!hookSet) return false;
  TFrameId fid = getCurrentFid();

  double pixelSize = getPixelSize();
  int i            = 0;
  for (i = 0; i < (int)hookSet->getHookCount(); i++) {
    Hook *hook = hookSet->getHook(i);
    if (!hook || hook->isEmpty()) continue;
    int trackerObjectId = hook->getTrackerObjectId();
    if (trackerObjectId < 0)  // se non è una trackeRregion
    {
      TPointD hookPos = hook->getPos(fid);
      TRectD overArea =
          TRectD(hookPos.x - 20 * pixelSize, hookPos.y - 20 * pixelSize,
                 hookPos.x + 20 * pixelSize, hookPos.y + 20 * pixelSize);
      if (overArea.contains(pos)) {
        hookIndex = i;  // setto l'indice dell'hook
        m_what    = NormalHook;
        return true;
      }
    } else {
      /*
      TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
      if(!trackerObjectsSet) return false;
      TrackerObject *trackerObject = trackerObjectsSet->getObjectFromIndex(i);
      int j=0;
      for(j=0;j<(int)trackerObject->getHooksCount();j++)
      {*/
      TPointD centerPos = hook->getPos(fid);
      double width      = hook->getTrackerRegionWidth();
      double height     = hook->getTrackerRegionHeight();
      double distance   = tdistance2(centerPos, pos);
      TPointD localPos  = pos - centerPos;
      TRectD rect       = hook->getTrackerRegion(fid);
      TRectD overArea   = TRectD(
          rect.getP00().x - 4 * pixelSize, rect.getP00().y - 4 * pixelSize,
          rect.getP11().x + 4 * pixelSize, rect.getP11().y + 4 * pixelSize);
      // se pos è all'interno del'area sensibile del rettangolo
      if (overArea.contains(pos)) {
        if (distance < minDistance || minDistance == -1) {
          minDistance = distance;
          hookIndex   = i;

          m_what = Inside;

          // scale Y Area
          double x =
              0.5 * (rect.getP01().x + rect.getP11().x);  // ascissa punto medio
          TPointD my = TPointD(x, rect.getP11().y);
          TRectD scaleYArea =
              TRectD(my.x - 4 * pixelSize, my.y - 4 * pixelSize,
                     my.x + 4 * pixelSize, my.y + 4 * pixelSize);
          if (scaleYArea.contains(pos)) m_what = PM1;
          // scale X Area
          double y = 0.5 * (rect.getP11().y +
                            rect.getP10().y);  // ordinata punto medio
          TPointD mx = TPointD(rect.getP10().x, y);
          TRectD scaleXArea =
              TRectD(mx.x - 4 * pixelSize, mx.y - 4 * pixelSize,
                     mx.x + 4 * pixelSize, mx.y + 4 * pixelSize);
          if (scaleXArea.contains(pos)) m_what = P1M;
          // resize area (scale X and Y)
          TRectD resizeArea = TRectD(
              rect.getP10().x - 4 * pixelSize, rect.getP10().y - 4 * pixelSize,
              rect.getP10().x + 4 * pixelSize, rect.getP10().y + 4 * pixelSize);
          if (resizeArea.contains(pos)) m_what = P10;
          // scale area

          TRectD scaleArea = TRectD(
              rect.getP00().x - 4 * pixelSize, rect.getP00().y - 4 * pixelSize,
              rect.getP00().x + 4 * pixelSize, rect.getP00().y + 4 * pixelSize);
          if (scaleArea.contains(pos)) m_what = P00;
        }
      }
      //}
    }
  }
  return (minDistance != -1);
}

//-----------------------------------------------------------------------------

void TrackerTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_buttonDown  = true;
  m_picked      = true;
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return;
  m_selection.setLevel(xl);
  m_firstPos = m_lastPos = pos;

  m_deselectArmed = false;

  m_oldPos         = pos;
  double pixelSize = getPixelSize();

  HookSet *hookSet = xl->getHookSet();
  if (!hookSet) return;
  TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
  TFrameId fid                         = getCurrentFid();
  if (!trackerObjectsSet) return;
  if (pick(m_hookSelectedIndex, pos)) {
    if (m_what == NormalHook) {
      invalidate();
      return;
    }
    Hook *hook = hookSet->getHook(m_hookSelectedIndex);
    if (!hook) return;
    m_selection.selectNone();
    m_selection.select(m_hookSelectedIndex, m_hookSelectedIndex);
    m_toolSizeWidth.setValue(hook->getTrackerRegionWidth());
    m_toolSizeHeight.setValue(hook->getTrackerRegionHeight());
    m_toolPosX.setValue(hook->getPos(fid).x);
    m_toolPosY.setValue(hook->getPos(fid).y);
    m_toolSizeWidth.notifyListeners();
    m_toolSizeHeight.notifyListeners();
    m_toolPosX.notifyListeners();
    m_toolPosY.notifyListeners();
  } else {
    m_selection.selectNone();

    if (xl->getSimpleLevel() && !xl->getSimpleLevel()->isReadOnly()) {
      TFrameId fid = getCurrentFid();
      m_what       = P10;

      // Se non è selezionato alcun oggetto allora aggiungo un nuovo oggetto e a
      // questo
      // aggiungo una trackerRegion
      int trackerObjectId;
      if (m_hookSelectedIndex == -1 ||
          hookSet->getHook(m_hookSelectedIndex) == 0) {
        trackerObjectId  = trackerObjectsSet->addObject();
        m_newObjectAdded = true;
      } else
        trackerObjectId =
            hookSet->getHook(m_hookSelectedIndex)->getTrackerObjectId();
      // se l'oggetto selezionato è un semplice Hook (senza region)
      // allora creo un nuovo hook con region
      if (trackerObjectId == -1) {
        trackerObjectId  = trackerObjectsSet->addObject();
        m_newObjectAdded = true;
      }

      // aggiungo un Hook all'oggetto selezionato
      Hook *newHook = hookSet->addHook();
      if (newHook) {
        newHook->setTrackerObjectId(trackerObjectId);
        newHook->setAPos(fid, pos);
        newHook->setTrackerRegionHeight(pixelSize * 10);
        newHook->setTrackerRegionWidth(pixelSize * 10);
        // setto l'indice della trackerRegion corrente
        m_hookSelectedIndex = newHook->getId();  // hookSet->getHookCount()-1;
      } else {
        if (hookSet->getHookCount() >= 20)
          QMessageBox::warning(0, "TrackerTool Error",
                               "Hooks number must be at most 20");
        m_hookSelectedIndex = -1;
      }
    }
  }
  m_selection.makeCurrent();
  invalidate();
  return;
}

//-----------------------------------------------------------------------------

void TrackerTool::leftButtonDrag(const TPointD &pp, const TMouseEvent &e) {
  if (!m_buttonDown) return;
  if (m_hookSelectedIndex < 0 && m_what != NormalHook) return;
  HookSet *hookSet = getHookSet();
  if (!hookSet) return;
  assert(hookSet->getHook(m_hookSelectedIndex) != 0);
  TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
  TFrameId fid                         = getCurrentFid();
  if (!trackerObjectsSet) return;

  if (m_dragged == false) {
    m_dragged = true;
  }

  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (xl->getSimpleLevel() && xl->getSimpleLevel()->isReadOnly() &&
      m_what != Inside && m_what != NormalHook)
    return;

  if (m_dragged == true) {
    Hook *hook = new Hook();
    hook       = getHookSet()->getHook(m_hookSelectedIndex);
    if (!hook || hook->isEmpty()) return;
    TPointD deltaPos = pp - m_oldPos;
    m_oldPos         = pp;
    double newWidth  = hook->getTrackerRegionWidth();
    double newHeight = hook->getTrackerRegionHeight();
    TAffine aff;
    const double epsilon = 1e-2;
    TPointD posCenter    = hook->getPos(fid);
    double a             = norm2(pp - posCenter);
    double b             = norm2(pp - deltaPos - posCenter);
    switch (m_what) {
    case Inside:  // Traslazione
      hook->setAPos(fid, hook->getPos(fid) + deltaPos);
      break;
    case NormalHook:  // Traslazione Hook
    {
      hook->setAPos(fid, hook->getPos(fid) + deltaPos);
      invalidate();
      return;
    }
    case P00:  // Scalatura
    {
      if (a <= epsilon || b <= epsilon) return;
      aff         = TScale(posCenter, sqrt(a / b));
      TRectD rect = hook->getTrackerRegion(fid);
      TPointD av(hook->getTrackerRegion(fid).getP00());
      TPointD av2(hook->getTrackerRegion(fid).getP11());
      av        = aff * av;
      av2       = aff * av2;
      rect      = TRectD(av, av2);
      newWidth  = rect.getLx();
      newHeight = rect.getLy();
      break;
    }
    case P10:  // Scalatura X e Y
    {
      TPointD pos                = hook->getPos(fid);
      TPointD diffPos            = pp - pos;
      int signumx                = 1;
      int signumy                = 1;
      if (diffPos.x < 0) signumx = -1;
      if (diffPos.y < 0) signumy = -1;
      newWidth = fabs(hook->getTrackerRegionWidth() + 2 * signumx * deltaPos.x);
      newHeight =
          fabs(hook->getTrackerRegionHeight() + 2 * signumy * deltaPos.y);
      // double newWidth = fabs(2*diffPos.x-fabs(2*signumx*deltaPos.x));
      // double newHeight = fabs(2*diffPos.y-fabs(2*signumy*deltaPos.y));
      break;
    }
    case P1M:  // Ridimensiono X
    {
      TRectD rect = hook->getTrackerRegion(fid);
      rect        = rect.enlarge(deltaPos.x, 0);
      newWidth    = rect.getLx();
      break;
    }
    case PM1:  // Ridimensiono Y
    {
      TRectD rect = hook->getTrackerRegion(fid);
      rect        = rect.enlarge(0, deltaPos.y);
      newHeight   = rect.getLy();
      break;
    }
    default:
      break;
    }
    if (newWidth > m_toolSizeWidth.getRange().second ||
        newHeight > m_toolSizeHeight.getRange().second)
      return;
    hook->setTrackerRegionWidth(newWidth);
    hook->setTrackerRegionHeight(newHeight);
    m_toolSizeWidth.setValue(hook->getTrackerRegionWidth());
    m_toolSizeHeight.setValue(hook->getTrackerRegionHeight());
    m_toolPosX.setValue(hook->getPos(fid).x);
    m_toolPosY.setValue(hook->getPos(fid).y);
    m_toolPosX.notifyListeners();
    m_toolPosY.notifyListeners();
    m_toolSizeWidth.notifyListeners();
    m_toolSizeHeight.notifyListeners();
  }
  // TTool::Application *app = TTool::getApplication());
  // app->getCurrentScene()->getScene()->getXsheet()->getPegbarTree()->invalidateAll();
  invalidate();
}

//-----------------------------------------------------------------------------

void TrackerTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  // note: apparently sometimes (when the user triple-clicks) we receive this
  // event twice
  if (!m_buttonDown) return;
  // se clicco su una TrackerRegion già selezionato lo deseleziono (per
  // permettere
  // poi l'aggiunta di un nuovo TrackerObject
  if (m_dragged == false && m_hookSelectedIndex == m_lastHookSelectedIndex) {
    m_hookSelectedIndex = -1;
  }
  if (m_newObjectAdded) {
    m_hookSelectedIndex = -1;
    m_newObjectAdded    = false;
    // emit signal in order to update schematic
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  m_lastHookSelectedIndex = m_hookSelectedIndex;

  m_dragged    = false;
  m_buttonDown = false;

  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl || !xl->getSimpleLevel()) return;
  xl->getSimpleLevel()->getProperties()->setDirtyFlag(true);

  // m_selection.selectNone();
  return;
}

//-----------------------------------------------------------------------------

void TrackerTool::deleteSelectedTrackerRegion() {
  TTool::Application *app = TTool::getApplication();
  TXshLevel *xl           = app->getCurrentLevel()->getLevel();
  HookSet *hookSet        = xl->getHookSet();
  if (!xl || !xl->getSimpleLevel() || !hookSet ||
      xl->getSimpleLevel()->isReadOnly())
    return;

  // HookUndo *undo = new HookUndo(xl->getSimpleLevel());
  TFrameId fid = getCurrentFid();

  Hook *hook          = hookSet->getHook(m_hookSelectedIndex);
  m_hookSelectedIndex = -1;
  if (!hook || hook->isEmpty()) return;

  hookSet->clearHook(hook);

  // TUndoManager::manager()->add(undo);
  app->getCurrentScene()
      ->getScene()
      ->getXsheet()
      ->getStageObjectTree()
      ->invalidateAll();
  invalidate();

  // emit signal in order to update schematic
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();

  /*
  int a = m_selectedObjectId;
  int b = m_selectedTrackerRegionIndex;
  if(m_selectedObjectId<0 ||m_selectedTrackerRegionIndex<0) return;
  TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
  TFrameId fid = getCurrentFid();
if(!trackerObjectsSet) return;
  TrackerObject *trackerObject=
trackerObjectsSet->getObject(m_selectedObjectId);
  if(trackerObject==NULL) return;
  if(trackerObject->getHooksCount()==0)
  {
          trackerObjectsSet->removeObject(m_selectedObjectId);
          m_selectedObjectId = -1;
          m_selectedTrackerRegionIndex = -1;
          return;
  }
  trackerObject->removeHook(m_selectedTrackerRegionIndex);
  m_selectedTrackerRegionIndex = trackerObject->getHooksCount()-1;
invalidate(); */
}

//-----------------------------------------------------------------------------

void TrackerTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_picked = true;
  if (m_dragged == false) {
    int hookSelectedIndex;
    pick(hookSelectedIndex, pos);
    if (hookSelectedIndex < 0) {
      m_pos    = pos;
      m_picked = false;
    }
    invalidate();
  }
}

//-----------------------------------------------------------------------------
bool TrackerTool::onPropertyChanged(std::string propertyName) {
  HookSet *hookSet = getHookSet();
  if (!hookSet || m_hookSelectedIndex < 0) return false;
  TFrameId fid = getCurrentFid();
  Hook *hook   = hookSet->getHook(m_hookSelectedIndex);
  if (!hook || hook->isEmpty()) return false;
  if (propertyName == "Width:") {
    double width = m_toolSizeWidth.getValue();
    hook->setTrackerRegionWidth(width);
  }
  if (propertyName == "Height:") {
    double height = m_toolSizeHeight.getValue();
    hook->setTrackerRegionHeight(height);
  }
  if (propertyName == "X:") {
    double x    = m_toolPosX.getValue();
    TPointD pos = hook->getPos(fid);
    pos.x       = x;
    hook->setAPos(fid, pos);
  }
  if (propertyName == "Y:") {
    double y    = m_toolPosY.getValue();
    TPointD pos = hook->getPos(fid);
    pos.y       = y;
    hook->setAPos(fid, pos);
  }
  invalidate();
  return true;
}
void TrackerTool::reset() { m_hookSelectedIndex = -1; }
int TrackerTool::getCursorId() const {
  switch (m_what) {
  case Outside:
    return ToolCursor::TrackerCursor;
  case Inside:
    return ToolCursor::MoveCursor;
  case NormalHook:
    return ToolCursor::MoveCursor;
  case P00:
    return ToolCursor::ScaleCursor;
  case P01:
    return ToolCursor::MoveCursor;
  case P10:
    return ToolCursor::ScaleCursor;
  case P1M:
    return ToolCursor::ScaleHCursor;
  case PM1:
    return ToolCursor::ScaleVCursor;
  case ADD_OBJECT:
    return ToolCursor::SplineEditorCursorAdd;
  default:
    return ToolCursor::TrackerCursor;
  }
  return ToolCursor::TrackerCursor;
}
bool TrackerTool::keyDown(QKeyEvent *event) {
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return false;
  HookSet *hookSet = xl->getHookSet();
  if (!hookSet) return false;
  TFrameId fid = getCurrentFid();
  Hook *hook   = hookSet->getHook(m_hookSelectedIndex);
  if (!hook || hook->isEmpty()) return false;
  TPointD hookPos = hook->getPos(fid);
  TPointD delta(0, 0);
  double pixelSize = getPixelSize();

  switch (event->key()) {
  case Qt::Key_Up:
    delta.y = 1;
    break;
  case Qt::Key_Down:
    delta.y = -1;
    break;
  case Qt::Key_Left:
    delta.x = -1;
    break;
  case Qt::Key_0:
    delta.x = 1;
    break;
  case Qt::Key_PageUp:  // converto in Hook
    hook->setTrackerObjectId(-1);
    break;
  case Qt::Key_PageDown:  // converto in trackerRegion
  {
    TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
    if (!trackerObjectsSet) return false;
    int trackerObjectId = hook->getTrackerObjectId();
    if (trackerObjectId != -1) return false;
    trackerObjectId = trackerObjectsSet->addObject();
    hook->setTrackerObjectId(trackerObjectId);
    hook->setTrackerRegionHeight(pixelSize * 20);
    hook->setTrackerRegionWidth(pixelSize * 20);
  } break;
  default:
    return false;
    break;
  }

  hookPos += delta;
  hook->setAPos(fid, hookPos);
  // TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  //  TNotifier::instance()->notify(TLevelChange());

  return true;
}
// bool moveCursor(const TPointD &pos){}
void TrackerTool::onEnter() {
  HookSet *hookSet = getHookSet();
  if (!hookSet || hookSet->getHookCount() <= m_hookSelectedIndex)
    m_hookSelectedIndex = -1;
}

//-----------------------------------------------------------------------------

void TrackerTool::onLeave() {
  TrackerObjectsSet *trackerObjectsSet = getTrackerObjectsSet();
  if (trackerObjectsSet) trackerObjectsSet->clearAll();
}

//-----------------------------------------------------------------------------

void TrackerTool::onActivate() {}

//-----------------------------------------------------------------------------

void TrackerTool::onDeactivate() {
  // m_selection.selectNone();
  // TSelection::setCurrent(0);
}

//-----------------------------------------------------------------------------

// returns true if the pressed key is recognized and processed in the tool
// instead of triggering the shortcut command.
bool TrackerTool::isEventAcceptable(QEvent *e) {
  if (!isEnabled()) return false;
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return false;
  HookSet *hookSet = xl->getHookSet();
  if (!hookSet) return false;
  Hook *hook = hookSet->getHook(m_hookSelectedIndex);
  if (!hook || hook->isEmpty()) return false;

  QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
  // shift + arrow will not be recognized for now
  if (keyEvent->modifiers() & Qt::ShiftModifier) return false;
  int key = keyEvent->key();
  return (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left ||
          key == Qt::Key_Right);
  // no need to override page up & down keys since they cannot be
  // used as shortcut key for now
}

//=============================================================================

static TTool *getTrackerToolTool() { return &trackerTool; }

//=============================================================================

void TrackerRegionSelection::enableCommands() {
  enableCommand(m_tool, MI_Clear, &TrackerTool::deleteSelectedTrackerRegion);
  // enableCommand(m_tool, MI_ConvertToRegion,
  // &TrackerRegionSelection::convertToRegion);
}

void TrackerRegionSelection::convertToRegion() {
  int i         = 0;
  TXshLevel *xl = TTool::getApplication()->getCurrentLevel()->getLevel();
  if (!xl) return;
  HookSet *hookSet = xl->getHookSet();
  if (!hookSet) return;
  for (i = 0; i < hookSet->getHookCount(); i++) {
    if (isSelected(i, i)) {
      int trackerObjectId = hookSet->getTrackerObjectsSet()->addObject();
      Hook *hook          = hookSet->getHook(i);
      if (!hook || hook->isEmpty()) continue;
      hook->setTrackerObjectId(trackerObjectId);
    }
  }
}
