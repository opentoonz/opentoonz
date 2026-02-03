#pragma once

#ifndef __TTWAIN_UTILP_H__
#define __TTWAIN_UTILP_H__

#ifdef __cplusplus
extern "C" {
#endif

int TTWAIN_NegotiateXferCount(TUINT32 nXfers);
int TTWAIN_InitSupportedCaps(void);
int TTWAIN_GetModalStatus(void);

#ifdef __cplusplus
}
#endif

#endif /*__TTWAIN_UTILP_H__*/
