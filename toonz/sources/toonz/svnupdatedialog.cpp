

#include "svnupdatedialog.h"

// Tnz6 includes
#include "tapp.h"
#include "versioncontrolwidget.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"

// Qt includes
#include <QWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QTextEdit>
#include <QCheckBox>
#include <QRegularExpression>
#include <QDir>
#include <QMainWindow>

//=============================================================================
// SVNUpdateDialog
//-----------------------------------------------------------------------------

SVNUpdateDialog::SVNUpdateDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files, int sceneIconsCount,
                                 bool isFolderOnly, bool updateToRevision,
                                 bool nonRecursive)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_updateToRevision(updateToRevision)
    , m_nonRecursive(nonRecursive)
    , m_sceneIconsCount(sceneIconsCount)
    , m_someSceneIsMissing(false) {
  // Set dialog properties
  setModal(false);
  setMinimumSize(300, 180);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Update"));

  // Create main container
  auto container  = new QWidget(this);
  auto mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // Create waiting layout
  auto hLayout   = new QHBoxLayout;
  m_waitingLabel = new QLabel(this);

  // Create waiting animation
  auto waitingMovie = new QMovie(":Resources/waiting.gif", QByteArray(), this);
  m_waitingLabel->setMovie(waitingMovie);
  waitingMovie->setCacheMode(QMovie::CacheAll);
  waitingMovie->start();

  m_textLabel = new QLabel(tr("Getting repository status..."), this);

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();
  mainLayout->addLayout(hLayout);

  // Create output text area
  m_output = new QTextEdit(this);
  m_output->setTextInteractionFlags(Qt::NoTextInteraction);
  m_output->setReadOnly(true);
  m_output->hide();
  mainLayout->addWidget(m_output);

  // Create conflict resolution widget
  m_conflictWidget = new ConflictWidget(this);
  m_conflictWidget->hide();
  mainLayout->addWidget(m_conflictWidget);

  // Create date/revision chooser widget
  m_dateChooserWidget = new DateChooserWidget(this);
  m_dateChooserWidget->hide();
  mainLayout->addWidget(m_dateChooserWidget);

  // Create "Get Scene Contents" checkbox if not folder only
  if (!isFolderOnly) {
    auto checkBoxLayout = new QHBoxLayout;
    checkBoxLayout->setContentsMargins(0, 0, 0, 0);

    m_updateSceneContentsCheckBox =
        new QCheckBox(tr("Get Scene Contents"), this);
    m_updateSceneContentsCheckBox->setChecked(false);
    m_updateSceneContentsCheckBox->hide();

    // Modern signal/slot connection
    connect(m_updateSceneContentsCheckBox, &QCheckBox::toggled, this,
            &SVNUpdateDialog::onUpdateSceneContentsToggled);

    checkBoxLayout->addStretch();
    checkBoxLayout->addWidget(m_updateSceneContentsCheckBox);
    checkBoxLayout->addStretch();

    mainLayout->addSpacing(10);
    mainLayout->addLayout(checkBoxLayout);
  }

  container->setLayout(mainLayout);

  // Add container to dialog
  beginHLayout();
  addWidget(container, false);
  endHLayout();

  // Create update button with appropriate action
  m_updateButton = new QPushButton(tr("Update"), this);
  m_updateButton->hide();
  if (m_updateToRevision) {
    connect(m_updateButton, &QPushButton::clicked, this,
            &SVNUpdateDialog::onUpdateToRevisionButtonClicked);
  } else {
    connect(m_updateButton, &QPushButton::clicked, this,
            &SVNUpdateDialog::onUpdateButtonClicked);
  }

  // Create close and cancel buttons
  m_closeButton = new QPushButton(tr("Close"), this);
  m_closeButton->setEnabled(false);
  connect(m_closeButton, &QPushButton::clicked, this, &SVNUpdateDialog::close);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  connect(m_cancelButton, &QPushButton::clicked, this,
          &SVNUpdateDialog::reject);

  addButtonBarWidget(m_updateButton, m_closeButton, m_cancelButton);

  // Connect SVN thread signals using modern syntax
  // Connect for svn errors (that may occur every time)
  connect(&m_thread, &VersionControlThread::error, this,
          &SVNUpdateDialog::onError);

  // Connect for status retrieval
  connect(&m_thread, &VersionControlThread::statusRetrieved, this,
          &SVNUpdateDialog::onStatusRetrieved);

  // Start getting SVN status
  m_thread.getSVNStatus(m_workingDir, m_files, true, false, true);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onStatusRetrieved(const QString &xmlResponse) {
  // Hide waiting animation
  m_waitingLabel->hide();

  // Parse SVN status from XML response
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  // Check which files need to be updated
  checkFiles();

  // Check if scene files (.tnz) are found
  bool hasTnzFiles =
      std::any_of(m_filesToUpdate.begin(), m_filesToUpdate.end(),
                  [](const QString &file) { return file.endsWith(".tnz"); });

  if (hasTnzFiles) {
    // Update text label with appropriate item count
    if (m_filesToUpdate.size() == 1) {
      m_textLabel->setText(
          tr("%1 items to update.")
              .arg(m_filesToUpdate.size() + m_sceneResources.size()));
    } else {
      m_textLabel->setText(tr("%1 items to update.")
                               .arg(m_filesToUpdate.size() +
                                    m_sceneResources.size() -
                                    m_sceneIconsCount));
    }

    // Show scene contents checkbox if no scenes are missing
    if (m_updateSceneContentsCheckBox && !m_someSceneIsMissing) {
      m_updateSceneContentsCheckBox->show();
    }

    m_updateButton->show();
    m_closeButton->hide();
    return;
  }

  // Handle update to specific revision
  if (m_updateToRevision) {
    if (!m_filesWithConflict.isEmpty()) {
      m_textLabel->setText(
          tr("Some items are currently modified in your working copy.\n"
             "Please commit or revert changes first."));
      m_textLabel->show();
      switchToCloseButton();
      return;
    }

    m_textLabel->setText(tr("Update to:"));
    m_textLabel->show();
    m_dateChooserWidget->show();
    m_updateButton->show();
    m_closeButton->hide();

    adjustSize();
  } else {
    // Handle conflicts in normal update
    if (!m_filesWithConflict.isEmpty()) {
      m_textLabel->setText(tr("Some conflict found. Select.."));
      m_textLabel->show();
      m_updateButton->setEnabled(false);
      m_updateButton->show();
      m_closeButton->hide();

      // Set files with conflict in conflict widget
      m_conflictWidget->setFiles(m_filesWithConflict);
      m_conflictWidget->show();
      adjustSize();

      // Connect conflict resolution signal
      connect(m_conflictWidget, &ConflictWidget::allConflictSetted, this,
              &SVNUpdateDialog::onConflictSetted);
    } else {
      // No conflicts, proceed with update
      updateFiles();
    }
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::checkFiles() {
  // Clear existing lists
  m_filesWithConflict.clear();
  m_filesToUpdate.clear();

  // Check each file's status to determine if it needs update or has conflicts
  for (const auto &status : m_status) {
    if (status.m_path == "." || status.m_path == "..") {
      continue;
    }

    // Check for conflicts
    if ((m_updateToRevision && status.m_item == "modified") ||
        (status.m_item == "modified" && status.m_repoStatus == "modified")) {
      m_filesWithConflict.prepend(status.m_path);
    }
    // Check for files that need update
    else if (status.m_item == "none" || status.m_item == "missing" ||
             status.m_repoStatus == "modified") {
      // Handle missing .tnz files and their icons
      if (status.m_path.endsWith(".tnz") &&
          (status.m_item == "missing" ||
           (status.m_item == "none" && status.m_repoStatus == "added"))) {
        TFilePath scenePath = TFilePath(m_workingDir.toStdWString()) +
                              status.m_path.toStdWString();
        TFilePath iconPath = ToonzScene::getIconPath(scenePath);
        QDir dir(m_workingDir);

#ifdef MACOSX
        m_filesToUpdate.append(dir.relativeFilePath(toQString(iconPath)));
#else
        m_filesToUpdate.append(
            dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif
        m_sceneIconsCount++;
        m_someSceneIsMissing = true;
      }

      // Add file to update list if it's in the requested files
      if (m_files.size() == 1 || m_files.contains(status.m_path)) {
        m_filesToUpdate.prepend(status.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::updateFiles() {
  // Check if there are any files to update
  if (m_filesToUpdate.isEmpty()) {
    m_textLabel->setText(tr("No items to update."));
    m_textLabel->show();
    switchToCloseButton();
    return;
  }

  // Adjust UI for update operation
  setMinimumSize(300, 200);

  if (m_updateSceneContentsCheckBox) {
    m_updateSceneContentsCheckBox->hide();
  }

  m_updateButton->setEnabled(false);
  m_waitingLabel->hide();
  m_textLabel->hide();
  m_output->show();

  // Prepare SVN update command arguments
  QStringList args = {"update"};
  args.append(m_filesToUpdate);
  args.append(m_sceneResources);

  if (m_nonRecursive) {
    args << "--non-recursive";
  }

  // Disconnect previous done signal and connect new ones
  disconnect(&m_thread, &VersionControlThread::done, this,
             &SVNUpdateDialog::onUpdateDone);

  connect(&m_thread, &VersionControlThread::outputRetrieved, this,
          &SVNUpdateDialog::addOutputText);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNUpdateDialog::onUpdateDone);

  // Execute SVN update command
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::switchToCloseButton() {
  // Hide update-related UI elements and show close button
  if (m_updateSceneContentsCheckBox) {
    m_updateSceneContentsCheckBox->hide();
  }

  m_cancelButton->hide();
  m_updateButton->hide();
  m_closeButton->show();
  m_closeButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::addOutputText(const QString &text) {
  // Remove "Updated to revision X." message using QRegularExpression
  QRegularExpression regExp("Updated to revision (\\d+)\\.");
  QString temp = text;
  temp.remove(regExp);

  // Split text by newlines and add to output
  QStringList split = temp.split(QRegularExpression("\\r\\n|\\n"));

  for (const auto &line : split) {
    if (!line.isEmpty()) {
      m_output->insertPlainText(line + "\n");
    }
  }

  // Scroll to bottom
  auto scrollBar = m_output->verticalScrollBar();
  scrollBar->setValue(scrollBar->maximum());
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateToRevisionButtonClicked() {
  // Hide UI elements for revision update
  m_textLabel->hide();
  m_cancelButton->hide();
  m_updateButton->hide();
  m_dateChooserWidget->hide();
  m_output->show();

  // Prepare SVN update command with specific revision
  QStringList args = {"update"};
  args.append(m_files);

  QString revisionString = m_dateChooserWidget->getRevisionString();
  args << "-r" << revisionString;

  // Connect output and done signals
  connect(&m_thread, &VersionControlThread::outputRetrieved, this,
          &SVNUpdateDialog::addOutputText);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNUpdateDialog::onUpdateDone);

  // Execute SVN update to specific revision
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateButtonClicked() {
  // Update conflicted files using "mine-full" option
  auto files = m_conflictWidget->getFilesWithOption(0);
  if (!files.isEmpty()) {
    m_waitingLabel->show();
    m_textLabel->setText(tr("Updating items..."));

    QStringList args = {"update"};
    args.append(files);
    args << "--accept" << "mine-full";

    // Disconnect previous done signal
    disconnect(&m_thread, &VersionControlThread::done, this,
               &SVNUpdateDialog::onUpdateToMineDone);

    // Connect for completion
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNUpdateDialog::onUpdateToMineDone);

    m_thread.executeCommand(m_workingDir, "svn", args);
  } else {
    onUpdateToMineDone();
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateToMineDone() {
  // Update conflicted files using "theirs-full" option
  auto files = m_conflictWidget->getFilesWithOption(1);
  if (!files.isEmpty()) {
    m_waitingLabel->show();
    m_textLabel->setText(tr("Updating to their items..."));

    QStringList args = {"update"};
    args.append(files);
    args << "--accept" << "theirs-full";

    // Disconnect previous done signal
    disconnect(&m_thread, &VersionControlThread::done, this,
               &SVNUpdateDialog::onConflictResolved);

    // Connect for completion
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNUpdateDialog::onConflictResolved);

    m_thread.executeCommand(m_workingDir, "svn", args);
  } else {
    onConflictResolved();
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onConflictResolved() {
  // Hide conflict widget and proceed with update
  m_conflictWidget->hide();
  updateFiles();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onError(const QString &errorString) {
  // Hide waiting animation and display error
  m_waitingLabel->hide();

  if (m_output->isVisible()) {
    addOutputText(errorString);
  } else {
    m_textLabel->setText(errorString);
    m_textLabel->show();
  }

  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateDone(const QString &text) {
  // Add final output text
  addOutputText(text);

  // Emit done signal with updated files
  emit done(m_filesToUpdate);

  // Switch to close button
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onConflictSetted() {
  // Enable update button after conflict resolution
  m_updateButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateSceneContentsToggled(bool checked) {
  // Clear or populate scene resources based on checkbox state
  m_sceneResources.clear();

  if (checked) {
    auto vc = VersionControl::instance();

    // Find .tnz files and get their scene contents
    for (const auto &fileName : m_filesToUpdate) {
      if (fileName.endsWith(".tnz")) {
        m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
      }
    }
  }

  // Update text label with new item count
  if (m_filesToUpdate.size() == 1) {
    m_textLabel->setText(
        tr("%1 items to update.")
            .arg(m_filesToUpdate.size() + m_sceneResources.size()));
  } else {
    m_textLabel->setText(tr("%1 items to update.")
                             .arg(m_filesToUpdate.size() +
                                  m_sceneResources.size() - m_sceneIconsCount));
  }
}
