

// TnzCore includes
#include "tundo.h"
#include "trandom.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tpalette.h"
#include "tsystem.h"
#include "tstream.h"
#include "tconvert.h"
#include "tfilepath_io.h"

// TnzBase includes
#include "tunit.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/stageobjectutil.h"
#include "toonz/toonzfolders.h"
#include "toonz/txshchildlevel.h"
#include "toonz/childstack.h"
#include "toonz/tproject.h"
#include "toonz/fxcommand.h"
#include "toonz/tfxhandle.h"
#include "toonz/scenefx.h"
#include "toonz/preferences.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/stageobjectsdata.h"
#include "historytypes.h"

// Tnz6 includes
#include "cellselection.h"
#include "columnselection.h"
#include "keyframeselection.h"
#include "celldata.h"
#include "tapp.h"
#include "duplicatepopup.h"
#include "menubarcommandids.h"
#include "columncommand.h"

// Qt includes
#include <QClipboard>

// tcg includes
#include "tcg/boost/range_utility.h"

// boost includes
#include <boost/bind.hpp>
#include <boost/bind/make_adaptable.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace ba = boost::adaptors;
using namespace std;  // Please, remove

//*****************************************************************************
//    Local Namespace  stuff
//*****************************************************************************

namespace {

void getColumns(std::vector<int> &columns) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
      app->getCurrentSelection()->getSelection());

  if (selection && selection->isEmpty()) selection = 0;

  if (!selection)  // Se non c'e' selezione inserisco la colonna di camera
    columns.push_back(-1);

  for (int c = 0; c < xsh->getColumnCount(); ++c)
    if (!selection || selection->isColumnSelected(c)) columns.push_back(c);
}

bool isKeyframe(int r, int c) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  TStageObjectId objectId =
      (c == -1) ? TStageObjectId::CameraId(0) : TStageObjectId::ColumnId(c);

  TStageObject *object = xsh->getStageObject(objectId);
  assert(object);

  return object->isKeyframe(r);
}

}  // namespace

//*****************************************************************************
//    InsertSceneFrame  command
//*****************************************************************************

namespace XshCmd {

class InsertSceneFrameUndo : public TUndo {
protected:
  int m_frame;

public:
  InsertSceneFrameUndo(int frame) : m_frame(frame) {}

  void undo() const override {
    doRemoveSceneFrame(m_frame);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    doInsertSceneFrame(m_frame);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Insert Frame  at Frame %1")
        .arg(QString::number(m_frame + 1));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }

protected:
  static void doInsertSceneFrame(int frame);
  static void doRemoveSceneFrame(int frame);
};

//-----------------------------------------------------------------------------

void InsertSceneFrameUndo::doInsertSceneFrame(int frame) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  int c, colsCount = xsh->getColumnCount();
  for (c = -1; c < colsCount; ++c) {
    TStageObjectId objectId;

    if (c == -1) {
#ifdef LINETEST
      objectId = TStageObjectId::CameraId(0);
#else
      continue;
#endif
    } else
      objectId = TStageObjectId::ColumnId(c);

    xsh->insertCells(frame, c);
    xsh->setCell(frame, c, xsh->getCell(frame + 1, c));

    if (!xsh->getColumn(c) || xsh->getColumn(c)->isLocked()) continue;

    if (TStageObject *obj = xsh->getStageObject(objectId))
      insertFrame(obj, frame);
  }
}

//-----------------------------------------------------------------------------

void InsertSceneFrameUndo::doRemoveSceneFrame(int frame) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  for (int c = -1; c != xsh->getColumnCount(); ++c) {
    TStageObjectId objectId;

    if (c == -1) {
#ifdef LINETEST
      objectId = TStageObjectId::CameraId(0);
#else
      continue;
#endif
    } else
      objectId = TStageObjectId::ColumnId(c);

    xsh->removeCells(frame, c);

    if (!xsh->getColumn(c) || xsh->getColumn(c)->isLocked()) continue;

    if (TStageObject *pegbar = xsh->getStageObject(objectId))
      removeFrame(pegbar, frame);
  }
}

//-----------------------------------------------------------------------------

static void insertSceneFrame(int frame) {
  InsertSceneFrameUndo *undo = new InsertSceneFrameUndo(frame);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//=============================================================================

class ToggleCurrentTimeIndicatorCommand final : public MenuItemHandler {
public:
  ToggleCurrentTimeIndicatorCommand()
      : MenuItemHandler(MI_ToggleCurrentTimeIndicator) {}
  void execute() override {
    bool currentTimeIndEnabled =
        Preferences::instance()->isCurrentTimelineIndicatorEnabled();
    Preferences::instance()->enableCurrentTimelineIndicator(
        !currentTimeIndEnabled);
  }
} toggleCurrentTimeIndicatorComman;

//=============================================================================

class InsertSceneFrameCommand final : public MenuItemHandler {
public:
  InsertSceneFrameCommand() : MenuItemHandler(MI_InsertSceneFrame) {}
  void execute() override {
    int frame = TApp::instance()->getCurrentFrame()->getFrame();
    XshCmd::insertSceneFrame(frame);
  }
} insertSceneFrameCommand;

//*****************************************************************************
//    RemoveSceneFrame  command
//*****************************************************************************

class RemoveSceneFrameUndo final : public InsertSceneFrameUndo {
  std::vector<TXshCell> m_cells;
  std::vector<TStageObject::Keyframe> m_keyframes;

public:
  RemoveSceneFrameUndo(int frame) : InsertSceneFrameUndo(frame) {
    // Store cells and TStageObject::Keyframe that will be canceled

    TXsheet *xsh  = TApp::instance()->getCurrentXsheet()->getXsheet();
    int colsCount = xsh->getColumnCount();

    m_cells.resize(colsCount);
    m_keyframes.resize(colsCount + 1);

    // Inserting the eventual camera keyframe at the end
    TStageObject *cameraObj = xsh->getStageObject(TStageObjectId::CameraId(0));
    if (cameraObj->isKeyframe(m_frame))
      m_keyframes[colsCount] = cameraObj->getKeyframe(m_frame);

    for (int c = 0; c != colsCount; ++c) {
      // Store cell
      m_cells[c] = xsh->getCell(m_frame, c);

      // Store stage object keyframes
      TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
      if (obj->isKeyframe(m_frame)) m_keyframes[c] = obj->getKeyframe(m_frame);
    }
  }

  void redo() const override { InsertSceneFrameUndo::undo(); }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    // Insert an empty frame, need space for our stored stuff
    doInsertSceneFrame(m_frame);

    // Insert cells
    int cellsCount = m_cells.size();

    // Deal with the eventual camera keyframe
    if (m_keyframes[cellsCount].m_isKeyframe) {
      TStageObject *cameraObj =
          xsh->getStageObject(TStageObjectId::CameraId(0));
      cameraObj->setKeyframeWithoutUndo(m_frame, m_keyframes[cellsCount]);
    }

    for (int c = 0; c != cellsCount; ++c) {
      xsh->setCell(m_frame, c, m_cells[c]);

      if (m_keyframes[c].m_isKeyframe) {
        TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
        obj->setKeyframeWithoutUndo(m_frame, m_keyframes[c]);
      }
    }

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return 10 << 10;
  }  // Gave up exact calculation. Say ~ 10 kB?

  QString getHistoryString() override {
    return QObject::tr("Remove Frame  at Frame %1")
        .arg(QString::number(m_frame + 1));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

static void removeSceneFrame(int frame) {
  RemoveSceneFrameUndo *undo = new RemoveSceneFrameUndo(frame);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//=============================================================================

class RemoveSceneFrameCommand final : public MenuItemHandler {
public:
  RemoveSceneFrameCommand() : MenuItemHandler(MI_RemoveSceneFrame) {}
  void execute() override {
    int frame = TApp::instance()->getCurrentFrame()->getFrame();
    XshCmd::removeSceneFrame(frame);
  }
} removeSceneFrameCommand;

//*****************************************************************************
//    GlobalKeyframeUndo  definition
//*****************************************************************************

class GlobalKeyframeUndo : public TUndo {
public:
  GlobalKeyframeUndo(int frame) : m_frame(frame) {}

  int getSize() const override {
    return sizeof(*this) + m_columns.size() * sizeof(int);
  }

  int getHistoryType() override { return HistoryType::Xsheet; }

protected:
  std::vector<int> m_columns;
  int m_frame;

protected:
  static void doInsertGlobalKeyframes(int frame,
                                      const std::vector<int> &columns);
  static void doRemoveGlobalKeyframes(int frame,
                                      const std::vector<int> &columns);
};

//-----------------------------------------------------------------------------

void GlobalKeyframeUndo::doInsertGlobalKeyframes(
    int frame, const std::vector<int> &columns) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  int i, colsCount = columns.size();
  for (i = 0; i != colsCount; ++i) {
    TStageObjectId objectId;

    int c = columns[i];

    TXshColumn *column = xsh->getColumn(c);
    if (column && column->getSoundColumn()) continue;

    if (c == -1) {
#ifdef LINETEST
      objectId = TStageObjectId::CameraId(0);
#else
      continue;
#endif
    } else
      objectId = TStageObjectId::ColumnId(c);

    TXshColumn *xshColumn = xsh->getColumn(c);
    if ((!xshColumn || xshColumn->isLocked() ||
         xshColumn->isCellEmpty(frame)) &&
        !objectId.isCamera())
      continue;

    TStageObject *obj = xsh->getStageObject(objectId);
    obj->setKeyframeWithoutUndo(frame);
  }
}

//-----------------------------------------------------------------------------

void GlobalKeyframeUndo::doRemoveGlobalKeyframes(
    int frame, const std::vector<int> &columns) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  int i, colsCount = columns.size();
  for (i = 0; i != colsCount; ++i) {
    TStageObjectId objectId;

    int c = columns[i];

    TXshColumn *column = xsh->getColumn(c);
    if (column && column->getSoundColumn()) continue;

    if (c == -1) {
#ifdef LINETEST
      objectId = TStageObjectId::CameraId(0);
#else
      continue;
#endif
    } else
      objectId = TStageObjectId::ColumnId(c);

    if (xsh->getColumn(c) && xsh->getColumn(c)->isLocked()) continue;

    TStageObject *obj = xsh->getStageObject(objectId);
    obj->removeKeyframeWithoutUndo(frame);
  }
}

//*****************************************************************************
//    InsertGlobalKeyframe  command
//*****************************************************************************

class InsertGlobalKeyframeUndo final : public GlobalKeyframeUndo {
public:
  InsertGlobalKeyframeUndo(int frame, const std::vector<int> &columns)
      : GlobalKeyframeUndo(frame) {
    tcg::substitute(
        m_columns,
        columns | ba::filtered(std::not1(boost::make_adaptable<bool, int>(
                      boost::bind(isKeyframe, frame, _1)))));
  }

  void redo() const override {
    doInsertGlobalKeyframes(m_frame, m_columns);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void undo() const override {
    doRemoveGlobalKeyframes(m_frame, m_columns);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  QString getHistoryString() override {
    return QObject::tr("Insert Multiple Keys  at Frame %1")
        .arg(QString::number(m_frame + 1));
  }
};

//-----------------------------------------------------------------------------

static void insertGlobalKeyframe(int frame) {
  std::vector<int> columns;
  ::getColumns(columns);

  if (columns.empty()) return;

  TUndo *undo = new InsertGlobalKeyframeUndo(frame, columns);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//=============================================================================

class InsertGlobalKeyframeCommand final : public MenuItemHandler {
public:
  InsertGlobalKeyframeCommand() : MenuItemHandler(MI_InsertGlobalKeyframe) {}
  void execute() override {
    int frame = TApp::instance()->getCurrentFrame()->getFrame();
    XshCmd::insertGlobalKeyframe(frame);
  }
} insertGlobalKeyframeCommand;

//*****************************************************************************
//    RemoveGlobalKeyframe  command
//*****************************************************************************

class RemoveGlobalKeyframeUndo final : public GlobalKeyframeUndo {
  std::vector<TStageObject::Keyframe> m_keyframes;

public:
  RemoveGlobalKeyframeUndo(int frame, const std::vector<int> &columns)
      : GlobalKeyframeUndo(frame) {
    struct locals {
      static TStageObject::Keyframe getKeyframe(int r, int c) {
        TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

        TStageObjectId objectId = (c == -1) ? TStageObjectId::CameraId(0)
                                            : TStageObjectId::ColumnId(c);

        TStageObject *object = xsh->getStageObject(objectId);
        assert(object);

        return object->getKeyframe(r);
      }
    };  // locals

    tcg::substitute(m_columns,
                    columns | ba::filtered(boost::bind(isKeyframe, frame, _1)));

    tcg::substitute(m_keyframes,
                    m_columns | ba::transformed(boost::bind(locals::getKeyframe,
                                                            frame, _1)));
  }

  void redo() const override {
    doRemoveGlobalKeyframes(m_frame, m_columns);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    int c, cCount = int(m_columns.size());
    for (c = 0; c != cCount; ++c) {
      int col = m_columns[c];

      TStageObjectId objectId = (col == -1) ? TStageObjectId::CameraId(0)
                                            : TStageObjectId::ColumnId(col);

      TStageObject *object = xsh->getStageObject(objectId);
      object->setKeyframeWithoutUndo(m_frame, m_keyframes[c]);
    }

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return GlobalKeyframeUndo::getSize() +
           m_keyframes.size() * sizeof(TStageObject::Keyframe);
  }

  QString getHistoryString() override {
    return QObject::tr("Remove Multiple Keys  at Frame %1")
        .arg(QString::number(m_frame + 1));
  }
};

//-----------------------------------------------------------------------------

static void removeGlobalKeyframe(int frame) {
  std::vector<int> columns;
  ::getColumns(columns);

  if (columns.empty()) return;

  TUndo *undo = new RemoveGlobalKeyframeUndo(frame, columns);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

//=============================================================================

class RemoveGlobalKeyframeCommand final : public MenuItemHandler {
public:
  RemoveGlobalKeyframeCommand() : MenuItemHandler(MI_RemoveGlobalKeyframe) {}
  void execute() override {
    int frame = TApp::instance()->getCurrentFrame()->getFrame();
    XshCmd::removeGlobalKeyframe(frame);
  }
} removeGlobalKeyframeCommand;

//============================================================
//	Drawing Substitution
//============================================================
class DrawingSubtitutionUndo final : public TUndo {
private:
  int m_direction, m_row, m_col;
  TCellSelection::Range m_range;
  bool m_selected;
  std::vector<std::pair<int, int>> emptyCells;

public:
  DrawingSubtitutionUndo(int dir, TCellSelection::Range range, int row, int col,
                         bool selected)
      : m_direction(dir)
      , m_range(range)
      , m_row(row)
      , m_col(col)
      , m_selected(selected) {
    TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();
    int tempCol, tempRow;
    int c = m_range.m_c0;
    int r = m_range.m_r0;
    while (c <= m_range.m_c1) {
      tempCol = c;
      while (r <= m_range.m_r1) {
        tempRow = r;
        if (xsh->getCell(tempRow, tempCol).isEmpty()) {
          emptyCells.push_back(std::make_pair(tempRow, tempCol));
        }
        r++;
      }
      r = m_range.m_r0;
      c++;
    }
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()->getXsheet();

    if (!m_selected) {
      changeDrawing(-m_direction, m_row, m_col);
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      return;
    }
    int col, row;
    int c = m_range.m_c0;
    int r = m_range.m_r0;
    while (c <= m_range.m_c1) {
      col = c;
      while (r <= m_range.m_r1) {
        row        = r;
        bool found = false;
        for (int i = 0; i < emptyCells.size(); i++) {
          if (emptyCells[i].first == row && emptyCells[i].second == col) {
            xsh->clearCells(row, col);
            found = true;
          }
        }
        if (found) {
          r++;
          continue;
        }
        changeDrawing(-m_direction, row, col);
        r++;
      }
      r = m_range.m_r0;
      c++;
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void redo() const override {
    if (!m_selected) {
      changeDrawing(m_direction, m_row, m_col);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      return;
    }

    int col, row;
    int c = m_range.m_c0;
    int r = m_range.m_r0;
    while (c <= m_range.m_c1) {
      col = c;
      while (r <= m_range.m_r1) {
        row = r;
        changeDrawing(m_direction, row, col);
        r++;
      }
      r = m_range.m_r0;
      c++;
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Change current drawing %1")
        .arg(QString::number(m_direction));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }

protected:
  static bool changeDrawing(int delta, int row, int col);
  static void setDrawing(const TFrameId &fid, int row, int col, TXshCell cell,
                         TXshLevel *level);
  friend class DrawingSubtitutionGroupUndo;
};

//============================================================

class DrawingSubtitutionGroupUndo final : public TUndo {
private:
  int m_direction;
  int m_row;
  int m_col;
  int m_count;

public:
  DrawingSubtitutionGroupUndo(int dir, int row, int col)
      : m_direction(dir), m_col(col), m_row(row) {
    m_count       = 1;
    TXshCell cell = TTool::getApplication()
                        ->getCurrentScene()
                        ->getScene()
                        ->getXsheet()
                        ->getCell(m_row, m_col);
    if (!cell.m_level ||
        !(cell.m_level->getSimpleLevel() || cell.m_level->getChildLevel() ||
          cell.m_level->getSoundTextLevel()))
      return;

    TFrameId id = cell.m_frameId;

    TXshCell nextCell = TTool::getApplication()
                            ->getCurrentScene()
                            ->getScene()
                            ->getXsheet()
                            ->getCell(m_row + m_count, m_col);
    if (!nextCell.m_level ||
        !(nextCell.m_level->getSimpleLevel() ||
          nextCell.m_level->getChildLevel() ||
          nextCell.m_level->getSoundTextLevel()))
      return;

    TFrameId nextId = nextCell.m_frameId;

    while (id == nextId) {
      m_count++;
      nextCell = TTool::getApplication()
                     ->getCurrentScene()
                     ->getScene()
                     ->getXsheet()
                     ->getCell(m_row + m_count, m_col);
      nextId = nextCell.m_frameId;
    }
  }

  void undo() const override {
    int n = 1;
    DrawingSubtitutionUndo::changeDrawing(-m_direction, m_row, m_col);
    while (n < m_count) {
      DrawingSubtitutionUndo::changeDrawing(-m_direction, m_row + n, m_col);
      n++;
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  void redo() const override {
    int n = 1;
    DrawingSubtitutionUndo::changeDrawing(m_direction, m_row, m_col);
    while (n < m_count) {
      DrawingSubtitutionUndo::changeDrawing(m_direction, m_row + n, m_col);
      n++;
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Change current drawing %1")
        .arg(QString::number(m_direction));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//============================================================

bool DrawingSubtitutionUndo::changeDrawing(int delta, int row, int col) {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  TXshCell cell           = xsh->getCell(row, col);
  bool usePrevCell        = false;
  if (cell.isEmpty()) {
    TXshCell prevCell = xsh->getCell(row - 1, col);
    if (prevCell.isEmpty() ||
        !(prevCell.m_level->getSimpleLevel() ||
          prevCell.m_level->getChildLevel() ||
          prevCell.m_level->getSoundTextLevel()))
      return false;
    cell        = prevCell;
    usePrevCell = true;
  } else if (!cell.m_level ||
             !(cell.m_level->getSimpleLevel() ||
               cell.m_level->getChildLevel() ||
               cell.m_level->getSoundTextLevel()))
    return false;
  TXshLevel *level  = cell.m_level->getSimpleLevel();
  if (!level) level = cell.m_level->getChildLevel();
  if (!level) level = cell.m_level->getSoundTextLevel();

  std::vector<TFrameId> fids;
  int framesTextSize = 0;
  int n, index;
  bool usingSoundText = false;
  TFrameId cellFrameId;
  if (cell.m_level->getSimpleLevel())
    cell.m_level->getSimpleLevel()->getFids(fids);
  if (cell.m_level->getChildLevel())
    cell.m_level->getChildLevel()->getFids(fids);
  if (cell.m_level->getSoundTextLevel()) {
    n              = cell.m_level->getSoundTextLevel()->m_framesText.size();
    usingSoundText = true;
  } else
    n = fids.size();

  if (n < 2) return false;

  if (!usingSoundText) {
    std::vector<TFrameId>::iterator it;
    it = std::find(fids.begin(), fids.end(), cell.m_frameId);

    if (it == fids.end()) return false;

    index = std::distance(fids.begin(), it);
  } else
    index = cell.getFrameId().getNumber();

  if (usePrevCell) {
    index -= delta;
    cell = xsh->getCell(row, col);
  }

  // if negative direction, add the size to the direction to avoid a negative
  // modulo
  while (delta < 0) delta += n;
  // the index is the remainder
  index = (index + delta) % n;
  assert(index < n);

  // sound text levels can't have a 0 index frame id
  // the index points to a qlist<qstring>
  // reading 1 below the frameid number
  // and you can't have a -1 index on a qlist
  if (usingSoundText && index == 0) index = n;

  if (!usingSoundText)
    cellFrameId = fids[index];
  else
    cellFrameId = TFrameId(index);

  setDrawing(cellFrameId, row, col, cell, level);

  return true;
}

void DrawingSubtitutionUndo::setDrawing(const TFrameId &fid, int row, int col,
                                        TXshCell cell, TXshLevel *level) {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  cell.m_frameId          = fid;
  cell.m_level            = level;
  xsh->setCell(row, col, cell);
  TStageObject *pegbar = xsh->getStageObject(TStageObjectId::ColumnId(col));
  pegbar->setOffset(pegbar->getOffset());
}

//-----------------------------------------------------------------------------

static void drawingSubstituion(int dir) {
  TCellSelection *selection = dynamic_cast<TCellSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  TCellSelection::Range range;
  bool selected = false;
  if (selection) {
    range                            = selection->getSelectedCells();
    if (!(range.isEmpty())) selected = true;
  }
  int row = TTool::getApplication()->getCurrentFrame()->getFrame();
  int col = TTool::getApplication()->getCurrentColumn()->getColumnIndex();

  DrawingSubtitutionUndo *undo =
      new DrawingSubtitutionUndo(dir, range, row, col, selected);
  TUndoManager::manager()->add(undo);

  undo->redo();
}

static void drawingSubstituionGroup(int dir) {
  int row = TTool::getApplication()->getCurrentFrame()->getFrame();
  int col = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
  TXshCell cell =
      TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getCell(
          row, col);
  bool isEmpty = cell.isEmpty();
  if (isEmpty) return;
  DrawingSubtitutionGroupUndo *undo =
      new DrawingSubtitutionGroupUndo(dir, row, col);
  TUndoManager::manager()->add(undo);
  undo->redo();
}

//=============================================================================

class DrawingSubstitutionForwardCommand final : public MenuItemHandler {
public:
  DrawingSubstitutionForwardCommand() : MenuItemHandler(MI_DrawingSubForward) {}
  void execute() override { XshCmd::drawingSubstituion(1); }
} DrawingSubstitutionForwardCommand;

//============================================================

class DrawingSubstitutionBackwardCommand final : public MenuItemHandler {
public:
  DrawingSubstitutionBackwardCommand()
      : MenuItemHandler(MI_DrawingSubBackward) {}
  void execute() override { XshCmd::drawingSubstituion(-1); }
} DrawingSubstitutionBackwardCommand;

//=============================================================================

class DrawingSubstitutionGroupForwardCommand final : public MenuItemHandler {
public:
  DrawingSubstitutionGroupForwardCommand()
      : MenuItemHandler(MI_DrawingSubGroupForward) {}
  void execute() override { XshCmd::drawingSubstituionGroup(1); }
} DrawingSubstitutionGroupForwardCommand;

//============================================================

class DrawingSubstitutionGroupBackwardCommand final : public MenuItemHandler {
public:
  DrawingSubstitutionGroupBackwardCommand()
      : MenuItemHandler(MI_DrawingSubGroupBackward) {}
  void execute() override { XshCmd::drawingSubstituionGroup(-1); }
} DrawingSubstitutionGroupBackwardCommand;

//============================================================

class NewNoteLevelUndo final : public TUndo {
  TXshSoundTextColumnP m_soundtextColumn;
  int m_col;
  QString m_columnName;

public:
  NewNoteLevelUndo(TXshSoundTextColumn *soundtextColumn, int col,
                   QString columnName)
      : m_soundtextColumn(soundtextColumn)
      , m_col(col)
      , m_columnName(columnName) {}

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    xsh->removeColumn(m_col);
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    xsh->insertColumn(m_col, m_soundtextColumn.getPointer());

    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(m_col));
    std::string str   = m_columnName.toStdString();
    obj->setName(str);

    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("New Note Level"); }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//============================================================

static void newNoteLevel() {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  int col = TTool::getApplication()->getCurrentColumn()->getColumnIndex();
  TXshSoundTextColumn *textSoundCol = new TXshSoundTextColumn();

  textSoundCol->setXsheet(xsh);
  QList<QString> textList;
  textList << " ";
  textSoundCol->createSoundTextLevel(0, textList);
  xsh->insertColumn(col, textSoundCol);

  // name the level a unique NoteLevel number
  TStageObjectTree *objects = xsh->getStageObjectTree();
  int objectCount           = objects->getStageObjectCount();
  int maxTextColumns        = 1;
  for (int i = 0; i < objectCount; i++) {
    TStageObject *object = objects->getStageObject(i);
    std::string objName  = object->getName();
    int pos              = objName.find("NoteLevel");
    if (pos != std::string::npos && pos == 0) {
      std::string currStrCount = objName.substr(9);
      bool ok;
      int currCount = QString::fromStdString(currStrCount).toInt(&ok);
      if (ok && currCount >= maxTextColumns) {
        maxTextColumns = currCount + 1;
      }
    }
  }
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(col));
  QString str       = "NoteLevel" + QString::number(maxTextColumns);
  obj->setName(str.toStdString());

  TUndoManager::manager()->add(new NewNoteLevelUndo(textSoundCol, col, str));

  TXsheetHandle *xshHandle = app->getCurrentXsheet();
  xshHandle->notifyXsheetChanged();
}

//============================================================

class NewNoteLevelCommand final : public MenuItemHandler {
public:
  NewNoteLevelCommand() : MenuItemHandler(MI_NewNoteLevel) {}
  void execute() override { XshCmd::newNoteLevel(); }
} NewNoteLevelCommand;

//============================================================

static void removeEmptyColumns() {
  TTool::Application *app = TTool::getApplication();
  TXsheet *xsh            = app->getCurrentScene()->getScene()->getXsheet();
  std::set<int> indices;

  for (int i = 0; i < xsh->getColumnCount(); i++) {
    TXshColumn *column = xsh->getColumn(i);
    if (!column || column->isEmpty()) indices.insert(i);
  }

  if (indices.size()) ColumnCmd::deleteColumns(indices, false, false);

  app->getCurrentXsheet()->notifyXsheetChanged();
}

class RemoveEmptyColumnsCommand final : public MenuItemHandler {
public:
  RemoveEmptyColumnsCommand() : MenuItemHandler(MI_RemoveEmptyColumns) {}
  void execute() override { XshCmd::removeEmptyColumns(); }
} RemoveEmptyColumnsCommand;

//============================================================

}  // namespace XshCmd

//*****************************************************************************
//    Selection  commands
//*****************************************************************************

class SelectRowKeyframesCommand final : public MenuItemHandler {
public:
  SelectRowKeyframesCommand() : MenuItemHandler(MI_SelectRowKeyframes) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int row = app->getCurrentFrame()->getFrame();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = -1; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      if (pegbar->isKeyframe(row)) selection->select(row, col);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

} SelectRowKeyframesCommand;

//-----------------------------------------------------------------------------

class SelectColumnKeyframesCommand final : public MenuItemHandler {
public:
  SelectColumnKeyframesCommand() : MenuItemHandler(MI_SelectColumnKeyframes) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int col                 = app->getCurrentColumn()->getColumnIndex();
    TStageObjectId objectId = app->getCurrentObject()->getObjectId();
    if (app->getCurrentObject()->getObjectId() == TStageObjectId::CameraId(0)) {
      objectId = TStageObjectId::CameraId(0);
      col      = -1;
    }
    selection->selectNone();
    ToonzScene *scene    = app->getCurrentScene()->getScene();
    TXsheet *xsh         = scene->getXsheet();
    TStageObject *pegbar = xsh->getStageObject(objectId);
    TStageObject::KeyframeMap keyframes;
    pegbar->getKeyframes(keyframes);
    for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
         it != keyframes.end(); ++it)
      selection->select(it->first, col);

    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectColumnKeyframesCommand;

//-----------------------------------------------------------------------------

class SelectAllKeyframesCommand final : public MenuItemHandler {
public:
  SelectAllKeyframesCommand() : MenuItemHandler(MI_SelectAllKeyframes) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = -1; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        int row = it->first;
        assert(pegbar->isKeyframe(row));
        selection->select(row, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectAllKeyframesCommand;

//-----------------------------------------------------------------------------

class SelectAllKeyframesBeforeCommand final : public MenuItemHandler {
public:
  SelectAllKeyframesBeforeCommand()
      : MenuItemHandler(MI_SelectAllKeyframesNotBefore) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow = app->getCurrentFrame()->getFrame();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    for (int col = -1; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        int row = it->first;
        if (row < currentRow) continue;
        assert(pegbar->isKeyframe(row));
        selection->select(row, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectAllKeyframesBeforeCommand;

//-----------------------------------------------------------------------------

class SelectAllKeyframesAfterCommand final : public MenuItemHandler {
public:
  SelectAllKeyframesAfterCommand()
      : MenuItemHandler(MI_SelectAllKeyframesNotAfter) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow = app->getCurrentFrame()->getFrame();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = -1; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        int row = it->first;
        if (row > currentRow) continue;
        assert(pegbar->isKeyframe(row));
        selection->select(row, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectAllKeyframesAfterCommand;

//-----------------------------------------------------------------------------

class SelectPreviousKeysInColumnCommand final : public MenuItemHandler {
public:
  SelectPreviousKeysInColumnCommand()
      : MenuItemHandler(MI_SelectPreviousKeysInColumn) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow    = app->getCurrentFrame()->getFrame();
    int currentColumn = app->getCurrentColumn()->getColumnIndex();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();

    TStageObjectId objectId =
        TApp::instance()->getCurrentObject()->getObjectId();
#ifdef LINETEST
    if (objectId == TStageObjectId::CameraId(0)) currentColumn = -1;
#endif
    TStageObject *pegbar = xsh->getStageObject(objectId);
    TStageObject::KeyframeMap keyframes;
    pegbar->getKeyframes(keyframes);
    for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
         it != keyframes.end(); ++it) {
      int row = it->first;
      if (row > currentRow) continue;
      assert(pegbar->isKeyframe(row));
      selection->select(row, currentColumn);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectPreviousKeysInColumnCommand;

//-----------------------------------------------------------------------------

class SelectFollowingKeysInColumnCommand final : public MenuItemHandler {
public:
  SelectFollowingKeysInColumnCommand()
      : MenuItemHandler(MI_SelectFollowingKeysInColumn) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow    = app->getCurrentFrame()->getFrame();
    int currentColumn = app->getCurrentColumn()->getColumnIndex();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    TStageObjectId objectId =
        TApp::instance()->getCurrentObject()->getObjectId();
#ifdef LINETEST
    if (objectId == TStageObjectId::CameraId(0)) currentColumn = -1;
#endif
    TStageObject *pegbar = xsh->getStageObject(objectId);
    TStageObject::KeyframeMap keyframes;
    pegbar->getKeyframes(keyframes);
    for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
         it != keyframes.end(); ++it) {
      int row = it->first;
      if (row < currentRow) continue;
      assert(pegbar->isKeyframe(row));
      selection->select(row, currentColumn);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectFollowingKeysInColumnCommand;

//-----------------------------------------------------------------------------

class SelectPreviousKeysInRowCommand final : public MenuItemHandler {
public:
  SelectPreviousKeysInRowCommand()
      : MenuItemHandler(MI_SelectPreviousKeysInRow) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow    = app->getCurrentFrame()->getFrame();
    int currentColumn = app->getCurrentColumn()->getColumnIndex();

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = -1; col <= currentColumn; col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        int row = it->first;
        if (row != currentRow) continue;
        assert(pegbar->isKeyframe(row));
        selection->select(row, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectPreviousKeysInRowCommand;

//-----------------------------------------------------------------------------

class SelectFollowingKeysInRowCommand final : public MenuItemHandler {
public:
  SelectFollowingKeysInRowCommand()
      : MenuItemHandler(MI_SelectFollowingKeysInRow) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;
    int currentRow    = app->getCurrentFrame()->getFrame();
    int currentColumn = app->getCurrentColumn()->getColumnIndex();

    TStageObjectId objectId =
        TApp::instance()->getCurrentObject()->getObjectId();
#ifdef LINETEST
    if (objectId == TStageObjectId::CameraId(0)) currentColumn = -1;
#endif

    selection->selectNone();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = currentColumn; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
      if (col == -1)
        objectId = TStageObjectId::CameraId(0);
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        int row = it->first;
        if (row != currentRow) continue;
        assert(pegbar->isKeyframe(row));
        selection->select(row, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} SelectFollowingKeysInRowCommand;

//-----------------------------------------------------------------------------

class InvertKeyframeSelectionCommand final : public MenuItemHandler {
public:
  InvertKeyframeSelectionCommand()
      : MenuItemHandler(MI_InvertKeyframeSelection) {}

  void execute() override {
    TApp *app                     = TApp::instance();
    TKeyframeSelection *selection = dynamic_cast<TKeyframeSelection *>(
        app->getCurrentSelection()->getSelection());
    if (!selection) return;

    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int col;
    for (col = -1; col < xsh->getColumnCount(); col++) {
      TStageObjectId objectId;
#ifdef LINETEST
      if (col == -1) objectId = TStageObjectId::CameraId(0);
#else
      if (col == -1) continue;
#endif
      else
        objectId           = TStageObjectId::ColumnId(col);
      TStageObject *pegbar = xsh->getStageObject(objectId);
      TStageObject::KeyframeMap keyframes;
      pegbar->getKeyframes(keyframes);
      for (TStageObject::KeyframeMap::iterator it = keyframes.begin();
           it != keyframes.end(); ++it) {
        assert(pegbar->isKeyframe(it->first));
        if (selection->isSelected(it->first, col))
          selection->unselect(it->first, col);
        else
          selection->select(it->first, col);
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
} InvertKeyframeSelectionCommand;

//*****************************************************************************
//    Kayframe Handles  commands
//*****************************************************************************

namespace {

class KeyFrameHandleCommandUndo final : public TUndo {
  TStageObjectId m_objId;
  int m_rowFirst, m_rowSecond;

  TStageObject::Keyframe m_oldKeyframeFirst, m_oldKeyframeSecond,
      m_newKeyframeFirst, m_newKeyframeSecond;

public:
  KeyFrameHandleCommandUndo(TStageObjectId id, int rowFirst, int rowSecond)
      : m_objId(id), m_rowFirst(rowFirst), m_rowSecond(rowSecond) {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);

    m_oldKeyframeFirst  = pegbar->getKeyframe(m_rowFirst);
    m_oldKeyframeSecond = pegbar->getKeyframe(m_rowSecond);
  }

  void onAdd() override {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);

    m_newKeyframeFirst  = pegbar->getKeyframe(m_rowFirst);
    m_newKeyframeSecond = pegbar->getKeyframe(m_rowSecond);
  }

  void setKeyframes(const TStageObject::Keyframe &k0,
                    const TStageObject::Keyframe &k1) const {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);

    pegbar->setKeyframeWithoutUndo(m_rowFirst, k0);
    pegbar->setKeyframeWithoutUndo(m_rowSecond, k1);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

  void redo() const override {
    setKeyframes(m_newKeyframeFirst, m_newKeyframeSecond);
  }
  void undo() const override {
    setKeyframes(m_oldKeyframeFirst, m_oldKeyframeSecond);
  }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Set Keyframe : %1")
        .arg(QString::fromStdString(m_objId.toString()));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//-----------------------------------------------------------------------------

class SetAccelerationCommand final : public MenuItemHandler {
public:
  SetAccelerationCommand() : MenuItemHandler(MI_SetAcceleration) {}

  void execute() override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int row      = app->getCurrentFrame()->getFrame();

    TStageObjectId objectId = app->getCurrentObject()->getObjectId();
    TStageObject *pegbar    = xsh->getStageObject(objectId);

    if (!pegbar) return;

    int r0, r1;
    double ease0, ease1;

    pegbar->getKeyframeSpan(row, r0, ease0, r1, ease1);

    std::unique_ptr<TUndo> undo(
        new KeyFrameHandleCommandUndo(objectId, r0, r1));

    TStageObject::Keyframe keyframe0 = pegbar->getKeyframe(r0);
    TStageObject::Keyframe keyframe1 = pegbar->getKeyframe(r1);

    int dr = std::max(r1 - r0, 0);
    if (keyframe0.m_easeOut == dr) return;

    keyframe0.m_easeOut = dr;
    keyframe1.m_easeIn  = 0;

    // The following TStageObject::setKeyframeWithoutUndo()s could probably
    // be left to the undo->redo(). I would have to inquire further. No big deal
    // anyway.
    pegbar->setKeyframeWithoutUndo(r0, keyframe0);
    pegbar->setKeyframeWithoutUndo(r1, keyframe1);

    TUndoManager::manager()->add(
        undo.release());  // Stores set keyframes in the undo

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

} SetAccelerationCommand;

//-----------------------------------------------------------------------------

class SetDecelerationCommand final : public MenuItemHandler {
public:
  SetDecelerationCommand() : MenuItemHandler(MI_SetDeceleration) {}

  void execute() override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int row      = app->getCurrentFrame()->getFrame();

    TStageObjectId objectId = app->getCurrentObject()->getObjectId();
    TStageObject *pegbar    = xsh->getStageObject(objectId);

    if (!pegbar) return;

    int r0, r1;
    double ease0, ease1;

    pegbar->getKeyframeSpan(row, r0, ease0, r1, ease1);

    std::unique_ptr<TUndo> undo(
        new KeyFrameHandleCommandUndo(objectId, r0, r1));

    TStageObject::Keyframe keyframe0 = pegbar->getKeyframe(r0);
    TStageObject::Keyframe keyframe1 = pegbar->getKeyframe(r1);

    int dr = std::max(r1 - r0, 0);
    if (keyframe1.m_easeIn == dr) return;

    keyframe0.m_easeOut = 0;
    keyframe1.m_easeIn  = dr;

    pegbar->setKeyframeWithoutUndo(r0, keyframe0);
    pegbar->setKeyframeWithoutUndo(r1, keyframe1);

    TUndoManager::manager()->add(undo.release());

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

} SetDecelerationCommand;

//-----------------------------------------------------------------------------

class SetConstantSpeedCommand final : public MenuItemHandler {
public:
  SetConstantSpeedCommand() : MenuItemHandler(MI_SetConstantSpeed) {}

  void execute() override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int row      = app->getCurrentFrame()->getFrame();

    TStageObjectId objectId = app->getCurrentObject()->getObjectId();
    TStageObject *pegbar    = xsh->getStageObject(objectId);

    if (!pegbar) return;

    int r0, r1;
    double ease0, ease1;

    pegbar->getKeyframeSpan(row, r0, ease0, r1, ease1);

    KeyFrameHandleCommandUndo *undo =
        new KeyFrameHandleCommandUndo(objectId, r0, r1);

    TStageObject::Keyframe keyframe0 = pegbar->getKeyframe(r0);
    TStageObject::Keyframe keyframe1 = pegbar->getKeyframe(r1);

    keyframe0.m_easeOut = 0;
    keyframe1.m_easeIn  = 0;

    pegbar->setKeyframeWithoutUndo(r0, keyframe0);
    pegbar->setKeyframeWithoutUndo(r1, keyframe1);

    TUndoManager::manager()->add(undo);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

} SetConstantSpeedCommand;

//-----------------------------------------------------------------------------

class ResetArrowCommand final : public MenuItemHandler {
public:
  ResetArrowCommand() : MenuItemHandler(MI_ResetInterpolation) {}

  void execute() override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int row      = app->getCurrentFrame()->getFrame();

    TStageObjectId objectId = app->getCurrentObject()->getObjectId();
    TStageObject *pegbar    = xsh->getStageObject(objectId);
    if (!pegbar) return;

    int r0, r1;
    double ease0, ease1;

    pegbar->getKeyframeSpan(row, r0, ease0, r1, ease1);

    KeyFrameHandleCommandUndo *undo =
        new KeyFrameHandleCommandUndo(objectId, r0, r1);

    TStageObject::Keyframe k0 = pegbar->getKeyframe(r0);
    TStageObject::Keyframe k1 = pegbar->getKeyframe(r1);

    k0.m_easeOut = (k0.m_easeOut != 0) ? 1 : k0.m_easeOut;
    k0.m_easeIn  = (k0.m_easeIn != 0) ? 1 : k0.m_easeIn;
    k1.m_easeOut = (k1.m_easeOut != 0) ? 1 : k1.m_easeOut;
    k1.m_easeIn  = (k1.m_easeIn != 0) ? 1 : k1.m_easeIn;

    pegbar->setKeyframeWithoutUndo(r0, k0);
    pegbar->setKeyframeWithoutUndo(r1, k1);

    TUndoManager::manager()->add(undo);

    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

} ResetArrowCommand;

//===========================================================
//    To Be Reworked
//===========================================================

class NewOutputFx final : public MenuItemHandler {
public:
  NewOutputFx() : MenuItemHandler(MI_NewOutputFx) {}

  void execute() override {
    TApp *app = TApp::instance();
    TFxCommand::createOutputFx(app->getCurrentXsheet(),
                               app->getCurrentFx()->getFx());
  }

} newOutputFx;

namespace {
int columnsPerPage = 10000;
int rowsPerPage    = 10000;
std::vector<std::pair<std::string, string>> infos;

void readParameters() {
  infos.clear();
  const std::string name("xsheet_html.xml");
  TFilePath fp = ToonzFolder::getModuleFile(name);
  if (!TFileStatus(fp).doesExist()) return;
  try {
    TIStream is(fp);
    std::string tagName;
    if (!is.matchTag(tagName) || tagName != "xsheet_html") return;

    while (is.matchTag(tagName)) {
      if (tagName == "page") {
        std::string s;
        s                                       = is.getTagAttribute("rows");
        if (s != "" && isInt(s)) rowsPerPage    = std::stoi(s);
        s                                       = is.getTagAttribute("columns");
        if (s != "" && isInt(s)) columnsPerPage = std::stoi(s);
      } else if (tagName == "info") {
        std::string name  = is.getTagAttribute("name");
        std::string value = is.getTagAttribute("value");
        infos.push_back(std::make_pair(name, value));
      } else
        return;
    }
  } catch (...) {
  }
}

void copyCss(TFilePath fp) {
  const std::string name("xsheet.css");
  TFilePath cssFp = fp.getParentDir() + name;
  if (TFileStatus(cssFp).doesExist()) return;
  TFilePath src = ToonzFolder::getModuleFile(name);
  if (TFileStatus(src).doesExist()) {
    try {
      TSystem::copyFile(cssFp, src);
    } catch (...) {
    }
  }
}

void getAllChildLevels(std::vector<TXshChildLevel *> &levels,
                       ToonzScene *scene) {
  std::set<TXsheet *> visited, toVisit;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  visited.insert(xsh);
  toVisit.insert(xsh);
  while (!toVisit.empty()) {
    xsh = *toVisit.begin();
    toVisit.erase(xsh);
    for (int i = 0; i < xsh->getColumnCount(); i++) {
      TXshColumn *column = xsh->getColumn(i);
      if (!column) continue;
      if (TXshCellColumn *cc = column->getCellColumn()) {
        int r0 = 0, r1 = -1;
        cc->getRange(r0, r1);
        if (!cc->isEmpty() && r0 <= r1) {
          for (int r = r0; r <= r1; r++) {
            TXshCell cell = cc->getCell(r);
            if (cell.m_level && cell.m_level->getChildLevel()) {
              TXsheet *subxsh = cell.m_level->getChildLevel()->getXsheet();
              if (visited.find(subxsh) == visited.end()) {
                levels.push_back(cell.m_level->getChildLevel());
                visited.insert(subxsh);
                toVisit.insert(subxsh);
              }
            }
          }
        }
      }
    }
  }
}

}  // namespace

struct NumericColumn {
  TStageObject *m_pegbar;
  TDoubleParamP m_curve;
  NumericColumn(TStageObject *pegbar, const TDoubleParamP &curve)
      : m_pegbar(pegbar), m_curve(curve){};
};

class XsheetWriter {
  TXsheet *m_xsh;
  std::map<TXshChildLevel *, int> m_childTable;
  std::vector<NumericColumn> m_numericColumns;

public:
  XsheetWriter(ToonzScene *scene);

  void setXsheet(TXsheet *xsh);
  void getSubXsheets(std::vector<TXsheet *> &xsheets);
  int getChildLevelIndex(TXshChildLevel *);

  void buildNumericColumns();
  void buildChildTable(ToonzScene *scene);

  void tableCaption(ostream &os);

  void columnHeader(ostream &os, int c);
  void numericColumnHeader(ostream &os, int c);
  void cell(ostream &os, int r, int c);
  void numericCell(ostream &os, int r, int c);

  void write(ostream &os);
};

XsheetWriter::XsheetWriter(ToonzScene *scene) : m_xsh(0) {
  buildChildTable(scene);
  setXsheet(scene->getXsheet());
}

void XsheetWriter::setXsheet(TXsheet *xsh) {
  m_xsh = xsh;
  buildNumericColumns();
}

void XsheetWriter::getSubXsheets(std::vector<TXsheet *> &xsheets) {
  std::map<TXshChildLevel *, int>::iterator it;
  for (it = m_childTable.begin(); it != m_childTable.end(); ++it)
    xsheets.push_back(it->first->getXsheet());
}

int XsheetWriter::getChildLevelIndex(TXshChildLevel *cl) {
  std::map<TXshChildLevel *, int>::iterator it;
  it = m_childTable.find(cl);
  return it == m_childTable.end() ? -1 : it->second;
}

void XsheetWriter::buildNumericColumns() {
  m_numericColumns.clear();
  TStageObjectTree *pegbarTree = m_xsh->getStageObjectTree();
  for (int i = 0; i < pegbarTree->getStageObjectCount(); i++) {
    TStageObject *pegbar = pegbarTree->getStageObject(i);
    for (int j = 0; j < TStageObject::T_ChannelCount; j++) {
      TDoubleParamP curve = pegbar->getParam((TStageObject::Channel)j);
      if (curve->hasKeyframes()) {
        m_numericColumns.push_back(NumericColumn(pegbar, curve));
      }
    }
  }
}

void XsheetWriter::buildChildTable(ToonzScene *scene) {
  std::vector<TXshChildLevel *> levels;
  getAllChildLevels(levels, scene);
  int i, k = 0;
  for (i = 0; i < (int)levels.size(); i++) m_childTable[levels[i]] = k++;
}

void XsheetWriter::tableCaption(ostream &os) { os << "<p>&nbsp;</p>\n"; }

void XsheetWriter::columnHeader(ostream &os, int c) {
  os << "  <th>" << (c + 1) << "</th>" << endl;
}

void XsheetWriter::numericColumnHeader(ostream &os, int c) {
  std::string pegbarName = m_numericColumns[c].m_pegbar->getName();
  std::string curveName =
      m_numericColumns[c]
          .m_curve
          ->getName();  // toString(TStringTable::translate(m_numericColumns[c].m_curve->getName()));
  os << "  <th class='" << (c > 0 ? "numeric" : "first_numeric") << "'>";
  os << pegbarName << "<br>" << curveName;
  os << "</th>" << endl;
}

void XsheetWriter::cell(ostream &os, int r, int c) {
  TXshCell prevCell;
  if (r > 0) prevCell = m_xsh->getCell(r - 1, c);
  TXshCell cell       = m_xsh->getCell(r, c);
  if (cell.isEmpty())
    os << "<td class='emptycell'>&nbsp;</td>";
  else {
    TXshLevel *level = cell.m_level.getPointer();
    std::string type = "levelcell";
    if (level->getChildLevel())
      type = "subxsheetcell";
    else if (level->getZeraryFxLevel())
      type = "fxcell";

    os << "<td class='" << type << "'>";
    bool newPage = r % rowsPerPage == 0;
    if (cell.m_level.getPointer() == prevCell.m_level.getPointer() &&
        !newPage) {
      // stesso livello
      if (cell.m_frameId == prevCell.m_frameId) {
        // stesso frame
        os << "|";
      } else {
        // frame diverso
        os << cell.m_frameId.getNumber();
      }
    } else {
      // livello diverso

      std::string levelName;
      if (level->getChildLevel()) {
        int index = getChildLevelIndex(level->getChildLevel());
        levelName = index >= 0 ? "Sub" + std::to_string(index + 1) : "";
      } else
        levelName = ::to_string(level->getName());
      os << levelName << " " << cell.m_frameId.getNumber();
    }
    os << "</td>" << endl;
  }
}

void XsheetWriter::numericCell(ostream &os, int r, int c) {
  TDoubleParamP curve = m_numericColumns[c].m_curve;

  double v          = curve->getValue(r);
  TMeasure *measure = curve->getMeasure();
  if (measure) {
    const TUnit *unit = measure->getCurrentUnit();
    if (unit) v       = unit->convertTo(v);
  }

  os << "<td class='" << (c > 0 ? "numeric" : "first_numeric") << "'>";
  os << v << "</td>" << endl;
}

void XsheetWriter::write(ostream &os) {
  int rowCount    = m_xsh->getFrameCount();
  int colCount    = m_xsh->getColumnCount();
  int totColCount = colCount + (int)m_numericColumns.size();
  int c0, c1;
  c0 = 0;
  for (;;) {
    c1      = std::min(totColCount, c0 + columnsPerPage) - 1;
    int ca0 = 0, ca1 = -1, cb0 = 0, cb1 = -1;
    if (c0 < colCount) {
      ca0 = c0;
      ca1 = std::min(colCount - 1, c1);
    }
    if (c1 >= colCount) {
      cb0 = std::max(c0, colCount);
      cb1 = c1;
    }

    int r0, r1, r, c;
    r0 = 0;
    for (;;) {
      r1 = std::min(rowCount, r0 + rowsPerPage) - 1;
      tableCaption(os);
      os << "<table>" << endl << "<tr>" << endl;
      if (c0 == 0) os << "  <th>&nbsp;</th>" << endl;
      for (c = c0; c <= c1; c++) {
        if (c < colCount)
          columnHeader(os, c);
        else
          numericColumnHeader(os, c - colCount);
      }
      os << "</tr>" << endl;
      for (r = r0; r <= r1; r++) {
        os << "<tr>" << endl;
        os << "  <th class='frame'>" << r + 1 << "</th>" << endl;

        for (c = ca0; c <= ca1; c++) cell(os, r, c);
        for (c = cb0; c <= cb1; c++) numericCell(os, r, c - colCount);
        os << "</tr>" << endl;
      }
      os << "</table>" << endl;
      r0 = r1 + 1;
      if (r0 >= rowCount) break;
    }
    c0 = c1 + 1;
    if (c0 >= totColCount) break;
  }
}

static void makeHtml(TFilePath fp) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  std::string sceneName   = scene->getScenePath().getName();
  std::string projectName = ::to_string(scene->getProject()->getName());

  Tofstream os(fp);

  os << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" " << endl;
  os << "  \"http://www.w3.org/TR/html4/strict.dtd\">" << endl;
  os << "<html><head>" << endl;
  os << "<title>" << sceneName << "</title>" << endl;
  os << "<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">"
     << endl;
  os << "<meta http-equiv=\"Content-Style-Type\" content=\"text/css\">" << endl;
  os << "<meta name=\"Generator\" content=\"Toonz 5.2\">" << endl;
  os << "<link rel=\"stylesheet\" type=\"text/css\" href=\"xsheet.css\">"
     << endl;
  os << "</head><body>" << endl;
  os << "<table class='header'>" << endl;
  for (int k = 0; k < (int)infos.size(); k++)
    os << "<tr><th>" << infos[k].first << ":</th><td>" << infos[k].second
       << "</td></tr>" << endl;
  os << "<tr><th>Project:</th><td>" << projectName << "</td></tr>" << endl;
  os << "<tr><th>Scene:</th><td>" << sceneName << "</td></tr>" << endl;
  os << "<tr><th>Frames:</th><td>" << scene->getFrameCount() << "</td></tr>"
     << endl;
  os << "</table>\n";
  os << "<p>&nbsp;</p>\n";

  os << "<h2>Main Xsheet</h2>\n";

  XsheetWriter writer(scene);
  writer.write(os);

  std::vector<TXsheet *> subXsheets;
  writer.getSubXsheets(subXsheets);
  int i;
  for (i = 0; i < (int)subXsheets.size(); i++) {
    os << "<h2>Sub Xsheet " << i + 1 << "</h2>\n";
    writer.setXsheet(subXsheets[i]);
    writer.write(os);
  }

  os << "<h2>Levels</h2>\n";
  std::vector<TXshLevel *> levels;
  scene->getLevelSet()->listLevels(levels);
  os << "<dl>" << endl;
  for (i = 0; i < (int)levels.size(); i++) {
    TXshLevel *level = levels[i];
    if (!level->getSimpleLevel()) continue;
    os << "<dt>" << ::to_string(level->getName()) << "</dt>" << endl;
    os << "<dd>" << endl;
    TFilePath fp = level->getPath();
    os << ::to_string(fp);
    TFilePath fp2 = scene->decodeFilePath(fp);
    if (fp != fp2) os << "<br>" << ::to_string(fp2);
    os << "</dd>" << endl;
  }
  os << "</dl>" << endl;
  os << "</body></html>" << endl;
}

class PrintXsheetCommand final : public MenuItemHandler {
public:
  PrintXsheetCommand() : MenuItemHandler(MI_PrintXsheet) {}
  void execute() override;
} printXsheetCommand;

void PrintXsheetCommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath fp      = scene->getScenePath().withType("html");
  readParameters();
  makeHtml(fp);
  copyCss(fp);

  QString str =
      QObject::tr("The %1 file has been generated").arg(toQString(fp));
  DVGui::warning(str);

  TSystem::showDocument(fp);
}
