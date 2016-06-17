

#include "addfilmstripframespopup.h"

// Tnz6 includes
#include "filmstripcommand.h"
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QMainWindow>

//=============================================================================
// AddFilmstripFramesPopup
//-----------------------------------------------------------------------------

AddFilmstripFramesPopup::AddFilmstripFramesPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, "AddFilmstripFrames") {
  setWindowTitle(tr("Add Frames"));

  m_startFld = new DVGui::IntLineEdit(this);
  m_endFld   = new DVGui::IntLineEdit(this);
  m_stepFld  = new DVGui::IntLineEdit(this);

  m_okBtn     = new QPushButton(tr("Add"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);

  QGridLayout *upperLay = new QGridLayout();
  upperLay->setMargin(0);
  upperLay->setSpacing(5);
  {
    upperLay->addWidget(new QLabel(tr("From Frame:"), this), 0, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
    upperLay->addWidget(m_startFld, 0, 1);

    upperLay->addWidget(new QLabel(tr("To Frame:"), this), 1, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
    upperLay->addWidget(m_endFld, 1, 1);

    upperLay->addWidget(new QLabel(tr("Step:"), this), 2, 0,
                        Qt::AlignRight | Qt::AlignVCenter);
    upperLay->addWidget(m_stepFld, 2, 1);
  }
  upperLay->setColumnStretch(0, 0);
  upperLay->setColumnStretch(1, 1);
  m_topLayout->addLayout(upperLay, 1);

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(10);
  {
    m_buttonLayout->addWidget(m_okBtn);
    m_buttonLayout->addWidget(m_cancelBtn);
  }

  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(onOk()));
  connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  update();
}

//-----------------------------------------------------------------------------

void AddFilmstripFramesPopup::onOk() {
  accept();

  /*
int start = (int)m_startFld->text().toInt();
int end   = (int)m_endFld->text().toInt();
int step  = (int)m_stepFld->text().toInt();

try {
FilmstripCmd::addFrames(start,end,step);
} catch( ... ) {
return;
}
close();
*/
}

//-----------------------------------------------------------------------------

void AddFilmstripFramesPopup::update() {
  m_startFld->setText(QString("1"));
  m_endFld->setText(QString("1"));
  m_stepFld->setText(QString("1"));
}

//-----------------------------------------------------------------------------

void AddFilmstripFramesPopup::getParameters(int &startFrame, int &endFrame,
                                            int &stepFrame) const {
  startFrame = m_startFld->text().toInt();
  endFrame   = m_endFld->text().toInt();
  stepFrame  = m_stepFld->text().toInt();
}
