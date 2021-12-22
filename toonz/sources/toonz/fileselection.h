#pragma once

#ifndef FILESELECTION_H
#define FILESELECTION_H

#include "dvitemview.h"
#include "tfilepath.h"

class InfoViewer;
class ExportScenePopup;

//=============================================================================
// FileSelection
//-----------------------------------------------------------------------------

class FileSelection final : public DvItemSelection {
  QList<InfoViewer *> m_infoViewers;
  ExportScenePopup *m_exportScenePopup;

public:
  FileSelection();
  ~FileSelection();

  void getSelectedFiles(std::vector<TFilePath> &files);

  // commands
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
  void selectAll();
  void separateFilesByColors();
};

#endif  // FILESELECTION_H
