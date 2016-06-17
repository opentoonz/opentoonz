

#include "svnpurgedialog.h"

// Tnz6 includes
#include "tapp.h"
#include "filebrowser.h"
#include "fileselection.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QPushButton>
#include <QMovie>
#include <QLabel>
#include <QMainWindow>

//=============================================================================
// SVNPurgeDialog
//-----------------------------------------------------------------------------

SVNPurgeDialog::SVNPurgeDialog(QWidget *parent, const QString &workingDir)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Purge"));

  setMinimumSize(300, 150);

  beginVLayout();

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;
  m_textLabel->setText(tr("Note: the file will be updated too."));

  m_textLabel->setText(tr("Getting repository status..."));

  addWidgets(m_waitingLabel, m_textLabel);

  endVLayout();

  m_purgeButton = new QPushButton(tr("Purge"));
  m_purgeButton->setEnabled(false);
  connect(m_purgeButton, SIGNAL(clicked()), this, SLOT(onPurgeClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_purgeButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs everythings)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status (with show-updates enabled...)
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir);
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::onError(const QString &text) {
  m_textLabel->setText(text);
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  checkFiles();

  int filesToPurgrCount = m_filesToPurge.size();
  if (filesToPurgrCount == 0) {
    m_textLabel->setText(tr("No items to purge."));
    switchToCloseButton();
  } else {
    m_textLabel->setText(tr("%1 items to purge.").arg(filesToPurgrCount));
    m_waitingLabel->hide();
    m_purgeButton->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;

    if (s.m_item == "normal") m_filesToPurge.append(s.m_path);
  }
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_purgeButton->disconnect();
  m_purgeButton->setText("Close");
  m_purgeButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_purgeButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::onPurgeClicked() {
  m_waitingLabel->show();
  m_textLabel->setText(tr("Purging files..."));
  m_cancelButton->hide();

  // Delete files locally
  int fileCount = m_filesToPurge.size();
  for (int i = 0; i < fileCount; i++) {
    TFilePath fp((m_workingDir + "/" + m_filesToPurge.at(i)).toStdWString());
    TSystem::moveFileOrLevelToRecycleBin(fp);
  }

  updateFileBrowser();

  emit done(m_filesToPurge);
  close();
}

//-----------------------------------------------------------------------------

void SVNPurgeDialog::updateFileBrowser() {
  // Reset the selection
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  FileSelection *fileSelection = dynamic_cast<FileSelection *>(selection);
  if (fileSelection) fileSelection->selectNone();

  TFilePath fp((m_workingDir + "/" + m_filesToPurge.at(0)).toStdWString());

  // Refresh FileBrowser
  FileBrowser::refreshFolder(fp.getParentDir());
}
