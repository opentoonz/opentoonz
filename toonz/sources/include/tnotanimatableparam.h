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

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

template <class T>
class TNotAnimatableParamChange : public TParamChange
{
	T m_oldValue;
	T m_newValue;

public:
	TNotAnimatableParamChange(TParam *param,
							  const T &oldValue, const T &newValue,
							  bool undoing)
		: TParamChange(param, TParamChange::m_minFrame, TParamChange::m_maxFrame,
					   false, false, undoing),
		  m_oldValue(oldValue), m_newValue(newValue) {}

	TParamChange *clone() const { return new TNotAnimatableParamChange<T>(*this); }
	TUndo *createUndo() const;
};

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChangeUndo
//-----------------------------------------------------------------------------

template <class T>
class TNotAnimatableParamChangeUndo : public TUndo
{
public:
	TNotAnimatableParamChangeUndo(TParam *param,
								  const T &oldValue,
								  const T &newValue);
	~TNotAnimatableParamChangeUndo();
	void undo() const;
	void redo() const;
	int getSize() const;

private:
	TParam *m_param;
	T m_oldValue;
	T m_newValue;
};

//-----------------------------------------------------------------------------
//  TNotAnimatableParamObserver
//-----------------------------------------------------------------------------

template <class T>
class TNotAnimatableParamObserver : public TParamObserver
{
public:
	TNotAnimatableParamObserver() {}
	virtual void onChange(const TParamChange &) = 0;
	void onChange(const TNotAnimatableParamChange<T> &change)
	{
		onChange(static_cast<const TParamChange &>(change));
	}
};

typedef TNotAnimatableParamObserver<int> TIntParamObserver;
typedef TNotAnimatableParamObserver<bool> TBoolParamObserver;
typedef TNotAnimatableParamObserver<TFilePath> TFilePathParamObserver;

//-----------------------------------------------------------------------------
//    TNotAnimatableParam base class
//-----------------------------------------------------------------------------
using std::set;

template <class T>
class DVAPI TNotAnimatableParam : public TParam
{
	T m_defaultValue, m_value;

protected:
	set<TNotAnimatableParamObserver<T> *> m_observers;
	set<TParamObserver *> m_paramObservers;

public:
	TNotAnimatableParam(T def = T()) : TParam(), m_defaultValue(def), m_value(def){};
	TNotAnimatableParam(const TNotAnimatableParam &src)
		: TParam(src.getName()), m_defaultValue(src.getDefaultValue()), m_value(src.getValue()){};
	~TNotAnimatableParam(){};

	T getValue() const { return m_value; }
	T getDefaultValue() const { return m_defaultValue; }
	void setValue(T v, bool undoing = false)
	{
		if (m_value == v)
			return;

		TNotAnimatableParamChange<T> change(this, m_value, v, undoing);
		m_value = v;
		for (typename std::set<TNotAnimatableParamObserver<T> *>::iterator obsIt = m_observers.begin();
			 obsIt != m_observers.end();
			 ++obsIt)
			(*obsIt)->onChange(change);
		for (std::set<TParamObserver *>::iterator parObsIt = m_paramObservers.begin();
			 parObsIt != m_paramObservers.end();
			 ++parObsIt)
			(*parObsIt)->onChange(change);
	}

	void setDefaultValue(T value) { m_defaultValue = value; }
	void copy(TParam *src)
	{
		TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(src);
		if (!p)
			throw TException("invalid source for copy");
		setName(src->getName());
		m_defaultValue = p->m_defaultValue;
		m_value = p->m_value;
	}

	void reset(bool undoing = false) { setValue(m_defaultValue, undoing); }

	void addObserver(TParamObserver *observer)
	{
		TNotAnimatableParamObserver<T> *obs = dynamic_cast<TNotAnimatableParamObserver<T> *>(observer);
		if (obs)
			m_observers.insert(obs);
		else
			m_paramObservers.insert(observer);
	}
	void removeObserver(TParamObserver *observer)
	{
		TNotAnimatableParamObserver<T> *obs = dynamic_cast<TNotAnimatableParamObserver<T> *>(observer);
		if (obs)
			m_observers.erase(obs);
		else
			m_paramObservers.erase(observer);
	}

	bool isAnimatable() const { return false; }
	bool isKeyframe(double) const { return false; }
	void deleteKeyframe(double) {}
	void clearKeyframes() {}
	void assignKeyframe(
		double,
		const TSmartPointerT<TParam> &, double,
		bool) {}

	string getValueAlias(double, int)
	{
		return toString(getValue());
	}
	bool hasKeyframes() const { return 0; };
	void getKeyframes(std::set<double> &) const {};
	int getNextKeyframe(double) const { return -1; };
	int getPrevKeyframe(double) const { return -1; };
};

//=========================================================
//
//  class TIntParam
//
//=========================================================

#ifdef WIN32
template class DVAPI TNotAnimatableParam<int>;
class TIntParam;
template class DVAPI TPersistDeclarationT<TIntParam>;
#endif

class DVAPI TIntParam : public TNotAnimatableParam<int>
{
	PERSIST_DECLARATION(TIntParam);
	int minValue, maxValue;
	bool m_isWheelEnabled;

public:
	TIntParam(int v = int()) : TNotAnimatableParam<int>(v),
							   minValue(-(std::numeric_limits<int>::max)()),
							   maxValue((std::numeric_limits<int>::max)()), m_isWheelEnabled(false) {}
	TIntParam(const TIntParam &src) : TNotAnimatableParam<int>(src) {}
	TParam *clone() const { return new TIntParam(*this); }
	void loadData(TIStream &is);
	void saveData(TOStream &os);
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

#ifdef WIN32
template class DVAPI TNotAnimatableParam<bool>;
class TBoolParam;
template class DVAPI TPersistDeclarationT<TBoolParam>;
#endif

class DVAPI TBoolParam : public TNotAnimatableParam<bool>
{
	PERSIST_DECLARATION(TBoolParam);

public:
	TBoolParam(bool v = bool()) : TNotAnimatableParam<bool>(v) {}
	TBoolParam(const TBoolParam &src) : TNotAnimatableParam<bool>(src) {}

	TParam *clone() const { return new TBoolParam(*this); }

	void loadData(TIStream &is);
	void saveData(TOStream &os);
};

DEFINE_PARAM_SMARTPOINTER(TBoolParam, bool)

//=========================================================
//
//  class TFilePathParam
//
//=========================================================

#ifdef WIN32
template class DVAPI TNotAnimatableParam<TFilePath>;
class TFilePathParam;
template class DVAPI TPersistDeclarationT<TFilePathParam>;
#endif

class DVAPI TFilePathParam : public TNotAnimatableParam<TFilePath>
{
	PERSIST_DECLARATION(TFilePathParam);

public:
	TFilePathParam(const TFilePath &v = TFilePath()) : TNotAnimatableParam<TFilePath>(v) {}
	TFilePathParam(const TFilePathParam &src) : TNotAnimatableParam<TFilePath>(src) {}
	TParam *clone() const { return new TFilePathParam(*this); }
	void loadData(TIStream &is);
	void saveData(TOStream &os);
};

DEFINE_PARAM_SMARTPOINTER(TFilePathParam, TFilePath)

//=========================================================
//
//  class TStringParam
//
//=========================================================

#ifdef WIN32
template class DVAPI TNotAnimatableParam<std::wstring>;
class TStringParam;
template class DVAPI TPersistDeclarationT<TStringParam>;
#endif

class DVAPI TStringParam : public TNotAnimatableParam<std::wstring>
{
	PERSIST_DECLARATION(TStringParam);

public:
	TStringParam(wstring v = L"") : TNotAnimatableParam<std::wstring>(v) {}
	TStringParam(const TStringParam &src) : TNotAnimatableParam<wstring>(src) {}
	TParam *clone() const { return new TStringParam(*this); }
	void loadData(TIStream &is);
	void saveData(TOStream &os);
};

DEFINE_PARAM_SMARTPOINTER(TStringParam, wstring)

//=========================================================
//
//  class TEnumParam
//
//=========================================================

class TEnumParamImp;

class DVAPI TEnumParam : public TNotAnimatableParam<int>
{

	PERSIST_DECLARATION(TEnumParam)

public:
	TEnumParam(const int &v, const string &caption);

	TEnumParam();

	TEnumParam(const TEnumParam &src);
	~TEnumParam();

	TParam *clone() const { return new TEnumParam(*this); }
	void copy(TParam *src);

	void setValue(int v, bool undoing = false);
	void setValue(const string &caption, bool undoing = false);

	void addItem(const int &item, const string &caption);

	int getItemCount() const;
	void getItem(int i, int &item, string &caption) const;

	// TPersist methods
	void loadData(TIStream &is);
	void saveData(TOStream &os);

private:
	std::unique_ptr<TEnumParamImp> m_imp;
};

typedef TEnumParam TIntEnumParam;

typedef TNotAnimatableParamObserver<TIntEnumParam> TIntEnumParamObserver;

DVAPI_PARAM_SMARTPOINTER(TIntEnumParam)

class DVAPI TIntEnumParamP : public TDerivedSmartPointerT<TIntEnumParam, TParam>
{
public:
	TIntEnumParamP(TIntEnumParam *p = 0) : DerivedSmartPointer(p) {}
	TIntEnumParamP(int v, const string &caption) : DerivedSmartPointer(new TEnumParam(v, caption)) {}
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

#ifdef WIN32
template class DVAPI TNotAnimatableParam<double>;
class TNADoubleParam;
template class DVAPI TPersistDeclarationT<TNADoubleParam>;
#endif

class DVAPI TNADoubleParam : public TNotAnimatableParam<double>
{
	PERSIST_DECLARATION(TNADoubleParam);

public:
	TNADoubleParam(double v = double()) : TNotAnimatableParam<double>(v), m_min(0.), m_max(100.) {}
	TNADoubleParam(const TNADoubleParam &src) : TNotAnimatableParam<double>(src) {}
	TParam *clone() const { return new TNADoubleParam(*this); }
	void setValueRange(double min, double max)
	{
		m_min = min;
		m_max = max;
	}
	void setValue(double v, bool undoing = false)
	{
		notMoreThan(m_max, v);
		notLessThan(m_min, v);
		TNotAnimatableParam<double>::setValue(v, undoing);
	}
	bool getValueRange(double &min, double &max) const
	{
		min = m_min;
		max = m_max;
		return min < max;
	}

	void loadData(TIStream &is);
	void saveData(TOStream &os);

private:
	double m_min, m_max;
};

DEFINE_PARAM_SMARTPOINTER(TNADoubleParam, double)

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChangeUndo
//-----------------------------------------------------------------------------

template <class T>
TNotAnimatableParamChangeUndo<T>::TNotAnimatableParamChangeUndo(TParam *param,
																const T &oldValue,
																const T &newValue)
	: m_param(param), m_oldValue(oldValue), m_newValue(newValue)
{
	m_param->addRef();
}

//-----------------------------------------------------------------------------

template <class T>
TNotAnimatableParamChangeUndo<T>::~TNotAnimatableParamChangeUndo()
{
	m_param->release();
}

//-----------------------------------------------------------------------------

template <class T>
void TNotAnimatableParamChangeUndo<T>::undo() const
{
	TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(m_param);
	p->setValue(m_oldValue, true);
}

//-----------------------------------------------------------------------------

template <class T>
void TNotAnimatableParamChangeUndo<T>::redo() const
{
	TNotAnimatableParam<T> *p = dynamic_cast<TNotAnimatableParam<T> *>(m_param);
	p->setValue(m_newValue, true);
}
//-----------------------------------------------------------------------------

template <class T>
int TNotAnimatableParamChangeUndo<T>::getSize() const
{
	return sizeof(*this);
}

//-----------------------------------------------------------------------------
//  TNotAnimatableParamChange
//-----------------------------------------------------------------------------

template <class T>
TUndo *TNotAnimatableParamChange<T>::createUndo() const
{
	return new TNotAnimatableParamChangeUndo<T>(m_param, m_oldValue, m_newValue);
}

//-----------------------------------------------------------------------------

#ifdef WIN32
#pragma warning(pop)
#endif

#endif
