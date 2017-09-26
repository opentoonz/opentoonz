

// TnzCore includes
#include "tsystem.h"
#include "tundo.h"
#include "tpalette.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelset.h"
#include "toonz/hook.h"
#include "toonz/levelproperties.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// Tnz6 includes
#include "tapp.h"
#include "cellselection.h"
#include "columnselection.h"
#include "keyframeselection.h"
#include "filmstripselection.h"
#include "menubarcommandids.h"
#include "columncommand.h"

// Qt includes
#include <QCoreApplication>
#include <QPushButton>
#include <QMainWindow>

#include "matchline.h"

//*****************************************************************************
//    MergeCmappedDialog  implementation
//*****************************************************************************

void MergeCmappedDialog::accept() {
  m_levelPath = TFilePath(QString(m_saveInFileFld->getPath() + "\\" +
                                  m_fileNameFld->text() + ".tlv")
                              .toStdString());
  TFilePath fp =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
          m_levelPath);

  if (TSystem::doesExistFileOrLevel(fp)) {
    if (DVGui::MsgBox(
            QObject::tr("Level ") +
                QString::fromStdWString(m_levelPath.getWideString()) +
                QObject::tr(
                    " already exists! Are you sure you want to overwrite it?"),
            tr("Ok"), tr("Cancel")) != 1)
      return;
    else {
      TSystem::removeFileOrLevel(fp);
      TSystem::removeFileOrLevel(fp.withType("tpl"));
    }
  }

  Dialog::accept();
}

//------------------------------------------------------------------------------

MergeCmappedDialog::MergeCmappedDialog(TFilePath &levelPath)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Merge Tlv")
    , m_levelPath(levelPath) {
  bool ret = true;

  QString path =
      QString::fromStdWString(m_levelPath.getParentDir().getWideString());
  QString name = QString::fromStdString(m_levelPath.getName());

  setWindowTitle(tr(" Merge Tlv Levels"));
  m_saveInFileFld = new DVGui::FileField(0, path);
  ret             = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onPathChanged()));
  addWidget(tr("Save in:"), m_saveInFileFld);

  m_fileNameFld = new DVGui::LineEdit(name + "_merged");
  m_fileNameFld->setMaximumHeight(DVGui::WidgetHeight);
  ret = ret && connect(m_fileNameFld, SIGNAL(editingFinished()),
                       SLOT(onNameChanged()));
  addWidget(tr("File Name:"), m_fileNameFld);

  QPushButton *okBtn = new QPushButton(tr("Apply"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

//*****************************************************************************
//    MergeColumns  command
//*****************************************************************************

class MergeColumnsCommand final : public MenuItemHandler {
public:
  MergeColumnsCommand() : MenuItemHandler(MI_MergeColumns) {}

  void execute() override {
    TColumnSelection *selection =
        dynamic_cast<TColumnSelection *>(TSelection::getCurrent());

    std::set<int> indices =
        selection ? selection->getIndices() : std::set<int>();

    if (indices.empty()) {
      DVGui::warning(
          tr("It is not possible to execute the merge column command because "
             "no column was selected."));
      return;
    }

    if (indices.size() == 1) {
      DVGui::warning(
          tr("It is not possible to execute the merge column command  because "
             "only one columns is  selected."));
      return;
    }

    mergeColumns(indices);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

} MergeColumnsCommand;

//*****************************************************************************
//    ApplyMatchlines  command
//*****************************************************************************

class ApplyMatchlinesCommand final : public MenuItemHandler {
public:
  ApplyMatchlinesCommand() : MenuItemHandler(MI_ApplyMatchLines) {}

  void execute() override {
    TColumnSelection *selection =
        dynamic_cast<TColumnSelection *>(TSelection::getCurrent());
    if (!selection) {
      DVGui::warning(
          tr("It is not possible to apply the match lines because no column "
             "was selected."));
      return;
    }

    std::set<int> indices = selection->getIndices();

    if (indices.size() != 2) {
      DVGui::warning(
          tr("It is not possible to apply the match lines because two columns "
             "have to be selected."));
      return;
    }

    std::set<int>::iterator it = indices.begin();
    int i = *it++, j = *it;

    doMatchlines(i, j, -1, -1);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

} ApplyMatchlinesCommand;

//--------------------------------------------

namespace {

bool checkColumnValidity(int column) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int start, end;

  xsh->getCellRange(column, start, end);

  if (start > end) return false;
  std::vector<TXshCell> cell(end - start + 1);

  xsh->getCells(start, column, cell.size(), &(cell[0]));

  TXshSimpleLevel *level = 0;

  for (int i = 0; i < (int)cell.size(); i++) {
    if (cell[i].isEmpty()) continue;
    if (!level) level = cell[i].getSimpleLevel();

    if (cell[i].getSimpleLevel()->getType() != TZP_XSHLEVEL) {
      DVGui::warning(QObject::tr(
          "Match lines can be applied to Toonz raster levels only."));
      return false;
    }
    if (level != cell[i].getSimpleLevel()) {
      DVGui::warning(
          QObject::tr("It is not possible to merge tlv columns containing more "
                      "than one level"));
      return false;
    }
  }

  if (!level) return false;

  if (!level->getPalette()) {
    DVGui::warning(
        QObject::tr("The level you are using has not a valid palette."));
    return false;
  }
  return true;
}

//---------------------------------------------------------------------------------------------------------

void doCloneLevelNoSave(const TCellSelection::Range &range,
                        const QString &newLevelName, bool withUndo);

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class CloneLevelNoSaveUndo final : public TUndo {
  std::map<TXshSimpleLevel *, TXshLevelP> m_createdLevels;
  std::set<int> m_insertedColumnIndices;

  TCellSelection::Range m_range;
  QString m_levelname;

public:
  CloneLevelNoSaveUndo(
      const TCellSelection::Range &range,
      const std::map<TXshSimpleLevel *, TXshLevelP> &createdLevels,
      const std::set<int> &insertedColumnIndices, const QString &levelname)
      : m_createdLevels(createdLevels)
      , m_range(range)
      , m_insertedColumnIndices(insertedColumnIndices)
      , m_levelname(levelname) {}

  void undo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    int i;
    for (i = m_range.getColCount(); i > 0; i--) {
      int index                        = m_range.m_c1 + i;
      std::set<int>::const_iterator it = m_insertedColumnIndices.find(index);
      xsh->removeColumn(index);
      if (it == m_insertedColumnIndices.end()) xsh->insertColumn(index);
    }

    std::map<TXshSimpleLevel *, TXshLevelP>::const_iterator it =
        m_createdLevels.begin();

    for (; it != m_createdLevels.end(); ++it) {
      it->second->addRef();
      scene->getLevelSet()->removeLevel(it->second.getPointer());
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    doCloneLevelNoSave(m_range, m_levelname, false);
  }

  int getSize() const override {
    return sizeof *this +
           (sizeof(TXshLevelP) + sizeof(TXshSimpleLevel *)) *
               m_createdLevels.size();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void doCloneLevelNoSave(const TCellSelection::Range &range,
                        const QString &newLevelName = QString(),
                        bool withUndo               = true) {
  std::map<TXshSimpleLevel *, TXshLevelP> createdLevels;

  TApp *app         = TApp::instance();
  TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  // Build indices of inserted columns
  std::set<int> insertedColumnIndices;
  int c;
  for (c = 1; c <= range.getColCount(); ++c) {
    int colIndex = range.m_c1 + c;
    if (xsh->isColumnEmpty(colIndex)) continue;

    xsh->insertColumn(colIndex);
    insertedColumnIndices.insert(colIndex);
  }

  bool isOneCellCloned = false;
  for (c = range.m_c0; c <= range.m_c1; ++c) {
    TXshLevelP xl;
    TXshSimpleLevel *sl = 0;
    TFrameId fid(1);

    bool keepOldLevel = false;

    // OverwriteDialog* dialog = new OverwriteDialog();
    for (int r = range.m_r0; r <= range.m_r1; ++r) {
      TXshCell cell = xsh->getCell(r, c);

      TImageP img = cell.getImage(true);
      if (!img) continue;

      fid = cell.getFrameId();

      if (cell.getSimpleLevel() == 0 ||
          cell.getSimpleLevel()->getPath().getType() == "psd")
        continue;

      std::map<TXshSimpleLevel *, TXshLevelP>::iterator it =
          createdLevels.find(cell.getSimpleLevel());
      if (it == createdLevels.end()) {
        // Create a new level if not already done

        TXshSimpleLevel *oldSl = cell.getSimpleLevel();
        {
          int levelType = oldSl->getType();
          assert(levelType > 0);

          xl = scene->createNewLevel(levelType, newLevelName.toStdWString());
          sl = xl->getSimpleLevel();
          // if(levelType == OVL_XSHLEVEL)
          //  dstPath = dstPath.withType(oldSl->getPath().getType());
          assert(sl);
          // sl->setPath(scene->codeFilePath(dstPath));
          // sl->setName(newName);
          sl->clonePropertiesFrom(oldSl);
          *sl->getHookSet() = *oldSl->getHookSet();

          if (levelType == TZP_XSHLEVEL || levelType == PLI_XSHLEVEL) {
            TPalette *palette = oldSl->getPalette();
            assert(palette);

            sl->setPalette(palette->clone());
          }
        }
        createdLevels[cell.getSimpleLevel()] = xl;
      } else {
        xl = it->second;
        sl = xl->getSimpleLevel();
      }

      TXshCell oldCell(cell);
      cell.m_level = xl;
      int k;
      for (k = range.m_r0; k < r; k++) {
        if (xsh->getCell(k, c).getImage(true).getPointer() ==
            img.getPointer()) {
          TFrameId oldFid = xsh->getCell(k, c).getFrameId();
          assert(fid == oldFid);
          sl->setFrame(fid,
                       xsh->getCell(k, c + range.getColCount()).getImage(true));
          break;
        }
      }

      if (!keepOldLevel && k >= r) {
        TImageP newImg(img->cloneImage());
        assert(newImg);

        sl->setFrame(fid, newImg);
      }

      cell.m_frameId = fid;
      xsh->setCell(r, c + range.getColCount(), cell);
      isOneCellCloned = true;
    }
    if (sl) sl->getProperties()->setDirtyFlag(true);
  }

  // Se non e' stata inserita nessuna cella rimuovo le colonne aggiunte e
  // ritorno.
  if (!isOneCellCloned) {
    if (!insertedColumnIndices.empty()) {
      int i;
      for (i = range.getColCount(); i > 0; i--) {
        int index                        = range.m_c1 + i;
        std::set<int>::const_iterator it = insertedColumnIndices.find(index);
        xsh->removeColumn(index);
      }
    }
    return;
  }
  if (withUndo)
    TUndoManager::manager()->add(new CloneLevelNoSaveUndo(
        range, createdLevels, insertedColumnIndices, newLevelName));

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifyCastChange();
}

void cloneColumn(const TCellSelection::Range &cells,
                 const TFilePath &newLevelPath) {
  std::set<std::pair<int, int>> positions;
  for (int i = cells.m_r0; i <= cells.m_r1; i++)
    positions.insert(std::pair<int, int>(i, cells.m_c0));

  TKeyframeSelection ks(positions);
  ks.copyKeyframes();
  doCloneLevelNoSave(cells,
                     QString::fromStdString(newLevelPath.getName() + "_"));
  ColumnCmd::deleteColumn(cells.m_c0);
  ks.pasteKeyframes();
}

}  // namespace

class MergeCmappedCommand final : public MenuItemHandler {
public:
  MergeCmappedCommand() : MenuItemHandler(MI_MergeCmapped) {}
  void execute() override {
    TColumnSelection *selection =
        dynamic_cast<TColumnSelection *>(TSelection::getCurrent());
    if (!selection) {
      DVGui::warning(
          tr("It is not possible to merge tlv columns because no column was "
             "selected."));
      return;
    }

    std::set<int> indices = selection->getIndices();

    if (indices.size() < 2) {
      DVGui::warning(
          tr("It is not possible to merge tlv columns because at least two "
             "columns have to be selected."));
      return;
    }

    std::set<int>::iterator it = indices.begin();
    int destColumn             = *it;

    TCellSelection::Range cells;
    cells.m_c0 = cells.m_c1 = destColumn;
    TXshColumn *column =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getColumn(
            destColumn);
    column->getRange(cells.m_r0, cells.m_r1);

    // column->getLevelColumn()

    TFilePath newLevelPath;
    TXshCell c = TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(
        cells.m_r0, destColumn);
    if (!c.isEmpty() && c.getSimpleLevel())
      newLevelPath = c.getSimpleLevel()->getPath();

    if (MergeCmappedDialog(newLevelPath).exec() != QDialog::Accepted) return;

    it = indices.begin();
    for (; it != indices.end(); ++it)
      if (!checkColumnValidity(*it)) return;

    DVGui::ProgressDialog progress(tr("Merging Tlv Levels..."), QString(), 0,
                                   indices.size() - 1,
                                   TApp::instance()->getMainWindow());
    progress.setWindowModality(Qt::WindowModal);
    progress.setWindowTitle(tr("Merging Tlv Levels..."));
    progress.setValue(0);
    progress.show();

    QCoreApplication::instance()->processEvents();

    TUndoManager::manager()->beginBlock();

    cloneColumn(cells, newLevelPath);

    TFilePath tmpPath = newLevelPath.withName(
        QString::fromStdString(newLevelPath.getName() + "_tmp").toStdWString());

    it = indices.begin();
    ++it;
    for (int count = 0; it != indices.end();) {
      int index = *it;
      it++;
      mergeCmapped(destColumn, index - count,
                   it == indices.end() ? newLevelPath.getQString()
                                       : tmpPath.getQString(),
                   false);
      ColumnCmd::deleteColumn(index - count);
      progress.setValue(++count);
      QCoreApplication::instance()->processEvents();
    }
    TUndoManager::manager()->endBlock();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

} MergeCmappedCommand;

//-----------------------------------------------------------------------------
namespace {
void doDeleteCommand(bool isMatchline) {
  TRect r;

  TCellSelection *sel =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  if (!sel) {
    TFilmstripSelection *filmstripSelection =
        dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
    TColumnSelection *columnSelection =
        dynamic_cast<TColumnSelection *>(TSelection::getCurrent());
    std::set<int> indices;
    std::set<TFrameId> fids;
    if (filmstripSelection &&
        (fids = filmstripSelection->getSelectedFids()).size() > 0) {
      TXshSimpleLevel *sl =
          TApp::instance()->getCurrentLevel()->getSimpleLevel();
      if (isMatchline)
        deleteMatchlines(sl, fids);
      else
        deleteInk(sl, fids);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      return;
    } else if (!columnSelection ||
               (indices = columnSelection->getIndices()).size() != 1) {
      DVGui::warning(
          QObject::tr("It is not possible to delete lines because no column, "
                      "cell or level strip frame was selected."));
      return;
    }
    int from, to;
    int columnIndex = *indices.begin();
    TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
    if (!xsh->getCellRange(*indices.begin(), from, to)) {
      DVGui::warning(QObject::tr("The selected column is empty."));
      return;
    }
    r.y0 = from;
    r.y1 = to;
    r.x0 = r.x1 = columnIndex;
  } else
    sel->getSelectedCells(r.y0, r.x0, r.y1, r.x1);

  if (r.x0 != r.x1) {
    DVGui::warning(QObject::tr("Selected cells must be in the same column."));
    return;
  }

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  int i;

  for (i = r.y0; i <= r.y1; i++) {
    TXshCell cell = xsh->getCell(i, r.x0);
    if (cell.isEmpty()) {
      DVGui::warning(
          QObject::tr("It is not possible to delete lines because no column, "
                      "cell or level strip frame was selected."));
      return;
    }
    if (cell.m_level->getType() != TZP_XSHLEVEL) {
      DVGui::warning(QObject::tr(
          "Match lines can be deleted from Toonz raster levels only"));
      return;
    }
  }

  std::set<TFrameId> fids;
  for (i = r.y0; i <= r.y1; i++) {
    const TXshCell &cell = xsh->getCell(i, r.x0);
    fids.insert(cell.getFrameId());
  }

  TXshSimpleLevel *sl = xsh->getCell(r.y0, r.x0).getSimpleLevel();
  assert(sl);

  if (isMatchline)
    deleteMatchlines(sl, fids);
  else
    deleteInk(sl, fids);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

}  // namespace

//-----------------------------------------------------------------------------

class DeleteInkCommand final : public MenuItemHandler {
public:
  DeleteInkCommand() : MenuItemHandler(MI_DeleteInk) {}
  void execute() override { doDeleteCommand(false); }

} DeleteInkCommand;

//-----------------------------------------------------------------------------

class DeleteMatchlinesCommand final : public MenuItemHandler {
public:
  DeleteMatchlinesCommand() : MenuItemHandler(MI_DeleteMatchLines) {}
  void execute() override { doDeleteCommand(true); }

} DeleteMatchlinesCommand;
