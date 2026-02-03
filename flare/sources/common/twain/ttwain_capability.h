#pragma once

#ifndef __TTWAIN_CAP_H__
#define __TTWAIN_CAP_H__

#include "ttwain.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/*	      GET CAP							     */
/*---------------------------------------------------------------------------*/
int TTWAIN_GetCap(TW_UINT16 cap_id, TW_UINT16 conType, void *data,
                  TUINT32 *cont_size);
int TTWAIN_GetCapCurrent(TW_UINT16 cap_id, TW_UINT16 conType, void *data,
                         TUINT32 *cont_size);
int TTWAIN_GetCapQuery(TW_UINT16 cap_id, TW_UINT16 *pattern);
int TTWAIN_SetCap(TW_UINT16 cap_id, TW_UINT16 conType, TW_UINT16 itemType,
                  TW_UINT32 *value);

#ifdef __cplusplus
}
#endif

#endif
