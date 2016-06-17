

#include <windows.h>
#include <assert.h>
#include "ttwain_win.h"
#include "ttwain_winPD.h"
#include "ttwain_state.h"
#include "ttwain_error.h"

#define HINSTLIB0 0
static HWND Dummy = 0; /* proxy window */

extern int TTWAIN_MessageHook(void *lpmsg);

static HWND CreateDummyWindow(void) {
  HWND hwnd;
  hwnd = CreateWindow("STATIC",                      // class
                      "Acquire Dummy",               // title
                      WS_POPUPWINDOW,                // style
                      CW_USEDEFAULT, CW_USEDEFAULT,  // x, y
                      CW_USEDEFAULT, CW_USEDEFAULT,  // width, height
                      HWND_DESKTOP,                  // parent window
                      NULL,                          // hmenu
                      HINSTLIB0,                     // hinst
                      NULL);                         // lpvparam
  return hwnd;
}
/*---------------------------------------------------------------------------*/
void *TTWAIN_GetValidHwndPD(void *_hwnd)
// Returns a valid window handle as follows:
// If hwnd is a valid window handle, hwnd is returned.
// Otherwise a proxy window handle is created and returned.
// Once created, a proxy window handle is destroyed when
// the source manager is unloaded.
// If hwnd is an invalid window handle (other than NULL)
// an error box is displayed.
{
  HWND hwnd = (HWND)_hwnd;
  if (!IsWindow(hwnd)) {
    if (hwnd != NULL) {
      assert(!"Window handle is invalid");
      hwnd = NULL;
    }
    if (!Dummy) {
      Dummy = CreateDummyWindow();
      if (!IsWindow(Dummy)) {
        assert(!"Unable to create Dummy window");
        Dummy = NULL;
      }
    }
    hwnd = Dummy;
  }
  return (void *)hwnd;
}
/*---------------------------------------------------------------------------*/
void TTWAIN_EmptyMessageQueuePD(void) {
  MSG msg;
#ifdef _DEBUG
  OutputDebugString("EmptyMsgQ<");
#endif
  while (PeekMessage((LPMSG)&msg, NULL, 0, 0, PM_REMOVE)) {
    if (!TTWAIN_MessageHook((LPMSG)&msg)) {
      TranslateMessage((LPMSG)&msg);
      DispatchMessage((LPMSG)&msg);
#ifdef _DEBUG
      OutputDebugString("-");
#endif
    } else {
#ifdef _DEBUG
      OutputDebugString("T");
#endif
    }
  }
#ifdef _DEBUG
  OutputDebugString(">\n");
#endif
}

void TTWAIN_ModalEventLoopPD(void) {
  MSG msg;
  // Clear global breakout flag
  TTwainData.breakModalLoop = FALSE;

  while ((TTWAIN_GetState() >= TWAIN_SOURCE_ENABLED) &&
         !TTwainData.breakModalLoop && GetMessage((LPMSG)&msg, NULL, 0, 0)) {
    if (!TTWAIN_MessageHook((LPMSG)&msg)) {
      TranslateMessage((LPMSG)&msg);
      DispatchMessage((LPMSG)&msg);
    }
  }  // while
  TTwainData.breakModalLoop = FALSE;
}
int TTWAIN_EnableWindowPD(void *hwnd, int flag) {
  return EnableWindow(hwnd, flag);
}
