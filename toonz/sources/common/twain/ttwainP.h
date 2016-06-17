#pragma once

#ifndef __TTWAINP_H__
#define __TTWAINP_H__

#include "ttwain.h"

#ifdef TTWAINLIB_MAIN
#define EXTERN
#else
#define EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TWAINSTATE {
  TWAIN_PRESESSION = 1, /* source manager not loaded  */
  TWAIN_SM_LOADED,      /* source manager loaded      */
  TWAIN_SM_OPEN,        /* source manager open	      */
  TWAIN_SOURCE_OPEN,    /* source open but not enabled*/
  TWAIN_SOURCE_ENABLED, /* source enabled to acquire  */
  TWAIN_TRANSFER_READY, /* image ready to transfer    */
  TWAIN_TRANSFERRING    /* image in transit	      */
} TWAINSTATE;

#ifdef _WIN32
#ifdef x64
#define DSM_FILENAME "TWAINDSM.DLL"
#else
#define DSM_FILENAME "TWAIN_32.DLL"
#endif
#define DSM_ENTRYPOINT "DSM_Entry"
#else
#define DSM_FILENAME ""
#define DSM_ENTRYPOINT ""
#endif

struct TRANSFER_INFO {
  TTWAIN_TRANSFER_MECH transferMech; /*current transfer mechanism      */
  TTWAIN_USAGE_MODE usageMode;
  /*used by buffered memory tranfer */
  unsigned char *memoryBuffer;
  unsigned long memorySize;
  /*used by native tranfer          */
  void *hDib;
  /*common to all the transfer mode */
  int preferredLx;
  int preferredLy;
  int multiTransfer;
  int nextImageNeedsToBeInverted;
  int lastTransferWasOk;
  TW_PENDINGXFERS pendingXfers;
  int oneAtLeast;
};

struct CALLBACKS {
  TTWAIN_ONDONE_CB *onDoneCb;
  void *onDoneArg;
  TTWAIN_ONERROR_CB *onErrorCb;
  void *onErrorArg;
};

struct _TTWAIN_DATA__ {
  /*HANDLE*/
  DSMENTRYPROC DSM_Entry; /* entry point of Data Source Manager(TWAIN.DLL)*/
  TW_INT32 hwnd32SM;      /* window handle              */
  /*IDENTITY*/
  TW_IDENTITY sourceId;     /* source identity structure  */
  TW_IDENTITY appId;        /* Application Ident. for DS  */
  TW_USERINTERFACE twainUI; /*                            */

  int initDone;
  TWAINAVAILABLE twainAvailable;
  TWAINSTATE twainState; /* the current twain state    */

  struct TRANSFER_INFO transferInfo;
  struct CALLBACKS callback;
  TW_ARRAY *supportedCaps; /* all the (exported)supported capabilities */
  int isSupportedCapsSupported;

  int breakModalLoop;

  int UIStatus; /* Hide or Show UserInterface */
  int modalStatus;

  TW_INT16 resultCode; /* result code for LAST error     */

  TUINT32 ErrRC; /* result & condition code        */
  TUINT32 ErrCC; /* for LAST RECORDED error        */
};

EXTERN struct _TTWAIN_DATA__ TTwainData;

EXTERN int TTWAIN_InitVar(void);

#ifdef __cplusplus
}
#endif

#endif
