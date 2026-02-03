#pragma once

#ifndef __TTWAIN_UTIL_H__
#define __TTWAIN_UTIL_H__

#include "ttwain.h"

#undef TNZAPI
#ifdef TNZ_IS_DEVICELIB
#define TNZ_EXPORT_API
#else
#define TNZ_IMPORT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

int TTWAIN_IsAvailable(void);
void TTWAIN_SetAvailable(TWAINAVAILABLE status);
char *TTWAIN_GetLastError(TUINT32 *rc, TUINT32 *cc);

int TTWAIN_GetResolution(float *min, float *max, float *step, float *def);
int TTWAIN_GetOpticalResolution(float *min, float *max, float *step,
                                float *def);

int TTWAIN_GetBrightness(float *min, float *max, float *step, float *def);
int TTWAIN_GetContrast(float *min, float *max, float *step, float *def);
int TTWAIN_GetThreshold(float *min, float *max, float *step, float *def);

int TTWAIN_GetPhysicalWidthWAdf(float *width);
int TTWAIN_GetPhysicalHeightWAdf(float *height);

int TTWAIN_GetMinimumWidthWAdf(float *width);
int TTWAIN_GetMinimumHeightWAdf(float *height);

int TTWAIN_GetPhysicalWidthWoAdf(float *width);
int TTWAIN_GetPhysicalHeightWoAdf(float *height);

int TTWAIN_GetMinimumWidthWoAdf(float *width);
int TTWAIN_GetMinimumHeightWoAdf(float *height);

int TTWAIN_SupportsPixelType(TTWAIN_PIXTYPE pix);
int TTWAIN_SupportsFeeder(void);
int TTWAIN_SupportsCompressionType(TW_UINT16 comprType);

int TTWAIN_GetSupportedCaps(void);
int TTWAIN_GetFeeder(void);
int TTWAIN_IsFeederLoaded(void);
/* this function should return the current value */
int TTWAIN_GetCurrentPixelType(TTWAIN_PIXTYPE *pixType);
/*                                          */

int TTWAIN_IsCapResolutionSupported(void);
int TTWAIN_IsCapOpticalResolutionSupported(void);
int TTWAIN_IsCapBrightnessSupported(void);
int TTWAIN_IsCapContrastSupported(void);
int TTWAIN_IsCapThresholdSupported(void);
int TTWAIN_IsCapPhysicalWidthSupported(void);
int TTWAIN_IsCapPhysicalHeightSupported(void);
int TTWAIN_IsCapMinimumWidthSupported(void);
int TTWAIN_IsCapMinimumHeightSupported(void);
int TTWAIN_IsCapPixelTypeSupported(void);
int TTWAIN_IsCapFeederSupported(void);
int TTWAIN_IsCapImageLayoutSupported(void);
int TTWAIN_IsCapOrientationSupported(void);
int TTWAIN_IsCapDeviceOnLineSupported(void);
int TTWAIN_IsCapBitDepthSupported(void);
int TTWAIN_IsCapBitOrderSupported(void);
int TTWAIN_IsCapCompressionSupported(void);

int TTWAIN_SetResolution(float resolution);
int TTWAIN_SetContrast(float contrast);
int TTWAIN_SetBrightness(float brightness);
int TTWAIN_SetThreshold(float threshold);

int TTWAIN_SetXScaling(float scale);
int TTWAIN_SetYScaling(float scale);

int TTWAIN_SetPixelType(TTWAIN_PIXTYPE pixtype);
int TTWAIN_SetBitDepth(USHORT bitDepth);
int TTWAIN_SetBitOrder(TTWAIN_BITORDER bitOrder);

int TTWAIN_SetImageLayout(float L, float T, float R, float B);
int TTWAIN_SetOrientation(USHORT orientation);

int TTWAIN_SetFeeder(int status); /* TRUE->enabled */
int TTWAIN_SetPage(void);

void TTWAIN_DumpCapabilities(void (*trace_fun)(const char *fmt, ...));

/*  USER INTERFACE */
int TTWAIN_HasControllableUI(void);
/* Return 1 if source claims UI can be hidden (see SetUIStatus above)
   Return 0 if source says UI *cannot* be hidden
   Return -1 if source (pre TWAIN 1.6) cannot answer the question. */
int TTWAIN_GetUIStatus(void);
void TTWAIN_SetUIStatus(int status);

int TTWAIN_IsDeviceOnLine(void); /* -1 unknown, 0 no, 1 yes      */
/* info about the twain driver              */
char *TTWAIN_GetManufacturer(void);  /*                              */
char *TTWAIN_GetProductFamily(void); /* return an internal static var*/
char *TTWAIN_GetProductName(void);   /* don't free ret. value        */
char *TTWAIN_GetVersion(void);       /*                              */
char *TTWAIN_GetTwainVersion(void);  /*                              */

int TTWAIN_Native2Raster(void *handle, void *the_ras, int *lx, int *ly);

int TTWAIN_SetXferMech(TTWAIN_TRANSFER_MECH mech, void *ptr, TUINT32 size,
                       int preferredLx, int preferredLy,
                       TUINT32 numberOfImages);
/*            NATIVE  BUFFERED	       FILE
mech          the transfer mechanism
ptr	       0       memory buffer      ?Not Impl.  filename ?
size	       0       size of the buffer ?Not Impl.?
preferredLx    lx      lx                 ?Not Impl.? lx?
preferredLy    ly      ly                 ?Not Impl.? ly?
numberOfImages 1,2,.... or -1 for all in the ADF
*/
#endif

#ifdef NOTES
here
/* The XScaling cap. should be negotiated before the YScaling, this is because
   some Sources may set the YScaling capability whenever XScaling capability
   is set, to maintain a square aspect ratio for Applications that do not
   bother to negotiate YScaling capability. (note from Twain Spec 1.9 draft)
*/
#endif

#ifdef __cplusplus
}
#endif
