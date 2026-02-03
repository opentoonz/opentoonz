#pragma once

#ifndef __TTWAIN_H__
#define __TTWAIN_H__

#ifdef _WIN32
#include <windows.h>
#else
#ifndef _UNIX_
#define _UNIX_
#endif
#endif

#include "twain.h"

#include "tnztypes.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef USHORT
#define USHORT unsigned short
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TTWAIN_PIXTYPE {
  TTWAIN_BW,    /* 1-bit per pixel, B&W     */
  TTWAIN_WB,    /* 1-bit per pixel, W&B     */
  TTWAIN_GRAY8, /* 1,4, or 8-bit grayscale  */
  TTWAIN_RGB24, /* 24-bit RGB color         */
  TTWAIN_PIXTYPE_HOWMANY,
  TTWAIN_PIXUNKNOWN = -1,
} TTWAIN_PIXTYPE;

typedef enum TTWAIN_BITORDER {
  TTWAIN_LSBFIRST = TWBO_LSBFIRST,
  TTWAIN_MSBFIRST = TWBO_MSBFIRST
} TTWAIN_BITORDER;

typedef enum TWAINAVAILABLE {
  AVAIABLE_DONTKNOW = -1,
  AVAIABLE_YES,
  AVAIABLE_NO
} TWAINAVAILABLE;

typedef enum TTWAIN_TRANSFER_MECH {
  TTWAIN_TRANSFERMODE_NATIVE = TWSX_NATIVE,
  TTWAIN_TRANSFERMODE_MEMORY = TWSX_MEMORY,
  TTWAIN_TRANSFERMODE_FILE   = TWSX_FILE
} TTWAIN_TRANSFER_MECH;

typedef enum TTWAIN_USAGE_MODE {
  TTWAIN_MODE_LEASHED,
  TTWAIN_MODE_UNLEASHED
} TTWAIN_USAGE_MODE;

/* application should return 0 to stop the scanning process, any other value
 * elsewhere */
typedef int TTWAIN_ONDONE_CB(UCHAR *buffer, TTWAIN_PIXTYPE pixelType, int lx,
                             int ly, int wrap, float xdpi, float ydpi,
                             void *usrData);

typedef void TTWAIN_ONERROR_CB(void *usrData, void *alwaysZero);

int TTWAIN_IsAvailable(void);
void TTWAIN_RegisterApp(
    int majorNum, int minorNum, /* app. revision*/
    int nLanguage,    /* (human) language (use TWLG_xxx from TWAIN.H) */
    int nCountry,     /* country (use TWCY_xxx from TWAIN.H)	     */
    char *version,    /* version info string                          */
    char *manufacter, /* name of manufacter			     */
    char *family,     /* product family				     */
    char *product);   /* specific product			     */
int TTWAIN_SelectImageSource(void *hwnd);
/*---------------------------------------------------------------------------*/
void *TTWAIN_AcquireNative(void *hwnd);
int TTWAIN_AcquireMemory(void *hwnd);
void TTWAIN_StopAcquire(void);

/*
nb. AcquireMemory returns: an upside-down bitmap :)
nb. AcquireNative returns:
                          under Windows an HBITMAP
                          under Mac     a  PICT
*/
/*---------------------------------------------------------------------------*/
void TTWAIN_SetTwainUsage(TTWAIN_USAGE_MODE um);

void TTWAIN_FreeMemory(void *hMem);
int TTWAIN_CloseAll(void *hwnd);
int TTWAIN_OpenSourceManager(void *hwnd);
int TTWAIN_OpenDefaultSource(void);

int TTWAIN_GetHideUI(void);
void TTWAIN_SetHideUI(int flag);

void TTWAIN_SetOnDoneCallback(TTWAIN_ONDONE_CB *proc, void *arg);
void TTWAIN_SetOnErrorCallback(TTWAIN_ONERROR_CB *proc, void *arg);

#ifdef __cplusplus
}
#endif

#endif
