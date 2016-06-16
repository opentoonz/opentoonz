

#include "toonzqt/validatedchoicedialog.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>

//************************************************************************
//    ValidatedChoiceDialog  implementation
//************************************************************************

DVGui::ValidatedChoiceDialog::ValidatedChoiceDialog(QWidget *parent,
                                                    Options opts)
    : Dialog(parent, true, false) {
  setModal(true);

  m_buttonGroup = new QButtonGroup(this);
  m_buttonGroup->setExclusive(true);

  bool ret = true;

  QPushButton *okBtn = new QPushButton(QString(tr("Apply")), this);
  ret                = ret && connect(okBtn, SIGNAL(clicked()), SLOT(accept()));
  addButtonBarWidget(okBtn);

  QPushButton *okToAllBtn = new QPushButton(QString(tr("Apply to All")), this);
  ret = ret && connect(okToAllBtn, SIGNAL(clicked()), SLOT(onApplyToAll()));
  addButtonBarWidget(okToAllBtn);

  QPushButton *cancelBtn = new QPushButton(QString(tr("Cancel")), this);
  ret = ret && connect(cancelBtn, SIGNAL(clicked()), SLOT(reject()));
  addButtonBarWidget(cancelBtn);

  assert(ret);

  reset();

  // Start layout
  beginVLayout();

  m_label = new QLabel(this);
  addWidget(m_label);

  // enbVLayout() must be invoked by derived classes
}

//------------------------------------------------------------------------

void DVGui::ValidatedChoiceDialog::reset() {
  m_appliedToAllRes = NO_REQUIRED_RESOLUTION;
  m_appliedToAll    = false;
}

//------------------------------------------------------------------------

QString validate(void *obj);

int DVGui::ValidatedChoiceDialog::execute(void *obj) {
  struct Resol {
    int m_res;
    bool m_all;
    Resol(int res, bool all) : m_res(res), m_all(all) {}
  };

  Resol curResolution(NO_REQUIRED_RESOLUTION, false),
      newResolution(m_appliedToAll ? m_appliedToAllRes : NO_REQUIRED_RESOLUTION,
                    m_appliedToAll);

  bool initialize = true;

  // Loop until a resolution gets accepted
  do {
    QString err =
        acceptResolution(obj, curResolution.m_res, curResolution.m_all);

    if (err.isEmpty()) break;

    if (newResolution.m_res == NO_REQUIRED_RESOLUTION) {
      // No new resolution selected - prompt for user interaction
      m_label->setText(err);
      m_applyToAll = false;

      if (initialize) initializeUserInteraction(obj), initialize = false;

      if (exec() == QDialog::Rejected) {
        curResolution.m_res = CANCEL;
        break;
      }

      newResolution = Resol(m_buttonGroup->checkedId(), m_applyToAll);
      assert(newResolution.m_res >= DEFAULT_RESOLUTIONS_COUNT);
    }

    // Substitute resolution and retry
    curResolution = newResolution,
    newResolution = Resol(NO_REQUIRED_RESOLUTION, false);

  } while (true);

  return curResolution.m_res;
}

//------------------------------------------------------------------------

void DVGui::ValidatedChoiceDialog::onApplyToAll() {
  m_appliedToAllRes = m_buttonGroup->checkedId();
  m_applyToAll = m_appliedToAll = true;

  assert(m_appliedToAllRes >= DEFAULT_RESOLUTIONS_COUNT);

  accept();
}
