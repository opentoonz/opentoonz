

#include "toonz/toonzfolders.h"
#include "tsystem.h"
#include "tenv.h"
//#include "appmainshell.h"
#include "tconvert.h"

using namespace TEnv;

TFilePath ToonzFolder::getModulesDir()
{
	return getProfileFolder() + "layouts";
}

TFilePathSet ToonzFolder::getProjectsFolders()
{
	TFilePathSet fps = getSystemVarPathSetValue(getSystemVarPrefix() + "PROJECTS");
	if (fps.empty())
		fps.push_back(TEnv::getStuffDir() + "Projects");
	return fps;
}

TFilePath ToonzFolder::getFirstProjectsFolder()
{
	TFilePathSet fps = getProjectsFolders();
	if (fps.empty())
		return TFilePath();
	else
		return *fps.begin();
}

TFilePath ToonzFolder::getLibraryFolder()
{
	TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "LIBRARY");
	if (fp == TFilePath())
		fp = getStuffDir() + "library";
	return fp;
}

TFilePath ToonzFolder::getStudioPaletteFolder()
{
	TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "STUDIOPALETTE");
	if (fp == TFilePath())
		fp = getStuffDir() + "studiopalette";
	return fp;
}

TFilePath ToonzFolder::getFxPresetFolder()
{
	TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "FXPRESETS");
	if (fp == TFilePath())
		fp = getStuffDir() + "fxs";
	return fp;
}

TFilePath ToonzFolder::getCacheRootFolder()
{
	TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "CACHEROOT");
	if (fp == TFilePath())
		fp = getStuffDir() + "cache";
	return fp;
}

TFilePath ToonzFolder::getProfileFolder()
{
	TFilePath fp = getSystemVarPathValue(getSystemVarPrefix() + "PROFILES");
	if (fp == TFilePath())
		fp = getStuffDir() + "profiles";
	return fp;
}

TFilePath ToonzFolder::getReslistPath(bool forCleanup)
{
	return getFirstProjectsFolder() + (forCleanup ? "cleanupreslist.txt" : "reslist.txt");
}

TFilePath ToonzFolder::getTemplateModuleDir()
{
	return getModulesDir() + getModuleName();
}

TFilePath ToonzFolder::getMyModuleDir()
{
	TFilePath fp(getTemplateModuleDir());
	return fp.withName(fp.getWideName() + L"." + TSystem::getUserName().toStdWString());
}

TFilePath ToonzFolder::getModuleFile(TFilePath filename)
{
	TFilePath fp = getMyModuleDir() + filename;
	if (TFileStatus(fp).doesExist())
		return fp;
	fp = getTemplateModuleDir() + filename;
	return fp;
}

TFilePath ToonzFolder::getModuleFile(std::string fn)
{
	return ToonzFolder::getModuleFile(TFilePath(fn));
}

//===================================================================

FolderListenerManager::FolderListenerManager()
{
}

//-------------------------------------------------------------------

FolderListenerManager::~FolderListenerManager()
{
}

//-------------------------------------------------------------------

FolderListenerManager *FolderListenerManager::instance()
{
	static FolderListenerManager _instance;
	return &_instance;
}

//-------------------------------------------------------------------

void FolderListenerManager::notifyFolderChanged(const TFilePath &path)
{
	for (std::set<Listener *>::iterator i = m_listeners.begin(); i != m_listeners.end(); ++i)
		(*i)->onFolderChanged(path);
}

//-------------------------------------------------------------------

void FolderListenerManager::addListener(Listener *listener)
{
	m_listeners.insert(listener);
}

//-------------------------------------------------------------------

void FolderListenerManager::removeListener(Listener *listener)
{
	m_listeners.erase(listener);
}
