#pragma once

#ifndef igs_os_type_h
#define igs_os_type_h

/*------ 2bytes文字対応 ----------------------------------*/
#include <windows.h> /* INT32,UINT32 */

/*------ ビット数固定変数定義 ----------------------------*/
typedef INT32 INT32_T;
typedef UINT32 UINT32_T;

/*------ std::coutのWide Character対応 -------------------*/
#ifndef TCOUT
#ifdef UNICODE
#define TCOUT wcout
#else
#define TCOUT cout
#endif
#endif /* !TCOUT */

#endif /* !igs_os_type_h */
