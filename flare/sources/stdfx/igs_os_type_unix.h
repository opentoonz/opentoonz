#pragma once

#ifndef igs_os_type_h
#define igs_os_type_h

/*------ ビット数固定変数定義 ----------------------------*/
#include <stdint.h> /* int32_t,uint32_tはC99で定義されたもの */
typedef int32_t INT32_T;
typedef uint32_t UINT32_T;

/*------ 2bytes文字対応 ----------------------------------*/
#if !defined TEXT
#ifdef UNICODE
#define TEXT(tt) L##tt
typedef wchar_t WCHAR;
typedef WCHAR TCHAR;
#else /* !UNICODE */
#define TEXT(tt) tt
typedef char TCHAR;
#endif /* !UNICODE */
typedef const TCHAR *LPCTSTR;
typedef unsigned long long int DWORDLONG;
typedef UINT32_T DWORD;
#endif /* !TEXT */

/*------ std::coutのWide Character対応 -------------------*/
#ifndef TCOUT
#ifdef UNICODE
#define TCOUT wcout
#else
#define TCOUT cout
#endif
#endif /* !TCOUT */

#endif /* !igs_os_type_h */
