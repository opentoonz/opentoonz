#pragma once

#ifndef __TTWAIN_CONV_H__
#define __TTWAIN_CONV_H__

#include "twain.h"

#ifdef __cplusplus
extern "C" {
#endif

float TTWAIN_Fix32ToFloat(TW_FIX32 fix);
TW_FIX32 TTWAIN_FloatToFix32(float fl);
void TTWAIN_ConvertRevStrToRevNum(const char *rev_str, TW_UINT16 *maj_num,
                                  TW_UINT16 *min_num);

#ifdef __cplusplus
}
#endif

#endif
