#pragma once

#ifndef __TTWAIN_WIN_H__
#define __TTWAIN_WIN_H__

#ifdef __cplusplus
extern "C" {
#endif

void *TTWAIN_GetValidHwnd(void *hwnd);
int TTWAIN_EnableWindow(void *hwnd, int flag);

#ifdef __cplusplus
}
#endif

#endif
