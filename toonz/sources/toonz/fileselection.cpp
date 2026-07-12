

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
#include <QFile>

// C++ includes
#include <memory>
#include <set>
#include <algorithm>

using namespace DVGui;
// TODO Refactor Move commands to FileBrowser class

//------------------------------------------------------------------------
// Undo classes
//------------------------------------------------------------------------
namespace {

//=============================================================================
// CopyFilesUndo
//-----------------------------------------------------------------------------

class CopyFilesUndo final : public TUndo {
  std::unique_ptr<QMimeData> m_oldData;
  std::unique_ptr<QMimeData> m_newData;

public:
  // Takes ownership of the passed unique_ptrs (they already contain cloned
  // data)
  CopyFilesUndo(std::unique_ptr<QMimeData> oldData,
                std::unique_ptr<QMimeData> newData)
      : m_oldData(std::move(oldData)), m_newData(std::move(newData)) {}

  void undo() const override {
    QApplication::clipboard()->setMimeData(cloneData(m_oldData.get()),
                                           QClipboard::Clipboard);
  }

  void redo() const override {
    QApplication::clipboard()->setMimeData(cloneData(m_newData.get()),
                                           QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return QObject::tr("Copy File"); }

private:
  Q_DISABLE_COPY_MOVE(CopyFilesUndo);
};

//=============================================================================
// PasteFilesUndo
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
    QString str = QObject::tr("Paste File: ");
    bool first  = true;
    for (const TFilePath &path : m_newFiles) {
      if (!first) str += ", ";
      first = false;
      str += QString::fromStdString(path.getLevelName());
    }
    return str;
  }

private:
  Q_DISABLE_COPY_MOVE(PasteFilesUndo);
};

//=============================================================================
// DuplicateUndo
//-----------------------------------------------------------------------------

class DuplicateUndo final : public TUndo {
  std::vector<TFilePath>
      m_srcFiles;  // original files that were successfully duplicated
  std::vector<TFilePath> m_dstFiles;  // corresponding new files

public:
  DuplicateUndo(std::vector<TFilePath> srcFiles,
                std::vector<TFilePath> dstFiles)
      : m_srcFiles(std::move(srcFiles)), m_dstFiles(std::move(dstFiles)) {
    // Both vectors must have the same size and be paired.
    assert(m_srcFiles.size() == m_dstFiles.size());
  }

  void undo() const override {
    for (const TFilePath &path : m_dstFiles) {
      if (path.isEmpty() || !TSystem::doesExistFileOrLevel(path)) continue;
      if (path.getType() == "tnz")
        TSystem::rmDirTree(path.getParentDir() + (path.getName() + "_files"));
      try {
        TSystem::removeFileOrLevel(path);
      } catch (...) {
      }
    }
    if (!m_dstFiles.empty())
      FileBrowser::refreshFolder(m_dstFiles[0].getParentDir());
  }

  void redo() const override {
    // Instead of calling ImageUtils::duplicate again (which might generate
    // different names), we directly copy the original files to the stored
    // destination paths.
    for (size_t i = 0; i < m_srcFiles.size(); ++i) {
      if (m_srcFiles[i].isEmpty() || m_dstFiles[i].isEmpty()) continue;
      try {
        // Use copyFileOrLevel to handle both files and levels.
        TSystem::copyFileOrLevel(m_dstFiles[i], m_srcFiles[i]);
      } catch (...) {
        // Ignore errors; at least we tried.
      }
    }
    if (!m_srcFiles.empty())
      FileBrowser::refreshFolder(m_srcFiles[0].getParentDir());
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    QString str = QObject::tr("Duplicate File: ");
    bool first  = true;
    for (size_t i = 0; i < m_srcFiles.size(); ++i) {
      if (!first) str += ", ";
      first = false;
      str += QString("%1 > %2")
                 .arg(QString::fromStdString(m_srcFiles[i].getLevelName()))
                 .arg(QString::fromStdString(m_dstFiles[i].getLevelName()));
    }
    return str;
  }

private:
  Q_DISABLE_COPY_MOVE(DuplicateUndo);
};

//=============================================================================
// Global viewed palette (for .tpl files)
//=============================================================================
TPaletteP viewedPalette;

}  // namespace

//------------------------------------------------------------------------
// FileSelection Implementation
//------------------------------------------------------------------------

FileSelection::FileSelection() {}

FileSelection::~FileSelection() {
  // Use deleteLater to safely delete InfoViewers, they will be cleaned up when
  // the event loop runs.
  for (auto &viewer : m_infoViewers) {
    if (viewer) viewer->deleteLater();
  }
}

//------------------------------------------------------------------------

void FileSelection::getSelectedFiles(std::vector<TFilePath> &files) {
  if (!getModel()) return;
  const std::set<int> &indices = getSelectedIndices();
  for (int idx : indices) {
    QString name =
        getModel()->getItemData(idx, DvItemListModel::FullPath).toString();
    // Avoid adding empty paths (should not happen, but be safe)
    if (!name.isEmpty()) files.emplace_back(name.toStdWString());
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
// Batch operations
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

  DVGui::info(QObject::tr("Task added to the Batch Render List."));
}

void FileSelection::addToBatchCleanupList() {
  if (!BatchesController::instance()->getTasksTree()) {
    QAction *taskPopup = CommandManager::instance()->getAction(MI_OpenTasks);
    taskPopup->trigger();
  }

  std::vector<TFilePath> files;
  getSelectedFiles(files);

  for (const TFilePath &fp : files)
    BatchesController::instance()->addCleanupTask(fp);

  DVGui::info(QObject::tr("Task added to the Batch Cleanup List."));
}

//------------------------------------------------------------------------
// Delete files/folders
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
      question = QObject::tr("Deleting %1. Are you sure?")
                     .arg(QString::fromStdWString(files[0].getWideString()));
    }
  } else {
    question = QObject::tr("Deleting %n files. Are you sure?", "",
                           static_cast<int>(files.size()));
  }

  int ret =
      DVGui::MsgBox(question, QObject::tr("Delete"), QObject::tr("Cancel"), 1);
  if (ret == 2 || ret == 0) return;

  for (const TFilePath &fp : files) {
    if (TFileStatus(fp).isDirectory())
      QFile(fp.getQString()).moveToTrash();
    else {
      TSystem::moveFileOrLevelToRecycleBin(fp);
      IconGenerator::instance()->remove(fp);
    }
  }
  selectNone();
  FileBrowser::refreshFolder(files[0].getParentDir());
}

//------------------------------------------------------------------------
// Copy files (clipboard)
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
      new CopyFilesUndo(std::move(oldData), std::move(newData)));
}

//------------------------------------------------------------------------
// Paste files
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
// Show folder in system explorer
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
// View file info (reuse hidden InfoViewers)
//------------------------------------------------------------------------

void FileSelection::viewFileInfo() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  // Remove any dead viewers from the list
  m_infoViewers.erase(
      std::remove_if(m_infoViewers.begin(), m_infoViewers.end(),
                     [](const QPointer<InfoViewer> &v) { return v.isNull(); }),
      m_infoViewers.end());

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
// View file (open in flipbook or default viewer)
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
      if (fb) FileBrowserPopup::setModalBrowserToParent(fb->parentWidget());
    }
  }
}

//------------------------------------------------------------------------
// Convert files (safe static popup, reused without deletion)
//------------------------------------------------------------------------

void FileSelection::convertFiles() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  static ConvertPopup *popup = nullptr;
  if (!popup) {
    popup = new ConvertPopup(false);
    // Ensure the popup is not deleted when closed, to avoid crashes with
    // background threads.
    popup->setAttribute(Qt::WA_DeleteOnClose, false);
  }

  if (popup->isConverting()) {
    DVGui::info(QObject::tr(
        "A conversion task is in progress! Wait until it stops or cancel it."));
    return;
  }
  popup->setFiles(files);
  popup->exec();  // modal, will block until closed
}

//------------------------------------------------------------------------
// Premultiply files
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
// Duplicate files
//------------------------------------------------------------------------

void FileSelection::duplicateFiles() {
  std::vector<TFilePath> selected;
  getSelectedFiles(selected);
  if (selected.empty()) return;

  std::vector<TFilePath> srcFiles, dstFiles;

  for (const TFilePath &fp : selected) {
    TFilePath newPath = ImageUtils::duplicate(fp);
    if (!newPath.isEmpty()) {
      srcFiles.push_back(fp);
      dstFiles.push_back(newPath);
    }
  }

  if (srcFiles.empty()) return;  // nothing duplicated

  // Refresh the folder of the first original file (or the first destination)
  FileBrowser::refreshFolder(srcFiles[0].getParentDir());

  TUndoManager::manager()->add(
      new DuplicateUndo(std::move(srcFiles), std::move(dstFiles)));
}

//------------------------------------------------------------------------
// Collect assets (with progress dialog)
//------------------------------------------------------------------------

static int collectAssetsInternal(const TFilePath &scenePath) {
  ToonzScene scene;
  scene.load(scenePath);
  ResourceCollector collector(&scene);
  SceneResources resources(&scene, scene.getXsheet());
  resources.accept(&collector);
  int count = collector.getCollectedResourceCount();
  if (count > 0) scene.save(scenePath);
  return count;
}

void FileSelection::collectAssets() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  int collectedAssets = 0;
  int count           = static_cast<int>(files.size());

  if (count > 1) {
    QMainWindow *mw = TApp::instance()->getMainWindow();
    ProgressDialog progress(QObject::tr("Collecting assets..."),
                            QObject::tr("Abort"), 0, count, mw);
    progress.setWindowModality(Qt::WindowModal);

    for (int i = 0; i < count; ++i) {
      collectedAssets += ::collectAssetsInternal(files[i]);
      progress.setValue(i + 1);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    collectedAssets = ::collectAssetsInternal(files[0]);
  }

  if (collectedAssets == 0)
    DVGui::info(QObject::tr("There are no assets to collect"));
  else
    DVGui::info(QObject::tr("%1 assets imported").arg(collectedAssets));

  DvDirModel::instance()->refreshFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
}

//------------------------------------------------------------------------
// Import scenes (with progress dialog)
//------------------------------------------------------------------------

static int importSceneInternal(const TFilePath &scenePath) {
  ToonzScene scene;
  try {
    IoCmd::loadScene(scene, scenePath, true);
  } catch (TException &e) {
    DVGui::error(QObject::tr("Error loading scene %1 : %2")
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
    DVGui::error(
        QObject::tr("Error saving scene %1").arg(toQString(scenePath)));
    return 0;
  }

  DvDirModel::instance()->refreshFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
  return 1;
}

void FileSelection::importScenes() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  int importedSceneCount = 0;
  int count              = static_cast<int>(files.size());

  if (count > 1) {
    QMainWindow *mw = TApp::instance()->getMainWindow();
    ProgressDialog progress(QObject::tr("Importing scenes..."),
                            QObject::tr("Abort"), 0, count, mw);
    progress.setWindowModality(Qt::WindowModal);

    for (int i = 0; i < count; ++i) {
      importedSceneCount += ::importSceneInternal(files[i]);
      progress.setValue(i + 1);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    importedSceneCount = ::importSceneInternal(files[0]);
  }

  if (importedSceneCount == 0)
    DVGui::info(QObject::tr("No scene imported"));
  else if (importedSceneCount == 1)
    DVGui::info(QObject::tr("One scene imported"));
  else
    DVGui::info(QObject::tr("%1 scenes imported").arg(importedSceneCount));
}

//------------------------------------------------------------------------
// Export scenes (disposable popups with WA_DeleteOnClose)
// Note: The constructor of ExportScenePopup currently does not accept a parent.
// We create the popup without explicit parent it will use the default.
//------------------------------------------------------------------------

void FileSelection::exportScenes() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  auto *popup = new ExportScenePopup(files);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  popup->show();
  popup->raise();
  popup->activateWindow();
}

void FileSelection::exportScene(TFilePath scenePath) {
  if (scenePath.isEmpty()) return;
  std::vector<TFilePath> files = {scenePath};
  auto *popup                  = new ExportScenePopup(files);
  popup->setAttribute(Qt::WA_DeleteOnClose);
  popup->show();
  popup->raise();
  popup->activateWindow();
}

//------------------------------------------------------------------------
// Select all
//------------------------------------------------------------------------

void FileSelection::selectAll() {
  DvItemSelection::selectAll();
  FileBrowser::updateItemViewerPanel();
}

//------------------------------------------------------------------------
// Separate files by colors (safe static popup, reused without deletion)
//------------------------------------------------------------------------

void FileSelection::separateFilesByColors() {
  std::vector<TFilePath> files;
  getSelectedFiles(files);
  if (files.empty()) return;

  static SeparateColorsPopup *popup = nullptr;
  if (!popup) {
    popup = new SeparateColorsPopup();
    // Ensure the popup is not deleted when closed.
    popup->setAttribute(Qt::WA_DeleteOnClose, false);
  }

  if (popup->isConverting()) {
    DVGui::MsgBox(INFORMATION,
                  QObject::tr("A separation task is in progress! "
                              "Wait until it stops or cancel it."));
    return;
  }
  popup->setFiles(files);
  popup->show();
  popup->raise();
  popup->activateWindow();
}

//-----------------------------------------------------------------------------
// Handler for "Export Current Scene" command
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

    std::vector<TFilePath> files = {fp};
    auto *popup                  = new ExportScenePopup(files);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->show();
    popup->raise();
    popup->activateWindow();
  }
} ExportCurrentSceneCommandHandler;
