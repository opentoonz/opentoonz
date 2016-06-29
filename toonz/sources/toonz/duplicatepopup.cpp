

#include "duplicatepopup.h"

// Tnz6 includes
#include "filmstripcommand.h"
#include "cellselection.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"

#include "menubarcommandids.h"
#include "tundo.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "historytypes.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

//=============================================================================
// Duplicate
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class DuplicateUndo final : public TUndo {
  int m_r0, m_c0;
  int m_r1, m_c1;
  int m_upTo;

public:
  DuplicateUndo(int r0, int c0, int r1, int c1, int upTo);
  ~DuplicateUndo() {}
  void undo() const override;
  void redo() const override;
  void repeat() const;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Duplicate"); }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

DuplicateUndo::DuplicateUndo(int r0, int c0, int r1, int c1, int upTo)
    : m_r0(r0), m_c0(c0), m_r1(r1), m_c1(c1), m_upTo(upTo) {}

//-----------------------------------------------------------------------------

void DuplicateUndo::undo() const {
  if (m_r0 == 0 && m_c0 == 0 && m_r1 == -1 && m_c1 == -1) return;
  TApp *app = TApp::instance();
  for (int j = m_c0; j <= m_c1; j++)
    app->getCurrentXsheet()->getXsheet()->removeCells(m_r1 + 1, j,
                                                      m_upTo - (m_r1 + 1) + 1);
  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void DuplicateUndo::redo() const {
  if (m_r0 == 0 && m_c0 == 0 && m_r1 == -1 && m_c1 == -1) return;
  TApp *app = TApp::instance();
  app->getCurrentXsheet()->getXsheet()->duplicateCells(m_r0, m_c0, m_r1, m_c1,
                                                       m_upTo);
  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void DuplicateUndo::repeat() const {}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// DuplicatePopup
//-----------------------------------------------------------------------------
/*--  "Repeat..." というコマンド  --*/
DuplicatePopup::DuplicatePopup()
    : QDialog(TApp::instance()->getMainWindow()), m_count(0), m_upTo(0) {
  setWindowTitle(tr("Repeat"));

  m_countFld = new DVGui::IntLineEdit(this);
  m_upToFld  = new DVGui::IntLineEdit(this);

  m_okBtn     = new QPushButton(tr("Repeat"), this);
  m_cancelBtn = new QPushButton(tr("Close"), this);
  m_applyBtn  = new QPushButton(tr("Apply"), this);

  //----layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(10);
  mainLayout->setSpacing(10);
  {
    QHBoxLayout *upperLay = new QHBoxLayout();
    upperLay->setMargin(0);
    upperLay->setSpacing(5);
    {
      upperLay->addWidget(new QLabel(tr("Times:"), this), 0);
      upperLay->addWidget(m_countFld, 1);
      upperLay->addSpacing(10);
      upperLay->addWidget(new QLabel(tr("Up to Frame:"), this), 0);
      upperLay->addWidget(m_upToFld, 1);
    }
    mainLayout->addLayout(upperLay, 0);

    QHBoxLayout *bottomLay = new QHBoxLayout();
    bottomLay->setMargin(0);
    bottomLay->setSpacing(10);
    {
      bottomLay->addWidget(m_okBtn);
      bottomLay->addWidget(m_applyBtn);
      bottomLay->addWidget(m_cancelBtn);
    }
    mainLayout->addLayout(bottomLay, 0);
  }
  setLayout(mainLayout);

  //----signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_countFld, SIGNAL(editingFinished()), this,
                       SLOT(updateValues()));
  ret = ret && connect(m_upToFld, SIGNAL(editingFinished()), this,
                       SLOT(updateValues()));

  ret = ret && connect(m_okBtn, SIGNAL(clicked()), this, SLOT(onOKPressed()));
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(close()));
  ret = ret &&
        connect(m_applyBtn, SIGNAL(clicked()), this, SLOT(onApplyPressed()));

  ret = ret && connect(TApp::instance()->getCurrentSelection(),
                       SIGNAL(selectionChanged(TSelection *)), this,
                       SLOT(onSelectionChanged()));
  assert(ret);
}

//-------------------------------------------------------------------

void DuplicatePopup::onApplyPressed() {
  TCellSelection *selection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (!selection) return;

  int count = 0, upTo = 0;
  getValues(count, upTo);

  try {
    int r0, r1, c0, c1;
    selection->getSelectedCells(r0, c0, r1, c1);
    TUndo *undo = new DuplicateUndo(r0, c0, r1, c1, (int)upTo - 1);
    TUndoManager::manager()->add(undo);
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();
    xsh->duplicateCells(r0, c0, r1, c1, (int)upTo - 1);
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  } catch (...) {
    DVGui::error(("Cannot duplicate"));
  }
}

//-------------------------------------------------------------------

void DuplicatePopup::onOKPressed() {
  onApplyPressed();
  close();
}

//-------------------------------------------------------------------

void DuplicatePopup::onSelectionChanged() {
  if (isVisible()) {
    m_count = 0;
    updateValues();
  }
}

//-------------------------------------------------------------------

void DuplicatePopup::showEvent(QShowEvent *) {
  /*-- ダイアログを開くたびにcount値を初期値に戻す --*/
  m_countFld->setValue(1);
  m_upToFld->setValue(1);
  m_count = 0;
  m_upTo  = 0;
  updateValues();
}

//-------------------------------------------------------------------

void DuplicatePopup::updateValues() {
  int count, upTo;
  getValues(count, upTo);
  if (count == m_count && upTo == m_upTo) return;
  TCellSelection *selection = dynamic_cast<TCellSelection *>(
      TApp::instance()->getCurrentSelection()->getSelection());

  if (!selection) {
    m_countFld->setEnabled(false);
    m_upToFld->setEnabled(false);
    m_okBtn->setEnabled(false);
    m_applyBtn->setEnabled(false);
    return;
  } else {
    m_countFld->setEnabled(true);
    m_upToFld->setEnabled(true);
    m_okBtn->setEnabled(true);
    m_applyBtn->setEnabled(true);
  }

  int r0, r1, c0, c1;
  selection->getSelectedCells(r0, c0, r1, c1);
  int chunkSize = r1 - r0 + 1;

  if (count != m_count) {
    if (count < 1) count = 1;
    upTo                 = r1 + 1 + count * chunkSize;
  }
  /*-- upToを編集した場合 --*/
  else {
    if (upTo < r1 + 1 + 1) upTo = r1 + 1 + 1;
    count                       = (upTo - (r1 + 2) + chunkSize) / chunkSize;
    if (count < 1) upTo         = 1;
  }
  m_count = count;
  m_upTo  = upTo;

  m_countFld->setText(QString::number(m_count));
  m_upToFld->setText(QString::number(m_upTo));
}

//-------------------------------------------------------------------

void DuplicatePopup::getValues(int &count, int &upTo) {
  count = m_countFld->text().toInt();
  upTo  = m_upToFld->text().toInt();
}

OpenPopupCommandHandler<DuplicatePopup> openDuplicatePopup(MI_Dup);
