

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
#include <QRegExp>
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
    , m_updateSceneContentsCheckBox(0)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_updateToRevision(updateToRevision)
    , m_nonRecursive(nonRecursive)
    , m_sceneIconsCount(sceneIconsCount)
    , m_someSceneIsMissing(false) {
  setModal(false);
  setMinimumSize(300, 180);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Update"));

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

  m_textLabel = new QLabel(tr("Getting repository status..."));

  hLayout->addStretch();
  hLayout->addWidget(m_waitingLabel);
  hLayout->addWidget(m_textLabel);
  hLayout->addStretch();

  mainLayout->addLayout(hLayout);

  m_output = new QTextEdit;
  m_output->setTextInteractionFlags(Qt::NoTextInteraction);
  m_output->setReadOnly(true);
  m_output->hide();

  mainLayout->addWidget(m_output);

  m_conflictWidget = new ConflictWidget;
  m_conflictWidget->hide();

  mainLayout->addWidget(m_conflictWidget);

  m_dateChooserWidget = new DateChooserWidget;
  m_dateChooserWidget->hide();

  mainLayout->addWidget(m_dateChooserWidget);

  if (!isFolderOnly) {
    QHBoxLayout *checkBoxLayout = new QHBoxLayout;
    checkBoxLayout->setMargin(0);
    m_updateSceneContentsCheckBox = new QCheckBox(this);
    m_updateSceneContentsCheckBox->setChecked(false);
    m_updateSceneContentsCheckBox->setText(tr("Get Scene Contents"));
    m_updateSceneContentsCheckBox->hide();
    connect(m_updateSceneContentsCheckBox, SIGNAL(toggled(bool)), this,
            SLOT(onUpdateSceneContentsToggled(bool)));

    checkBoxLayout->addStretch();
    checkBoxLayout->addWidget(m_updateSceneContentsCheckBox);
    checkBoxLayout->addStretch();

    mainLayout->addSpacing(10);
    mainLayout->addLayout(checkBoxLayout);
  }

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_updateButton = new QPushButton(tr("Update"));
  m_updateButton->hide();
  if (m_updateToRevision)
    connect(m_updateButton, SIGNAL(clicked()), this,
            SLOT(onUpdateToRevisionButtonClicked()));
  else
    connect(m_updateButton, SIGNAL(clicked()), this,
            SLOT(onUpdateButtonClicked()));

  m_closeButton = new QPushButton(tr("Close"));
  m_closeButton->setEnabled(false);
  connect(m_closeButton, SIGNAL(clicked()), this, SLOT(close()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_updateButton, m_closeButton, m_cancelButton);

  // 0. Connect for svn errors (that may occur every time)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir, m_files, true, false, true);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onStatusRetrieved(const QString &xmlResponse) {
  m_waitingLabel->hide();

  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  checkFiles();

  // Check if a scene files is found
  int fileSize = m_filesToUpdate.size();
  for (int i = 0; i < fileSize; i++) {
    if (m_filesToUpdate.at(i).endsWith(".tnz")) {
      if (fileSize == 1)
        m_textLabel->setText(
            tr("%1 items to update.")
                .arg(m_filesToUpdate.size() + m_sceneResources.size()));
      else
        m_textLabel->setText(tr("%1 items to update.")
                                 .arg(m_filesToUpdate.size() +
                                      m_sceneResources.size() -
                                      m_sceneIconsCount));
      if (m_updateSceneContentsCheckBox && !m_someSceneIsMissing)
        m_updateSceneContentsCheckBox->show();
      m_updateButton->show();
      m_closeButton->hide();
      return;
    }
  }

  if (m_updateToRevision) {
    if (m_filesWithConflict.count() > 0) {
      m_textLabel->setText(
          tr("Some items are currently modified in your working copy.\nPlease "
             "commit or revert changes first."));
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
    // Resolve first files with conflict
    if (m_filesWithConflict.count() > 0) {
      m_textLabel->setText(tr("Some conflict found. Select.."));
      m_textLabel->show();
      m_updateButton->setEnabled(false);
      m_updateButton->show();
      m_closeButton->hide();

      QStringList fileswithConflict;
      int fileswithConflictCount = m_filesWithConflict.size();
      for (int i = 0; i < fileswithConflictCount; i++)
        fileswithConflict.append(m_filesWithConflict.at(i));
      m_conflictWidget->setFiles(fileswithConflict);
      m_conflictWidget->show();
      adjustSize();
      connect(m_conflictWidget, SIGNAL(allConflictSetted()), this,
              SLOT(onConflictSetted()));
    } else
      updateFiles();
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if ((m_updateToRevision && s.m_item == "modified") ||
        (s.m_item == "modified" && s.m_repoStatus == "modified"))
      m_filesWithConflict.prepend(s.m_path);
    else if (s.m_item == "none" || s.m_item == "missing" ||
             s.m_repoStatus == "modified" || s.m_repoStatus == "modified") {
      if (s.m_path.endsWith(".tnz") &&
          (s.m_item == "missing" ||
           (s.m_item == "none" && s.m_repoStatus == "added"))) {
        TFilePath scenePath =
            TFilePath(m_workingDir.toStdWString()) + s.m_path.toStdWString();
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
      if (m_files.count() == 1 || m_files.contains(s.m_path))
        m_filesToUpdate.prepend(s.m_path);
    }
  }
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::updateFiles() {
  if (m_filesToUpdate.count() == 0) {
    QString msg = QString(tr("No items to update."));
    m_textLabel->setText(msg);
    m_textLabel->show();
    switchToCloseButton();
    return;
  }

  setMinimumSize(300, 200);

  if (m_updateSceneContentsCheckBox) m_updateSceneContentsCheckBox->hide();
  m_updateButton->setEnabled(false);
  m_waitingLabel->hide();
  m_textLabel->hide();
  m_output->show();

  QStringList args;
  args << "update";

  int filesCount = m_filesToUpdate.count();
  for (int i = 0; i < filesCount; i++) {
    QString file = m_filesToUpdate.at(i);
    if (!file.isEmpty()) args << file;
  }

  int resourceCount = m_sceneResources.size();
  for (int i = 0; i < resourceCount; i++) args << m_sceneResources.at(i);

  if (m_nonRecursive) args << "--non-recursive";

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(outputRetrieved(const QString &)),
          SLOT(addOutputText(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)),
          SLOT(onUpdateDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::switchToCloseButton() {
  if (m_updateSceneContentsCheckBox) m_updateSceneContentsCheckBox->hide();
  m_cancelButton->hide();
  m_updateButton->hide();
  m_closeButton->show();
  m_closeButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::addOutputText(const QString &text) {
  QRegExp regExp("Updated to revision (\\d+)\\.");
  QString temp = text;
  temp.remove(regExp);

  QStringList split = temp.split(QRegExp("\\r\\n|\\n"));

  for (int i = 0; i < split.size(); i++) {
    QString s = split.at(i);
    if (!s.isEmpty()) m_output->insertPlainText(s + "\n");
  }
  QScrollBar *scrollBar = m_output->verticalScrollBar();
  scrollBar->setValue(scrollBar->maximum());
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateToRevisionButtonClicked() {
  m_textLabel->hide();
  m_cancelButton->hide();
  m_updateButton->hide();
  m_dateChooserWidget->hide();
  m_output->show();

  QStringList args;
  args << "update";

  // Pay attention: I have to perform update of the whole selected files
  // (that could become updatable "looking in the past")
  int filesCount = m_files.count();
  for (int i = 0; i < filesCount; i++) {
    QString file = m_files.at(i);
    if (!file.isEmpty()) args << file;
  }
  QString revisionString = m_dateChooserWidget->getRevisionString();

  args << "-r" << revisionString;

  connect(&m_thread, SIGNAL(outputRetrieved(const QString &)),
          SLOT(addOutputText(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)),
          SLOT(onUpdateDone(const QString &)));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateButtonClicked() {
  // Update to mine
  QStringList files = m_conflictWidget->getFilesWithOption(0);
  if (files.size() > 0) {
    m_waitingLabel->show();
    m_textLabel->setText(tr("Updating items..."));
    QStringList args;
    args << "update";
    args.append(files);
    args << "--accept"
         << "mine-full";

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)),
            SLOT(onUpdateToMineDone()));
    m_thread.executeCommand(m_workingDir, "svn", args);
  } else
    onUpdateToMineDone();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateToMineDone() {
  // Update to theirs
  QStringList files = m_conflictWidget->getFilesWithOption(1);
  if (files.size() > 0) {
    m_waitingLabel->show();
    m_textLabel->setText(tr("Updating to their items..."));
    QStringList args;
    args << "update";
    args.append(files);
    args << "--accept"
         << "theirs-full";

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)),
            SLOT(onConflictResolved()));
    m_thread.executeCommand(m_workingDir, "svn", args);
  } else
    onConflictResolved();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onConflictResolved() {
  m_conflictWidget->hide();
  updateFiles();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onError(const QString &errorString) {
  m_waitingLabel->hide();
  if (m_output->isVisible())
    addOutputText(errorString);
  else {
    m_textLabel->setText(errorString);
    m_textLabel->show();
  }
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateDone(const QString &text) {
  addOutputText(text);

  QStringList files;
  for (int i = 0; i < m_filesToUpdate.size(); i++)
    files.append(m_filesToUpdate.at(i));
  emit done(files);

  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onConflictSetted() { m_updateButton->setEnabled(true); }

//-----------------------------------------------------------------------------

void SVNUpdateDialog::onUpdateSceneContentsToggled(bool checked) {
  if (!checked)
    m_sceneResources.clear();
  else {
    VersionControl *vc = VersionControl::instance();

    int fileSize = m_filesToUpdate.count();
    for (int i = 0; i < fileSize; i++) {
      QString fileName = m_filesToUpdate.at(i);
      if (fileName.endsWith(".tnz"))
        m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
    }
  }

  if (m_filesToUpdate.size() == 1)
    m_textLabel->setText(
        tr("%1 items to update.")
            .arg(m_filesToUpdate.size() + m_sceneResources.size()));
  else
    m_textLabel->setText(tr("%1 items to update.")
                             .arg(m_filesToUpdate.size() +
                                  m_sceneResources.size() - m_sceneIconsCount));
}
