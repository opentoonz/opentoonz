#include "reframepopup.h"

// Tnz includes
#include "tapp.h"
#include "menubarcommandids.h"
// TnzQt includes
#include "toonzqt/intfield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

#include <iostream>
//-----------------------------------------------------------------------------

ReframePopup::ReframePopup()
    : Dialog(TApp::instance()->getMainWindow(), true, true, "ReframePopup") {
  setWindowTitle(tr("Reframe with Empty Inbetweens"));
  m_step  = new DVGui::IntLineEdit(this, 1, 1);
  m_blank = new DVGui::IntLineEdit(this, 0, 0);

  m_blankCellCountLbl = new QLabel("0", this);

  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  m_step->setObjectName("LargeSizedText");
  m_blank->setObjectName("LargeSizedText");
  // layout
  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(5);
  {
    mainLay->addWidget(m_step);
    mainLay->addWidget(new QLabel(tr("steps"), this));
    mainLay->addSpacing(10);
    mainLay->addWidget(new QLabel(tr("with"), this));
    mainLay->addWidget(m_blank);
    mainLay->addWidget(new QLabel(tr("empty inbetweens"), this));
  }
  m_topLayout->addLayout(mainLay);

  QHBoxLayout* textLay = new QHBoxLayout();
  textLay->setMargin(0);
  {
    textLay->addStretch(1);
    textLay->addWidget(new QLabel(tr("("), this));
    textLay->addWidget(m_blankCellCountLbl);
    textLay->addWidget(new QLabel(tr(" blank cells will be inserted.)"), this));
  }
  m_topLayout->addLayout(textLay);

  // buttons
  m_buttonLayout->addWidget(okBtn);
  m_buttonLayout->addWidget(cancelBtn);

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_step, SIGNAL(editingFinished()), this,
                       SLOT(updateBlankCellCount()));
  ret = ret && connect(m_blank, SIGNAL(editingFinished()), this,
                       SLOT(updateBlankCellCount()));
  ret = ret && connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void ReframePopup::updateBlankCellCount() {
  int blankCellCount = m_step->getValue() * m_blank->getValue();
  m_blankCellCountLbl->setText(QString::number(blankCellCount));
}

//-----------------------------------------------------------------------------

void ReframePopup::getValues(int& step, int& blank) {
  step  = m_step->getValue();
  blank = m_blank->getValue();
}
