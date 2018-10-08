

#include "tenv.h"
#include "tsystem.h"
#include "tconvert.h"
#include "tfilepath_io.h"

#include <QDir>
#include <QSettings>

#ifdef LEVO_MACOSX

#include "macofflinegl.h"
#include "tofflinegl.h"

// Imposto l'offlineGL usando AGL (per togliere la dipendenza da X)

TOfflineGL::Imp *MacOfflineGenerator1(const TDimension &dim) {
  return new MacImplementation(dim);
}
#endif

#include <map>
#include <sstream>

using namespace TEnv;

//=========================================================
//
// root dir
//
//=========================================================

namespace {
const std::map<std::string, std::string> systemPathMap{
    {"LIBRARY", "library"},   {"STUDIOPALETTE", "studiopalette"},
    {"FXPRESETS", "fxs"},     {"CACHEROOT", "cache"},
    {"PROFILES", "profiles"}, {"CONFIG", "config"},
    {"PROJECTS", "projects"}};

class EnvGlobals {  // singleton

  std::string m_applicationName;
  std::string m_applicationVersion;
  std::string m_applicationVersionWithoutRevision;
  std::string m_applicationFullName;
  std::string m_moduleName;
  std::string m_rootVarName;
  std::string m_systemVarPrefix;
  std::string m_workingDirectory;
  TFilePath m_registryRoot;
  TFilePath m_envFile;
  TFilePath *m_stuffDir;
  TFilePath *m_dllRelativeDir;
  bool m_isPortable = false;

  // path values specified with command line arguments
  std::map<std::string, std::string> m_argPathValues;

  EnvGlobals() : m_stuffDir(0) { setWorkingDirectory(); }

public:
  ~EnvGlobals() { delete m_stuffDir; }

  static EnvGlobals *instance() {
    static EnvGlobals _instance;
    return &_instance;
  }

  TFilePath getSystemVarPath(std::string varName) {
#ifdef _WIN32
    return m_registryRoot + varName;
#else
    QString settingsPath;

#ifdef MACOSX
    settingsPath =
        QString::fromStdString(getApplicationName()) + QString("_") +
        QString::fromStdString(getApplicationVersionWithoutRevision()) +
        QString(".app") + QString("/Contents/Resources/SystemVar.ini");
#else /* Generic Unix */
    // TODO: use QStandardPaths::ConfigLocation when we drop Qt4
    settingsPath = QDir::homePath();
    settingsPath.append("/.config/");
    settingsPath.append(getApplicationName().c_str());
    settingsPath.append("/SystemVar.ini");
#endif

    QSettings settings(settingsPath, QSettings::IniFormat);
    QString qStr      = QString::fromStdString(varName);
    QString systemVar = settings.value(qStr).toString();
    // printf("getSystemVarPath: path:%s key:%s var:%s\n",
    // settingsPath.toStdString().data(), varName.data(),
    // systemVar.toStdString().data());
    return TFilePath(systemVar.toStdWString());
#endif
  }

  TFilePath getRootVarPath() { return getSystemVarPath(m_rootVarName); }

  std::string getSystemVarValue(std::string varName) {
    if (getIsPortable()) return "";
#ifdef _WIN32
    return TSystem::getSystemValue(getSystemVarPath(varName)).toStdString();
#else
    TFilePath systemVarPath = getSystemVarPath(varName);
    if (systemVarPath.isEmpty()) {
      std::cout << "varName:" << varName << " TOONZROOT not set..."
                << std::endl;
      return "";
    }
    return ::to_string(systemVarPath);
/*
                        char *value = getenv(varName.c_str());
                        if (!value)
                                {
                                std::cout << varName << " not set, returning
   TOONZROOT" << std::endl;
        //value = getenv("TOONZROOT");
                        value="";
                        std::cout << "!!!value= "<< value << std::endl;
                        if (!value)
                                        {
                                        std::cout << varName << "TOONZROOT not
   set..." << std::endl;
                                        //exit(-1);
                                        return "";
                                        }
                                }
      return string(value);
        */
#endif
  }

  TFilePath getStuffDir() {
    if (m_stuffDir) return *m_stuffDir;
    if (m_isPortable)
      return TFilePath((getWorkingDirectory() + "\\portablestuff\\"));

    return TFilePath(getSystemVarValue(m_rootVarName));
  }
  void setStuffDir(const TFilePath &stuffDir) {
    delete m_stuffDir;
    m_stuffDir = new TFilePath(stuffDir);
  }

  void updateEnvFile() {
    TFilePath profilesDir =
        getSystemVarPathValue(getSystemVarPrefix() + "PROFILES");
    if (profilesDir == TFilePath())
      profilesDir = getStuffDir() + systemPathMap.at("PROFILES");
    m_envFile =
        profilesDir + "env" + (TSystem::getUserName().toStdString() + ".env");
  }

  void setApplication(std::string applicationName,
                      std::string applicationVersion, std::string revision) {
    m_applicationName                   = applicationName;
    m_applicationVersionWithoutRevision = applicationVersion;
    if (!revision.empty()) {
      m_applicationVersion =
          m_applicationVersionWithoutRevision + "." + revision;
    } else {
      m_applicationVersion = m_applicationVersionWithoutRevision;
    }
    m_applicationFullName = m_applicationName + " " + m_applicationVersion;
    m_moduleName          = m_applicationName;
    m_rootVarName         = toUpper(m_applicationName) + "ROOT";
#ifdef _WIN32
    m_registryRoot = TFilePath("SOFTWARE\\OpenToonz\\") + m_applicationName +
                     applicationVersion;
#endif
    m_systemVarPrefix = m_applicationName;
    updateEnvFile();
  }

  std::string getApplicationName() { return m_applicationName; }
  std::string getApplicationVersion() { return m_applicationVersion; }
  std::string getApplicationVersionWithoutRevision() {
    return m_applicationVersionWithoutRevision;
  }

  TFilePath getEnvFile() { return m_envFile; }
  TFilePath getTemplateEnvFile() {
    return m_envFile.getParentDir() + TFilePath("template.env");
  }

  void setApplicationFullName(std::string applicationFullName) {
    m_applicationFullName = applicationFullName;
  }
  std::string getApplicationFullName() { return m_applicationFullName; }

  void setModuleName(std::string moduleName) { m_moduleName = moduleName; }
  std::string getModuleName() { return m_moduleName; }

  void setRootVarName(std::string varName) {
    m_rootVarName = varName;
    updateEnvFile();
  }
  std::string getRootVarName() { return m_rootVarName; }

  void setSystemVarPrefix(std::string prefix) {
    m_systemVarPrefix = prefix;
    updateEnvFile();
  }
  std::string getSystemVarPrefix() { return m_systemVarPrefix; }

  void setWorkingDirectory() {
    QString workingDirectoryTmp  = QDir::currentPath();
    QByteArray ba                = workingDirectoryTmp.toLatin1();
    const char *workingDirectory = ba.data();
    m_workingDirectory           = workingDirectory;

    // check if portable
    TFilePath portableCheck =
        TFilePath(m_workingDirectory + "\\portablestuff\\");
    TFileStatus portableStatus(portableCheck);
    m_isPortable = portableStatus.doesExist();
  }
  std::string getWorkingDirectory() { return m_workingDirectory; }

  bool getIsPortable() { return m_isPortable; }

  void setDllRelativeDir(const TFilePath &dllRelativeDir) {
    delete m_dllRelativeDir;
    m_dllRelativeDir = new TFilePath(dllRelativeDir);
  }

  TFilePath getDllRelativeDir() {
    if (m_dllRelativeDir) return *m_dllRelativeDir;
    return TFilePath(".");
  }

  void setArgPathValue(std::string key, std::string value) {
    m_argPathValues.emplace(key, value);
    if (key == m_systemVarPrefix + "PROFILES") updateEnvFile();
  }

  std::string getArgPathValue(std::string key) {
    decltype(m_argPathValues)::iterator it = m_argPathValues.find(key);
    if (it != m_argPathValues.end())
      return it->second;
    else
      return "";
  }
};

/*
TFilePath EnvGlobals::getSystemPath(int id)
{
  std::map<int, TFilePath>::iterator it = m_systemPaths.find(id);
  if(it != m_systemPaths.end()) return it->second;
  switch(id)
    {
     case StuffDir:        return TFilePath();
     case ConfigDir:       return getSystemPath(StuffDir) + "config";
     case ProfilesDir:      return getSystemPath(StuffDir) + "profiles";
     default: return TFilePath();
    }
}

void EnvGlobals::setSystemPath(int id, const TFilePath &fp)
{
  m_systemPaths[id] = fp;
}
*/

}  // namespace

//=========================================================
//
// Variable::Imp
//
//=========================================================

class Variable::Imp {
public:
  std::string m_name;
  std::string m_value;
  bool m_loaded, m_defaultDefined, m_assigned;

  Imp(std::string name)
      : m_name(name)
      , m_value("")
      , m_loaded(false)
      , m_defaultDefined(false)
      , m_assigned(false) {}
};

//=========================================================
//
// varaible manager (singleton)
//
//=========================================================

namespace {

class VariableSet {
  std::map<std::string, Variable::Imp *> m_variables;
  bool m_loaded;

public:
  VariableSet() : m_loaded(false) {}

  ~VariableSet() {
    std::map<std::string, Variable::Imp *>::iterator it;
    for (it = m_variables.begin(); it != m_variables.end(); ++it)
      delete it->second;
  }

  static VariableSet *instance() {
    static VariableSet instance;
    return &instance;
  }

  Variable::Imp *getImp(std::string name) {
    std::map<std::string, Variable::Imp *>::iterator it;
    it = m_variables.find(name);
    if (it == m_variables.end()) {
      Variable::Imp *imp = new Variable::Imp(name);
      m_variables[name]  = imp;
      return imp;
    } else
      return it->second;
  }

  void commit() {
    // save();
  }

  void loadIfNeeded() {
    if (m_loaded) return;
    m_loaded = true;
    try {
      load();
    } catch (...) {
    }
  }

  void load();
  void save();
};

//-------------------------------------------------------------------

void VariableSet::load() {
#ifndef WIN32
  EnvGlobals::instance()->updateEnvFile();
#endif
  TFilePath fp = EnvGlobals::instance()->getEnvFile();
  if (fp == TFilePath()) return;
  // if the personal env is not found, then try to find the template
  if (!TFileStatus(fp).doesExist())
    fp = EnvGlobals::instance()->getTemplateEnvFile();
  Tifstream is(fp);
  if (!is.isOpen()) return;
  char buffer[1024];
  while (is.getline(buffer, sizeof(buffer))) {
    char *s = buffer;
    while (*s == ' ') s++;
    char *t = s;
    while ('a' <= *s && *s <= 'z' || 'A' <= *s && *s <= 'Z' ||
           '0' <= *s && *s <= '9' || *s == '_')
      s++;
    std::string name(t, s - t);
    if (name.size() == 0) continue;
    while (*s == ' ') s++;
    if (*s != '\"') continue;
    s++;
    std::string value;
    while (*s != '\n' && *s != '\0' && *s != '\"') {
      if (*s != '\\')
        value.push_back(*s);
      else {
        s++;
        if (*s == '\\')
          value.push_back('\\');
        else if (*s == '"')
          value.push_back('"');
        else if (*s == 'n')
          value.push_back('\n');
        else
          continue;
      }
      s++;
    }
    Variable::Imp *imp = getImp(name);
    imp->m_value       = value;
    imp->m_loaded      = true;
  }
}

//-------------------------------------------------------------------

void VariableSet::save() {
  TFilePath fp = EnvGlobals::instance()->getEnvFile();
  if (fp == TFilePath()) return;
  bool exists = TFileStatus(fp.getParentDir()).doesExist();
  if (!exists) {
    try {
      TSystem::mkDir(fp.getParentDir());
    } catch (...) {
      return;
    }
  }
  Tofstream os(fp);
  if (!os) return;
  std::map<std::string, Variable::Imp *>::iterator it;
  for (it = m_variables.begin(); it != m_variables.end(); ++it) {
    os << it->first << " \"";
    std::string s = it->second->m_value;
    for (int i = 0; i < (int)s.size(); i++)
      if (s[i] == '\"')
        os << "\\\"";
      else if (s[i] == '\\')
        os << "\\\\";
      else if (s[i] == '\n')
        os << "\\n";
      else
        os.put(s[i]);
    os << "\"" << std::endl;
  }
}

//-------------------------------------------------------------------

}  // namespace

//=========================================================

Variable::Variable(std::string name)
    : m_imp(VariableSet::instance()->getImp(name)) {}

//-------------------------------------------------------------------

Variable::Variable(std::string name, std::string defaultValue)
    : m_imp(VariableSet::instance()->getImp(name)) {
  // assert(!m_imp->m_defaultDefined);
  m_imp->m_defaultDefined              = true;
  if (!m_imp->m_loaded) m_imp->m_value = defaultValue;
}

//-------------------------------------------------------------------

Variable::~Variable() {}

//-------------------------------------------------------------------

std::string Variable::getName() const { return m_imp->m_name; }

//-------------------------------------------------------------------

std::string Variable::getValue() const {
  VariableSet::instance()->loadIfNeeded();
  return m_imp->m_value;
}

//-------------------------------------------------------------------

void Variable::assignValue(std::string value) {
  VariableSet *vs = VariableSet::instance();
  vs->loadIfNeeded();
  m_imp->m_value = value;
  try {
    vs->commit();
  } catch (...) {
  }
}

//===================================================================

void TEnv::setApplication(std::string applicationName,
                          std::string applicationVersion,
                          std::string revision) {
  EnvGlobals::instance()->setApplication(applicationName, applicationVersion,
                                         revision);

#ifdef LEVO_MACOSX
  TOfflineGL::defineImpGenerator(MacOfflineGenerator1);
#endif
}

std::string TEnv::getApplicationName() {
  return EnvGlobals::instance()->getApplicationName();
}

std::string TEnv::getApplicationVersion() {
  return EnvGlobals::instance()->getApplicationVersion();
}

void TEnv::setApplicationFullName(std::string applicationFullName) {
  EnvGlobals::instance()->setApplicationFullName(applicationFullName);
}

std::string TEnv::getApplicationFullName() {
  return EnvGlobals::instance()->getApplicationFullName();
}

void TEnv::setModuleName(std::string moduleName) {
  EnvGlobals::instance()->setModuleName(moduleName);
}

std::string TEnv::getModuleName() {
  return EnvGlobals::instance()->getModuleName();
}

void TEnv::setRootVarName(std::string varName) {
  EnvGlobals::instance()->setRootVarName(varName);
}

std::string TEnv::getRootVarName() {
  return EnvGlobals::instance()->getRootVarName();
}

TFilePath TEnv::getRootVarPath() {
  return EnvGlobals::instance()->getRootVarPath();
}

std::string TEnv::getSystemVarStringValue(std::string varName) {
  return EnvGlobals::instance()->getSystemVarValue(varName);
}

TFilePath TEnv::getSystemVarPathValue(std::string varName) {
  EnvGlobals *eg = EnvGlobals::instance();
  // return if the path is registered by command line argument
  std::string argVar = eg->getArgPathValue(varName);
  if (argVar != "") return TFilePath(argVar);
  return TFilePath(eg->getSystemVarValue(varName));
}

TFilePathSet TEnv::getSystemVarPathSetValue(std::string varName) {
  TFilePathSet lst;
  EnvGlobals *eg = EnvGlobals::instance();
  // if the path is registered by command line argument, then use it
  std::string value      = eg->getArgPathValue(varName);
  if (value == "") value = eg->getSystemVarValue(varName);
  int len                = (int)value.size();
  int i                  = 0;
  int j                  = value.find(';');
  while (j != std::string::npos) {
    std::string s = value.substr(i, j - i);
    lst.push_back(TFilePath(s));
    i = j + 1;
    if (i >= len) return lst;
    j = value.find(';', i);
  }
  if (i < len) lst.push_back(TFilePath(value.substr(i)));
  return lst;
}

void TEnv::setSystemVarPrefix(std::string varName) {
  EnvGlobals::instance()->setSystemVarPrefix(varName);
}

std::string TEnv::getSystemVarPrefix() {
  return EnvGlobals::instance()->getSystemVarPrefix();
}

TFilePath TEnv::getStuffDir() {
  //#ifdef MACOSX
  // return TFilePath("/Applications/Toonz 5.0/Toonz 5.0 stuff");
  //#else
  return EnvGlobals::instance()->getStuffDir();
  //#endif
}

bool TEnv::getIsPortable() { return EnvGlobals::instance()->getIsPortable(); }

TFilePath TEnv::getConfigDir() {
  TFilePath configDir = getSystemVarPathValue(getSystemVarPrefix() + "CONFIG");
  if (configDir == TFilePath())
    configDir = getStuffDir() + systemPathMap.at("CONFIG");
  return configDir;
}

/*TFilePath TEnv::getProfilesDir()
{
  TFilePath fp(getStuffDir());
  return fp != TFilePath() ? fp + "profiles" : fp;
}
*/
void TEnv::setStuffDir(const TFilePath &stuffDir) {
  EnvGlobals::instance()->setStuffDir(stuffDir);
}

TFilePath TEnv::getDllRelativeDir() {
  return EnvGlobals::instance()->getDllRelativeDir();
}

void TEnv::setDllRelativeDir(const TFilePath &dllRelativeDir) {
  EnvGlobals::instance()->setDllRelativeDir(dllRelativeDir);
}

void TEnv::saveAllEnvVariables() { VariableSet::instance()->save(); }

bool TEnv::setArgPathValue(std::string key, std::string value) {
  EnvGlobals *eg = EnvGlobals::instance();
  // in case of "-TOONZROOT" , set the all unregistered paths
  if (key == getRootVarName()) {
    TFilePath rootPath(value);
    eg->setStuffDir(rootPath);
    for (auto itr = systemPathMap.begin(); itr != systemPathMap.end(); ++itr) {
      std::string k   = getSystemVarPrefix() + (*itr).first;
      std::string val = value + "\\" + (*itr).second;
      // set all unregistered values
      if (eg->getArgPathValue(k) == "") eg->setArgPathValue(k, val);
    }
    return true;
  } else {
    for (auto itr = systemPathMap.begin(); itr != systemPathMap.end(); ++itr) {
      // found the corresponding registry key
      if (key == getSystemVarPrefix() + (*itr).first) {
        eg->setArgPathValue(key, value);
        return true;
      }
    }
    // registry key not found. failed to register
    return false;
  }
}

const std::map<std::string, std::string> &TEnv::getSystemPathMap() {
  return systemPathMap;
}

/*
void TEnv::defineSystemPath(SystemFileId id, const TFilePath &registryName)
{
  string s = TSystem::getSystemValue(registryName);
  if(s=="") return;
  EnvGlobals::instance()->setSystemPath(id, TFilePath(s));
}

//---------------------------------------------------------


TFilePath TEnv::getSystemPath(SystemFileId id)
{
  return EnvGlobals::instance()->getSystemPath(id);
}
*/

//=========================================================
//
// Variabili tipizzate
//
//=========================================================

namespace {

std::istream &operator>>(std::istream &is, TFilePath &path) {
  std::string s;
  is >> s;
  return is;
}

std::istream &operator>>(std::istream &is, TRect &rect) {
  return is >> rect.x0 >> rect.y0 >> rect.x1 >> rect.y1;
}

template <class T>
std::string toString2(T value) {
  std::ostringstream ss;
  ss << value << '\0';
  return ss.str();
}

template <>
std::string toString2(TRect value) {
  std::ostringstream ss;
  ss << value.x0 << " " << value.y0 << " " << value.x1 << " " << value.y1
     << '\0';
  return ss.str();
}

template <class T>
void fromString(std::string s, T &value) {
  if (s.empty()) return;
  std::istringstream is(s);
  is >> value;
}

void fromString(std::string s, std::string &value) { value = s; }

}  // namespace

//-------------------------------------------------------------------

IntVar::IntVar(std::string name, int defValue)
    : Variable(name, std::to_string(defValue)) {}
IntVar::IntVar(std::string name) : Variable(name) {}
IntVar::operator int() const {
  int v;
  fromString(getValue(), v);
  return v;
}
void IntVar::operator=(int v) { assignValue(std::to_string(v)); }

//-------------------------------------------------------------------

DoubleVar::DoubleVar(std::string name, double defValue)
    : Variable(name, std::to_string(defValue)) {}
DoubleVar::DoubleVar(std::string name) : Variable(name) {}
DoubleVar::operator double() const {
  double v;
  fromString(getValue(), v);
  return v;
}
void DoubleVar::operator=(double v) { assignValue(std::to_string(v)); }

//-------------------------------------------------------------------

StringVar::StringVar(std::string name, const std::string &defValue)
    : Variable(name, defValue) {}
StringVar::StringVar(std::string name) : Variable(name) {}
StringVar::operator std::string() const {
  std::string v;
  fromString(getValue(), v);
  return v;
}
void StringVar::operator=(const std::string &v) { assignValue(v); }

//-------------------------------------------------------------------

FilePathVar::FilePathVar(std::string name, const TFilePath &defValue)
    : Variable(name, ::to_string(defValue)) {}
FilePathVar::FilePathVar(std::string name) : Variable(name) {}
FilePathVar::operator TFilePath() const {
  std::string v;
  fromString(getValue(), v);
  return TFilePath(v);
}
void FilePathVar::operator=(const TFilePath &v) { assignValue(::to_string(v)); }

//-------------------------------------------------------------------

RectVar::RectVar(std::string name, const TRect &defValue)
    : Variable(name, toString2(defValue)) {}
RectVar::RectVar(std::string name) : Variable(name) {}
RectVar::operator TRect() const {
  TRect v;
  fromString(getValue(), v);
  return v;
}
void RectVar::operator=(const TRect &v) { assignValue(toString2(v)); }

//=========================================================
