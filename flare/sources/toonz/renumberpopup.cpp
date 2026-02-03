

#include "renumberpopup.h"

// Tnz6 includes
#include "filmstripcommand.h"
#include "tapp.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

//=============================================================================
// RenumberPopup
//-----------------------------------------------------------------------------

RenumberPopup::RenumberPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "Renumber") {
  setWindowTitle(tr("Renumber"));

  beginHLayout();
  addWidget(tr("Start:"), m_startFld = new DVGui::IntLineEdit(this));
  addSpacing(10);
  addWidget(tr("Step:"), m_stepFld = new DVGui::IntLineEdit(this));
  endHLayout();

  m_okBtn = new QPushButton(tr("Renumber"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

//-------------------------------------------------------------------

void RenumberPopup::setValues(int start, int step) {
  m_startFld->setText(QString::number(start));
  m_stepFld->setText(QString::number(step));
}

//-------------------------------------------------------------------

void RenumberPopup::getValues(int &start, int &step) {
  start = m_startFld->text().toInt();
  step  = m_stepFld->text().toInt();
}
