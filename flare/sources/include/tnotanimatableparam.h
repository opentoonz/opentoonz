#pragma once

#ifndef TNOTANIMATABLEPARAM_H
#define TNOTANIMATABLEPARAM_H

#include <memory>

#include "tparam.h"
#include "tparamchange.h"
#include "tfilepath.h"
#include "texception.h"
#include "tundo.h"
#include "tconvert.h"

#include <set>
#include <QFont>

#undef DVAPI
#undef DVVAR
#ifdef TPARAM_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChange
//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

template <class T>
class TNotAnimatableParamChange final : public TParamChange {
  T m_oldValue;
  T m_newValue;

public:
  TNotAnimatableParamChange(TParam *param, const T &oldValue, const T &newValue,
                            bool undoing)
      : TParamChange(param, TParamChange::m_minFrame, TParamChange::m_maxFrame,
                     false, false, undoing)
      , m_oldValue(oldValue)
      , m_newValue(newValue) {}

  TParamChange *clone() const {
    return new TNotAnimatableParamChange<T>(*this);
  }
  TUndo *createUndo() const;
};

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChangeUndo
//-----------------------------------------------------------------------------

template <class T>
class TNotAnimatableParamChangeUndo final : public TUndo {
public:
  TNotAnimatableParamChangeUndo(TParam *param, const T &oldValue,
                                const T &newValue);
  ~TNotAnimatableParamChangeUndo();
  void undo() const override;
  void redo() const override;
  int getSize() const override;

private:
  TParam *m_param;
  T m_oldValue;
  T m_newValue;
};

//-----------------------------------------------------------------------------
//  TNotAnimatableParamObserver
//-----------------------------------------------------------------------------

template <class T>
class TNotAnimatableParamObserver : public TParamObserver {
public:
  TNotAnimatableParamObserver() {}

  void onChange(const TParamChange &) override = 0;
  void onChange(const TNotAnimatableParamChange<T> &change) {
    onChange(static_cast<const TParamChange &>(change));
  }
};

typedef TNotAnimatableParamObserver<int> TIntParamObserver;
typedef TNotAnimatableParamObserver<bool> TBoolParamObserver;
typedef TNotAnimatableParamObserver<TFilePath> TFilePathParamObserver;

//-----------------------------------------------------------------------------
//    TNotAnimatableParam base class
//-----------------------------------------------------------------------------

template <class T>
class DVAPI TNotAnimatableParam : public TParam {
  T m_defaultValue, m_value;

protected:
  std::set<TNotAnimatableParamObserver<T> *> m_observers;
  std::set<TParamObserver *> m_paramObservers;

public:
  TNotAnimatableParam(T def = T())
      : TParam(), m_defaultValue(def), m_value(def){};
  TNotAnimatableParam(const TNotAnimatableParam &src)
      : TParam(src.getName())
      , m_defaultValue(src.getDefaultValue())
      , m_value(src.getValue()){};
  ~TNotAnimatableParam(){};

  T getValue() const { return m_value; }
  T getDefaultValue() const { return m_defaultValue; }
  void setValue(T v, bool undoing = false) {
    if (m_value == v) return;

    TNotAnimatableParamChange<T> change(this, m_value, v, undoing);
    m_value = v;
    for (typename std::set<TNotAnimatableParamObserver<T> *>::iterator obsIt =
             m_observers.begin();
         obsIt != m_observers.end(); ++obsIt)
      (*obsIt)->onChange(change);
    for (std::set<TParamObserver *>::iterator parObsIt =
             m_paramObservers.begin();
         parObsIt != m_paramObservers.end(); ++parObsIt)
      (*parObsIt)->onChange(change);
  }

  void setDefaultValue(T value) { m_defaultValue = value; }
  void copy(TParam *src) override {
    TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(src);
    if (!p) throw TException("invalid source for copy");
    setName(src->getName());
    m_defaultValue = p->m_defaultValue;
    m_value        = p->m_value;
  }

  void reset(bool undoing = false) { setValue(m_defaultValue, undoing); }

  void addObserver(TParamObserver *observer) override {
    TNotAnimatableParamObserver<T> *obs =
        dynamic_cast<TNotAnimatableParamObserver<T> *>(observer);
    if (obs)
      m_observers.insert(obs);
    else
      m_paramObservers.insert(observer);
  }
  void removeObserver(TParamObserver *observer) override {
    TNotAnimatableParamObserver<T> *obs =
        dynamic_cast<TNotAnimatableParamObserver<T> *>(observer);
    if (obs)
      m_observers.erase(obs);
    else
      m_paramObservers.erase(observer);
  }

  bool isAnimatable() const override { return false; }
  bool isKeyframe(double) const override { return false; }
  void deleteKeyframe(double) override {}
  void clearKeyframes() override {}
  void assignKeyframe(double, const TSmartPointerT<TParam> &, double,
                      bool) override {}

  std::string getValueAlias(double, int) override {
    using namespace std;
    return to_string(getValue());
  }

  bool hasKeyframes() const override { return 0; };
  void getKeyframes(std::set<double> &) const override{};
  int getNextKeyframe(double) const override { return -1; };
  int getPrevKeyframe(double) const override { return -1; };
};

//=========================================================
//
//  class TIntParam
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<int>;
class TIntParam;
template class DVAPI TPersistDeclarationT<TIntParam>;
#endif

class DVAPI TIntParam final : public TNotAnimatableParam<int> {
  PERSIST_DECLARATION(TIntParam);
  int minValue, maxValue;
  bool m_isWheelEnabled;

public:
  TIntParam(int v = int())
      : TNotAnimatableParam<int>(v)
      , minValue(-(std::numeric_limits<int>::max)())
      , maxValue((std::numeric_limits<int>::max)())
      , m_isWheelEnabled(false) {}
  TIntParam(const TIntParam &src) : TNotAnimatableParam<int>(src) {}
  TParam *clone() const override { return new TIntParam(*this); }
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  void enableWheel(bool on);

  bool isWheelEnabled() const;
  void setValueRange(int min, int max);
  bool getValueRange(int &min, int &max) const;
};

DEFINE_PARAM_SMARTPOINTER(TIntParam, int)

//=========================================================
//
//  class TBoolParam
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<bool>;
class TBoolParam;
template class DVAPI TPersistDeclarationT<TBoolParam>;
#endif

class DVAPI TBoolParam final : public TNotAnimatableParam<bool> {
  PERSIST_DECLARATION(TBoolParam);

public:
  TBoolParam(bool v = bool()) : TNotAnimatableParam<bool>(v) {}
  TBoolParam(const TBoolParam &src) : TNotAnimatableParam<bool>(src) {}

  TParam *clone() const override { return new TBoolParam(*this); }

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
};

DEFINE_PARAM_SMARTPOINTER(TBoolParam, bool)

//=========================================================
//
//  class TFilePathParam
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<TFilePath>;
class TFilePathParam;
template class DVAPI TPersistDeclarationT<TFilePathParam>;
#endif

class DVAPI TFilePathParam final : public TNotAnimatableParam<TFilePath> {
  PERSIST_DECLARATION(TFilePathParam);

public:
  TFilePathParam(const TFilePath &v = TFilePath())
      : TNotAnimatableParam<TFilePath>(v) {}
  TFilePathParam(const TFilePathParam &src)
      : TNotAnimatableParam<TFilePath>(src) {}
  TParam *clone() const override { return new TFilePathParam(*this); }
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
};

DEFINE_PARAM_SMARTPOINTER(TFilePathParam, TFilePath)

//=========================================================
//
//  class TStringParam
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<std::wstring>;
class TStringParam;
template class DVAPI TPersistDeclarationT<TStringParam>;
#endif

class DVAPI TStringParam final : public TNotAnimatableParam<std::wstring> {
  PERSIST_DECLARATION(TStringParam);

  bool m_multiLine = false;

public:
  TStringParam(std::wstring v = L"") : TNotAnimatableParam<std::wstring>(v) {}
  TStringParam(const TStringParam &src)
      : TNotAnimatableParam<std::wstring>(src) {}
  TParam *clone() const override { return new TStringParam(*this); }
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  void setMultiLineEnabled(bool enable) { m_multiLine = enable; }
  bool isMultiLineEnabled() { return m_multiLine; }
};

DEFINE_PARAM_SMARTPOINTER(TStringParam, std::wstring)

//=========================================================
//
//  class TEnumParam
//
//=========================================================

class TEnumParamImp;

class DVAPI TEnumParam final : public TNotAnimatableParam<int> {
  PERSIST_DECLARATION(TEnumParam)

public:
  TEnumParam(const int &v, const std::string &caption);

  TEnumParam();

  TEnumParam(const TEnumParam &src);
  ~TEnumParam();

  TParam *clone() const override { return new TEnumParam(*this); }
  void copy(TParam *src) override;

  void setValue(int v, bool undoing = false);
  void setValue(const std::string &caption, bool undoing = false);

  void addItem(const int &item, const std::string &caption);

  int getItemCount() const;
  void getItem(int i, int &item, std::string &caption) const;

  // TPersist methods
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

private:
  std::unique_ptr<TEnumParamImp> m_imp;
};

typedef TEnumParam TIntEnumParam;

typedef TNotAnimatableParamObserver<TIntEnumParam> TIntEnumParamObserver;

DVAPI_PARAM_SMARTPOINTER(TIntEnumParam)

class DVAPI TIntEnumParamP final
    : public TDerivedSmartPointerT<TIntEnumParam, TParam> {
public:
  TIntEnumParamP(TIntEnumParam *p = 0) : DerivedSmartPointer(p) {}
  TIntEnumParamP(int v, const std::string &caption)
      : DerivedSmartPointer(new TEnumParam(v, caption)) {}
  TIntEnumParamP(TParamP &p) : DerivedSmartPointer(p) {}
  TIntEnumParamP(const TParamP &p) : DerivedSmartPointer(p) {}
  operator TParamP() const { return TParamP(m_pointer); }
};

//------------------------------------------------------------------------------

//=========================================================
//
//  class TNADoubleParam   //is a not animatable double param
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<double>;
class TNADoubleParam;
template class DVAPI TPersistDeclarationT<TNADoubleParam>;
#endif

class DVAPI TNADoubleParam final : public TNotAnimatableParam<double> {
  PERSIST_DECLARATION(TNADoubleParam);

public:
  TNADoubleParam(double v = double())
      : TNotAnimatableParam<double>(v), m_min(0.), m_max(100.) {}
  TNADoubleParam(const TNADoubleParam &src)
      : TNotAnimatableParam<double>(src) {}
  TParam *clone() const override { return new TNADoubleParam(*this); }
  void setValueRange(double min, double max) {
    m_min = min;
    m_max = max;
  }
  void setValue(double v, bool undoing = false) {
    notMoreThan(m_max, v);
    notLessThan(m_min, v);
    TNotAnimatableParam<double>::setValue(v, undoing);
  }
  bool getValueRange(double &min, double &max) const {
    min = m_min;
    max = m_max;
    return min < max;
  }

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

private:
  double m_min, m_max;
};

DEFINE_PARAM_SMARTPOINTER(TNADoubleParam, double)

//=========================================================
//
//  class TFontParam
//
//=========================================================

#ifdef _WIN32
template class DVAPI TNotAnimatableParam<std::wstring>;
class TFontParam;
template class DVAPI TPersistDeclarationT<TFontParam>;
#endif

class DVAPI TFontParam final : public TNotAnimatableParam<std::wstring> {
  PERSIST_DECLARATION(TFontParam);

public:
  TFontParam(std::wstring v = QFont().toString().toStdWString())
      : TNotAnimatableParam<std::wstring>(v) {}
  TFontParam(const TFontParam &src) : TNotAnimatableParam<std::wstring>(src) {}
  TParam *clone() const override { return new TFontParam(*this); }
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
};

DEFINE_PARAM_SMARTPOINTER(TFontParam, std::wstring)

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChangeUndo
//-----------------------------------------------------------------------------

template <class T>
TNotAnimatableParamChangeUndo<T>::TNotAnimatableParamChangeUndo(
    TParam *param, const T &oldValue, const T &newValue)
    : m_param(param), m_oldValue(oldValue), m_newValue(newValue) {
  m_param->addRef();
}

//-----------------------------------------------------------------------------

template <class T>
TNotAnimatableParamChangeUndo<T>::~TNotAnimatableParamChangeUndo() {
  m_param->release();
}

//-----------------------------------------------------------------------------

template <class T>
void TNotAnimatableParamChangeUndo<T>::undo() const {
  TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(m_param);
  p->setValue(m_oldValue, true);
}

//-----------------------------------------------------------------------------

template <class T>
void TNotAnimatableParamChangeUndo<T>::redo() const {
  TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(m_param);
  p->setValue(m_newValue, true);
}
//-----------------------------------------------------------------------------

template <class T>
int TNotAnimatableParamChangeUndo<T>::getSize() const {
  return sizeof(*this);
}

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChange
//-----------------------------------------------------------------------------

template <class T>
TUndo *TNotAnimatableParamChange<T>::createUndo() const {
  return new TNotAnimatableParamChangeUndo<T>(m_param, m_oldValue, m_newValue);
}

//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
