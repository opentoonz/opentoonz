

#include "ttwain_win.h"
#include "ttwain_winPD.h"

#ifdef __cplusplus
extern "C" {
#endif

void *TTWAIN_GetValidHwnd(void *hwnd) { return TTWAIN_GetValidHwndPD(hwnd); }

int TTWAIN_EnableWindow(void *hwnd, int flag) {
  return TTWAIN_EnableWindowPD(hwnd, flag);
}

// static void TTWAIN_EmptyMessageQueue(void) { TTWAIN_EmptyMessageQueuePD(); }

// static void TTWAIN_ModalEventLoop(void) { TTWAIN_ModalEventLoopPD(); }

#ifdef __cplusplus
}
#endif
