

#include "fillholespopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "selectionutils.h"
#include "tools/toolutils.h"
#include "tundo.h"
#include "ttoonzimage.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/fill.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/txsheethandle.h"

// Qt includes
#include <QPushButton>
#include <QProgressBar>

#include <memory>
#include <utility>  // for std::pair (kept for compatibility)
#include <cassert>

using namespace DVGui;
using namespace ToolUtils;

//-----------------------------------------------------------------------------

class TFillHolesUndo final : public TRasterUndo {
  int m_size;

public:
  TFillHolesUndo(TTileSetCM32* tileSet, int size, TXshSimpleLevel* sl,
                 const TFrameId& fid)
      : TRasterUndo(tileSet, sl, fid, false, false, 0), m_size(size) {}

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    TTool::Application* app = TTool::getApplication();
    if (!app) return;
    fillHoles(image->getRaster(), m_size);
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QStringLiteral("Fill Holes"); }
};

//-----------------------------------------------------------------------------

FillHolesDialog::FillHolesDialog()
    : Dialog(nullptr, true, true, "Fill Small Holes")
    , m_progressDialog(nullptr) {
  setWindowTitle(tr("Fill Small Holes"));
  setModal(false);
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

  beginVLayout();
  m_size = new IntField(this, false);
  m_size->setRange(1, 25);
  m_size->setValue(5);
  addWidget(tr("Size"), m_size);
  endVLayout();

  QPushButton* okBtn = new QPushButton(tr("Apply"), this);
  okBtn->setDefault(true);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  connect(okBtn, &QPushButton::clicked, this, &FillHolesDialog::apply);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------

void FillHolesDialog::apply() {
  std::set<TXshLevel*> levels;
  SelectionUtils::getSelectedLevels(levels);

  // Collect all frames from Toonz raster levels
  std::vector<std::pair<TXshSimpleLevel*, TFrameId>> frames;
  for (TXshLevel* level : levels) {
    if (level->getType() != TXshLevelType::TZP_XSHLEVEL) continue;
    TXshSimpleLevel* sl = level->getSimpleLevel();
    if (!sl) continue;
    for (const TFrameId& fid : sl->getFids()) {
      frames.emplace_back(sl, fid);
    }
  }

  const int totalFrames = static_cast<int>(frames.size());
  if (totalFrames == 0) {
    DVGui::warning(tr("No Toonz Raster Level Selected"));
    return;
  }

  // Create progress dialog – use std::make_unique for automatic cleanup
  m_progressDialog = std::make_unique<ProgressDialog>(
      tr("Filling Holes..."), tr("Cancel"), 0, totalFrames, this);
  m_progressDialog->show();

  int count = 0;
  TUndoManager::manager()->beginBlock();

  for (const auto& frame : frames) {
     const auto& [sl, fid] = frame;

    TImageP img     = sl->getFrame(fid, true);
    TToonzImageP ti = img;
    if (!ti) continue;  // safety check
    TRasterCM32P ras = ti->getRaster();

    auto tileSet = std::make_unique<TTileSetCM32>(ras->getSize());
    auto saver   = std::make_unique<TTileSaverCM32>(ras, tileSet.get());

    if (m_progressDialog->wasCanceled()) {
      break;
    }

    fillHoles(ras, m_size->getValue(), saver.get());

    if (tileSet->getTileCount() != 0) {
      // Transfer ownership of tileSet to the undo object
      TUndoManager::manager()->add(
          new TFillHolesUndo(tileSet.release(), m_size->getValue(), sl, fid));
      // saver is destroyed automatically when we leave the scope
    }
    // else: tileSet and saver are automatically destroyed by unique_ptr

    ++count;
    m_progressDialog->setValue(count);
  }

  // Progress dialog is automatically destroyed when m_progressDialog is reset
  m_progressDialog.reset();

  TUndoManager::manager()->endBlock();
  Dialog::accept();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<FillHolesDialog> fillholesPopup(MI_FillHoles);
