#pragma once

#ifndef TPARAMCONTAINER_INCLUDED
#define TPARAMCONTAINER_INCLUDED

#include <memory>

#include "tparam.h"
//#include "tfx.h"
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TIStream;
class TOStream;
class TParamObserver;
class TParam;

class DVAPI TParamVar {
  std::string m_name;
  bool m_isHidden;
  // Flag for an obsolete parameter used for maintaining backward-compatiblity.
  // - The obsolete parameter will call a special function
  // (TFx::onObsoleteParameterLoaded) on loaded which enables to do some special
  // action. (e.g. converting to a new parameter etc.)
  // - The obsolete parameter will not be saved.
  bool m_isObsolete;
  TParamObserver *m_paramObserver;

public:
  TParamVar(std::string name, bool hidden = false, bool obsolete = false)
      : m_name(name)
      , m_isHidden(hidden)
      , m_isObsolete(obsolete)
      , m_paramObserver(0) {}
  virtual ~TParamVar() {}
  virtual TParamVar *clone() const = 0;
  std::string getName() const { return m_name; }
  bool isHidden() const { return m_isHidden; }
  void setIsHidden(bool hidden) { m_isHidden = hidden; }
  bool isObsolete() const { return m_isObsolete; }
  virtual void setParam(TParam *param) = 0;
  virtual TParam *getParam() const     = 0;
  void setParamObserver(TParamObserver *obs);
};

template <class T>
class TParamVarT final : public TParamVar {
  // Very dirty fix for link fx, separating the variable between the plugin fx
  // and the built-in fx.
  // Note that for now link fx is available only with built-in fx, since m_var
  // must be "pointer to pointer" of parameter to make the link fx to work
  // properly.
  T *m_var            = nullptr;
  TParamP m_pluginVar = 0;

public:
  TParamVarT(std::string name, T *var = nullptr, TParamP pluginVar = 0,
             bool hidden = false, bool obsolete = false)
      : TParamVar(name, hidden), m_var(var), m_pluginVar(pluginVar) {}
  TParamVarT() = delete;
  void setParam(TParam *param) {
    if (m_var)
      *m_var = TParamP(param);
    else
      m_pluginVar = TParamP(param);
  }
  virtual TParam *getParam() const {
    if (m_var)
      return m_var->getPointer();
    else
      return m_pluginVar.getPointer();
  }
  TParamVar *clone() const {
    return new TParamVarT<T>(getName(), m_var, m_pluginVar, isHidden());
  }
};

class DVAPI TParamContainer {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TParamContainer();
  ~TParamContainer();

  void add(TParamVar *var);

  int getParamCount() const;

  bool isParamHidden(int index) const;

  TParam *getParam(int index) const;
  std::string getParamName(int index) const;
  TParam *getParam(std::string name) const;
  TParamVar *getParamVar(std::string name) const;
  const TParamVar *getParamVar(int index) const;

  void unlink();
  void link(const TParamContainer *src);
  void copy(const TParamContainer *src);

  void setParamObserver(TParamObserver *);
  TParamObserver *getParamObserver() const;

private:
  TParamContainer(const TParamContainer &);
  TParamContainer &operator=(const TParamContainer &);
};

#endif
