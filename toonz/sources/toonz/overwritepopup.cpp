

#include "overwritepopup.h"

// Tnz6 includes
#include "tapp.h"
#include "tsystem.h"

// TnzQt includes
#include "toonzqt/checkbox.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"

// TnzCore includes
#include "tfilepath.h"

// Qt includes
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QApplication>
#include <QMainWindow>

//************************************************************************************
//    OverwriteDialog::ExistsFunc  implementation
//************************************************************************************

QString OverwriteDialog::DecodeFileExistsFunc::conflictString(
    const TFilePath &fp) const {
  return OverwriteDialog::tr(
             "File \"%1\" already exists.\nWhat do you want to do?")
      .arg(toQString(fp));
}

//----------------------------------------------------------------------------------

bool OverwriteDialog::DecodeFileExistsFunc::operator()(
    const TFilePath &fp) const {
  return TSystem::doesExistFileOrLevel(m_scene->decodeFilePath(fp));
}

//************************************************************************************
//    OverwriteDialog  implementation
//************************************************************************************

OverwriteDialog::OverwriteDialog()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true) {
  setModal(true);
  setWindowTitle(tr("Warning!"));

  QButtonGroup *buttonGroup = new QButtonGroup(this);
  buttonGroup->setExclusive(true);
  bool ret = connect(buttonGroup, SIGNAL(buttonClicked(int)), this,
                     SLOT(onButtonClicked(int)));

  beginVLayout();

  m_label = new QLabel(this);
  addWidget(m_label);

  m_keep = new QRadioButton(tr("Keep existing file"), this);
  buttonGroup->addButton(m_keep);
  addWidget(m_keep);

  m_overwrite = new QRadioButton(
      tr("Overwrite the existing file with the new one"), this);
  buttonGroup->addButton(m_overwrite);
  addWidget(m_overwrite);

  m_rename =
      new QRadioButton(tr("Rename the new file adding the suffix"), this);
  buttonGroup->addButton(m_rename);

  m_suffix = new DVGui::LineEdit("_1", this);
  m_suffix->setFixedWidth(25);
  m_suffix->setEnabled(false);

  QHBoxLayout *boxLayout = new QHBoxLayout();
  boxLayout->setMargin(0);
  boxLayout->setSpacing(0);
  boxLayout->addWidget(m_rename);
  boxLayout->addWidget(m_suffix);
  boxLayout->setAlignment(m_rename, Qt::AlignLeft);
  boxLayout->setAlignment(m_suffix, Qt::AlignLeft);
  addLayout(boxLayout);

  endVLayout();

  m_okBtn = new QPushButton(QString(tr("Apply")), this);
  ret     = ret && connect(m_okBtn, SIGNAL(clicked()), this, SLOT(accept()));
  addButtonBarWidget(m_okBtn);

  m_okToAllBtn = new QPushButton(QString(tr("Apply to All")), this);
  ret =
      ret && connect(m_okToAllBtn, SIGNAL(clicked()), this, SLOT(applyToAll()));
  addButtonBarWidget(m_okToAllBtn);

  m_cancelBtn = new QPushButton(QString(tr("Cancel")), this);
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(cancel()));
  addButtonBarWidget(m_cancelBtn);

  assert(ret);

  reset();
  m_keep->setChecked(true);
}

//----------------------------------------------------------------------------------

void OverwriteDialog::reset() {
  m_choice        = KEEP_OLD;
  m_applyToAll    = false;
  m_cancelPressed = false;
}

//----------------------------------------------------------------------------------

std::wstring OverwriteDialog::getSuffix() {
  return m_suffix->text().toStdWString();
}

//----------------------------------------------------------------------------------

void OverwriteDialog::applyToAll() {
  m_applyToAll = true;
  accept();
}

//----------------------------------------------------------------------------------

void OverwriteDialog::cancel() {
  m_cancelPressed = true;
  reject();
}

//----------------------------------------------------------------------------------

void OverwriteDialog::onButtonClicked(int) {
  m_suffix->setEnabled(m_rename->isChecked());
}

//----------------------------------------------------------------------------------

TFilePath OverwriteDialog::addSuffix(const TFilePath &src) const {
  return src.withName(src.getWideName() + m_suffix->text().toStdWString());
}

//----------------------------------------------------------------------------------

OverwriteDialog::Resolution OverwriteDialog::execute(TFilePath &filePath,
                                                     const ExistsFunc &exists,
                                                     Resolution acceptedRes,
                                                     Flags flags) {
  typedef QRadioButton *OverwriteDialog::*RadioRes;
  static const RadioRes radios[3] = {&OverwriteDialog::m_keep,
                                     &OverwriteDialog::m_overwrite,
                                     &OverwriteDialog::m_rename};

  struct locals {
    static inline int idx(Resolution res) {
      int r = 0;
      while (r < 3 && !(res & (1 << r))) ++r;
      return r;
    }
  };

  TFilePath writePath = filePath;

  // Deal with the case where "Apply To All" was already clicked
  if (m_applyToAll && (m_choice & acceptedRes)) {
    if (m_choice == RENAME) writePath = addSuffix(writePath);

    if (m_choice != RENAME || !exists(writePath))
      return filePath = writePath, m_choice;
  }

  reset();

  // Find a compatible suffix to be displayed
  if (acceptedRes & RENAME)
    for (int i = 1; exists(addSuffix(filePath)); ++i)
      m_suffix->setText("_" + QString::number(i));

  // Show the various dialog components as specified
  m_overwrite->setVisible(acceptedRes & OVERWRITE);
  m_keep->setVisible(acceptedRes & KEEP_OLD);
  m_rename->setVisible(acceptedRes & RENAME);
  m_suffix->setVisible(acceptedRes & RENAME);
  m_okToAllBtn->setVisible(flags & APPLY_TO_ALL_FLAG);

  // Ensure that there is a checked button among the accepted resolutions
  for (int r = 0; r < 3; ++r)
    if ((this->*radios[r])->isChecked()) {
      if (!(acceptedRes & (1 << r)))
        (this->*radios[locals::idx(acceptedRes)])->setChecked(true);
      break;
    }

  // Prompt the dialog to let the user decide resolution
  while (true) {
    // Build text to be displayed
    m_label->setText(exists.conflictString(writePath));

    // Execute dialog
    int retCode = exec();

    m_choice = (retCode == QDialog::Rejected)
                   ? CANCELED
                   : m_overwrite->isChecked()
                         ? OVERWRITE
                         : m_rename->isChecked() ? RENAME : KEEP_OLD;

    if (m_choice == RENAME) {
      if (exists(writePath = addSuffix(filePath))) continue;
    }

    break;
  }

  return filePath = writePath, m_choice;
}

//----------------------------------------------------------------------------------

std::wstring OverwriteDialog::execute(ToonzScene *scene,
                                      const TFilePath &srcLevelPath,
                                      bool multiload) {
  TFilePath levelPath       = srcLevelPath;
  TFilePath actualLevelPath = scene->decodeFilePath(levelPath);
  if (!TSystem::doesExistFileOrLevel(actualLevelPath))
    return levelPath.getWideName();

  if (m_applyToAll && m_choice == RENAME) {
    levelPath       = addSuffix(levelPath);
    actualLevelPath = scene->decodeFilePath(levelPath);
  }
  if (m_applyToAll) {
    if (m_choice != RENAME || !TSystem::doesExistFileOrLevel(actualLevelPath))
      return levelPath.getWideName();
  }

  m_label->setText(tr("File %1 already exists.\nWhat do you want to do?")
                       .arg(toQString(levelPath)));
  // find a compatible suffix
  if (TSystem::doesExistFileOrLevel(actualLevelPath)) {
    int i = 0;
    while (TSystem::doesExistFileOrLevel(
        scene->decodeFilePath(addSuffix(srcLevelPath)))) {
      m_suffix->setText("_" + QString::number(++i));
    }
  }

  if (multiload)
    m_okToAllBtn->show();
  else
    m_okToAllBtn->hide();

  // there could be a WaitCursor cursor
  QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
  raise();
  exec();
  QApplication::restoreOverrideCursor();

  if (m_rename->isChecked()) {
    if (m_suffix->text() == "") {
      DVGui::warning(tr("The suffix field is empty. Please specify a suffix."));
      return execute(scene, srcLevelPath, multiload);
    }
    levelPath       = addSuffix(srcLevelPath);
    actualLevelPath = scene->decodeFilePath(levelPath);
    if (TSystem::doesExistFileOrLevel(actualLevelPath)) {
      DVGui::warning(
          tr("File %1 exists as well; please choose a different suffix.")
              .arg(toQString(levelPath)));
      return execute(scene, srcLevelPath, multiload);
    }
    m_choice = RENAME;
  } else if (m_overwrite->isChecked())
    m_choice = OVERWRITE;
  else {
    assert(m_keep->isChecked());
    m_choice = KEEP_OLD;
  }

  return levelPath.getWideName();
}
