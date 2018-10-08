#pragma once

#ifndef TENV_INCLUDED
#define TENV_INCLUDED

//#include "texception.h"
#include "tgeometry.h"
#include "tfilepath.h"

//===================================================================

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================
namespace TEnv {

//-------------------------------------------------------

class DVAPI Variable {
public:
  class Imp;
  Imp *m_imp;

public:
  Variable(std::string name, std::string defaultValue);
  Variable(std::string name);
  virtual ~Variable();

  std::string getName() const;
  std::string getValue() const;

  void assignValue(std::string str);
};

class DVAPI IntVar final : public Variable {
public:
  IntVar(std::string name, int defValue);
  IntVar(std::string name);
  operator int() const;
  void operator=(int v);
};

class DVAPI DoubleVar final : public Variable {
public:
  DoubleVar(std::string name, double defValue);
  DoubleVar(std::string name);
  operator double() const;
  void operator=(double v);
};

class DVAPI StringVar final : public Variable {
public:
  StringVar(std::string name, const std::string &defValue);
  StringVar(std::string name);
  operator std::string() const;
  void operator=(const std::string &v);
};

class DVAPI FilePathVar final : public Variable {
public:
  FilePathVar(std::string name, const TFilePath &defValue);
  FilePathVar(std::string name);
  operator TFilePath() const;
  void operator=(const TFilePath &v);
};

class DVAPI RectVar final : public Variable {
public:
  RectVar(std::string name, const TRect &defValue);
  RectVar(std::string name);
  operator TRect() const;
  void operator=(const TRect &v);
};

//-------------------------------------------------------

// NOTA BENE: bisogna chiamare setApplication() il prima possibile
// questa operazione inizializza il registry root (su Windows) e definisce il
// file dove
// vengono lette e scritte le variabili di environment
//
// es.:  TEnv::setApplication("Toonz","5.0");
//
DVAPI void setApplication(std::string applicationName, std::string version,
                          std::string revision = std::string());

DVAPI std::string getApplicationName();
DVAPI std::string getApplicationVersion();

DVAPI bool getIsPortable();

// es.: TEnv::setModuleFullName("Toonz 5.0.1 Harlequin");
// (default: "<applicationName> <applicationVersion>")
DVAPI void setApplicationFullName(std::string applicationFullName);
DVAPI std::string getApplicationFullName();

// es.: TEnv::setModuleName("inknpaint")
// (default: "<applicationName>")
DVAPI void setModuleName(std::string moduleName);
DVAPI std::string getModuleName();

// es.: TEnv::setRootVarName("TABROOT");
// (default: toUpper(<applicationName> + "ROOT"))
DVAPI std::string getRootVarName();
DVAPI void setRootVarName(std::string varName);

// es.: TEnv::setRootVarName("Toonz");
// (default: <programName>)
DVAPI void setSystemVarPrefix(std::string prefix);
DVAPI std::string getSystemVarPrefix();

// su Windows ritorna
//   'SOFTWARE\Digital Video\<applicationName>\<applicationVersion>\<rootvar>'
// su Unix/Linux/MacOsX
//   '<rootvar>'
DVAPI TFilePath getRootVarPath();

// restituisce il valore della variabile di sistema varName
// (su window aggiunge "SOFTWARE\Digital Video\....." all'inizio)
DVAPI std::string getSystemVarStringValue(std::string varName);
DVAPI TFilePath getSystemVarPathValue(std::string varName);
DVAPI TFilePathSet getSystemVarPathSetValue(std::string varName);

DVAPI TFilePath getStuffDir();
DVAPI TFilePath getConfigDir();
// DVAPI TFilePath getProfilesDir();

// per l'utilizzo di ToonzLib senza che sia definita una TOONZROOT
// bisogna chiamare TEnv::setStuffDir(stuffdir) prima di ogni altra operazione
DVAPI void setStuffDir(const TFilePath &stuffDir);

DVAPI TFilePath getDllRelativeDir();
DVAPI void setDllRelativeDir(const TFilePath &dllRelativeDir);

DVAPI void saveAllEnvVariables();

// register command line argument paths.
// returns true on success
DVAPI bool setArgPathValue(std::string key, std::string value);

DVAPI const std::map<std::string, std::string> &getSystemPathMap();

/*

  enum SystemFileId {
    StuffDir,
    ConfigDir,
    ProfilesDir
  };



  // bisogna assegnare (in qualsiasi ordine) tutti i vari nomi dei registry alle
  varie
  // directory di sistema.
  // StuffDir e' obbligatoria, tutte le altre hanno un valore di default
  relativo a StuffDir
  DVAPI void defineSystemPath(SystemFileId id, const TFilePath &registryName);

  // restituisce il file (non la variabile di registro)
  DVAPI TFilePath getSystemPath(SystemFileId id);
*/

//=========================================================

//=========================================================

}  // namespace TEnv

//=========================================================

#endif
