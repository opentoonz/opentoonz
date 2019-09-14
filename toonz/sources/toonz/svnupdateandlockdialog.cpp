

#include "svnupdateandlockdialog.h"

// Tnz6 includes
#include "tapp.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QWidget>
#include <QMovie>
#include <QBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QMainWindow>

//=============================================================================
// SVNUpdateAndLockDialog
//-----------------------------------------------------------------------------

SVNUpdateAndLockDialog::SVNUpdateAndLockDialog(QWidget *parent,
                                               const QString &workingDir,
                                               const QStringList &files,
                                               int workingRevision,
                                               int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_editSceneContentsCheckBox(0)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_workingRevision(workingRevision)
    , m_hasError(false)
    , m_sceneIconAdded(sceneIconAdded)
    , m_updateWorkingRevisionNeeded(false) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit"));

  setMinimumSize(300, 150);

  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setMargin(0);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);
  m_waitingLabel->hide();

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;

  m_textLabel->setText("The file has an update version on the server...");

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);
  mainLayout->addSpacing(10);

  QHBoxLayout *commentLayout = new QHBoxLayout;

  m_commentTextEdit = new QPlainTextEdit;
  m_commentTextEdit->setMaximumHeight(50);

  m_commentLabel = new QLabel(tr("Comment:"));
  m_commentLabel->setFixedWidth(55);

  commentLayout->addWidget(m_commentLabel);
  commentLayout->addWidget(m_commentTextEdit);

  mainLayout->addLayout(commentLayout);

  m_editSceneContentsCheckBox = new QCheckBox(this);
  m_editSceneContentsCheckBox->setChecked(false);
  m_editSceneContentsCheckBox->setText(tr("Edit Scene Contents"));

  m_editSceneContentsCheckBox->hide();

  connect(m_editSceneContentsCheckBox, SIGNAL(toggled(bool)), this,
          SLOT(onEditSceneContentsToggled(bool)));

  int fileSize = m_files.size();
  for (int i = 0; i < fileSize; i++) {
    if (m_files.at(i).endsWith(".tnz")) {
      m_editSceneContentsCheckBox->show();
      break;
    }
  }

  QHBoxLayout *sceneContentsLayout = new QHBoxLayout;
  sceneContentsLayout->addStretch();
  sceneContentsLayout->addWidget(m_editSceneContentsCheckBox);
  sceneContentsLayout->addStretch();

  mainLayout->addLayout(sceneContentsLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_updateAndLockButton = new QPushButton(tr("Get And Edit "));
  connect(m_updateAndLockButton, SIGNAL(clicked()), this,
          SLOT(onUpdateAndLockButtonClicked()));

  m_updateAndLockButton->hide();

  m_lockButton = new QPushButton(tr("Edit"));
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(onLockButtonClicked()));

  m_lockButton->hide();

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_updateAndLockButton, m_lockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir);
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));

  checkFiles();

  if (m_filesToEdit.size() == 0) {
    m_textLabel->setText(tr("No items to edit."));
    m_commentTextEdit->show();
    m_commentLabel->show();
    switchToCloseButton();
  } else {
    setMinimumSize(300, 150);

    m_waitingLabel->hide();

    m_textLabel->setText(
        tr("%1 items to edit.").arg(m_filesToEdit.size() - m_sceneIconAdded));
    m_lockButton->show();
    m_updateAndLockButton->show();
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if (s.m_item != "unversioned" && s.m_item != "missing") {
      if (m_files.contains(s.m_path)) {
        if (!s.m_isLocked) m_filesToEdit.append(s.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::switchToCloseButton() {
  m_editSceneContentsCheckBox->hide();
  m_waitingLabel->hide();
  m_commentTextEdit->hide();
  m_commentLabel->hide();
  m_updateAndLockButton->disconnect();
  m_updateAndLockButton->setText("Close");
  m_updateAndLockButton->setEnabled(true);
  m_lockButton->hide();
  m_cancelButton->hide();
  connect(m_updateAndLockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onError(const QString &errorString) {
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onLockButtonClicked() {
  m_updateWorkingRevisionNeeded = true;
  updateCommand();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onUpdateAndLockButtonClicked() {
  m_updateWorkingRevisionNeeded = false;
  updateCommand();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onSceneResourcesStatusRetrieved(
    const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));

  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if (s.m_item != "unversioned" && s.m_item != "missing") {
      if (!s.m_isLocked) m_filesToEdit.append(s.m_path);
    }
  }
  lockCommand();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::updateCommand() {
  m_commentLabel->hide();
  m_commentTextEdit->hide();
  m_textLabel->setText(tr("Updating %1 items...")
                           .arg(m_filesToEdit.size() + m_sceneResources.size() -
                                m_sceneIconAdded));
  m_waitingLabel->show();
  // Step 1: update
  QStringList args;
  args << "update";

  int fileCount = m_filesToEdit.size();
  for (int i = 0; i < fileCount; i++) args << m_files.at(i);

  int resourceCount = m_sceneResources.size();
  for (int i = 0; i < resourceCount; i++) args << m_sceneResources.at(i);

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onUpdateDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onUpdateDone() {
  // Check the status of sceneResources
  if (!m_sceneResources.empty()) {
    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
            SLOT(onSceneResourcesStatusRetrieved(const QString &)));
    m_thread.getSVNStatus(m_workingDir, m_sceneResources);
    return;
  } else
    lockCommand();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::lockCommand() {
  m_textLabel->setText(
      tr("Editing %1 items...").arg(m_filesToEdit.size() - m_sceneIconAdded));

  // Step 2: lock
  QStringList args;
  args << "lock";

  int fileCount = m_filesToEdit.size();
  for (int i = 0; i < fileCount; i++) args << m_filesToEdit.at(i);

  if (!m_commentTextEdit->toPlainText().isEmpty())
    args << QString("-m").append(TSystem::getHostName() + ":" +
                                 m_commentTextEdit->toPlainText());
  else
    args << QString("-m").append(TSystem::getHostName() + ":" +
                                 VersionControl::instance()->getUserName() +
                                 " edit files.");

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onLockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onLockDone() {
  // Step 3: reverse merging (== update To revision with modified state)
  if (m_updateWorkingRevisionNeeded) {
    m_filesToMerge = m_filesToEdit;
    reverseMerge();
  } else
    finish();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::reverseMerge() {
  if (!m_filesToMerge.isEmpty()) {
    QString file = m_filesToMerge.takeFirst();

    QStringList args;
    args << "merge";
    args << QString("-rHEAD:").append(QString::number(m_workingRevision));
    args << file;

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)), SLOT(reverseMerge()));
    m_thread.executeCommand(m_workingDir, "svn", args, false);
  } else
    finish();
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::finish() {
  if (!m_hasError) {
    emit done(m_filesToEdit);
    close();
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateAndLockDialog::onEditSceneContentsToggled(bool checked) {
  if (!checked)
    m_sceneResources.clear();
  else {
    VersionControl *vc = VersionControl::instance();

    int fileSize = m_files.count();
    for (int i = 0; i < fileSize; i++) {
      QString fileName = m_files.at(i);
      if (fileName.endsWith(".tnz"))
        m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
    }
  }

  m_textLabel->setText(tr("%1 items to edit.")
                           .arg(m_filesToEdit.size() + m_sceneResources.size() -
                                m_sceneIconAdded));
}
