#pragma once

#ifndef TPLUGINMANAGER_INCLUDED
#define TPLUGINMANAGER_INCLUDED

//
// TPluginManager
//
// usage example. Main program:
//
//    TPluginManager::instance()->setIgnored("tnzimage");
//    TPluginManager::instance()->loadStandardPlugins();
//
// N.B. "tnzimagevector" is ignored by default
//
// Plugin :
//
//    TPluginInfo info("pluginName");
//    TLIBMAIN
//    {
//       ....
//       return &info;
//    }
//

//#include "tfilepath.h"
#include "tcommon.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TSYSTEM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFilePath;

//-----------------------------------------------------------------------------

class DVAPI TPluginInfo {
  std::string m_name;

public:
  TPluginInfo(std::string name = "") : m_name(name){};
  ~TPluginInfo(){};
  std::string getName() const { return m_name; };
};

//-----------------------------------------------------------------------------
//
// L'entry point del plugin e' TLIBMAIN {....}
//
#ifdef _WIN32
#define TLIBMAIN extern "C" __declspec(dllexport) const TPluginInfo *TLibMain()
#else
#define TLIBMAIN extern "C" const TPluginInfo *TLibMain()
#endif

//-----------------------------------------------------------------------------

class DVAPI TPluginManager {  // singleton

  class Plugin;

  std::set<std::string> m_ignoreList;
  typedef std::vector<const Plugin *> PluginTable;
  PluginTable m_pluginTable;
  std::set<TFilePath> m_loadedPlugins;

  TPluginManager();

public:
  ~TPluginManager();
  static TPluginManager *instance();

  // the name should be ignored? (name only; case insensitive. e.g. "tnzimage")
  bool isIgnored(std::string name) const;

  // set names to ignore; clear previous list
  void setIgnoredList(const std::set<std::string> &lst);

  // helper method.
  void setIgnored(std::string name) {
    std::set<std::string> lst;
    lst.insert(name);
    setIgnoredList(lst);
  }

  // try to load plugin specified by fp; check if already loaded
  void loadPlugin(const TFilePath &fp);

  // load all plugins in dir
  void loadPlugins(const TFilePath &dir);

  // load all plugins in <bin>/plugins/io and <bin>/plugins/fx
  void loadStandardPlugins();

  // unload plugins (automatically called atexit)
  void unloadPlugins();
};

#endif
