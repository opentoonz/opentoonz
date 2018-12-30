

#include "svnlockdialog.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QWidget>
#include <QBoxLayout>
#include <QFormLayout>
#include <QMovie>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTime>
#include <QCheckBox>
#include <QHeaderView>
#include <QTreeWidget>
#include <QTextStream>
#include <QMainWindow>

//=============================================================================
// SVNLockDialog
//-----------------------------------------------------------------------------

SVNLockDialog::SVNLockDialog(QWidget *parent, const QString &workingDir,
                             const QStringList &files, bool lock,
                             int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_editSceneContentsCheckBox(0)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_lock(lock)
    , m_hasError(false)
    , m_sceneIconAdded(sceneIconAdded)
    , m_targetTempFile(0) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  if (m_lock)
    setWindowTitle(tr("Version Control: Edit"));
  else
    setWindowTitle(tr("Version Control: Unlock"));

  setMinimumSize(300, 150);

  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setMargin(0);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel = new QLabel;
  m_waitingLabel->setMaximumHeight(55);

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

  m_treeWidget = new QTreeWidget;
  m_treeWidget->setStyleSheet("QTreeWidget { border: 1px solid gray; }");
  m_treeWidget->header()->hide();
  m_treeWidget->hide();
  mainLayout->addWidget(m_treeWidget);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->setLabelAlignment(Qt::AlignRight);
  formLayout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
  formLayout->setSpacing(10);
  formLayout->setMargin(0);

  m_commentTextEdit = new QPlainTextEdit;
  m_commentTextEdit->setMaximumHeight(50);

  m_commentLabel = new QLabel(tr("Comment:"));
  m_commentLabel->setFixedWidth(55);

  if (!m_lock) {
    m_commentTextEdit->hide();
    m_commentLabel->hide();
  }

  formLayout->addRow(m_commentLabel, m_commentTextEdit);

  m_editSceneContentsCheckBox = new QCheckBox(this);
  m_editSceneContentsCheckBox->setChecked(false);
  if (m_lock)
    m_editSceneContentsCheckBox->setText(tr("Edit Scene Contents"));
  else
    m_editSceneContentsCheckBox->setText(tr("Unlock Scene Contents"));

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

  formLayout->addRow("", m_editSceneContentsCheckBox);

  mainLayout->addLayout(formLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_lockButton = new QPushButton();
  if (m_lock)
    m_lockButton->setText(tr("Edit"));
  else
    m_lockButton->setText(tr("Unlock"));

  m_lockButton->hide();

  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(onLockButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_lockButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs everythings)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir);
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));

  checkFiles();

  initTreeWidget();

  if (m_filesToEdit.size() == 0) {
    if (m_lock) {
      m_textLabel->setText(tr("No items to edit."));
      m_commentTextEdit->show();
      m_commentLabel->show();
    } else
      m_textLabel->setText(tr("No items to unlock."));

    switchToCloseButton();
  } else {
    m_waitingLabel->hide();

    if (m_lock)
      m_textLabel->setText(
          tr("%1 items to edit.")
              .arg(m_filesToEdit.size() == 1
                       ? 1
                       : m_filesToEdit.size() - m_sceneIconAdded));
    else
      m_textLabel->setText(
          tr("%1 items to unlock.")
              .arg(m_filesToEdit.size() == 1
                       ? 1
                       : m_filesToEdit.size() - m_sceneIconAdded));

    m_lockButton->show();
  }
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onSceneResourcesStatusRetrieved(
    const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));

  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if (s.m_item != "unversioned" && s.m_item != "missing") {
      if (m_lock && !s.m_isLocked) m_filesToEdit.append(s.m_path);
      if (!m_lock && s.m_isLocked) m_filesToEdit.append(s.m_path);
    }
  }

  executeCommand();
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onError(const QString &errorString) {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = 0;
  }

  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onLockButtonClicked() {
  m_waitingLabel->show();
  m_commentLabel->hide();
  m_commentTextEdit->hide();
  m_treeWidget->hide();
  m_editSceneContentsCheckBox->hide();

  // Check the status of sceneResources
  if (!m_sceneResources.empty()) {
    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
            SLOT(onSceneResourcesStatusRetrieved(const QString &)));
    m_thread.getSVNStatus(m_workingDir, m_sceneResources);
    return;
  } else
    executeCommand();
}

//-----------------------------------------------------------------------------

void SVNLockDialog::executeCommand() {
  if (m_lock)
    m_textLabel->setText(
        tr("Editing %1 items...")
            .arg(m_filesToEdit.size() == 1
                     ? 1
                     : m_filesToEdit.size() - m_sceneIconAdded));
  else
    m_textLabel->setText(
        tr("Unlocking %1 items...")
            .arg(m_filesToEdit.size() == 1
                     ? 1
                     : m_filesToEdit.size() - m_sceneIconAdded));

  QStringList args;
  if (m_lock)
    args << "lock";
  else
    args << "unlock";

  // Use a temporary file to store all the files list
  m_targetTempFile = new QFile(m_workingDir + "/" + "tempLockFile");
  if (m_targetTempFile->open(QFile::WriteOnly | QFile::Truncate)) {
    QTextStream out(m_targetTempFile);

    int fileCount = m_filesToEdit.size();
    for (int i = 0; i < fileCount; i++) {
      QString path = m_filesToEdit.at(i);
      ;
      out << path + "\n";
    }
  }
  m_targetTempFile->close();

  args << "--targets";
  args << "tempLockFile";

  if (m_lock) {
    if (!m_commentTextEdit->toPlainText().isEmpty())
      args << QString("-m").append(TSystem::getHostName() + ":" +
                                   m_commentTextEdit->toPlainText());
    else
      args << QString("-m").append(TSystem::getHostName() + ":" +
                                   VersionControl::instance()->getUserName() +
                                   " edit files.");
  }

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onLockDone()));
  m_thread.executeCommand(m_workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onLockDone() {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = 0;
  }

  if (!m_hasError) {
    QStringList files;
    int fileCount = m_files.size();
    for (int i = 0; i < fileCount; i++) files.append(m_files.at(i));
    emit done(files);
    close();
  }
}

//-----------------------------------------------------------------------------

void SVNLockDialog::initTreeWidget() {
  int filesSize = m_filesToEdit.size();

  bool itemAdded = false;

  for (int i = 0; i < filesSize; i++) {
    QString fileName = m_filesToEdit.at(i);
    TFilePath fp =
        TFilePath(m_workingDir.toStdWString()) + fileName.toStdWString();
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    QStringList linkedFiles;

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it) {
      QString fn = toQString((*it).withoutParentDir());
      if (m_filesToEdit.contains(fn)) linkedFiles.append(fn);
    }

    if (!linkedFiles.isEmpty()) {
      itemAdded            = true;
      QTreeWidgetItem *twi = new QTreeWidgetItem(m_treeWidget);
      twi->setText(0, fileName);
      twi->setFirstColumnSpanned(false);
      twi->setFlags(Qt::NoItemFlags);

      for (int i = 0; i < linkedFiles.size(); i++) {
        QTreeWidgetItem *child = new QTreeWidgetItem(twi);
        child->setText(0, linkedFiles.at(i));
        child->setFlags(Qt::NoItemFlags);
      }
      twi->setExpanded(true);
    }
  }

  if (itemAdded) m_treeWidget->show();
}

//-----------------------------------------------------------------------------

void SVNLockDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if (s.m_item != "unversioned" && s.m_item != "missing") {
      if (m_files.contains(s.m_path)) {
        if (m_lock && !s.m_isLocked) m_filesToEdit.append(s.m_path);
        if (!m_lock && s.m_isLocked) m_filesToEdit.append(s.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNLockDialog::switchToCloseButton() {
  m_editSceneContentsCheckBox->hide();
  m_commentLabel->hide();
  m_commentTextEdit->hide();
  m_treeWidget->hide();
  m_waitingLabel->hide();
  m_lockButton->disconnect();
  m_lockButton->setText("Close");
  m_lockButton->setEnabled(true);
  m_lockButton->show();
  m_cancelButton->hide();
  connect(m_lockButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNLockDialog::onEditSceneContentsToggled(bool checked) {
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

  if (m_lock)
    m_textLabel->setText(
        tr("%1 items to edit.")
            .arg(m_filesToEdit.size() + m_sceneResources.size() == 1
                     ? 1
                     : m_filesToEdit.size() + m_sceneResources.size() -
                           m_sceneIconAdded));
  else
    m_textLabel->setText(
        tr("%1 items to unlock.")
            .arg(m_filesToEdit.size() + m_sceneResources.size() == 1
                     ? 1
                     : m_filesToEdit.size() + m_sceneResources.size() -
                           m_sceneIconAdded));
}

//=============================================================================
// SVNLockInfoDialog
//-----------------------------------------------------------------------------

SVNLockInfoDialog::SVNLockInfoDialog(QWidget *parent, const SVNStatus &status)
    : Dialog(TApp::instance()->getMainWindow(), true, false), m_status(status) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Edit Info"));

  setMinimumSize(300, 150);

  QFormLayout *mainLayout = new QFormLayout;
  mainLayout->setMargin(0);
  mainLayout->setLabelAlignment(Qt::AlignLeft);

  mainLayout->addRow(tr("<b>Edited By:</b>"), new QLabel(m_status.m_lockOwner));
  mainLayout->addRow(tr("<b>Host:</b>"), new QLabel(m_status.m_lockHostName));
  mainLayout->addRow(tr("<b>Comment:</b>"), new QLabel(m_status.m_lockComment));

  QDate d      = QDate::fromString(m_status.m_lockDate.left(10), "yyyy-MM-dd");
  int dayCount = d.daysTo(QDate::currentDate());
  QString dateString;

  if (dayCount == 0) {
    QString timeString = m_status.m_lockDate.split("T").at(1);
    timeString         = timeString.left(5);

    // Convert the current time, to UTC
    QDateTime currentTime = QDateTime::currentDateTime().toUTC();

    QTime t     = QTime::fromString(timeString, "hh:mm");
    QTime now   = QTime::fromString(currentTime.toString("hh:mm"), "hh:mm");
    int seconds = t.secsTo(now);
    int minute  = seconds / 60;
    dateString += QString::number(minute) + " minutes ago.";
  } else
    dateString += QString::number(dayCount) + " days ago.";

  mainLayout->addRow(tr("<b>Date:</b>"), new QLabel(dateString)),
      // container->setLayout(mainLayout);

      beginHLayout();
  addLayout(mainLayout, false);
  endHLayout();

  QPushButton *closeButton = new QPushButton(tr("Close"));
  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

  addButtonBarWidget(closeButton);
}
