

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

// TnzTools includes
#include "tools/toolutils.h"
#include "tools/toolhandle.h"

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

  bool ret = cellData->getCells(xsh, r0, c0, r1, c1, insert, doZeraryClone);
  if (!ret) return false;

  return true;
}

//-----------------------------------------------------------------------------

void deleteCellsWithoutUndo(int &r0, int &c0, int &r1, int &c1) {
  try {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int c;
    for (c = c0; c <= c1; c++) {
      xsh->clearCells(r0, c, r1 - r0 + 1);
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->isEmpty()) {
        TFx *fx = column->getFx();
        if (fx) {
          int i;
          for (i = fx->getOutputConnectionCount() - 1; i >= 0; i--) {
            TFxPort *port = fx->getOutputConnection(i);
            port->setFx(0);
          }
        }
      }
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
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
    TXshColumn *column = xsh->getColumn(c);
    if (column && column->isEmpty()) {
      TFx *fx = column->getFx();
      if (!fx) continue;
      int i;
      for (i = fx->getOutputConnectionCount() - 1; i >= 0; i--) {
        TFxPort *port = fx->getOutputConnection(i);
        port->setFx(0);
      }
    }
  }

  // Se la selezione corrente e' TCellSelection svuoto la selezione.
  TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (cellSelection) cellSelection->selectNone();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================
//  PasteCellsUndo
//-----------------------------------------------------------------------------

class PasteCellsUndo final : public TUndo {
  TCellSelection *m_oldSelection;
  TCellSelection *m_newSelection;
  TCellData *m_data;
  std::vector<bool> m_areOldColumnsEmpty;

public:
  PasteCellsUndo(int r0, int c0, int r1, int c1, int oldR0, int oldC0,
                 int oldR1, int oldC1, const std::vector<bool> &areColumnsEmpty)
      : m_areOldColumnsEmpty(areColumnsEmpty) {
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

    int c0BeforeCut = c0;
    int c1BeforeCut = c1;
    // Cut delle celle che sono in newSelection
    cutCellsWithoutUndo(r0, c0, r1, c1);
    // Se le colonne erano vuote le resetto (e' necessario farlo per le colonne
    // particolari, colonne sount o palette)
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    assert(c1BeforeCut - c0BeforeCut + 1 == (int)m_areOldColumnsEmpty.size());
    int c;
    for (c = c0BeforeCut; c <= c1BeforeCut; c++) {
      if (!m_areOldColumnsEmpty[c - c0BeforeCut] || !xsh->getColumn(c))
        continue;
    }

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, c1, r1);
    // Cut delle celle che sono in newSelection
    pasteCellsWithoutUndo(m_data, r0, c0, c1, r1, true, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Paste Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  DeleteCellsUndo
//-----------------------------------------------------------------------------

class DeleteCellsUndo final : public TUndo {
  TCellSelection *m_selection;
  QMimeData *m_data;
  QMap<int, QList<TFxPort *>> m_outputConnections;
  QMap<int, TXshColumn *> m_columns;

public:
  DeleteCellsUndo(TCellSelection *selection, QMimeData *data) : m_data(data) {
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int i;
    for (i = c0; i <= c1; i++) {
      TXshColumn *col = xsh->getColumn(i);
      if (!col || col->isEmpty()) continue;
      int colr0, colr1;
      col->getRange(colr0, colr1);
      if (r0 <= colr0 && r1 >= colr1 && !col->getLevelColumn()) {
        // la colonna verra' rimossa dall'xsheet
        m_columns[i] = col;
        col->addRef();
      }
      TFx *fx = col->getFx();
      if (!fx) continue;
      int j;
      QList<TFxPort *> fxPorts;
      for (j = 0; j < fx->getOutputConnectionCount(); j++)
        fxPorts.append(fx->getOutputConnection(j));
      if (fxPorts.isEmpty()) continue;
      m_outputConnections[i] = fxPorts;
    }
  }

  ~DeleteCellsUndo() {
    delete m_selection;
    QMap<int, TXshColumn *>::iterator it;
    for (it = m_columns.begin(); it != m_columns.end(); it++)
      it.value()->release();
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    // devo rimettere le colonne che ho rimosso dall'xsheet
    QMap<int, TXshColumn *>::const_iterator colIt;
    for (colIt = m_columns.begin(); colIt != m_columns.end(); colIt++) {
      int index          = colIt.key();
      TXshColumn *column = colIt.value();
      xsh->removeColumn(index);
      xsh->insertColumn(index, column);
    }

    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    QMap<int, QList<TFxPort *>>::const_iterator it;
    for (it = m_outputConnections.begin(); it != m_outputConnections.end();
         it++) {
      TXshColumn *col          = xsh->getColumn(it.key());
      QList<TFxPort *> fxPorts = it.value();
      int i;
      for (i = 0; i < fxPorts.size(); i++) fxPorts[i]->setFx(col->getFx());
    }

    const TCellData *cellData = dynamic_cast<const TCellData *>(m_data);
    pasteCellsWithoutUndo(cellData, r0, c0, r1, c1, false, false);

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    deleteCellsWithoutUndo(r0, c0, r1, c1);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Delete Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  CutCellsUndo
//-----------------------------------------------------------------------------

class CutCellsUndo final : public TUndo {
  TCellSelection *m_selection;
  TCellData *m_data;
  QMap<int, QList<TFxPort *>> m_outputConnections;

public:
  CutCellsUndo(TCellSelection *selection) : m_data() {
    int r0, c0, r1, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    m_selection = new TCellSelection();
    m_selection->selectCells(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int i;
    for (i = c0; i <= c1; i++) {
      TXshColumn *col = xsh->getColumn(i);
      if (!col || col->isEmpty()) continue;
      TFx *fx = col->getFx();
      if (!fx) continue;
      int j;
      QList<TFxPort *> fxPorts;
      for (j = 0; j < fx->getOutputConnectionCount(); j++)
        fxPorts.append(fx->getOutputConnection(j));
      if (fxPorts.isEmpty()) continue;
      m_outputConnections[i] = fxPorts;
    }
  }

  void setCurrentData(int r0, int c0, int r1, int c1) {
    m_data       = new TCellData();
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    m_data->setCells(xsh, r0, c0, r1, c1);
  }

  ~CutCellsUndo() {
    delete m_selection;
    delete m_data;
  }

  void undo() const override {
    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    QMap<int, QList<TFxPort *>>::const_iterator it;
    for (it = m_outputConnections.begin(); it != m_outputConnections.end();
         it++) {
      TXshColumn *col          = xsh->getColumn(it.key());
      QList<TFxPort *> fxPorts = it.value();
      int i;
      for (i = 0; i < fxPorts.size(); i++) fxPorts[i]->setFx(col->getFx());
    }

    pasteCellsWithoutUndo(m_data, r0, c0, r1, c1, true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    QClipboard *clipboard  = QApplication::clipboard();
    QMimeData *currentData = cloneData(clipboard->mimeData());

    int r0, c0, r1, c1;
    m_selection->getSelectedCells(r0, c0, r1, c1);
    cutCellsWithoutUndo(r0, c0, r1, c1);

    clipboard->setMimeData(currentData, QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Cut Cells"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  InsertUndo
//-----------------------------------------------------------------------------

class InsertUndo final : public TUndo {
  TCellSelection::Range m_range;

public:
  InsertUndo(const TCellSelection::Range &range) : m_range(range) {}

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int rowCount = m_range.getRowCount();
    int c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++)
      xsh->removeCells(m_range.m_r0, c, rowCount);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int rowCount = m_range.getRowCount();
    int c;
    for (c = m_range.m_c0; c <= m_range.m_c1; c++)
      xsh->insertCells(m_range.m_r0, c, rowCount);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
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
    sl                = cell.getSimpleLevel();
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
    if (sl->getType() == OVL_XSHLEVEL && sl->getPath().getType() == "psd")
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
      oldPalette                 = cell.getSimpleLevel()->getPalette();
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
  TCamera *camera   = scene->getCurrentCamera();
  if (cell.isEmpty()) {
    if (row > 0) cell = xsh->getCell(row - 1, col);
    sl                = cell.getSimpleLevel();
    if (!sl || (sl->getType() == OVL_XSHLEVEL &&
                sl->getPath().getFrame() == TFrameId::NO_FRAME)) {
      int levelType;
      if (dynamic_cast<const ToonzImageData *>(rasterImageData))
        levelType = TZP_XSHLEVEL;
      else if (dynamic_cast<const FullColorImageData *>(rasterImageData))
        levelType   = OVL_XSHLEVEL;
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
      sl->setFrame(fid, img);
      app->getCurrentLevel()->setLevel(sl);
      app->getCurrentLevel()->notifyLevelChange();
      sl->save();
    } else {
      img = sl->createEmptyFrame();
      assert(img);
      std::vector<TFrameId> fids;
      sl->getFids(fids);
      if (fids.size() > 0) fid = TFrameId(fids.back().getNumber() + 1);
      sl->setFrame(fid, img);
    }
    xsh->setCell(row, col, TXshCell(sl, fid));
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
    if (rects.size() > 0) boxD   = rects[0];
    if (strokes.size() > 0) boxD = strokes[0].getBBox();
    for (i = 0; i < rects.size(); i++) boxD += rects[i];
    for (i = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
    boxD   = affine * boxD;
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
                                 bool isLevelCreated)
      : ToolUtils::TFullColorRasterUndo(tiles, level, id, createdFrame,
                                        isLevelCreated, oldPalette)
      , m_rasterImageData(data->clone()) {}

  ~PasteFullColorImageInCellsUndo() { delete m_rasterImageData; }

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TTileSet *tiles = 0;
    bool isLevelCreated;
    pasteRasterImageInCellWithoutUndo(m_row, m_col, m_rasterImageData, &tiles,
                                      isLevelCreated);
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
                                    const set<TFrameId> &frameIds, int r0,
                                    int c0) {
  int frameToInsert = frameIds.size();
  xsh->insertCells(r0, c0, frameToInsert);
  set<TFrameId>::const_iterator it;
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
  set<TFrameId> m_frameIds;
  TXshSimpleLevelP m_level;

public:
  PasteDrawingsInCellUndo(TXshSimpleLevel *level, const set<TFrameId> &frameIds,
                          int r0, int c0)
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

public:
  OverwritePasteCellsUndo(int r0, int c0, int r1, int c1, int oldR0, int oldC0,
                          int oldR1, int oldC1,
                          const std::vector<bool> &areColumnsEmpty,
                          TCellData *beforeData)
      : m_areOldColumnsEmpty(areColumnsEmpty) {
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
  }

  void redo() const override {
    int r0, c0, r1, c1;
    m_newSelection->getSelectedCells(r0, c0, c1, r1);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);
    // Cut delle celle che sono in newSelection
    pasteCellsWithoutUndo(r0, c0, c1, r1, false, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
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

TCellSelection::TCellSelection() : m_timeStretchPopup(0), m_reframePopup(0) {}

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

  enableCommand(this, MI_Copy, &TCellSelection::copyCells);
  enableCommand(this, MI_Paste, &TCellSelection::pasteCells);

  if (dynamic_cast<const TKeyframeData *>(
          QApplication::clipboard()->mimeData()))
    enableCommand(this, MI_PasteInto, &TCellSelection::pasteKeyframesInto);

  enableCommand(this, MI_Cut, &TCellSelection::cutCells);
  enableCommand(this, MI_Clear, &TCellSelection::deleteCells);
  enableCommand(this, MI_Insert, &TCellSelection::insertCells);

  enableCommand(this, MI_PasteInto, &TCellSelection::overWritePasteCells);

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
                                        MI_ConvertToToonzRaster,
                                        MI_ConvertVectorToVector};
  return commands.contains(commandId);
}

//-----------------------------------------------------------------------------

bool TCellSelection::isEmpty() const { return m_range.isEmpty(); }

//-----------------------------------------------------------------------------

void TCellSelection::selectCells(int r0, int c0, int r1, int c1) {
  if (r0 > r1) tswap(r0, r1);
  if (c0 > c1) tswap(c0, c1);
  m_range.m_r0            = r0;
  m_range.m_c0            = c0;
  m_range.m_r1            = r1;
  m_range.m_c1            = c1;
  bool onlyOneRasterLevel = containsOnlyOneRasterLevel(r0, c0, r1, c1);
  CommandManager::instance()->enable(MI_CanvasSize, onlyOneRasterLevel);
}

//-----------------------------------------------------------------------------

void TCellSelection::selectCell(int row, int col) {
  m_range.m_r0            = row;
  m_range.m_c0            = col;
  m_range.m_r1            = row;
  m_range.m_c1            = col;
  bool onlyOneRasterLevel = containsOnlyOneRasterLevel(row, col, row, col);
  CommandManager::instance()->enable(MI_CanvasSize, onlyOneRasterLevel);
}

//-----------------------------------------------------------------------------

void TCellSelection::selectNone() {
  m_range = Range();
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
                                   const RasterImageData *rasterImageData) {
  TXsheet *xsh         = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshCell cell        = xsh->getCell(row, col);
  bool createdFrame    = false;
  bool isLevelCreated  = false;
  TPaletteP oldPalette = 0;
  if (!cell.getSimpleLevel()) {
    createdFrame        = true;
    TXshSimpleLevel *sl = xsh->getCell(row - 1, col).getSimpleLevel();
    if (sl) oldPalette  = sl->getPalette();
  } else {
    TXshSimpleLevel *sl = cell.getSimpleLevel();
    if (sl->getType() == OVL_XSHLEVEL && sl->getPath().getType() == "psd")
      return;
    oldPalette = sl->getPalette();
  }
  if (oldPalette) oldPalette = oldPalette->clone();
  TTileSet *tiles            = 0;
  bool isPaste = pasteRasterImageInCellWithoutUndo(row, col, rasterImageData,
                                                   &tiles, isLevelCreated);
  if (isLevelCreated && oldPalette.getPointer()) oldPalette = 0;
  if (!isPaste) return;
  cell = xsh->getCell(row, col);

  TTileSetCM32 *cm32Tiles           = dynamic_cast<TTileSetCM32 *>(tiles);
  TTileSetFullColor *fullColorTiles = dynamic_cast<TTileSetFullColor *>(tiles);
  if (cm32Tiles) {
    TUndoManager::manager()->add(new PasteToonzImageInCellsUndo(
        row, col, rasterImageData, cm32Tiles, cell.getSimpleLevel(),
        cell.getFrameId(), oldPalette, createdFrame, isLevelCreated));
  } else if (fullColorTiles) {
    TUndoManager::manager()->add(new PasteFullColorImageInCellsUndo(
        rasterImageData, fullColorTiles, cell.getSimpleLevel(),
        cell.getFrameId(), oldPalette, createdFrame, isLevelCreated));
  }
}

//-----------------------------------------------------------------------------

void TCellSelection::pasteCells() {
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  XsheetViewer *viewer      = TApp::instance()->getCurrentXsheetViewer();

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

    bool isPaste = pasteCellsWithoutUndo(cellData, r0, c0, r1, c1);

    // Se la selezione corrente e' TCellSelection selezione le celle copiate
    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection) cellSelection->selectCells(r0, c0, r1, c1);

    // Controllo se le colonne erano vuote prima del paste
    // n.b. devo farlo dopo il paste perche' prima non ho il range delle colonne
    // incollate corrette.
    std::vector<bool> areColumnsEmpty;
    int c;
    for (c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column) {
        areColumnsEmpty.push_back(false);
        continue;
      }
      int newCr0, newCr1;
      column->getRange(newCr0, newCr1);
      areColumnsEmpty.push_back(!column || column->isEmpty() ||
                                (newCr0 == r0 && newCr1 == r1));
    }
    if (!isPaste) return;
    initUndo = true;
    TUndoManager::manager()->beginBlock();
    TUndoManager::manager()->add(new PasteCellsUndo(
        r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1, areColumnsEmpty));
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
    if (isEmpty() &&
        TApp::instance()->getCurrentObject()->getObjectId() ==
            TStageObjectId::CameraId(0))
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
      if (viewer && !viewer->orientation()->isVerticalTimeline())
        newC0 = c0 - keyframeData->getColumnSpanCount() + 1;
      positions.insert(TKeyframeSelection::Position(r0, newC0));
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

    set<TFrameId> frameIds;
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
  if (const RasterImageData *rasterImageData =
          dynamic_cast<const RasterImageData *>(mimeData)) {
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
    const FullColorImageData *fullColData =
        dynamic_cast<const FullColorImageData *>(rasterImageData);
    TToonzImageP ti(img);
    TVectorImageP vi(img);
    if (!initUndo) {
      initUndo = true;
      TUndoManager::manager()->beginBlock();
    }
    if (fullColData && (vi || ti)) {
      DVGui::error(QObject::tr(
          "The copied selection cannot be pasted in the current drawing."));
      return;
    }
    if (vi) {
      TXshSimpleLevel *sl = xsh->getCell(r0, c0).getSimpleLevel();
      if (!sl) sl         = xsh->getCell(r0 - 1, c0).getSimpleLevel();
      assert(sl);
      StrokesData *strokesData = rasterImageData->toStrokesData(sl->getScene());
      pasteStrokesInCell(r0, c0, strokesData);
    } else
      pasteRasterImageInCell(r0, c0, rasterImageData);
  }
  if (!initUndo) {
    DVGui::error(QObject::tr(
        "It is not possible to paste data: there is nothing to paste."));
    return;
  }
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void TCellSelection::deleteCells() {
  if (isEmpty()) return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // if all the selected cells are already empty, then do nothing
  if (xsh->isRectEmpty(CellPosition(r0, c0), CellPosition(r1, c1))) return;
  TCellData *data = new TCellData();
  data->setCells(xsh, r0, c0, r1, c1);
  DeleteCellsUndo *undo =
      new DeleteCellsUndo(new TCellSelection(m_range), data);

  deleteCellsWithoutUndo(r0, c0, r1, c1);
  // emit selectionChanged() signal so that the rename field will update
  // accordingly
  if (Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled())
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  else
    selectNone();

  TUndoManager::manager()->add(undo);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void TCellSelection::cutCells() { cutCells(false); }

//-----------------------------------------------------------------------------

void TCellSelection::cutCells(bool withoutCopy) {
  if (isEmpty()) return;

  CutCellsUndo *undo = new CutCellsUndo(new TCellSelection(m_range));

  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);

  undo->setCurrentData(r0, c0, r1, c1);
  if (!withoutCopy) copyCellsWithoutUndo(r0, c0, r1, c1);
  cutCellsWithoutUndo(r0, c0, r1, c1);

  TUndoManager::manager()->add(undo);
  // cutCellsWithoutUndo will clear the selection, so select cells again
  if (Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
    selectCells(r0, c0, r1, c1);
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
  } else
    selectNone();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
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
  const TKeyframeData *keyframeData = dynamic_cast<const TKeyframeData *>(
      QApplication::clipboard()->mimeData());
  if (keyframeData) {
    int r0, c0, r1, c1;
    getSelectedCells(r0, c0, r1, c1);

    TKeyframeSelection selection;
    if (isEmpty() &&
        TApp::instance()->getCurrentObject()->getObjectId() ==
            TStageObjectId::CameraId(0))
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
      TFrameId newFid = TFrameId(r + 1);

      toCell.m_level   = sl;
      toCell.m_frameId = newFid;

      // Build the level frames mapping
      if (sl->isFid(oldFid))
        levelsTable[sl].push_back(std::make_pair(oldFid, newFid));
    }
  }

  // Ensure renumber consistency in case some destination fid would overwrite
  // some unrenumbered fid in the level
  {
    CellsMap::iterator it, end = cellsMap.end();
    for (it = cellsMap.begin(); it != end; ++it) {
      if (cellsMap.find(it->second) == cellsMap.end() &&
          it->first.getSimpleLevel()->isFid(it->second.getFrameId())) {
        TFrameId &fid = it->second.m_frameId;
        fid           = TFrameId(fid.getNumber(),
                       fid.getLetter() ? fid.getLetter() + 1 : 'a');
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
    m_newCell      = getXsheet()->getCell(m_row, m_col);
    TImageP img    = m_newCell.getImage(false);
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
    while (fid.getLetter() < 'z' && sl->isFid(fid))
      fid = TFrameId(fid.getNumber(), fid.getLetter() + 1);
  }
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
  if (isEmpty())  // Se la selezione delle celle e' vuota ritorno.
    return;
  int r0, c0, r1, c1;
  getSelectedCells(r0, c0, r1, c1);
  TUndoManager::manager()->beginBlock();
  TXsheet *xsh              = TApp::instance()->getCurrentXsheet()->getXsheet();
  QClipboard *clipboard     = QApplication::clipboard();
  const QMimeData *mimeData = clipboard->mimeData();
  if (DYNAMIC_CAST(TCellData, cellData, mimeData)) {
    if (!cellData->canChange(xsh, c0)) return;
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
      set<TFrameId> frameIds;
      drawingData->getFrames(frameIds);
      int r = r0;
      for (set<TFrameId>::iterator it = frameIds.begin(); it != frameIds.end();
           ++it)
        createNewDrawing(xsh, r++, c0, level->getType());
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

    /*-- InsertをFalseにすることで、Ovewriteペーストになる
            r1,c1はペースト範囲にあわせリサイズされる
    --*/
    bool isPaste = pasteCellsWithoutUndo(r0, c0, r1, c1, false);

    if (!isPaste) {
      delete beforeData;
      return;
    }

    /*-- 各カラムについて、ペースト前にカラムが空だったならtrue --*/
    std::vector<bool> areColumnsEmpty;
    int c;
    /*-- ペースト後の各カラムについて --*/
    for (c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (!column) {
        areColumnsEmpty.push_back(false);
        continue;
      }
      int newCr0, newCr1;
      /*-- 新たなカラムに何かペーストされているかどうか --*/
      column->getRange(newCr0, newCr1);
      areColumnsEmpty.push_back(!column || column->isEmpty() ||
                                (newCr0 == r0 && newCr1 == r1));
    }
    /*-- r0,c0,r1,c1はペーストされた範囲　old付きはペースト前の選択範囲 --*/
    TUndoManager::manager()->add(
        new OverwritePasteCellsUndo(r0, c0, r1, c1, oldR0, oldC0, oldR1, oldC1,
                                    areColumnsEmpty, beforeData));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);

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
      c1                  = c0 + cellData->getColCount() - 1;
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
  if (sourceSl->getType() != PLI_XSHLEVEL) {
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
  int i, j;

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
  int i, j;

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