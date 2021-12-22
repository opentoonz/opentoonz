

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#define TTWAINLIB_MAIN
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "ttwain_state.h"
#include "ttwain_statePD.h"
#include "ttwain_error.h"
#include "ttwain_win.h"
#include "ttwain_winPD.h"
#include "ttwain_util.h"
#include "ttwain_utilP.h"
#include "ttwain_conversion.h"
#include "ttwainP.h"

#include "ttwain_global_def.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef MACOSX
extern int exitTwainSession(void);
#endif
static void TTWAIN_FreeVar(void);

#define RELEASE_STR "5.1"
#define TITLEBAR_STR "Toonz5.1"
#define TwProgramName "Toonz5.1"

#define COMPANY "Digital Video"
#define PRODUCT "TOONZ"

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define CEIL(x) ((int)(x) < (x) ? (int)(x) + 1 : (int)(x))

#ifndef _WIN32
#define PRINTF(args...)
#else
#define PRINTF
#endif
/*---------------------------------------------------------------------------*/
/* LOCAL PROTOTYPES
 */
/*---------------------------------------------------------------------------*/
static int TTWAIN_DoOneTransfer(void);
static int TTWAIN_WaitForXfer(void *hwnd);
static void *TTWAIN_WaitForNativeXfer(void *hwnd);
static int TTWAIN_WaitForMemoryXfer(void *hwnd);
static int TTWAIN_NativeXferHandler(void);
static int TTWAIN_MemoryXferHandler(void);
static int TTWAIN_AbortAllPendingXfers(void);
static void TTWAIN_ModalEventLoop(void);
static void TTWAIN_BreakModalLoop(void);
static void TTWAIN_EmptyMessageQueue(void);
/*---------------------------------------------------------------------------*/
void TTWAIN_SetState(TWAINSTATE status);
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* int TTWAIN_LoadSourceManager(void)      <- this is in ttwain_state.h */

/* int TTWAIN_SelectImageSource(void* hwnd) <-\
   int TTWAIN_SelectImageSource(void* hwnd) <-|--\
   int TTWAIN_OpenDefaultSource(void)       <-|-- these are in ttwain.h
   int TTWAIN_CloseAll        (void *hwnd)  <-/
*/

/* these are local */
static int TTWAIN_EnableSource(void *hwnd);
int TTWAIN_MessageHook(void *lpmsg);
static int TTWAIN_EndXfer(void);
static int TTWAIN_DisableSource(void);
static int TTWAIN_CloseSource(void);
static int TTWAIN_CloseSourceManager(void *hwnd);

/* int TTWAIN_UnloadSourceManager(void)       <- this is in ttwain_state.h */

static int TTWAIN_MGR(TUINT32 dg, TUINT32 dat, TUINT32 msg, void *pd);

#define INVERT_BYTE(START, HOWMANY)                                            \
  {                                                                            \
    UCHAR *p = (START);                                                        \
    unsigned int ijk;                                                          \
    for (ijk = 0; ijk < (HOWMANY); ijk++, p++) {                               \
      *p = ~*p;                                                                \
    }                                                                          \
  }

#define BB(H, L) (H << 8 | L)
/*---------------------------------------------------------------------------*/
/*	  TWAIN STATE		 					     */
/*---------------------------------------------------------------------------*/
/*STATE 1 TO 2*/
int TTWAIN_LoadSourceManager(void) {
  TTWAIN_InitVar();
  return TTWAIN_LoadSourceManagerPD();
}
/*---------------------------------------------------------------------------*/
/*STATE 2 TO 3*/
int TTWAIN_OpenSourceManager(void *hwnd) {
  TTwainData.hwnd32SM = (TW_INT32)TTWAIN_GetValidHwnd(hwnd);

  if (TTWAIN_GetState() < TWAIN_SM_OPEN) {
    if (TTWAIN_LoadSourceManager() &&
        TTWAIN_MGR(DG_CONTROL, DAT_PARENT, MSG_OPENDSM, &TTwainData.hwnd32SM)) {
      assert(TTWAIN_GetState() == TWAIN_SM_OPEN);
    }
  }
  return (TTWAIN_GetState() >= TWAIN_SM_OPEN);
}
/*---------------------------------------------------------------------------*/
/*STATE 3*/
static TW_IDENTITY newSourceId;
int TTWAIN_SelectImageSource(void *hwnd) {
  int success           = FALSE;
  TWAINSTATE entryState = TTWAIN_GetState();

  if (TTWAIN_GetState() >= TWAIN_SM_OPEN || TTWAIN_OpenSourceManager(hwnd)) {
    // TW_IDENTITY	newSourceId;
    memset(&newSourceId, 0, sizeof newSourceId);

    TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_GETDEFAULT, &newSourceId);
    /* Post the Select Source dialog */
    success =
        TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_USERSELECT, &newSourceId);
  } else {
    char msg[2048];
    snprintf(msg, sizeof(msg), "Unable to open Source Manager (%s)",
             DSM_FILENAME);
    TTWAIN_ErrorBox(msg);
    return FALSE;
  }

  if (entryState < TWAIN_SM_OPEN) {
    TTWAIN_CloseSourceManager(hwnd);
    if (entryState < TWAIN_SM_LOADED) {
      TTWAIN_UnloadSourceManager();
    }
  }
  return success;
}
/*---------------------------------------------------------------------------*/
/*STATE 3 TO 4*/
int TTWAIN_OpenDefaultSource(void) {
  TW_IDENTITY tempId;
  int rc;
  static int first_time = TRUE;

  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    if (TTWAIN_GetState() < TWAIN_SM_OPEN && !TTWAIN_OpenSourceManager(NULL))
      return FALSE;

    rc = TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, &tempId);
    while (rc && tempId.Id != 0) {
      if (strcmp((char *)newSourceId.ProductName, (char *)tempId.ProductName) ==
          0) {
        newSourceId = tempId;
        break;
      }
      rc = TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &tempId);
    }

    if (TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, &newSourceId)) {
      assert(TTWAIN_GetState() == TWAIN_SOURCE_OPEN);
    }
  }

  if (first_time && (TTWAIN_GetState() == TWAIN_SOURCE_OPEN)) {
    TTWAIN_GetSupportedCaps();
    /*first_time = FALSE;*/
  }
  return (TTWAIN_GetState() == TWAIN_SOURCE_OPEN);
}
/*---------------------------------------------------------------------------*/
/*STATE 4 TO 5*/
static int TTWAIN_EnableSource(void *hwnd) {
  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN && !TTWAIN_OpenDefaultSource())
    return FALSE;

  TTwainData.twainUI.ShowUI  = TTWAIN_GetUIStatus();
  TTwainData.twainUI.ModalUI = TTWAIN_GetModalStatus();
  TTwainData.twainUI.hParent = (TW_HANDLE)TTWAIN_GetValidHwnd(hwnd);
  TTWAIN_DS(DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &TTwainData.twainUI);
  return (TTWAIN_GetState() == TWAIN_SOURCE_ENABLED);
}
/*---------------------------------------------------------------------------*/
/*STATE 5 TO 6 TO 7*/
int TTWAIN_MessageHook(void *lpmsg)
/* returns TRUE if msg processed by TWAIN (source) */
{
  int bProcessed = FALSE;
  // printf("%s\n", __PRETTY_FUNCTION__);
  if (TTWAIN_GetState() >= TWAIN_SOURCE_ENABLED) {
/* source enabled */
#ifdef _WIN32
    TW_EVENT twEvent;
    twEvent.pEvent    = (TW_MEMREF)lpmsg;
    twEvent.TWMessage = MSG_NULL;
    /* see if source wants to process (eat) the message */
    bProcessed = (TTWAIN_DS(DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT,
                            &twEvent) == TWRC_DSEVENT);
#else
    TW_EVENT twEvent;
    twEvent.pEvent    = (TW_MEMREF)0;
    twEvent.TWMessage = (TW_UINT32)lpmsg;
#endif
    switch (twEvent.TWMessage) {
    case MSG_XFERREADY:
#ifdef MACOSX
      TTWAIN_SetState(TWAIN_TRANSFER_READY);
#endif
      assert(TTWAIN_GetState() == TWAIN_TRANSFER_READY);
      TTWAIN_DoOneTransfer();
      break;
    case MSG_CLOSEDSREQ:
      TTWAIN_DisableSource();
      break;
    case MSG_NULL:
      /* no message returned from DS */
      break;
    } /* switch */
  }
  return bProcessed;
}
/*---------------------------------------------------------------------------*/
/*STATE 7 TO 6 TO 5*/
static int TTWAIN_EndXfer(void) {
  if (TTWAIN_GetState() == TWAIN_TRANSFERRING)
    TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER,
              &TTwainData.transferInfo.pendingXfers);
  return (TTWAIN_GetState() < TWAIN_TRANSFERRING);
}
/*---------------------------------------------------------------------------*/
/* STATE 5 TO 1 */
/* 3 OP + 1 PD OP*/

/*STATE 5 TO 4*/
static int TTWAIN_DisableSource(void) {
  TTWAIN_AbortAllPendingXfers();

  if ((TTWAIN_GetState() >= TWAIN_SOURCE_ENABLED) &&
      (TTWAIN_DS(DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS,
                 &TTwainData.twainUI) == TWRC_SUCCESS)) {
    assert(TTWAIN_GetState() == TWAIN_SOURCE_OPEN);
    return FALSE;
  }
  TTWAIN_EmptyMessageQueue();
  return (TTWAIN_GetState() < TWAIN_SOURCE_ENABLED);
}
/*---------------------------------------------------------------------------*/
/*STATE 4 TO 3*/
static int TTWAIN_CloseSource(void) {
  TTwainData.resultCode = TWRC_SUCCESS;

  TTWAIN_DisableSource();
  if (TTWAIN_GetState() == TWAIN_SOURCE_OPEN &&
      TTWAIN_MGR(DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &TTwainData.sourceId)) {
    assert(TTWAIN_GetState() == TWAIN_SM_OPEN);
  }
  return (TTWAIN_GetState() <= TWAIN_SM_OPEN);
}
/*---------------------------------------------------------------------------*/
/*STATE 3 TO 2*/
static int TTWAIN_CloseSourceManager(void *hwnd) {
  TTWAIN_EmptyMessageQueue();
  TTwainData.hwnd32SM = (TW_INT32)TTWAIN_GetValidHwnd(hwnd);

  TTwainData.resultCode = TWRC_SUCCESS;

  if (TTWAIN_CloseSource() &&
      TTWAIN_MGR(DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, &TTwainData.hwnd32SM)) {
    assert(TTWAIN_GetState() == TWAIN_SM_LOADED);
  }
  return (TTWAIN_GetState() <= TWAIN_SM_LOADED);
}
/*---------------------------------------------------------------------------*/
/*STATE 2 TO 1*/
int TTWAIN_UnloadSourceManager(void) {
  TTWAIN_CloseSourceManager(NULL);
  return TTWAIN_UnloadSourceManagerPD();
}
/*---------------------------------------------------------------------------*/
int TTWAIN_CloseAll(void *hwnd) {
  TTWAIN_EndXfer();
  TTWAIN_DisableSource();
  TTWAIN_CloseSource();
  TTWAIN_CloseSourceManager(hwnd);
  TTWAIN_UnloadSourceManager();
  TTWAIN_FreeVar();
  return 1;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*    MISC. AUX FUNCTIONS						     */
/*---------------------------------------------------------------------------*/
TWAINSTATE TTWAIN_GetState(void) { return TTwainData.twainState; }
/*---------------------------------------------------------------------------*/
void TTWAIN_SetState(TWAINSTATE status) { TTwainData.twainState = status; }
/*---------------------------------------------------------------------------*/
static int TTWAIN_AbortAllPendingXfers(void) {
  TTWAIN_EndXfer();
  if (TTWAIN_GetState() == TWAIN_TRANSFER_READY) {
    TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET,
              &TTwainData.transferInfo.pendingXfers);
  }
  TTWAIN_EmptyMessageQueue();
  return (TTWAIN_GetState() < TWAIN_TRANSFER_READY);
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*		DATA SOURCE						     */
/*---------------------------------------------------------------------------*/
TW_INT16 TTWAIN_DS(TUINT32 dg, TUINT32 dat, TUINT32 msg,
                   void *pd) { /* Call the current source with a triplet */
  int bOk                   = FALSE;
  static TUINT32 nMemBuffer = 0;

  PRINTF("%s dg=0x%x dat=0x%x msg=0x%x pd=0x%x\n", __FUNCTION__, dg, dat, msg,
         pd);

  assert(TTWAIN_GetState() >= TWAIN_SOURCE_OPEN);
  TTwainData.resultCode = TWRC_FAILURE;
  if (dg == DG_IMAGE) {
    if (dat == DAT_IMAGEMEMXFER) {
      if (msg == MSG_GET && pd != NULL) {
        pTW_IMAGEMEMXFER pmxb = (pTW_IMAGEMEMXFER)pd;
        pmxb->Compression     = TWON_DONTCARE16;
        pmxb->BytesPerRow     = TWON_DONTCARE32;
        pmxb->Columns         = TWON_DONTCARE32;
        pmxb->Rows            = TWON_DONTCARE32;
        pmxb->XOffset         = TWON_DONTCARE32;
        pmxb->YOffset         = TWON_DONTCARE32;
        pmxb->BytesWritten    = TWON_DONTCARE32;
      }
    }
  }
  if (TTwainData.DSM_Entry) {
    TTwainData.resultCode = (*TTwainData.DSM_Entry)(
        &TTwainData.appId, &TTwainData.sourceId, (TW_UINT32)dg, (TW_UINT16)dat,
        (TW_UINT16)msg, (TW_MEMREF)pd);
    bOk = (TTwainData.resultCode == TWRC_SUCCESS);

    if (dg == DG_CONTROL) {
      switch (dat) {
      case DAT_EVENT:
        if (msg == MSG_PROCESSEVENT) {
          if (((pTW_EVENT)pd)->TWMessage == MSG_XFERREADY) {
            TTWAIN_SetState(TWAIN_TRANSFER_READY);
          }
          bOk = (TTwainData.resultCode == TWRC_DSEVENT);
        }
        break;

      case DAT_CAPABILITY:
        break;

      case DAT_PENDINGXFERS:
        if (msg == MSG_ENDXFER && bOk) {
          TTWAIN_SetState(((pTW_PENDINGXFERS)pd)->Count ? TWAIN_TRANSFER_READY
                                                        : TWAIN_SOURCE_ENABLED);
        }
        if (msg == MSG_RESET && bOk) {
          TTWAIN_SetState(TWAIN_SOURCE_ENABLED);
        }
        break;

      case DAT_USERINTERFACE:
        if (msg == MSG_ENABLEDS) {
          if (TTwainData.resultCode == TWRC_FAILURE ||
              TTwainData.resultCode == TWRC_CANCEL) {
            TTWAIN_RecordError();
          } else {
            /* TTwainData.resultCode could be either SUCCESS or CHECKSTATUS */
            TTWAIN_SetState(TWAIN_SOURCE_ENABLED);
            bOk = TRUE;
          }
        }
        if (msg == MSG_DISABLEDS && bOk) {
          TTWAIN_SetState(TWAIN_SOURCE_OPEN);
#ifdef MACOSX
          exitTwainSession();  // exited from TwainUI using close button
#endif
        }
        break;

      case DAT_SETUPMEMXFER:
        if (msg == MSG_GET && bOk) {
          nMemBuffer = 0;
        }
        break;
      } /* switch */
    } else if (dg == DG_IMAGE) {
      if (dat == DAT_IMAGENATIVEXFER || dat == DAT_IMAGEFILEXFER) {
        /* Native and File transfers work much the same way. */
        if (msg == MSG_GET) {
          bOk = (TTwainData.resultCode == TWRC_XFERDONE);
          switch (TTwainData.resultCode) {
          case TWRC_XFERDONE:
          case TWRC_CANCEL:
            TTWAIN_SetState(TWAIN_TRANSFERRING);
            /* Need to acknowledge end of transfer with */
            /* DG_CONTROL / DAT_PENDINGXFERS / MSG_ENDXFER */
            break;

          case TWRC_FAILURE:
          default:
            /* Transfer failed (e.g. insufficient memory, write-locked file) */
            /* check condition code for more info */
            TTWAIN_SetState(TWAIN_TRANSFER_READY);
            /* The image is still pending */
            break;
          } /* switch */
        }
      } else if (dat == DAT_IMAGEMEMXFER) {
        if (msg == MSG_GET) {
          bOk = FALSE;
          switch (TTwainData.resultCode) {
          case TWRC_SUCCESS:
          case TWRC_XFERDONE:
            bOk = TRUE;
            nMemBuffer++;
            TTWAIN_SetState(TWAIN_TRANSFERRING);
            break;

          case TWRC_FAILURE:
            /* "If the failure occurred during the transfer of the first
buffer, the session is in State 6. If the failure occurred
on a subsequent buffer, the session is in State 7."
*/
            TTWAIN_SetState(nMemBuffer == 0 ? TWAIN_TRANSFER_READY
                                            : TWAIN_TRANSFERRING);
            break;

          case TWRC_CANCEL:
            /* Transfer cancelled, no state change. */
            TTWAIN_BreakModalLoop();
            break;
          } /* switch */
        }
      }
    }
  }

  /*
if (!bOk)
TTWAIN_RecordError();
*/
  return TTwainData.resultCode;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*	    DATA SOURCE MANAGER						     */
/*---------------------------------------------------------------------------*/
static int TTWAIN_MGR(TUINT32 dg, TUINT32 dat, TUINT32 msg, void *pd)
/* Call the Source Manager with a triplet */
{
  int bOk               = FALSE;
  TTwainData.resultCode = TWRC_FAILURE;

  if (TTwainData.DSM_Entry) {
    TTwainData.resultCode =
        (*TTwainData.DSM_Entry)(&TTwainData.appId, NULL, (TW_UINT32)dg,
                                (TW_UINT16)dat, (TW_UINT16)msg, (TW_MEMREF)pd);
    bOk = (TTwainData.resultCode == TWRC_SUCCESS);
    if (dg == DG_CONTROL) {
      if (dat == DAT_IDENTITY) {
        if (msg == MSG_OPENDS) {
          if (bOk) {
            /* Source opened - record identity for future triplets */
            memcpy(&TTwainData.sourceId, pd, sizeof(TW_IDENTITY));
            TTWAIN_SetState(TWAIN_SOURCE_OPEN);
          } else { /*RecordError(ED_DSM_FAILURE);*/
            TTWAIN_RecordError();
          }
        }
        if (msg == MSG_CLOSEDS && bOk) {
          TTWAIN_SetState(TWAIN_SM_OPEN);
        }
      }
      if (dat == DAT_PARENT) {
        if (msg == MSG_OPENDSM && bOk) {
          TTWAIN_SetState(TWAIN_SM_OPEN);
        }
        if (msg == MSG_CLOSEDSM && bOk) {
          TTWAIN_SetState(TWAIN_SM_LOADED);
        }
      }
    }
  }
  return bOk;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*			ERRORS						     */
/*---------------------------------------------------------------------------*/
TUINT32 TTWAIN_GetConditionCode(void) {
  TW_STATUS twStatus;
  TW_INT16 rcLast        = TTwainData.resultCode;
  TW_INT16 rc            = TWRC_FAILURE;
  twStatus.ConditionCode = TWCC_BUMMER;

  if (TTWAIN_GetState() >= 4) {
    /* get source status if open */
    rc = TTWAIN_DS(DG_CONTROL, DAT_STATUS, MSG_GET, (TW_MEMREF)&twStatus);
  } else if (TTWAIN_GetState() == 3) {
    /* otherwise get source manager status */
    rc = TTWAIN_MGR(DG_CONTROL, DAT_STATUS, MSG_GET, (TW_MEMREF)&twStatus);
  }
  TTwainData.resultCode = rcLast;

  if (rc != TWRC_SUCCESS) return -1;
  return twStatus.ConditionCode;
}
/*---------------------------------------------------------------------------*/
TUINT32 TTWAIN_GetResultCode(void) { return TTwainData.resultCode; }
/*---------------------------------------------------------------------------*/
int TTWAIN_DSM_HasEntryPoint(void) { return !!(TTwainData.DSM_Entry); }
/*---------------------------------------------------------------------------*/
static int TTWAIN_DoOneTransfer(void) {
  int rc = FALSE;
  switch (TTwainData.transferInfo.transferMech) {
  case TTWAIN_TRANSFERMODE_NATIVE:
    rc = TTWAIN_NativeXferHandler();
    break;
  case TTWAIN_TRANSFERMODE_FILE:
    assert(!"NOT IMPL!");
    /*TTWAIN_FileXferHandler(); */
    break;
  case TTWAIN_TRANSFERMODE_MEMORY:
    rc = TTWAIN_MemoryXferHandler();
    break;
  } /* switch */

  TTwainData.transferInfo.lastTransferWasOk = rc;
  /* If inside ModalEventLoop, break out */
  TTWAIN_BreakModalLoop();

  /* Acknowledge transfer */
  TTWAIN_EndXfer();
  assert(TTWAIN_GetState() == TWAIN_TRANSFER_READY ||
         TTWAIN_GetState() == TWAIN_SOURCE_ENABLED);
  return rc;
}
/*---------------------------------------------------------------------------*/
void TTWAIN_RegisterApp(
    int majorNum, int minorNum, /* app. revision*/
    int nLanguage,    /* (human) language (use TWLG_xxx from TWAIN.H) */
    int nCountry,     /* country (use TWCY_xxx from TWAIN.H)	     */
    char *version,    /* version info string                          */
    char *manufacter, /* name of manufacter			     */
    char *family,     /* product family				     */
    char *product)    /* specific product			     */
{
  memset(&TTwainData.appId, 0, sizeof(TTwainData.appId));
  TTwainData.appId.Id =
      0; /* init to 0, but Source Manager will assign THE real value*/
  TTwainData.appId.Version.MajorNum = majorNum;
  TTwainData.appId.Version.MinorNum = minorNum;
  TTwainData.appId.Version.Language = nLanguage;
  TTwainData.appId.Version.Country  = nCountry;
  TTwainData.appId.ProtocolMajor    = TWON_PROTOCOLMAJOR;
  TTwainData.appId.ProtocolMinor    = TWON_PROTOCOLMINOR;
  TTwainData.appId.SupportedGroups  = DG_IMAGE | DG_CONTROL;
  strcpy((char *)TTwainData.appId.Version.Info, version);
  strcpy((char *)TTwainData.appId.Manufacturer, manufacter);
  strcpy((char *)TTwainData.appId.ProductFamily, family);
  strcpy((char *)TTwainData.appId.ProductName, product);
}
/*---------------------------------------------------------------------------*/
void *TTWAIN_AcquireNative(void *hwnd) {
  void *hnative = NULL;

  TTwainData.transferInfo.lastTransferWasOk = FALSE;

  if (TTwainData.transferInfo.transferMech != TTWAIN_TRANSFERMODE_NATIVE)
    return 0;

  hwnd = TTWAIN_GetValidHwnd(hwnd);
  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    if (!TTWAIN_OpenSourceManager(hwnd)) /* Bring up to state 4 */
    {
      char msg[2048];
      snprintf(msg, sizeof(msg), "Unable to open Source Manager (%s)",
               DSM_FILENAME);
      TTWAIN_ErrorBox(msg);
      return 0;
    }
    if (!TTWAIN_OpenDefaultSource())
      /*TTWAIN_ReportLastError("Unable to open default Data Source.");*/
      TTWAIN_RecordError();
    else
      assert(TTWAIN_GetState() == TWAIN_SOURCE_OPEN);
  }

  if (TTWAIN_GetState() >= TWAIN_SOURCE_OPEN)
    hnative = TTWAIN_WaitForNativeXfer(hwnd);

  if (!TTwainData.transferInfo.multiTransfer) {
    /* shut everything down in the right sequence */
    TTWAIN_AbortAllPendingXfers(); /* TRANSFER_READY or TRANSFERRING ->
                                      SOURCE_ENABLED */
    TTWAIN_UnloadSourceManager();
  }

  TTwainData.transferInfo.lastTransferWasOk = !!(hnative);
  return hnative;
}
/*---------------------------------------------------------------------------*/
#ifdef _WIN32

typedef void(MyFun)(HWND);

/*it's an hack function to force bring to top the twain UI module window     */
static BOOL CALLBACK myHackEnumFunction(HWND hwnd, LPARAM lParam) {
  MyFun *f = 0;

#define TITLESIZE (1024)
  char title[TITLESIZE + 1];
  int len;

  GetWindowText(hwnd, (char *)&title, TITLESIZE);
  if (title[0] == 0x00) return 1; /*continue the search....*/

  len = strlen(TTwainData.sourceId.ProductName);
  if (!len) return 0;

  len--;

  while (
      len &&
      (isdigit(TTwainData.sourceId.ProductName[len]) /*skip digit at the end*/
       || TTwainData.sourceId.ProductName[len] == '.')) /*skip . */
    len--;

  if (len && !strncmp(title, TTwainData.sourceId.ProductName, len)) {
    /*
char dbg_str[1024];
snprintf(dbg_str, sizeof(dbg_str), "set focus on 0x%8x %s\n",hwnd, title);
OutputDebugString(dbg_str);
*/
    f = (MyFun *)lParam;
    f(hwnd);

    return FALSE; /*it means stop the search...*/
  }
  return 1; /*continue the search....*/
}

/*---------------------------------------------------------------------------*/

static BOOL CALLBACK myHackEnumFunction2(HWND hwnd, LPARAM lParam) {
  MyFun *f = 0;
  char *ptr;

#define TITLESIZE (1024)
  char title[TITLESIZE + 1];

  GetWindowText(hwnd, (char *)&title, TITLESIZE);
  if (title[0] == 0x00) return 1; /*continue the search....*/

  ptr = strstr(title, TTwainData.sourceId.Manufacturer);
  if (!ptr) return 1; /*continue the search....*/

  f = (MyFun *)lParam;
  f(hwnd);

  return FALSE; /*it means stop the search...*/
}

/*---------------------------------------------------------------------------*/

static void bringToTop(HWND hwnd) { BringWindowToTop(hwnd); }

static void putToBottom(HWND hwnd) {
  const int unused = 0;

  BOOL rc = SetWindowPos(hwnd,         // handle to window
                         HWND_BOTTOM,  // placement-order handle
                         unused,       // horizontal position
                         unused,       // vertical position
                         unused,       // width
                         unused,       // height
                         SWP_ASYNCWINDOWPOS | SWP_NOMOVE |
                             SWP_NOSIZE  // window-positioning options
                         );
}

static void myHackFunction(int v) {
  BOOL rc;
  if (v == 1)
    rc = EnumWindows((WNDENUMPROC)myHackEnumFunction, (LPARAM)&bringToTop);
  else
    rc = EnumWindows((WNDENUMPROC)myHackEnumFunction, (LPARAM)&putToBottom);

  if (rc) /* it means that myHackEnumFunction fails to find the proper window to
             bring up or put down*/
  {
    if (v == 1)
      rc = EnumWindows((WNDENUMPROC)myHackEnumFunction2, (LPARAM)&bringToTop);
    else
      rc = EnumWindows((WNDENUMPROC)myHackEnumFunction2, (LPARAM)&putToBottom);
  }
}
#else
static void myHackFunction(int v) { /*it's empty...*/
}
#endif
/*---------------------------------------------------------------------------*/
int TTWAIN_AcquireMemory(void *hwnd) {
  int rc                                    = FALSE;
  TTwainData.transferInfo.lastTransferWasOk = FALSE;

  if (TTwainData.transferInfo.transferMech != TTWAIN_TRANSFERMODE_MEMORY)
    return FALSE;

  myHackFunction(1);

  hwnd = TTWAIN_GetValidHwnd(hwnd);
  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    if (!TTWAIN_OpenSourceManager(hwnd)) /* Bring up to state 4 */
    {
      TTWAIN_ErrorBox("Unable to open Source Manager (" DSM_FILENAME ")");
      return FALSE;
    }
    if (!TTWAIN_OpenDefaultSource())
      /*TTWAIN_ReportLastError("Unable to open default Data Source.");*/
      TTWAIN_RecordError();
    else
      assert(TTWAIN_GetState() == TWAIN_SOURCE_OPEN);
  }

  if (TTWAIN_GetState() >= TWAIN_SOURCE_OPEN)
    rc = TTWAIN_WaitForMemoryXfer(hwnd);

  if (!TTwainData.transferInfo.multiTransfer) {
    /* shut everything down in the right sequence */
    TTWAIN_AbortAllPendingXfers(); /* TRANSFER_READY or TRANSFERRING ->
                                      SOURCE_ENABLED */
    TTWAIN_UnloadSourceManager();
  }

  myHackFunction(0);

  return TTwainData.transferInfo.lastTransferWasOk;
}
/*---------------------------------------------------------------------------*/
void TTWAIN_StopAcquire(void) { TTwainData.transferInfo.oneAtLeast = FALSE; }
/*---------------------------------------------------------------------------*/
static void *TTWAIN_WaitForNativeXfer(void *hwnd) {
  TTwainData.transferInfo.hDib = NULL;
  if (TTWAIN_GetState() >= TWAIN_SOURCE_OPEN)
    TTWAIN_WaitForXfer(hwnd);
  else
    TTWAIN_ErrorBox("TWAIN_WaitForNativeXfer called in state < 4.");
  return TTwainData.transferInfo.hDib;
}
/*---------------------------------------------------------------------------*/
static int TTWAIN_WaitForMemoryXfer(void *hwnd) {
  int rc = FALSE;
  if (TTWAIN_GetState() >= TWAIN_SOURCE_OPEN)
    rc = TTWAIN_WaitForXfer(hwnd);
  else
    TTWAIN_ErrorBox("TWAIN_WaitForNativeXfer called in state < 4.");
  return rc;
}
/*---------------------------------------------------------------------------*/
static int TTWAIN_WaitForXfer(void *hwnd) {
  int bWasEnabled;
  int rc = FALSE;
  /* Make up a valid window if we weren't given one */
  hwnd = TTWAIN_GetValidHwnd(hwnd);
  /* Disable the parent window during the modal acquire */
  bWasEnabled = (TTWAIN_EnableWindow(hwnd, FALSE) == 0);

  TTwainData.transferInfo.oneAtLeast = TRUE;
  /*
TTWAIN_DS( DG_CONTROL,DAT_PENDINGXFERS, MSG_ENDXFER,
       (TW_MEMREF)&TTwainData.transferInfo.pendingXfers);
*/
  do {
    if (TTWAIN_GetState() == TWAIN_TRANSFER_READY)
      rc = TTWAIN_DoOneTransfer();
    else if (TTWAIN_GetState() >= TWAIN_SOURCE_ENABLED ||
             TTWAIN_EnableSource(hwnd)) {
      /* source is enabled, wait for transfer or source closed */
      if (TTwainData.resultCode != TWRC_CANCEL)
        TTWAIN_ModalEventLoop();
      else {
        TTWAIN_BreakModalLoop();
        break;
      }
    } else
      /*TTWAIN_ReportLastError("Failed to enable Data Source.");*/
      TTWAIN_RecordError();
  } while (TTwainData.transferInfo.pendingXfers.Count &&
           TTwainData.transferInfo.oneAtLeast /*&& rc*/);

  /* Re-enable the parent window if it was enabled */
  TTWAIN_EnableWindow(hwnd, bWasEnabled);
  return rc;
}
/*---------------------------------------------------------------------------*/
void TTWAIN_FreeMemory(void *hMem) {
  free(hMem);
  /*
if (hMem)
GLOBAL_FREE(hMem);
*/
}
/*---------------------------------------------------------------------------*/
static void TTWAIN_ModalEventLoop(void) { TTWAIN_ModalEventLoopPD(); }
/*---------------------------------------------------------------------------*/
static void TTWAIN_BreakModalLoop(void) { TTwainData.breakModalLoop = TRUE; }
/*---------------------------------------------------------------------------*/
static void TTWAIN_EmptyMessageQueue(void) { TTWAIN_EmptyMessageQueuePD(); }
/*---------------------------------------------------------------------------*/
static int TTWAIN_NativeXferHandler(void) {
  TW_UINT32 hNative;

  assert(TTWAIN_GetState() == TWAIN_TRANSFER_READY);
  if (TTWAIN_DS(DG_IMAGE, DAT_IMAGENATIVEXFER, MSG_GET, &hNative) ==
      TWRC_XFERDONE)
    TTwainData.transferInfo.hDib = (void *)hNative;
  else
    TTwainData.transferInfo.hDib = NULL;
  return (!!TTwainData.transferInfo.hDib);
}
/*---------------------------------------------------------------------------*/
static int TTWAIN_MemoryXferHandler(void) {
  TW_IMAGEMEMXFER *imageMemXfer = 0;
  TW_HANDLE imageMemXferH       = 0;
  TW_HANDLE transferBufferH     = 0;
  TW_SETUPMEMXFER setup;
  TW_IMAGEINFO info;
  TW_IMAGELAYOUT imageLayout;
  TUINT32 nTransferDone;
  TW_INT16 rc1, rc2, rc3, rc4, twRC2;
  int ret               = FALSE;
  int stopScanning      = 0;
  UCHAR *transferBuffer = 0;
  UCHAR *sourceBuffer   = 0;
  UCHAR *targetBuffer   = 0;
  unsigned int rows;
  double pixSize;
  int extraX              = 0;
  int extraY              = 0;
  TW_UINT32 rowsToCopy    = 0;
  TW_UINT32 rowsRemaining = 0;
  TW_UINT32 bytesToCopy   = 0;
  TW_UINT32 bytesToWrap   = 0;
  TW_UINT32 memorySize    = 0;
  int imgInfoOk; /* on Mac often (always) is impossible to get the imageinfo
                                                                           about
                    the transfer... so no I can't prealloc memory
                                                                           and
                    do other checks about size etc...
                                                                  */

  /*printf("%s\n", __PRETTY_FUNCTION__);*/

  memset(&info, 0, sizeof(TW_IMAGEINFO));
  rc1       = TTWAIN_DS(DG_IMAGE, DAT_IMAGEINFO, MSG_GET, (TW_MEMREF)&info);
  imgInfoOk = (rc1 == TWRC_SUCCESS);

  /*printf("get image info returns %d\n", imgInfoOk);*/

  rc4 = TTWAIN_DS(DG_IMAGE, DAT_IMAGELAYOUT, MSG_GET, &imageLayout);

  /* determine the transfer buffer size */
  rc2 = TTWAIN_DS(DG_CONTROL, DAT_SETUPMEMXFER, MSG_GET, (TW_MEMREF)&setup);
  transferBufferH = GLOBAL_ALLOC(GMEM_FIXED, setup.Preferred);
  if (!transferBufferH) return FALSE;
  transferBuffer = (UCHAR *)GLOBAL_LOCK(transferBufferH);

  if (imgInfoOk) {
    pixSize    = info.BitsPerPixel / 8.0;
    memorySize = info.ImageLength * CEIL(info.ImageWidth * pixSize);
  } else {
    /* we need to allocate incrementally the memory needs to store the image*/
    memorySize =
        setup.Preferred; /* start using the setupmemxfer.preferred size*/
    pixSize = 3;
  }

  if (TTwainData.transferInfo.usageMode == TTWAIN_MODE_UNLEASHED) {
    /*
TTwainData.transferInfo = GLOBAL_ALLOC(GMEM_FIXED, memorySize);
*/
    TTwainData.transferInfo.memoryBuffer = (UCHAR *)malloc(memorySize);

    if (!TTwainData.transferInfo.memoryBuffer) {
      /*tmsg_error("unable to allocate memory!");*/
      return FALSE;
    }
    if (imgInfoOk) {
      TTwainData.transferInfo.memorySize  = memorySize;
      TTwainData.transferInfo.preferredLx = info.ImageWidth;
      TTwainData.transferInfo.preferredLy = info.ImageLength;
    } else {
      TTwainData.transferInfo.memorySize  = setup.Preferred;
      TTwainData.transferInfo.preferredLx = 0;
      TTwainData.transferInfo.preferredLy = 0;
    }
  }

  extraX = info.ImageWidth - TTwainData.transferInfo.preferredLx;
  extraY = info.ImageLength - TTwainData.transferInfo.preferredLy;

  rowsRemaining = min(TTwainData.transferInfo.preferredLy, info.ImageLength);

  targetBuffer = TTwainData.transferInfo.memoryBuffer;

  /*clean-up the buffer
memset(targetBuffer, 0xff, TTwainData.transferInfo.memorySize);
*/

  imageMemXferH = GLOBAL_ALLOC(GMEM_FIXED, sizeof(TW_IMAGEMEMXFER));
  if (!imageMemXferH) return FALSE;

  imageMemXfer = (TW_IMAGEMEMXFER *)GLOBAL_LOCK(imageMemXferH);

  imageMemXfer->Memory.TheMem                = (char *)transferBuffer;
  imageMemXfer->Memory.Length                = setup.Preferred;
  imageMemXfer->Memory.Flags                 = TWMF_APPOWNS | TWMF_POINTER;
  TTwainData.transferInfo.pendingXfers.Count = 0;
  /* transfer the data -- loop until done or canceled */
  nTransferDone = 0;
  do {
    rc3 =
        TTWAIN_DS(DG_IMAGE, DAT_IMAGEMEMXFER, MSG_GET, (TW_MEMREF)imageMemXfer);
    nTransferDone++;
    switch (rc3) {
    case TWRC_SUCCESS:
      PRINTF("IMAGEMEMXFER, GET, returns SUCCESS\n");
      if (imgInfoOk) {
        TW_UINT32 colsToCopy;
        rowsToCopy = min(imageMemXfer->Rows, rowsRemaining);
        colsToCopy = min(imageMemXfer->Columns,
                         (unsigned long)TTwainData.transferInfo.preferredLx);
        bytesToCopy = CEIL(colsToCopy * pixSize);
        bytesToWrap = CEIL(TTwainData.transferInfo.preferredLx * pixSize);
      } else {
        TW_UINT32 newMemorySize;
        rowsToCopy  = imageMemXfer->Rows;
        bytesToCopy = imageMemXfer->BytesPerRow;
        bytesToWrap = bytesToCopy;
        newMemorySize =
            (TTwainData.transferInfo.preferredLy + imageMemXfer->Rows) *
            imageMemXfer->BytesPerRow;
        if (TTwainData.transferInfo.memorySize < newMemorySize) {
          TTwainData.transferInfo.memoryBuffer = (UCHAR *)realloc(
              TTwainData.transferInfo.memoryBuffer, newMemorySize);
          TTwainData.transferInfo.memorySize = newMemorySize;
          targetBuffer =
              TTwainData.transferInfo.memoryBuffer +
              (TTwainData.transferInfo.preferredLy * imageMemXfer->BytesPerRow);
        }
        TTwainData.transferInfo.preferredLy += rowsToCopy;
        if ((int)imageMemXfer->Columns > TTwainData.transferInfo.preferredLx)
          TTwainData.transferInfo.preferredLx = imageMemXfer->Columns;
      }

      sourceBuffer = (UCHAR *)imageMemXfer->Memory.TheMem;
      if (TTwainData.transferInfo.nextImageNeedsToBeInverted)
        INVERT_BYTE(sourceBuffer, bytesToCopy)

      for (rows = 0; rows < rowsToCopy; rows++) {
        memcpy(targetBuffer, sourceBuffer, bytesToCopy);
        targetBuffer += bytesToWrap;
        sourceBuffer += imageMemXfer->BytesPerRow;
      }
      rowsRemaining -= rowsToCopy;
      break;

    case TWRC_XFERDONE:
      PRINTF("IMAGEMEMXFER, GET, returns XFERDONE\n");
      /*copy the last transfer data*/
      if (imgInfoOk) {
        TW_UINT32 colsToCopy;
        rowsToCopy = min(imageMemXfer->Rows, rowsRemaining);
        colsToCopy = min(imageMemXfer->Columns,
                         (unsigned long)TTwainData.transferInfo.preferredLx);
        bytesToCopy = CEIL(colsToCopy * pixSize);
        bytesToWrap = CEIL(TTwainData.transferInfo.preferredLx * pixSize);
      } else {
        TW_UINT32 newMemorySize;
        rowsToCopy  = imageMemXfer->Rows;
        bytesToCopy = imageMemXfer->BytesPerRow;
        bytesToWrap = bytesToCopy;
        newMemorySize =
            (TTwainData.transferInfo.preferredLy + imageMemXfer->Rows) *
            imageMemXfer->BytesPerRow;
        if (TTwainData.transferInfo.memorySize < newMemorySize) {
          TTwainData.transferInfo.memoryBuffer = (UCHAR *)realloc(
              TTwainData.transferInfo.memoryBuffer, newMemorySize);
          TTwainData.transferInfo.memorySize = newMemorySize;
          targetBuffer =
              TTwainData.transferInfo.memoryBuffer +
              (TTwainData.transferInfo.preferredLy * imageMemXfer->BytesPerRow);
        }
        TTwainData.transferInfo.preferredLy += rowsToCopy;
        if ((int)imageMemXfer->Columns > TTwainData.transferInfo.preferredLx)
          TTwainData.transferInfo.preferredLx = imageMemXfer->Columns;
      }
      sourceBuffer = (UCHAR *)imageMemXfer->Memory.TheMem;
      if (TTwainData.transferInfo.nextImageNeedsToBeInverted)
        INVERT_BYTE(sourceBuffer, bytesToCopy)

      for (rows = 0; rows < rowsToCopy; rows++) {
        memcpy(targetBuffer, sourceBuffer, bytesToCopy);
        targetBuffer += bytesToWrap;
        sourceBuffer += imageMemXfer->BytesPerRow;
      }
      rowsRemaining -= rowsToCopy;
      PRINTF("get pending xfers\n");
      twRC2 = TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER,
                        (TW_MEMREF)&TTwainData.transferInfo.pendingXfers);
      if (twRC2 != TWRC_SUCCESS) {
        printf("pending xfers != success");
        ret = FALSE;
        goto done;
      }
      PRINTF(" pending count = %d\n",
             TTwainData.transferInfo.pendingXfers.Count);
      if (TTwainData.transferInfo.pendingXfers.Count == 0) {
        ret = TRUE;
        goto done;
      }
      if (TTwainData.transferInfo.pendingXfers.Count == 0xffff) {
        ret = TRUE;
        goto done;
      }
      if (TTwainData.transferInfo.pendingXfers.Count == 0xfffe) {
        ret = TRUE;
        goto done;
      }
      ret = TRUE;
      goto done;

    case TWRC_CANCEL:
      TTWAIN_RecordError();
      twRC2 = TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER,
                        (TW_MEMREF)&TTwainData.transferInfo.pendingXfers);
      if (twRC2 != TWRC_SUCCESS) {
        ret = FALSE;
        goto done;
      }
      if (TTwainData.transferInfo.pendingXfers.Count == 0) {
        ret = FALSE;
        goto done;
      }
      break;

    case TWRC_FAILURE:
      PRINTF("IMAGEMEMXFER, GET, returns FAILURE\n");
      TTWAIN_RecordError();
      twRC2 = TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER,
                        (TW_MEMREF)&TTwainData.transferInfo.pendingXfers);
      if (twRC2 != TWRC_SUCCESS) {
        ret = FALSE;
        goto done;
      }
      if (TTwainData.transferInfo.pendingXfers.Count == 0) {
        ret = FALSE;
        goto done;
      }
      break;

    default:
      PRINTF("IMAGEMEMXFER, GET, returns ?!? Default handler called\n");
      /* Abort the image */
      TTWAIN_RecordError();
      twRC2 = TTWAIN_DS(DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER,
                        (TW_MEMREF)&TTwainData.transferInfo.pendingXfers);
      if (twRC2 != TWRC_SUCCESS) {
        ret = FALSE;
        goto done;
      }
      if (TTwainData.transferInfo.pendingXfers.Count == 0) {
        ret = FALSE;
        goto done;
      }
    }
  } while (rc3 == TWRC_SUCCESS);

done:
  if (ret == TRUE) {
    if (TTwainData.callback.onDoneCb) {
      float xdpi, ydpi;
      TTWAIN_PIXTYPE pixType;
      xdpi = TTWAIN_Fix32ToFloat(info.XResolution);
      ydpi = TTWAIN_Fix32ToFloat(info.YResolution);

      if (imgInfoOk) {
        xdpi = TTWAIN_Fix32ToFloat(info.XResolution);
        ydpi = TTWAIN_Fix32ToFloat(info.YResolution);
        switch (BB(info.PixelType, info.BitsPerPixel)) {
        case BB(TWPT_BW, 1):
          pixType = TTWAIN_BW;
          break;
        case BB(TWPT_GRAY, 8):
          pixType = TTWAIN_GRAY8;
          break;
        case BB(TWPT_RGB, 24):
          pixType = TTWAIN_RGB24;
          break;
        default:
          pixType = TTWAIN_RGB24;
          break;
        }
      } else {
        float lx = TTWAIN_Fix32ToFloat(imageLayout.Frame.Right) -
                   TTWAIN_Fix32ToFloat(imageLayout.Frame.Left);
        float ly = TTWAIN_Fix32ToFloat(imageLayout.Frame.Bottom) -
                   TTWAIN_Fix32ToFloat(imageLayout.Frame.Top);

        xdpi = (float)TTwainData.transferInfo.preferredLx / lx;
        ydpi = (float)TTwainData.transferInfo.preferredLy / ly;

        switch (imageMemXfer->BytesPerRow /
                TTwainData.transferInfo.preferredLx) {
        case 1:
          pixType = TTWAIN_GRAY8;
          break;
        case 3:
          pixType = TTWAIN_RGB24;
          break;
        default: {
          double b = (imageMemXfer->BytesPerRow /
                      (double)TTwainData.transferInfo.preferredLx);
          if ((b >= 0.125) && (b < 8))
            pixType = TTWAIN_BW;
          else {
            printf("unable to det pix type assume RGB24\n");
            pixType = TTWAIN_RGB24;
          }
          break;
        }
        }
      }
      stopScanning = !TTwainData.callback.onDoneCb(
          TTwainData.transferInfo.memoryBuffer, pixType,
          TTwainData.transferInfo.preferredLx,
          TTwainData.transferInfo.preferredLy,
          TTwainData.transferInfo.preferredLx, xdpi, ydpi,
          TTwainData.callback.onDoneArg);
#ifdef MACOSX
      PRINTF("stopScanning = %d\n", stopScanning);
      exitTwainSession();
#endif
    }
  } else /*ret == FALSE*/
  {
    if (TTwainData.callback.onErrorCb) {
      TTwainData.callback.onErrorCb(TTwainData.callback.onErrorArg, 0);
    }
  }

  if (imageMemXferH) {
    GLOBAL_UNLOCK(imageMemXferH);
    GLOBAL_FREE(imageMemXferH);
  }

  if (transferBufferH) {
    GLOBAL_UNLOCK(transferBuffer);
    GLOBAL_FREE(transferBufferH);
  }
  return ret && !stopScanning;
}
/*---------------------------------------------------------------------------*/
int TTWAIN_InitVar(void) {
  char *c;
  if (TTwainData.initDone) return TRUE;

  TTwainData.DSM_Entry                  = 0;
  TTwainData.hwnd32SM                   = 0;
  TTwainData.twainAvailable             = AVAIABLE_DONTKNOW;
  TTwainData.breakModalLoop             = FALSE;
  TTwainData.UIStatus                   = FALSE;
  TTwainData.twainState                 = TWAIN_PRESESSION;
  TTwainData.transferInfo.hDib          = 0;
  TTwainData.transferInfo.multiTransfer = TRUE;
  TTwainData.supportedCaps              = 0;
  TTwainData.resultCode                 = 0;
  TTwainData.ErrRC                      = 0;
  TTwainData.ErrCC                      = 0;
  TTwainData.modalStatus                = FALSE;
  TTwainData.appId.Id                   = 0;
  TTWAIN_ConvertRevStrToRevNum(RELEASE_STR, &TTwainData.appId.Version.MajorNum,
                               &TTwainData.appId.Version.MinorNum);
  TTwainData.appId.Version.Language = TWLG_USA;
  TTwainData.appId.Version.Country  = TWCY_USA;
  strcpy((char *)TTwainData.appId.Version.Info, TITLEBAR_STR);

  TTwainData.appId.ProtocolMajor   = TWON_PROTOCOLMAJOR;
  TTwainData.appId.ProtocolMinor   = TWON_PROTOCOLMINOR;
  TTwainData.appId.SupportedGroups = DG_IMAGE | DG_CONTROL;

  c = (char *)TTwainData.appId.Manufacturer;
#ifdef MACOSX
  *c = strlen(COMPANY);
  c++;
#endif
  strcpy(c, COMPANY);

  c = (char *)TTwainData.appId.ProductFamily;
#ifdef MACOSX
  *c = strlen(PRODUCT);
  c++;
#endif
  strcpy(c, PRODUCT);

  c = (char *)TTwainData.appId.ProductName;
#ifdef MACOSX
  *c = strlen(TwProgramName);
  c++;
#endif
  strcpy(c, TwProgramName);

  TTwainData.initDone = TRUE;
  return TRUE;
}
/*---------------------------------------------------------------------------*/
static void TTWAIN_FreeVar(void) {
  if (TTwainData.supportedCaps) {
    /* MEMORY LEAK !
GLOBAL_FREE(TTwainData.supportedCaps);
*/
    TTwainData.supportedCaps = 0;
  }
}
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
