

#include "toonz/toonzfolders.h"
#include "tsystem.h"
#include "tenv.h"
//#include "appmainshell.h"
#include "tconvert.h"
#include "toonz/preferences.h"


#ifdef _WIN32
#include <shlobj.h>
#include <Winnetwk.h>
#endif
#ifdef MACOSX
#include <Cocoa/Cocoa.h>
#endif

using namespace TEnv;

//-------------------------------------------------------------------


TFilePath getMyDocumentsPath() {
#ifdef _WIN32
	WCHAR szPath[MAX_PATH];
	if (SHGetSpecialFolderPathW(NULL, szPath, CSIDL_PERSONAL, 0)) {
		return TFilePath(szPath);
	}
	return TFilePath();
#elif defined MACOSX
	NSArray *foundref = NSSearchPathForDirectoriesInDomains(
		NSDocumentDirectory, NSUserDomainMask, YES);
	if (!foundref) return TFilePath();
	int c = [foundref count];
	assert(c == 1);
	NSString *documentsDirectory = [foundref objectAtIndex : 0];
	return TFilePath((const char *)[documentsDirectory
	cStringUsingEncoding : NSASCIIStringEncoding]);
#else
	return TFilePath();
#endif
}

// Desktop Path
TFilePath getDesktopPath() {
#ifdef _WIN32
	WCHAR szPath[MAX_PATH];
	if (SHGetSpecialFolderPathW(NULL, szPath, CSIDL_DESKTOP, 0)) {
		return TFilePath(szPath);
	}
	return TFilePath();
#elif defined MACOSX
	NSArray *foundref = NSSearchPathForDirectoriesInDomains(
		NSDesktopDirectory, NSUserDomainMask, YES);
	if (!foundref) return TFilePath();
	int c = [foundref count];
	assert(c == 1);
	NSString *desktopDirectory = [foundref objectAtIndex : 0];
	return TFilePath((const char *)[desktopDirectory
	cStringUsingEncoding : NSASCIIStringEncoding]);
#else
	return TFilePath();
#endif
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
	int documents = (projectPaths / 1000) % 10;
	int desktop = (projectPaths / 100) % 10;
	int stuff = (projectPaths / 10) % 10;
	int custom = projectPaths % 10;
	//fps = getSystemVarPathSetValue(getSystemVarPrefix() + "PROJECTS");
	if (documents) fps.push_back(getMyDocumentsPath() + "OpenToonz");
	if (desktop) fps.push_back(getDesktopPath() + "OpenToonz");
	if (stuff) fps.push_back(TEnv::getStuffDir() + "Projects");
	if (custom) {
		if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
			fps.push_back(TFilePath(path));
		}	
	}
	if (fps.empty()) fps.push_back(TEnv::getStuffDir() + "Projects");
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
