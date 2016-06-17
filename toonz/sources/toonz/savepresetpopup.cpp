

#include "savepresetpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "filebrowsermodel.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnfx.h"
#include "toonz/toonzfolders.h"

// TnzBase includes
#include "tfx.h"
#include "tmacrofx.h"

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tfxattributes.h"

// Qt includes
#include <QPushButton>
#include <QMainWindow>

//=============================================================================
// SavePresetPopup
//-----------------------------------------------------------------------------

SavePresetPopup::SavePresetPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "SavePreset") {
  setWindowTitle(tr("Save Preset"));
  addWidget(tr("Preset Name:"), m_nameFld = new DVGui::LineEdit(this));
  m_nameFld->setMinimumWidth(170);

  QPushButton *okBtn = new QPushButton(tr("Save"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtn()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);
}

//-----------------------------------------------------------------------------

SavePresetPopup::~SavePresetPopup() {}

//-----------------------------------------------------------------------------

bool SavePresetPopup::apply() {
  TFxHandle *fxHandle = TApp::instance()->getCurrentFx();
  TFx *fx             = fxHandle->getFx();
  if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx))
    fx = zfx->getZeraryFx();
  if (!fx) return 0;

  TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx);
  bool isMacro      = macroFx != 0;
  std::wstring name = m_nameFld->text().toStdWString();
  if (name.empty()) return 0;
  TFilePath fp = ToonzFolder::getFxPresetFolder() + "presets" +
                 fx->getFxType() + (name + (isMacro ? L".macrofx" : L".fx"));
  if (!TFileStatus(fp.getParentDir()).doesExist()) {
    try {
      TFilePath parent = fp.getParentDir();
      TSystem::mkDir(parent);
      DvDirModel::instance()->refreshFolder(parent.getParentDir());
    } catch (...) {
      DVGui::error(
          tr("It is not possible to create the preset folder %1.")
              .arg(QString::fromStdString(fp.getParentDir().getName())));
      return 0;
    }
  }
  if (TFileStatus(fp).doesExist()) {
    if (DVGui::MsgBox(tr("Do you want to overwrite?"), tr("Yes"), tr("No")) ==
        2)
      return 0;
  }
  if (isMacro) {
    TOStream os(fp);
    TFx *fx2                                    = fx->clone(false);
    fx2->getAttributes()->passiveCacheDataIdx() = -1;
    os << fx2;
    delete fx2;
  } else {
    TOStream os(fp);
    fx->savePreset(os);
  }
  fxHandle->notifyFxPresetSaved();
  return true;
}

//-----------------------------------------------------------------------------

void SavePresetPopup::onOkBtn() {
  if (apply()) accept();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<SavePresetPopup> openSavePresetPopup(MI_SavePreset);
