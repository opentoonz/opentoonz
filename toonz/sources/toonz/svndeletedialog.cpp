

#include "svndeletedialog.h"

// Tnz6 includes
#include "tapp.h"
#include "tsystem.h"
#include "tfilepath.h"
#include "filebrowser.h"
#include "fileselection.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

// Qt includes
#include <QWidget>
#include <QLabel>
#include <QMovie>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QBoxLayout>
#include <QFormLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QTextStream>
#include <QDir>
#include <QMainWindow>

//=============================================================================
// SVNDeleteDialog
//-----------------------------------------------------------------------------

SVNDeleteDialog::SVNDeleteDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files, bool isFolder,
                                 int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_deleteSceneContentsCheckBox(0)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_targetTempFile(0)
    , m_isFolder(isFolder)
    , m_sceneIconAdded(sceneIconAdded) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Delete"));

  setMinimumSize(300, 180);

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

  if (m_isFolder) {
    QDir dir(m_workingDir + "/" + m_files.at(0));
    int filesCount = dir.entryList(QDir::Files | QDir::Dirs).count();
    if (filesCount > 0)
      m_textLabel->setText(tr("Delete folder that contains %1 items.")
                               .arg(filesCount - m_sceneIconAdded));
    else
      m_textLabel->setText(tr("Delete empty folder."));
  } else
    m_textLabel->setText(
        tr("Delete %1 items.").arg(m_files.size() - m_sceneIconAdded));

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

  m_commentTextEdit->hide();
  m_commentLabel->hide();

  formLayout->addRow(m_commentLabel, m_commentTextEdit);

  if (!m_isFolder) {
    m_deleteSceneContentsCheckBox = new QCheckBox(this);
    connect(m_deleteSceneContentsCheckBox, SIGNAL(toggled(bool)), this,
            SLOT(onDeleteSceneContentsToggled(bool)));
    m_deleteSceneContentsCheckBox->setChecked(false);
    m_deleteSceneContentsCheckBox->hide();
    m_deleteSceneContentsCheckBox->setText(tr("Delete Scene Contents"));

    formLayout->addRow("", m_deleteSceneContentsCheckBox);

    int fileSize = m_files.size();
    for (int i = 0; i < fileSize; i++) {
      if (m_files.at(i).endsWith(".tnz")) {
        m_deleteSceneContentsCheckBox->show();
        break;
      }
    }
  }

  m_keepLocalCopyCheckBox = new QCheckBox(tr(" Keep Local Copy"));
  m_keepLocalCopyCheckBox->setChecked(true);
  m_keepLocalCopyCheckBox->hide();

  formLayout->addRow("", m_keepLocalCopyCheckBox);

  mainLayout->addLayout(formLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_deleteLocalButton = new QPushButton();
  m_deleteLocalButton->setText(tr("Delete Local Copy "));
  connect(m_deleteLocalButton, SIGNAL(clicked()), this,
          SLOT(onDeleteLocalButtonClicked()));

  m_deleteServerButton = new QPushButton();
  m_deleteServerButton->setText(tr("Delete on Server "));
  connect(m_deleteServerButton, SIGNAL(clicked()), this,
          SLOT(onDeleteServerButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_deleteLocalButton, m_deleteServerButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::showEvent(QShowEvent *e) {
  if (!m_isFolder) {
    initTreeWidget();

    int height = 50;
    if (m_treeWidget->isVisible()) height += (m_files.size() * 50);

    setMinimumSize(300, std::min(height, 350));
  }
  QDialog::showEvent(e);
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::onError(const QString &errorString) {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = 0;
  }

  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::switchToCloseButton() {
  if (m_deleteSceneContentsCheckBox) m_deleteSceneContentsCheckBox->hide();
  m_commentTextEdit->hide();
  m_commentLabel->hide();
  m_waitingLabel->hide();
  m_treeWidget->hide();
  m_keepLocalCopyCheckBox->hide();
  m_deleteLocalButton->disconnect();
  m_deleteLocalButton->setText("Close");
  m_deleteLocalButton->show();
  m_cancelButton->hide();
  m_deleteServerButton->hide();
  connect(m_deleteLocalButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::onDeleteLocalButtonClicked() {
  // Delete files locally
  int fileCount = m_files.size();
  m_waitingLabel->show();
  m_textLabel->setText(
      tr("Deleting %1 items...")
          .arg(fileCount + m_sceneResources.size() - m_sceneIconAdded));
  m_textLabel->show();

  for (int i = 0; i < fileCount; i++) {
    TFilePath fp((m_workingDir + "/" + m_files.at(i)).toStdWString());
    TSystem::moveFileOrLevelToRecycleBin(fp);
  }

  int sceneResourcesCount = m_sceneResources.size();
  for (int i = 0; i < sceneResourcesCount; i++) {
    TFilePath fp(m_sceneResources.at(i).toStdWString());
    TSystem::moveFileOrLevelToRecycleBin(fp);
  }

  updateFileBrowser();

  emit done(m_files);

  m_waitingLabel->hide();
  m_textLabel->hide();

  close();
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::updateFileBrowser() {
  // Reset the selection
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  FileSelection *fileSelection = dynamic_cast<FileSelection *>(selection);
  if (fileSelection) fileSelection->selectNone();

  TFilePath fp((m_workingDir + "/" + m_files.at(0)).toStdWString());

  // Refresh FileBrowser
  FileBrowser::refreshFolder(fp.getParentDir());
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::onDeleteServerButtonClicked() {
  m_deleteLocalButton->hide();

  disconnect(m_deleteServerButton, SIGNAL(clicked()), this,
             SLOT(onDeleteServerButtonClicked()));
  m_deleteServerButton->setText(tr("Delete"));
  connect(m_deleteServerButton, SIGNAL(clicked()), this, SLOT(deleteFiles()));

  int height = 175;
  if (m_treeWidget->isVisible()) height += (m_files.size() * 50);
  setMinimumSize(300, std::min(height, 350));

  m_textLabel->setText(
      tr("You are deleting items also on repository. Are you sure ?"));
  m_commentLabel->show();
  m_commentTextEdit->show();
  m_keepLocalCopyCheckBox->show();
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::deleteFiles() {
  m_waitingLabel->show();
  m_textLabel->setText(
      tr("Deleting %1 items...")
          .arg(QString::number(m_files.size() + m_sceneResources.size() -
                               m_sceneIconAdded)));
  m_commentLabel->hide();
  m_commentTextEdit->hide();
  m_treeWidget->hide();
  m_keepLocalCopyCheckBox->hide();
  if (m_deleteSceneContentsCheckBox) m_deleteSceneContentsCheckBox->hide();
  m_deleteServerButton->hide();
  m_cancelButton->hide();

  QStringList args;
  args << "delete";

  if (m_keepLocalCopyCheckBox->isChecked()) args << "--keep-local";

  // Use a temporary file to store all the files list
  m_targetTempFile = new QFile(m_workingDir + "/" + "tempDeleteFile");
  if (m_targetTempFile->open(QFile::WriteOnly | QFile::Truncate)) {
    QTextStream out(m_targetTempFile);

    int filesCount = m_files.count();
    for (int i = 0; i < filesCount; i++) out << m_files.at(i) + "\n";

    int sceneResourcesCount = m_sceneResources.size();
    for (int i = 0; i < sceneResourcesCount; i++)
      out << m_sceneResources.at(i) + "\n";
  }
  m_targetTempFile->close();

  args << "--targets";
  args << "tempDeleteFile";

  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(commitDeletedFiles()));
  m_thread.executeCommand(m_workingDir, "svn", args, false);
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::commitDeletedFiles() {
  updateFileBrowser();
  QStringList args;
  args << "commit";
  args << "--targets";
  args << "tempDeleteFile";

  if (!m_commentTextEdit->toPlainText().isEmpty())
    args << QString("-m").append(m_commentTextEdit->toPlainText());
  else
    args << QString("-m").append(VersionControl::instance()->getUserName() +
                                 " delete files.");
  m_thread.disconnect(SIGNAL(done(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onCommitDone()));
  m_thread.executeCommand(m_workingDir, "svn", args);
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::onCommitDone() {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = 0;
  }

  emit done(m_files);
  close();
}

//-----------------------------------------------------------------------------

void SVNDeleteDialog::initTreeWidget() {
  int filesSize  = m_files.size();
  bool itemAdded = false;

  for (int i = 0; i < filesSize; i++) {
    QString fileName = m_files.at(i);
    TFilePath fp =
        TFilePath(m_workingDir.toStdWString()) + fileName.toStdWString();
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    QStringList linkedFiles;

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it) {
      QString fn = toQString((*it).withoutParentDir());

      if (m_files.contains(fn)) linkedFiles.append(fn);
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

void SVNDeleteDialog::onDeleteSceneContentsToggled(bool checked) {
  if (!checked)
    m_sceneResources.clear();
  else {
    VersionControl *vc = VersionControl::instance();

    int fileSize = m_files.count();
    for (int i = 0; i < fileSize; i++) {
      QString fileName = m_files.at(i);
      if (fileName.endsWith(".tnz")) {
        if (m_files.contains(fileName))
          m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
      }
    }
  }
  m_textLabel->setText(
      tr("Delete %1 items.")
          .arg(m_files.size() - m_sceneIconAdded + m_sceneResources.size()));
}
