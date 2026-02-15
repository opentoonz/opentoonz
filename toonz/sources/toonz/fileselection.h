#pragma once

#ifndef FILESELECTION_H
#define FILESELECTION_H

#include "dvitemview.h"
#include "tfilepath.h"

// Qt includes
#include <QList>
#include <QPointer>
#include <memory>

class InfoViewer;
class ExportScenePopup;

//=============================================================================
// FileSelection
//-----------------------------------------------------------------------------

class FileSelection final : public DvItemSelection {
  QList<QPointer<InfoViewer>> m_infoViewers;  // QPointer handles nullification
  std::unique_ptr<ExportScenePopup> m_exportScenePopup;

public:
  FileSelection();
  virtual ~FileSelection();

  // Prohibit copying and moving
  FileSelection(const FileSelection&)            = delete;
  FileSelection& operator=(const FileSelection&) = delete;
  FileSelection(FileSelection&&)                 = delete;
  FileSelection& operator=(FileSelection&&)      = delete;

  void getSelectedFiles(std::vector<TFilePath>& files);

  // Commands
  void enableCommands() override;

  void duplicateFiles();
  void deleteFiles();
  void copyFiles();
  void pasteFiles();
  void showFolderContents();
  void viewFileInfo();
  void viewFile();
  void convertFiles();

  void premultiplyFiles();

  void addToBatchRenderList();
  void addToBatchCleanupList();

  void collectAssets();
  void importScenes();
  void exportScenes();
  void exportScene(TFilePath scenePath);
  void selectAll();
  void separateFilesByColors();
};

#endif  // FILESELECTION_H
