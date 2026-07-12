

#include "svnrevertdialog.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tfilepath.h"
#include "tsystem.h"

// Qt includes
#include <QPushButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QMovie>
#include <QLabel>
#include <QDir>
#include <QRegularExpression>
#include <QMainWindow>
#include <memory>

//=============================================================================
// SVNRevertDialog
//-----------------------------------------------------------------------------

SVNRevertDialog::SVNRevertDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files, bool folderOnly,
                                 int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_folderOnly(folderOnly)
    , m_sceneIconAdded(sceneIconAdded) {
  // Set dialog properties
  setModal(false);
  setMinimumSize(300, 150);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Revert changes"));

  // Create main container
  auto container  = new QWidget(this);
  auto mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // Create waiting layout
  auto hLayout   = new QHBoxLayout;
  m_waitingLabel = new QLabel(this);

  // Use smart pointer for QMovie with proper parent ownership
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

  // Create tree widget
  m_treeWidget = new QTreeWidget(this);
  m_treeWidget->setStyleSheet("QTreeWidget { border: 1px solid gray; }");
  m_treeWidget->header()->hide();
  m_treeWidget->hide();
  mainLayout->addWidget(m_treeWidget);

  // Create revert scene contents checkbox if not folder only
  if (!m_folderOnly) {
    mainLayout->addSpacing(10);
    auto checkBoxLayout = new QHBoxLayout;
    checkBoxLayout->setContentsMargins(0, 0, 0, 0);

    m_revertSceneContentsCheckBox =
        new QCheckBox(tr("Revert Scene Contents"), this);
    m_revertSceneContentsCheckBox->setChecked(false);
    m_revertSceneContentsCheckBox->hide();

    // Modern signal/slot connection
    connect(m_revertSceneContentsCheckBox, &QCheckBox::toggled, this,
            &SVNRevertDialog::onRevertSceneContentsToggled);

    checkBoxLayout->addStretch();
    checkBoxLayout->addWidget(m_revertSceneContentsCheckBox);
    checkBoxLayout->addStretch();
    mainLayout->addLayout(checkBoxLayout);
  }

  container->setLayout(mainLayout);

  // Add container to dialog
  beginHLayout();
  addWidget(container, false);
  endHLayout();

  // Create buttons
  m_revertButton = new QPushButton(tr("Revert"), this);
  m_revertButton->setEnabled(false);
  connect(m_revertButton, &QPushButton::clicked, this,
          &SVNRevertDialog::onRevertButtonClicked);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  connect(m_cancelButton, &QPushButton::clicked, this,
          &SVNRevertDialog::reject);

  addButtonBarWidget(m_revertButton, m_cancelButton);

  // Connect SVN thread signals using modern syntax
  // Connect for svn errors (that may occur every time)
  connect(&m_thread, &VersionControlThread::error, this,
          &SVNRevertDialog::onError);

  // Connect for status retrieval
  connect(&m_thread, &VersionControlThread::statusRetrieved, this,
          &SVNRevertDialog::onStatusRetrieved);

  // Start getting SVN status
  m_thread.getSVNStatus(m_workingDir);
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onStatusRetrieved(const QString &xmlResponse) {
  // Parse SVN status from XML response
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  // Calculate initial height
  int height = 50;

  // Check which files need to be reverted
  checkFiles();

  // Show revert scene contents checkbox if .tnz files are found
  if (std::any_of(m_filesToRevert.begin(), m_filesToRevert.end(),
                  [](const QString &file) { return file.endsWith(".tnz"); })) {
    if (m_revertSceneContentsCheckBox) {
      m_revertSceneContentsCheckBox->show();
    }
  }

  // Initialize tree widget with linked files
  initTreeWidget();

  // Handle case where no items need to be reverted
  if (m_filesToRevert.isEmpty()) {
    m_textLabel->setText(tr("No items to revert."));
    switchToCloseButton();
  } else {
    // Adjust dialog size based on number of items
    if (m_treeWidget->isVisible()) {
      height += (m_filesToRevert.size() * 50);
    }

    setMinimumSize(300, std::min(height, 350));

    // Update status text
    const int itemCount = m_filesToRevert.size() - m_sceneIconAdded;
    m_textLabel->setText(
        tr("%1 items to revert.").arg(itemCount == 1 ? 1 : itemCount));

    // Hide waiting animation and enable revert button
    m_waitingLabel->hide();
    m_revertButton->setEnabled(true);

    // Disconnect status retrieved signal since we're done
    disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
               &SVNRevertDialog::onStatusRetrieved);
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::checkFiles() {
  // Filter files that need to be reverted based on SVN status
  for (const auto &status : m_status) {
    if (status.m_path == "." || status.m_path == "..") {
      continue;
    }

    // Check if file is not in normal or unversioned state
    if (status.m_item != "normal" && status.m_item != "unversioned") {
      if (m_folderOnly || m_files.contains(status.m_path)) {
        m_filesToRevert.append(status.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::initTreeWidget() {
  // Clear existing items
  m_treeWidget->clear();

  bool itemAdded = false;

  // Group linked files in tree widget
  for (const auto &fileName : m_filesToRevert) {
    TFilePath fp =
        TFilePath(m_workingDir.toStdWString()) + fileName.toStdWString();
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    QStringList linkedFiles;

    // Find linked files that are also in the revert list
    for (const auto &linkedFp : fpset) {
      QString fn = toQString(linkedFp.withoutParentDir());

      if (m_filesToRevert.contains(fn)) {
        linkedFiles.append(fn);
      }
    }

    // Create tree widget item for file with linked children
    if (!linkedFiles.isEmpty()) {
      itemAdded       = true;
      auto parentItem = new QTreeWidgetItem(m_treeWidget);
      parentItem->setText(0, fileName);
      parentItem->setFirstColumnSpanned(false);
      parentItem->setFlags(Qt::NoItemFlags);

      for (const auto &linkedFile : linkedFiles) {
        auto childItem = new QTreeWidgetItem(parentItem);
        childItem->setText(0, linkedFile);
        childItem->setFlags(Qt::NoItemFlags);
      }

      parentItem->setExpanded(true);
    }
  }

  // Show tree widget if items were added
  if (itemAdded) {
    m_treeWidget->show();
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::switchToCloseButton() {
  // Hide waiting animation and tree widget
  m_waitingLabel->hide();
  m_treeWidget->hide();

  // Disconnect revert button and change to close functionality
  m_revertButton->disconnect();
  m_revertButton->setText(tr("Close"));
  m_revertButton->setEnabled(true);

  // Hide cancel button
  m_cancelButton->hide();

  // Connect close functionality
  connect(m_revertButton, &QPushButton::clicked, this, &SVNRevertDialog::close);
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertButtonClicked() {
  // Disable revert button to prevent multiple clicks
  m_revertButton->setEnabled(false);

  // Start revert process
  revertFiles();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::revertFiles() {
  // Hide UI elements and show waiting animation
  m_treeWidget->hide();
  if (m_revertSceneContentsCheckBox) {
    m_revertSceneContentsCheckBox->hide();
  }
  m_waitingLabel->show();

  // Calculate total files to revert
  const int totalFilesToRevert =
      m_filesToRevert.size() + m_sceneResources.size();

  if (totalFilesToRevert > 0) {
    // Update status text
    const int displayCount = totalFilesToRevert - m_sceneIconAdded;
    m_textLabel->setText(
        tr("Reverting %1 items...").arg(displayCount == 1 ? 1 : displayCount));

    // Prepare SVN revert command arguments
    QStringList args = {"revert"};
    args.append(m_filesToRevert);
    args.append(m_sceneResources);

    // Connect done signal and execute command
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNRevertDialog::onRevertDone);

    m_thread.executeCommand(m_workingDir, "svn", args);
  } else {
    // No files to revert
    onRevertDone();
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertDone() {
  // Update status text
  m_textLabel->setText(tr("Revert done successfully."));

  // Emit done signal with reverted files
  emit done(m_filesToRevert);

  // Switch to close button
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onError(const QString &errorString) {
  // Display error and switch to close button
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertSceneContentsToggled(bool checked) {
  // Clear or populate scene resources based on checkbox state
  m_sceneResources.clear();

  if (checked) {
    auto vc = VersionControl::instance();

    // Find .tnz files and get their scene contents
    for (const auto &fileName : m_filesToRevert) {
      if (fileName.endsWith(".tnz")) {
        if (m_filesToRevert.contains(fileName)) {
          m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
        }
      }
    }
  }

  // Update status text with new item count
  const int totalItems   = m_filesToRevert.size() + m_sceneResources.size();
  const int displayCount = totalItems - m_sceneIconAdded;

  m_textLabel->setText(
      tr("%1 items to revert.").arg(displayCount == 1 ? 1 : displayCount));
}

//=============================================================================
// SVNRevertFrameRangeDialog
//-----------------------------------------------------------------------------

SVNRevertFrameRangeDialog::SVNRevertFrameRangeDialog(
    QWidget *parent, const QString &workingDir, const QString &file,
    const QString &tempFileName)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_file(file)
    , m_tempFileName(tempFileName) {
  // Set dialog properties
  setModal(false);
  setMinimumSize(300, 150);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Revert Frame Range changes"));

  beginVLayout();

  // Create waiting animation
  m_waitingLabel    = new QLabel(this);
  auto waitingMovie = new QMovie(":Resources/waiting.gif", QByteArray(), this);

  m_waitingLabel->setMovie(waitingMovie);
  waitingMovie->setCacheMode(QMovie::CacheAll);
  waitingMovie->start();
  m_waitingLabel->hide();

  // Create status label
  m_textLabel = new QLabel(tr("1 item to revert."), this);

  addWidgets(m_waitingLabel, m_textLabel);

  endVLayout();

  // Create buttons with modern signal connections
  m_revertButton = new QPushButton(tr("Revert"), this);
  connect(m_revertButton, &QPushButton::clicked, this,
          &SVNRevertFrameRangeDialog::onRevertButtonClicked);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  connect(m_cancelButton, &QPushButton::clicked, this,
          &SVNRevertFrameRangeDialog::reject);

  addButtonBarWidget(m_revertButton, m_cancelButton);
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::switchToCloseButton() {
  // Hide waiting animation
  m_waitingLabel->hide();

  // Change revert button to close functionality
  m_revertButton->disconnect();
  m_revertButton->setText(tr("Close"));
  m_revertButton->setEnabled(true);

  // Hide cancel button
  m_cancelButton->hide();

  // Connect close functionality
  connect(m_revertButton, &QPushButton::clicked, this,
          &SVNRevertFrameRangeDialog::close);
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onRevertButtonClicked() {
  // Disable revert button and start revert process
  m_revertButton->setEnabled(false);
  revertFiles();
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::revertFiles() {
  // Show waiting animation and update status
  m_waitingLabel->show();
  m_textLabel->setText(tr("Reverting 1 item..."));

  // Remove temporary files
  try {
    TFilePath pathToRemove(m_tempFileName.toStdWString());

    if (TSystem::doesExistFileOrLevel(pathToRemove)) {
      TSystem::removeFileOrLevel(pathToRemove);
    }

    // Handle TLV/TPL file pairs
    if (pathToRemove.getType() == "tlv") {
      pathToRemove = pathToRemove.withType("tpl");
      if (TSystem::doesExistFileOrLevel(pathToRemove)) {
        TSystem::removeFileOrLevel(pathToRemove);
      }
    }

    // Remove hook files if they exist
    TFilePath hookFilePath =
        pathToRemove.withName(pathToRemove.getName() + "_hooks")
            .withType("xml");

    if (TSystem::doesExistFileOrLevel(hookFilePath)) {
      TSystem::removeFileOrLevel(hookFilePath);
    }
  } catch (...) {
    m_textLabel->setText(tr("It is not possible to revert the file."));
    switchToCloseButton();
    return;
  }

  // Handle frame range files
  TFilePath path =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();

  if (path.getDots() == "..") {
    // Get directory and prepare regular expression for frame files
    TFilePath dir = path.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));

    // Escape level name for regex and create pattern
    QString levelName =
        QRegularExpression::escape(QString::fromStdWString(path.getWideName()));
    QString levelType = QString::fromStdString(path.getType());

    // Create regular expression for frame files
    QString pattern = QString("%1\\.[0-9]{1,4}\\.%2").arg(levelName, levelType);
    QRegularExpression regex(pattern);

    // Filter files matching the pattern
    QStringList list = qDir.entryList(QDir::Files);
    m_files          = list.filter(regex);

    // Connect SVN thread signals using modern syntax
    connect(&m_thread, &VersionControlThread::error, this,
            &SVNRevertFrameRangeDialog::onError);
    connect(&m_thread, &VersionControlThread::statusRetrieved, this,
            &SVNRevertFrameRangeDialog::onStatusRetrieved);

    // Get SVN status for filtered files
    m_thread.getSVNStatus(m_workingDir, m_files);
  } else {
    // Single file, emit done signal and switch to close button
    m_textLabel->setText(tr("Revert done successfully."));
    emit done({m_file});
    switchToCloseButton();
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onStatusRetrieved(const QString &xmlResponse) {
  // Parse SVN status
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  // Check which files need to be reverted
  checkFiles();

  const int fileToRevertCount = m_filesToRevert.size();

  if (fileToRevertCount == 0) {
    // No files to revert
    m_textLabel->setText(tr("Revert done successfully."));
    switchToCloseButton();
  } else {
    // Disconnect status signal and start revert process
    disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
               &SVNRevertFrameRangeDialog::onStatusRetrieved);

    m_textLabel->setText(
        tr("Reverting %1 items...").arg(QString::number(fileToRevertCount)));

    // Prepare SVN revert command
    QStringList args = {"revert"};
    args.append(m_filesToRevert);

    // Connect done signal and execute command
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNRevertFrameRangeDialog::onRevertDone);

    m_thread.executeCommand(m_workingDir, "svn", args);
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onError(const QString &errorString) {
  // Display error and switch to close button
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::checkFiles() {
  // Filter files that need to be reverted based on SVN status
  for (const auto &status : m_status) {
    if (status.m_item != "normal" && status.m_item != "unversioned") {
      if (m_files.contains(status.m_path)) {
        m_filesToRevert.append(status.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onRevertDone() {
  // Update status and emit done signal
  m_textLabel->setText(tr("Revert done successfully."));
  emit done(m_filesToRevert);
  switchToCloseButton();
}
