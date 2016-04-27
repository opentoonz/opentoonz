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

inline bool fromStr(int &value, std::string s)
{
	if (isInt(s)) {
		value = toInt(s);
		return true;
	} else
		return false;
}
inline bool fromStr(double &value, std::string s)
{
	if (isDouble(s)) {
		value = toDouble(s);
		return true;
	} else
		return false;
}
inline bool fromStr(std::string &value, std::string s)
{
	value = s;
	return true;
}
inline bool fromStr(TFilePath &value, std::string s)
{
	value = TFilePath(s);
	return true;
}

//=========================================================

class UsageError
{
	std::string m_msg;

public:
	UsageError(std::string msg) : m_msg(msg){};
	~UsageError(){};
	std::string getError() const { return m_msg; };
};

//=========================================================

class DVAPI UsageElement
{
protected:
	std::string m_name, m_help;
	bool m_selected;

public:
	UsageElement(std::string name, std::string help);
	virtual ~UsageElement(){};
	std::string getName() const { return m_name; };
	bool isSelected() const { return m_selected; };
	void select() { m_selected = true; };

	virtual bool isHidden() const { return false; };
	virtual bool isSwitcher() const { return false; };
	virtual bool isArgument() const { return false; };
	virtual bool isMultiArgument() const { return false; };
	void setHelp(std::string help) { m_help = help; };

	virtual void print(std::ostream &out) const;
	virtual void printHelpLine(std::ostream &out) const;
	virtual void dumpValue(std::ostream &out) const = 0;
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
	Qualifier(std::string name, std::string help)
		: UsageElement(name, help), m_switcher(false){};
	~Qualifier(){};

	virtual bool isSwitcher() const { return m_switcher; };
	virtual bool isHidden() const { return m_help == ""; };

	operator bool() const { return isSelected(); };
	virtual void fetch(int index, int &argc, char *argv[]) = 0;
	virtual void print(std::ostream &out) const;
};

//---------------------------------------------------------

class DVAPI SimpleQualifier : public Qualifier
{
public:
	SimpleQualifier(std::string name, std::string help)
		: Qualifier(name, help){};
	~SimpleQualifier(){};
	void fetch(int index, int &argc, char *argv[]);
	void dumpValue(std::ostream &out) const;
	void resetValue();
};

//---------------------------------------------------------

class DVAPI Switcher : public SimpleQualifier
{
public:
	Switcher(std::string name, std::string help)
		: SimpleQualifier(name, help) { m_switcher = true; };
	~Switcher(){};
};

//---------------------------------------------------------

template <class T>
class QualifierT : public Qualifier
{
	T m_value;

public:
	QualifierT<T>(std::string name, std::string help)
		: Qualifier(name, help), m_value(){};
	~QualifierT<T>(){};

	T getValue() const { return m_value; };

	virtual void fetch(int index, int &argc, char *argv[])
	{
		if (index + 1 >= argc)
			throw UsageError("missing argument");
		if (!fromStr(m_value, argv[index + 1]))
			throw UsageError(
				m_name + ": bad argument type /" + std::string(argv[index + 1]) + "/");
		for (int i = index; i < argc - 1; i++)
			argv[i] = argv[i + 2];
		argc -= 2;
	};

	void dumpValue(std::ostream &out) const
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
	Argument(std::string name, std::string help)
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
	ArgumentT<T>(std::string name, std::string help) : Argument(name, help){};
	~ArgumentT<T>(){};
	operator T() const { return m_value; };
	T getValue() const { return m_value; };

	bool assign(char *src) { return fromStr(m_value, src); };
	void dumpValue(std::ostream &out) const
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
	MultiArgument(std::string name, std::string help)
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
	std::unique_ptr<T[]> m_values;

public:
	MultiArgumentT(std::string name, std::string help)
		: MultiArgument(name, help) {}
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

	void dumpValue(std::ostream &out) const
	{
		out << m_name << " = {";
		for (int i = 0; i < m_count; i++)
			out << " " << m_values[i];
		out << "}" << std::endl;
	};

	void resetValue()
	{
		m_values.reset();
		m_count = m_index = 0;
	};

	void allocate(int count)
	{
		m_values.reset((count > 0) ? new T[count] : nullptr);
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
	std::unique_ptr<UsageElementPtr[]> m_elements;
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
	Usage(std::string progName);
	~Usage();
	void add(const UsageLine &);

	void print(std::ostream &out) const;
	void dumpValues(std::ostream &out) const; // per debug

	bool parse(int argc, char *argv[], std::ostream &err = std::cerr);
	bool parse(const char *argvString, std::ostream &err = std::cerr);
	void clear(); // per debug

private:
	//not implemented
	Usage(const Usage &);
	Usage &operator=(const Usage &);
};

//=========================================================

typedef QualifierT<int> IntQualifier;
typedef QualifierT<double> DoubleQualifier;
typedef QualifierT<std::string> StringQualifier;

typedef ArgumentT<int> IntArgument;
typedef ArgumentT<double> DoubleArgument;
typedef ArgumentT<std::string> StringArgument;
typedef ArgumentT<TFilePath> FilePathArgument;

typedef MultiArgumentT<int> IntMultiArgument;
typedef MultiArgumentT<double> DoubleMultiArgument;
typedef MultiArgumentT<std::string> StringMultiArgument;
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
	void dumpValue(std::ostream &out) const;
	void resetValue();
};

//=========================================================

} // namespace TCli

#endif // TCLI_INCLUDED
