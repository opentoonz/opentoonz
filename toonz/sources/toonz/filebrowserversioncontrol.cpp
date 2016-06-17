

#include "filebrowser.h"
#include "filebrowsermodel.h"
#include "versioncontrolgui.h"
#include "versioncontroltimeline.h"
#include "fileselection.h"
#include "dvdirtreeview.h"
#include "tsystem.h"
#include "tapp.h"

#include "toonz/tscenehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"

namespace {
//---------------------------------------------------------------------------

QStringList getLevelFileNames(TFilePath path) {
  TFilePath dir = path.getParentDir();
  QDir qDir(QString::fromStdWString(dir.getWideString()));
  QString levelName =
      QRegExp::escape(QString::fromStdWString(path.getWideName()));
  QString levelType = QString::fromStdString(path.getType());
  QString exp(levelName + ".[0-9]{1,4}." + levelType);
  QRegExp regExp(exp);
  QStringList list = qDir.entryList(QDir::Files);
  return list.filter(regExp);
}
}  // namespace

//-----------------------------------------------------------------------------

DvItemListModel::Status FileBrowser::getItemVersionControlStatus(
    const FileBrowser::Item &item) {
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  return m_folderTreeView->getItemVersionControlStatus(node, item.m_path);
}

//-----------------------------------------------------------------------------

void FileBrowser::setupVersionControlCommand(QString &file, QString &path) {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.empty()) return;
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();

  VersionControl *vc = VersionControl::instance();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  path = toQString(node->getPath());
  file = toQString(filePaths[0].withoutParentDir());
}

//-----------------------------------------------------------------------------

void FileBrowser::setupVersionControlCommand(QStringList &files, QString &path,
                                             int &sceneIconsCount) {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.empty()) return;

  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  int fileCount = filePaths.size();
  for (int i = 0; i < fileCount; i++) {
    TFilePath fp = filePaths[i];
    if (fp.getDots() == "..") {
      QStringList levelNames = getLevelFileNames(fp);

      if (levelNames.isEmpty()) {
        QString levelName =
            QRegExp::escape(QString::fromStdWString(fp.getWideName()));
        QString levelType = QString::fromStdString(fp.getType());
        QString exp(levelName + ".[0-9]{1,4}." + levelType);
        QRegExp regExp(exp);
        levelNames = node->getMissingFiles(regExp);
      }

      int count = levelNames.size();
      for (int i = 0; i < count; i++) files.append(levelNames.at(i));
    } else {
      QFileInfo fi(toQString(fp));
      files.append(fi.fileName());
    }

    // Add also auxiliary files
    if (fp.getDots() == ".." || fp.getType() == "tlv" ||
        fp.getType() == "pli") {
      TFilePathSet fpset;
      TXshSimpleLevel::getFiles(fp, fpset);

      TFilePathSet::iterator it;
      for (it = fpset.begin(); it != fpset.end(); ++it)
        files.append(QFileInfo(toQString(*it)).fileName());
    }
    // Add sceneIcon (only for scene files)
    else if (fp.getType() == "tnz") {
      TFilePath iconPath = ToonzScene::getIconPath(fp);
      if (TFileStatus(iconPath).doesExist()) {
        QDir dir(toQString(fp.getParentDir()));

#ifdef MACOSX
        files.append(dir.relativeFilePath(toQString(iconPath)));
#else
        files.append(
            dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif
        sceneIconsCount++;
      }
    }
  }

  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();

  if (rootNode) {
    VersionControl *vc = VersionControl::instance();
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  path = toQString(node->getPath());
}

//-----------------------------------------------------------------------------

void FileBrowser::editVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;

  VersionControl *vc = VersionControl::instance();

  bool hasCurrentSceneFile = false;

  // Check the scene file
  TFilePath fp = TApp::instance()
                     ->getCurrentScene()
                     ->getScene()
                     ->getScenePath()
                     .withoutParentDir();
  if (files.contains(toQString(fp))) hasCurrentSceneFile = true;

  // Check the scene resource files
  if (!hasCurrentSceneFile) {
    QStringList currentSceneContents = vc->getCurrentSceneContents();
    int fileSize                     = files.size();
    for (int i = 0; i < fileSize; i++) {
#ifdef MACOSX
      QString fp = path + "/" + files.at(i);
#else
      QString fp = path + "\\" + files.at(i);
#endif
      if (currentSceneContents.contains(fp)) {
        hasCurrentSceneFile = true;
        break;
      }
    }
  }

  if (hasCurrentSceneFile) {
    DVGui::warning(
        tr("Some files that you want to edit are currently opened. Close them "
           "first."));
    return;
  }

  vc->lock(this, path, files, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::editFrameRangeVersionControl() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  TFilePath fp(TFilePath(path.toStdWString()) + file.toStdWString());
  if (fp.getDots() == "..") {
    QStringList list = getLevelFileNames(fp);
    VersionControl::instance()->lockFrameRange(this, path, list);
  } else {
    const std::set<int> &indices = m_itemViewer->getSelectedIndices();
    int frameCount =
        m_itemViewer->getModel()
            ->getItemData(*indices.begin(), DvItemListModel::FrameCount)
            .toInt();

    VersionControl::instance()->lockFrameRange(this, path, file, frameCount);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::unlockFrameRangeVersionControl() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  TFilePath fp(TFilePath(path.toStdWString()) + file.toStdWString());
  if (fp.getDots() == "..") {
    QStringList list = getLevelFileNames(fp);
    VersionControl::instance()->unlockFrameRange(this, path, list);
  } else
    VersionControl::instance()->unlockFrameRange(this, path, file);
}

//-----------------------------------------------------------------------------

void FileBrowser::putFrameRangeVersionControl() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  VersionControl::instance()->commitFrameRange(this, path, file);
}

//-----------------------------------------------------------------------------

void FileBrowser::revertFrameRangeVersionControl() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  TFilePath fp = TFilePath(path.toStdWString()) + file.toStdWString();
  if (fp.getDots() != "..") {
    SVNStatus s = node->getVersionControlStatus(file);

    QString from     = QString::number(s.m_editFrom);
    QString to       = QString::number(s.m_editTo);
    QString userName = VersionControl::instance()->getUserName();
    QString hostName = TSystem::getHostName();

    TFilePath fp(s.m_path.toStdWString());
    QString tempFileName = QString::fromStdWString(fp.getWideName()) + "_" +
                           userName + "_" + hostName + "_" + from + "-" + to +
                           "." + QString::fromStdString(fp.getType());

    TFilePath fullPath = node->getPath() + tempFileName.toStdWString();

    if (TFileStatus(fullPath).doesExist())
      VersionControl::instance()->revertFrameRange(this, path, file,
                                                   toQString(fullPath));
  } else {
    // Use the hook file, if exist, as a tempFileName
    QString tempFile;

    QDir dir(path);
    dir.setNameFilters(QStringList("*.xml"));
    QStringList list = dir.entryList(QDir::Files | QDir::Hidden);
    int listCount    = list.size();

    if (listCount > 0) {
      QString prefix = QString::fromStdWString(fp.getWideName()) + "_" +
                       VersionControl::instance()->getUserName() + "_" +
                       TSystem::getHostName();

      for (int i = 0; i < listCount; i++) {
        QString str = list.at(i);
        if (str.startsWith(prefix)) {
          tempFile = str;
          tempFile.remove("_hooks");
          tempFile = path + "/" + tempFile;
          break;
        }
      }
    }
    VersionControl::instance()->revertFrameRange(this, path, file, tempFile);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::updateAndEditVersionControl() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.empty()) return;
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  QStringList files;
  int sceneIconsCount = 0;

  TFilePath fp = filePaths[0];
  QFileInfo fi(toQString(fp));
  if (fp.getDots() == "..") {
    QStringList levelNames = getLevelFileNames(fp);
    int count              = levelNames.size();
    for (int i = 0; i < count; i++) files.append(levelNames.at(i));
  } else
    files.append(fi.fileName());

  SVNStatus status    = node->getVersionControlStatus(fi.fileName());
  int workingRevision = status.m_workingRevision.toInt();

  // Add also auxiliary files
  if (fp.getDots() == ".." || fp.getType() == "tlv" || fp.getType() == "pli") {
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(fp, fpset);

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it)
      files.append(QFileInfo(toQString(*it)).fileName());
  }
  // Add sceneIcon (only for scene files)
  else if (fp.getType() == "tnz") {
    TFilePath iconPath = ToonzScene::getIconPath(fp);
    if (TFileStatus(iconPath).doesExist()) {
      QDir dir(toQString(fp.getParentDir()));

#ifdef MACOSX
      files.append(dir.relativeFilePath(toQString(iconPath)));
#else
      files.append(
          dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif
      sceneIconsCount++;
    }
  }

  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();

  VersionControl *vc = VersionControl::instance();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  QString path = toQString(node->getPath());

  vc->updateAndLock(this, path, files, workingRevision, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::unlockVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;

  VersionControl *vc = VersionControl::instance();

  bool hasCurrentSceneFile = false;

  // Check the scene file
  TFilePath fp = TApp::instance()
                     ->getCurrentScene()
                     ->getScene()
                     ->getScenePath()
                     .withoutParentDir();
  if (files.contains(toQString(fp))) hasCurrentSceneFile = true;

  // Check the scene resource files
  if (!hasCurrentSceneFile) {
    QStringList currentSceneContents = vc->getCurrentSceneContents();
    int fileSize                     = files.size();
    for (int i = 0; i < fileSize; i++) {
#ifdef MACOSX
      QString fp = path + "/" + files.at(i);
#else
      QString fp = path + "\\" + files.at(i);
#endif
      if (currentSceneContents.contains(fp)) {
        hasCurrentSceneFile = true;
        break;
      }
    }
  }

  if (hasCurrentSceneFile) {
    DVGui::warning(
        tr("Some files that you want to unlock are currently opened. Close "
           "them first."));
    return;
  }
  vc->unlock(this, path, files, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::putVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;
  VersionControl::instance()->commit(this, path, files, false, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::revertVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;
  VersionControl::instance()->revert(this, path, files, false, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::deleteVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;
  VersionControl::instance()->deleteFiles(this, path, files, sceneIconsCount);
}

//-----------------------------------------------------------------------------

void FileBrowser::getVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;
  VersionControl::instance()->update(this, path, files, sceneIconsCount, false,
                                     false, false);
}

//-----------------------------------------------------------------------------

void FileBrowser::getRevisionVersionControl() {
  QStringList files;
  QString path;

  int sceneIconsCount = 0;
  setupVersionControlCommand(files, path, sceneIconsCount);
  if (files.isEmpty() || path.isEmpty()) return;
  VersionControl::instance()->update(this, path, files, sceneIconsCount, false,
                                     true, false);
}

//-----------------------------------------------------------------------------

void FileBrowser::getRevisionHistory() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  QStringList files;
  int iconAdded;
  setupVersionControlCommand(files, path, iconAdded);

  files.removeAt(files.indexOf(file));

  SVNTimeline *timelineDialog = new SVNTimeline(this, path, file, files);
  connect(timelineDialog, SIGNAL(commandDone(const QStringList &)), this,
          SLOT(onVersionControlCommandDone(const QStringList &)));

  timelineDialog->show();
  timelineDialog->raise();
}

//-----------------------------------------------------------------------------

void FileBrowser::onVersionControlCommandDone(const QStringList &files) {
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  // Refresh the tree view and hence the version control status of each item
  m_folderTreeView->refreshVersionControl(node);

  // Get the current scene
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  std::vector<TXshLevel *> levels;
  scene->getLevelSet()->listLevels(levels);
  QMap<TFilePath, TXshLevel *> sceneLevelPaths;
  for (int i = 0; i < levels.size(); i++)
    sceneLevelPaths[scene->decodeFilePath(levels[i]->getPath())] = levels[i];

  TLevelSet *levelSetToCheck = new TLevelSet;

  for (int i = 0; i < files.size(); i++) {
    TFilePath path = TFilePath(files.at(i).toStdWString());
    if (!path.isAbsolute()) {
      TFilePath tempPath = TFilePath(node->getPath()) + path;
      if (!TFileStatus(path).doesExist()) {
        DvDirVersionControlNode *parent =
            dynamic_cast<DvDirVersionControlNode *>(node->getParent());
        while (parent && !TFileStatus(tempPath).doesExist()) {
          tempPath = TFilePath(node->getPath()) + path;
          parent = dynamic_cast<DvDirVersionControlNode *>(parent->getParent());
        }
        path = tempPath;
      }
    }

    if (!TFileStatus(path).doesExist()) continue;

    // Invalidate icons
    IconGenerator::instance()->invalidate(path);

    // TODO: Scene checking (could be useful)

    // Level check
    if (!sceneLevelPaths.isEmpty() && sceneLevelPaths.contains(path)) {
      TXshLevel *level    = sceneLevelPaths.value(path);
      TXshSimpleLevel *sl = level->getSimpleLevel();
      if (sl) {
        levelSetToCheck->insertLevel(sl);
        sl->updateReadOnly();
      }
    }
  }

  VersionControlManager::instance()->setFrameRange(levelSetToCheck, true);
}

//-----------------------------------------------------------------------------

void FileBrowser::showLockInformation() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.empty()) return;

  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (!node) return;

  QFileInfo fi(toQString(filePaths[0]));
  SVNStatus status = node->getVersionControlStatus(fi.fileName());

  SVNLockInfoDialog *dialog = new SVNLockInfoDialog(this, status);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void FileBrowser::showFrameRangeLockInfo() {
  QString file;
  QString path;
  setupVersionControlCommand(file, path);
  if (file.isEmpty() || path.isEmpty()) return;

  TFilePath fp(TFilePath(path.toStdWString()) + file.toStdWString());
  if (fp.getDots() == "..") {
    QStringList list = getLevelFileNames(fp);
    VersionControl::instance()->showFrameRangeLockInfo(this, path, list);
  } else
    VersionControl::instance()->showFrameRangeLockInfo(this, path, file);
}

//-----------------------------------------------------------------------------
