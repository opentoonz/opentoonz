#ifndef TCLI_INCLUDED
#define TCLI_INCLUDED

#include <memory>

//#include "tcommon.h"   contenuto in tconvert.h
#include "tconvert.h"

#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TAPPTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

//forward declaration
class TFilePath;
//=========================================================

namespace TCli
{

//=========================================================

inline bool fromStr(int &value, string s)
{
	if (isInt(s)) {
		value = toInt(s);
		return true;
	} else
		return false;
}
inline bool fromStr(double &value, string s)
{
	if (isDouble(s)) {
		value = toDouble(s);
		return true;
	} else
		return false;
}
inline bool fromStr(string &value, string s)
{
	value = s;
	return true;
}
inline bool fromStr(TFilePath &value, string s)
{
	value = TFilePath(s);
	return true;
}

//=========================================================

class UsageError
{
	string m_msg;

public:
	UsageError(string msg) : m_msg(msg){};
	~UsageError(){};
	string getError() const { return m_msg; };
};

//=========================================================

class DVAPI UsageElement
{
protected:
	string m_name, m_help;
	bool m_selected;

public:
	UsageElement(string name, string help);
	virtual ~UsageElement(){};
	string getName() const { return m_name; };
	bool isSelected() const { return m_selected; };
	void select() { m_selected = true; };

	virtual bool isHidden() const { return false; };
	virtual bool isSwitcher() const { return false; };
	virtual bool isArgument() const { return false; };
	virtual bool isMultiArgument() const { return false; };
	void setHelp(string help) { m_help = help; };

	virtual void print(ostream &out) const;
	virtual void printHelpLine(ostream &out) const;
	virtual void dumpValue(ostream &out) const = 0;
	virtual void resetValue() = 0;

private:
	// not implemented
	UsageElement(const UsageElement &);
	UsageElement &operator=(const UsageElement &);
};

//=========================================================

class DVAPI Qualifier : public UsageElement
{
protected:
	bool m_switcher;

public:
	Qualifier(string name, string help)
		: UsageElement(name, help), m_switcher(false){};
	~Qualifier(){};

	virtual bool isSwitcher() const { return m_switcher; };
	virtual bool isHidden() const { return m_help == ""; };

	operator bool() const { return isSelected(); };
	virtual void fetch(int index, int &argc, char *argv[]) = 0;
	virtual void print(ostream &out) const;
};

//---------------------------------------------------------

class DVAPI SimpleQualifier : public Qualifier
{
public:
	SimpleQualifier(string name, string help)
		: Qualifier(name, help){};
	~SimpleQualifier(){};
	void fetch(int index, int &argc, char *argv[]);
	void dumpValue(ostream &out) const;
	void resetValue();
};

//---------------------------------------------------------

class DVAPI Switcher : public SimpleQualifier
{
public:
	Switcher(string name, string help)
		: SimpleQualifier(name, help) { m_switcher = true; };
	~Switcher(){};
};

//---------------------------------------------------------

template <class T>
class QualifierT : public Qualifier
{
	T m_value;

public:
	QualifierT<T>(string name, string help)
		: Qualifier(name, help), m_value(){};
	~QualifierT<T>(){};

	T getValue() const { return m_value; };

	virtual void fetch(int index, int &argc, char *argv[])
	{
		if (index + 1 >= argc)
			throw UsageError("missing argument");
		if (!fromStr(m_value, argv[index + 1]))
			throw UsageError(
				m_name + ": bad argument type /" + string(argv[index + 1]) + "/");
		for (int i = index; i < argc - 1; i++)
			argv[i] = argv[i + 2];
		argc -= 2;
	};

	void dumpValue(ostream &out) const
	{
		out << m_name << " = " << (isSelected() ? "on" : "off") << " : "
			<< m_value << "\n";
	};

	void resetValue()
	{
		m_value = T();
		m_selected = false;
	};
};

//=========================================================

class DVAPI Argument : public UsageElement
{
public:
	Argument(string name, string help)
		: UsageElement(name, help){};
	~Argument(){};
	virtual void fetch(int index, int &argc, char *argv[]);
	virtual bool assign(char *) = 0;
	bool isArgument() const { return true; };
};

//---------------------------------------------------------

template <class T>
class ArgumentT : public Argument
{
	T m_value;

public:
	ArgumentT<T>(string name, string help) : Argument(name, help){};
	~ArgumentT<T>(){};
	operator T() const { return m_value; };
	T getValue() const { return m_value; };

	bool assign(char *src) { return fromStr(m_value, src); };
	void dumpValue(ostream &out) const
	{
		out << m_name << " = " << m_value << "\n";
	};
	void resetValue()
	{
		m_value = T();
		m_selected = false;
	};
};

//=========================================================

class DVAPI MultiArgument : public Argument
{
protected:
	int m_count, m_index;

public:
	MultiArgument(string name, string help)
		: Argument(name, help), m_count(0), m_index(0){};
	~MultiArgument(){};
	int getCount() const { return m_count; };
	virtual void fetch(int index, int &argc, char *argv[]);
	bool isMultiArgument() const { return true; };
	virtual void allocate(int count) = 0;
};

//---------------------------------------------------------

template <class T>
class MultiArgumentT : public MultiArgument
{
	T *m_values;

public:
	MultiArgumentT(string name, string help)
		: MultiArgument(name, help), m_values(0){};
	~MultiArgumentT()
	{
		if (m_values)
			delete[] m_values;
	};
	T operator[](int index)
	{
		assert(0 <= index && index < m_count);
		return m_values[index];
	};
	virtual bool assign(char *src)
	{
		assert(0 <= m_index && m_index < m_count);
		return fromStr(m_values[m_index], src);
	};

	void dumpValue(ostream &out) const
	{
		out << m_name << " = {";
		for (int i = 0; i < m_count; i++)
			out << " " << m_values[i];
		out << "}" << std::endl;
	};

	void resetValue()
	{
		if (m_values)
			delete[] m_values;
		m_values = 0;
		m_count = m_index = 0;
	};

	void allocate(int count)
	{
		if (m_values)
			delete[] m_values;
		m_values = count ? new T[count] : 0;
		m_count = count;
		m_index = 0;
	};
};

//=========================================================

typedef UsageElement *UsageElementPtr;

//---------------------------------------------------------

class DVAPI UsageLine
{
protected:
	UsageElementPtr *m_elements;
	int m_count;

public:
	UsageLine();
	virtual ~UsageLine();
	UsageLine(const UsageLine &ul);
	UsageLine &operator=(const UsageLine &ul);

	UsageLine(int count);
	UsageLine(const UsageLine &, UsageElement &elem);
	UsageLine(UsageElement &elem);
	UsageLine(UsageElement &a, UsageElement &b);

	UsageLine operator+(UsageElement &);

	int getCount() const { return m_count; };
	UsageElementPtr &operator[](int index) { return m_elements[index]; };
	const UsageElementPtr &operator[](int index) const { return m_elements[index]; };
};

//---------------------------------------------------------

DVAPI UsageLine operator+(UsageElement &a, UsageElement &b);

//---------------------------------------------------------

class DVAPI Optional : public UsageLine
{
public:
	Optional(const UsageLine &ul);
	~Optional(){};
};

//---------------------------------------------------------

DVAPI UsageLine operator+(const UsageLine &a, const Optional &b);

//=========================================================

class UsageImp;

class DVAPI Usage
{
	std::unique_ptr<UsageImp> m_imp;

public:
	Usage(string progName);
	~Usage();
	void add(const UsageLine &);

	void print(ostream &out) const;
	void dumpValues(ostream &out) const; // per debug

	bool parse(int argc, char *argv[], ostream &err = std::cerr);
	bool parse(const char *argvString, ostream &err = std::cerr);
	void clear(); // per debug

private:
	//not implemented
	Usage(const Usage &);
	Usage &operator=(const Usage &);
};

//=========================================================

typedef QualifierT<int> IntQualifier;
typedef QualifierT<double> DoubleQualifier;
typedef QualifierT<string> StringQualifier;

typedef ArgumentT<int> IntArgument;
typedef ArgumentT<double> DoubleArgument;
typedef ArgumentT<string> StringArgument;
typedef ArgumentT<TFilePath> FilePathArgument;

typedef MultiArgumentT<int> IntMultiArgument;
typedef MultiArgumentT<double> DoubleMultiArgument;
typedef MultiArgumentT<string> StringMultiArgument;
typedef MultiArgumentT<TFilePath> FilePathMultiArgument;

//=========================================================

class DVAPI RangeQualifier : public Qualifier
{
	int m_from, m_to;

public:
	RangeQualifier();
	~RangeQualifier(){};

	int getFrom() const { return m_from; };
	int getTo() const { return m_to; };
	bool contains(int frame) const
	{
		return m_from <= frame && frame <= m_to;
	};
	void fetch(int index, int &argc, char *argv[]);
	void dumpValue(ostream &out) const;
	void resetValue();
};

//=========================================================

} // namespace TCli

#endif // TCLI_INCLUDED
