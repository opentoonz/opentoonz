

#include "svnlockframerangedialog.h"

// Tnz6 includes
#include "tapp.h"

// Qt includes
#include <QPushButton>
#include <QLabel>
#include <QMovie>
#include <QBoxLayout>
#include <QHostInfo>
#include <QPlainTextEdit>
#include <QMainWindow>

//=============================================================================
// SVNLockFrameRangeDialog
//-----------------------------------------------------------------------------

SVNLockFrameRangeDialog::SVNLockFrameRangeDialog(QWidget *parent,
                                                 const QString &workingDir,
                                                 const QString &file,
                                                 int frameCount)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_file(file)
    , m_hasError(false)
    , m_fromIsValid(true)
    , m_toIsValid(true) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit Frame Range"));

  setMinimumSize(300, 220);
  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setMargin(0);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;

  m_textLabel->setText(tr("Temporary Lock file..."));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);
  mainLayout->addSpacing(10);

  QHBoxLayout *fromToLayout = new QHBoxLayout;

  m_fromLabel = new QLabel(tr("From:"));
  m_fromLabel->setMaximumWidth(60);
  m_fromLabel->hide();

  m_toLabel = new QLabel(tr("To:"));
  m_toLabel->setMaximumWidth(60);
  m_toLabel->hide();

  m_fromLineEdit = new DVGui::IntLineEdit;
  m_fromLineEdit->setRange(1, frameCount);
  m_fromLineEdit->hide();
  connect(m_fromLineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(onFromLineEditTextChanged()));

  m_toLineEdit = new DVGui::IntLineEdit;
  m_toLineEdit->setRange(1, frameCount);
  m_toLineEdit->hide();
  m_toLineEdit->setValue(frameCount);
  connect(m_toLineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(onToLineEditTextChanged()));

  fromToLayout->addStretch();
  fromToLayout->addWidget(m_fromLabel);
  fromToLayout->addWidget(m_fromLineEdit);
  fromToLayout->addWidget(m_toLabel);
  fromToLayout->addWidget(m_toLineEdit);
  fromToLayout->addStretch();

  mainLayout->addLayout(fromToLayout);

  QHBoxLayout *commentLayout = new QHBoxLayout;

  m_commentTextEdit = new QPlainTextEdit;
  m_commentTextEdit->setMaximumHeight(50);
  m_commentTextEdit->hide();

  m_commentLabel = new QLabel(tr("Comment:"));
  m_commentLabel->setFixedWidth(55);
  m_commentLabel->hide();

  commentLayout->addWidget(m_commentLabel);
  commentLayout->addWidget(m_commentTextEdit);

  mainLayout->addLayout(commentLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_lockButton = new QPushButton(tr("Edit"));
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(onLockButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this,
          SLOT(onCancelButtonClicked()));

  addButtonBarWidget(m_lockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // Step 1: Lock
  QStringList args;
  args << "lock";
  args << m_file;

  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(onLockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_commentTextEdit->hide();
  m_commentLabel->hide();
  m_lockButton->disconnect();
  m_lockButton->setText("Close");
  m_lockButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onError(const QString &errorString) {
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onPropGetDone(const QString &xmlResponse) {
  SVNPartialLockReader reader(xmlResponse);
  QList<SVNPartialLock> lockList = reader.getPartialLock();
  if (lockList.isEmpty())
    m_textLabel->setText(tr("No frame range edited."));
  else {
    m_lockInfos = lockList.at(0).m_partialLockList;
    if (m_lockInfos.isEmpty())
      m_textLabel->setText(tr("No frame range edited."));
    else {
      QString temp;
      for (int i = 0; i < m_lockInfos.size(); i++) {
        if (i != 0) temp.append("\n\n");
        SVNPartialLockInfo lock = m_lockInfos.at(i);
        temp.append(tr("%1 on %2 is editing frames from %3 to %4.")
                        .arg(lock.m_userName)
                        .arg(lock.m_hostName)
                        .arg(lock.m_from)
                        .arg(lock.m_to));
      }
      m_textLabel->setText(temp);
    }
  }

  int height =
      180 + (m_lockInfos.isEmpty() ? 0 : ((m_lockInfos.size() - 1) * 25));
  setMinimumSize(300, height);

  m_lockButton->show();

  m_cancelButton->show();

  // Update from and to lineedit valid status
  onFromLineEditTextChanged();
  onToLineEditTextChanged();

  m_waitingLabel->hide();

  m_fromLabel->show();
  m_toLabel->show();
  m_fromLineEdit->show();
  m_toLineEdit->show();

  m_commentLabel->show();
  m_commentTextEdit->show();
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onLockButtonClicked() {
  // Build the old partial Lock string
  QString partialLockString;

  int count = m_lockInfos.size();
  for (int i = 0; i < count; i++) {
    if (i != 0) partialLockString.append(";");
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    partialLockString.append(lockInfo.m_userName + "@" + lockInfo.m_hostName +
                             ":" + QString::number(lockInfo.m_from) + ":" +
                             QString::number(lockInfo.m_to));
  }

  // Add the new value
  QString userName = VersionControl::instance()->getUserName();
  QString hostName = QHostInfo::localHostName();
  QString from     = QString::number(m_fromLineEdit->getValue());
  QString to       = QString::number(m_toLineEdit->getValue());

  if (count != 0) partialLockString.append(";");
  partialLockString.append(userName + "@" + hostName + ":" + from + ":" + to);

  // Step 3: propset
  QStringList args;
  args << "propset";
  args << "partial-lock";
  args << partialLockString;
  args << m_file;

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onPropSetDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onCancelButtonClicked() {
  QStringList args;
  args << "unlock";
  args << m_file;

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(close()));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onFromLineEditTextChanged() {
  int value = m_fromLineEdit->getValue();

  // Check if from is inside one range
  int count = m_lockInfos.size();

  m_fromIsValid = true;
  for (int i = 0; i < count; i++) {
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    if (value >= lockInfo.m_from && value <= lockInfo.m_to) {
      m_fromIsValid = false;
      break;
    }
  }

  m_fromLineEdit->setStyleSheet(
      m_fromIsValid ? "" : "color: red; background-color: red;");
  m_lockButton->setEnabled(m_fromIsValid && m_toIsValid);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onToLineEditTextChanged() {
  int value = m_toLineEdit->getValue();

  // Check if from is inside one range
  int count   = m_lockInfos.size();
  m_toIsValid = true;
  for (int i = 0; i < count; i++) {
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    if (value >= lockInfo.m_from && value <= lockInfo.m_to) {
      m_toIsValid = false;
      break;
    }
  }

  m_toLineEdit->setStyleSheet(
      m_toIsValid ? "" : "color: red; background-color: red;");
  m_lockButton->setEnabled(m_fromIsValid && m_toIsValid);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onLockDone() {
  if (!m_hasError) {
    m_textLabel->setText(tr("Getting frame range edit information..."));

    // Step 2: propget
    QStringList args;
    args << "proplist";
    args << m_file;
    args << "--xml";
    args << "-v";

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)), this,
            SLOT(onPropGetDone(const QString &)));
    m_thread.executeCommand(m_workingDir, "svn", args, true);
  }
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::onPropSetDone() {
  // Step 4: Commit
  QStringList args;
  args << "commit";
  args << m_file;
  if (!m_commentTextEdit->toPlainText().isEmpty())
    args << QString("-m").append(m_commentTextEdit->toPlainText());
  else
    args << QString("-m").append(VersionControl::instance()->getUserName() +
                                 " edit frame range.");
  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(finish()));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNLockFrameRangeDialog::finish() {
  if (!m_hasError) {
    emit done(QStringList(m_file));
    close();
  }
}

//=============================================================================
// SVNLockMultiFrameRangeDialog
//-----------------------------------------------------------------------------

SVNLockMultiFrameRangeDialog::SVNLockMultiFrameRangeDialog(
    QWidget *parent, const QString &workingDir, const QStringList &files)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_hasError(false)
    , m_fromIsValid(true)
    , m_toIsValid(true) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit Frame Range"));

  setMinimumSize(300, 220);
  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setMargin(0);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;

  m_textLabel->setText(tr("Getting repository status..."));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);
  mainLayout->addSpacing(10);

  QHBoxLayout *fromToLayout = new QHBoxLayout;

  m_fromLabel = new QLabel(tr("From:"));
  m_fromLabel->setMaximumWidth(60);
  m_fromLabel->hide();

  m_toLabel = new QLabel(tr("To:"));
  m_toLabel->setMaximumWidth(60);
  m_toLabel->hide();

  int frameCount = m_files.size();

  m_fromLineEdit = new DVGui::IntLineEdit;
  m_fromLineEdit->setRange(1, frameCount);
  m_fromLineEdit->hide();
  connect(m_fromLineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(onFromLineEditTextChanged()));

  m_toLineEdit = new DVGui::IntLineEdit;
  m_toLineEdit->setRange(1, frameCount);
  m_toLineEdit->hide();
  m_toLineEdit->setValue(frameCount);
  connect(m_toLineEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(onToLineEditTextChanged()));

  fromToLayout->addStretch();
  fromToLayout->addWidget(m_fromLabel);
  fromToLayout->addWidget(m_fromLineEdit);
  fromToLayout->addWidget(m_toLabel);
  fromToLayout->addWidget(m_toLineEdit);
  fromToLayout->addStretch();

  mainLayout->addLayout(fromToLayout);

  QHBoxLayout *commentLayout = new QHBoxLayout;

  m_commentTextEdit = new QPlainTextEdit;
  m_commentTextEdit->setMaximumHeight(50);
  m_commentTextEdit->hide();

  m_commentLabel = new QLabel(tr("Comment:"));
  m_commentLabel->setFixedWidth(55);
  m_commentLabel->hide();

  commentLayout->addWidget(m_commentLabel);
  commentLayout->addWidget(m_commentTextEdit);

  mainLayout->addLayout(commentLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_lockButton = new QPushButton(tr("Edit"));
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(onLockButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  m_lockButton->hide();
  m_cancelButton->hide();

  addButtonBarWidget(m_lockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir, m_files, true);
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_commentTextEdit->hide();
  m_commentLabel->hide();
  m_lockButton->disconnect();
  m_lockButton->setText("Close");
  m_lockButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onError(const QString &errorString) {
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onStatusRetrieved(
    const QString &xmlResponse) {
  m_waitingLabel->hide();

  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  // Fill m_lockInfos list
  SVNPartialLockInfo info;
  info.m_from = 0;
  int count   = m_status.size();
  for (int i = 0; i < count; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_isLocked) {
      if (info.m_userName != s.m_lockOwner && info.m_userName != "") {
        m_lockInfos.append(info);
        info.m_from = 0;
      }
      info.m_userName = s.m_lockOwner;
      if (info.m_from == 0)
        info.m_from = i + 1;
      else
        info.m_to = i + 1;
    }
  }

  if (info.m_userName != "" && info.m_from != 0) m_lockInfos.append(info);

  if (m_lockInfos.isEmpty())
    m_textLabel->setText(tr("No frame range edited."));
  else {
    QString temp;
    for (int i = 0; i < m_lockInfos.size(); i++) {
      if (i != 0) temp.append("\n\n");
      SVNPartialLockInfo lock = m_lockInfos.at(i);
      temp.append(tr("%1 is editing frames from %2 to %3")
                      .arg(lock.m_userName)
                      .arg(lock.m_from)
                      .arg(lock.m_to));
    }
    m_textLabel->setText(temp);
  }

  int height =
      180 + (m_lockInfos.isEmpty() ? 0 : ((m_lockInfos.size() - 1) * 25));
  setMinimumSize(300, height);

  m_lockButton->show();

  m_cancelButton->show();

  // Update from and to lineedit valid status
  onFromLineEditTextChanged();
  onToLineEditTextChanged();

  m_waitingLabel->hide();

  m_fromLabel->show();
  m_toLabel->show();
  m_fromLineEdit->show();
  m_toLineEdit->show();

  m_commentLabel->show();
  m_commentTextEdit->show();
}
//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onFromLineEditTextChanged() {
  int value = m_fromLineEdit->getValue();

  // Check if from is inside one range
  int count = m_lockInfos.size();

  m_fromIsValid = true;
  for (int i = 0; i < count; i++) {
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    if (value >= lockInfo.m_from && value <= lockInfo.m_to) {
      m_fromIsValid = false;
      break;
    }
  }

  m_fromLineEdit->setStyleSheet(
      m_fromIsValid ? "" : "color: red; background-color: red;");
  m_lockButton->setEnabled(m_fromIsValid && m_toIsValid);
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onToLineEditTextChanged() {
  int value = m_toLineEdit->getValue();

  // Check if from is inside one range
  int count   = m_lockInfos.size();
  m_toIsValid = true;
  for (int i = 0; i < count; i++) {
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    if (value >= lockInfo.m_from && value <= lockInfo.m_to) {
      m_toIsValid = false;
      break;
    }
  }

  m_toLineEdit->setStyleSheet(
      m_toIsValid ? "" : "color: red; background-color: red;");
  m_lockButton->setEnabled(m_fromIsValid && m_toIsValid);
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onLockButtonClicked() {
  for (int i = m_fromLineEdit->getValue() - 1; i < m_toLineEdit->getValue();
       i++)
    m_filesToLock.append(m_files.at(i));

  m_waitingLabel->show();
  m_fromLabel->hide();
  m_toLabel->hide();
  m_fromLineEdit->hide();
  m_toLineEdit->hide();
  m_commentLabel->hide();
  m_commentTextEdit->hide();

  m_textLabel->setText(tr("Editing %1 items...").arg(m_filesToLock.size()));

  QStringList args;
  args << "lock";

  int fileCount = m_filesToLock.size();
  for (int i = 0; i < fileCount; i++) args << m_filesToLock.at(i);

  if (!m_commentTextEdit->toPlainText().isEmpty())
    args << QString("-m").append(m_commentTextEdit->toPlainText());
  else
    args << QString("-m").append(VersionControl::instance()->getUserName() +
                                 " edit frame range.");
  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onLockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNLockMultiFrameRangeDialog::onLockDone() {
  if (!m_hasError) {
    QStringList files;
    int fileCount = m_files.size();
    for (int i = 0; i < fileCount; i++) files.append(m_files.at(i));
    emit done(files);
    close();
  }
}

//=============================================================================
// SVNUnlockFrameRangeDialog
//-----------------------------------------------------------------------------

SVNUnlockFrameRangeDialog::SVNUnlockFrameRangeDialog(QWidget *parent,
                                                     const QString &workingDir,
                                                     const QString &file)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_file(file)
    , m_hasError(false) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Unlock Frame Range"));

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

  m_textLabel->setText(
      tr("Note: the file will be updated too. Are you sure ?"));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);
  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_unlockButton = new QPushButton(tr("Unlock"));
  connect(m_unlockButton, SIGNAL(clicked()), this,
          SLOT(onUnlockButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_unlockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onCommitDone() {
  m_textLabel->setText(tr("Unlock done successfully."));

  QStringList files;
  files.append(m_file);
  emit done(files);
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onUpdateDone() {
  m_textLabel->setText(tr("Locking file..."));

  // Step 2: Lock
  QStringList args;
  args << "lock";
  args << m_file;

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(onLockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onLockDone() {
  m_textLabel->setText(tr("Getting frame range edit information..."));

  // Step 3: propget
  QStringList args;
  args << "proplist";
  args << m_file;
  args << "--xml";
  args << "-v";

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onPropGetDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onError(const QString &text) {
  m_textLabel->setText(text);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onPropGetDone(const QString &xmlResponse) {
  SVNPartialLockReader reader(xmlResponse);
  QList<SVNPartialLock> lockList = reader.getPartialLock();
  if (lockList.isEmpty()) {
    m_textLabel->setText(tr("No frame range edited."));
    switchToCloseButton();
    return;
  } else {
    m_lockInfos = lockList.at(0).m_partialLockList;
    if (m_lockInfos.isEmpty()) {
      m_textLabel->setText(tr("No frame range edited."));
      switchToCloseButton();
      return;
    }
  }

  QString partialLockString;

  QString userName = VersionControl::instance()->getUserName();
  QString hostName = QHostInfo::localHostName();

  int count = m_lockInfos.size();

  int entryAdded = 0;
  for (int i = 0; i < count; i++) {
    SVNPartialLockInfo lockInfo = m_lockInfos.at(i);
    bool isMe =
        lockInfo.m_userName == userName && lockInfo.m_hostName == hostName;
    if (!isMe) {
      if (i != 0) partialLockString.append(";");
      partialLockString.append(lockInfo.m_userName + "@" + lockInfo.m_hostName +
                               ":" + QString::number(lockInfo.m_from) + ":" +
                               QString::number(lockInfo.m_to));
      entryAdded++;
    } else
      m_myInfo = lockInfo;
  }

  m_textLabel->setText(tr("Updating frame range edit information..."));

  // There is no more partial-lock > propdel
  if (entryAdded == 0) {
    // Step 4: propdel
    QStringList args;
    args << "propdel";
    args << "partial-lock";
    args << m_file;

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)), this,
            SLOT(onPropSetDone()));
    m_thread.executeCommand(m_workingDir, "svn", args, true);
  }
  // Set the partial-lock property
  else {
    // Step 4: propset
    QStringList args;
    args << "propset";
    args << "partial-lock";
    args << partialLockString;
    args << m_file;

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)), this,
            SLOT(onPropSetDone()));
    m_thread.executeCommand(m_workingDir, "svn", args, true);
  }
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onPropSetDone() {
  m_textLabel->setText(tr("Putting changes..."));

  // Step 5: commit
  QStringList args;
  args << "commit";
  args << m_file;
  args << QString("-m").append(m_myInfo.m_userName + " on " +
                               m_myInfo.m_hostName + " unlock frames from " +
                               QString::number(m_myInfo.m_from) + " to " +
                               QString::number(m_myInfo.m_to) + ".");

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(onCommitDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::onUnlockButtonClicked() {
  m_waitingLabel->show();
  m_textLabel->setText(tr("Updating file..."));

  // Step 1: Update
  QStringList args;
  args << "update";
  args << m_file;

  connect(&m_thread, SIGNAL(done(const QString &)), this, SLOT(onUpdateDone()));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUnlockFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_unlockButton->disconnect();
  m_unlockButton->setText(tr("Close"));
  m_unlockButton->setEnabled(true);
  m_unlockButton->show();
  m_cancelButton->hide();
  connect(m_unlockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//=============================================================================
// SVNUnlockMultiFrameRangeDialog
//-----------------------------------------------------------------------------

SVNUnlockMultiFrameRangeDialog::SVNUnlockMultiFrameRangeDialog(
    QWidget *parent, const QString &workingDir, const QStringList &files)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_hasError(false) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Unlock Frame Range"));

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

  m_textLabel->setText(tr("Getting repository status..."));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);
  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_unlockButton = new QPushButton(tr("Unlock"));
  connect(m_unlockButton, SIGNAL(clicked()), this,
          SLOT(onUnlockButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  m_unlockButton->hide();

  addButtonBarWidget(m_unlockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir, m_files, true);
}
//-----------------------------------------------------------------------------

void SVNUnlockMultiFrameRangeDialog::onUnlockButtonClicked() {
  m_waitingLabel->show();

  int fileCount = m_filesToUnlock.size();

  m_textLabel->setText(tr("Unlocking %1 items...").arg(fileCount));

  QStringList args;
  args << "unlock";

  for (int i = 0; i < fileCount; i++) args << m_filesToUnlock.at(i);

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onUnlockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNUnlockMultiFrameRangeDialog::onStatusRetrieved(
    const QString &xmlResponse) {
  m_waitingLabel->hide();

  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  QString userName = VersionControl::instance()->getUserName();

  int count = m_status.size();
  for (int i = 0; i < count; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_isLocked && s.m_lockOwner == userName)
      m_filesToUnlock.append(m_files.at(i));
  }

  int filesToUnlockCount = m_filesToUnlock.size();

  if (filesToUnlockCount == 0) {
    m_textLabel->setText(tr("No items to unlock."));
    switchToCloseButton();
  } else {
    m_waitingLabel->hide();
    m_textLabel->setText(tr("%1 items to unlock.").arg(filesToUnlockCount));
    m_unlockButton->show();
  }
}

//-----------------------------------------------------------------------------

void SVNUnlockMultiFrameRangeDialog::onError(const QString &text) {
  m_textLabel->setText(text);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNUnlockMultiFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_unlockButton->disconnect();
  m_unlockButton->setText("Close");
  m_unlockButton->setEnabled(true);
  m_unlockButton->show();
  m_cancelButton->hide();
  connect(m_unlockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNUnlockMultiFrameRangeDialog::onUnlockDone() {
  if (!m_hasError) {
    QStringList files;
    int fileCount = m_filesToUnlock.size();
    for (int i = 0; i < fileCount; i++) files.append(m_filesToUnlock.at(i));
    emit done(files);
    close();
  }
}

//=============================================================================
// SVNFrameRangeLockInfoDialog
//-----------------------------------------------------------------------------

SVNFrameRangeLockInfoDialog::SVNFrameRangeLockInfoDialog(
    QWidget *parent, const QString &workingDir, const QString &file)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_file(file) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit Info"));

  setMinimumSize(300, 150);

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;

  m_textLabel->setText(tr("Getting repository status..."));

  QHBoxLayout *mainLayout = new QHBoxLayout;
  mainLayout->addStretch();
  mainLayout->addWidget(m_waitingLabel);
  mainLayout->addWidget(m_textLabel);
  mainLayout->addStretch();

  beginHLayout();
  addLayout(mainLayout, false);
  endHLayout();

  QPushButton *closeButton = new QPushButton(tr("Close"));
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

  addButtonBarWidget(closeButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. propget
  QStringList args;
  args << "proplist";
  args << m_file;
  args << "--xml";
  args << "-v";

  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onPropGetDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void SVNFrameRangeLockInfoDialog::onError(const QString &errorString) {
  m_waitingLabel->hide();
  m_textLabel->setText(errorString);
}

//-----------------------------------------------------------------------------

void SVNFrameRangeLockInfoDialog::onPropGetDone(const QString &xmlResponse) {
  m_waitingLabel->hide();

  SVNPartialLockReader reader(xmlResponse);
  QList<SVNPartialLock> lockList = reader.getPartialLock();
  if (lockList.isEmpty())
    m_textLabel->setText(tr("No frame range edited."));
  else {
    QList<SVNPartialLockInfo> lockInfos = lockList.at(0).m_partialLockList;
    if (lockInfos.isEmpty())
      m_textLabel->setText(tr("No frame range edited."));
    else {
      QString temp;
      for (int i = 0; i < lockInfos.size(); i++) {
        if (i != 0) temp.append("\n\n");
        SVNPartialLockInfo lock = lockInfos.at(i);
        temp.append(tr("%1 on %2 is editing frames from %3 to %4.")
                        .arg(lock.m_userName)
                        .arg(lock.m_hostName)
                        .arg(lock.m_from)
                        .arg(lock.m_to));
      }
      int height = 100 + ((lockInfos.size() - 1) * 25);
      setMinimumSize(300, height);

      m_textLabel->setText(temp);
    }
  }
}

//=============================================================================
// SVNMultiFrameRangeLockInfoDialog
//-----------------------------------------------------------------------------

SVNMultiFrameRangeLockInfoDialog::SVNMultiFrameRangeLockInfoDialog(
    QWidget *parent, const QString &workingDir, const QStringList &files)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit Info"));

  setMinimumSize(300, 150);

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;

  m_textLabel->setText(tr("Getting repository status..."));

  QHBoxLayout *mainLayout = new QHBoxLayout;
  mainLayout->addStretch();
  mainLayout->addWidget(m_waitingLabel);
  mainLayout->addWidget(m_textLabel);
  mainLayout->addStretch();

  beginHLayout();
  addLayout(mainLayout, false);
  endHLayout();

  QPushButton *closeButton = new QPushButton(tr("Close"));
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

  addButtonBarWidget(closeButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir, m_files, true);
}

//-----------------------------------------------------------------------------

void SVNMultiFrameRangeLockInfoDialog::onError(const QString &errorString) {
  m_waitingLabel->hide();
  m_textLabel->setText(errorString);
}

//-----------------------------------------------------------------------------

void SVNMultiFrameRangeLockInfoDialog::onStatusRetrieved(
    const QString &xmlResponse) {
  m_waitingLabel->hide();

  SVNStatusReader sr(xmlResponse);
  QList<SVNStatus> status = sr.getStatus();

  // Fill_lockInfos list
  QList<SVNPartialLockInfo> lockInfos;
  SVNPartialLockInfo info;
  info.m_from = 0;
  int count   = status.size();
  for (int i = 0; i < count; i++) {
    SVNStatus s = status.at(i);
    if (s.m_isLocked) {
      if (info.m_userName != s.m_lockOwner && info.m_userName != "") {
        lockInfos.append(info);
        info.m_from = 0;
      }
      info.m_userName = s.m_lockOwner;
      if (info.m_from == 0) {
        info.m_from = i + 1;
        info.m_to   = i + 1;
      } else
        info.m_to = i + 1;
    }
  }

  if (info.m_userName != "" && info.m_from != 0) lockInfos.append(info);

  if (lockInfos.isEmpty())
    m_textLabel->setText(tr("No frame range edited."));
  else {
    QString temp;
    for (int i = 0; i < lockInfos.size(); i++) {
      if (i != 0) temp.append("\n\n");
      SVNPartialLockInfo lock = lockInfos.at(i);
      temp.append(tr("%1 is editing frames from %2 to %3")
                      .arg(lock.m_userName)
                      .arg(lock.m_from)
                      .arg(lock.m_to));
    }
    int height = 100 + ((lockInfos.size() - 1) * 25);
    setMinimumSize(300, height);

    m_textLabel->setText(temp);
  }
}
