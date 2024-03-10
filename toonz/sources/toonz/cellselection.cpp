

#include "cellselection.h"

// Tnz6 includes
#include "celldata.h"
#include "keyframeselection.h"
#include "keyframedata.h"
#include "drawingdata.h"
#include "cellkeyframedata.h"
#include "filmstripcommand.h"
#include "menubarcommandids.h"
#include "timestretchpopup.h"
#include "tapp.h"
#include "xsheetviewer.h"
#include "levelcommand.h"
#include "columncommand.h"

// TnzTools includes
#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/strokesdata.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/palettecontroller.h"
#include "toonz/preferences.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"
#include "toonz/levelproperties.h"
#include "toonz/toonzimageutils.h"
#include "toonz/trasterimageutils.h"
#include "toonz/levelset.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/stage.h"
#include "vectorizerpopup.h"
#include "tools/rasterselection.h"
#include "tools/strokeselection.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonz/tstageobjectcmd.h"

// TnzCore includes
#include "timagecache.h"
#include "tundo.h"
#include "tropcm.h"
#include "tvectorimage.h"
#include "tsystem.h"
#include "filebrowsermodel.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

bool containsOnlyOneRasterLevel(int r0, int c0, int r1, int c1) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int r, c;
  TXshLevelP xl = xsh->getCell(r0, c0).m_level;
  for (r = r0; r <= r1; r++) {
    for (c = c0; c <= c1; c++)
      if (xsh->getCell(r, c).m_level.getPointer() != xl.getPointer())
        return false;
  }
  return xl && (xl->getType() == TZP_XSHLEVEL ||
                xl->getType() == OVL_XSHLEVEL || xl->getType() == TZI_XSHLEVEL);
}

//-----------------------------------------------------------------------------

void copyCellsWithoutUndo(int r0, int c0, int r1, int c1) {
  int colCount = c1 - c0 + 1;
  int rowCount = r1 - r0 + 1;
  if (colCount <= 0 || rowCount <= 0) return;
  TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
  TCellData *data = new TCellData();
  data->setCells(xsh, r0, c0, r1, c1);
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

bool pasteCellsWithoutUndo(const TCellData *cellData, int &r0, int &c0, int &r1,
                           int &c1, bool insert = true,
                           bool doZeraryClone = true) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (!cellData) return false;
  if (r0 < 0 || c0 < 0) return false;

  bool ret =
      cellData->getCells(xsh, r0, c0, r1, c1, insert, doZeraryClone, false);
  if (!ret) return false;

  return true;
}

//-----------------------------------------------------------------------------

void deleteCellsWithoutUndo(int &r0, int &c0, int &r1, int &c1, bool doShift) {
  try {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int c;
    for (c = c0; c <= c1; c++) {
      if (xsh->isColumnEmpty(c)) continue;

      if (doShift)
        xsh->removeCells(r0, c, r1 - r0 + 1);
      else
        xsh->clearCells(r0, c, r1 - r0 + 1);
      // when the column becomes empty after deletion,
      // ColumnCmd::DeleteColumn() will take care of column related operations
      // like disconnecting from fx nodes etc.
      /*
      TXshColumn* column = xsh->getColumn(c);
      if (column && column->isEmpty()) {
        column->resetColumnProperties();
        TFx *fx = column->getFx();
        if (fx) {
          int i;
          for (i = fx->getOutputConnectionCount() - 1; i >= 0; i--) {
            TFxPort *port = fx->getOutputConnection(i);
            port->setFx(0);
          }
        }
      }
      */
    }
  } catch (...) {
    DVGui::error(QObject::tr("It is not possible to delete the selection."));
  }
}

//-----------------------------------------------------------------------------

void cutCellsWithoutUndo(int &r0, int &c0, int &r1, int &c1) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int c;
  for (c = c0; c <= c1; c++) {
    xsh->removeCells(r0, c, r1 - r0 + 1);
    // when the column becomes empty after deletion,
    // ColumnCmd::DeleteColumn() will take care of column related operations
    // like disconnecting from fx nodes etc.
    /*
    TXshColumn* column = xsh->getColumn(c);
    if (column && column->isEmpty()) {
      column->resetColumnProperties();
      TFx* fx = column->getFx();
      if (!fx) continue;
      int i;
      for (i = fx->getOutputConnectionCount() - 1; i >= 0; i--) {
        TFxPort* port = fx->getOutputConnection(i);
        port->setFx(0);
      }
    }
    */
  }

  // Se la selezione corrente e' TCellSelection svuoto la selezione.
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (cellSelection) cellSelection->selectNone();
}

//-----------------------------------------------------------------------------
// check if the operation may remove expression reference as column becomes
// empty and deleted after the operation. return true to continue the operation.

bool checkColumnRemoval(const int r0, const int c0, const int r1, const int c1,
                        std::set<int> &removedColIds) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // std::set<int> colIndicesToBeRemoved;
  for (int c = c0; c <= c1; c++) {
    TXshColumnP column = xsh->getColumn(c);
    if (!column || column->isEmpty() || column->isLocked()) continue;
    int tmp_r0, tmp_r1;
    xsh->getCellRange(c, tmp_r0, tmp_r1);
    if (r0 <= tmp_r0 && r1 >= tmp_r1) removedColIds.insert(c);
  }

  if (removedColIds.empty() ||
      !Preferences::instance()->isModifyExpressionOnMovingReferencesEnabled())
    return true;
  std::set<TFx *> dummy_fxs;
  return ColumnCmd::checkExpressionReferences(removedColIds, dummy_fxs, true);
}

//=============================================================================
//  PasteCellsUndo
//-----------------------------------------------------------------------------

class PasteCellsUndo final : public TUndo {
  TCellSelection *m_oldSelection;
  TCellSelection *m_newSelection;
  TCellData *m_data;
  std::vector<bool> m_areOldColumnsEmpty;
  bool m_containsSoundColumn;
  std::map<int, std::string> m_oldEmptyColumnNames;

public:
  PasteCellsUndo(int r0, int c0, int r1, int c1, int oldR0, int oldC0,
                 int oldR1, int oldC1, const std::vector<bool> &areColumnsEmpty,
                 bool containsSoundColumn,
                 const std::map<int, std::string> &oldEmptyColumnNames =
                     std::map<int, std::string>())
      : m_areOldColumnsEmpty(areColumnsEmpty)
      , m_containsSoundColumn(containsSoundColumn)
      , m_oldEmptyColumnNames(oldEmptyColumnNames) {
    m_data       = new TCellData();
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    m_data->setCells(xsh, r0, c0, r1, c1);
    m_newSelection = new TCellSelection();
    m_newSelection->selectCells(r0, c0, r1, c1);
    m_oldSelection = new TCellSelection();
    m_oldSelection->selectCells(oldR0, oldC0, oldR1, oldC1);
  }

  ~PasteCellsUndo() {
    delete m_newSelection;
    delete m_oldSelection;
    delete m_data;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, r1, c1);

    int oldR0, oldC0, oldR1, oldC1;
    m_oldSelection->getSelectedCells(oldR0, oldC0, oldR1, oldC1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (const auto &item : m_oldEmptyColumnNames) {
      xsh->getStageObject(TStageObjectId::ColumnId(item.first))
          ->setName(item.second);
    }

    int c0BeforeCut = c0;
    int c1BeforeCut = c1;
    // Cut delle celle che sono in newSelection
    cutCellsWithoutUndo(r0, c0, r1, c1);
    // Se le colonne erano vuote le resetto (e' necessario farlo per le colonne
    // particolari, colonne sount o palette)
    assert(c1BeforeCut - c0BeforeCut + 1 == (int)m_areOldColumnsEmpty.size());
    int c;
    for (c = c0BeforeCut; c <= c1BeforeCut; c++) {
      if (!m_areOldColumnsEmpty[c - c0BeforeCut] || !xsh->getColumn(c))
        continue;
    }

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, c1, r1);
    // Cut delle celle che sono in newSelection
    pasteCellsWithoutUndo(m_data, r0, c0, c1, r1, true, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Paste Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  DeleteCellsUndo
//  Recovering the column information (such as reconnecting nodes) when
//  undoing deletion of entire column will be done by DeleteColumnsUndo which
//  will be called in the same undo block. So here we only need to recover the
//  cell arrangement.
//-----------------------------------------------------------------------------

class DeleteCellsUndo final : public TUndo {
  TCellSelection *m_selection;
  TCellData *m_data;

  bool m_doShift;  // whether clear cell or remove and shift cells up
  bool m_containsSoundColumn;

public:
  DeleteCellsUndo(TCellSelection *selection, TCellData *data, bool doShift)
      : m_data(data), m_doShift(doShift), m_containsSoundColumn(false) {
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    if (c0 < 0) c0 = 0;  // Ignore camera column
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getSoundColumn()) {
        m_containsSoundColumn = true;
        break;
      }
    }
  }

  ~DeleteCellsUndo() { delete m_selection; }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);

    // insert cells if the delete operation had shifted cells up
    pasteCellsWithoutUndo(m_data, r0, c0, r1, c1, m_doShift, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    deleteCellsWithoutUndo(r0, c0, r1, c1, m_doShift);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Delete Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
  bool containsSoundColumn() { return m_containsSoundColumn; }
};

//=============================================================================
//  CutCellsUndo
//  Just like DeleteCellsUndo, recovering the column information (such as
//  reconnecting nodes) when undoing deletion of entire column will be done
//  by DeleteColumnsUndo which will be called in the same undo block.
//-----------------------------------------------------------------------------

class CutCellsUndo final : public TUndo {
  TCellSelection *m_selection;
  TCellData *m_data;

  bool m_containsSoundColumn;

public:
  CutCellsUndo(TCellSelection *selection)
      : m_data(), m_containsSoundColumn(false) {
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    if (c0 < 0) c0 = 0;  // Ignore camera column
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);
  }

  void setCurrentData(int r0, int c0, int r1, int c1) {
    m_data       = new TCellData();
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    m_data->setCells(xsh, r0, c0, r1, c1);

    for (int c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getSoundColumn()) {
        m_containsSoundColumn = true;
        break;
      }
    }
  }

  ~CutCellsUndo() {
    delete m_selection;
    delete m_data;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);

    pasteCellsWithoutUndo(m_data, r0, c0, r1, c1, true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    QClipboard *clipboard  = QApplication::clipboard();
    QMimeData *currentData = cloneData(clipboard->mimeData());

    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    cutCellsWithoutUndo(r0, c0, r1, c1);

    clipboard->setMimeData(currentData, QClipboard::Clipboard);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Cut Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }

  bool containsSoundColumn() { return m_containsSoundColumn; }
};

//=============================================================================
//  InsertUndo
//-----------------------------------------------------------------------------

class InsertUndo final : public TUndo {
  TCellSelection::Range m_range;
  bool m_containsSoundColumn;

public:
  InsertUndo(const TCellSelection::Range &range) : m_range(range) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c = m_range.m_c0; c <= m_range.m_c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getSoundColumn()) {
        m_containsSoundColumn = true;
        break;
      }
    }
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int rowCount = m_range.getRowCount();
    int c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++)
      xsh->removeCells(m_range.m_r0, c, rowCount);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int rowCount = m_range.getRowCount();
    int c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++)
      xsh->insertCells(m_range.m_r0, c, rowCount);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return QObject::tr("Insert Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  RenumberUndo
//-----------------------------------------------------------------------------

class RenumberUndo {
public:
  class RedoNotifier;
  class UndoNotifier;
};

class RenumberUndo::RedoNotifier final : public TUndo {
  void undo() const override {}
  void redo() const override {
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
};

class RenumberUndo::UndoNotifier final : public TUndo {
  void redo() const override {}
  void undo() const override {
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================

bool pasteStrokesInCellWithoutUndo(
    int row, int col, const StrokesData *strokesData,
    std::vector<int> &indices)  // in indices gli stroke aggiunti
{
  TApp *app     = TApp::instance();
  TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
  TXshCell cell = xsh->getCell(row, col);
  TVectorImageP vi;
  TXshSimpleLevel *sl = 0;
  TFrameId fid(1);
  if (cell.isEmpty()) {
    if (row > 0) cell = xsh->getCell(row - 1, col);
    sl = cell.getSimpleLevel();
    if (!sl || sl->getType() != PLI_XSHLEVEL) {
      ToonzScene *scene = app->getCurrentScene()->getScene();
      TXshLevel *xl     = scene->createNewLevel(PLI_XSHLEVEL);
      if (!xl) return false;
      sl = xl->getSimpleLevel();
      assert(sl);
      app->getCurrentScene()->notifyCastChange();
    }
    vi = sl->createEmptyFrame();
    assert(vi);
    std::vector<TFrameId> fids;
    sl->getFids(fids);
    if (fids.size() > 0) fid = TFrameId(fids.back().getNumber() + 1);
    sl->setFrame(fid, vi);
    app->getCurrentLevel()->setLevel(sl);
    app->getCurrentLevel()->notifyLevelChange();
    xsh->setCell(row, col, TXshCell(sl, fid));
    app->getCurrentXsheet()->notifyXsheetChanged();
  } else {
    vi = cell.getImage(true);
    sl = cell.getSimpleLevel();
    if (sl->getType() == OVL_XSHLEVEL && sl->getPath().isUneditable())
      return false;
    fid = cell.getFrameId();
    if (!vi) {
      DVGui::error(QObject::tr(
          "It is not possible to paste vectors in the current cell."));
      return false;
    }
  }
  if (vi) {
    std::set<int> indicesSet;
    strokesData->getImage(vi, indicesSet, true);
    for (int index : indicesSet) {
      indices.push_back(index);
    }
    assert(sl);
    app->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
    IconGenerator::instance()->invalidate(sl, fid);
  }
  return true;
}

//=============================================================================
//  PasteStrokesInCellsUndo
//-----------------------------------------------------------------------------

class PasteStrokesInCellUndo final : public TUndo {
  int m_row, m_col;
  StrokesData *m_strokesData;
  TFrameId m_fid;
  TVectorImageP m_image;
  TPaletteP m_oldPalette;
  TXshSimpleLevelP m_sl;
  bool m_createdFrame;
  bool m_isLevelCreated;
  std::vector<int> m_indices;

public:
  PasteStrokesInCellUndo(int row, int col, const StrokesData *data)
      : m_row(row)
      , m_col(col)
      , m_strokesData(data->clone())
      , m_image()
      , m_oldPalette(0)
      , m_createdFrame(false)
      , m_isLevelCreated(false) {
    TXsheet *xsh         = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell        = xsh->getCell(row, col);
    TPaletteP oldPalette = 0;
    if (!cell.getSimpleLevel()) {
      m_createdFrame      = true;
      TXshSimpleLevel *sl = xsh->getCell(row - 1, col).getSimpleLevel();
      if (row <= 0 || !sl || sl->getType() != PLI_XSHLEVEL)
        m_isLevelCreated = true;
      else
        oldPalette = xsh->getCell(row - 1, col).getSimpleLevel()->getPalette();
    } else
      oldPalette = cell.getSimpleLevel()->getPalette();
    if (oldPalette) m_oldPalette = oldPalette->clone();
  }

  ~PasteStrokesInCellUndo() { delete m_strokesData; }

  void setIndices(const std::vector<int> &indices) { m_indices = indices; }

  void onAdd() override {
    TXsheet *xsh  = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(m_row, m_col);
    m_sl          = cell.getSimpleLevel();
    m_fid         = cell.m_frameId;
    m_image       = cell.getImage(false);
  }

  void undo() const override {
    m_image->removeStrokes(m_indices, true, true);
    if (m_createdFrame) {
      TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
      m_sl->eraseFrame(m_fid);
      xsh->clearCells(m_row, m_col);
      if (m_isLevelCreated) {
        // butta il livello
        TLevelSet *levelSet =
            TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
        if (levelSet) {
          levelSet->removeLevel(m_sl.getPointer());
          TApp::instance()->getCurrentScene()->notifyCastChange();
        }
      }
    }
    if (m_oldPalette.getPointer()) {
      m_sl->getPalette()->assign(m_oldPalette->clone());
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
    }
    IconGenerator::instance()->invalidate(m_sl.getPointer(), m_fid);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    if (m_isLevelCreated) {
      TLevelSet *levelSet =
          TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
      if (levelSet) {
        levelSet->insertLevel(m_sl.getPointer());
        TApp::instance()->getCurrentScene()->notifyCastChange();
      }
    }
    if (m_createdFrame) {
      TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
      m_sl->setFrame(m_fid, m_image.getPointer());
      TXshCell cell(m_sl.getPointer(), m_fid);
      xsh->setCell(m_row, m_col, cell);
    }
    std::vector<int> indices = m_indices;
    pasteStrokesInCellWithoutUndo(m_row, m_col, m_strokesData, indices);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
  QString getHistoryString() override { return QObject::tr("Paste (Strokes)"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

bool pasteRasterImageInCellWithoutUndo(int row, int col,
                                       const RasterImageData *rasterImageData,
                                       TTileSet **tiles, bool &isLevelCreated) {
  isLevelCreated = false;
  TApp *app      = TApp::instance();
  TXsheet *xsh   = app->getCurrentXsheet()->getXsheet();
  TXshCell cell  = xsh->getCell(row, col);
  TImageP img;
  TXshSimpleLevel *sl = 0;
  TFrameId fid(1);
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TFrameId tmplFId  = scene->getProperties()->formatTemplateFIdForInput();

  TCamera *camera = scene->getCurrentCamera();
  if (cell.isEmpty()) {
    if (row > 0) cell = xsh->getCell(row - 1, col);
    sl = cell.getSimpleLevel();
    if (!sl || (sl->getType() == OVL_XSHLEVEL &&
                sl->getPath().getFrame() == TFrameId::NO_FRAME)) {
      int levelType;
      if (dynamic_cast<const ToonzImageData *>(rasterImageData))
        levelType = TZP_XSHLEVEL;
      else if (dynamic_cast<const FullColorImageData *>(rasterImageData))
        levelType = OVL_XSHLEVEL;
      TXshLevel *xl = 0;
      if (levelType == TZP_XSHLEVEL)
        xl = scene->createNewLevel(TZP_XSHLEVEL, L"", rasterImageData->getDim(),
                                   rasterImageData->getDpi().x);
      else if (levelType == OVL_XSHLEVEL)
        xl = scene->createNewLevel(OVL_XSHLEVEL, L"", rasterImageData->getDim(),
                                   rasterImageData->getDpi().x);
      if (!xl) return false;
      isLevelCreated = true;
      sl             = xl->getSimpleLevel();
      assert(sl);
      app->getCurrentScene()->notifyCastChange();

      if (levelType == TZP_XSHLEVEL || levelType == OVL_XSHLEVEL) {
        img = sl->createEmptyFrame();
      } else
        return false;

      // modify frameId to be with the same frame format as existing frames
      sl->formatFId(fid, tmplFId);

      sl->setFrame(fid, img);
      app->getCurrentLevel()->setLevel(sl);
      app->getCurrentLevel()->notifyLevelChange();
      sl->save();
      // after saving you need to obtain the image again
      img = sl->getFrame(fid, true);
    } else {
      img = sl->createEmptyFrame();
      assert(img);
      std::vector<TFrameId> fids;
      sl->getFids(fids);
      if (fids.size() > 0) fid = TFrameId(fids.back().getNumber() + 1);

      // modify frameId to be with the same frame format as existing frames
      sl->formatFId(fid, tmplFId);

      sl->setFrame(fid, img);
    }
    xsh->setCell(row, col, TXshCell(sl, fid));
    // to let the undo to know which frame is edited
    TTool::m_cellsData.push_back({row, row, TTool::CellOps::BlankToNew});
  } else {
    sl  = cell.getSimpleLevel();
    fid = cell.getFrameId();
    img = cell.getImage(true);
    if (!img) {
      DVGui::error(QObject::tr(
          "It is not possible to paste image on the current cell."));
      return false;
    }
  }
  if (img) {
    // This seems redundant, but newly created levels were having an issue with
    // pasting.
    // Don't know why.
    cell = xsh->getCell(row, col);
    sl   = cell.getSimpleLevel();
    fid  = cell.getFrameId();
    img  = cell.getImage(true);
    if (!img->getPalette()) {
      img->setPalette(sl->getPalette());
    }
    TRasterP ras;
    TRasterP outRas;
    double imgDpiX, imgDpiY;
    if (TToonzImageP ti = (TToonzImageP)(img)) {
      outRas = ti->getRaster();
      ti->getDpi(imgDpiX, imgDpiY);
    }
    if (TRasterImageP ri = (TRasterImageP)(img)) {
      outRas = ri->getRaster();
      ri->getDpi(imgDpiX, imgDpiY);
    }
    double dpiX, dpiY;
    std::vector<TRectD> rects;
    std::vector<TStroke> strokes;
    std::vector<TStroke> originalStrokes;
    TAffine affine;
    rasterImageData->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes,
                             affine, img->getPalette());
    if (dpiX == 0 || dpiY == 0) {
      TPointD dpi = camera->getDpi();
      dpiX        = dpi.x;
      dpiY        = dpi.y;
    }
    if (imgDpiX == 0 || imgDpiY == 0) {
      TPointD dpi = camera->getDpi();
      imgDpiX     = dpi.x;
      imgDpiY     = dpi.y;
    }
    TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
    affine *= sc;
    int i;
    TRectD boxD;
    if (rects.size() > 0) boxD = rects[0];
    if (strokes.size() > 0) boxD = strokes[0].getBBox();
    for (i = 0; i < rects.size(); i++) boxD += rects[i];
    for (i = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
    boxD = affine * boxD;
    TPoint pos;
    if (sl->getType() == TZP_XSHLEVEL) {
      TRect box = ToonzImageUtils::convertWorldToRaster(boxD, img);
      pos       = box.getP00();
      *tiles    = new TTileSetCM32(outRas->getSize());
      (*tiles)->add(outRas, ras->getBounds() + pos);
    } else if (sl->getType() == OVL_XSHLEVEL || sl->getType() == TZI_XSHLEVEL) {
      TRect box = TRasterImageUtils::convertWorldToRaster(boxD, img);
      pos       = box.getP00();
      *tiles    = new TTileSetFullColor(outRas->getSize());
      (*tiles)->add(outRas, ras->getBounds() + pos);
    } else
      return false;
    TRasterCM32P rasCM    = ras;
    TRasterCM32P outRasCM = outRas;
    if (!outRasCM && rasCM)
      TRop::over(outRas, rasCM, img->getPalette(), pos, affine);
    else
      TRop::over(outRas, ras, pos, affine);
    assert(sl);
    ToolUtils::updateSaveBox(sl, fid);
    IconGenerator::instance()->invalidate(sl, fid);
    sl->setDirtyFlag(true);
    app->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
  }
  return true;
}

//=============================================================================
//  PasteToonzImageInCellsUndo
//-----------------------------------------------------------------------------

class PasteToonzImageInCellsUndo final : public ToolUtils::TRasterUndo {
  RasterImageData *m_rasterImageData;

public:
  PasteToonzImageInCellsUndo(int row, int col, const RasterImageData *data,
                             TTileSetCM32 *tiles, TXshSimpleLevel *level,
                             const TFrameId &id, TPaletteP oldPalette,
                             bool createdFrame, bool isLevelCreated)
      : ToolUtils::TRasterUndo(tiles, level, id, createdFrame, isLevelCreated,
                               oldPalette)
      , m_rasterImageData(data->clone()) {}

  ~PasteToonzImageInCellsUndo() { delete m_rasterImageData; }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TTileSet *tiles;
    bool isLevelCreated;
    pasteRasterImageInCellWithoutUndo(m_row, m_col, m_rasterImageData, &tiles,
                                      isLevelCreated);
    delete tiles;
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return m_rasterImageData->getMemorySize() + TRasterUndo::getSize();
  }
  QString getHistoryString() override { return QObject::tr("Paste"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  PasteFullColorImageInCellsUndo
//-----------------------------------------------------------------------------

class PasteFullColorImageInCellsUndo final
    : public ToolUtils::TFullColorRasterUndo {
  RasterImageData *m_rasterImageData;

public:
  PasteFullColorImageInCellsUndo(const RasterImageData *data,
                                 TTileSetFullColor *tiles,
                                 TXshSimpleLevel *level, const TFrameId &id,
                                 TPaletteP oldPalette, bool createdFrame,
                                 bool isLevelCreated, int col = -1)
      : ToolUtils::TFullColorRasterUndo(tiles, level, id, createdFrame,
                                        isLevelCreated, oldPalette)
      , m_rasterImageData(data->clone()) {
    if (col > 0) m_col = col;
  }

  ~PasteFullColorImageInCellsUndo() { delete m_rasterImageData; }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TTileSet *tiles = 0;
    bool isLevelCreated;
    pasteRasterImageInCellWithoutUndo(m_row, m_col, m_rasterImageData, &tiles,
                                      isLevelCreated);
    m_level->setDirtyFlag(true);
    if (tiles) delete tiles;
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return m_rasterImageData->getMemorySize() + TFullColorRasterUndo::getSize();
  }
  QString getHistoryString() override { return QObject::tr("Paste (Raster)"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

void pasteDrawingsInCellWithoutUndo(TXsheet *xsh, TXshSimpleLevel *level,
                                    const std::set<TFrameId> &frameIds, int r0,
                                    int c0) {
  int frameToInsert = frameIds.size();
  xsh->insertCells(r0, c0, frameToInsert);
  std::set<TFrameId>::const_iterator it;
  int r = r0;
  for (it = frameIds.begin(); it != frameIds.end(); it++, r++) {
    TXshCell cell(level, *it);
    xsh->setCell(r, c0, cell);
  }
}

//=============================================================================
//  PasteDrawingsInCellUndo
//-----------------------------------------------------------------------------

class PasteDrawingsInCellUndo final : public TUndo {
  TXsheet *m_xsheet;
  int m_r0, m_c0;
  std::set<TFrameId> m_frameIds;
  TXshSimpleLevelP m_level;

public:
  PasteDrawingsInCellUndo(TXshSimpleLevel *level,
                          const std::set<TFrameId> &frameIds, int r0, int c0)
      : TUndo(), m_level(level), m_frameIds(frameIds), m_r0(r0), m_c0(c0) {
    m_xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    m_xsheet->addRef();
  }

  //--------------------------------

  ~PasteDrawingsInCellUndo() { m_xsheet->release(); }

  void undo() const override {
    int cellsToRemove = m_frameIds.size();
    m_xsheet->removeCells(m_r0, m_c0, cellsToRemove);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    pasteDrawingsInCellWithoutUndo(m_xsheet, m_level.getPointer(), m_frameIds,
                                   m_r0, m_c0);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override { return QObject::tr("Paste"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

bool pasteCellsWithoutUndo(int &r0, int &c0, int &r1, int &c1,
                           bool insert = true, bool doZeraryClone = true,
                           bool skipEmptyCells = true) {
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData);

  if (!cellData) return false;

  if (r0 < 0 || c0 < 0) return false;

  /*-- この中で、r1,c1はペースト範囲にあわせリサイズされる --*/
  bool ret = cellData->getCells(xsh, r0, c0, r1, c1, insert, doZeraryClone,
                                skipEmptyCells);
  if (!ret) return false;

  // Se la selezione corrente e' TCellSelection selezione le celle copiate
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (cellSelection) cellSelection->selectCells(r0, c0, r1, c1);
  return true;
}

//=============================================================================

class OverwritePasteCellsUndo final : public TUndo {
  TCellSelection *m_oldSelection;
  TCellSelection *m_newSelection;
  QMimeData *m_data;
  std::vector<bool> m_areOldColumnsEmpty;

  QMimeData *m_beforeData;
  bool m_containsSoundColumn;

public:
  OverwritePasteCellsUndo(int r0, int c0, int r1, int c1, int oldR0, int oldC0,
                          int oldR1, int oldC1,
                          const std::vector<bool> &areColumnsEmpty,
                          TCellData *beforeData, bool containsSoundColumn)
      : m_areOldColumnsEmpty(areColumnsEmpty)
      , m_containsSoundColumn(containsSoundColumn) {
    QClipboard *clipboard = QApplication::clipboard();
    /*-- ペーストされたセルをdataに保持しておく --*/
    TCellData *data = new TCellData();
    TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
    data->setCells(xsh, r0, c0, r1, c1);
    m_data         = data->clone();
    m_newSelection = new TCellSelection();
    m_newSelection->selectCells(r0, c0, r1, c1);
    m_oldSelection = new TCellSelection();
    m_oldSelection->selectCells(oldR0, oldC0, oldR1, oldC1);

    /*-- さらに、ペースト前のセルも保持しておく --*/
    m_beforeData = beforeData->clone();
  }

  ~OverwritePasteCellsUndo() {
    delete m_newSelection;
    delete m_oldSelection;
    delete m_data;
    delete m_beforeData;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, r1, c1);

    int oldR0, oldC0, oldR1, oldC1;
    m_oldSelection->getSelectedCells(oldR0, oldC0, oldR1, oldC1);

    QClipboard *clipboard = QApplication::clipboard();
    int c0BeforeCut       = c0;
    int c1BeforeCut       = c1;
    cutCellsWithoutUndo(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    assert(c1BeforeCut - c0BeforeCut + 1 == (int)m_areOldColumnsEmpty.size());
    int c;
    for (c = c0BeforeCut; c <= c1BeforeCut; c++) {
      if (!m_areOldColumnsEmpty[c - c0BeforeCut] || !xsh->getColumn(c))
        continue;
      xsh->removeColumn(c);
      xsh->insertColumn(c);
    }

    /*-- クリップボードの内容を取っておく --*/
    QMimeData *mimeData = cloneData(clipboard->mimeData());

    /*--
     * ペースト前にあったセル配列をcellDataとしていったんクリップボードに入れ、ペーストさせる
     * --*/
    clipboard->setMimeData(cloneData(m_beforeData), QClipboard::Clipboard);
    pasteCellsWithoutUndo(r0, c0, r1, c1, true, false, false);

    /*-- クリップボードを元に戻す --*/
    clipboard->setMimeData(mimeData, QClipboard::Clipboard);

    // Se le selezione corrente e' TCellSelection seleziono le celle che sono in
    // oldSelection
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection && oldR0 != -1 && oldC0 != -1)
      cellSelection->selectCells(oldR0, oldC0, oldR1, oldC1);

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, c1, r1);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);
    // Cut delle celle che sono in newSelection
    pasteCellsWithoutUndo(r0, c0, c1, r1, false, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Overwrite Paste Cells");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

bool pasteNumbersWithoutUndo(int &r0, int &c0, int &r1, int &c1) {
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData);

  if (!cellData) return false;

  if (r0 < 0 || c0 < 0) return false;

  bool ret = cellData->getNumbers(xsh, r0, c0, r1, c1);
  if (!ret) return false;

  // Se la selezione corrente e' TCellSelection selezione le celle copiate
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (cellSelection) cellSelection->selectCells(r0, c0, r1, c1);
  return true;
}

//=============================================================================

class OverwritePasteNumbersUndo final : public TUndo {
  TCellSelection *m_oldSelection;
  TCellSelection *m_newSelection;
  QMimeData *m_data;
  QMimeData *m_beforeData;

public:
  OverwritePasteNumbersUndo(int r0, int c0, int r1, int c1, int oldR0,
                            int oldC0, int oldR1, int oldC1,
                            TCellData *beforeData) {
    QClipboard *clipboard = QApplication::clipboard();
    // keep the pasted data
    TCellData *data = new TCellData();
    TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
    data->setCells(xsh, r0, c0, r1, c1);
    m_data         = data->clone();
    m_newSelection = new TCellSelection();
    m_newSelection->selectCells(r0, c0, r1, c1);
    m_oldSelection = new TCellSelection();
    m_oldSelection->selectCells(oldR0, oldC0, oldR1, oldC1);
    // keep the cells before pasting
    m_beforeData = beforeData->clone();
  }

  ~OverwritePasteNumbersUndo() {
    delete m_newSelection;
    delete m_oldSelection;
    delete m_data;
    delete m_beforeData;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, r1, c1);

    int oldR0, oldC0, oldR1, oldC1;
    m_oldSelection->getSelectedCells(oldR0, oldC0, oldR1, oldC1);

    QClipboard *clipboard = QApplication::clipboard();
    int c0BeforeCut       = c0;
    int c1BeforeCut       = c1;
    cutCellsWithoutUndo(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    // keep the clipboard content
    QMimeData *mimeData = cloneData(clipboard->mimeData());

    // execute pasting
    clipboard->setMimeData(cloneData(m_beforeData), QClipboard::Clipboard);
    pasteCellsWithoutUndo(r0, c0, r1, c1, true, false, false);

    // restore clipboard
    clipboard->setMimeData(mimeData, QClipboard::Clipboard);

    // revert old cell selection
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection && oldR0 != -1 && oldC0 != -1)
      cellSelection->selectCells(oldR0, oldC0, oldR1, oldC1);

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, c1, r1);
    QClipboard *clipboard = QApplication::clipboard();

    // keep the clipboard content
    QMimeData *mimeData = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);
    // Cut delle celle che sono in newSelection
    pasteCellsWithoutUndo(r0, c0, c1, r1, false, false);

    // restore clipboard
    clipboard->setMimeData(mimeData, QClipboard::Clipboard);

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Paste Numbers"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------
// if at least one of the cell in the range, return false
bool checkIfCellsHaveTheSameContent(const int &r0, const int &c0, const int &r1,
                                    const int &c1, const TXshCell &cell) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (int c = c0; c <= c1; c++) {
    for (int r = r0; r <= r1; r++) {
      if (cell != xsh->getCell(r, c)) return false;
    }
  }
  return true;
}

void renameCellsWithoutUndo(int &r0, int &c0, int &r1, int &c1,
                            const TXshCell &cell) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  for (int c = c0; c <= c1; c++) {
    for (int r = r0; r <= r1; r++) {
      xsh->setCell(r, c, TXshCell(cell));
    }
  }
  app->getCurrentXsheet()->notifyXsheetChanged();
  int currentFrame = app->getCurrentFrame()->getFrame();
  if (r0 <= currentFrame && currentFrame <= r1) {
    app->getCurrentTool()->onImageChanged(
        (TImage::Type)app->getCurrentImageType());
    xsh->getStageObjectTree()->invalidateAll();
  }
}

class RenameCellsUndo final : public TUndo {
  TCellSelection *m_selection;
  QMimeData *m_data;
  TXshCell m_cell;

public:
  RenameCellsUndo(TCellSelection *selection, QMimeData *data, TXshCell &cell)
      : m_data(data), m_cell(cell) {
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);
  }

  RenameCellsUndo(int r0, int c0, int r1, int c1, QMimeData *data,
                  TXshCell &cell)
      : m_data(data), m_cell(cell) {
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);
  }

  ~RenameCellsUndo() {
    delete m_selection;
    delete m_data;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c = c0; c <= c1; c++) {
      xsh->clearCells(r0, c, r1 - r0 + 1);
    }
    const TCellData *cellData = dynamic_cast<const TCellData *>(m_data);
    pasteCellsWithoutUndo(cellData, r0, c0, r1, c1, false, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    renameCellsWithoutUndo(r0, c0, r1, c1, m_cell);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    QString colRange = QString::number(c0 + 1);
    if (c0 != c1)
      colRange.append(QString(" - %1").arg(QString::number(c1 + 1)));
    QString frameRange = QString::number(r0 + 1);
    if (r0 != r1)
      frameRange.append(QString(" - %1").arg(QString::number(r1 + 1)));

    return QObject::tr("Rename Cell  at Column %1  Frame %2")
        .arg(colRange)
        .arg(frameRange);
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

class CreateBlankDrawingUndo final : public ToolUtils::TToolUndo {
  int row;
  int col;

public:
  CreateBlankDrawingUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                         bool levelCreated, const TPaletteP &oldPalette)
      : TToolUndo(level, frameId, true, levelCreated, oldPalette) {}

  ~CreateBlankDrawingUndo() {}

  void undo() const override {
    removeLevelAndFrameIfNeeded();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    IconGenerator::instance()->invalidate(m_level.getPointer(), m_frameId);

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Create Blank Drawing");
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
  //-----------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------

class DuplicateDrawingUndo final : public ToolUtils::TToolUndo {
  TFrameId origFrameId;
  TFrameId dupFrameId;

public:
  DuplicateDrawingUndo(TXshSimpleLevel *level, const TFrameId &srcFrameId,
                       const TFrameId &tgtFrameId)
      : TToolUndo(level, tgtFrameId, true) {
    origFrameId = srcFrameId;
    dupFrameId  = tgtFrameId;
  }

  ~DuplicateDrawingUndo() {}

  void undo() const override {
    removeLevelAndFrameIfNeeded();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Duplicate Drawing");
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
  //-----------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------

class FillEmptyCellUndo final : public TUndo {
  TCellSelection *m_selection;
  TXshCell m_cell;

public:
  FillEmptyCellUndo(int r0, int r1, int c, TXshCell &cell) : m_cell(cell) {
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c, r1, c);
  }

  ~FillEmptyCellUndo() { delete m_selection; }

  void undo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int c = c0; c <= c1; c++) {
      xsh->clearCells(r0, c, r1 - r0 + 1);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int i = r0; i <= r1; i++) xsh->setCell(i, c0, m_cell);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Fill In Empty Cells");
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
  //-----------------------------------------------------------------------------
};

//=============================================================================
// Duplicate in XSheet Undo
//-----------------------------------------------------------------------------

class DuplicateInXSheetUndo final : public TUndo {
  int m_r0, m_r1, m_c;
  TXshCell m_oldCell;
  TXshCell m_newCell;
  bool m_goForward;

public:
  DuplicateInXSheetUndo(int r0, int r1, int c, TXshCell oldCell,
                        TXshCell newCell, bool goForward)
      : m_r0(r0)
      , m_r1(r1)
      , m_c(c)
      , m_oldCell(oldCell)
      , m_newCell(newCell)
      , m_goForward(goForward) {}

  ~DuplicateInXSheetUndo() {}

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    if (m_r1 > m_r0) {
      for (int i = m_r0; i <= m_r1; i++) {
        xsh->setCell(i, m_c, m_oldCell);
      }
    } else
      xsh->setCell(m_r0, m_c, m_oldCell);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_goForward) CommandManager::instance()->execute(MI_PrevFrame);
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    if (m_r1 > m_r0) {
      for (int i = m_r0; i <= m_r1; i++) {
        xsh->setCell(i, m_c, m_newCell);
      }
    } else
      xsh->setCell(m_r0, m_c, m_newCell);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_goForward) CommandManager::instance()->execute(MI_NextFrame);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Duplicate Frame in XSheet");
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
  //-----------------------------------------------------------------------------
};

//-----------------------------------------------------------------------------
// obtain level set contained in the cellData
// it is used for checking and updating the scene cast when pasting
void getLevelSetFromData(const TCellData *cellData,
                         std::set<TXshLevel *> &levelSet) {
  for (int c = 0; c < cellData->getCellCount(); c++) {
    TXshCell cell = cellData->getCell(c);
    if (cell.isEmpty()) continue;
    TXshLevelP tmpLevel = cell.m_level;
    if (levelSet.count(tmpLevel.getPointer()) == 0) {
      // gather levels in subxsheet
      if (tmpLevel->getChildLevel()) {
        TXsheet *childXsh = tmpLevel->getChildLevel()->getXsheet();
        childXsh->getUsedLevels(levelSet);
      }
      levelSet.insert(tmpLevel.getPointer());
    }
  }
}

}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// TCellSelection::Range
//-----------------------------------------------------------------------------

TCellSelection::Range::Range() : m_c0(0), m_r0(0), m_c1(-1), m_r1(-1) {}

bool TCellSelection::Range::isEmpty() const {
  return m_c0 > m_c1 || m_r0 > m_r1;
}

bool TCellSelection::Range::contains(int r, int c) const {
  return m_r0 <= r && r <= m_r1 && m_c0 <= c && c <= m_c1;
}

int TCellSelection::Range::getRowCount() const { return m_r1 - m_r0 + 1; }

int TCellSelection::Range::getColCount() const { return m_c1 - m_c0 + 1; }

//=============================================================================
// TCellSelection
//-----------------------------------------------------------------------------

TCellSelection::TCellSelection()
    : m_timeStretchPopup(0), m_reframePopup(0), m_resizePivotRow(-1) {
  setAlternativeCommandNames();
}

//-----------------------------------------------------------------------------

TCellSelection::~TCellSelection() {}

//-----------------------------------------------------------------------------

void TCellSelection::enableCommands() {
  // enableCommand(this, MI_PasteNew,     &TCellSelection::dPasteCells);
  enableCommand(this, MI_Autorenumber, &TCellSelection::dRenumberCells);

  enableCommand(this, MI_Reverse, &TCellSelection::reverseCells);
  enableCommand(this, MI_Swing, &TCellSelection::swingCells);
  enableCommand(this, MI_Random, &TCellSelection::randomCells);
  enableCommand(this, MI_Increment, &TCellSelection::incrementCells);
  enableCommand(this, MI_ResetStep, &TCellSelection::resetStepCells);
  enableCommand(this, MI_IncreaseStep, &TCellSelection::increaseStepCells);
  enableCommand(this, MI_DecreaseStep, &TCellSelection::decreaseStepCells);
  enableCommand(this, MI_Step2, &TCellSelection::step2Cells);
  enableCommand(this, MI_Step3, &TCellSelection::step3Cells);
  enableCommand(this, MI_Step4, &TCellSelection::step4Cells);
  enableCommand(this, MI_Each2, &TCellSelection::each2Cells);
  enableCommand(this, MI_Each3, &TCellSelection::each3Cells);
  enableCommand(this, MI_Each4, &TCellSelection::each4Cells);

  enableCommand(this, MI_Rollup, &TCellSelection::rollupCells);
  enableCommand(this, MI_Rolldown, &TCellSelection::rolldownCells);

  enableCommand(this, MI_TimeStretch, &TCellSelection::openTimeStretchPopup);
  enableCommand(this, MI_CloneLevel, &TCellSelection::cloneLevel);
  enableCommand(this, MI_SetKeyframes, &TCellSelection::setKeyframes);

  enableCommand(this, MI_ShiftKeyframesDown,
                &TCellSelection::shiftKeyframesDown);
  enableCommand(this, MI_ShiftKeyframesUp, &TCellSelection::shiftKeyframesUp);

  enableCommand(this, MI_Copy, &TCellSelection::copyCells);
  enableCommand(this, MI_Paste,
                &TCellSelection::doPaste);  // choose pasting behavior by
                                            // preference option

  if (dynamic_cast<const TKeyframeData *>(
          QApplication::clipboard()->mimeData()))
    enableCommand(this, MI_PasteInto, &TCellSelection::pasteKeyframesInto);

  enableCommand(this, MI_Cut, &TCellSelection::cutCells);
  enableCommand(this, MI_Clear, &TCellSelection::deleteCells);
  enableCommand(this, MI_Insert, &TCellSelection::insertCells);

  enableCommand(this, MI_PasteInto, &TCellSelection::overWritePasteCells);

  enableCommand(this, MI_FillEmptyCell, &TCellSelection::fillEmptyCell);
  enableCommand(this, MI_Reframe1, &TCellSelection::reframe1Cells);
  enableCommand(this, MI_Reframe2, &TCellSelection::reframe2Cells);
  enableCommand(this, MI_Reframe3, &TCellSelection::reframe3Cells);
  enableCommand(this, MI_Reframe4, &TCellSelection::reframe4Cells);
  enableCommand(this, MI_ConvertToToonzRaster,
                &TCellSelection::convertToToonzRaster);
  enableCommand(this, MI_ConvertVectorToVector,
                &TCellSelection::convertVectortoVector);
  enableCommand(this, MI_ReframeWithEmptyInbetweens,
                &TCellSelection::reframeWithEmptyInbetweens);

  enableCommand(this, MI_PasteNumbers, &TCellSelection::overwritePasteNumbers);
  enableCommand(this, MI_PasteCellContent, &TCellSelection::pasteCells);
  enableCommand(this, MI_CreateBlankDrawing,
                &TCellSelection::createBlankDrawings);
  enableCommand(this, MI_Duplicate, &TCellSelection::duplicateFrames);
  enableCommand(this, MI_PasteDuplicate, &TCellSelection::pasteDuplicateCells);
}

//-----------------------------------------------------------------------------

void TCellSelection::setAlternativeCommandNames() {
  m_alternativeCommandNames = {
      {MI_Copy, QObject::tr("Copy Cells", "TCellSelection")},
      {MI_Paste, QObject::tr("Paste Cells", "TCellSelection")},
      {MI_PasteInto, QObject::tr("Overwrite Paste Cells", "TCellSelection")},
      {MI_Cut, QObject::tr("Cut Cells", "TCellSelection")},
      {MI_Clear, QObject::tr("Delete Cells", "TCellSelection")},
      {MI_Insert, QObject::tr("Insert Cells", "TCellSelection")}};
}

//-----------------------------------------------------------------------------
// Used in RenameCellField::eventFilter()

bool TCellSelection::isEnabledCommand(
    std::string commandId) {  // static function
  static QList<std::string> commands = {MI_Autorenumber,
                                        MI_Reverse,
                                        MI_Swing,
                                        MI_Random,
                                        MI_Increment,
                                        MI_ResetStep,
                                        MI_IncreaseStep,
                                        MI_DecreaseStep,
                                        MI_Step2,
                                        MI_Step3,
                                        MI_Step4,
                                        MI_Each2,
                                        MI_Each3,
                                        MI_Each4,
                                        MI_Rollup,
                                        MI_Rolldown,
                                        MI_TimeStretch,
                                        MI_CloneLevel,
                                        MI_SetKeyframes,
                                        MI_ShiftKeyframesDown,
                                        MI_ShiftKeyframesUp,
                                        MI_Copy,
                                        MI_Paste,
                                        MI_PasteInto,
                                        MI_Cut,
                                        MI_Clear,
                                        MI_Insert,
                                        MI_Reframe1,
                                        MI_Reframe2,
                                        MI_Reframe3,
                                        MI_Reframe4,
                                        MI_ReframeWithEmptyInbetweens,
                                        MI_Undo,
                                        MI_Redo,
                                        MI_PasteNumbers,
                                        MI_PasteCellContent,
                                        MI_ConvertToToonzRaster,
                                        MI_ConvertVectorToVector,
                                        MI_CreateBlankDrawing,
                                        MI_FillEmptyCell};
  return commands.contains(commandId);
}

//-----------------------------------------------------------------------------

bool TCellSelection::isEmpty() const { return m_range.isEmpty(); }

//-----------------------------------------------------------------------------

void TCellSelection::selectCells(int r0, int c0, int r1, int c1) {
  if (r0 > r1) std::swap(r0, r1);
  if (c0 > c1) std::swap(c0, c1);
  m_range.m_r0            = r0;
  m_range.m_c0            = c0;
  m_range.m_r1            = r1;
  m_range.m_c1            = c1;
  bool onlyOneRasterLevel = containsOnlyOneRasterLevel(r0, c0, r1, c1);
  // set the nearest row
  m_resizePivotRow =
      (std::abs(r0 - m_resizePivotRow) < std::abs(r1 - m_resizePivotRow)) ? r0
                                                                          : r1;
  CommandManager::instance()->enable(MI_CanvasSize, onlyOneRasterLevel);
}

//-----------------------------------------------------------------------------

void TCellSelection::selectCell(int row, int col) {
  m_range.m_r0            = row;
  m_range.m_c0            = col;
  m_range.m_r1            = row;
  m_range.m_c1            = col;
  bool onlyOneRasterLevel = containsOnlyOneRasterLevel(row, col, row, col);
  m_resizePivotRow        = row;
  CommandManager::instance()->enable(MI_CanvasSize, onlyOneRasterLevel);
}

//-----------------------------------------------------------------------------

void TCellSelection::selectNone() {
  m_range          = Range();
  m_resizePivotRow = -1;
  CommandManager::instance()->enable(MI_CanvasSize, false);
}

//-----------------------------------------------------------------------------

void TCellSelection::getSelectedCells(int &r0, int &c0, int &r1,
                                      int &c1) const {
  r0 = m_range.m_r0;
  r1 = m_range.m_r1;
  c0 = m_range.m_c0;
  c1 = m_range.m_c1;
}

//-----------------------------------------------------------------------------

TCellSelection::Range TCellSelection::getSelectedCells() const {
  return m_range;
}

//-----------------------------------------------------------------------------

bool TCellSelection::isCellSelected(int r, int c) const {
  return m_range.contains(r, c);
}

//-----------------------------------------------------------------------------

bool TCellSelection::isRowSelected(int r) const {
  return m_range.m_r0 <= r && r <= m_range.m_r1;
}

//-----------------------------------------------------------------------------

bool TCellSelection::isColSelected(int c) const {
  return m_range.m_c0 <= c && c >= m_range.m_c1;
}
//-----------------------------------------------------------------------------

bool TCellSelection::areAllColSelectedLocked() const {
  int c;
  for (c = m_range.m_c0; c <= m_range.m_c1; c++) {
    TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
    TXshColumn *column = xsh->getColumn(c);
    if (column && !xsh->getColumn(c)->isLocked()) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

void TCellSelection::copyCells() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (isEmpty()) return;
  copyCellsWithoutUndo(r0, c0, r1, c1);
}

//-----------------------------------------------------------------------------

static void pasteStrokesInCell(int row, int col,
                               const StrokesData *strokesData) {
  PasteStrokesInCellUndo *undo =
      new PasteStrokesInCellUndo(row, col, strokesData);
  std::vector<int> indices;
  bool isPaste = pasteStrokesInCellWithoutUndo(row, col, strokesData, indices);
  if (!isPaste) {
    delete undo;
    return;
  }
  undo->setIndices(indices);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

static void pasteRasterImageInCell(int row, int col,
                                   const RasterImageData *rasterImageData,
                                   bool newLevel = false) {
  // to let the undo to know which frame is edited
  TTool::m_cellsData.clear();

  TXsheet *xsh         = TApp::instance()->getCurrentXsheet()->getXsheet();
  bool createdFrame    = false;
  bool isLevelCreated  = false;
  TPaletteP oldPalette = 0;

  if (newLevel) {
    while (!xsh->getCell(row, col).isEmpty()) {
      col += 1;
    }
    createdFrame = true;
  }
  // get the current cell
  TXshCell cell = xsh->getCell(row, col);
  // if the cell doesn't have a level...
  if (!newLevel) {
    if (!cell.getSimpleLevel()) {
      createdFrame = true;
      // try the previous frame
      TXshSimpleLevel *sl = xsh->getCell(row - 1, col).getSimpleLevel();
      if (sl) oldPalette = sl->getPalette();
    } else {
      TXshSimpleLevel *sl = cell.getSimpleLevel();
      // don't do anything to ffmpeg level types
      if (sl->getType() == OVL_XSHLEVEL && sl->getPath().isUneditable()) return;
      oldPalette = sl->getPalette();
    }
  }
  if (oldPalette) oldPalette = oldPalette->clone();
  TTileSet *tiles = 0;
  bool isPaste    = pasteRasterImageInCellWithoutUndo(row, col, rasterImageData,
                                                      &tiles, isLevelCreated);
  if (isLevelCreated && oldPalette.getPointer()) oldPalette = 0;
  if (!isPaste) return;
  cell = xsh->getCell(row, col);

  // The flag TTool::m_isLevelRenumbererd is evaluated in the undo.
  // We need to reset the flag here as the operation does not call
  // TTool::touchImage(). Currently the flag can be always false as the
  // operation does not renumber cells regardless of the preferences.
  TTool::m_isLevelRenumbererd = false;

  TTileSetCM32 *cm32Tiles           = dynamic_cast<TTileSetCM32 *>(tiles);
  TTileSetFullColor *fullColorTiles = dynamic_cast<TTileSetFullColor *>(tiles);
  if (cm32Tiles) {
    TUndoManager::manager()->add(new PasteToonzImageInCellsUndo(
        row, col, rasterImageData, cm32Tiles, cell.getSimpleLevel(),
        cell.getFrameId(), oldPalette, createdFrame, isLevelCreated));
  } else if (fullColorTiles) {
    TUndoManager::manager()->add(new PasteFullColorImageInCellsUndo(
        rasterImageData, fullColorTiles, cell.getSimpleLevel(),
        cell.getFrameId(), oldPalette, createdFrame, isLevelCreated, col));
  }
}

//-----------------------------------------------------------------------------
// choose pasting behavior by preference option
void TCellSelection::doPaste() {
  if (Preferences::instance()->getPasteCellsBehavior() ==
      0)  // insert paste whole contents of copied cells
    pasteCells();
  else  // overwrite paste numbers, consistent with QuickChecker
    overwritePasteNumbers();
}

//-----------------------------------------------------------------------------

void TCellSelection::pasteCells() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  XsheetViewer *viewer      = TApp::instance()->getCurrentXsheetViewer();
  ToolHandle *toolHandle    = TApp::instance()->getCurrentTool();

  bool initUndo = false;
  const TCellKeyframeData *cellKeyframeData =
      dynamic_cast<const TCellKeyframeData *>(mimeData);

  const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData);
  if (!cellData && cellKeyframeData) cellData = cellKeyframeData->getCellData();
  if (cellData) {
    if (isEmpty())  // Se la selezione delle celle e' vuota ritorno.
      return;

    if (cellData->getCellCount() == 0) {
      DVGui::error(QObject::tr("No data to paste."));
      return;
    }

    if (viewer && !viewer->orientation()->isVerticalTimeline()) {
      int cAdj = cellData->getColCount() - 1;
      c0 -= cAdj;
      c1 -= cAdj;
    }

    int oldR0 = r0;
    int oldC0 = c0;
    int oldR1 = r1;
    int oldC1 = c1;
    if (!cellData->canChange(xsh, c0)) return;

    // Check Circular References
    int i;
    for (i = 0; i < cellData->getCellCount(); i++) {
      if (!xsh->checkCircularReferences(cellData->getCell(i))) continue;
      DVGui::error(
          QObject::tr("It is not possible to paste the cells: there is a "
                      "circular reference."));
      return;
    }

    // pasting cells may change empty column names. So keep old column names for
    // undo
    std::map<int, std::string> emptyColumnNames;
    if (Preferences::instance()->isLinkColumnNameWithLevelEnabled()) {
      for (int c = c0; c < c0 + cellData->getColCount(); c++) {
        TXshColumn *column = xsh->getColumn(c);
        if (!column || column->isEmpty()) {
          if (!xsh->getStageObject(TStageObjectId::ColumnId(c))
                   ->hasSpecifiedName())
            emptyColumnNames[c] = "";
          else
            emptyColumnNames[c] =
                xsh->getStageObject(TStageObjectId::ColumnId(c))->getName();
        }
      }
    }

    bool isPaste = pasteCellsWithoutUndo(cellData, r0, c0, r1, c1);

    // Se la selezione corrente e' TCellSelection selezione le celle copiate
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection) cellSelection->selectCells(r0, c0, r1, c1);

    // Controllo se le colonne erano vuote prima del paste
    // n.b. devo farlo dopo il paste perche' prima non ho il range delle colonne
    // incollate corrette.
    std::vector<bool> areColumnsEmpty;
    bool containsSoundColumn = false;
    int c;
    for (c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column) {
        areColumnsEmpty.push_back(false);
        continue;
      }
      if (!containsSoundColumn && column->getSoundColumn())
        containsSoundColumn = true;
      int newCr0, newCr1;
      column->getRange(newCr0, newCr1);
      areColumnsEmpty.push_back(!column || column->isEmpty() ||
                                (newCr0 == r0 && newCr1 == r1));
    }
    if (!isPaste) return;

    initUndo = true;
    TUndoManager::manager()->beginBlock();

    // make sure that the pasting levels are registered in the scene cast
    // it may rename the level if there is another level with the same name
    std::set<TXshLevel *> pastedLevels;
    getLevelSetFromData(cellData, pastedLevels);
    LevelCmd::addMissingLevelsToCast(pastedLevels);

    TUndoManager::manager()->add(new PasteCellsUndo(
        r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1, areColumnsEmpty,
        containsSoundColumn, emptyColumnNames));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    if (containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  const TKeyframeData *keyframeData =
      dynamic_cast<const TKeyframeData *>(mimeData);
  if (!keyframeData && cellKeyframeData)
    keyframeData = cellKeyframeData->getKeyframeData();
  if (keyframeData) {
    if (keyframeData->m_keyData.empty()) {
      DVGui::error(QObject::tr(
          "It is not possible to paste data: there is nothing to paste."));
      return;
    }
    TKeyframeSelection selection;
    if (isEmpty() && TApp::instance()->getCurrentObject()->getObjectId() ==
                         TStageObjectId::CameraId(xsh->getCameraColumnIndex()))
    // Se la selezione e' vuota e l'objectId e' quello della camera sono nella
    // colonna di camera quindi devo selezionare la row corrente e -1.
    {
      int row = TApp::instance()->getCurrentFrame()->getFrame();
      selection.select(row, -1);
    } else {
      // Retrieves all keyframe positions from mime data and translates them by
      // (r0,c0)
      std::set<TKeyframeSelection::Position> positions;
      int newC0 = c0;
      if (viewer && !viewer->orientation()->isVerticalTimeline() && !cellData)
        newC0 = c0 - keyframeData->getColumnSpanCount() + 1;
      TKeyframeSelection::Position offset(keyframeData->getKeyframesOffset());
      positions.insert(TKeyframeSelection::Position(r0 + offset.first,
                                                    newC0 + offset.second));
      keyframeData->getKeyframes(positions);
      selection.select(positions);

      if (!cellKeyframeData) {
        // Retrieve the keyframes bbox
        r1 = positions.rbegin()->first;
        c1 = c0;

        std::set<TKeyframeSelection::Position>::const_iterator it,
            end = positions.end();
        for (it = positions.begin(); it != end; ++it)
          c1 = std::max(c1, it->second);
      }
    }
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    selection.pasteKeyframesWithShift(r0, r1, c0, c1);
  }

  if (const DrawingData *drawingData =
          dynamic_cast<const DrawingData *>(mimeData)) {
    if (isEmpty())  // Se la selezione delle celle e' vuota ritorno.
      return;

    std::set<TFrameId> frameIds;
    drawingData->getFrames(frameIds);
    TXshSimpleLevel *level = drawingData->getLevel();
    if (level && !frameIds.empty())
      pasteDrawingsInCellWithoutUndo(xsh, level, frameIds, r0, c0);
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    TUndoManager::manager()->add(
        new PasteDrawingsInCellUndo(level, frameIds, r0, c0));
  }
  if (const StrokesData *strokesData =
          dynamic_cast<const StrokesData *>(mimeData)) {
    if (isEmpty())  // Se la selezione delle celle e' vuota ritorno.
      return;

    TImageP img = xsh->getCell(r0, c0).getImage(false);
    if (!img && r0 > 0) {
      TXshCell cell = xsh->getCell(r0 - 1, c0);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl && (xl->getType() != OVL_XSHLEVEL ||
                 xl->getPath().getFrame() != TFrameId::NO_FRAME))
        img = cell.getImage(false);
    }
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    RasterImageData *rasterImageData = 0;
    if (TToonzImageP ti = img) {
      rasterImageData = strokesData->toToonzImageData(ti);
      pasteRasterImageInCell(r0, c0, rasterImageData);
    } else if (TRasterImageP ri = img) {
      double dpix, dpiy;
      ri->getDpi(dpix, dpiy);
      if (dpix == 0 || dpiy == 0) {
        TPointD dpi = xsh->getScene()->getCurrentCamera()->getDpi();
        dpix        = dpi.x;
        dpiy        = dpi.y;
        ri->setDpi(dpix, dpiy);
      }
      rasterImageData = strokesData->toFullColorImageData(ri);
      pasteRasterImageInCell(r0, c0, rasterImageData);
    } else
      pasteStrokesInCell(r0, c0, strokesData);
  }
  // Raster Time
  // See if an image was copied from outside OpenToonz
  QImage clipImage = clipboard->image();
  // See if the clipboard contains rasterData
  const RasterImageData *rasterImageData =
      dynamic_cast<const RasterImageData *>(mimeData);
  if (rasterImageData || clipImage.height() > 0) {
    if (isEmpty())  // Nothing selected.
      return;

    // get the current image and find out the type
    TImageP img = xsh->getCell(r0, c0).getImage(false);
    if (!img && r0 > 0) {
      // Try the previous cell.
      TXshCell cell = xsh->getCell(r0 - 1, c0);
      TXshLevel *xl = cell.m_level.getPointer();
      if (xl && (xl->getType() != OVL_XSHLEVEL ||
                 xl->getPath().getFrame() != TFrameId::NO_FRAME))
        img = cell.getImage(false);
    }
    const FullColorImageData *fullColData =
        dynamic_cast<const FullColorImageData *>(rasterImageData);
    TToonzImageP ti(img);
    TVectorImageP vi(img);
    if (fullColData && (vi || ti)) {
      // Bail out if the level is Smart Raster or Vector with normal raster
      // data.
      DVGui::error(QObject::tr(
          "The copied selection cannot be pasted in the current drawing."));
      return;
    }
    // Convert non-plain raster data to strokes data
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    if (vi && clipImage.isNull()) {
      // Vector stuff
      TXshSimpleLevel *sl = xsh->getCell(r0, c0).getSimpleLevel();
      if (!sl) sl = xsh->getCell(r0 - 1, c0).getSimpleLevel();
      assert(sl);
      StrokesData *strokesData = rasterImageData->toStrokesData(sl->getScene());
      pasteStrokesInCell(r0, c0, strokesData);
      // end strokes stuff
    } else {
      TXshSimpleLevel *sl = xsh->getCell(r0, c0).getSimpleLevel();
      if (!sl && r0 > 0) sl = xsh->getCell(r0 - 1, c0).getSimpleLevel();
      bool newLevel = false;
      TRasterImageP ri(img);

      if (clipImage.height() > 0) {
        // This stuff is only if we have a pasted image from outside OpenToonz
        bool cancel = false;

        if (sl && sl->getType() == OVL_XSHLEVEL) {
          if (sl->getResolution().lx < clipImage.width() ||
              sl->getResolution().ly < clipImage.height()) {
            clipImage =
                clipImage.scaled(sl->getResolution().lx, sl->getResolution().ly,
                                 Qt::KeepAspectRatio);
          }
        } else {
          QString question = QObject::tr(
              "Pasting external image from clipboard.\n\nWhat do you want to "
              "do?");
          int ret = DVGui::MsgBox(question, QObject::tr("New raster level"),
                                  QObject::tr("Cancel"), 0);
          if (ret == 1) {  // New level chosen
            newLevel = true;
          } else {  // Cancel or close
            cancel = true;
          }
        }

        if (cancel) {
          // Cancel or dialog closed
          if (initUndo) TUndoManager::manager()->endBlock();
          return;
        }

        if (newLevel) {
          if (sl) {
            // find the next empty column
            while (!xsh->isColumnEmpty(c0)) {
              c0 += 1;
            }
          }
          TXshColumn *col =
              TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(c0);
          TApp::instance()->getCurrentColumn()->setColumnIndex(c0);
          TApp::instance()->getCurrentColumn()->setColumn(col);
          TApp::instance()->getCurrentFrame()->setFrame(r0);
        }

        // create variables to go into the Full Color Raster Selection data
        std::vector<TRectD> rects;
        const std::vector<TStroke> strokes;
        const std::vector<TStroke> originalStrokes;
        TAffine aff;
        TRasterP ras = rasterFromQImage(clipImage);
        rects.push_back(TRectD(0.0 - clipImage.width() / 2,
                               0.0 - clipImage.height() / 2,
                               clipImage.width() / 2, clipImage.height() / 2));
        FullColorImageData *qimageData = new FullColorImageData();
        TPalette *p;
        if (!ri || !ri->getPalette() || newLevel)
          p = new TPalette();
        else
          p = ri->getPalette()->clone();
        TDimension dim;
        if (ri && !newLevel) {
          dim = ri->getRaster()->getSize();
        } else {
          dim = TDimension(clipImage.width(), clipImage.height());
        }
        qimageData->setData(ras, p, 120.0, 120.0, dim, rects, strokes,
                            originalStrokes, aff);
        rasterImageData = qimageData;
        // end of pasted from outside OpenToonz stuff
        // rasterImageData holds all the info either way now.
      }

      if (sl && sl->getType() == OVL_XSHLEVEL) {
        // make selection always work on new raster cells
        if (toolHandle->getTool()->getName() == "T_Selection") {
          TSelection *ts      = toolHandle->getTool()->getSelection();
          RasterSelection *rs = dynamic_cast<RasterSelection *>(ts);
          if (rs) {
            toolHandle->getTool()->onDeactivate();
            toolHandle->getTool()->onActivate();
            rs->pasteSelection();
            assert(initUndo);
            TUndoManager::manager()->endBlock();
            return;
          }
        }
      }
      pasteRasterImageInCell(r0, c0, rasterImageData, newLevel);

    }  // end of full raster stuff
  }    // end of raster stuff
  if (!initUndo) {
    DVGui::error(QObject::tr(
        "It is not possible to paste data: there is nothing to paste."));
    return;
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellSelection::pasteDuplicateCells() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();

  TXsheet *xsh         = TApp::instance()->getCurrentXsheet()->getXsheet();
  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();

  bool initUndo = false;
  const TCellKeyframeData *cellKeyframeData =
      dynamic_cast<const TCellKeyframeData *>(mimeData);

  const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData);
  if (!cellData && cellKeyframeData) cellData = cellKeyframeData->getCellData();
  if (!cellData) {
    DVGui::error(QObject::tr("There are no copied cells to duplicate."));
    return;
  }
  if (cellData) {
    // go through all the reasons this might fail
    if (isEmpty()) return;

    if (cellData->getCellCount() == 0) {
      DVGui::error(QObject::tr("No data to paste."));
      return;
    }

    // xsheet columns increase in number as they go to the right
    // timeline "columns" (visually rows) increase in number as they go up
    // To paste in the frames the user would expect, we need to adjust the
    // "column" placement if using the timeline.

    if (viewer && !viewer->orientation()->isVerticalTimeline()) {
      int cAdj = cellData->getColCount() - 1;
      c0 -= cAdj;
      c1 -= cAdj;
    }

    // make sure we aren't trying to paste anything in locked columns
    for (int i = 0; i < cellData->getColCount(); i++) {
      TXshColumn *column = xsh->getColumn(c0 + i);
      if (column && column->isLocked()) {
        DVGui::warning(QObject::tr("Cannot paste cells on locked layers."));
        return;
      }
    }

    // make sure the camera column isn't included in the selection
    if (c0 < 0) {
      DVGui::warning(QObject::tr("Can't place drawings on the camera column."));
      return;
    }

    int oldR0 = r0;
    int oldC0 = c0;
    int oldR1 = r1;
    int oldC1 = c1;
    if (!cellData->canChange(xsh, c0)) return;

    // find what cells we need
    int cellCount = cellData->getCellCount();

    // use a pair to ensure that only unique cells are duplicated
    std::set<TXshCell> cells;
    std::vector<TXshCell> cellsVector;
    for (int i = 0; i < cellCount; i++) {
      TXshCell cell = cellData->getCell(i);
      cells.insert(cell);
      cellsVector.push_back(cell);
    }

    // now put the set into a vector since it's easier to deal with
    std::vector<TXshCell> uniqueCells;
    std::set<TXshCell>::iterator it = cells.begin();
    while (it != cells.end()) {
      uniqueCells.push_back(*it);
      it++;
    }

    TXshSimpleLevel *sl;
    TXshLevelP level;

    // check that the selection only includes types that can be duplicated and
    // edited
    it = cells.begin();
    while (it != cells.end()) {
      if (it->isEmpty()) {
        it++;
        continue;
      }
      level = it->m_level;
      if (level) {
        int levelType = level->getType();
        if (levelType == ZERARYFX_XSHLEVEL || levelType == PLT_XSHLEVEL ||
            levelType == SND_XSHLEVEL || levelType == SND_TXT_XSHLEVEL ||
            levelType == MESH_XSHLEVEL) {
          DVGui::warning(
              QObject::tr("Cannot duplicate a drawing in the current column"));
          return;
        } else if (level->getSimpleLevel() &&
                   level->getSimpleLevel()->isReadOnly()) {
          DVGui::warning(
              QObject::tr("Cannot duplicate frames in read only levels"));
          return;
        } else if (level->getSimpleLevel() &&
                   it->getFrameId() == TFrameId::NO_FRAME) {
          DVGui::warning(QObject::tr(
              "Can only duplicate frames in image sequence levels."));
          return;
        }
      }
      it++;
    }

    // now we do the work of duplicating cells

    // start a undo block first
    initUndo = true;
    TUndoManager::manager()->beginBlock();

    // Turn on sync level strip changes with xsheet if it is off
    bool syncXsheetOn = true;
    if (!Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
      syncXsheetOn = false;
      Preferences::instance()->setValue(syncLevelRenumberWithXsheet, true);
    }

    // now actually duplicate
    std::vector<std::pair<TXshCell, TXshCell>> cellPairs;

    for (int i = 0; i < uniqueCells.size(); i++) {
      level = uniqueCells[i].m_level;
      if (level) {
        sl = uniqueCells[i].getSimpleLevel();
        if (sl) {
          std::set<TFrameId> frames;
          TFrameId fid = uniqueCells[i].getFrameId();
          frames.insert(fid);
          FilmstripCmd::duplicate(sl, frames, true);

          // see if we need to increment the frameId of
          // other cells in the same level
          std::set<int> cellsVectorAlreadyIncremented;
          std::set<int> cellsPairsAlreadyIncremented;
          for (int j = 0; j < uniqueCells.size(); j++) {
            if (j == i) continue;
            TXshSimpleLevel *tempSl;
            TXshLevelP tempLevel;
            tempLevel = uniqueCells[j].m_level;
            if (tempLevel) {
              tempSl = uniqueCells[j].getSimpleLevel();
              if (tempSl) {
                if (tempSl == sl) {
                  TFrameId tempId    = uniqueCells[j].getFrameId();
                  TFrameId currentId = uniqueCells[i].getFrameId();

                  if (tempId.getNumber() > currentId.getNumber()) {
                    tempId = TFrameId(tempId.getNumber() + 1, 0);

                    // keep cellsVector in sync with uniqueCells
                    for (int k = 0; k < cellsVector.size(); k++) {
                      if (cellsVector[k] == uniqueCells[j] &&
                          cellsVectorAlreadyIncremented.find(k) ==
                              cellsVectorAlreadyIncremented.end()) {
                        cellsVector[k].m_frameId = tempId;
                        cellsVectorAlreadyIncremented.insert(k);
                      }
                    }
                    // and with cellPairs
                    for (int k = 0; k < cellPairs.size(); k++) {
                      if (cellPairs[k].first == uniqueCells[j] &&
                          cellsPairsAlreadyIncremented.find(k) ==
                              cellsPairsAlreadyIncremented.end()) {
                        cellPairs[k].first.m_frameId = tempId;
                        cellPairs[k].second.m_frameId =
                            cellPairs[k].second.m_frameId + 1;
                        cellsPairsAlreadyIncremented.insert(k);
                      }
                    }
                    uniqueCells[j].m_frameId = tempId;
                  }
                }
              }
            }
          }
          TXshCell newCell;
          newCell.m_level   = sl;
          newCell.m_frameId = fid + 1;
          cellPairs.push_back(std::make_pair(uniqueCells[i], newCell));
        }
      }
    }

    // turn off sync with xsheet if it wasn't on originally
    if (syncXsheetOn == false) {
      Preferences::instance()->setValue(syncLevelRenumberWithXsheet, false);
    }

    // we should now have a vector with all original cells
    // and a pair vector with the original cell and its duplicate
    // let's make a new cellData populated with the new cells when needed.
    std::vector<TXshCell> newCells;
    for (int i = 0; i < cellsVector.size(); i++) {
      std::vector<std::pair<TXshCell, TXshCell>>::iterator pairIt =
          cellPairs.begin();
      bool found = false;
      while (pairIt != cellPairs.end()) {
        if (cellsVector[i] == pairIt->first) {
          found = true;
          newCells.push_back(pairIt->second);
          break;
        }
        pairIt++;
      }
      if (!found) {
        newCells.push_back(cellsVector[i]);
      }
    }

    TCellData *newCellData = new TCellData(cellData);
    newCellData->replaceCells(newCells);

    assert(newCellData->getCellCount() == cellData->getCellCount());

    bool isPaste = pasteCellsWithoutUndo(newCellData, r0, c0, r1, c1);

    // Se la selezione corrente e' TCellSelection selezione le celle copiate
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection) cellSelection->selectCells(r0, c0, r1, c1);

    std::vector<bool> areColumnsEmpty;
    bool containsSoundColumn = false;
    int c;
    for (c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column) {
        areColumnsEmpty.push_back(false);
        continue;
      }
      if (!containsSoundColumn && column->getSoundColumn())
        containsSoundColumn = true;
      int newCr0, newCr1;
      column->getRange(newCr0, newCr1);
      areColumnsEmpty.push_back(!column || column->isEmpty() ||
                                (newCr0 == r0 && newCr1 == r1));
    }

    if (!isPaste) {
      TUndoManager::manager()->endBlock();
      if (cellPairs.size() > 0) {
        TUndoManager::manager()->undo();
        TUndoManager::manager()->popUndo(1);
      }
      return;
    }

    // make sure that the pasting levels are registered in the scene cast
    // it may rename the level if there is another level with the same name
    std::set<TXshLevel *> pastedLevels;
    getLevelSetFromData(newCellData, pastedLevels);
    LevelCmd::addMissingLevelsToCast(pastedLevels);

    TUndoManager::manager()->add(
        new PasteCellsUndo(r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1,
                           areColumnsEmpty, containsSoundColumn));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }

  const TKeyframeData *keyframeData =
      dynamic_cast<const TKeyframeData *>(mimeData);
  if (!keyframeData && cellKeyframeData)
    keyframeData = cellKeyframeData->getKeyframeData();
  if (keyframeData) {
    if (keyframeData->m_keyData.empty()) {
      DVGui::error(QObject::tr(
          "It is not possible to paste data: there is nothing to paste."));
      return;
    }
    TKeyframeSelection selection;
    if (isEmpty() && TApp::instance()->getCurrentObject()->getObjectId() ==
                         TStageObjectId::CameraId(xsh->getCameraColumnIndex()))
    // Se la selezione e' vuota e l'objectId e' quello della camera sono nella
    // colonna di camera quindi devo selezionare la row corrente e -1.
    {
      int row = TApp::instance()->getCurrentFrame()->getFrame();
      selection.select(row, -1);
    } else {
      // Retrieves all keyframe positions from mime data and translates them by
      // (r0,c0)
      std::set<TKeyframeSelection::Position> positions;
      int newC0 = c0;
      if (viewer && !viewer->orientation()->isVerticalTimeline() && !cellData)
        newC0 = c0 - keyframeData->getColumnSpanCount() + 1;
      TKeyframeSelection::Position offset(keyframeData->getKeyframesOffset());
      positions.insert(TKeyframeSelection::Position(r0 + offset.first,
                                                    newC0 + offset.second));
      keyframeData->getKeyframes(positions);
      selection.select(positions);

      if (!cellKeyframeData) {
        // Retrieve the keyframes bbox
        r1 = positions.rbegin()->first;
        c1 = c0;

        std::set<TKeyframeSelection::Position>::const_iterator it,
            end = positions.end();
        for (it = positions.begin(); it != end; ++it)
          c1 = std::max(c1, it->second);
      }
    }
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    selection.pasteKeyframesWithShift(r0, r1, c0, c1);
  }

  if (!initUndo) {
    DVGui::error(QObject::tr("There are no copied cells to duplicate."));
    return;
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TUndoManager::manager()->endBlock();
  clipboard->clear();
}

//-----------------------------------------------------------------------------

void TCellSelection::deleteCells() {
  // choose clear or remove+shift behavior
  // 0: Clear Cell / Frame
  // 1: Remove and Shift Cells / Frames Up
  bool withShift = Preferences::instance()->getDeleteCommandBehaviour() == 1;
  deleteCells(withShift);
}

//-----------------------------------------------------------------------------

void TCellSelection::deleteCells(bool withShift) {
  if (isEmpty()) return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (c0 < 0) c0 = 0;  // Ignore camera column
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // if with "Clear Cell" behavior and all the selected cells are already empty,
  // then do nothing
  if (!withShift &&
      xsh->isRectEmpty(CellPosition(r0, c0), CellPosition(r1, c1)))
    return;

  std::set<int> removedColIds;
  // check if the operation may remove expression reference as column becomes
  // empty and deleted after the operation.
  if (!checkColumnRemoval(r0, c0, r1, c1, removedColIds)) return;

  TCellData *data = new TCellData();
  data->setCells(xsh, r0, c0, r1, c1);

  // clear empty column
  if (!removedColIds.empty()) {
    TUndoManager::manager()->beginBlock();
    // remove, then insert empty column
    for (auto colId : removedColIds) {
      // keep column name if specified
      std::string columnName = "";
      if (Preferences::instance()->isLinkColumnNameWithLevelEnabled() &&
          xsh->getStageObject(TStageObjectId::ColumnId(colId))
              ->hasSpecifiedName())
        columnName =
            xsh->getStageObject(TStageObjectId::ColumnId(colId))->getName();

      ColumnCmd::deleteColumn(colId, true);
      ColumnCmd::insertEmptyColumn(colId);

      if (!columnName.empty())
        TStageObjectCmd::rename(TStageObjectId::ColumnId(colId), columnName,
                                TApp::instance()->getCurrentXsheet());
    }
  }

  DeleteCellsUndo *undo =
      new DeleteCellsUndo(new TCellSelection(m_range), data, withShift);

  deleteCellsWithoutUndo(r0, c0, r1, c1, withShift);

  TUndoManager::manager()->add(undo);

  if (!removedColIds.empty()) {
    TUndoManager::manager()->endBlock();
  }

  // emit selectionChanged() signal so that the rename field will update
  // accordingly
  if (Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled())
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  else
    selectNone();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  if (undo->containsSoundColumn())
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
}

//-----------------------------------------------------------------------------

void TCellSelection::cutCells() { cutCells(false); }

//-----------------------------------------------------------------------------

void TCellSelection::cutCells(bool withoutCopy) {
  if (isEmpty()) return;

  CutCellsUndo *undo = new CutCellsUndo(new TCellSelection(m_range));

  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (c0 < 0) c0 = 0;  // Ignore camera column

  std::set<int> removedColIds;
  // check if the operation may remove expression reference as column becomes
  // empty and deleted after the operation.
  if (!checkColumnRemoval(r0, c0, r1, c1, removedColIds)) {
    delete undo;
    return;
  }

  undo->setCurrentData(r0, c0, r1, c1);
  if (!withoutCopy) copyCellsWithoutUndo(r0, c0, r1, c1);

  // clear empty column
  if (!removedColIds.empty()) {
    TUndoManager::manager()->beginBlock();
    // remove, then insert empty column
    for (auto colId : removedColIds) {
      ColumnCmd::deleteColumn(colId, true);
      ColumnCmd::insertEmptyColumn(colId);
    }
  }

  cutCellsWithoutUndo(r0, c0, r1, c1);

  TUndoManager::manager()->add(undo);

  if (!removedColIds.empty()) {
    TUndoManager::manager()->endBlock();
  }

  // cutCellsWithoutUndo will clear the selection, so select cells again
  if (Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
    selectCells(r0, c0, r1, c1);
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  } else
    selectNone();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  if (undo->containsSoundColumn())
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
}

//-----------------------------------------------------------------------------

void TCellSelection::insertCells() {
  if (isEmpty()) return;
  InsertUndo *undo = new InsertUndo(getSelectedCells());
  undo->redo();
  TUndoManager::manager()->add(undo);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void TCellSelection::pasteKeyframesInto() {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  const TKeyframeData *keyframeData = dynamic_cast<const TKeyframeData *>(
      QApplication::clipboard()->mimeData());
  if (keyframeData) {
    int r0, c0, r1, c1;
    getSelectedCells(r0, c0, r1, c1);

    TKeyframeSelection selection;
    if (isEmpty() && TApp::instance()->getCurrentObject()->getObjectId() ==
                         TStageObjectId::CameraId(xsh->getCameraColumnIndex()))
    // Se la selezione e' vuota e l'objectId e' quello della camera sono nella
    // colonna di camera quindi devo selezionare la row corrente e -1.
    {
      int row = TApp::instance()->getCurrentFrame()->getFrame();
      selection.select(row, -1);
    } else {
      // Retrieves all keyframe positions from mime data and translates them by
      // (r0,c0)
      XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
      std::set<TKeyframeSelection::Position> positions;
      int newC0 = c0;
      if (viewer && !viewer->orientation()->isVerticalTimeline())
        newC0 = c0 - keyframeData->getColumnSpanCount() + 1;
      positions.insert(TKeyframeSelection::Position(r0, newC0));
      keyframeData->getKeyframes(positions);
      selection.select(positions);
    }

    selection.pasteKeyframes();
  }
}

//-----------------------------------------------------------------------------

void TCellSelection::createBlankDrawing(int row, int col, bool multiple) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  if (col < 0) {
    if (!multiple)
      DVGui::warning(
          QObject::tr("Unable to create a blank drawing on the camera column"));
    return;
  }

  TXshColumn *column = xsh->getColumn(col);
  if (column && column->isLocked()) {
    if (!multiple) DVGui::warning(QObject::tr("The current column is locked"));
    return;
  }

  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  TApp::instance()->getCurrentFrame()->setCurrentFrame(row + 1);

  TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
  if (!level && Preferences::instance()->isAutoCreateEnabled() &&
      Preferences::instance()->isAnimationSheetEnabled()) {
    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    for (int r = std::min(r1, row); r > r0; r--) {
      TXshCell cell = xsh->getCell(r, col);
      if (cell.isEmpty()) continue;
      level = cell.m_level.getPointer();
      if (!level) continue;
      break;
    }
  }
  if (level) {
    int levelType = level->getType();
    if (levelType == ZERARYFX_XSHLEVEL || levelType == PLT_XSHLEVEL ||
        levelType == SND_XSHLEVEL || levelType == SND_TXT_XSHLEVEL ||
        levelType == MESH_XSHLEVEL) {
      if (!multiple)
        DVGui::warning(
            QObject::tr("Cannot create a blank drawing on the current column"));
      return;
    } else if (level->getSimpleLevel() &&
               level->getSimpleLevel()->isReadOnly()) {
      if (!multiple)
        DVGui::warning(QObject::tr("The current level is not editable"));
      return;
    }
  }

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();

  // If autocreate disabled, let's turn it on temporarily
  bool isAutoCreateEnabled = Preferences::instance()->isAutoCreateEnabled();
  if (!isAutoCreateEnabled)
    Preferences::instance()->setValue(EnableAutocreation, true, false);
  // Enable inserting in the hold cells temporarily too.
  bool isCreationInHoldCellsEnabled =
      Preferences::instance()->isCreationInHoldCellsEnabled();
  if (!isCreationInHoldCellsEnabled)
    Preferences::instance()->setValue(EnableCreationInHoldCells, true, false);

  TImage *img = toolHandle->getTool()->touchImage();

  TXshCell cell       = xsh->getCell(row, col);
  TXshSimpleLevel *sl = cell.getSimpleLevel();

  if (!img || !sl) {
    if (!isAutoCreateEnabled)
      Preferences::instance()->setValue(EnableAutocreation, false, false);
    if (!isCreationInHoldCellsEnabled)
      Preferences::instance()->setValue(EnableCreationInHoldCells, false,
                                        false);
    if (!multiple)
      DVGui::warning(QObject::tr(
          "Unable to create a blank drawing on the current column"));
    return;
  }

  if (!toolHandle->getTool()->m_isFrameCreated) {
    if (!isAutoCreateEnabled)
      Preferences::instance()->setValue(EnableAutocreation, false, false);
    if (!isCreationInHoldCellsEnabled)
      Preferences::instance()->setValue(EnableCreationInHoldCells, false,
                                        false);
    if (!multiple)
      DVGui::warning(QObject::tr(
          "Unable to replace the current drawing with a blank drawing"));
    return;
  }

  sl->setDirtyFlag(true);

  TPalette *palette = sl->getPalette();
  TFrameId frame    = cell.getFrameId();

  CreateBlankDrawingUndo *undo = new CreateBlankDrawingUndo(
      sl, frame, toolHandle->getTool()->m_isLevelCreated, palette);
  TUndoManager::manager()->add(undo);

  IconGenerator::instance()->invalidate(sl, frame);

  // Reset back to what these were
  if (!isAutoCreateEnabled)
    Preferences::instance()->setValue(EnableAutocreation, false, false);
  if (!isCreationInHoldCellsEnabled)
    Preferences::instance()->setValue(EnableCreationInHoldCells, false, false);
}

//-----------------------------------------------------------------------------

void TCellSelection::createBlankDrawings() {
  int col = TApp::instance()->getCurrentColumn()->getColumnIndex();
  int row = TApp::instance()->getCurrentFrame()->getFrameIndex();

  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  bool multiple = (r1 - r0 > 1) || (c1 - c0 > 1);

  TUndoManager::manager()->beginBlock();
  for (int c = c0; c <= c1; c++) {
    for (int r = r0; r <= r1; r++) {
      createBlankDrawing(r, c, multiple);
    }
  }
  TUndoManager::manager()->endBlock();

  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  TApp::instance()->getCurrentFrame()->setCurrentFrame(row + 1);
}

//-----------------------------------------------------------------------------

void TCellSelection::duplicateFrame(int row, int col, bool multiple) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  if (col < 0) {
    if (!multiple)
      DVGui::warning(QObject::tr(
          "There are no drawings in the camera column to duplicate"));
    return;
  }

  TXshColumn *column = xsh->getColumn(col);
  if (column && column->isLocked()) {
    if (!multiple) DVGui::warning(QObject::tr("The current column is locked"));
    return;
  }

  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  TApp::instance()->getCurrentFrame()->setCurrentFrame(row + 1);

  TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
  if (!level && Preferences::instance()->isAutoCreateEnabled() &&
      Preferences::instance()->isAnimationSheetEnabled()) {
    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    for (int r = std::min(r1, row); r > r0; r--) {
      TXshCell cell = xsh->getCell(r, col);
      if (cell.isEmpty()) continue;
      level = cell.m_level.getPointer();
      if (!level) continue;
      break;
    }
  }
  if (level) {
    int levelType = level->getType();
    if (levelType == ZERARYFX_XSHLEVEL || levelType == PLT_XSHLEVEL ||
        levelType == SND_XSHLEVEL || levelType == SND_TXT_XSHLEVEL ||
        levelType == MESH_XSHLEVEL) {
      if (!multiple)
        DVGui::warning(
            QObject::tr("Cannot duplicate a drawing in the current column"));
      return;
    } else if (level->getSimpleLevel() &&
               level->getSimpleLevel()->isReadOnly()) {
      if (!multiple)
        DVGui::warning(QObject::tr("The current level is not editable"));
      return;
    }
  }

  TXshCell targetCell = xsh->getCell(row, col);
  TXshCell prevCell   = xsh->getCell(row - 1, col);
  ;

  // check if we use the current cell to duplicate or the previous cell
  if (!targetCell.isEmpty() && targetCell != prevCell) {
    // Current cell has a drawing to duplicate and it's not a hold shift focus
    // to next cell as if they selected that one to duplicate into
    prevCell = targetCell;
    TApp::instance()->getCurrentFrame()->setCurrentFrame(row + 2);
    row++;
  }

  if (prevCell.isEmpty() || !(prevCell.m_level->getSimpleLevel())) return;

  TXshSimpleLevel *sl = prevCell.getSimpleLevel();
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;

  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();

  // If autocreate disabled, let's turn it on temporarily
  bool isAutoCreateEnabled = Preferences::instance()->isAutoCreateEnabled();
  if (!isAutoCreateEnabled)
    Preferences::instance()->setValue(EnableAutocreation, true, false);
  // Enable inserting in the hold cells temporarily too.
  bool isCreationInHoldCellsEnabled =
      Preferences::instance()->isCreationInHoldCellsEnabled();
  if (!isCreationInHoldCellsEnabled)
    Preferences::instance()->setValue(EnableCreationInHoldCells, true, false);

  TImage *img = toolHandle->getTool()->touchImage();
  if (!img) {
    if (!isAutoCreateEnabled)
      Preferences::instance()->setValue(EnableAutocreation, false, false);
    if (!isCreationInHoldCellsEnabled)
      Preferences::instance()->setValue(EnableCreationInHoldCells, false,
                                        false);
    if (!multiple)
      DVGui::warning(
          QObject::tr("Unable to duplicate a drawing on the current column"));
    return;
  }

  bool frameCreated = toolHandle->getTool()->m_isFrameCreated;
  if (!frameCreated) {
    if (!isAutoCreateEnabled)
      Preferences::instance()->setValue(EnableAutocreation, false, false);
    if (!isCreationInHoldCellsEnabled)
      Preferences::instance()->setValue(EnableCreationInHoldCells, false,
                                        false);
    if (!multiple)
      DVGui::warning(
          QObject::tr("Unable to replace the current or next drawing with a "
                      "duplicate drawing"));
    return;
  }

  targetCell           = xsh->getCell(row, col);
  TPalette *palette    = sl->getPalette();
  TFrameId srcFrame    = prevCell.getFrameId();
  TFrameId targetFrame = targetCell.getFrameId();
  std::set<TFrameId> frames;

  FilmstripCmd::duplicateFrameWithoutUndo(sl, srcFrame, targetFrame);

  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  DuplicateDrawingUndo *undo =
      new DuplicateDrawingUndo(sl, srcFrame, targetFrame);
  TUndoManager::manager()->add(undo);

  if (!isAutoCreateEnabled)
    Preferences::instance()->setValue(EnableAutocreation, false, false);
  if (!isCreationInHoldCellsEnabled)
    Preferences::instance()->setValue(EnableCreationInHoldCells, false, false);
}

//-----------------------------------------------------------------------------

void TCellSelection::duplicateFrames() {
  int col = TApp::instance()->getCurrentColumn()->getColumnIndex();
  int row = TApp::instance()->getCurrentFrame()->getFrameIndex();

  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  bool multiple = (r1 - r0 > 1) || (c1 - c0 > 1);

  TUndoManager::manager()->beginBlock();
  for (int c = c0; c <= c1; c++) {
    for (int r = r0; r <= r1; r++) {
      duplicateFrame(r, c, multiple);
    }
  }
  TUndoManager::manager()->endBlock();

  if (multiple) {
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
    TApp::instance()->getCurrentFrame()->setCurrentFrame(row + 1);
  }
}

//-----------------------------------------------------------------------------

void TCellSelection::openTimeStretchPopup() {
  if (!m_timeStretchPopup) m_timeStretchPopup = new TimeStretchPopup();
  m_timeStretchPopup->show();
  m_timeStretchPopup->raise();
  m_timeStretchPopup->activateWindow();
}

//-----------------------------------------------------------------------------

static bool operator<(const TXshCell &a, const TXshCell &b) {
  if (a.getSimpleLevel() < b.getSimpleLevel())
    return true;
  else if (a.getSimpleLevel() > b.getSimpleLevel())
    return false;
  return a.m_frameId < b.m_frameId;
}

//-----------------------------------------------------------------------------

static void dRenumberCells(int col, int r0, int r1) {
  typedef std::vector<std::pair<TFrameId, TFrameId>> FramesMap;
  typedef std::map<TXshCell, TXshCell> CellsMap;
  typedef std::map<TXshSimpleLevel *, FramesMap> LevelsTable;

  LevelsTable levelsTable;
  CellsMap cellsMap;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (int r = r0; r <= r1; ++r) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.getSimpleLevel() &&
        (r <= 0 || xsh->getCell(r - 1, col) != cell)) {
      // In case the cell was already mapped, skip
      TXshCell &toCell = cellsMap[cell];
      if (!toCell.isEmpty()) continue;

      // Build cell mapping
      TXshSimpleLevel *sl = cell.getSimpleLevel();

      TFrameId oldFid = cell.getFrameId();
      TFrameId newFid =
          TFrameId(r + 1, 0, oldFid.getZeroPadding(), oldFid.getStartSeqInd());

      toCell.m_level   = sl;
      toCell.m_frameId = newFid;

      // Build the level frames mapping
      if (sl->isFid(oldFid))
        levelsTable[sl].push_back(std::make_pair(oldFid, newFid));
    }
  }
  auto getNextLetter = [](const QString &letter) {
    if (letter.isEmpty()) return QString('a');
    QByteArray byteArray = letter.toUtf8();
    // return incrementing the last letter
    byteArray.data()[byteArray.size() - 1]++;
    return QString::fromUtf8(byteArray);
  };

  // Ensure renumber consistency in case some destination fid would overwrite
  // some unrenumbered fid in the level
  {
    CellsMap::iterator it, end = cellsMap.end();
    for (it = cellsMap.begin(); it != end; ++it) {
      if (cellsMap.find(it->second) == cellsMap.end() &&
          it->first.getSimpleLevel()->isFid(it->second.getFrameId())) {
        TFrameId &fid = it->second.m_frameId;
        fid = TFrameId(fid.getNumber(), getNextLetter(fid.getLetter()),
                       fid.getZeroPadding(), fid.getStartSeqInd());
      }
    }
  }

  // Renumber the level strip of each found level
  {
    LevelsTable::iterator it, end = levelsTable.end();
    for (it = levelsTable.begin(); it != end; ++it)
      FilmstripCmd::renumber(it->first, it->second, true);
  }
}

//-----------------------------------------------------------------------------

void TCellSelection::dRenumberCells() {
  TUndoManager *undoManager = TUndoManager::manager();

  undoManager->beginBlock();
  undoManager->add(new RenumberUndo::UndoNotifier);

  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  for (int c = c1; c >= c0; --c) ::dRenumberCells(c, r0, r1);

  undoManager->add(new RenumberUndo::RedoNotifier);
  undoManager->endBlock();

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

class PasteNewCellUndo final : public TUndo {
  TXshCell m_oldCell, m_newCell;
  TImageP m_img;
  int m_row, m_col;
  bool m_levelCreated;

public:
  PasteNewCellUndo(int row, int col, bool levelCreated)
      : m_row(row), m_col(col), m_levelCreated(levelCreated) {
    m_oldCell = getXsheet()->getCell(m_row, m_col);
  }
  void onAdd() override {
    m_newCell   = getXsheet()->getCell(m_row, m_col);
    TImageP img = m_newCell.getImage(false);
    if (img) m_img = img->cloneImage();
  }
  TXsheet *getXsheet() const {
    return TApp::instance()->getCurrentXsheet()->getXsheet();
  }
  TLevelSet *getLevelSet() const {
    return TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  }

  void undo() const override {
    getXsheet()->setCell(m_row, m_col, m_oldCell);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TXshSimpleLevel *sl = m_newCell.getSimpleLevel();
    if (sl) {
      sl->eraseFrame(m_newCell.getFrameId());
      if (m_levelCreated) {
        getLevelSet()->removeLevel(sl);
        TApp::instance()->getCurrentScene()->notifyCastChange();
      } else {
        TApp::instance()->getCurrentLevel()->notifyLevelChange();
      }
    }
  }
  void redo() const override {
    getXsheet()->setCell(m_row, m_col, m_newCell);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TXshSimpleLevel *sl = m_newCell.getSimpleLevel();
    if (sl) {
      TFrameId fid = m_newCell.getFrameId();
      sl->setFrame(fid, m_img);
      if (m_levelCreated) {
        getLevelSet()->insertLevel(sl);
        IconGenerator::instance()->invalidate(sl, fid);
        TApp::instance()->getCurrentScene()->notifyCastChange();
      } else {
        TApp::instance()->getCurrentLevel()->notifyLevelChange();
      }
    }
  }
  int getSize() const override {
    return sizeof(*this) + sizeof(TXshLevel);  // impreciso.
  }
};

//-----------------------------------------------------------------------------

// TODO: spostare queste funzioni in un posto piu' generale e riutilizzabile

static int getLevelType(const TImageP &img) {
  if (img->getType() == TImage::RASTER)
    return OVL_XSHLEVEL;
  else if (img->getType() == TImage::VECTOR)
    return PLI_XSHLEVEL;
  else if (img->getType() == TImage::TOONZ_RASTER)
    return TZP_XSHLEVEL;
  else
    return NO_XSHLEVEL;
}

//-----------------------------------------------------------------------------
/*
void setNewDrawing(TXsheet *xsh, int row, int col, const TImageP &img)
{
  // find a level type suitable for the image

  // search for the level
  TXshCell old = xsh->getCell(row,col);
  if(old.isEmpty())
  {
    for(int rr=row-1;rr>=0;rr--)
    {
      old = xsh->getCell(rr,col);
      if(!old.isEmpty())
        break;
    }
  }
  bool levelCreated = false;
  TXshSimpleLevel *sl;
  if(old.isEmpty())
  {
    // no level found. create it
    TXshLevel *xl = xsh->getScene()->createNewLevel(levelType);
    TApp::instance()->getCurrentScene()->notifyCastChange();
    levelCreated = true;
    sl = xl->getSimpleLevel();
  }
  else
  {
    // found a level. check its type
    sl = old.getSimpleLevel();
    if(sl->getType() != levelType)
      return;
  }
  // compute the frameid
  TFrameId fid(row+1);
  if(sl->isFid(fid))
  {
    fid = TFrameId(fid.getNumber(), 'a');
    while(fid.getLetter()<'z' && sl->isFid(fid))
      fid = TFrameId(fid.getNumber(), fid.getLetter()+1);
  }
  // add the new frame
  sl->setFrame(fid, img->cloneImage());
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  // undo
  TUndo *undo = new PasteNewCellUndo(row,col,levelCreated);
  // set xsheet cell
  xsh->setCell(row,col,TXshCell(sl,fid));
  TUndoManager::manager()->add(undo);
}
*/

//-----------------------------------------------------------------------------

static void createNewDrawing(TXsheet *xsh, int row, int col,
                             int preferredLevelType) {
  // search for the level
  TXshCell old = xsh->getCell(row, col);
  if (old.isEmpty()) {
    for (int rr = row - 1; rr >= 0; rr--) {
      old = xsh->getCell(rr, col);
      if (!old.isEmpty()) break;
    }
  }
  bool levelCreated = false;
  TXshSimpleLevel *sl;
  if (old.isEmpty()) {
    // no level found. create it
    TXshLevel *xl = xsh->getScene()->createNewLevel(preferredLevelType);
    sl            = xl->getSimpleLevel();
    TApp::instance()->getCurrentScene()->notifyCastChange();
    levelCreated = true;
  } else {
    sl = old.getSimpleLevel();
  }
  // compute the frameid
  TFrameId fid(row + 1);
  if (sl->isFid(fid)) {
    fid = TFrameId(fid.getNumber(), 'a');
    while (fid.getLetter().toUtf8().at(0) < 'z' && sl->isFid(fid))
      fid = TFrameId(fid.getNumber(), fid.getLetter().toUtf8().at(0) + 1);
  }
  // modify frameId to be with the same frame format as existing frames
  TFrameId tmplFId =
      xsh->getScene()->getProperties()->formatTemplateFIdForInput();
  sl->formatFId(fid, tmplFId);
  // add the new frame
  sl->setFrame(fid, sl->createEmptyFrame());
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  // undo
  TUndo *undo = new PasteNewCellUndo(row, col, levelCreated);
  // set xsheet cell
  xsh->setCell(row, col, TXshCell(sl, fid));
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

#define DYNAMIC_CAST(Type, Var, Data)                                          \
  const Type *Var = dynamic_cast<const Type *>(Data)

void TCellSelection::dPasteCells() {
  if (isEmpty()) return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  TUndoManager::manager()->beginBlock();
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  if (DYNAMIC_CAST(TCellData, cellData, mimeData)) {
    if (!cellData->canChange(xsh, c0)) {
      TUndoManager::manager()->endBlock();
      return;
    }
    for (int c = 0; c < cellData->getColCount(); c++) {
      for (int r = 0; r < cellData->getRowCount(); r++) {
        TXshCell src = cellData->getCell(r, c);
        if (src.getSimpleLevel())
          createNewDrawing(xsh, r0 + r, c0 + c,
                           src.getSimpleLevel()->getType());
      }
    }
  } else if (DYNAMIC_CAST(DrawingData, drawingData, mimeData)) {
    TXshSimpleLevel *level = drawingData->getLevel();
    if (level) {
      std::set<TFrameId> frameIds;
      drawingData->getFrames(frameIds);
      for (int i = 0; i < frameIds.size(); ++i)
        createNewDrawing(xsh, r0 + i, c0, level->getType());
    }
  } else if (DYNAMIC_CAST(StrokesData, strokesData, mimeData)) {
    createNewDrawing(xsh, r0, c0, PLI_XSHLEVEL);
  } else if (DYNAMIC_CAST(ToonzImageData, toonzImageData, mimeData)) {
    createNewDrawing(xsh, r0, c0, TZP_XSHLEVEL);
  } else if (DYNAMIC_CAST(FullColorImageData, fullColorImageData, mimeData)) {
    createNewDrawing(xsh, r0, c0, OVL_XSHLEVEL);
  } else {
  }
  pasteCells();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------
/*-- セルの上書きペースト --*/
void TCellSelection::overWritePasteCells() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData)) {
    if (isEmpty())  // Se la selezione delle celle e' vuota ritorno.
      return;

    if (cellData->getCellCount() == 0) {
      DVGui::error(QObject::tr("No data to paste."));
      return;
    }

    XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
    if (viewer && !viewer->orientation()->isVerticalTimeline()) {
      int cAdj = cellData->getColCount() - 1;
      c0 -= cAdj;
      c1 -= cAdj;
    }

    int oldR0 = r0;
    int oldC0 = c0;
    int oldR1 = r1;
    int oldC1 = c1;

    /*-- ペースト先の各カラムが、コピーした各セルと同種ならばOK --*/
    if (!cellData->canChange(xsh, c0)) return;

    // Check Circular References
    int i;
    for (i = 0; i < cellData->getCellCount(); i++) {
      if (!xsh->checkCircularReferences(cellData->getCell(i))) continue;
      DVGui::error(
          QObject::tr("It is not possible to paste the cells: there is a "
                      "circular reference."));
      return;
    }

    /*---- Undoのためにペースト前のCellDataを保存しておく --*/
    r1                    = r0 + cellData->getRowCount() - 1;
    c1                    = c0 + cellData->getColCount() - 1;
    TCellData *beforeData = new TCellData();
    beforeData->setCells(xsh, r0, c0, r1, c1);

    /*-- InsertをFalseにすることで、Overwriteペーストになる
            r1,c1はペースト範囲にあわせリサイズされる
    --*/
    bool isPaste = pasteCellsWithoutUndo(r0, c0, r1, c1, false);

    if (!isPaste) {
      delete beforeData;
      return;
    }

    /*-- 各カラムについて、ペースト前にカラムが空だったならtrue --*/
    std::vector<bool> areColumnsEmpty;
    bool containsSoundColumn = false;
    int c;
    /*-- ペースト後の各カラムについて --*/
    for (c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column) {
        areColumnsEmpty.push_back(false);
        continue;
      }
      if (!containsSoundColumn && column->getSoundColumn())
        containsSoundColumn = true;
      int newCr0, newCr1;
      /*-- 新たなカラムに何かペーストされているかどうか --*/
      column->getRange(newCr0, newCr1);
      areColumnsEmpty.push_back(!column || column->isEmpty() ||
                                (newCr0 == r0 && newCr1 == r1));
    }
    TUndoManager::manager()->beginBlock();

    // make sure that the pasting levels are registered in the scene cast
    // it may rename the level if there is another level with the same name
    std::set<TXshLevel *> pastedLevels;
    getLevelSetFromData(cellData, pastedLevels);
    LevelCmd::addMissingLevelsToCast(pastedLevels);

    /*-- r0,c0,r1,c1はペーストされた範囲　old付きはペースト前の選択範囲 --*/
    TUndoManager::manager()->add(new OverwritePasteCellsUndo(
        r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1, areColumnsEmpty, beforeData,
        containsSoundColumn));
    TUndoManager::manager()->endBlock();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    if (containsSoundColumn)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();

    delete beforeData;
  } else
    DVGui::error(QObject::tr("Cannot paste data \n Nothing to paste"));

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------
// special paste command - overwrite paste cell numbers only

void TCellSelection::overwritePasteNumbers() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  if (c0 < 0) c0 = 0;  // Ignore camera column

  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (const TCellData *cellData = dynamic_cast<const TCellData *>(mimeData)) {
    if (isEmpty()) return;

    if (cellData->getCellCount() == 0) {
      DVGui::error(QObject::tr("No data to paste."));
      return;
    }

    XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();
    if (viewer && !viewer->orientation()->isVerticalTimeline()) {
      int cAdj = cellData->getColCount() - 1;
      c0 -= cAdj;
      c1 -= cAdj;
    }

    int oldR0 = r0;
    int oldC0 = c0;
    int oldR1 = r1;
    int oldC1 = c1;

    // Check if the copied cells are the same type as the target columns
    if (!cellData->canChange(xsh, c0)) {
      DVGui::error(
          QObject::tr("It is not possible to paste the cells: "
                      "Some column is locked or column type is not match."));
      return;
    }

    // Check Circular References
    int i;
    for (i = 0; i < cellData->getCellCount(); i++) {
      if (!xsh->checkCircularReferences(cellData->getCell(i))) continue;
      DVGui::error(
          QObject::tr("It is not possible to paste the cells: there is a "
                      "circular reference."));
      return;
    }

    // store celldata for undo
    r1 = r0 + cellData->getRowCount() - 1;
    if (cellData->getColCount() != 1 || c0 == c1)
      c1 = c0 + cellData->getColCount() - 1;
    TCellData *beforeData = new TCellData();
    beforeData->setCells(xsh, r0, c0, r1, c1);

    bool isPaste = pasteNumbersWithoutUndo(r0, c0, r1, c1);

    if (!isPaste) {
      delete beforeData;
      return;
    }

    // r0,c0,r1,c1 : pasted region  oldR0,oldC0,oldR1,oldC1 : selected area
    // before pasting
    TUndoManager::manager()->add(new OverwritePasteNumbersUndo(
        r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1, beforeData));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);

    delete beforeData;
  } else
    DVGui::error(QObject::tr("Cannot paste data \n Nothing to paste"));

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------
// called from RenameCellField::RenameCell

void TCellSelection::renameCells(TXshCell &cell) {
  if (isEmpty()) return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  // register undo only if the cell is modified
  if (checkIfCellsHaveTheSameContent(r0, c0, r1, c1, cell)) return;
  TCellData *data = new TCellData();
  TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
  data->setCells(xsh, r0, c0, r1, c1);
  RenameCellsUndo *undo = new RenameCellsUndo(this, data, cell);
  undo->redo();

  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------
// rename cells for each columns with correspondent item in the list

void TCellSelection::renameMultiCells(QList<TXshCell> &cells) {
  if (isEmpty()) return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  assert(cells.size() == c1 - c0 + 1);
  // register undo only if the cell is modified
  bool somethingChanged = false;
  for (int c = c0; c <= c1; c++) {
    somethingChanged =
        !checkIfCellsHaveTheSameContent(r0, c, r1, c, cells[c - c0]);
    if (somethingChanged) break;
  }
  if (!somethingChanged) return;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TUndoManager::manager()->beginBlock();
  for (int c = c0; c <= c1; c++) {
    TCellData *data = new TCellData();
    data->setCells(xsh, r0, c, r1, c);
    RenameCellsUndo *undo =
        new RenameCellsUndo(r0, c, r1, c, data, cells[c - c0]);
    undo->redo();
    TUndoManager::manager()->add(undo);
  }
  TUndoManager::manager()->endBlock();
}

//=============================================================================
// CreateLevelUndo
//-----------------------------------------------------------------------------

class CreateLevelUndo final : public TUndo {
  int m_rowIndex;
  int m_columnIndex;
  int m_frameCount;
  int m_oldLevelCount;
  int m_step;
  TXshSimpleLevelP m_sl;
  bool m_areColumnsShifted;
  bool m_keepLevel;

public:
  CreateLevelUndo(int row, int column, int frameCount, int step,
                  bool areColumnsShifted, bool keepLevel = false)
      : m_rowIndex(row)
      , m_columnIndex(column)
      , m_frameCount(frameCount)
      , m_step(step)
      , m_sl(0)
      , m_keepLevel(keepLevel)
      , m_areColumnsShifted(areColumnsShifted) {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    m_oldLevelCount   = scene->getLevelSet()->getLevelCount();
  }
  ~CreateLevelUndo() { m_sl = 0; }

  void onAdd(TXshSimpleLevelP sl) { m_sl = sl; }

  void undo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    if (m_areColumnsShifted)
      xsh->removeColumn(m_columnIndex);
    else if (m_frameCount > 0)
      xsh->removeCells(m_rowIndex, m_columnIndex, m_frameCount);
    if (!m_keepLevel) {
      TLevelSet *levelSet = scene->getLevelSet();
      if (levelSet) {
        int m = levelSet->getLevelCount();
        while (m > 0 && m > m_oldLevelCount) {
          --m;
          TXshLevel *level = levelSet->getLevel(m);
          if (level) levelSet->removeLevel(level);
        }
      }
    }
    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    if (!m_sl.getPointer()) return;
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    scene->getLevelSet()->insertLevel(m_sl.getPointer());
    TXsheet *xsh = scene->getXsheet();
    if (m_areColumnsShifted) xsh->insertColumn(m_columnIndex);
    std::vector<TFrameId> fids;
    m_sl->getFids(fids);
    int i = m_rowIndex;
    int f = 0;
    while (i < m_frameCount + m_rowIndex) {
      TFrameId fid = (fids.size() != 0) ? fids[f] : i;
      TXshCell cell(m_sl.getPointer(), fid);
      f++;
      xsh->setCell(i, m_columnIndex, cell);
      int appo = i++;
      while (i < m_step + appo) xsh->setCell(i++, m_columnIndex, cell);
    }
    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
  QString getHistoryString() override {
    return QObject::tr("Create Level %1  at Column %2")
        .arg(QString::fromStdWString(m_sl->getName()))
        .arg(QString::number(m_columnIndex + 1));
  }
};

//-----------------------------------------------------------------------------
// Convert selected vector cells to ToonzRaster

bool TCellSelection::areOnlyVectorCellsSelected() {
  // set up basics
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  int i;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  TXshCell firstCell = xsh->getCell(r0, c0);
  if (firstCell.isEmpty()) {
    DVGui::error(QObject::tr("This command only works on vector cells."));
    return false;
  }

  TXshSimpleLevel *sourceSl = firstCell.getSimpleLevel();
  if (!sourceSl || sourceSl->getType() != PLI_XSHLEVEL) {
    DVGui::error(QObject::tr("This command only works on vector cells."));
    return false;
  }

  if (c0 != c1) {
    DVGui::error(
        QObject::tr("Please select only one column for this command."));
    return false;
  }

  for (i = r0; i <= r1; i++) {
    TXshCell testCell = xsh->getCell(i, c0);
    if (testCell.isEmpty() || testCell.getSimpleLevel() != sourceSl) {
      DVGui::error(
          QObject::tr("All selected cells must belong to the same level."));
      return false;
    }
  }
  return true;
}

TXshSimpleLevel *TCellSelection::getNewToonzRasterLevel(
    TXshSimpleLevel *sourceSl) {
  ToonzScene *scene       = TApp::instance()->getCurrentScene()->getScene();
  TFilePath sourcePath    = sourceSl->getPath();
  std::wstring sourceName = sourcePath.getWideName();
  TFilePath parentDir     = sourceSl->getPath().getParentDir();
  TFilePath fp            = scene->getDefaultLevelPath(TZP_XSHLEVEL, sourceName)
                     .withParentDir(parentDir);
  TFilePath actualFp = scene->decodeFilePath(fp);

  int i                = 1;
  std::wstring newName = sourceName;
  while (TSystem::doesExistFileOrLevel(actualFp)) {
    newName = sourceName + QString::number(i).toStdWString();
    fp      = scene->getDefaultLevelPath(TZP_XSHLEVEL, newName)
             .withParentDir(parentDir);
    actualFp = scene->decodeFilePath(fp);
    i++;
  }
  parentDir = scene->decodeFilePath(parentDir);

  TXshLevel *level =
      scene->createNewLevel(TZP_XSHLEVEL, newName, TDimension(), 0, fp);
  TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
  return sl;
}

//=============================================================================
// VectorToVectorUndo
//-----------------------------------------------------------------------------

class VectorToVectorUndo final : public TUndo {
  TXshSimpleLevelP m_sl;
  std::vector<TFrameId> m_frameIds;
  // std::vector<TFrameId> m_newFrameIds;
  std::vector<TImageP> m_oldImages;
  std::vector<TImageP> m_newImages;
  HookSet *m_oldLevelHooks;

public:
  VectorToVectorUndo(TXshSimpleLevelP sl, std::vector<TFrameId> frameIds,
                     std::vector<TImageP> oldImages,
                     std::vector<TImageP> newImages, HookSet *oldLevelHooks)
      : m_sl(sl)
      , m_frameIds(frameIds)
      , m_oldImages(oldImages)
      , m_newImages(newImages)
      , m_oldLevelHooks(oldLevelHooks) {}

  ~VectorToVectorUndo() {}

  void undo() const override {
    for (int i = 0; i < m_frameIds.size(); i++) {
      m_sl->getSimpleLevel()->setFrame(m_frameIds[i], m_oldImages[i]);
    }
    TApp::instance()->getCurrentLevel()->notifyLevelChange();

    // update all icons
    std::vector<TFrameId> sl_fids;
    m_sl.getPointer()->getFids(sl_fids);
    invalidateIcons(m_sl.getPointer(), sl_fids);

    *m_sl->getHookSet() = *m_oldLevelHooks;
  }

  void redo() const override {
    for (int i = 0; i < m_frameIds.size(); i++) {
      m_sl->getSimpleLevel()->setFrame(m_frameIds[i], m_newImages[i]);
    }
    TApp::instance()->getCurrentLevel()->notifyLevelChange();

    // update all icons
    std::vector<TFrameId> sl_fids;
    m_sl.getPointer()->getFids(sl_fids);
    invalidateIcons(m_sl.getPointer(), m_frameIds);

    *m_sl->getHookSet() = *m_oldLevelHooks;
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Simplify Vectors : Level %1")
                      .arg(QString::fromStdWString(m_sl->getName()));
    return str;
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------
// Convert selected vector cells to ToonzRaster

void TCellSelection::convertToToonzRaster() {
  // set up basics
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  TApp *app = TApp::instance();
  int row   = app->getCurrentFrame()->getFrame();
  int col   = app->getCurrentColumn()->getColumnIndex();
  int i;

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  TXshCell firstCell        = xsh->getCell(r0, c0);
  TXshSimpleLevel *sourceSl = firstCell.getSimpleLevel();

  if (!areOnlyVectorCellsSelected()) return;

  // set up new level
  TXshSimpleLevel *sl = getNewToonzRasterLevel(sourceSl);

  // get camera settings
  TCamera *camera = app->getCurrentScene()->getScene()->getCurrentCamera();
  double dpi      = camera->getDpi().x;
  int xres        = camera->getRes().lx;
  int yres        = camera->getRes().ly;

  assert(sl);

  sl->getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
  sl->getProperties()->setDpi(dpi);
  sl->getProperties()->setImageDpi(TPointD(dpi, dpi));
  sl->getProperties()->setImageRes(TDimension(xres, yres));

  TFrameId frameId = firstCell.getFrameId();
  std::set<TFrameId> frameIdsSet;
  std::vector<TFrameId> frameIds;
  frameIds.push_back(frameId);
  for (i = r0 + 1; i <= r1; i++) {
    TXshCell newCell    = xsh->getCell(i, c0);
    TFrameId newFrameId = xsh->getCell(i, c0).getFrameId();
    if (newCell.getSimpleLevel() == sourceSl) {
      if (std::find(frameIds.begin(), frameIds.end(), newCell.getFrameId()) ==
          frameIds.end()) {
        frameIds.push_back(newFrameId);
      }
    }
  }

  int totalImages = frameIds.size();
  for (i = 0; i < totalImages; i++) {
    frameIdsSet.insert(frameIds[i]);
  }

  DrawingData *data = new DrawingData();
  data->setLevelFrames(sourceSl, frameIdsSet);

  // This is where the copying actually happens
  std::set<TFrameId> newFrameIds;
  for (i = 0; i < totalImages; i++) {
    TRasterCM32P raster(xres, yres);
    raster->fill(TPixelCM32());
    TToonzImageP firstImage(raster, TRect());
    firstImage->setDpi(dpi, dpi);
    TFrameId newFrameId = TFrameId(i + 1);
    sl->setFrame(newFrameId, firstImage);
    newFrameIds.insert(newFrameId);
    firstImage->setSavebox(TRect(0, 0, xres - 1, yres - 1));
  }

  bool keepOriginalPalette;
  bool success = data->getLevelFrames(
      sl, newFrameIds, DrawingData::OVER_SELECTION, true, keepOriginalPalette,
      true);  // setting is redo = true skips the
              // question about the palette
  std::vector<TFrameId> newFids;
  sl->getFids(newFids);

  // make a new column
  col += 1;
  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  xsh->insertColumn(col);

  CreateLevelUndo *undo = new CreateLevelUndo(row, col, totalImages, 1, true);
  TUndoManager::manager()->add(undo);

  // expose the new frames in the column
  for (i = 0; i < totalImages; i++) {
    for (int k = r0; k <= r1; k++) {
      TXshCell oldCell = xsh->getCell(k, c0);
      TXshCell newCell(sl, newFids[i]);
      if (oldCell.getFrameId() == frameIds[i]) {
        xsh->setCell(k, col, newCell);
      }
    }
  }

  invalidateIcons(sl, newFrameIds);
  sl->save(sl->getPath(), TFilePath(), true);

  DvDirModel::instance()->refreshFolder(sl->getPath().getParentDir());

  undo->onAdd(sl);

  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->notifyCastChange();
  app->getCurrentXsheet()->notifyXsheetChanged();

  app->getCurrentTool()->onImageChanged(
      (TImage::Type)app->getCurrentImageType());
}

//-----------------------------------------------------------------------------
// Convert selected vector cells to ToonzRaster and back to vecor

void TCellSelection::convertVectortoVector() {
  // set up basics
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  TApp *app = TApp::instance();
  int row   = app->getCurrentFrame()->getFrame();
  int col   = app->getCurrentColumn()->getColumnIndex();
  int i;

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  TXshCell firstCell        = xsh->getCell(r0, c0);
  TXshSimpleLevel *sourceSl = firstCell.getSimpleLevel();
  if (!areOnlyVectorCellsSelected()) return;

  // set up new level name
  TXshSimpleLevel *sl = getNewToonzRasterLevel(sourceSl);
  assert(sl);

  // get camera settings
  TCamera *camera = app->getCurrentScene()->getScene()->getCurrentCamera();
  double dpi      = camera->getDpi().x;
  int xres        = camera->getRes().lx;
  int yres        = camera->getRes().ly;

  if (Preferences::instance()->getUseHigherDpiOnVectorSimplify()) {
    dpi *= 2;
    xres *= 2;
    yres *= 2;
  }

  sl->getProperties()->setDpiPolicy(LevelProperties::DP_ImageDpi);
  sl->getProperties()->setDpi(dpi);
  sl->getProperties()->setImageDpi(TPointD(dpi, dpi));
  sl->getProperties()->setImageRes(TDimension(xres, yres));

  // Get the used FrameIds and images
  // The cloned images are used in the undo.
  TFrameId frameId = firstCell.getFrameId();
  std::set<TFrameId> frameIdsSet;
  std::vector<TFrameId> frameIds;
  std::vector<TImageP> oldImages;
  frameIds.push_back(frameId);
  oldImages.push_back(firstCell.getImage(false).getPointer()->cloneImage());
  for (i = r0 + 1; i <= r1; i++) {
    TXshCell newCell    = xsh->getCell(i, c0);
    TFrameId newFrameId = xsh->getCell(i, c0).getFrameId();
    if (newCell.getSimpleLevel() == sourceSl) {
      if (std::find(frameIds.begin(), frameIds.end(), newCell.getFrameId()) ==
          frameIds.end()) {
        frameIds.push_back(newFrameId);
        oldImages.push_back(newCell.getImage(false)->cloneImage());
      }
    }
  }

  int totalImages = frameIds.size();

  for (i = 0; i < totalImages; i++) {
    frameIdsSet.insert(frameIds[i]);
  }

  DrawingData *data = new DrawingData();
  data->setLevelFrames(sourceSl, frameIdsSet);

  // Make empty frames for the new data
  std::set<TFrameId> newFrameIds;
  for (i = 0; i < totalImages; i++) {
    TRasterCM32P raster(xres, yres);
    raster->fill(TPixelCM32());
    TToonzImageP firstImage(raster, TRect());
    firstImage->setDpi(dpi, dpi);
    TFrameId newFrameId = TFrameId(i + 1);
    sl->setFrame(newFrameId, firstImage);
    newFrameIds.insert(newFrameId);
    firstImage->setSavebox(TRect(0, 0, xres - 1, yres - 1));
  }

  // This is where the copying actually happens
  // copy old frames to Toonz Raster
  bool keepOriginalPalette;
  bool success = data->getLevelFrames(
      sl, newFrameIds, DrawingData::OVER_SELECTION, true, keepOriginalPalette,
      true);  // setting is redo = true skips the
              // question about the palette
              // get the new FrameIds
  std::vector<TFrameId> newFids;
  sl->getFids(newFids);

  // copy the Toonz Raster frames onto the old level
  data->clear();
  data->setLevelFrames(sl, newFrameIds);
  if (Preferences::instance()->getKeepFillOnVectorSimplify())
    data->setKeepVectorFills(true);
  success = data->getLevelFrames(
      sourceSl, frameIdsSet, DrawingData::OVER_SELECTION, true,
      keepOriginalPalette, true);  // setting is redo = true skips the
                                   // question about the palette

  // get clones of the new images for undo
  std::vector<TImageP> newImages;
  for (i = 0; i < totalImages; i++) {
    newImages.push_back(sourceSl->getFrame(frameIds[i], false)->cloneImage());
  }

  HookSet *oldLevelHooks = new HookSet();
  *oldLevelHooks         = *sourceSl->getHookSet();

  TUndoManager::manager()->add(new VectorToVectorUndo(
      sourceSl, frameIds, oldImages, newImages, oldLevelHooks));

  invalidateIcons(sourceSl, frameIdsSet);

  // remove the toonz raster level
  sl->clearFrames();
  app->getCurrentScene()->getScene()->getLevelSet()->removeLevel(sl, true);

  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->notifyCastChange();
  app->getCurrentXsheet()->notifyXsheetChanged();

  app->getCurrentTool()->onImageChanged(
      (TImage::Type)app->getCurrentImageType());
}

void TCellSelection::fillEmptyCell() {
  if (isEmpty()) return;

  // set up basics
  bool initUndo = false;
  TXsheet *xsh  = TApp::instance()->getCurrentXsheet()->getXsheet();
  int r, r0, c0, c, r1, c1;

  getSelectedCells(r0, c0, r1, c1);

  for (c = c0; c <= c1; c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (!column || column->isEmpty() || column->isLocked() ||
        column->getSoundColumn())
      continue;

    for (r = r1; r >= r0; r--) {
      int fillCount = 0;
      TXshCell cell = xsh->getCell(r, c);

      if (cell.isEmpty()) fillCount++;

      int prevR = r - 1;
      while (prevR >= 0) {
        cell = xsh->getCell(prevR, c);
        if (!cell.isEmpty()) break;

        prevR--;
        fillCount++;
      }

      // We've gone past the beginning of timeline without finding a cell.
      // Do nothing, and end current column processing immediately.
      if (prevR < 0) break;

      // Adjacent cell is filled.  Move to next one.
      if (!fillCount) continue;

      // Ok, time fill
      int startR = prevR + 1;
      int endR   = startR + fillCount - 1;

      if (!initUndo) {
        initUndo = true;
        TUndoManager::manager()->beginBlock();
      }

      FillEmptyCellUndo *undo = new FillEmptyCellUndo(startR, endR, c, cell);
      TUndoManager::manager()->add(undo);
      undo->redo();

      // Let's skip cells we just filled
      r = prevR;
    }
  }

  if (initUndo) TUndoManager::manager()->endBlock();

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}