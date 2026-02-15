

#include "fileselection.h"

// Tnz6 includes
#include "convertpopup.h"
#include "filebrowser.h"
#include "filedata.h"
#include "iocommand.h"
#include "menubarcommandids.h"
#include "flipbook.h"
#include "filebrowsermodel.h"
#include "exportscenepopup.h"
#include "separatecolorspopup.h"
#include "tapp.h"

#include "batches.h"

// TnzQt includes
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/infoviewer.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "historytypes.h"
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneresources.h"
#include "toonz/preferences.h"
#include "toonz/studiopalette.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"

// TnzCore includes
#include "tfiletype.h"
#include "tsystem.h"

// Qt includes
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QVariant>
#include <QMainWindow>

// C++ includes
#include <memory>

using namespace DVGui;
// TODO Refactor Move commands to FileBrowser class

//------------------------------------------------------------------------
namespace {

//=============================================================================
//  CopyFilesUndo
//-----------------------------------------------------------------------------

class CopyFilesUndo final : public TUndo {
  std::unique_ptr<QMimeData> m_oldData;
  std::unique_ptr<QMimeData> m_newData;

public:
  CopyFilesUndo(QMimeData *oldData, QMimeData *newData)
      : m_oldData(cloneData(oldData)), m_newData(cloneData(newData)) {}

  void undo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_oldData.get()), QClipboard::Clipboard);
  }

  void redo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_newData.get()), QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Copy File"); }

private:
  Q_DISABLE_COPY_MOVE(CopyFilesUndo);
};

//=============================================================================
//  PasteFilesUndo
//-----------------------------------------------------------------------------

class PasteFilesUndo final : public TUndo {
  std::vector<TFilePath> m_newFiles;
  TFilePath m_folder;

public:
  PasteFilesUndo(std::vector<TFilePath> files, TFilePath folder)
      : m_newFiles(std::move(files)), m_folder(std::move(folder)) {}

  void undo() const override {
    for (const TFilePath &path : m_newFiles) {
      if (!TSystem::doesExistFileOrLevel(path)) continue;
      try {
        TSystem::removeFileOrLevel(path);
      } catch (...) {
      }
    }
    FileBrowser::refreshFolder(m_folder);
  }

  void redo() const override {
    if (!TSystem::touchParentDir(m_folder)) TSystem::mkDir(m_folder);
    const auto *data =
        dynamic_cast<const FileData *>(QApplication::clipboard()->mimeData());
    if (!data) return;
    std::vector<TFilePath> files;
    data->getFiles(m_folder, files);
    FileBrowser::refreshFolder(m_folder);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste  File  : ");
    bool first  = true;
    for (const TFilePath &path : m_newFiles) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::fromStdString(path.getLevelName());
    }
    return str;
  }

private:
  Q_DISABLE_COPY_MOVE(PasteFilesUndo);
};

//=============================================================================
//  DuplicateUndo
//-----------------------------------------------------------------------------

class DuplicateUndo final : public TUndo {
  std::vector<TFilePath> m_files;
  std::vector<TFilePath> m_newFiles;

public:
  DuplicateUndo(std::vector<TFilePath> files, std::vector<TFilePath> newFiles)
      : m_files(std::move(files)), m_newFiles(std::move(newFiles)) {}

  void undo() const override {
    for (const TFilePath &path : m_newFiles) {
      if (!TSystem::doesExistFileOrLevel(path)) continue;
      if (path.getType() == "tnz")
        TSystem::rmDirTree(path.getParentDir() + (path.getName() + "_files"));
      try {
        TSystem::removeFileOrLevel(path);
      } catch (...) {
      }
    }
    FileBrowser::refreshFolder(m_newFiles[0].getParentDir());
  }

  void redo() const override {
    for (const TFilePath &fp : m_files) {
      ImageUtils::duplicate(fp);
      FileBrowser::refreshFolder(fp.getParentDir());
    }
    FileBrowser::refreshFolder(m_files[0].getParentDir());
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Duplicate  File  : ");
    bool first  = true;
    for (size_t i = 0; i < m_files.size(); ++i) {
      if (!first) str += QString(",  ");
      first = false;
      str += QString("%1 > %2")
                 .arg(QString::fromStdString(m_files[i].getLevelName()))
                 .arg(QString::fromStdString(m_newFiles[i].getLevelName()));
    }
    return str;
  }

private:
  Q_DISABLE_COPY_MOVE(DuplicateUndo);
};

TPaletteP viewedPalette;

}  // namespace

//------------------------------------------------------------------------

FileSelection::FileSelection() : m_exportScenePopup(nullptr) {}

FileSelection::~FileSelection() {
  // QPointer automatically nullifies, but we still own the InfoViewer objects.
  // They are stored in m_infoViewers as QPointer; we need to delete them.
  // However, QPointer does not own the objects; we must delete the raw
  // pointers.
  for (auto &viewer : m_infoViewers) {
    delete viewer;  // delete if not null
  }
  // m_exportScenePopup is a unique_ptr, no manual delete needed
}

//------------------------------------------------------------------------

void FileSelection::getSelectedFiles(std::vector<TFilePath> &files) {
  if (!getModel()) return;
  const std::set<int> &indices = getSelectedIndices();
  for (int idx : indices) {
    QString name =
        getModel()->getItemData(idx, DvItemListModel::FullPath).toString();
    files.emplace_back(name.toStdWString());
  }
}

//------------------------------------------------------------------------

void FileSelection::enableCommands() {
  DvItemSelection::enableCommands();
  enableCommand(this, MI_DuplicateFile, &FileSelection::duplicateFiles);
  enableCommand(this, MI_Clear, &FileSelection::deleteFiles);
  enableCommand(this, MI_Copy, &FileSelection::copyFiles);
  enableCommand(this, MI_Paste, &FileSelection::pasteFiles);
  enableCommand(this, MI_ShowFolderContents,
                &FileSelection::showFolderContents);
  enableCommand(this, MI_ConvertFiles, &FileSelection::convertFiles);
  enableCommand(this, MI_AddToBatchRenderList,
                &FileSelection::addToBatchRenderList);
  enableCommand(this, MI_AddToBatchCleanupList,
                &FileSelection::addToBatchCleanupList);
  enableCommand(this, MI_CollectAssets, &FileSelection::collectAssets);
  enableCommand(this, MI_ImportScenes, &FileSelection::importScenes);
  enableCommand(this, MI_ExportScenes, &FileSelection::exportScenes);
  enableCommand(this, MI_SelectAll, &FileSelection::selectAll);
  enableCommand(this, MI_SeparateColors, &FileSelection::separateFilesByColors);
}

//------------------------------------------------------------------------

void FileSelection::addToBatchRenderList() {
  if (!BatchesController::instance()->getTasksTree()) {
    QAction *taskPopup = CommandManager::instance()->getAction(MI_OpenTasks);
    taskPopup->trigger();
  }
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  for (const TFilePath &fp : files)
    BatchesController::instance()->addComposerTask(fp);

  DVGui::info(QObject::tr(" Task added to the Batch Render List."));
}

//------------------------------------------------------------------------

void FileSelection::addToBatchCleanupList() {
  if (!BatchesController::instance()->getTasksTree()) {
    QAction *taskPopup = CommandManager::instance()->getAction(MI_OpenTasks);
    taskPopup->trigger();
  }

  std::vector<TFilePath> files;
  getSelectedFiles(files);
  for (const TFilePath &fp : files)
    BatchesController::instance()->addCleanupTask(fp);

  DVGui::info(QObject::tr(" Task added to the Batch Cleanup List."));
}

//------------------------------------------------------------------------

void FileSelection::deleteFiles() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  QString question;
  if (files.size() == 1) {
    TFileStatus fs(files[0]);
    if (fs.isDirectory()) {
      if (!fs.isWritable()) return;
      question = QObject::tr("Deleting folder %1. Are you sure?")
                     .arg(files[0].getQString());
    } else {
      QString fn = QString::fromStdWString(files[0].getWideString());
      question   = QObject::tr("Deleting %1. Are you sure?").arg(fn);
    }
  } else {
    question = QObject::tr("Deleting %n files. Are you sure?", "",
                           static_cast<int>(files.size()));
  }
  int ret =
      DVGui::MsgBox(question, QObject::tr("Delete"), QObject::tr("Cancel"), 1);
  if (ret == 2 || ret == 0) return;

  for (const TFilePath &fp : files) {
    if (TFileStatus(fp).isDirectory()) {
      QFile(fp.getQString()).moveToTrash();
    } else {
      TSystem::moveFileOrLevelToRecycleBin(fp);
      IconGenerator::instance()->remove(fp);
    }
  }
  selectNone();
  FileBrowser::refreshFolder(files[0].getParentDir());
}

//------------------------------------------------------------------------

void FileSelection::copyFiles() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  QClipboard *clipboard = QApplication::clipboard();
  std::unique_ptr<QMimeData> oldData(cloneData(clipboard->mimeData()));
  auto *data = new FileData();
  data->setFiles(files);
  clipboard->setMimeData(data);
  std::unique_ptr<QMimeData> newData(cloneData(clipboard->mimeData()));

  TUndoManager::manager()->add(
      new CopyFilesUndo(oldData.release(), newData.release()));
}

//------------------------------------------------------------------------

void FileSelection::pasteFiles() {
  const auto *data =
      dynamic_cast<const FileData *>(QApplication::clipboard()->mimeData());
  if (!data) return;
  auto *model = dynamic_cast<FileBrowser *>(getModel());
  if (!model) return;
  TFilePath folder = model->getFolder();
  std::vector<TFilePath> newFiles;
  data->getFiles(folder, newFiles);
  FileBrowser::refreshFolder(folder);
  TUndoManager::manager()->add(
      new PasteFilesUndo(std::move(newFiles), std::move(folder)));
}

//------------------------------------------------------------------------

void FileSelection::showFolderContents() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  TFilePath folderPath;
  if (!files.empty()) folderPath = files[0].getParentDir();
  if (folderPath.isEmpty()) {
    auto *model = dynamic_cast<FileBrowser *>(getModel());
    if (!model) return;
    folderPath = model->getFolder();
  }
  if (TSystem::isUNC(folderPath)) {
    bool ok = QDesktopServices::openUrl(
        QUrl(QString::fromStdWString(folderPath.getWideString())));
    if (ok) return;
    // If the above fails, then try opening UNC path with the same way as the
    // local files.. QUrl::fromLocalFile() seems to work for UNC path as well in
    // our environment. (8/10/2021 shun-iwasawa)
  }
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdWString(folderPath.getWideString())));
}

//------------------------------------------------------------------------

void FileSelection::viewFileInfo() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  for (const TFilePath &fp : files) {
    QPointer<InfoViewer> infoViewer = nullptr;

    // Look for a hidden InfoViewer that can be reused
    for (auto &v : m_infoViewers) {
      if (v && v->isHidden()) {
        infoViewer = v;
        break;
      }
    }

    if (!infoViewer) {
      infoViewer = new InfoViewer();
      m_infoViewers.append(infoViewer);
    }

    FileBrowserPopup::setModalBrowserToParent(infoViewer);
    infoViewer->setItem(0, 0, fp);
  }
}

//------------------------------------------------------------------------

void FileSelection::viewFile() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  for (const TFilePath &fp : files) {
    if (!TFileType::isViewable(TFileType::getInfo(fp)) && fp.getType() != "tpl")
      continue;

    if (Preferences::instance()->isDefaultViewerEnabled() &&
        (fp.getType() == "mov" || fp.getType() == "avi" ||
         fp.getType() == "3gp"))
      QDesktopServices::openUrl(QUrl("file:///" + toQString(fp)));
    else if (fp.getType() == "tpl") {
      viewedPalette = StudioPalette::instance()->getPalette(fp, false);
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->setPalette(viewedPalette.getPointer());
      CommandManager::instance()->execute("MI_OpenPalette");
    } else {
      FlipBook *fb = ::viewFile(fp);
      if (fb) {
        FileBrowserPopup::setModalBrowserToParent(fb->parentWidget());
      }
    }
  }
}

//------------------------------------------------------------------------

void FileSelection::convertFiles() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  static std::unique_ptr<ConvertPopup> popup =
      std::make_unique<ConvertPopup>(false);
  if (popup->isConverting()) {
    DVGui::info(QObject::tr(
        "A conversion task is in progress! wait until it stops or cancel it"));
    return;
  }
  popup->setFiles(files);
  popup->exec();
}

//------------------------------------------------------------------------

void FileSelection::premultiplyFiles() {
  QString question = QObject::tr(
      "You are going to premultiply selected files.\nThe operation cannot be "
      "undone: are you sure?");
  int ret = DVGui::MsgBox(question, QObject::tr("Premultiply"),
                          QObject::tr("Cancel"), 1);
  if (ret == 2 || ret == 0) return;

  std::vector<TFilePath> files;
  getSelectedFiles(files);
  for (const TFilePath &fp : files) ImageUtils::premultiply(fp);
}

//------------------------------------------------------------------------

void FileSelection::duplicateFiles() {
  std::vector<TFilePath> files;
  std::vector<TFilePath> newFiles;
  getSelectedFiles(files);
  for (const TFilePath &fp : files) {
    TFilePath newPath = ImageUtils::duplicate(fp);
    newFiles.push_back(newPath);
    FileBrowser::refreshFolder(fp.getParentDir());
  }
  TUndoManager::manager()->add(
      new DuplicateUndo(std::move(files), std::move(newFiles)));
}

//------------------------------------------------------------------------

static int collectAssets(const TFilePath &scenePath) {
  ToonzScene scene;
  scene.load(scenePath);
  ResourceCollector collector(&scene);
  SceneResources resources(&scene, scene.getXsheet());
  resources.accept(&collector);
  int count = collector.getCollectedResourceCount();
  if (count > 0) {
    scene.save(scenePath);
  }
  return count;
}

//------------------------------------------------------------------------

void FileSelection::collectAssets() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;
  int collectedAssets = 0;

  int count = static_cast<int>(files.size());

  if (count > 1) {
    QMainWindow *mw = TApp::instance()->getMainWindow();
    ProgressDialog progress(QObject::tr("Collecting assets..."),
                            QObject::tr("Abort"), 0, count, mw);
    progress.setWindowModality(Qt::WindowModal);

    int i = 1;
    for (const TFilePath &fp : files) {
      collectedAssets += ::collectAssets(fp);
      progress.setValue(i++);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    collectedAssets = ::collectAssets(files[0]);
  }
  if (collectedAssets == 0)
    DVGui::info(QObject::tr("There are no assets to collect"));
  else if (collectedAssets == 1)
    DVGui::info(QObject::tr("One asset imported"));
  else
    DVGui::info(QObject::tr("%1 assets imported").arg(collectedAssets));
  DvDirModel::instance()->refreshFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
}

//------------------------------------------------------------------------

void FileSelection::separateFilesByColors() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  static std::unique_ptr<SeparateColorsPopup> popup =
      std::make_unique<SeparateColorsPopup>();
  if (popup->isConverting()) {
    DVGui::MsgBox(INFORMATION, QObject::tr("A separation task is in progress! "
                                           "wait until it stops or cancel it"));
    return;
  }
  popup->setFiles(files);
  popup->show();
  popup->raise();
}

//------------------------------------------------------------------------

static int importScene(const TFilePath &scenePath) {
  ToonzScene scene;
  try {
    IoCmd::loadScene(scene, scenePath, true);
  } catch (TException &e) {
    DVGui::error(QObject::tr("Error loading scene %1 :%2")
                     .arg(toQString(scenePath))
                     .arg(QString::fromStdWString(e.getMessage())));
    return 0;
  } catch (...) {
    DVGui::error(
        QObject::tr("Error loading scene %1").arg(toQString(scenePath)));
    return 0;
  }

  try {
    scene.save(scene.getScenePath());
  } catch (TException &) {
    DVGui::error(QObject::tr("There was an error saving the %1 scene.")
                     .arg(toQString(scenePath)));
    return 0;
  }

  DvDirModel::instance()->refreshFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());

  return 1;
}

//------------------------------------------------------------------------

void FileSelection::importScenes() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;
  int importedSceneCount = 0;

  int count = static_cast<int>(files.size());

  if (count > 1) {
    QMainWindow *mw = TApp::instance()->getMainWindow();
    ProgressDialog progress(QObject::tr("Importing scenes..."),
                            QObject::tr("Abort"), 0, count, mw);
    progress.setWindowModality(Qt::WindowModal);

    int i = 1;
    for (const TFilePath &fp : files) {
      int ret = ::importScene(fp);
      importedSceneCount += ret;
      progress.setValue(i++);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    int ret = ::importScene(files[0]);
    importedSceneCount += ret;
  }
  if (importedSceneCount == 0)
    DVGui::info(QObject::tr("No scene imported"));
  else if (importedSceneCount == 1)
    DVGui::info(QObject::tr("One scene imported"));
  else
    DVGui::info(QString::number(importedSceneCount) +
                QObject::tr(" scenes imported"));
}

//------------------------------------------------------------------------

void FileSelection::exportScenes() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (!m_exportScenePopup)
    m_exportScenePopup = std::make_unique<ExportScenePopup>(files);
  else
    m_exportScenePopup->setScenes(files);
  m_exportScenePopup->show();
}

//------------------------------------------------------------------------

void FileSelection::exportScene(TFilePath scenePath) {
  if (scenePath.isEmpty()) return;

  std::vector<TFilePath> files;
  files.push_back(std::move(scenePath));
  if (!m_exportScenePopup)
    m_exportScenePopup = std::make_unique<ExportScenePopup>(files);
  else
    m_exportScenePopup->setScenes(files);
  m_exportScenePopup->show();
}

//------------------------------------------------------------------------

void FileSelection::selectAll() {
  DvItemSelection::selectAll();
  const std::set<int> &indices = getSelectedIndices();
  if (!indices.empty()) {
    int firstIdx = *indices.begin();
    QString name =
        getModel()->getItemData(firstIdx, DvItemListModel::FullPath).toString();
    FileBrowser::updateItemViewerPanel();
  }
}

//-----------------------------------------------------------------------------

class ExportCurrentSceneCommandHandler final : public MenuItemHandler {
public:
  ExportCurrentSceneCommandHandler() : MenuItemHandler(MI_ExportCurrentScene) {}
  void execute() override {
    TApp *app                 = TApp::instance();
    TSceneHandle *sceneHandle = app->getCurrentScene();
    if (!sceneHandle) return;
    ToonzScene *scene = sceneHandle->getScene();
    if (!scene) return;
    TFilePath fp = scene->getScenePath();

    if (sceneHandle->getDirtyFlag() || scene->isUntitled() ||
        !TSystem::doesExistFileOrLevel(fp)) {
      DVGui::warning(tr("You must save the current scene first."));
      return;
    }

    auto fs = std::make_unique<FileSelection>();
    fs->exportScene(fp);
  }
} ExportCurrentSceneCommandHandler;
