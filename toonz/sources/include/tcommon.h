

#ifndef T_COMMON_INCLUDED
#define T_COMMON_INCLUDED

#ifdef _WIN32
#pragma warning(disable : 4786)
#pragma warning(disable : 4251)
#pragma warning(disable : 4146)
#pragma warning(disable : 4267)
#endif

#ifndef _DEBUG
#undef _STLP_DEBUG
#else
#define _STLP_DEBUG 1
#endif
#if defined(NDEBUG) && defined(VERIFY_TNZCORE)
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#include <string>
#include <strstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <limits>

#if 0 && defined(__GNUC__)
// typedef seems strong on GCC and breaks code with TException..
#define TString std::wstring
#else
typedef std::wstring TString;
#endif

// STL

/*
 * On  Irix stl_alloc.h refers to nanosleep and timespec
 */

#ifdef __sgi
/*
#if (_MIPS_SZLONG == 32)
struct timespec {TLong tv_sec; TLong tv_nsec;};
#else
struct timespec {int tv_sec; TLong tv_nsec;};
#endif

#ifdef __cplusplus
extern "C" 
#endif
int nanosleep(struct timespec *, int);
*/

#endif

#include <list>
#include <vector>
#include <map>
// .. and so on

/* the value of pi defined in IRIX-math.h is not defined neither in WIN32-math.h nor elsewhere */
namespace TConsts
{
const double pi = 3.1415926535897932384626433832795;
const double pi_2 = 1.5707963267948966192313216916398;
const double epsilon = 1e-8;
const double sqrt2 = 1.4142135623730950488016887242097;
const double sqrt2_2 = 0.7071067811865475244008443621048;
const double pi_180 = pi / 180.0;
const double invOf_pi_180 = 180.0 / pi;
}

// typedef's

#include "tnztypes.h"

typedef signed char SCHAR;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned int UINT;

#ifndef _WIN32
typedef unsigned char BYTE;
#endif

template <class T>
inline T tmin(T a, T b)
{
	return a < b ? a : b;
}
template <class T>
inline T tmax(T a, T b) { return a > b ? a : b; }
template <class T>
inline T tmin(T a, T b, T c) { return tmin(tmin(a, b), c); }
template <class T>
inline T tmax(T a, T b, T c) { return tmax(tmax(a, b), c); }
template <class T>
inline T tmin(T a, T b, T c, T d) { return tmin(tmin(a, b), tmin(c, d)); }
template <class T>
inline T tmax(T a, T b, T c, T d) { return tmax(tmax(a, b), tmax(c, d)); }

template <class T>
inline void tswap(T &a, T &b)
{
	T tmp = a;
	a = b;
	b = tmp;
}
template <class T>
inline T tcrop(T x, T a, T b) { return tmin(tmax(x, a), b); }
template <class T>
inline void notLessThan(T threshold, T &x)
{
	if (x < threshold)
		x = threshold;
}
template <class T>
inline void notMoreThan(T threshold, T &x)
{
	if (x > threshold)
		x = threshold;
}

/*! round(x) returns the nearest integer to x. 
    Works well for x<0, but is slower than roundp()
    /sa roundp() */
inline int tround(double x)
{
	return ((int)(((int)(-0.9) == 0 && x < 0.0) ? (x - 0.5) : (x + 0.5)));
	//return ((int)(((int)(-0.9F) == 0 && x<0.0F) ? (x - 0.5F) : (x + 0.5F)));
}

/*! roundp(x) is like round(x), but the result if x<0 is 
    platform dependent 
	/sa round() */
inline int troundp(double x)
{
	return ((int)((x) + 0.5F));
}

/* li metto dopo
   equivalenti a (int)floor() e (int)ceil() (ma piu' veloci)
   solo se la risoluzione dell'argomento e' inferiore ad 1 
#define FLOOR(x) ((int)(x) > (x) ? (int)(x)-1 : (int)(x))
#define CEIL(x)  ((int)(x) < (x) ? (int)(x)+1 : (int)(x))
*/

/*! byteFromUshort(u) converts integer from [0..65535] to [0..255] */
inline UCHAR byteFromUshort(USHORT u)
{
	return ((256U * 255U + 1U) * u + (1 << 23)) >> 24;
}

/*! ditheredByteFromUshort(u) is like byteFromUshort().
    It is used in dithering ... 
 */

inline UCHAR ditheredByteFromUshort(USHORT u, int r)
{
	return ((((u * (256U * 255U + 1U)) -
			  ((u * (256U * 255U + 1U)) >> 24)) +
			 r) >>
			24);
}

/*! ushortFromByte(u) converts integer from [0..255] to  [0..65535]*/
inline USHORT ushortFromByte(UCHAR u)
{
	return u | u << 8;
}

const int c_maxint = ((int)((~0U) >> 1));
const int c_minint = ~c_maxint;

const unsigned int c_maxuint = (unsigned int)(~0U);

#ifdef _WIN32
#define DV_EXPORT_API __declspec(dllexport)
#define DV_IMPORT_API __declspec(dllimport)
#define DV_EXPORT_VAR __declspec(dllexport)
#define DV_IMPORT_VAR __declspec(dllimport)
#else
#define DV_EXPORT_API
#define DV_IMPORT_API
#define DV_EXPORT_VAR
#define DV_IMPORT_VAR
#endif

#ifdef _WIN32
#define DV_ALIGNED(val) __declspec(align(val))
#else
#define DV_ALIGNED(val) __attribute__((aligned(val)))
#endif

inline short swapShort(short val)
{
	return ((val >> 8) & 0x00ff) | (val << 8);
}
inline TINT32 swapTINT32(TINT32 val)
{
	TINT32 appo, aux, aux1;
	appo = 0xff0000ff;
	aux = (val & appo);
	aux1 = (aux >> 24) & 0x000000ff;
	aux = (aux << 24) | aux1;
	appo = 0x00ffff00;
	aux1 = (val & appo);
	aux1 = ((aux1 >> 8) | (aux1 << 8)) & appo;
	aux = (aux | aux1);
	return aux;
}
inline USHORT swapUshort(USHORT val) { return val >> 8 | val << 8; }

inline std::ostream &operator<<(std::ostream &out, const std::string &s)
{
	return out << s.c_str();
}

#define tArrayCount(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

const std::string mySettingsFileName = "mysettings.ini";

#endif //__T_COMMON_INCLUDED
