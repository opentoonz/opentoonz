

#ifndef TCONVERT_INCLUDED
#define TCONVERT_INCLUDED

#include "tcommon.h"
class TFilePath;

//
// Nota: il file tconvert.cpp esiste gia' in rop.
// l'implementazione di queste funzioni si trova in tstring.cpp
//

#undef DVAPI
#ifdef TNZCORE_EXPORTS //TNZCORE_DLL
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

DVAPI bool isInt(std::string s);
DVAPI bool isDouble(std::string s);

DVAPI std::string toString(int v);
DVAPI std::string toString(unsigned long v);
DVAPI std::string toString(unsigned long long v);
DVAPI std::string toString(double v, int prec = -1);
DVAPI std::string toString(std::wstring s);
DVAPI std::string toString(const TFilePath &fp);
DVAPI std::string toString(void *p);

DVAPI int toInt(std::string s);
DVAPI double toDouble(std::string s);

DVAPI bool isInt(std::wstring s);
DVAPI bool isDouble(std::wstring s);

DVAPI std::wstring toWideString(std::string s);
DVAPI std::wstring toWideString(int v);
DVAPI std::wstring toWideString(double v, int prec = -1);

DVAPI int toInt(std::wstring s);
DVAPI double toDouble(std::wstring s);

inline bool fromStr(int &v, std::string s)
{
	if (isInt(s)) {
		v = toInt(s);
		return true;
	} else
		return false;
}

inline bool fromStr(double &v, std::string s)
{
	if (isDouble(s)) {
		v = toDouble(s);
		return true;
	} else
		return false;
}

inline bool fromStr(std::string &out, std::string s)
{
	out = s;
	return true;
}

DVAPI std::string toUpper(std::string a);
DVAPI std::string toLower(std::string a);

DVAPI std::wstring toUpper(std::wstring a);
DVAPI std::wstring toLower(std::wstring a);

#ifndef TNZCORE_LIGHT
#include <QString>

inline bool fromStr(int &v, QString s)
{
	bool ret;
	v = s.toInt(&ret);
	return ret;
}

inline bool fromStr(double &v, QString s)
{
	bool ret;
	v = s.toDouble(&ret);
	return ret;
}

#endif

#endif
