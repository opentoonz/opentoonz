#include "fillholespopup.h"
#include "toonz/fill.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "selectionutils.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "ttoonzimage.h"
#include "tools/toolutils.h"
#include "tundo.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/txsheethandle.h"

#include <QPushButton>
#include <QProgressBar>

using namespace DVGui;
using namespace ToolUtils;

class TFillHolesUndo final : public TRasterUndo {
  int m_size;

public:
  TFillHolesUndo(TTileSetCM32* tileSet, int size, TXshSimpleLevel* sl,
                 const TFrameId& fid)
      : TRasterUndo(tileSet, sl, fid, false, false, 0), m_size(size) {};

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    TTool::Application* app = TTool::getApplication();
    if (!app) return;
    fillHoles(image->getRaster(), m_size);
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  };
  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  };
  QString getToolName() override { return QString("Fill Holes"); };
};

FillHolesDialog::FillHolesDialog() : Dialog(0, true, true, "Fill Small Holes") {
  setWindowTitle(tr("Fill Small Holes"));
  setModal(false);
  setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

  beginVLayout();
  m_size = new IntField(this,false);
  m_size->setRange(1, 25);
  m_size->setValue(5);
  addWidget(tr("Size"), m_size);
  endVLayout();

  QPushButton* okBtn = new QPushButton(tr("Apply"), this);
  okBtn->setDefault(true);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
  bool ret = connect(okBtn, SIGNAL(clicked()), this, SLOT(apply()));
  ret      = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);

  addButtonBarWidget(okBtn, cancelBtn);
};

void FillHolesDialog::apply() {
  std::set<TXshLevel*> levels;
  SelectionUtils::getSelectedLevels(levels);
  std::vector<std::pair<TXshSimpleLevel*, TFrameId>> Frames;
  for (const auto& level : levels) {
    if (level->getType() != TXshLevelType::TZP_XSHLEVEL) continue;
    TXshSimpleLevel* sl = level->getSimpleLevel();
    if (!sl) continue;
    for (const TFrameId& fid : sl->getFids()) {
      Frames.push_back(std::make_pair(sl, fid));
    }
  }
  int size = Frames.size();
  if (size == 0) {
    DVGui::warning(tr("No Toonz Raster Level Selected"));
    return;
  } else {
    m_progressDialog =
        new ProgressDialog("Filling Holes...", QObject::tr("Cancel"), 0, size, this);
    m_progressDialog->show();
  }

  int count = 0;
  TUndoManager::manager()->beginBlock();
  for (const auto& frame : Frames) {
    TXshSimpleLevel* sl = frame.first;
    TFrameId fid        = frame.second;

    TImageP img           = sl->getFrame(fid, true);
    TToonzImageP ti       = TToonzImageP(img);
    TRasterCM32P ras      = ti->getRaster();
    TTileSetCM32* tileSet = new TTileSetCM32(ras->getSize());
    TTileSaverCM32* saver = new TTileSaverCM32(ras, tileSet);
    if (m_progressDialog->wasCanceled()) break;
    fillHoles(ras, m_size->getValue(), saver);
    if (tileSet->getTileCount() != 0)
      TUndoManager::manager()->add(
          new TFillHolesUndo(tileSet, m_size->getValue(), sl, fid));
    count++;
    m_progressDialog->setValue(count);
  }
  TUndoManager::manager()->endBlock();
  m_progressDialog->close();
  Dialog::accept();
}

OpenPopupCommandHandler<FillHolesDialog> fillholesPopup(MI_FillHoles);