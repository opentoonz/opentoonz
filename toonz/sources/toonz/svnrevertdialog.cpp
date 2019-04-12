

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
#include <QRegExp>
#include <QMainWindow>

//=============================================================================
// SVNRevertDialog
//-----------------------------------------------------------------------------

SVNRevertDialog::SVNRevertDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files, bool folderOnly,
                                 int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_revertSceneContentsCheckBox(0)
    , m_folderOnly(folderOnly)
    , m_sceneIconAdded(sceneIconAdded) {
  setModal(false);
  setMinimumSize(300, 150);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Revert changes"));

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

  m_treeWidget = new QTreeWidget;
  m_treeWidget->setStyleSheet("QTreeWidget { border: 1px solid gray; }");
  m_treeWidget->header()->hide();
  m_treeWidget->hide();
  mainLayout->addWidget(m_treeWidget);

  if (!m_folderOnly) {
    mainLayout->addSpacing(10);
    QHBoxLayout *checkBoxLayout = new QHBoxLayout;
    checkBoxLayout->setMargin(0);
    m_revertSceneContentsCheckBox = new QCheckBox(this);
    connect(m_revertSceneContentsCheckBox, SIGNAL(toggled(bool)), this,
            SLOT(onRevertSceneContentsToggled(bool)));
    m_revertSceneContentsCheckBox->setChecked(false);
    m_revertSceneContentsCheckBox->hide();
    m_revertSceneContentsCheckBox->setText(tr("Revert Scene Contents"));
    checkBoxLayout->addStretch();
    checkBoxLayout->addWidget(m_revertSceneContentsCheckBox);
    checkBoxLayout->addStretch();
    mainLayout->addLayout(checkBoxLayout);
  }

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_revertButton = new QPushButton(tr("Revert"));
  m_revertButton->setEnabled(false);
  connect(m_revertButton, SIGNAL(clicked()), this,
          SLOT(onRevertButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_revertButton, m_cancelButton);

  // 0. Connect for svn errors (that may occurs everythings)
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));

  // 1. Getting status
  connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
          SLOT(onStatusRetrieved(const QString &)));
  m_thread.getSVNStatus(m_workingDir);
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status   = sr.getStatus();
  int height = 50;

  checkFiles();

  int fileSize = m_filesToRevert.size();
  for (int i = 0; i < fileSize; i++) {
    if (m_filesToRevert.at(i).endsWith(".tnz")) {
      if (m_revertSceneContentsCheckBox) m_revertSceneContentsCheckBox->show();
      break;
    }
  }

  initTreeWidget();

  if (m_filesToRevert.size() == 0) {
    QString msg = QString(tr("No items to revert."));
    m_textLabel->setText(msg);
    switchToCloseButton();
  } else {
    if (m_treeWidget->isVisible()) height += (m_filesToRevert.size() * 50);

    setMinimumSize(300, std::min(height, 350));

    QString msg = QString(tr("%1 items to revert."))
                      .arg(m_filesToRevert.size() == 1
                               ? 1
                               : m_filesToRevert.size() - m_sceneIconAdded);
    m_textLabel->setText(msg);

    m_waitingLabel->hide();
    m_revertButton->setEnabled(true);

    m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == "." || s.m_path == "..") continue;
    if (s.m_item != "normal" && s.m_item != "unversioned") {
      if (m_folderOnly || m_files.contains(s.m_path))
        m_filesToRevert.append(s.m_path);
    }
  }
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::initTreeWidget() {
  int filesSize = m_filesToRevert.size();

  bool itemAdded = false;

  for (int i = 0; i < filesSize; i++) {
    QString fileName = m_filesToRevert.at(i);
    TFilePath fp =
        TFilePath(m_workingDir.toStdWString()) + fileName.toStdWString();
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    QStringList linkedFiles;

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it) {
      QString fn = toQString((*it).withoutParentDir());

      if (m_filesToRevert.contains(fn)) linkedFiles.append(fn);
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

void SVNRevertDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_treeWidget->hide();
  m_revertButton->disconnect();
  m_revertButton->setText("Close");
  m_revertButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_revertButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertButtonClicked() {
  m_revertButton->setEnabled(false);
  revertFiles();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::revertFiles() {
  m_treeWidget->hide();
  if (m_revertSceneContentsCheckBox) m_revertSceneContentsCheckBox->hide();
  m_waitingLabel->show();

  int fileToRevertSize          = m_filesToRevert.size();
  int sceneResourceToRevertSize = m_sceneResources.size();
  int totalFilesToRevert        = fileToRevertSize + sceneResourceToRevertSize;
  if (totalFilesToRevert > 0) {
    m_textLabel->setText(tr("Reverting %1 items...")
                             .arg(totalFilesToRevert == 1
                                      ? 1
                                      : totalFilesToRevert - m_sceneIconAdded));

    QStringList args;
    args << "revert";

    for (int i = 0; i < fileToRevertSize; i++) args << m_filesToRevert.at(i);

    for (int i = 0; i < sceneResourceToRevertSize; i++)
      args << m_sceneResources.at(i);

    connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onRevertDone()));
    m_thread.executeCommand(m_workingDir, "svn", args);
  } else
    onRevertDone();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertDone() {
  m_textLabel->setText(tr("Revert done successfully."));

  QStringList files;
  for (int i = 0; i < m_filesToRevert.size(); i++)
    files.append(m_filesToRevert.at(i));
  emit done(files);

  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onError(const QString &errorString) {
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNRevertDialog::onRevertSceneContentsToggled(bool checked) {
  if (!checked)
    m_sceneResources.clear();
  else {
    VersionControl *vc = VersionControl::instance();

    int fileSize = m_filesToRevert.count();
    for (int i = 0; i < fileSize; i++) {
      QString fileName = m_filesToRevert.at(i);
      if (fileName.endsWith(".tnz")) {
        if (m_filesToRevert.contains(fileName))
          m_sceneResources.append(vc->getSceneContents(m_workingDir, fileName));
      }
    }
  }
  m_textLabel->setText(
      tr("%1 items to revert.")
          .arg(m_filesToRevert.size() + m_sceneResources.size() == 1
                   ? 1
                   : m_filesToRevert.size() + m_sceneResources.size() -
                         m_sceneIconAdded));
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
  setModal(false);
  setMinimumSize(300, 150);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Revert Frame Range changes"));

  beginVLayout();

  m_waitingLabel      = new QLabel;
  QMovie *waitingMove = new QMovie(":Resources/waiting.gif");
  waitingMove->setParent(this);

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();
  m_waitingLabel->hide();

  m_textLabel = new QLabel(tr("1 item to revert."));

  addWidgets(m_waitingLabel, m_textLabel);

  endVLayout();

  m_revertButton = new QPushButton(tr("Revert"));
  connect(m_revertButton, SIGNAL(clicked()), this,
          SLOT(onRevertButtonClicked()));

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(m_revertButton, m_cancelButton);
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_revertButton->disconnect();
  m_revertButton->setText("Close");
  m_revertButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_revertButton, SIGNAL(clicked()), this, SLOT(close()));
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onRevertButtonClicked() {
  m_revertButton->setEnabled(false);
  revertFiles();
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::revertFiles() {
  m_waitingLabel->show();
  m_textLabel->setText(tr("Reverting 1 item..."));

  try {
    TFilePath pathToRemove = TFilePath(m_tempFileName.toStdWString());
    if (TSystem::doesExistFileOrLevel(pathToRemove))
      TSystem::removeFileOrLevel(TFilePath(m_tempFileName.toStdWString()));
    if (pathToRemove.getType() == "tlv") {
      pathToRemove = pathToRemove.withType("tpl");
      if (TSystem::doesExistFileOrLevel(pathToRemove))
        TSystem::removeFileOrLevel(pathToRemove);
    }

    TFilePath hookFilePath =
        pathToRemove.withName(pathToRemove.getName() + "_hooks")
            .withType("xml");
    if (TSystem::doesExistFileOrLevel(hookFilePath))
      TSystem::removeFileOrLevel(hookFilePath);
  } catch (...) {
    m_textLabel->setText(tr("It is not possible to revert the file."));
    switchToCloseButton();
    return;
  }

  TFilePath path =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();

  if (path.getDots() == "..") {
    TFilePath dir = path.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));
    QString levelName =
        QRegExp::escape(QString::fromStdWString(path.getWideName()));
    QString levelType = QString::fromStdString(path.getType());
    QString exp(levelName + ".[0-9]{1,4}." + levelType);
    QRegExp regExp(exp);
    QStringList list = qDir.entryList(QDir::Files);
    m_files          = list.filter(regExp);

    // 0. Connect for svn errors (that may occurs everythings)
    connect(&m_thread, SIGNAL(error(const QString &)), this,
            SLOT(onError(const QString &)));

    // 1. Getting status
    connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
            SLOT(onStatusRetrieved(const QString &)));
    m_thread.getSVNStatus(m_workingDir, m_files);
  } else {
    m_textLabel->setText(tr("Revert done successfully."));

    QStringList files;
    files.append(m_file);
    emit done(files);

    switchToCloseButton();
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  checkFiles();

  int fileToRevertCount = m_filesToRevert.size();

  if (fileToRevertCount == 0) {
    m_textLabel->setText(tr("Revert done successfully."));
    switchToCloseButton();
  } else {
    m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));
    m_textLabel->setText(
        tr("Reverting %1 items...").arg(QString::number(fileToRevertCount)));

    QStringList args;
    args << "revert";
    for (int i = 0; i < fileToRevertCount; i++) args << m_filesToRevert.at(i);

    connect(&m_thread, SIGNAL(done(const QString &)), SLOT(onRevertDone()));
    m_thread.executeCommand(m_workingDir, "svn", args);
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onError(const QString &errorString) {
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::checkFiles() {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_item != "normal" && s.m_item != "unversioned") {
      if (m_files.contains(s.m_path)) m_filesToRevert.append(s.m_path);
    }
  }
}

//-----------------------------------------------------------------------------

void SVNRevertFrameRangeDialog::onRevertDone() {
  m_textLabel->setText(tr("Revert done successfully."));

  QStringList files;
  for (int i = 0; i < m_filesToRevert.size(); i++)
    files.append(m_filesToRevert.at(i));
  emit done(files);

  switchToCloseButton();
}
