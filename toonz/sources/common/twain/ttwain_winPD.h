#pragma once

#ifndef __TTWAIN_WIN_PD_H__
#define __TTWAIN_WIN_PD_H__

#ifdef __cplusplus
extern "C" {
#endif

void *TTWAIN_GetValidHwndPD(void *hwnd);
int TTWAIN_EnableWindowPD(void *hwnd, int flag);
void TTWAIN_EmptyMessageQueuePD(void);
void TTWAIN_ModalEventLoopPD(void);

#ifdef __cplusplus
}
#endif

#endif
