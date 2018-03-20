

#include "tools/imagegrouping.h"

// TnzTools includes
#include "tools/strokeselection.h"
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "vectorselectiontool.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selectioncommandids.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"

// TnzCore includes
#include "tvectorimage.h"
#include "tundo.h"
#include "tthreadmessage.h"

// Qt includes
#include <QMenu>

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

void groupWithoutUndo(TVectorImage *vimg, StrokeSelection *selection) {
  int count = 0, fromStroke = -1, lastSelected = -1;

  for (int i = 0; i < (int)vimg->getStrokeCount(); i++)
    if (selection->isSelected(i)) {
      if (fromStroke == -1)
        fromStroke = i;
      else if (lastSelected != i - 1)  // non sono contigui gli stroke
                                       // selezionati: faccio affiorare quelli
                                       // sotto
      {
        int j = 0;
        for (j = 0; j < count; j++) selection->select(fromStroke + j, false);
        vimg->moveStrokes(fromStroke, count, i);
        fromStroke = i - count;
        for (j = 0; j < count; j++) selection->select(fromStroke + j, true);
      }
      lastSelected = i;
      count++;
    }

  assert(count > 0);

  vimg->group(fromStroke, count);
  TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void ungroupWithoutUndo(TVectorImage *vimg, StrokeSelection *selection) {
  for (int i = 0; i < (int)vimg->getStrokeCount();)
    if (selection->isSelected(i)) {
      if (!vimg->isStrokeGrouped(i)) {
        i++;
        continue;
      }
      i += vimg->ungroup(i);
    } else
      i++;
  TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//=============================================================================
// GroupUndo
//-----------------------------------------------------------------------------

class GroupUndo final : public ToolUtils::TToolUndo {
  std::unique_ptr<StrokeSelection> m_selection;

public:
  GroupUndo(TXshSimpleLevel *level, const TFrameId &frameId,
            StrokeSelection *selection)
      : ToolUtils::TToolUndo(level, frameId), m_selection(selection) {}

  void undo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (image) ungroupWithoutUndo(image.getPointer(), m_selection.get());
  }

  void redo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (image) groupWithoutUndo(image.getPointer(), m_selection.get());
  }

  int getSize() const override { return sizeof(*this); }

  QString getToolName() override { return QObject::tr("Group"); }
};

//=============================================================================
// UngroupUndo
//-----------------------------------------------------------------------------

class UngroupUndo final : public ToolUtils::TToolUndo {
  std::unique_ptr<StrokeSelection> m_selection;

public:
  UngroupUndo(TXshSimpleLevel *level, const TFrameId &frameId,
              StrokeSelection *selection)
      : ToolUtils::TToolUndo(level, frameId), m_selection(selection) {}

  void undo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (image) groupWithoutUndo(image.getPointer(), m_selection.get());
  }

  void redo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (image) ungroupWithoutUndo(image.getPointer(), m_selection.get());
  }

  int getSize() const override { return sizeof(*this); }

  QString getToolName() override { return QObject::tr("Ungroup"); }
};

//=============================================================================
// MoveGroupUndo
//-----------------------------------------------------------------------------

class MoveGroupUndo final : public ToolUtils::TToolUndo {
  UCHAR m_moveType;
  int m_refStroke, m_count, m_moveBefore;
  std::vector<std::pair<TStroke *, int>> m_selectedGroups;

public:
  MoveGroupUndo(TXshSimpleLevel *level, const TFrameId &frameId, UCHAR moveType,
                int refStroke, int count, int moveBefore,
                const std::vector<std::pair<TStroke *, int>> &selectedGroups)
      : ToolUtils::TToolUndo(level, frameId)
      , m_moveType(moveType)
      , m_refStroke(refStroke)
      , m_count(count)
      , m_moveBefore(moveBefore)
      , m_selectedGroups(selectedGroups) {}

  ~MoveGroupUndo() {}

  void undo() const override {
    int refStroke;
    int moveBefore;
    switch (m_moveType) {
    case TGroupCommand::FRONT:
      refStroke  = m_moveBefore - m_count;
      moveBefore = m_refStroke;
      break;
    case TGroupCommand::FORWARD:
      refStroke  = m_moveBefore - m_count;
      moveBefore = m_refStroke;
      break;
    case TGroupCommand::BACK:
      refStroke  = m_moveBefore;
      moveBefore = m_refStroke + m_count;
      break;
    case TGroupCommand::BACKWARD:
      refStroke  = m_moveBefore;
      moveBefore = m_refStroke + m_count;
      break;
    default:
      assert(!"group move not defined!");
      break;
    }
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    image->moveStrokes(refStroke, m_count, moveBefore);
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) {  // Se la selezione corrente e' la StrokeSelection
      // seleziono gli stroke che ho modificato con l'undo
      selection->selectNone();

      for (int i = 0; i < (int)m_selectedGroups.size(); i++) {
        int index = image->getStrokeIndex(m_selectedGroups[i].first);
        if (index == -1) continue;
        for (int j = index; j < index + m_selectedGroups[i].second; j++)
          selection->select(j, true);
      }
    }
    TTool::getApplication()->getCurrentScene()->notifySceneChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    image->moveStrokes(m_refStroke, m_count, m_moveBefore);
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) {  // Se la selezione corrente e' la StrokeSelection
      // seleziono gli stroke che ho modificato con il redo
      selection->selectNone();

      for (int i = 0; i < (int)m_selectedGroups.size(); i++) {
        int index = image->getStrokeIndex(m_selectedGroups[i].first);
        if (index == -1) continue;
        for (int j = index; j < index + m_selectedGroups[i].second; j++)
          selection->select(j, true);
      }
    }
    TTool::getApplication()->getCurrentScene()->notifySceneChanged();
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getToolName() override {
    static QMap<int, QString> commandTypeStrMap{
        {TGroupCommand::FRONT, QObject::tr(" to Front")},
        {TGroupCommand::FORWARD, QObject::tr(" to Forward")},
        {TGroupCommand::BACK, QObject::tr(" to Back")},
        {TGroupCommand::BACKWARD, QObject::tr(" to Backward")}};

    return QObject::tr("Move Group") + commandTypeStrMap.value(m_moveType);
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// TGroupCommand
//-----------------------------------------------------------------------------

namespace {

std::vector<std::pair<TStroke *, int>> getSelectedGroups(TVectorImage *vimg,
                                                         StrokeSelection *sel) {
  UINT i, j;
  std::vector<std::pair<TStroke *, int>> ret;

  for (i = 0; i < vimg->getStrokeCount(); i++)
    if (sel->isSelected(i)) {
      if (vimg->isStrokeGrouped(i)) {
        for (j = i + 1; j < vimg->getStrokeCount() && vimg->sameSubGroup(i, j);
             j++)
          if (!sel->isSelected(j))
            return std::vector<std::pair<TStroke *, int>>();
        ret.push_back(std::pair<TStroke *, int>(vimg->getStroke(i), j - i));
        i = j - 1;
      } else
        ret.push_back(std::pair<TStroke *, int>(vimg->getStroke(i), 1));
    }
  return ret;
}

}  // namepsace

//--------------------------------------------------------------------------------------

UCHAR TGroupCommand::getGroupingOptions() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return 0;
  TVectorImage *vimg = dynamic_cast<TVectorImage *>(tool->getImage(false));
  if (!vimg) return 0;

  UCHAR mask = 0;
  int count  = 0;
  UINT i, j;

  // spostamento: si possono  spostare solo gruppi interi  oppure  stroke   non
  // gruppate

  std::vector<std::pair<TStroke *, int>> strokeIndexes =
      getSelectedGroups(vimg, m_sel);

  /*
  //spostamento: si puo' spostare solo un gruppo(e uno solo per volta) oppure
una stroke singola  non gruppata
for (i=0; i<vimg->getStrokeCount(); i++)
  if (m_sel->isSelected(i))
  {
          if (strokeIndex != -1)
                  {
                  if (!vimg->isStrokeGrouped(i) ||
!vimg->sameSubGroup(strokeIndex, i))
                          break;
                  }
          else
                  {
                  strokeIndex = i;
                  if (vimg->isStrokeGrouped(i))
                          {
                          for (j=0; j<vimg->getStrokeCount(); j++)
                                  if (!m_sel->isSelected(j) &&
vimg->sameSubGroup(i, j))
                                          break;//non tutto il gruppo e'
selezionato
                          if (j<vimg->getStrokeCount())
                                  break;
                          }
                  }
          count++;
          }
   */

  if (strokeIndexes.empty()) return 0;  // no stroke selected

  int strokeIndex = vimg->getStrokeIndex(strokeIndexes[0].first);

  if (strokeIndexes.size() > 1 || strokeIndex > 0) {
    mask |= BACK;
    mask |= BACKWARD;
  }
  if (strokeIndexes.size() > 1 ||
      strokeIndex + strokeIndexes[0].second - 1 <
          (int)vimg->getStrokeCount() - 1) {
    mask |= FRONT;
    mask |= FORWARD;
  }

  /*
if (i == vimg->getStrokeCount())
{
  if (strokeIndex+count<(int)vimg->getStrokeCount())
    {
          mask |= FRONT;
          mask |= FORWARD;
    }
  if (strokeIndex>0)
    {
          mask |= BACK;
          mask |= BACKWARD;
    }
}
*/

  // PER l'UNGROUP: si ungruppa solo se tutti gli stroke selezionati stanno nel
  // gruppo (anche piu' gruppi insieme)

  for (i = 0; i < vimg->getStrokeCount(); i++) {
    if (m_sel->isSelected(i)) {
      if (!vimg->isStrokeGrouped(i)) break;
      for (j = 0; j < vimg->getStrokeCount(); j++)
        if (!m_sel->isSelected(j) && vimg->sameSubGroup(i, j)) break;
      if (j < vimg->getStrokeCount()) break;
    }
  }
  if (i == vimg->getStrokeCount()) mask |= UNGROUP;

  // PER il GROUP: si raggruppa solo  se:
  // //almeno una delle  stroke selezionate non fa parte di gruppi o e' di un
  // gruppo diverso
  // e se c'e' una stroke di un gruppo, allora tutto il gruppo della stroke e'
  // selezionato

  bool groupingMakesSense = false;
  int refStroke           = -1;
  for (i = 0; i < vimg->getStrokeCount(); i++)
    if (m_sel->isSelected(i)) {
      if (vimg->isStrokeGrouped(i)) {
        if (refStroke == -1)
          refStroke = i;
        else if (!vimg->sameSubGroup(refStroke, i))
          groupingMakesSense = true;  // gli storke selezionati non sono gia'
                                      // tutti dello stesso gruppo
        for (j = 0; j < vimg->getStrokeCount(); j++)
          if (!m_sel->isSelected(j) && vimg->sameGroup(i, j)) return mask;
      } else
        groupingMakesSense = true;
    }

  if (groupingMakesSense) mask |= GROUP;

  return mask;
}

//-----------------------------------------------------------------------------

#ifdef NUOVO

UCHAR TGroupCommand::getGroupingOptions() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return 0;
  TVectorImage *vimg = dynamic_cast<TVectorImage *>(tool->getImage(false));
  if (!vimg) return 0;

  int strokeIndex = -1;
  UCHAR mask      = 0;
  int count       = 0;
  UINT i, j;
  bool valid = true;
  // spostamento: si puo' spostare solo un gruppo(e uno solo per volta) oppure
  // una stroke singola  non gruppata
  std::vector<pair<int, int>> groups;

  for (i = 0; i < vimg->getStrokeCount() && valid;)
    if (m_sel->isSelected(i)) {
      strokeIndex = i;
      count       = 1;
      if (vimg->isStrokeGrouped(i)) {
        for (j = i + 1; j < vimg->getStrokeCount() && valid; j++)
          if (m_sel->isSelected(j) && vimg->sameSubGroup(i, j))
            count++;
          else if (!m_sel->isSelected(j) && vimg->sameSubGroup(i, j))
            valid = false;  // non tutto il gruppo e' selezionato
      }
      groups.push_back(pair<int, int>(strokeIndex, count));

      i = i + count;
    }

  if (groups.empty()) return 0;  // no stroke selected

  if (!valid) return 0;

  if (strokeIndex + count < (int)vimg->getStrokeCount()) {
    mask |= FRONT;
    mask |= FORWARD;
  }
  if (strokeIndex > 0) {
    mask |= BACK;
    mask |= BACKWARD;
  }

  // PER l'UNGROUP: si ungruppa solo se tutti gli stroke selezionati stanno nel
  // gruppo (anche piu' gruppi insieme)

  for (i = 0; i < vimg->getStrokeCount(); i++) {
    if (m_sel->isSelected(i)) {
      if (!vimg->isStrokeGrouped(i)) break;
      for (j = 0; j < vimg->getStrokeCount(); j++)
        if (!m_sel->isSelected(j) && vimg->sameSubGroup(i, j)) break;
      if (j < vimg->getStrokeCount()) break;
    }
  }
  if (i == vimg->getStrokeCount()) mask |= UNGROUP;

  // PER il GROUP: si raggruppa solo  se:
  // //almeno una delle  stroke selezionate non fa parte di gruppi o e' di un
  // gruppo diverso
  // e se c'e' una stroke di un gruppo, allora tutto il gruppo della stroke e'
  // selezionato

  bool groupingMakesSense = false;
  int refStroke           = -1;
  for (i = 0; i < vimg->getStrokeCount(); i++)
    if (m_sel->isSelected(i)) {
      if (vimg->isStrokeGrouped(i)) {
        if (refStroke == -1)
          refStroke = i;
        else if (!vimg->sameSubGroup(refStroke, i))
          groupingMakesSense = true;  // gli storke selezionati non sono gia'
                                      // tutti dello stesso gruppo
        for (j = 0; j < vimg->getStrokeCount(); j++)
          if (!m_sel->isSelected(j) && vimg->sameGroup(i, j)) return mask;
      } else
        groupingMakesSense = true;
    }

  if (groupingMakesSense) mask |= GROUP;

  return mask;
}

#endif

//-----------------------------------------------------------------------------

void TGroupCommand::group() {
  if (!(getGroupingOptions() & GROUP)) return;

  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImage *vimg = (TVectorImage *)tool->getImage(true);

  if (!vimg) return;
  QMutexLocker lock(vimg->getMutex());
  groupWithoutUndo(vimg, m_sel);
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  TUndoManager::manager()->add(
      new GroupUndo(level, tool->getCurrentFid(), new StrokeSelection(*m_sel)));
}

//-----------------------------------------------------------------------------

void TGroupCommand::enterGroup() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImage *vimg = (TVectorImage *)tool->getImage(true);

  if (!vimg) return;
  int index = -1;

  for (int i = 0; i < (int)vimg->getStrokeCount(); i++)
    if (m_sel->isSelected(i)) {
      index = i;
      break;
    }

  if (index == -1) return;

  if (!vimg->canEnterGroup(index)) return;
  vimg->enterGroup(index);
  TSelection *selection = TSelection::getCurrent();
  if (selection) selection->selectNone();

  TTool::getApplication()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void TGroupCommand::exitGroup() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImage *vimg = (TVectorImage *)tool->getImage(true);

  if (!vimg) return;
  vimg->exitGroup();
  TTool::getApplication()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void TGroupCommand::ungroup() {
  if (!(getGroupingOptions() & UNGROUP)) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImage *vimg = (TVectorImage *)tool->getImage(true);
  if (!vimg) return;
  QMutexLocker lock(vimg->getMutex());
  ungroupWithoutUndo(vimg, m_sel);
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  TUndoManager::manager()->add(new UngroupUndo(level, tool->getCurrentFid(),
                                               new StrokeSelection(*m_sel)));
}

//-----------------------------------------------------------------------------

/*
void computeMovingBounds(TVectorImage*vimg, int fromIndex, int toIndex,
int&lower, int& upper)
{
lower = 0;
upper = vimg->getStrokeCount();

int refDepth = vimg->getGroupDepth(fromIndex)-1;

if (refDepth==0)
  return;

int i;
for (i=fromIndex-1; i>=0; i--)
  if (vimg->getCommonGroupDepth(fromIndex, i)<refDepth)
    {
    lower = i+1;
    break;
    }

for (i=fromIndex+1; i<vimg->getStrokeCount(); i++)
  if (vimg->getCommonGroupDepth(fromIndex, i)<refDepth)
    {
    upper = i;
    break;
    }
}
*/
//-----------------------------------------------------------------------------
namespace {

int commonDepth(TVectorImage *vimg, int index1, int count, int index2) {
  int i, ret = 1000;
  for (i = index1; i < index1 + count; i++)
    ret = std::min(ret, vimg->getCommonGroupDepth(i, index2));
  return ret;
}

//-----------------------------------------------------------------------------

/*
bool cantMove1(TVectorImage* vimg, int refStroke, int count, int moveBefore,
bool rev)
  {
  if (moveBefore<(int)vimg->getStrokeCount() && moveBefore>0 &&
             vimg->getCommonGroupDepth(moveBefore-1,
moveBefore)>commonDepth(vimg, refStroke, count, moveBefore) &&
             vimg->getCommonGroupDepth(moveBefore-1,
moveBefore)>commonDepth(vimg, refStroke, count, moveBefore-1))
    return true;

  int prev = (rev)?moveBefore:moveBefore-1;

  if (refStroke>0 && commonDepth(vimg, refStroke, count,
refStroke-1)>commonDepth(vimg, refStroke, count, prev))
    return true;
  if (refStroke+count<(int)vimg->getStrokeCount() && commonDepth(vimg,
refStroke, count, refStroke+count)>commonDepth(vimg, refStroke, count, prev))
    return true;

  return false;
  }
*/

}  // namespace

//-----------------------------------------------------------------------------

/*
  int i, refStroke=-1, count=0;
  for (i=0; i<(int)vimg->getStrokeCount(); i++)
    if(m_sel->isSelected(i))
      {
      assert(refStroke==-1 || i==0  || m_sel->isSelected(i-1));
      if (refStroke==-1)
        refStroke = i;
      count++;
      }

  if(count==0) return;
  */

static int doMoveGroup(
    UCHAR moveType, TVectorImage *vimg,
    const std::vector<std::pair<TStroke *, int>> &selectedGroups, int index) {
  int refStroke = vimg->getStrokeIndex(selectedGroups[index].first);
  int count     = selectedGroups[index].second;

  int moveBefore;
  switch (moveType) {
  case TGroupCommand::FRONT:
    moveBefore = vimg->getStrokeCount();

    while (moveBefore >= refStroke + count + 1 &&
           !vimg->canMoveStrokes(refStroke, count, moveBefore))
      moveBefore--;
    if (moveBefore == refStroke + count) return -1;

    break;
  case TGroupCommand::FORWARD:
    moveBefore = refStroke + count + 1;
    while (moveBefore <= (int)vimg->getStrokeCount() &&
           !vimg->canMoveStrokes(refStroke, count, moveBefore))
      moveBefore++;
    if (moveBefore == vimg->getStrokeCount() + 1) return -1;
    break;
  case TGroupCommand::BACK:
    moveBefore = 0;
    while (moveBefore <= refStroke - 1 &&
           !vimg->canMoveStrokes(refStroke, count, moveBefore))
      moveBefore++;
    if (moveBefore == refStroke) return -1;
    break;
  case TGroupCommand::BACKWARD:
    moveBefore = refStroke - 1;
    while (moveBefore >= 0 &&
           !vimg->canMoveStrokes(refStroke, count, moveBefore))
      moveBefore--;
    if (moveBefore == -1) return -1;
    break;
  default:
    assert(false);
  }

  vimg->moveStrokes(refStroke, count, moveBefore);

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  TUndoManager::manager()->add(new MoveGroupUndo(level, tool->getCurrentFid(),
                                                 moveType, refStroke, count,
                                                 moveBefore, selectedGroups));

  return moveBefore;
}

//-----------------------------------------------------------------------------

void TGroupCommand::moveGroup(UCHAR moveType) {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  TVectorImage *vimg = (TVectorImage *)tool->getImage(true);
  if (!vimg) return;

  std::vector<std::pair<TStroke *, int>> selectedGroups =
      getSelectedGroups(vimg, m_sel);
  if (selectedGroups.empty()) return;

  int i, j;

  TUndoManager::manager()->beginBlock();

  switch (moveType) {
  case TGroupCommand::FRONT:
  case TGroupCommand::BACKWARD:
    i = 0;
    if (moveType == TGroupCommand::BACKWARD &&
        vimg->getStrokeIndex(selectedGroups[i].first) ==
            0)  // tutti i gruppi adiacenti gia in fondo non possono essere
                // backwardati
    {
      i++;
      while (i < (int)selectedGroups.size() &&
             vimg->getStrokeIndex(selectedGroups[i - 1].first) +
                     selectedGroups[i - 1].second - 1 ==
                 vimg->getStrokeIndex(selectedGroups[i].first) - 1)
        i++;
    }
    for (; i <= (int)selectedGroups.size() - 1; i++)
      doMoveGroup(moveType, vimg, selectedGroups,
                  i);  // vimg->getStrokeIndex(selectedGroups[i].first),
                       // selectedGroups[i].second);
    break;

  case TGroupCommand::BACK:
  case TGroupCommand::FORWARD:
    i = selectedGroups.size() - 1;
    if (moveType == TGroupCommand::FORWARD &&
        vimg->getStrokeIndex(selectedGroups[i].first) +
                selectedGroups[i].second - 1 ==
            vimg->getStrokeCount() - 1)  // tutti i gruppi adiacenti gia in cime
                                         // non possono essere forwardati
    {
      i--;
      while (i >= 0 &&
             vimg->getStrokeIndex(selectedGroups[i + 1].first) - 1 ==
                 vimg->getStrokeIndex(selectedGroups[i].first) +
                     selectedGroups[i].second - 1)
        i--;
    }
    for (; i >= 0; i--) doMoveGroup(moveType, vimg, selectedGroups, i);
    break;

  default:
    assert(false);
  }

  TUndoManager::manager()->endBlock();

  m_sel->selectNone();

  for (i = 0; i < (int)selectedGroups.size(); i++) {
    int index = vimg->getStrokeIndex(selectedGroups[i].first);
    for (j = index; j < index + selectedGroups[i].second; j++)
      m_sel->select(j, true);
  }

  tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void TGroupCommand::addMenuItems(QMenu *menu) {
  UCHAR optionMask = getGroupingOptions();
  if (optionMask == 0) return;

  CommandManager *cm = CommandManager::instance();

  if (optionMask & TGroupCommand::GROUP)
    menu->addAction(cm->getAction(MI_Group));

  if (optionMask & TGroupCommand::UNGROUP)
    menu->addAction(cm->getAction(MI_Ungroup));

  if (optionMask & (TGroupCommand::GROUP | TGroupCommand::UNGROUP) &&
      optionMask & (TGroupCommand::FORWARD | TGroupCommand::BACKWARD))
    menu->addSeparator();

  if (optionMask & TGroupCommand::FORWARD) {
    menu->addAction(cm->getAction(MI_BringToFront));
    menu->addAction(cm->getAction(MI_BringForward));
  }
  if (optionMask & TGroupCommand::BACKWARD) {
    menu->addAction(cm->getAction(MI_SendBack));
    menu->addAction(cm->getAction(MI_SendBackward));
  }
  menu->addSeparator();
}
