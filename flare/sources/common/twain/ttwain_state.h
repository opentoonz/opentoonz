#pragma once

#ifndef __TTWAIN_STATE_H__
#define __TTWAIN_STATE_H__

#include "ttwain.h"
#include "ttwainP.h"

#ifdef __cplusplus
extern "C" {
#endif

int TTWAIN_LoadSourceManager(void);
int TTWAIN_UnloadSourceManager(void);
TUINT32 TTWAIN_GetResultCode(void);
TUINT32 TTWAIN_GetConditionCode(void);
int TTWAIN_DSM_HasEntryPoint(void);
TW_INT16 TTWAIN_DS(TUINT32 dg, TUINT32 dat, TUINT32 msg, void *pd);
TWAINSTATE TTWAIN_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
