

#include "tpluginmanager.h"
#include "tsystem.h"
#include "tconvert.h"
#include "tlogger.h"

#ifdef _WIN32
#include <windows.h>
#else

// QUALE DI QUESTI SERVE VERAMENTE??
#include <grp.h>
#include <utime.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/types.h>
#ifndef FREEBSD
#include <dirent.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/dir.h>
#include <sys/param.h>  // for getfsstat
#ifdef MACOSX
#include <sys/ucred.h>
#endif
#include <sys/mount.h>
#include <pwd.h>
#include <dlfcn.h>
#endif

//-----------------------------------------------------------------------------

class TPluginManager::Plugin {
public:
#ifdef _WIN32
  typedef HINSTANCE Handle;
#else
  typedef void *Handle;
#endif

private:
  Handle m_handle;
  TPluginInfo m_info;

public:
  Plugin(Handle handle) : m_handle(handle) {}

  Handle getHandle() const { return m_handle; }
  const TPluginInfo &getInfo() const { return m_info; }
  void setInfo(const TPluginInfo &info) { m_info = info; }
  std::string getName() const { return m_info.getName(); }
};

//-----------------------------------------------------------------------------

typedef const TPluginInfo *TnzLibMainProcType();

//--------------------------------------------------------------

namespace {
const char *TnzLibMainProcName = "TLibMain";
#if !defined(_WIN32)
const char *TnzLibMainProcName2 = "_TLibMain";
#endif
}

//=============================================================================

TPluginManager::TPluginManager() { m_ignoreList.insert("tnzimagevector"); }

//-----------------------------------------------------------------------------

TPluginManager::~TPluginManager() {
  //   try { unloadPlugins(); } catch(...) {}
}

//-----------------------------------------------------------------------------

TPluginManager *TPluginManager::instance() {
  static TPluginManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

bool TPluginManager::isIgnored(std::string name) const {
  return m_ignoreList.count(toLower(name)) > 0;
}

//-----------------------------------------------------------------------------

void TPluginManager::unloadPlugins() {
  for (PluginTable::iterator it = m_pluginTable.begin();
       it != m_pluginTable.end(); ++it) {
    Plugin::Handle handle = (*it)->getHandle();
#if !(defined(LINUX) || defined(FREEBSD))
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
#endif
    delete (*it);
  }
  m_pluginTable.clear();
}

//--------------------------------------------------------------

void TPluginManager::loadPlugin(const TFilePath &fp) {
  if ((int)m_loadedPlugins.count(fp) > 0) {
    TLogger::debug() << "Already loaded " << fp;
    return;
  }
  std::string name = fp.getName();
  if (isIgnored(name)) {
    TLogger::debug() << "Ignored " << fp;
    return;
  }

  TLogger::debug() << "Loading " << fp;
#ifdef _WIN32
  Plugin::Handle handle = LoadLibraryW(fp.getWideString().c_str());
#else
  Plugin::Handle handle =
      dlopen(::to_string(fp).c_str(), RTLD_NOW);  // RTLD_LAZY
#endif
  if (!handle) {
    // non riesce a caricare la libreria;
    TLogger::warning() << "Unable to load " << fp;
#ifdef _WIN32
    std::wstring getFormattedMessage(DWORD lastError);
    TLogger::warning() << ::to_string(getFormattedMessage(GetLastError()));
#else
    TLogger::warning() << dlerror();
#endif
  } else {
    m_loadedPlugins.insert(fp);
    Plugin *plugin = new Plugin(handle);
    m_pluginTable.push_back(plugin);
    // cout << "loaded" << endl;
    TnzLibMainProcType *tnzLibMain = 0;
#ifdef _WIN32
    tnzLibMain =
        (TnzLibMainProcType *)GetProcAddress(handle, TnzLibMainProcName);
#else
    tnzLibMain = (TnzLibMainProcType *)dlsym(handle, TnzLibMainProcName);
    if (!tnzLibMain)  // provo _ come prefisso
      tnzLibMain = (TnzLibMainProcType *)dlsym(handle, TnzLibMainProcName2);
#endif

    if (!tnzLibMain) {
      // La libreria non esporta TLibMain;
      TLogger::warning() << "Corrupted " << fp;

#ifdef _WIN32
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
    } else {
      const TPluginInfo *info = tnzLibMain();
      if (info) plugin->setInfo(*info);
    }
  }
}

//--------------------------------------------------------------

void TPluginManager::loadPlugins(const TFilePath &dir) {
#if defined(_WIN32)
  const std::string extension = "dll";
#elif defined(LINUX) || defined(FREEBSD) || defined(__sgi)
  const std::string extension = "so";
#elif defined(MACOSX)
  const std::string extension = "dylib";
#endif

  TFilePathSet dirContent = TSystem::readDirectory(dir, false);
  if (dirContent.empty()) return;

  for (TFilePathSet::iterator it = dirContent.begin(); it != dirContent.end();
       it++) {
    TFilePath fp = *it;
    if (fp.getType() != extension) continue;
    std::wstring fullpath = fp.getWideString();

#ifdef _WIN32

    bool isDebugLibrary =
        (fullpath.find(L".d.") == fullpath.size() - (extension.size() + 3));

#ifdef _DEBUG
    if (!isDebugLibrary)
#else
    if (isDebugLibrary)
#endif
      continue;

#endif

    try {
      loadPlugin(fp);
    } catch (...) {
      TLogger::warning() << "unexpected error loading " << fp;
    }
  }
}

//--------------------------------------------------------------

void TPluginManager::loadStandardPlugins() {
  TFilePath pluginsDir = TSystem::getDllDir() + "plugins";

  // loadPlugins(pluginsDir + "io");
  loadPlugins(pluginsDir + "fx");
}

//--------------------------------------------------------------

void TPluginManager::setIgnoredList(const std::set<std::string> &names) {
  m_ignoreList.clear();
  for (std::set<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it)
    m_ignoreList.insert(toLower(*it));
}
