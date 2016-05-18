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

class DVAPI TParamVar
{
	std::string m_name;
	bool m_isHidden;
	TParamObserver *m_paramObserver;

public:
	TParamVar(std::string name, bool hidden = false)
		: m_name(name), m_isHidden(hidden), m_paramObserver(0) {}
	virtual ~TParamVar() {}
	virtual TParamVar *clone() const = 0;
	std::string getName() const { return m_name; }
	bool isHidden() const { return m_isHidden; }
	void setIsHidden(bool hidden) { m_isHidden = hidden; }
	virtual void setParam(TParam *param) = 0;
	virtual TParam *getParam() const = 0;
	void setParamObserver(TParamObserver *obs);
};

template <class T>
class TParamVarT : public TParamVar
{
	TParamP m_var;

public:
	TParamVarT(std::string name, TParamP var, bool hidden = false)
		: TParamVar(name, hidden), m_var(var)
	{
	}
	TParamVarT(std::string name, T *var, bool hidden = false)
		: TParamVar(name, hidden), m_var(var)
	{
	}
	void setParam(TParam *param)
	{
		m_var = TParamP(param);
	}
	virtual TParam *getParam() const { return m_var.getPointer(); }
	TParamVar *clone() const
	{
		return new TParamVarT<T>(getName(), m_var, isHidden());
	}
};

class DVAPI TParamContainer
{
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
