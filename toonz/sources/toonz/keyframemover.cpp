

#include "keyframemover.h"

// Tnz6 includes
#include "tapp.h"
#include "xsheetviewer.h"

// TnzQt includes
#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheet.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QPainter>

//=============================================================================
// KeyframeMover
//-----------------------------------------------------------------------------

KeyframeMover::KeyframeMover()
    : m_qualifiers(eMoveKeyframes), m_lastKeyframeData(0) {}

//-----------------------------------------------------------------------------

KeyframeMover::~KeyframeMover() {
  delete m_lastKeyframeData;
  m_lastKeyframeData = 0;
}

//-----------------------------------------------------------------------------

TXsheet *KeyframeMover::getXsheet() const {
  return TApp::instance()->getCurrentXsheet()->getXsheet();
}

//-----------------------------------------------------------------------------

void KeyframeMover::setKeyframes() {
  TXsheet *xsh = getXsheet();
  std::set<KeyframePosition>::iterator posIt;
  for (auto const &key : m_lastKeyframes) {
    int c = key.second;
    TStageObjectId objId =
        c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
    TStageObject *stObj = xsh->getStageObject(objId);
    TStageObject::KeyframeMap keyframes;
    stObj->getKeyframes(keyframes);
    for (auto const &frame : keyframes) {
      stObj->removeKeyframeWithoutUndo(frame.first);
    }
  }
  m_lastKeyframeData->getKeyframes(m_lastKeyframes, xsh);
}

//-----------------------------------------------------------------------------

void KeyframeMover::getKeyframes() {
  TXsheet *xsh = getXsheet();
  for (auto const &pos : m_startSelectedKeyframes) {
    int c = pos.second;
    TStageObjectId objId =
        c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
    TStageObject *stObj = xsh->getStageObject(objId);
    assert(stObj->isKeyframe(pos.first));
    TStageObject::KeyframeMap keyframes;
    stObj->getKeyframes(keyframes);
    for (auto const &frame : keyframes) {
      m_lastKeyframes.insert(KeyframePosition(frame.first, c));
    }
  }

  if (m_lastKeyframeData) {
    delete m_lastKeyframeData;
    m_lastKeyframeData = 0;
  }
  m_lastKeyframeData = new TKeyframeData();
  m_lastKeyframeData->setKeyframes(m_lastKeyframes, xsh);
}

//-----------------------------------------------------------------------------

void KeyframeMover::start(TKeyframeSelection *selection, int qualifiers) {
  selection->unselectLockedColumn();
  m_qualifiers                                     = qualifiers;
  std::set<TKeyframeSelection::Position> positions = selection->getSelection();
  m_startSelectedKeyframes                         = positions;
  getKeyframes();
}

//-----------------------------------------------------------------------------
// If can't move keyframe return false; otherwise move keyframe
bool KeyframeMover::moveKeyframes(
    int dr, std::set<TKeyframeSelection::Position> &newPositions,
    TKeyframeSelection *selection) {
  TXsheet *xsh = getXsheet();
  std::set<TKeyframeSelection::Position> positions;
  if (selection)
    positions = selection->getSelection();
  else
    positions = m_startSelectedKeyframes;
  if (positions.empty()) return false;
  std::set<TKeyframeSelection::Position>::iterator posIt;

  if (m_qualifiers & eMoveKeyframes) {
    for (posIt = positions.begin(); posIt != positions.end(); ++posIt) {
      int c = posIt->second;
      int r = posIt->first;
      TStageObjectId objId =
          c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
      TStageObject *stObj = xsh->getStageObject(objId);
      if (r + dr < 0) dr  = -r;
      if (stObj->isKeyframe(r + dr)) return false;
    }

    bool firstTime = false;
    for (posIt = positions.begin(); posIt != positions.end(); ++posIt) {
      int c = posIt->second;
      int r = posIt->first;
      TStageObjectId objId =
          c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
      TStageObject *stObj = xsh->getStageObject(objId);
      if (m_qualifiers & eCopyKeyframes) {
        firstTime = true;
        stObj->setKeyframeWithoutUndo(r + dr, stObj->getKeyframe(r));
      } else
        stObj->moveKeyframe(r + dr, r);
      newPositions.insert(TKeyframeSelection::Position(r + dr, c));
    }
    if (firstTime) {
      m_qualifiers = 0;
      m_qualifiers = eMoveKeyframes;
    }
    return true;
  }

  setKeyframes();
  bool notChange = false;
  for (posIt = m_startSelectedKeyframes.begin();
       posIt != m_startSelectedKeyframes.end(); ++posIt) {
    int c = posIt->second;
    int r = posIt->first;
    TStageObjectId objId =
        c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
    TStageObject *stObj    = xsh->getStageObject(objId);
    if (r + dr < 0) dr     = -r;
    if (dr == 0) notChange = true;
    newPositions.insert(TKeyframeSelection::Position(r, c));
  }
  if (notChange) return true;

  newPositions.clear();
  for (posIt = m_startSelectedKeyframes.begin();
       posIt != m_startSelectedKeyframes.end(); ++posIt) {
    int c = posIt->second;
    int r = posIt->first;
    TStageObjectId objId =
        c >= 0 ? TStageObjectId::ColumnId(c) : TStageObjectId::CameraId(0);
    TStageObject *stObj = xsh->getStageObject(objId);

    if (m_qualifiers & eOverwriteKeyframes) {
      stObj->removeKeyframeWithoutUndo(r + dr);
      if (m_qualifiers & eCopyKeyframes)
        stObj->setKeyframeWithoutUndo(r + dr, stObj->getKeyframe(r));
      else
        stObj->moveKeyframe(r + dr, r);
      newPositions.insert(TKeyframeSelection::Position(r + dr, c));
    } else if (m_qualifiers & eInsertKeyframes) {
      int s                           = r;
      TStageObject::Keyframe keyframe = stObj->getKeyframe(r);
      if (!m_qualifiers & eCopyKeyframes) stObj->removeKeyframeWithoutUndo(r);
      std::set<int> keyframeToShift;
      while (stObj->isKeyframe(s + dr) && s + dr != r) {
        keyframeToShift.insert(s + dr);
        s++;
      }
      stObj->moveKeyframes(keyframeToShift, 1);
      stObj->setKeyframeWithoutUndo(r + dr, keyframe);
      newPositions.insert(TKeyframeSelection::Position(r + dr, c));
    }
  }

  return true;
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// UndoMoveKeyFrame
//-----------------------------------------------------------------------------

class UndoMoveKeyFrame final : public TUndo {
  int m_dr;
  KeyframeMover *m_mover;

public:
  UndoMoveKeyFrame(int dr, KeyframeMover *mover) : m_dr(dr), m_mover(mover) {}

  void undo() const override {
    m_mover->undoMoveKeyframe();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    std::set<TKeyframeSelection::Position> newPositions;
    m_mover->moveKeyframes(m_dr, newPositions);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Move Keyframe"); }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// KeyframeMoverTool
//-----------------------------------------------------------------------------

KeyframeMoverTool::KeyframeMoverTool(XsheetViewer *viewer, bool justMovement)
    : XsheetGUI::DragTool(viewer)
    , m_startSelection()
    , m_offset(0)
    , m_firstRow(0)
    , m_firstCol(0)
    , m_selecting(false)
    , m_startPos()
    , m_curPos()
    , m_firstKeyframeMovement(false)
    , m_justMovement(justMovement) {
  m_mover = new KeyframeMover();
}

//-----------------------------------------------------------------------------

TKeyframeSelection *KeyframeMoverTool::getSelection() {
  return getViewer()->getKeyframeSelection();
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::ctrlSelect(int row, int col) {
  TKeyframeSelection *selection = getSelection();
  bool isSelected               = selection->isSelected(row, col);
  if (isSelected)
    selection->unselect(row, col);
  else
    selection->select(row, col);
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::shiftSelect(int row, int col) {
  TXsheet *xsh = getViewer()->getXsheet();
  int r0 = 0, c0 = 0, r1 = -1, c1 = -1;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TStageObject *obj = xsh->getStageObject(getViewer()->getObjectId(c));
    int ra, rb;
    obj->getKeyframeRange(ra, rb);
    for (int r = ra; r <= rb; r++) {
      if (getSelection()->isSelected(r, c)) {
        if (r0 > r1)
          r0 = r1 = r;
        else if (r < r0)
          r0 = r;
        else if (r > r1)
          r1 = r;
        if (c0 > c1)
          c0 = c1 = c;
        else if (c < c0)
          c0 = c;
        else if (c > c1)
          c1 = c;
      }
    }
  }
  if (row >= r0 && col >= c0) {
    r1 = row;
    c1 = col;
  } else if (row <= r1 && col <= c1) {
    r0 = row;
    c0 = col;
  } else if (col <= c0) {
    c0 = col;
    r1 = row;
  } else {
    c1 = col;
    r0 = row;
  }

  for (int c = c0; c <= c1; c++) {
    TStageObject *obj = xsh->getStageObject(getViewer()->getObjectId(c));
    for (int r = r0; r <= r1; r++)
      if (obj->isKeyframe(r)) getSelection()->select(r, c);
  }
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::rectSelect(int row, int col) {
  TKeyframeSelection *selection = getSelection();
  selection->selectNone();
  TXsheet *xsh = getViewer()->getXsheet();
  int r0 = row, c0 = col, r1 = m_firstRow, c1 = m_firstCol;
  if (r0 > r1) tswap(r0, r1);
  if (c0 > c1) tswap(c0, c1);
  for (int c = c0; c <= c1; c++) {
    TStageObject *obj = xsh->getStageObject(getViewer()->getObjectId(c));
    for (int r = r0; r <= r1; r++) {
      if (obj->isKeyframe(r)) selection->select(r, c);
    }
  }
  m_startSelection = *getSelection();
  refreshCellsArea();
}

//-----------------------------------------------------------------------------

bool KeyframeMoverTool::canMove(const TPoint &pos) {
  const Orientation *o = getViewer()->orientation();
  TXsheet *xsh         = getViewer()->getXsheet();

  TPoint usePos = pos;
  if (!o->isVerticalTimeline()) {
    usePos.x = pos.y;
    usePos.y = pos.x;
  }

  if (usePos.x < 0) return false;

  int col      = usePos.x;
  int startCol = getViewer()->xyToPosition(m_startPos).layer();
  if (col != startCol) return false;

  return true;
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::onCellChange(int row, int col) {
  int lastRow  = getSelection()->getFirstRow() + m_offset;
  int firstRow = m_startSelection.getFirstRow() + m_offset;
  int dr       = row - lastRow;
  int dfr      = row - firstRow;
  int d = (m_mover->getQualifiers() & KeyframeMover::eMoveKeyframes) ? dr : dfr;
  std::set<TKeyframeSelection::Position> newPositions;
  bool ret = m_mover->moveKeyframes(d, newPositions, getSelection());
  if (!ret) return;
  getSelection()->m_positions.clear();
  getSelection()->m_positions.swap(newPositions);
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::onClick(const QMouseEvent *event) {
  m_firstKeyframeMovement   = true;
  m_selecting               = false;
  TXsheet *xsheet           = getViewer()->getXsheet();
  CellPosition cellPosition = getViewer()->xyToPosition(event->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  m_firstRow                = row;
  m_firstCol                = col;
  bool isSelected           = getSelection()->isSelected(row, col);
  if (!m_justMovement) {
    if (event->modifiers() & Qt::ControlModifier)
      ctrlSelect(row, col);
    else if (event->modifiers() & Qt::ShiftModifier)
      shiftSelect(row, col);
    else if (!isSelected) {
      getSelection()->selectNone();
      getSelection()->select(row, col);
      m_selecting = true;
    }
    getSelection()->makeCurrent();
  }
  if (!getSelection()->isEmpty())
    m_offset       = row - getSelection()->getFirstRow();
  m_startSelection = *getSelection();
  getViewer()->update();
  m_startPos = TPointD(event->pos().x(), event->pos().y());
  m_curPos   = m_startPos;
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::onDrag(const QMouseEvent *e) {
  int x                     = e->pos().x();
  int y                     = e->pos().y();
  m_curPos                  = TPointD(x, y);
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();
  if (m_selecting)
    rectSelect(row, col);
  else {
    if (m_firstKeyframeMovement) {
      int qualifiers = 0;
      if (e->modifiers() & Qt::ControlModifier)
        qualifiers |= KeyframeMover::eCopyKeyframes;
      if (e->modifiers() & Qt::ShiftModifier)
        qualifiers |= KeyframeMover::eInsertKeyframes;
      else if (e->modifiers() & Qt::AltModifier)
        qualifiers |= KeyframeMover::eOverwriteKeyframes;
      else
        qualifiers |= KeyframeMover::eMoveKeyframes;
      m_mover->start(getSelection(), qualifiers);
    }
    m_firstKeyframeMovement = false;
    onCellChange(row, col);
    refreshCellsArea();
  }
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::onRelease(const CellPosition &pos) {
  if (m_selecting) {
    m_selecting = false;
    getViewer()->updateCells();
    return;
  }
  m_offset = 0;
  int dr   = getSelection()->getFirstRow() - m_startSelection.getFirstRow();
  if (dr) {
    TUndoManager::manager()->add(new UndoMoveKeyFrame(dr, m_mover));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
}

//-----------------------------------------------------------------------------

void KeyframeMoverTool::drawCellsArea(QPainter &p) {
  if (!m_selecting) return;
  QPen pen(Qt::DashLine);
  pen.setColor(Qt::black);
  p.setPen(pen);
  double endRectPosX = m_curPos.x - m_startPos.x;
  double endRectPosY = m_curPos.y - m_startPos.y;
  p.drawRect(m_startPos.x, m_startPos.y, endRectPosX, endRectPosY);
}
