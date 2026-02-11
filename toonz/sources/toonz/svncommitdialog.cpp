

#include "svncommitdialog.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tfilepath.h"
#include "tsystem.h"

// Qt includes
#include <QWidget>
#include <QLabel>
#include <QMovie>
#include <QFile>
#include <QBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QHostInfo>
#include <QTextStream>
#include <QMainWindow>
#include <QRegularExpression>

//=============================================================================
// SVNCommitDialog
//-----------------------------------------------------------------------------

SVNCommitDialog::SVNCommitDialog(QWidget *parent, const QString &workingDir,
                                 const QStringList &files, bool folderOnly,
                                 int sceneIconAdded)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_commitSceneContentsCheckBox(nullptr)
    , m_workingDir(workingDir)
    , m_files(files)
    , m_folderOnly(folderOnly)
    , m_targetTempFile(nullptr)
    , m_selectionCheckBox(nullptr)
    , m_selectionLabel(nullptr)
    , m_sceneIconAdded(sceneIconAdded)
    , m_folderAdded(0) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);
  setWindowTitle(tr("Version Control: Commit changes"));

  if (m_folderOnly)
    setMinimumSize(320, 320);
  else
    setMinimumSize(300, 180);

  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  m_treeWidget = new QTreeWidget;
  m_treeWidget->header()->hide();
  m_treeWidget->hide();

  if (m_folderOnly) {
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->setIconSize(QSize(21, 18));
  }
  m_treeWidget->setStyleSheet(
      QStringLiteral("QTreeWidget { border: 1px solid gray; }"));

  if (m_folderOnly) {
    mainLayout->addWidget(m_treeWidget);

    QHBoxLayout *belowTreeLayout = new QHBoxLayout;
    belowTreeLayout->setContentsMargins(0, 0, 0, 0);

    m_selectionCheckBox = new QCheckBox(tr("Select / Deselect All"), nullptr);
    connect(m_selectionCheckBox, &QCheckBox::clicked, this,
            &SVNCommitDialog::onSelectionCheckBoxClicked);

    m_selectionLabel = new QLabel;
    m_selectionLabel->setText(tr("0 Selected / 0 Total"));

    m_selectionCheckBox->hide();
    m_selectionLabel->hide();

    belowTreeLayout->addWidget(m_selectionCheckBox);
    belowTreeLayout->addStretch();
    belowTreeLayout->addWidget(m_selectionLabel);

    mainLayout->addLayout(belowTreeLayout);
  }

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel = new QLabel;
  QMovie *waitingMove =
      new QMovie(QStringLiteral(":Resources/waiting.gif"), QByteArray(), this);

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

  if (!m_folderOnly)
    mainLayout->addWidget(m_treeWidget);
  else
    mainLayout->addSpacing(10);

  QFormLayout *formLayout = new QFormLayout;
  formLayout->setLabelAlignment(Qt::AlignRight);
  formLayout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
  formLayout->setSpacing(10);
  formLayout->setContentsMargins(0, 0, 0, 0);

  m_commentTextEdit = new QPlainTextEdit;
  m_commentTextEdit->setMaximumHeight(50);
  m_commentTextEdit->hide();

  m_commentLabel = new QLabel(tr("Comment:"));
  m_commentLabel->setFixedWidth(55);
  m_commentLabel->hide();

  formLayout->addRow(m_commentLabel, m_commentTextEdit);

  if (!m_folderOnly) {
    m_commitSceneContentsCheckBox = new QCheckBox(this);
    connect(m_commitSceneContentsCheckBox, &QCheckBox::toggled, this,
            &SVNCommitDialog::onCommitSceneContentsToggled);
    m_commitSceneContentsCheckBox->setChecked(false);
    m_commitSceneContentsCheckBox->hide();
    m_commitSceneContentsCheckBox->setText(tr("Commit Scene Contents"));
    formLayout->addRow("", m_commitSceneContentsCheckBox);
  }

  mainLayout->addLayout(formLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  m_commitButton = new QPushButton(tr("Commit"));
  m_commitButton->setEnabled(false);
  connect(m_commitButton, &QPushButton::clicked, this,
          &SVNCommitDialog::onCommitButtonClicked);

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, &QPushButton::clicked, this,
          &SVNCommitDialog::reject);

  addButtonBarWidget(m_commitButton, m_cancelButton);

  // 0. Connect for svn errors (that may occur every time)
  connect(&m_thread, &VersionControlThread::error, this,
          &SVNCommitDialog::onError);

  // 1. Getting status (with show-updates enabled...)
  connect(&m_thread, &VersionControlThread::statusRetrieved, this,
          &SVNCommitDialog::onStatusRetrieved);
  m_thread.getSVNStatus(m_workingDir, m_files, true);
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onCommitButtonClicked() {
  if (m_folderOnly) {
    QMapIterator<QString, QTreeWidgetItem *> i(m_items);
    while (i.hasNext()) {
      i.next();
      QTreeWidgetItem *item = i.value();
      if (item && item->checkState(0) == Qt::Checked) {
        if (item->data(0, Qt::UserRole + 1).toBool())
          m_filesToAdd.append(item->data(0, Qt::UserRole).toString());
        else
          m_filesToCommit.append(item->data(0, Qt::UserRole).toString());
      }
    }
  } else
    m_commitSceneContentsCheckBox->hide();

  m_commitButton->setEnabled(false);

  // Check the status of the scene resource to see if some items need to be
  // added or can be directly committed
  if (!m_folderOnly && !m_sceneResourcesToCommit.empty()) {
    QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                        &SVNCommitDialog::onStatusRetrieved);
    connect(&m_thread, &VersionControlThread::statusRetrieved, this,
            &SVNCommitDialog::onResourcesStatusRetrieved);
    m_thread.getSVNStatus(m_workingDir, m_sceneResourcesToCommit, true);
    return;
  }

  // addFiles > getStatus > setProperties > commitFiles
  addFiles();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::addFiles() {
  m_waitingLabel->show();
  m_commentLabel->hide();
  m_commentTextEdit->hide();
  m_treeWidget->hide();
  if (m_folderOnly) {
    m_selectionCheckBox->hide();
    m_selectionLabel->hide();
  }
  m_textLabel->show();

  int fileToAddCount = m_filesToAdd.size();
  if (fileToAddCount + m_sceneResourcesToCommit.size() > 0) {
    m_textLabel->setText(
        tr("Adding %1 items...")
            .arg(QString::number(fileToAddCount - m_sceneIconAdded)));

    QStringList args;
    args << QStringLiteral("add");
    args << QStringLiteral("--parents");
    args << QStringLiteral("-N");  // Non recursive add

    // Use a temporary file to store all the files list
    m_targetTempFile = new QFile(m_workingDir + "/" + "tempAddFile");
    if (m_targetTempFile->open(QFile::WriteOnly | QFile::Truncate)) {
      QTextStream out(m_targetTempFile);

      for (int i = 0; i < fileToAddCount; i++) {
        QString path = m_filesToAdd.at(i);
        out << path + "\n";
        m_filesToCommit.push_back(path);
      }
      for (int i = 0; i < m_sceneResourcesToCommit.size(); i++)
        out << m_sceneResourcesToCommit.at(i) + "\n";
    }
    m_targetTempFile->close();

    args << QStringLiteral("--targets");
    args << QStringLiteral("tempAddFile");

    QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNCommitDialog::onAddDone);
    m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args);
  } else
    onAddDone();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::commitFiles() {
  m_waitingLabel->show();

  int fileToCommitCount = m_filesToCommit.size();
  if (fileToCommitCount > 0) {
    QStringList args;
    args << QStringLiteral("commit");
    args << QStringLiteral("-N");  // non recursive commit

    // Use a temporary file to store all the files list
    m_targetTempFile = new QFile(m_workingDir + "/" + "tempCommitFile");
    if (m_targetTempFile->open(QFile::WriteOnly | QFile::Truncate)) {
      QTextStream out(m_targetTempFile);

      for (int i = 0; i < fileToCommitCount; i++)
        out << m_filesToCommit.at(i) + "\n";

      int sceneResourcesCount = m_sceneResourcesToCommit.size();
      for (int i = 0; i < sceneResourcesCount; i++)
        out << m_sceneResourcesToCommit.at(i) + "\n";
    }

    m_targetTempFile->close();

    args << QStringLiteral("--targets");
    args << QStringLiteral("tempCommitFile");

    if (!m_commentTextEdit->toPlainText().isEmpty())
      args << QStringLiteral("-m").append(m_commentTextEdit->toPlainText());
    else
      args << QStringLiteral("-m").append(
          VersionControl::instance()->getUserName() +
          QStringLiteral(" commit changes."));
    QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                        nullptr);
    QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNCommitDialog::onCommitDone);
    m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args);
  } else
    onCommitDone();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::setPropertyFiles() {
  m_textLabel->setText(tr("Setting needs-lock property..."));

  int count = m_filesAdded.size();
  if (count > 0) {
    QStringList args;
    args << QStringLiteral("propset") << QStringLiteral("svn:needs-lock")
         << QStringLiteral("'*'");

    // Use a temporary file to store all the files list
    m_targetTempFile = new QFile(m_workingDir + "/" + "tempPropSetFile");
    if (m_targetTempFile->open(QFile::WriteOnly | QFile::Truncate)) {
      QTextStream out(m_targetTempFile);
      for (int i = 0; i < count; i++) out << m_filesAdded.at(i) + "\n";
    }
    m_targetTempFile->close();

    args << QStringLiteral("--targets");
    args << QStringLiteral("tempPropSetFile");

    QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                        nullptr);
    QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNCommitDialog::onSetPropertyDone);
    m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args);
  } else
    onSetPropertyDone();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_commitButton->disconnect();
  m_commitButton->setText(tr("Close"));
  m_commitButton->setEnabled(true);
  m_cancelButton->hide();
  connect(m_commitButton, &QPushButton::clicked, this, &SVNCommitDialog::close);
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onAddDone() {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = nullptr;
  }

  int fileToAddCount = m_filesToAdd.size();
  if (fileToAddCount + m_sceneResourcesToCommit.size() > 0) {
    m_textLabel->setText(tr("Getting repository status..."));

    QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                        &SVNCommitDialog::onStatusRetrieved);
    connect(&m_thread, &VersionControlThread::statusRetrieved, this,
            &SVNCommitDialog::onStatusRetrievedAfterAdd);

    QStringList oldFilesAndResources;
    oldFilesAndResources.append(m_files);
    for (int i = 0; i < m_filesToAdd.size(); i++) {
      QString fileToAdd = m_filesToAdd.at(i);
      if (!oldFilesAndResources.contains(fileToAdd))
        oldFilesAndResources.append(fileToAdd);
    }

    int resCount = m_sceneResourcesToCommit.size();
    for (int r = 0; r < resCount; r++) {
      QString path = m_sceneResourcesToCommit.at(r);
      int lastIndex =
          path.lastIndexOf(QRegularExpression(QStringLiteral("[\\\\/]")));

      while (lastIndex != -1) {
        path = path.left(lastIndex);
        if (!oldFilesAndResources.contains(path) && path != QLatin1String(".."))
          oldFilesAndResources.append(path);
        lastIndex =
            path.lastIndexOf(QRegularExpression(QStringLiteral("[\\\\/]")));
      }
    }

    // Add also sceneIcons folder
    for (int x = 0; x < m_filesToAdd.size(); x++) {
      QString s = m_filesToAdd.at(x);
      if (s.contains(QLatin1String("sceneIcons")) &&
          s.endsWith(QLatin1String(".png"))) {
        int lastIndex =
            s.lastIndexOf(QRegularExpression(QStringLiteral("[\\\\/]")));
        while (lastIndex != -1) {
          s = s.left(lastIndex);
          if (!oldFilesAndResources.contains(s) && s != QLatin1String(".."))
            oldFilesAndResources.append(s);
          lastIndex =
              s.lastIndexOf(QRegularExpression(QStringLiteral("[\\\\/]")));
        }
      }
    }

    m_thread.getSVNStatus(m_workingDir, oldFilesAndResources, true);
  } else {
    int fileToCommitCount = m_filesToCommit.size();
    if (fileToCommitCount > 0)
      m_textLabel->setText(
          tr("Committing %1 items...")
              .arg(QString::number(fileToCommitCount - m_sceneIconAdded)));
    // Then we commit changes (add, remove, modified files)
    commitFiles();
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onCommitDone() {
  m_waitingLabel->hide();
  m_textLabel->setText(tr("Commit completed successfully."));

  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = nullptr;
  }

  QStringList files;
  for (int i = 0; i < m_filesToCommit.size(); i++)
    files.append(m_filesToCommit.at(i));

  emit done(files);
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onSetPropertyDone() {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = nullptr;
  }

  if (m_filesToCommit.size() + m_sceneResourcesToCommit.size() > 0) {
    int commitCount = m_filesToCommit.size() - m_sceneIconAdded - m_folderAdded;
    for (int i = 0; i < m_sceneResourcesToCommit.size(); i++) {
      if (m_filesToCommit.contains(m_sceneResourcesToCommit.at(i)))
        commitCount--;
    }
    m_textLabel->setText(
        tr("Committing %1 items...").arg(QString::number(commitCount)));
  }

  // Then we commit changes (add, remove, modified files)
  commitFiles();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onResourcesStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status = sr.getStatus();

  QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                      nullptr);

  checkFiles(true);

  // addFiles > getStatus > setProperties > commitFiles
  addFiles();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onStatusRetrieved(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  m_status   = sr.getStatus();
  int height = 160;

  QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                      nullptr);

  checkFiles(m_folderOnly);

  if (!m_folderOnly) {
    // Add, if necessary the scene icons: only when the folder "sceneIcons" is
    // unversioned the status doesn't contain the scene icon information, so we
    // add here manually...
    if (m_sceneIconAdded != 0) {
      for (int i = 0; i < m_files.size(); i++) {
        QString s = m_files.at(i);
        if (s.contains(QLatin1String("sceneIcons")) &&
            s.endsWith(QLatin1String(".png")) && !m_filesToAdd.contains(s))
          m_filesToAdd.append(s);
      }
    }

    m_filesToPut.append(m_filesToAdd);
    m_filesToPut.append(m_filesToCommit);

    int filesToPutCount = m_filesToPut.size();
    if (filesToPutCount == 0) {
      m_textLabel->setText(tr("No items to commit."));
      switchToCloseButton();
    } else {
      m_textLabel->setText(
          tr("%1 items to commit.").arg(filesToPutCount - m_sceneIconAdded));

      initTreeWidget();

      for (int i = 0; i < filesToPutCount; i++) {
        if (m_filesToPut.at(i).endsWith(QLatin1String(".tnz"))) {
          m_commitSceneContentsCheckBox->show();
          break;
        }
      }

      if (m_treeWidget->isVisible()) height += (filesToPutCount * 25);

      setMinimumSize(350, std::min(height, 350));

      m_waitingLabel->hide();
      m_commentLabel->show();
      m_commentTextEdit->show();

      m_commitButton->setEnabled(true);
    }
    return;
  }

  if (m_items.isEmpty()) {
    m_textLabel->setText(tr("No items to commit."));

    switchToCloseButton();
  } else {
    if (m_folderOnly) {
      updateTreeSelectionLabel();
      connect(m_treeWidget, &QTreeWidget::itemChanged, this,
              &SVNCommitDialog::onItemChanged);
      m_treeWidget->show();
      m_selectionCheckBox->show();
      m_selectionLabel->show();
    }

    if (m_treeWidget->isVisible()) height += (m_items.size() * 25);

    setMinimumSize(350, std::min(height, 350));

    m_waitingLabel->hide();
    m_textLabel->hide();
    m_commentLabel->show();
    m_commentTextEdit->show();

    if (!m_folderOnly) m_commitButton->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onStatusRetrievedAfterAdd(const QString &xmlResponse) {
  SVNStatusReader sr(xmlResponse);
  QList<SVNStatus> status = sr.getStatus();

  QObject::disconnect(&m_thread, &VersionControlThread::statusRetrieved, this,
                      nullptr);

  int statusCount = status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = status.at(i);
    if (s.m_item == QLatin1String("added")) {
      // Note: the svn:needs-lock property (or any property) cannot be set on
      // folders Remove folder from files to Add
      QFileInfo fi(s.m_path);
      if (fi.isAbsolute()) {
        if (fi.isFile()) m_filesAdded.append(s.m_path);
      } else {
        fi = QFileInfo(m_workingDir + "/" + s.m_path);
        if (fi.isFile()) {
          if (!m_filesAdded.contains(s.m_path)) m_filesAdded.append(s.m_path);
        } else {
          if (!m_filesToCommit.contains(s.m_path)) {
            m_filesToCommit.append(s.m_path);
            m_folderAdded++;
          }
        }
      }
    }
  }

  setPropertyFiles();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::checkFiles(bool isExternalFiles) {
  int statusCount = m_status.size();
  for (int i = 0; i < statusCount; i++) {
    SVNStatus s = m_status.at(i);
    if (s.m_path == QLatin1String(".") || s.m_path == QLatin1String(".."))
      continue;
    if (s.m_item == QLatin1String("unversioned")) {
      if (isExternalFiles || m_files.contains(s.m_path)) {
        if (m_folderOnly)
          addUnversionedItem(s.m_path);
        else
          m_filesToAdd.append(s.m_path);
      }
    } else if (s.m_item != QLatin1String("normal") &&
               s.m_item != QLatin1String("incomplete") &&
               s.m_item != QLatin1String("missing")) {
      if (isExternalFiles || m_files.contains(s.m_path)) {
        if (m_folderOnly)
          addModifiedItem(s.m_path);
        else
          m_filesToCommit.append(s.m_path);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::addUnversionedItem(const QString &relativePath) {
  // Split the string to recreate the hierarchy
  QStringList list =
      relativePath.split(QRegularExpression(QStringLiteral("[\\\\/]")));

  QTreeWidgetItem *parent = nullptr;
  QString tempString;

  QIcon folderIcon = QIcon(createQIcon("folder_vc", true));

  int levelCount = list.count();
  for (int i = 0; i < levelCount; i++) {
    i == 0 ? tempString.append(list.at(i))
           : tempString.append('/' + list.at(i));
    if (m_items.contains(tempString))
      parent = m_items.value(tempString);
    else {
      QFileInfo fi(m_workingDir + "/" + tempString);
      // Don't add and show hidden files
      if (fi.isHidden()) continue;

      QTreeWidgetItem *item;
      if (parent)
        item = new QTreeWidgetItem(parent, QStringList(list.at(i)));
      else
        item = new QTreeWidgetItem(m_treeWidget, QStringList(list.at(i)));

      if (fi.isDir()) item->setIcon(0, folderIcon);
      if ((i + 1) == levelCount) {
        item->setCheckState(0, m_folderOnly ? Qt::Unchecked : Qt::Checked);
        item->setData(0, Qt::UserRole, tempString);
        item->setData(0, Qt::UserRole + 1, true);
      }
      m_treeWidget->addTopLevelItem(item);

      m_items[tempString] = item;
      parent              = item;
    }
  }

  // Add unversioned dir entries (recursively)

  QFileInfo fi(m_workingDir + "/" + relativePath);
  if (fi.isDir() && !fi.isHidden()) {
    QDir dir(m_workingDir + "/" + relativePath);
    addUnversionedFolders(dir, relativePath);
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::addUnversionedFolders(const QDir &dir,
                                            const QString &relativePath) {
  QStringList entries =
      dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  int count = entries.count();

  QIcon folderIcon = QIcon(createQIcon("folder_vc", true));

  for (int i = 0; i < count; i++) {
    QString entry = entries.at(i);
    QFileInfo fi(m_workingDir + "/" + relativePath + "/" + entry);

    // Don't add hidden files and folders
    if (fi.isHidden()) continue;

    if (fi.isDir()) {
      QTreeWidgetItem *parent = m_items[relativePath];
      QTreeWidgetItem *item   = new QTreeWidgetItem(parent, QStringList(entry));
      item->setCheckState(0, m_folderOnly ? Qt::Unchecked : Qt::Checked);
      item->setData(0, Qt::UserRole, relativePath + "/" + entry);
      item->setData(0, Qt::UserRole + 1, true);
      item->setIcon(0, folderIcon);
      if (parent) parent->insertChild(0, item);

      m_items[relativePath + "/" + entry] = item;

      QDir dir2(m_workingDir + "/" + relativePath + "/" + entry);
      addUnversionedFolders(dir2, relativePath + "/" + entry);
    }

    QStringList list =
        (entries.at(i)).split(QRegularExpression(QStringLiteral("[\\\\/]")));

    QTreeWidgetItem *parent = m_items[relativePath];
    QString tempString      = relativePath;

    int levelCount = list.count();
    for (int l = 0; l < levelCount; l++) {
      tempString.append('/' + list.at(l));
      if (m_items.contains(tempString))
        parent = m_items.value(tempString);
      else {
        QTreeWidgetItem *item;
        if (parent)
          item = new QTreeWidgetItem(parent, QStringList(list.at(l)));
        else
          item = new QTreeWidgetItem(m_treeWidget, QStringList(list.at(l)));
        QFileInfo fi(m_workingDir + "/" + tempString);
        if (fi.isDir()) item->setIcon(0, folderIcon);
        item->setCheckState(0, m_folderOnly ? Qt::Unchecked : Qt::Checked);
        item->setData(0, Qt::UserRole, tempString);
        item->setData(0, Qt::UserRole + 1, true);
        if (parent) parent->insertChild(0, item);

        m_items[tempString] = item;
        parent              = item;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::addModifiedItem(const QString &relativePath) {
  // Split the string to recreate the hierarchy
  QStringList list =
      relativePath.split(QRegularExpression(QStringLiteral("[\\\\/]")));

  QTreeWidgetItem *parent = nullptr;
  QString tempString;

  QBrush brush(Qt::red);
  QIcon folderIcon = QIcon(createQIcon("folder_vc", true));

  int levelCount = list.count();
  for (int i = 0; i < levelCount; i++) {
    i == 0 ? tempString.append(list.at(i))
           : tempString.append('/' + list.at(i));
    if (m_items.contains(tempString))
      parent = m_items.value(tempString);
    else {
      QTreeWidgetItem *item;
      if (parent)
        item = new QTreeWidgetItem(parent, QStringList(list.at(i)));
      else
        item = new QTreeWidgetItem(m_treeWidget, QStringList(list.at(i)));
      QFileInfo fi(m_workingDir + "/" + tempString);
      if (fi.isDir()) item->setIcon(0, folderIcon);
      item->setCheckState(0, Qt::Checked);
      item->setForeground(0, brush);
      item->setData(0, Qt::UserRole, tempString);
      item->setData(0, Qt::UserRole + 1, false);
      m_treeWidget->addTopLevelItem(item);

      m_items[tempString] = item;
      parent              = item;
    }
  }
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onItemChanged(QTreeWidgetItem *item) {
  disconnect(m_treeWidget, &QTreeWidget::itemChanged, this,
             &SVNCommitDialog::onItemChanged);

  Qt::CheckState state = item->checkState(0);

  QList<QTreeWidgetItem *> list = m_treeWidget->selectedItems();
  for (int i = 0; i < list.count(); i++) {
    QTreeWidgetItem *selItem = list.at(i);
    if (selItem != item && !selItem->data(0, Qt::CheckStateRole).isNull())
      selItem->setCheckState(0, state);
  }

  connect(m_treeWidget, &QTreeWidget::itemChanged, this,
          &SVNCommitDialog::onItemChanged);

  updateTreeSelectionLabel();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onSelectionCheckBoxClicked(bool checked) {
  QMapIterator<QString, QTreeWidgetItem *> i(m_items);

  int totalCount = 0;

  while (i.hasNext()) {
    i.next();
    QTreeWidgetItem *item = i.value();
    if (item && !item->data(0, Qt::CheckStateRole).isNull()) {
      totalCount++;
      item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    }
  }

  m_selectionLabel->setText(tr("%1 Selected / %2 Total")
                                .arg(checked ? totalCount : 0)
                                .arg(totalCount));
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::updateTreeSelectionLabel() {
  int totalCount    = 0;
  int selectedCount = 0;
  QMapIterator<QString, QTreeWidgetItem *> i(m_items);
  while (i.hasNext()) {
    i.next();
    QTreeWidgetItem *item = i.value();
    if (item && !item->data(0, Qt::CheckStateRole).isNull()) {
      totalCount++;
      if (item->checkState(0) == Qt::Checked) selectedCount++;
    }
  }
  m_selectionLabel->setText(
      tr("%1 Selected / %2 Total").arg(selectedCount).arg(totalCount));
  m_commitButton->setEnabled(selectedCount != 0);
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::onError(const QString &errorString) {
  if (m_targetTempFile) {
    QFile::remove(m_targetTempFile->fileName());
    delete m_targetTempFile;
    m_targetTempFile = nullptr;
  }

  m_textLabel->show();
  m_textLabel->setText(errorString);
  switchToCloseButton();
  update();
}

//-----------------------------------------------------------------------------

void SVNCommitDialog::initTreeWidget() {
  int filesSize  = m_filesToPut.size();
  bool itemAdded = false;

  for (int i = 0; i < filesSize; i++) {
    QString fileName = m_filesToPut.at(i);
    TFilePath fp =
        TFilePath(m_workingDir.toStdWString()) + fileName.toStdWString();
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    QStringList linkedFiles;

    for (const TFilePath &path : fpset) {
      QString fn = toQString(path.withoutParentDir());

      if (m_filesToPut.contains(fn)) linkedFiles.append(fn);
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

void SVNCommitDialog::onCommitSceneContentsToggled(bool checked) {
  if (!checked) {
    m_sceneResourcesToCommit.clear();
  } else {
    VersionControl *vc = VersionControl::instance();

    int fileSize = m_filesToPut.count();
    for (int i = 0; i < fileSize; i++) {
      QString fileName = m_filesToPut.at(i);
      if (fileName.endsWith(QLatin1String(".tnz"))) {
        QStringList sceneContents =
            vc->getSceneContents(m_workingDir, fileName);
        QDir workingDir(m_workingDir);
        for (int s = 0; s < sceneContents.size(); s++) {
          QString resource     = sceneContents.at(s);
          QString relativePath = workingDir.relativeFilePath(resource);
          m_sceneResourcesToCommit.append(relativePath);
        }
      }
    }
  }
  m_textLabel->setText(tr("%1 items to commit.")
                           .arg(m_filesToPut.size() +
                                m_sceneResourcesToCommit.size() -
                                m_sceneIconAdded));
}

//=============================================================================
// SVNCommitFrameRangeDialog
//-----------------------------------------------------------------------------

SVNCommitFrameRangeDialog::SVNCommitFrameRangeDialog(QWidget *parent,
                                                     const QString &workingDir,
                                                     const QString &file)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_workingDir(workingDir)
    , m_file(file)
    , m_hasError(false)
    , m_isOVLLevel(false)
    , m_scene(nullptr)
    , m_level(nullptr) {
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose, true);

  setWindowTitle(tr("Version Control: Commit Frame Range"));

  setMinimumSize(350, 150);
  QWidget *container = new QWidget;

  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setAlignment(Qt::AlignHCenter);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  QHBoxLayout *hLayout = new QHBoxLayout;

  m_waitingLabel = new QLabel;
  QMovie *waitingMove =
      new QMovie(QStringLiteral(":Resources/waiting.gif"), QByteArray(), this);

  m_waitingLabel->hide();

  m_waitingLabel->setMovie(waitingMove);
  waitingMove->setCacheMode(QMovie::CacheAll);
  waitingMove->start();

  m_textLabel = new QLabel;
  m_textLabel->setText(tr("Note: the file will be updated too."));

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

  commentLayout->addWidget(m_commentLabel, Qt::AlignTop);
  commentLayout->addWidget(m_commentTextEdit);

  mainLayout->addLayout(commentLayout);

  container->setLayout(mainLayout);

  beginHLayout();
  addWidget(container, false);
  endHLayout();

  // Check if the hook file still exists
  TFilePath path =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();
  TFilePath hookFile = TXshSimpleLevel::getExistingHookFile(path);

  if (!hookFile.isEmpty())
    m_hookFileName =
        QString::fromStdWString(hookFile.withoutParentDir().getWideString());

  m_isOVLLevel = path.getDots() == "..";

  m_commitButton = new QPushButton(tr("Commit"));
  connect(m_commitButton, &QPushButton::clicked, this,
          &SVNCommitFrameRangeDialog::onCommitButtonClicked);

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, &QPushButton::clicked, this,
          &SVNCommitFrameRangeDialog::reject);

  addButtonBarWidget(m_commitButton, m_cancelButton);

  // 0. Connect for svn errors (that may occur)
  connect(&m_thread, &VersionControlThread::error, this,
          &SVNCommitFrameRangeDialog::onError);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onCommitDone() {
  m_waitingLabel->hide();
  m_textLabel->setText(tr("Commit completed successfully."));

  QStringList files;
  files.append(m_file);
  emit done(files);
  switchToCloseButton();
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onUpdateDone() {
  // Load Level
  m_scene = new ToonzScene();
  TFilePath levelPath =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();
  m_level = m_scene->loadLevel(levelPath);

  m_textLabel->setText(tr("Locking file..."));

  if (m_isOVLLevel && m_hookFileName.isEmpty()) {
    onLockDone();
    return;
  }

  // Step 2: Lock
  QStringList args;
  args << QStringLiteral("lock");
  if (!m_isOVLLevel) args << m_file;
  if (!m_hookFileName.isEmpty()) args << m_hookFileName;

  QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNCommitFrameRangeDialog::onLockDone);
  m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, false);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onLockDone() {
  m_textLabel->setText(tr("Getting frame range edit information..."));

  if (m_isOVLLevel) {
    onPropSetDone();
    return;
  }

  // Step 3: propget
  QStringList args;
  args << QStringLiteral("proplist");
  args << m_file;
  args << QStringLiteral("--xml");
  args << QStringLiteral("-v");

  QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNCommitFrameRangeDialog::onPropGetDone);
  m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, true);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onError(const QString &text) {
  m_textLabel->setText(text);
  switchToCloseButton();
  update();
  m_hasError = true;
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onPropGetDone(const QString &xmlResponse) {
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
      if (i != 0) partialLockString.append(';');
      partialLockString.append(lockInfo.m_userName + '@' + lockInfo.m_hostName +
                               ':' + QString::number(lockInfo.m_from) + ':' +
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
    args << QStringLiteral("propdel");
    args << QStringLiteral("partial-lock");
    args << m_file;

    QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNCommitFrameRangeDialog::onPropSetDone);
    m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, true);
  }
  // Set the partial-lock property
  else {
    // Step 4: propset
    QStringList args;
    args << QStringLiteral("propset");
    args << QStringLiteral("partial-lock");
    args << partialLockString;
    args << m_file;

    QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
    connect(&m_thread, &VersionControlThread::done, this,
            &SVNCommitFrameRangeDialog::onPropSetDone);
    m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, true);
  }
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onPropSetDone() {
  m_textLabel->setText(tr("Committing changes..."));

  // Merge Level
  TFilePath levelPath =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();

  // OVL level could only merge the temporary hook file (if exists)
  if (m_isOVLLevel) {
    QDir dir(m_workingDir);
    dir.setNameFilters(QStringList(QStringLiteral("*.xml")));
    QStringList list = dir.entryList(QDir::Files | QDir::Hidden);
    int listCount    = list.size();

    if (listCount > 0) {
      QString prefix = QString::fromStdWString(levelPath.getWideName()) + '_' +
                       VersionControl::instance()->getUserName() + '_' +
                       TSystem::getHostName();

      QString tempFile;
      QString hookFileName;
      for (int i = 0; i < listCount; i++) {
        QString str = list.at(i);
        if (str.startsWith(prefix)) {
          hookFileName = str;
          tempFile     = QString(hookFileName);
          tempFile.remove(QStringLiteral("_hooks"));
          break;
        }
      }

      QStringList temp = tempFile.split('_');

      if (temp.size() >= 4) {
        QString frameRangeString = temp.at(3);
        frameRangeString = frameRangeString.left(frameRangeString.indexOf('.'));

        QStringList rangeString = frameRangeString.split('-');
        if (rangeString.size() == 2) {
          TFilePath hookFile =
              levelPath.getParentDir() + hookFileName.toStdWString();

          // Merge and then delete temporary hook file
          if (TFileStatus(hookFile).doesExist()) {
            m_level->getSimpleLevel()->mergeTemporaryHookFile(
                rangeString.at(0).toInt() - 1, rangeString.at(1).toInt() - 1,
                hookFile);
            TSystem::removeFileOrLevel(hookFile);
          }

          m_level->setDirtyFlag(true);
          m_level->save();
          delete m_scene;
          m_scene = nullptr;
        }
      }
    }
  } else {
    m_level->getSimpleLevel()->setEditableRange(
        m_myInfo.m_from - 1, m_myInfo.m_to - 1,
        m_myInfo.m_userName.toStdWString());
    m_level->setDirtyFlag(true);
    m_level->save();
    delete m_scene;
    m_scene = nullptr;

    // Delete temporary file
    QString tempFileName = QString::fromStdWString(levelPath.getWideName()) +
                           '_' + m_myInfo.m_userName + '_' +
                           m_myInfo.m_hostName + '_' +
                           QString::number(m_myInfo.m_from) + '-' +
                           QString::number(m_myInfo.m_to) + '.' +
                           QString::fromStdString(levelPath.getType());

    TFilePath pathToRemove =
        levelPath.getParentDir() + tempFileName.toStdWString();
    if (TSystem::doesExistFileOrLevel(pathToRemove))
      TSystem::removeFileOrLevel(pathToRemove);
    if (pathToRemove.getType() == "tlv") {
      pathToRemove = pathToRemove.withType("tpl");
      if (TSystem::doesExistFileOrLevel(pathToRemove))
        TSystem::removeFileOrLevel(pathToRemove);
    }

    // Delete temporary hook file
    TFilePath hookFilePath = TXshSimpleLevel::getHookPath(pathToRemove);
    if (TSystem::doesExistFileOrLevel(hookFilePath))
      TSystem::removeFileOrLevel(hookFilePath);
  }

  // If, after saving the level, a new hook file has been created (and
  // previously there wasn't a hook file) we have to add the new hook file to
  // the repository and set its needs-lock property before committing
  if (m_hookFileName.isEmpty()) {
    // Check if a new hook file exists
    TFilePath path =
        TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();
    TFilePath hookFile = TXshSimpleLevel::getHookPath(path);
    if (TFileStatus(hookFile).doesExist()) {
      m_newHookFileName =
          QString::fromStdWString(hookFile.withoutParentDir().getWideString());

      m_textLabel->setText(tr("Adding hook file to repository..."));

      // Step 4a: Add hookFile to repository
      QStringList args;
      args << QStringLiteral("add");
      args << QStringLiteral("--parents");
      args << m_newHookFileName;

      QObject::disconnect(&m_thread, &VersionControlThread::done, this,
                          nullptr);
      connect(&m_thread, &VersionControlThread::done, this,
              &SVNCommitFrameRangeDialog::onHookFileAdded);
      m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, true);
    } else
      commit();
  } else
    commit();
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onHookFileAdded() {
  m_textLabel->setText(tr("Setting the needs-lock property to hook file..."));
  // Step 4b: set needs-lock property
  QStringList args;
  args << QStringLiteral("propset") << QStringLiteral("svn:needs-lock")
       << QStringLiteral("'*'");
  args << m_newHookFileName;

  QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNCommitFrameRangeDialog::commit);
  m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::commit() {
  m_textLabel->setText(tr("Committing changes..."));

  // Explode, if needed the m_file (OVL)
  TFilePath path =
      TFilePath(m_workingDir.toStdWString()) + m_file.toStdWString();
  if (path.getDots() == "..") {
    TFilePath dir = path.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));
    QString levelName =
        QRegularExpression::escape(QString::fromStdWString(path.getWideName()));
    QString levelType = QString::fromStdString(path.getType());
    QString exp(levelName + "\\.[0-9]{1,4}\\." + levelType);
    QRegularExpression regExp(exp);
    QStringList list = qDir.entryList(QDir::Files);
    m_filesToCommit  = list.filter(regExp);
  } else
    m_filesToCommit = QStringList(m_file);

  // Add also hook file name (if needed)
  if (!m_hookFileName.isEmpty())
    m_filesToCommit.append(m_hookFileName);
  else if (!m_newHookFileName.isEmpty())
    m_filesToCommit.append(m_newHookFileName);

  // Step 5: commit
  QStringList args;
  args << QStringLiteral("commit");
  int size = m_filesToCommit.size();
  for (int i = 0; i < size; i++) args << m_filesToCommit.at(i);

  // Comment
  if (!m_commentTextEdit->toPlainText().isEmpty())
    args << QStringLiteral("-m").append(m_commentTextEdit->toPlainText());
  else
    args << QStringLiteral("-m").append(
        VersionControl::instance()->getUserName() +
        QStringLiteral(" commit changes."));

  QObject::disconnect(&m_thread, &VersionControlThread::done, this, nullptr);
  connect(&m_thread, &VersionControlThread::done, this,
          &SVNCommitFrameRangeDialog::onCommitDone);
  m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, true);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::onCommitButtonClicked() {
  m_commentLabel->hide();
  m_commentTextEdit->hide();

  m_waitingLabel->show();
  m_textLabel->setText(tr("Updating file..."));

  if (m_isOVLLevel && m_hookFileName.isEmpty()) {
    onUpdateDone();
    return;
  }

  // Step 1: Update
  QStringList args;
  args << QStringLiteral("update");
  if (!m_isOVLLevel) args << m_file;
  if (!m_hookFileName.isEmpty()) args << m_hookFileName;

  connect(&m_thread, &VersionControlThread::done, this,
          &SVNCommitFrameRangeDialog::onUpdateDone);
  m_thread.executeCommand(m_workingDir, QStringLiteral("svn"), args, false);
}

//-----------------------------------------------------------------------------

void SVNCommitFrameRangeDialog::switchToCloseButton() {
  m_waitingLabel->hide();
  m_commitButton->disconnect();
  m_commitButton->setText(tr("Close"));
  m_commitButton->setEnabled(true);
  m_commitButton->show();
  m_cancelButton->hide();
  connect(m_commitButton, &QPushButton::clicked, this,
          &SVNCommitFrameRangeDialog::close);
}
