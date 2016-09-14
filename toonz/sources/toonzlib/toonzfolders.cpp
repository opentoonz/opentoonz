

#include "toonz/toonzfolders.h"
#include "tsystem.h"
#include "tenv.h"
//#include "appmainshell.h"
#include "tconvert.h"
#include "toonz/preferences.h"
#include <QStandardPaths>
#ifdef _WIN32
#include <shlobj.h>
#include <Winnetwk.h>
#endif
#ifdef MACOSX
#include <Cocoa/Cocoa.h>
#endif

using namespace TEnv;

//-------------------------------------------------------------------
namespace {
TFilePath getMyDocumentsPath() {
  QString documentsPath =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0];
  return TFilePath(documentsPath);
}

// Desktop Path
TFilePath getDesktopPath() {
  QString desktopPath =
      QStandardPaths::standardLocations(QStandardPaths::DesktopLocation)[0];
  return TFilePath(desktopPath);
}
}
//-------------------------------------------------------------------

TFilePath ToonzFolder::getModulesDir() {
  return getProfileFolder() + "layouts";
}

TFilePathSet ToonzFolder::getProjectsFolders() {
  int location = Preferences::instance()->getProjectRoot();
  QString path = Preferences::instance()->getCustomProjectRoot();
  TFilePathSet fps;
  int projectPaths = Preferences::instance()->getProjectRoot();
  int stuff        = (projectPaths / 1000) % 10;
  int desktop      = (projectPaths / 100) % 10;
  int documents    = (projectPaths / 10) % 10;
  int custom       = projectPaths % 10;

  // make sure at least something is there
  if (!desktop && !stuff && !custom) documents = 1;
  // fps = getSystemVarPathSetValue(getSystemVarPrefix() + "PROJECTS");
  TFilePathSet tempFps =
      getSystemVarPathSetValue(getSystemVarPrefix() + "PROJECTS");
  if (stuff) {
    for (TFilePath tempPath : tempFps) {
      if (TSystem::doesExistFileOrLevel(TFilePath(tempPath))) {
        fps.push_back(TFilePath(tempPath));
      }
    }
    if (tempFps.size() == 0) fps.push_back(TEnv::getStuffDir() + "Projects");
  }
  if (documents) {
    fps.push_back(getMyDocumentsPath() + "OpenToonz");
    if (!TSystem::doesExistFileOrLevel(getMyDocumentsPath() + "OpenToonz")) {
      TSystem::mkDir(getMyDocumentsPath() + "OpenToonz");
    }
  }
  if (desktop) {
    fps.push_back(getDesktopPath() + "OpenToonz");
    if (!TSystem::doesExistFileOrLevel(getDesktopPath() + "OpenToonz")) {
      TSystem::mkDir(getDesktopPath() + "OpenToonz");
    }
  }
  if (custom) {
    QStringList paths = path.split("**");
    for (QString tempPath : paths) {
      if (TSystem::doesExistFileOrLevel(TFilePath(tempPath))) {
        fps.push_back(TFilePath(tempPath));
      }
    }
  }
  return fps;
}

TFilePath ToonzFolder::getFirstProjectsFolder() {
  TFilePathSet fps = getProjectsFolders();
  if (fps.empty())
    return TFilePath();
  else
    return *fps.begin();
}

TFilePath ToonzFolder::getLibraryFolder() {
  TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "LIBRARY");
  if (fp == TFilePath()) fp = getStuffDir() + "library";
  return fp;
}

TFilePath ToonzFolder::getStudioPaletteFolder() {
  TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "STUDIOPALETTE");
  if (fp == TFilePath()) fp = getStuffDir() + "studiopalette";
  return fp;
}

TFilePath ToonzFolder::getFxPresetFolder() {
  TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "FXPRESETS");
  if (fp == TFilePath()) fp = getStuffDir() + "fxs";
  return fp;
}

TFilePath ToonzFolder::getCacheRootFolder() {
  TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "CACHEROOT");
  if (fp == TFilePath()) fp = getStuffDir() + "cache";
  return fp;
}

TFilePath ToonzFolder::getProfileFolder() {
  TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "PROFILES");
  if (fp == TFilePath()) fp = getStuffDir() + "profiles";
  return fp;
}

TFilePath ToonzFolder::getReslistPath(bool forCleanup) {
  return getFirstProjectsFolder() +
         (forCleanup ? "cleanupreslist.txt" : "reslist.txt");
}

TFilePath ToonzFolder::getTemplateModuleDir() {
  // return getModulesDir() + getModuleName();
  return getModulesDir() + "settings";
}

TFilePath ToonzFolder::getMyModuleDir() {
  TFilePath fp(getTemplateModuleDir());
  return fp.withName(fp.getWideName() + L"." +
                     TSystem::getUserName().toStdWString());
}

TFilePath ToonzFolder::getModuleFile(TFilePath filename) {
  TFilePath fp = getMyModuleDir() + filename;
  if (TFileStatus(fp).doesExist()) return fp;
  fp = getTemplateModuleDir() + filename;
  return fp;
}

TFilePath ToonzFolder::getModuleFile(std::string fn) {
  return ToonzFolder::getModuleFile(TFilePath(fn));
}

// turtle
TFilePath ToonzFolder::getRoomsDir() {
  return getProfileFolder() + "layouts/rooms";
}

TFilePath ToonzFolder::getTemplateRoomsDir() {
  return getRoomsDir() +
         Preferences::instance()->getCurrentRoomChoice().toStdWString();
  // TFilePath fp(getMyModuleDir() + TFilePath(mySettingsFileName));
  // return getRoomsDir() + getModuleName();
}

TFilePath ToonzFolder::getMyRoomsDir() {
  // TFilePath fp(getTemplateRoomsDir());
  TFilePath fp(getProfileFolder());
  return fp.withName(
      fp.getWideName() + L"/layouts/personal/" +
      Preferences::instance()->getCurrentRoomChoice().toStdWString() + L"." +
      TSystem::getUserName().toStdWString());
}

TFilePath ToonzFolder::getRoomsFile(TFilePath filename) {
  TFilePath fp = getMyRoomsDir() + filename;
  if (TFileStatus(fp).doesExist()) return fp;
  fp = getTemplateRoomsDir() + filename;
  return fp;
}

TFilePath ToonzFolder::getRoomsFile(std::string fn) {
  return ToonzFolder::getRoomsFile(TFilePath(fn));
}

//===================================================================

FolderListenerManager::FolderListenerManager() {}

//-------------------------------------------------------------------

FolderListenerManager::~FolderListenerManager() {}

//-------------------------------------------------------------------

FolderListenerManager *FolderListenerManager::instance() {
  static FolderListenerManager _instance;
  return &_instance;
}

//-------------------------------------------------------------------

void FolderListenerManager::notifyFolderChanged(const TFilePath &path) {
  for (std::set<Listener *>::iterator i = m_listeners.begin();
       i != m_listeners.end(); ++i)
    (*i)->onFolderChanged(path);
}

//-------------------------------------------------------------------

void FolderListenerManager::addListener(Listener *listener) {
  m_listeners.insert(listener);
}

//-------------------------------------------------------------------

void FolderListenerManager::removeListener(Listener *listener) {
  m_listeners.erase(listener);
}
