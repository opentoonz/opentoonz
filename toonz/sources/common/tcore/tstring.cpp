

#include "tconvert.h"
//#include "texception.h"
#include "tfilepath.h"

#ifndef TNZCORE_LIGHT
#include <QString>
#endif

#ifdef _WIN32
#pragma warning(disable : 4996)
#include "windows.h"
#endif

#include <sstream>

class TStringConvertException : public TException
{
	std::string m_string;

public:
	TStringConvertException(const std::string str) : m_string(str) {}
};

std::wstring to_wstring(std::string s)
{
#ifdef TNZCORE_LIGHT
	std::wstring ws;
	ws.assign(s.begin(), s.end());
	return ws;
#else

	QString testString = QString::fromStdString(s);
	QString qString = QString::fromUtf8(s.c_str());

	// To detect if 's' is UTF-8 encoded or not
	if (qString != testString && std::string(qString.toUtf8()) == s)
		return qString.toStdWString();
	else
		return testString.toStdWString();
#endif
}

std::string to_string(std::wstring ws)
{
#ifdef TNZCORE_LIGHT
	std::string s;
	s.assign(ws.begin(), ws.end());
	return s;
#else
	QString const qString = QString::fromStdWString(ws);

	// Test if 'ws' is not unicode (UTF-8)
	if (qString.toLatin1() == qString)
		return qString.toStdString();

	return std::string(qString.toUtf8());
#endif
}

std::string to_string(const TFilePath &fp)
{
	return ::to_string(fp.getWideString());
}

/*!
  The  default precision is six decimal places. If the 
  precision is less than of the decimal places in the fractonal
  part, the remainder is not cut off but rounded.
*/

std::string to_string(double value, int prec)
{
	if (prec < 0) {
		return std::to_string(value);
	}

	std::ostringstream out;
	out.setf(std::ios_base::fixed, std::ios_base::floatfield);
	out.precision(prec);
	out << value;
	return out.str();
}

std::string to_string(void* p)
{
	std::ostringstream out;
	out << p;
	return out.str();
}

bool isInt(std::string s)
{
	int i = 0, len = (int)s.size();
	if (len == 0)
		return false;
	if (s[0] == '-') {
		if (len == 1)
			return false;
		else
			i++;
	}

	while (i < len) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	return true;
}

bool isDouble(std::string s)
{
	int i = 0, len = (int)s.size();
	if (len == 0)
		return false;
	if (i < len && s[i] == '-')
		i++;
	while (i < len && s[i] != '.') {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	if (i >= len)
		return true;
	i++;
	while (i < len) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		i++;
	}
	return true;
}

bool isInt(std::wstring s) { return isInt(::to_string(s)); }
bool isDouble(std::wstring s) { return isDouble(::to_string(s)); }

std::string toUpper(std::string a)
{
#ifdef _WIN32
	return _strupr(const_cast<char *>(a.c_str()));
#else
	std::string ret = a;
	for (int i = 0; i < (int)ret.length(); i++)
		ret[i] = toupper(ret[i]);
	return ret;
#endif
}

std::string toLower(std::string a)
{
#ifdef _WIN32
	return _strlwr(const_cast<char *>(a.c_str()));
#else
	std::string ret = a;
	for (int i = 0; i < (int)ret.length(); i++)
		ret[i] = tolower(ret[i]);
	return ret;
#endif
}

std::wstring toUpper(std::wstring a)
{
#ifdef _WIN32
	return _wcsupr(const_cast<wchar_t *>(a.c_str()));
#else
	std::wstring ret;
	for (int i = 0; i < (int)a.length(); i++) {
		wchar_t c = towupper(a[i]);
		ret += c;
	}
	return ret;
#endif
}

std::wstring toLower(std::wstring a)
{
#ifdef _WIN32
	return _wcslwr(const_cast<wchar_t *>(a.c_str()));
#else
	std::wstring ret;
	for (int i = 0; i < (int)a.length(); i++) {
		wchar_t c = towlower(a[i]);
		ret += c;
	}
	return ret;
#endif
}
