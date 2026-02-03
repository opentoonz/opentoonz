#pragma once

#ifndef TOONZFOLDERS_INCLUDED
#define TOONZFOLDERS_INCLUDED

#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <set>

namespace ToonzFolder {

DVAPI TFilePath getModulesDir();
DVAPI TFilePath getTemplateModuleDir();
DVAPI TFilePath getMyModuleDir();

DVAPI TFilePath getRoomsDir();
DVAPI TFilePath getTemplateRoomsDir();
DVAPI TFilePath getMyRoomsDir();
DVAPI TFilePath getRoomsFile(TFilePath filename);
DVAPI TFilePath getRoomsFile(std::string fn);

// restituisce getMyModuleDir() + filename
// o getTemplateModuleDir() + filename
DVAPI TFilePath getModuleFile(TFilePath filename);
DVAPI TFilePath getModuleFile(std::string fn);

DVAPI TFilePathSet getProjectsFolders();
DVAPI TFilePath getFirstProjectsFolder();
DVAPI TFilePath getStudioPaletteFolder();
DVAPI TFilePath getFxPresetFolder();
DVAPI TFilePath getLibraryFolder();
DVAPI TFilePath getReslistPath(bool forCleanup);
DVAPI TFilePath getCacheRootFolder();
DVAPI TFilePath getCrashReportFolder();
DVAPI TFilePath getProfileFolder();
};

class DVAPI FolderListenerManager {  // singleton

public:
  class Listener {
  public:
    virtual void onFolderChanged(const TFilePath &path) = 0;
    virtual ~Listener() {}
  };

private:
  std::set<Listener *> m_listeners;

  FolderListenerManager();

public:
  static FolderListenerManager *instance();

  ~FolderListenerManager();

  void notifyFolderChanged(const TFilePath &path);

  void addListener(Listener *listener);
  void removeListener(Listener *listener);
};

#endif
