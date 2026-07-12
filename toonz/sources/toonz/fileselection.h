#pragma once

#ifndef FILESELECTION_H
#define FILESELECTION_H

#include "dvitemview.h"
#include "tfilepath.h"

// Qt includes
#include <QList>
#include <QPointer>

class InfoViewer;

//=============================================================================
// FileSelection: Manages file selection commands in FileBrowser
// Uses QPointer for InfoViewer to auto-nullify when deleted.
// Copy/move is prohibited to prevent dangling pointers.
//=============================================================================
class FileSelection final : public DvItemSelection {
  QList<QPointer<InfoViewer>> m_infoViewers;  // automatically nullified

public:
  FileSelection();
  ~FileSelection() override;

  // Disable copy and move operations
  FileSelection(const FileSelection&)            = delete;
  FileSelection& operator=(const FileSelection&) = delete;
  FileSelection(FileSelection&&)                 = delete;
  FileSelection& operator=(FileSelection&&)      = delete;

  // Retrieve currently selected files
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
