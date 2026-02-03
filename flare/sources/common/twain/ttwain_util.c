

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#ifdef NOTE
here
/*
TTWAIN_Get???Value (TW_RANGE range) restituisce sempre un float e (ovviamente)
funziona solo con gli item il cui sizeof() e' <= sizeof (float) (unica eccezione
TW_FIX32)

USA_LE_QUERY: -> non definito!
Le query dovrebbero essere un modo per capire le azioni che sono permesse
su una determinata capability, questo sta scritto in twainSpec1.8:
nessun DS ha ritornato valori sensati, per cui non vengono usate.
*/
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#include "ttwain_util.h"
#include "ttwain_utilP.h"
#include "ttwain_utilPD.h"
#include "ttwainP.h"
#include "ttwain.h"
#include "ttwain_state.h"
#include "ttwain_capability.h"
#include "ttwain_conversion.h"
#include "ttwain_error.h"

#include "ttwain_global_def.h"

/*
Scommentare MACOSX_NO_PARAMS, per evitare che l'applicazione abbia il controllo
dei parametri (Caps supportate, ImageLayout, XferCount)
Utile sotto MACOSX per evitare che i DS della HP si aprano alla richiesta,
e rimangano li.... in attesa di una scansione (tutto continua a funzionare,
alla prima scansione il DS si chiude correttamente e l'applicazione ottiene
l'immagine)
*/
/*
#define MACOSX_NO_PARAMS
*/

#ifdef __cplusplus
    extern "C" {
#endif

  /*#define USA_LE_QUERY*/
  /* local  */
  static int TTWAIN_GetPhysicalWidth(float *width);
  static int TTWAIN_GetPhysicalHeight(float *height);

  static int TTWAIN_GetMinimumWidth(float *width);
  static int TTWAIN_GetMinimumHeight(float *height);

  static int TTWAIN_IsCapSupported(void *cap);
  static int TTWAIN_IsCapSupportedTW_INT16(TW_INT16 cap);

  /* MISC AUX FUNCTIONS */
  static float TTWAIN_GetMinValue(TW_RANGE range);
  static float TTWAIN_GetMaxValue(TW_RANGE range);
  static float TTWAIN_GetDefValue(TW_RANGE range);
  static float TTWAIN_GetStepValue(TW_RANGE range);
  static int TTWAIN_IsItemInList(void *list, void *item, TUINT32 list_count,
                                 TUINT32 item_size);

  static const size_t DCItemSize[13] = {
      sizeof(TW_INT8),   sizeof(TW_INT16),  sizeof(TW_INT32), sizeof(TW_UINT8),
      sizeof(TW_UINT16), sizeof(TW_UINT32), sizeof(TW_BOOL),  sizeof(TW_FIX32),
      sizeof(TW_FRAME),  sizeof(TW_STR32),  sizeof(TW_STR64), sizeof(TW_STR128),
      sizeof(TW_STR255),
  }; /* see twain.h */

#define FLAVOR_UNUSED (0xffff)

  struct TTWAIN_PIXELTYPEP {
    TW_UINT16 type;
    TW_UINT16 flavor;
    TW_UINT16 bitDepth;
  };
  static struct TTWAIN_PIXELTYPEP PixType[TTWAIN_PIXTYPE_HOWMANY] = {
      {TWPT_BW, TWPF_CHOCOLATE, 1},  /* BW    */
      {TWPT_BW, TWPF_VANILLA, 1},    /* WB    */
      {TWPT_GRAY, FLAVOR_UNUSED, 8}, /* GRAY8 */
      {TWPT_RGB, FLAVOR_UNUSED, 24}  /* RGB24 */
  };
  /*---------------------------------------------------------------------------*/
  /*    GET CAPABILITIES */
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetResolution(float *min, float *max, float *step, float *def) {
    TW_RANGE range_data;
    int rc;

    if (!(min && max && step && def)) return FALSE;
    rc = TTWAIN_GetCap(ICAP_XRESOLUTION, TWON_RANGE, (void *)&range_data, 0);
    if (!rc) return FALSE;
    *min  = TTWAIN_GetMinValue(range_data);
    *max  = TTWAIN_GetMaxValue(range_data);
    *step = TTWAIN_GetStepValue(range_data);
    *def  = TTWAIN_GetDefValue(range_data);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetOpticalResolution(float *min, float *max, float *step,
                                  float *def) {
    TW_RANGE range_data;
    int rc;

    if (!(min && max && step && def)) return FALSE;
    rc = TTWAIN_GetCap(ICAP_XNATIVERESOLUTION, TWON_RANGE, (void *)&range_data,
                       0);
    if (!rc) return FALSE;
    *min  = TTWAIN_GetMinValue(range_data);
    *max  = TTWAIN_GetMaxValue(range_data);
    *step = TTWAIN_GetStepValue(range_data);
    *def  = TTWAIN_GetDefValue(range_data);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetBrightness(float *min, float *max, float *step, float *def) {
    TW_RANGE range_data;
    int rc;

    assert(min && max && step && def);

    rc = TTWAIN_GetCap(ICAP_BRIGHTNESS, TWON_RANGE, (void *)&range_data, 0);
    if (!rc) return FALSE;
    *min  = TTWAIN_GetMinValue(range_data);
    *max  = TTWAIN_GetMaxValue(range_data);
    *step = TTWAIN_GetStepValue(range_data);
    *def  = TTWAIN_GetDefValue(range_data);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetContrast(float *min, float *max, float *step, float *def) {
    TW_RANGE range_data;
    int rc;

    assert(min && max && step && def);

    rc = TTWAIN_GetCap(ICAP_CONTRAST, TWON_RANGE, (void *)&range_data, 0);
    if (!rc) return FALSE;
    *min  = TTWAIN_GetMinValue(range_data);
    *max  = TTWAIN_GetMaxValue(range_data);
    *step = TTWAIN_GetStepValue(range_data);
    *def  = TTWAIN_GetDefValue(range_data);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetThreshold(float *min, float *max, float *step, float *def) {
    TW_RANGE range_data;
    int rc;

    assert(min && max && step && def);

    rc = TTWAIN_GetCap(ICAP_THRESHOLD, TWON_RANGE, (void *)&range_data, 0);
    if (!rc) return FALSE;
    *min  = TTWAIN_GetMinValue(range_data);
    *max  = TTWAIN_GetMaxValue(range_data);
    *step = TTWAIN_GetStepValue(range_data);
    *def  = TTWAIN_GetDefValue(range_data);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_GetPhysicalWidth(float *width) {
    TW_ONEVALUE width_data;
    if (TTWAIN_GetCap(ICAP_PHYSICALWIDTH, TWON_ONEVALUE, (void *)&width_data,
                      0)) {
      *width = TTWAIN_Fix32ToFloat(*(TW_FIX32 *)&width_data.Item);
      return TRUE;
    } else
      return FALSE;
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_GetPhysicalHeight(float *height) {
    TW_ONEVALUE height_data;
    if (TTWAIN_GetCap(ICAP_PHYSICALHEIGHT, TWON_ONEVALUE, (void *)&height_data,
                      0)) {
      *height = TTWAIN_Fix32ToFloat(*(TW_FIX32 *)&height_data.Item);
      return TRUE;
    } else
      return FALSE;
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_GetMinimumWidth(float *width) {
    TW_ONEVALUE width_data;
    if (TTWAIN_GetCap(ICAP_MINIMUMWIDTH, TWON_ONEVALUE, (void *)&width_data,
                      0)) {
      *width = TTWAIN_Fix32ToFloat(*(TW_FIX32 *)&width_data.Item);
      return TRUE;
    } else
      return FALSE;
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_GetMinimumHeight(float *height) {
    TW_ONEVALUE height_data;
    if (TTWAIN_GetCap(ICAP_MINIMUMHEIGHT, TWON_ONEVALUE, (void *)&height_data,
                      0)) {
      *height = TTWAIN_Fix32ToFloat(*(TW_FIX32 *)&height_data.Item);
      return TRUE;
    } else
      return FALSE;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetPhysicalWidthWAdf(float *width) {
    int rc = FALSE;
    int feeder_status;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
      if (rc) rc = TTWAIN_GetPhysicalWidth(width);
      TTWAIN_SetFeeder(feeder_status);
    }
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetPhysicalHeightWAdf(float *height) {
    int rc = FALSE;
    int feeder_status;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
      if (rc) rc = TTWAIN_GetPhysicalHeight(height);
      TTWAIN_SetFeeder(feeder_status);
    }
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetMinimumWidthWAdf(float *width) {
    int rc = FALSE;
    int feeder_status;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
      if (rc) rc = TTWAIN_GetMinimumWidth(width);
      TTWAIN_SetFeeder(feeder_status);
    }
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetMinimumHeightWAdf(float *height) {
    int rc = FALSE;
    int feeder_status;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
      if (rc) rc = TTWAIN_GetMinimumHeight(height);
      TTWAIN_SetFeeder(feeder_status);
    }
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetPhysicalWidthWoAdf(float *width) {
    int rc            = FALSE;
    int feeder_status = FALSE;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(FALSE);
    }
    rc = TTWAIN_GetPhysicalWidth(width);
    if (TTWAIN_SupportsFeeder()) TTWAIN_SetFeeder(feeder_status);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetPhysicalHeightWoAdf(float *height) {
    int rc            = FALSE;
    int feeder_status = FALSE;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
    }
    rc = TTWAIN_GetPhysicalHeight(height);
    if (TTWAIN_SupportsFeeder()) TTWAIN_SetFeeder(feeder_status);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetMinimumWidthWoAdf(float *width) {
    int rc            = FALSE;
    int feeder_status = FALSE;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(FALSE);
    }
    rc = TTWAIN_GetMinimumWidth(width);
    if (TTWAIN_SupportsFeeder()) TTWAIN_SetFeeder(feeder_status);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetMinimumHeightWoAdf(float *height) {
    int rc            = FALSE;
    int feeder_status = FALSE;

    if (TTWAIN_SupportsFeeder()) {
      feeder_status = TTWAIN_GetFeeder();
      rc            = TTWAIN_SetFeeder(TRUE);
    }
    rc = TTWAIN_GetMinimumHeight(height);
    if (TTWAIN_SupportsFeeder()) TTWAIN_SetFeeder(feeder_status);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SupportsPixelType(TTWAIN_PIXTYPE pix_type) {
    TW_HANDLE handle;
    TW_ENUMERATION *container;
    int rc, found = FALSE;
    TUINT32 size4data;
    TW_UINT16 twPix;

    twPix = PixType[pix_type].type;
    rc = TTWAIN_GetCap(ICAP_PIXELTYPE, TWON_ENUMERATION, (void *)0, &size4data);
    if (!rc) return FALSE;
    if (!size4data) return FALSE;
    handle = GLOBAL_ALLOC(GMEM_FIXED, size4data);
    if (!handle) return FALSE;

    container = (TW_ENUMERATION *)GLOBAL_LOCK(handle);
    rc = TTWAIN_GetCap(ICAP_PIXELTYPE, TWON_ENUMERATION, (void *)container, 0);

    if (!rc || !container) goto done;
    found =
        TTWAIN_IsItemInList(container->ItemList, &twPix, container->NumItems,
                            DCItemSize[container->ItemType]);

  done:
    GLOBAL_UNLOCK(handle);
    GLOBAL_FREE(handle);
    return found;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SupportsCompressionType(TW_UINT16 comprType) {
    int rc;
    TUINT32 size;
    int found                 = FALSE;
    TW_ENUMERATION *container = 0;
    TW_HANDLE handle          = 0;

    if (!TTWAIN_IsCapCompressionSupported()) return FALSE;
    rc = TTWAIN_GetCap(ICAP_COMPRESSION, TWON_ENUMERATION, (void *)0, &size);
    if (!rc || !size) return FALSE;

    handle = GLOBAL_ALLOC(GMEM_FIXED, size);
    if (!handle) return FALSE;

    container = (TW_ENUMERATION *)GLOBAL_LOCK(handle);

    rc =
        TTWAIN_GetCap(ICAP_COMPRESSION, TWON_ENUMERATION, (void *)container, 0);

    if (!rc || !container) goto done;
    found = TTWAIN_IsItemInList(container->ItemList, &comprType,
                                container->NumItems,
                                DCItemSize[container->ItemType]);
    found = TRUE;
  done:
    if (handle) {
      GLOBAL_UNLOCK(handle);
      GLOBAL_FREE(handle);
    }
    return found;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SupportsFeeder(void) {
    TW_ONEVALUE feeder;
    int rc;
    feeder.Item = 0;
    rc = TTWAIN_GetCap(CAP_FEEDERENABLED, TWON_ONEVALUE, (void *)&feeder, 0);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetFeeder(void) /* TRUE->enabled */
  {
    TW_ONEVALUE feeder;
    int rc;
    feeder.Item = 0;
    rc = TTWAIN_GetCap(CAP_FEEDERENABLED, TWON_ONEVALUE, (void *)&feeder, 0);
    return feeder.Item && rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsFeederLoaded(void) /* TRUE->loaded */
  {
    TW_ONEVALUE feeder;
    int rc;

#ifdef MACOSX_NO_PARAMS
    printf("%s always returns false (disabled at compile time)\n",
           __FUNCTION__);
    return FALSE;
#endif

    feeder.Item = 0;
    rc = TTWAIN_GetCap(CAP_FEEDERLOADED, TWON_ONEVALUE, (void *)&feeder, 0);
    return feeder.Item && rc;
  }
  /*---------------------------------------------------------------------------*/
  char *TTWAIN_GetManufacturer(void) {
#ifdef MACOSX
    static char msg[1024];
    strncpy(msg, (const char *)&TTwainData.sourceId.Manufacturer[1],
            *TTwainData.sourceId.Manufacturer);
    return msg;
#else
  return (char *)TTwainData.sourceId.Manufacturer;
#endif
  }
  /*---------------------------------------------------------------------------*/
  char *TTWAIN_GetProductFamily(void) {
#ifdef MACOSX
    static char msg[1024];
    strncpy(msg, (const char *)&TTwainData.sourceId.ProductFamily[1],
            *TTwainData.sourceId.ProductFamily);
    return msg;
#else
  return (char *)TTwainData.sourceId.ProductFamily;
#endif
  }
  /*---------------------------------------------------------------------------*/
  char *TTWAIN_GetProductName(void) {
#ifdef MACOSX
    static char msg[1024];
    strncpy(msg, (const char *)&TTwainData.sourceId.ProductName[1],
            *TTwainData.sourceId.ProductName);
    return msg;
#else
  return (char *)TTwainData.sourceId.ProductName;
#endif
  }
  /*---------------------------------------------------------------------------*/
  char *TTWAIN_GetVersion(void) {
    static char version[5 + 1 + 5 + 1 + 32 + 1];
    snprintf(version, sizeof(version), "%d.%d %s",
             TTwainData.sourceId.Version.MajorNum,
             TTwainData.sourceId.Version.MinorNum,
             (char *)TTwainData.sourceId.Version.Info);
    return version;
  }
  /*---------------------------------------------------------------------------*/
  char *TTWAIN_GetTwainVersion(void) {
    static char version[5 + 1 + 5 + 1];
    snprintf(version, sizeof(version), "%d.%d",
             TTwainData.sourceId.ProtocolMajor,
             TTwainData.sourceId.ProtocolMinor);
    return version;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetSupportedCaps(void) {
    TW_ARRAY *container;
    TW_HANDLE handle = 0;
    int rc;
    TUINT32 size4data;

    TTwainData.isSupportedCapsSupported = FALSE;

#ifdef MACOSX_NO_PARAMS
    printf("%s always returns false (disabled at compile time)\n",
           __FUNCTION__);
    return FALSE;
/* PER ora su mac sono disabilitate! */
/*return FALSE;*/
#endif

    rc = TTWAIN_GetCap(CAP_SUPPORTEDCAPS, TWON_ARRAY, (void *)0, &size4data);
    if (!rc) return FALSE;
    if (!size4data) return FALSE;
    handle = GLOBAL_ALLOC(GMEM_FIXED, size4data);
    if (!handle) return FALSE;
    container = (TW_ARRAY *)GLOBAL_LOCK(handle);
    rc = TTWAIN_GetCap(CAP_SUPPORTEDCAPS, TWON_ARRAY, (void *)container, 0);
    if (rc) {
      /*
MEMORY LEAK !!! ma per ora va bene cosi'...
if (TTwainData.supportedCaps)
GLOBAL_FREE(TTwainData.supportedCaps);
*/
      TTwainData.supportedCaps = container;
    }
    TTwainData.isSupportedCapsSupported = TRUE;
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetCurrentPixelType(TTWAIN_PIXTYPE * pixType) {
    int rc1, rc2;
    TW_ONEVALUE pixelType, pixelFlavor;
    int i;
    rc1 = TTWAIN_GetCapCurrent(ICAP_PIXELTYPE, TWON_ONEVALUE, &pixelType, 0);
    if (!rc1) return FALSE;
    *pixType = TTWAIN_PIXUNKNOWN;
    if (pixelType.Item == TWPT_BW) {
      rc2 = TTWAIN_GetCapCurrent(ICAP_PIXELFLAVOR, TWON_ONEVALUE, &pixelFlavor,
                                 0);
      if (!rc2) pixelFlavor.Item = TWPF_CHOCOLATE; /*this is the default */
    } else {
      pixelFlavor.Item = FLAVOR_UNUSED;
    }

    for (i = 0; i < TTWAIN_PIXTYPE_HOWMANY; i++) {
      if ((PixType[i].type == pixelType.Item) &&
          (PixType[i].flavor == pixelFlavor.Item)) {
        *pixType = (TTWAIN_PIXTYPE)i;
        return TRUE;
      }
    }
    return FALSE;
  }

  /*---------------------------------------------------------------------------*/
  /*    SUPPORTED CAPABILITIES */
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_IsCapSupported(void *cap) {
#ifdef USA_LE_QUERY
    int queryRc;
    TW_UINT16 pattern;
    int et;
    queryRc = TTWAIN_GetCapQuery(*((TW_INT16 *)cap), &pattern);

    et = pattern & (TWQC_SET | TWQC_GET);
    return (TTwainData.isSupportedCapsSupported && queryRc && et) ||
           (TTwainData.supportedCaps &&
            TTWAIN_IsItemInList(
                TTwainData.supportedCaps, cap,
                TTwainData.supportedCaps->NumItems,
                DCItemSize[TTwainData.supportedCaps->ItemType])) ||
           (queryRc && et);
#else
  return TTwainData.isSupportedCapsSupported &&
         (TTwainData.supportedCaps &&
          TTWAIN_IsItemInList(TTwainData.supportedCaps, cap,
                              TTwainData.supportedCaps->NumItems,
                              DCItemSize[TTwainData.supportedCaps->ItemType]));
#endif
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_IsCapSupportedTW_INT16(TW_INT16 cap) {
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapResolutionSupported(void) {
    TW_UINT16 cap = ICAP_XRESOLUTION;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapOpticalResolutionSupported(void) {
    TW_UINT16 cap = ICAP_XNATIVERESOLUTION;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapBrightnessSupported(void) {
    TW_UINT16 cap = ICAP_BRIGHTNESS;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapContrastSupported(void) {
    TW_UINT16 cap = ICAP_CONTRAST;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapThresholdSupported(void) {
    TW_UINT16 cap = ICAP_THRESHOLD;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapPhysicalWidthSupported(void) {
    TW_UINT16 cap = ICAP_PHYSICALWIDTH;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapPhysicalHeightSupported(void) {
    TW_UINT16 cap = ICAP_PHYSICALHEIGHT;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapMinimumWidthSupported(void) {
    TW_UINT16 cap = ICAP_MINIMUMWIDTH;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapMinimumHeightSupported(void) {
    TW_UINT16 cap = ICAP_MINIMUMHEIGHT;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapPixelTypeSupported(void) {
    TW_UINT16 cap = ICAP_PIXELTYPE;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapFeederSupported(void) {
    TW_UINT16 cap = CAP_FEEDERENABLED;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapImageLayoutSupported(void) {
    /*assert(0);*/
    return TRUE;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapOrientationSupported(void) {
    TW_UINT16 cap = ICAP_ORIENTATION;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapDeviceOnLineSupported(void) {
    TW_UINT16 cap = CAP_DEVICEONLINE;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapBitDepthSupported(void) {
    TW_UINT16 cap = ICAP_BITDEPTH;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapBitOrderSupported(void) {
    TW_UINT16 cap = ICAP_BITORDER;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsCapCompressionSupported(void) {
    TW_UINT16 cap = ICAP_COMPRESSION;
    return TTWAIN_IsCapSupported(&cap);
  }
  /*---------------------------------------------------------------------------*/
  /*    SET CAPABILITIES */
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetContrast(float contrast) {
    TW_FIX32 twfix = TTWAIN_FloatToFix32(contrast);
    return TTWAIN_SetCap(ICAP_CONTRAST, TWON_ONEVALUE, TWTY_FIX32,
                         (TW_UINT32 *)&twfix);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetBrightness(float brightness) {
    TW_FIX32 twfix = TTWAIN_FloatToFix32(brightness);
    return TTWAIN_SetCap(ICAP_BRIGHTNESS, TWON_ONEVALUE, TWTY_FIX32,
                         (TW_UINT32 *)&twfix);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetThreshold(float threshold) {
    TW_FIX32 twfix = TTWAIN_FloatToFix32(threshold);
    return TTWAIN_SetCap(ICAP_THRESHOLD, TWON_ONEVALUE, TWTY_FIX32,
                         (TW_UINT32 *)&twfix);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetResolution(float resolution) {
    int rc1, rc2;
    TW_FIX32 twfix = TTWAIN_FloatToFix32(resolution);
    rc1            = TTWAIN_SetCap(ICAP_XRESOLUTION, TWON_ONEVALUE, TWTY_FIX32,
                                   (TW_UINT32 *)&twfix);
    rc2            = TTWAIN_SetCap(ICAP_YRESOLUTION, TWON_ONEVALUE, TWTY_FIX32,
                                   (TW_UINT32 *)&twfix);
    return (rc1 & rc2);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetXScaling(float scale) {
    int rc;
    TW_FIX32 twfix = TTWAIN_FloatToFix32(scale);
    rc             = TTWAIN_SetCap(ICAP_XSCALING, TWON_ONEVALUE, TWTY_FIX32,
                                   (TW_UINT32 *)&twfix);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetYScaling(float scale) {
    int rc;
    TW_FIX32 twfix = TTWAIN_FloatToFix32(scale);
    rc             = TTWAIN_SetCap(ICAP_YSCALING, TWON_ONEVALUE, TWTY_FIX32,
                                   (TW_UINT32 *)&twfix);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetPixelType(TTWAIN_PIXTYPE pixtype) {
    TUINT32 twPix;
    int rc, rc2, rc3, rc4, rc5, bdRc;
    TW_ENUMERATION *container = 0;
    TW_HANDLE handle          = 0;

    TW_UINT16 twFlavor, twBitDepth;
    int found;
    TUINT32 size;

    twPix      = PixType[pixtype].type;
    twFlavor   = PixType[pixtype].flavor;
    twBitDepth = PixType[pixtype].bitDepth;
    /*the default in twain specs is chocolate*/
    TTwainData.transferInfo.nextImageNeedsToBeInverted =
        ((twFlavor != TWPF_CHOCOLATE) && (twFlavor != FLAVOR_UNUSED));

    rc = TTWAIN_SetCap(ICAP_PIXELTYPE, TWON_ONEVALUE, TWTY_UINT16,
                       (TW_UINT32 *)&twPix);
    if (TTWAIN_IsCapBitDepthSupported()) bdRc = TTWAIN_SetBitDepth(twBitDepth);

    if (rc) {
      if (twFlavor != FLAVOR_UNUSED) {
        rc2 =
            TTWAIN_GetCap(ICAP_PIXELFLAVOR, TWON_ENUMERATION, (void *)0, &size);
        if (rc2 && size) {
          handle = GLOBAL_ALLOC(GMEM_FIXED, size);
          if (!handle)
            return TRUE; /*non sono semplicamente riuscito a prendere info
riguardo
il pixelFlavor, ma setPixelType e' andato a buon fine */
#ifdef _WIN32
          container = (TW_ENUMERATION *)handle;
#else
        container = (TW_ENUMERATION *)GLOBAL_LOCK(handle);
#endif
          rc3 = TTWAIN_GetCap(ICAP_PIXELFLAVOR, TWON_ENUMERATION,
                              (void *)container, 0);
          if (rc3) {
            found = TTWAIN_IsItemInList(container->ItemList, &twFlavor,
                                        container->NumItems,
                                        DCItemSize[container->ItemType]);
            /*let's try to set....*/
            if (found) {
              rc4 = TTWAIN_SetCap(ICAP_PIXELFLAVOR, TWON_ONEVALUE, TWTY_UINT16,
                                  (TW_UINT32 *)&twFlavor);
              if (rc4) {
                TW_UINT16 current, *itemList;
                /*check if it's properly set...*/
                rc5 = TTWAIN_GetCap(ICAP_PIXELFLAVOR, TWON_ENUMERATION,
                                    (void *)container, 0);
                if (rc5) {
                  itemList = (TW_UINT16 *)container->ItemList;
                  current  = itemList[container->CurrentIndex];
                  if (current == twFlavor) {
                    TTwainData.transferInfo.nextImageNeedsToBeInverted = FALSE;
                  }
                }
              }
            }
          }
        }
      }
    }
    if (handle) {
      GLOBAL_UNLOCK(handle);
      GLOBAL_FREE(handle);
    }
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetBitDepth(USHORT bitDepth) {
    TUINT32 bd = bitDepth;
    int rc;

    rc = TTWAIN_SetCap(ICAP_BITDEPTH, TWON_ONEVALUE, TWTY_UINT16,
                       (TW_UINT32 *)&bd);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetBitOrder(TTWAIN_BITORDER bitOrder) {
    TUINT32 bo = bitOrder;
    int rc;

    rc = TTWAIN_SetCap(ICAP_BITORDER, TWON_ONEVALUE, TWTY_UINT16,
                       (TW_UINT32 *)&bo);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetImageLayout(float L, float T, float R, float B) {
    TW_IMAGELAYOUT layout;
    TW_IMAGELAYOUT layout2;
    int rc;
    int rc2;

#ifdef MACOSX_NO_PARAMS
    printf("%s always returns true (disabled at compile time)\n", __FUNCTION__);
    return TRUE;
#endif
    /*
if (TTWAIN_GetState() != TWAIN_SOURCE_OPEN)
{
TTWAIN_RecordError();
return FALSE;
}
*/
    layout.Frame.Left   = TTWAIN_FloatToFix32(L);
    layout.Frame.Top    = TTWAIN_FloatToFix32(T);
    layout.Frame.Right  = TTWAIN_FloatToFix32(R);
    layout.Frame.Bottom = TTWAIN_FloatToFix32(B);

    layout.DocumentNumber = TWON_DONTCARE32;
    layout.PageNumber     = TWON_DONTCARE32;
    layout.FrameNumber    = TWON_DONTCARE32;
    rc  = (TTWAIN_DS(DG_IMAGE, DAT_IMAGELAYOUT, MSG_SET, &layout) ==
          TWRC_SUCCESS);
    rc2 = (TTWAIN_DS(DG_IMAGE, DAT_IMAGELAYOUT, MSG_GET, &layout2) ==
           TWRC_SUCCESS);
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_NegotiateXferCount(TUINT32 nXfers) {
#ifdef MACOSX_NO_PARAMS
    printf("%s always returns true (disabled at compile time)\n", __FUNCTION__);
    return TRUE;
#endif
    return TTWAIN_SetCap(CAP_XFERCOUNT, TWON_ONEVALUE, TWTY_INT16,
                         (TW_UINT32 *)&nXfers);
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetXferMech(TTWAIN_TRANSFER_MECH mech, void *ptr, TUINT32 size,
                         int preferredLx, int preferredLy,
                         TUINT32 numberOfImages) {
    TW_UINT32 theMech = mech;

    TTwainData.transferInfo.preferredLx = preferredLx;
    TTwainData.transferInfo.preferredLy = preferredLy;
    if (mech == TTWAIN_TRANSFERMODE_NATIVE) {
      /*assert(!ptr);*/
      /*invalidate mem  & len*/
      TTwainData.transferInfo.memorySize   = -1;
      TTwainData.transferInfo.memoryBuffer = 0;
    } else {
      /*assert(ptr);*/
      /*store*/
      assert(!ptr); /* questo va rivisto, comunque NON si puo' fornire il
                       buffer! */
      TTwainData.transferInfo.memorySize   = size;
      TTwainData.transferInfo.memoryBuffer = (UCHAR *)ptr;
    }

    if (TTWAIN_SetCap(ICAP_XFERMECH, TWON_ONEVALUE, TWTY_UINT16,
                      (TW_UINT32 *)&theMech)) {
      if (numberOfImages != -1) TTWAIN_NegotiateXferCount(numberOfImages);
      TTwainData.transferInfo.transferMech = mech;
      return TRUE;
    } else
      return FALSE;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_SetFeeder(int status) /* TRUE->enabled */
  {
    int rc;
    rc = TTWAIN_SetCap(CAP_FEEDERENABLED, TWON_ONEVALUE, TWTY_BOOL,
                       (TW_UINT32 *)&status);
    /*
if (TTwainData.transferInfo.usageMode == TTWAIN_MODE_UNLEASHED)
TTwainData.transferInfo.multiTransfer = status;
*/
    return rc;
  }
  /*---------------------------------------------------------------------------*/
  /*    USER INTERFACE */
  /*---------------------------------------------------------------------------*/
  int TTWAIN_HasControllableUI(void) {
    TW_ONEVALUE onevalue_data;
    int rc;
    rc = TTWAIN_GetCap(CAP_UICONTROLLABLE, TWON_ONEVALUE,
                       (void *)&onevalue_data, 0);
    if (!rc) return FALSE;
    return (int)onevalue_data.Item;
  }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetUIStatus(void) { return TTwainData.UIStatus; }
  /*---------------------------------------------------------------------------*/
  void TTWAIN_SetUIStatus(int status) { TTwainData.UIStatus = status; }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_GetModalStatus(void) { return TTwainData.modalStatus; }
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsDeviceOnLine(void) /* -1 unknown, 0 no, 1 yes */
  {
    TW_ONEVALUE onevalue_data;
    int rc;
    rc = TTWAIN_GetCap(CAP_DEVICEONLINE, TWON_ONEVALUE, (void *)&onevalue_data,
                       0);
    if (!rc) return -1;
    return (int)onevalue_data.Item;
  }
  /*---------------------------------------------------------------------------*/
  /*    TWAIN AVAILABILITY */
  /*---------------------------------------------------------------------------*/
  int TTWAIN_IsAvailable(void) {
    TTWAIN_InitVar();
    if (TTWAIN_DSM_HasEntryPoint()) return TRUE;

    if (TTwainData.twainAvailable == AVAIABLE_DONTKNOW) {
      if (TTWAIN_LoadSourceManager()) {
        TTWAIN_UnloadSourceManager();
      } else {
        TTwainData.twainAvailable = AVAIABLE_NO;
      }
    }

    return (TTwainData.twainAvailable == AVAIABLE_YES);
  }
  /*---------------------------------------------------------------------------*/
  void TTWAIN_SetAvailable(TWAINAVAILABLE status) {
    TTwainData.twainAvailable = status;
  }
  /*---------------------------------------------------------------------------*/

  /*---------------------------------------------------------------------------*/
  /*    MISC AUX FUNCTIONS */
  /*---------------------------------------------------------------------------*/

  static float TTWAIN_GetMinValue(TW_RANGE range) {
    switch (range.ItemType) {
    case TWTY_INT8:
    case TWTY_INT16:
    case TWTY_INT32:
    case TWTY_UINT8:
    case TWTY_UINT16:
      return (float)range.MinValue;

    case TWTY_FIX32: {
      TW_FIX32 *fix32 = (TW_FIX32 *)&range.MinValue;
      return (float)TTWAIN_Fix32ToFloat(*fix32);
    }

    default:
      // TWTY_UINT32
      // TWTY_BOOL
      // TWTY_FRAME
      // TWTY_STR32
      // TWTY_STR64
      // TWTY_STR128
      // TWTY_STR255
      assert(!"Invalid type!!!");
      return 0;
    }
    return 0;
  }
  /*---------------------------------------------------------------------------*/
  static float TTWAIN_GetMaxValue(TW_RANGE range) {
    switch (range.ItemType) {
    case TWTY_INT8:
    case TWTY_INT16:
    case TWTY_INT32:
    case TWTY_UINT8:
    case TWTY_UINT16:
      return (float)range.MaxValue;

    case TWTY_FIX32: {
      TW_FIX32 *fix32 = (TW_FIX32 *)&range.MaxValue;
      return (float)TTWAIN_Fix32ToFloat(*fix32);
    }

    default:
      // TWTY_UINT32
      // TWTY_BOOL
      // TWTY_FRAME
      // TWTY_STR32
      // TWTY_STR64
      // TWTY_STR128
      // TWTY_STR255
      assert(!"Invalid type!!");
      return 0;
    }
    return 0;
  }
  /*---------------------------------------------------------------------------*/
  static float TTWAIN_GetDefValue(TW_RANGE range) {
    switch (range.ItemType) {
    case TWTY_INT8:
    case TWTY_INT16:
    case TWTY_INT32:
    case TWTY_UINT8:
    case TWTY_UINT16:
      return (float)range.DefaultValue;

    case TWTY_FIX32: {
      TW_FIX32 *fix32 = (TW_FIX32 *)&range.DefaultValue;
      return (float)TTWAIN_Fix32ToFloat(*fix32);
    }

    default:
      // TWTY_UINT32
      // TWTY_BOOL
      // TWTY_FRAME
      // TWTY_STR32
      // TWTY_STR64
      // TWTY_STR128
      // TWTY_STR255
      assert(!"Invalid type!!");
      return 0;
    }
    return 0;
  }
  /*---------------------------------------------------------------------------*/
  static float TTWAIN_GetStepValue(TW_RANGE range) {
    switch (range.ItemType) {
    case TWTY_INT8:
    case TWTY_INT16:
    case TWTY_INT32:
    case TWTY_UINT8:
    case TWTY_UINT16:
      return (float)range.StepSize;

    case TWTY_FIX32: {
      TW_FIX32 *fix32 = (TW_FIX32 *)&range.StepSize;
      return (float)TTWAIN_Fix32ToFloat(*fix32);
    }

    default:
      // TWTY_UINT32
      // TWTY_BOOL
      // TWTY_FRAME
      // TWTY_STR32
      // TWTY_STR64
      // TWTY_STR128
      // TWTY_STR255
      assert(!"Invalid type!!");
      return 0;
    }
    return 0;
  }
  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/
  int TTWAIN_Native2Raster(void *handle, void *the_ras, int *lx, int *ly) {
    return TTWAIN_Native2RasterPD(handle, the_ras, lx, ly);
  }
  /*---------------------------------------------------------------------------*/
  static int TTWAIN_IsItemInList(void *list, void *item, TUINT32 list_count,
                                 TUINT32 item_size) {
    int found     = FALSE;
    TUINT32 count = list_count;
    UCHAR *pList  = (UCHAR *)list;
    UCHAR *pItem  = (UCHAR *)item;

    if (!list || !item) return FALSE;
    while (count--) {
      if (memcmp(pList, pItem, item_size) == 0) {
        found = TRUE;
        break;
      }
      pList += item_size;
    }
    return found;
  }
  /*---------------------------------------------------------------------------*/
  void TTWAIN_DumpCapabilities(void (*trace_fun)(const char *fmt, ...)) {
    if (!trace_fun) return;
    trace_fun("list of supported (exported) capabilities\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_XFERCOUNT))
      trace_fun("CAP_XFERCOUNT");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_COMPRESSION))
      trace_fun("ICAP_COMPRESSION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PIXELTYPE))
      trace_fun("ICAP_PIXELTYPE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_UNITS)) trace_fun("ICAP_UNITS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_XFERMECH))
      trace_fun("ICAP_XFERMECH\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_AUTHOR)) trace_fun("CAP_AUTHOR\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_CAPTION)) trace_fun("CAP_CAPTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_FEEDERENABLED))
      trace_fun("CAP_FEEDERENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_FEEDERLOADED))
      trace_fun("CAP_FEEDERLOADED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_TIMEDATE))
      trace_fun("CAP_TIMEDATE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_SUPPORTEDCAPS))
      trace_fun("CAP_SUPPORTEDCAPS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_EXTENDEDCAPS))
      trace_fun("CAP_EXTENDEDCAPS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_AUTOFEED))
      trace_fun("CAP_AUTOFEED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_CLEARPAGE))
      trace_fun("CAP_CLEARPAGE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_FEEDPAGE))
      trace_fun("CAP_FEEDPAGE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_REWINDPAGE))
      trace_fun("CAP_REWINDPAGE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_INDICATORS))
      trace_fun("CAP_INDICATORS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_SUPPORTEDCAPSEXT))
      trace_fun("CAP_SUPPORTEDCAPSEXT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PAPERDETECTABLE))
      trace_fun("CAP_PAPERDETECTABLE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_UICONTROLLABLE))
      trace_fun("CAP_UICONTROLLABLE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_DEVICEONLINE))
      trace_fun("CAP_DEVICEONLINE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_AUTOSCAN))
      trace_fun("CAP_AUTOSCAN\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_THUMBNAILSENABLED))
      trace_fun("CAP_THUMBNAILSENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_DUPLEX)) trace_fun("CAP_DUPLEX\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_DUPLEXENABLED))
      trace_fun("CAP_DUPLEXENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_ENABLEDSUIONLY))
      trace_fun("CAP_ENABLEDSUIONLY\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_CUSTOMDSDATA))
      trace_fun("CAP_CUSTOMDSDATA\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_ENDORSER))
      trace_fun("CAP_ENDORSER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_JOBCONTROL))
      trace_fun("CAP_JOBCONTROL\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_ALARMS)) trace_fun("CAP_ALARMS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_ALARMVOLUME))
      trace_fun("CAP_ALARMVOLUME\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_AUTOMATICCAPTURE))
      trace_fun("CAP_AUTOMATICCAPTURE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_TIMEBEFOREFIRSTCAPTURE))
      trace_fun("CAP_TIMEBEFOREFIRSTCAPTURE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_TIMEBETWEENCAPTURES))
      trace_fun("CAP_TIMEBETWEENCAPTURES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_CLEARBUFFERS))
      trace_fun("CAP_CLEARBUFFERS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_MAXBATCHBUFFERS))
      trace_fun("CAP_MAXBATCHBUFFERS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_DEVICETIMEDATE))
      trace_fun("CAP_DEVICETIMEDATE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_POWERSUPPLY))
      trace_fun("CAP_POWERSUPPLY\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_CAMERAPREVIEWUI))
      trace_fun("CAP_CAMERAPREVIEWUI\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_DEVICEEVENT))
      trace_fun("CAP_DEVICEEVENT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_SERIALNUMBER))
      trace_fun("CAP_SERIALNUMBER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTER)) trace_fun("CAP_PRINTER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTERENABLED))
      trace_fun("CAP_PRINTERENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTERINDEX))
      trace_fun("CAP_PRINTERINDEX\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTERMODE))
      trace_fun("CAP_PRINTERMODE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTERSTRING))
      trace_fun("CAP_PRINTERSTRING\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_PRINTERSUFFIX))
      trace_fun("CAP_PRINTERSUFFIX\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_LANGUAGE))
      trace_fun("CAP_LANGUAGE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_FEEDERALIGNMENT))
      trace_fun("CAP_FEEDERALIGNMENT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_FEEDERORDER))
      trace_fun("CAP_FEEDERORDER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_REACQUIREALLOWED))
      trace_fun("CAP_REACQUIREALLOWED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_BATTERYMINUTES))
      trace_fun("CAP_BATTERYMINUTES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(CAP_BATTERYPERCENTAGE))
      trace_fun("CAP_BATTERYPERCENTAGE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_AUTOBRIGHT))
      trace_fun("ICAP_AUTOBRIGHT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BRIGHTNESS))
      trace_fun("ICAP_BRIGHTNESS\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_CONTRAST))
      trace_fun("ICAP_CONTRAST\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_CUSTHALFTONE))
      trace_fun("ICAP_CUSTHALFTONE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_EXPOSURETIME))
      trace_fun("ICAP_EXPOSURETIME\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_FILTER)) trace_fun("ICAP_FILTER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_FLASHUSED))
      trace_fun("ICAP_FLASHUSED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_GAMMA)) trace_fun("ICAP_GAMMA\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_HALFTONES))
      trace_fun("ICAP_HALFTONES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_HIGHLIGHT))
      trace_fun("ICAP_HIGHLIGHT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_IMAGEFILEFORMAT))
      trace_fun("ICAP_IMAGEFILEFORMAT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_LAMPSTATE))
      trace_fun("ICAP_LAMPSTATE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_LIGHTSOURCE))
      trace_fun("ICAP_LIGHTSOURCE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_ORIENTATION))
      trace_fun("ICAP_ORIENTATION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PHYSICALWIDTH))
      trace_fun("ICAP_PHYSICALWIDTH\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PHYSICALHEIGHT))
      trace_fun("ICAP_PHYSICALHEIGHT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_SHADOW)) trace_fun("ICAP_SHADOW\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_FRAMES)) trace_fun("ICAP_FRAMES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_XNATIVERESOLUTION))
      trace_fun("ICAP_XNATIVERESOLUTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_YNATIVERESOLUTION))
      trace_fun("ICAP_YNATIVERESOLUTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_XRESOLUTION))
      trace_fun("ICAP_XRESOLUTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_YRESOLUTION))
      trace_fun("ICAP_YRESOLUTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_MAXFRAMES))
      trace_fun("ICAP_MAXFRAMES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_TILES)) trace_fun("ICAP_TILES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BITORDER))
      trace_fun("ICAP_BITORDER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_CCITTKFACTOR))
      trace_fun("ICAP_CCITTKFACTOR\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_LIGHTPATH))
      trace_fun("ICAP_LIGHTPATH\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PIXELFLAVOR))
      trace_fun("ICAP_PIXELFLAVOR\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PLANARCHUNKY))
      trace_fun("ICAP_PLANARCHUNKY\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_ROTATION))
      trace_fun("ICAP_ROTATION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_SUPPORTEDSIZES))
      trace_fun("ICAP_SUPPORTEDSIZES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_THRESHOLD))
      trace_fun("ICAP_THRESHOLD\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_XSCALING))
      trace_fun("ICAP_XSCALING\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_YSCALING))
      trace_fun("ICAP_YSCALING\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BITORDERCODES))
      trace_fun("ICAP_BITORDERCODES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PIXELFLAVORCODES))
      trace_fun("ICAP_PIXELFLAVORCODES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_JPEGPIXELTYPE))
      trace_fun("ICAP_JPEGPIXELTYPE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_TIMEFILL))
      trace_fun("ICAP_TIMEFILL\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BITDEPTH))
      trace_fun("ICAP_BITDEPTH\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BITDEPTHREDUCTION))
      trace_fun("ICAP_BITDEPTHREDUCTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_UNDEFINEDIMAGESIZE))
      trace_fun("ICAP_UNDEFINEDIMAGESIZE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_IMAGEDATASET))
      trace_fun("ICAP_IMAGEDATASET\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_EXTIMAGEINFO))
      trace_fun("ICAP_EXTIMAGEINFO\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_MINIMUMHEIGHT))
      trace_fun("ICAP_MINIMUMHEIGHT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_MINIMUMWIDTH))
      trace_fun("ICAP_MINIMUMWIDTH\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_FLIPROTATION))
      trace_fun("ICAP_FLIPROTATION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODEDETECTIONENABLED))
      trace_fun("ICAP_BARCODEDETECTIONENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_SUPPORTEDBARCODETYPES))
      trace_fun("ICAP_SUPPORTEDBARCODETYPES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODEMAXSEARCHPRIORITIES))
      trace_fun("ICAP_BARCODEMAXSEARCHPRIORITIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODESEARCHPRIORITIES))
      trace_fun("ICAP_BARCODESEARCHPRIORITIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODESEARCHMODE))
      trace_fun("ICAP_BARCODESEARCHMODE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODEMAXRETRIES))
      trace_fun("ICAP_BARCODEMAXRETRIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_BARCODETIMEOUT))
      trace_fun("ICAP_BARCODETIMEOUT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_ZOOMFACTOR))
      trace_fun("ICAP_ZOOMFACTOR\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODEDETECTIONENABLED))
      trace_fun("ICAP_PATCHCODEDETECTIONENABLED\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_SUPPORTEDPATCHCODETYPES))
      trace_fun("ICAP_SUPPORTEDPATCHCODETYPES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODEMAXSEARCHPRIORITIES))
      trace_fun("ICAP_PATCHCODEMAXSEARCHPRIORITIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODESEARCHPRIORITIES))
      trace_fun("ICAP_PATCHCODESEARCHPRIORITIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODESEARCHMODE))
      trace_fun("ICAP_PATCHCODESEARCHMODE\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODEMAXRETRIES))
      trace_fun("ICAP_PATCHCODEMAXRETRIES\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_PATCHCODETIMEOUT))
      trace_fun("ICAP_PATCHCODETIMEOUT\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_FLASHUSED2))
      trace_fun("ICAP_FLASHUSED2\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_IMAGEFILTER))
      trace_fun("ICAP_IMAGEFILTER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_NOISEFILTER))
      trace_fun("ICAP_NOISEFILTER\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_OVERSCAN))
      trace_fun("ICAP_OVERSCAN\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_AUTOMATICBORDERDETECTION))
      trace_fun("ICAP_AUTOMATICBORDERDETECTION\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_AUTOMATICDESKEW))
      trace_fun("ICAP_AUTOMATICDESKEW\n");
    if (TTWAIN_IsCapSupportedTW_INT16(ICAP_AUTOMATICROTATE))
      trace_fun("ICAP_AUTOMATICROTATE\n");
    trace_fun("end-of-list\n");
  }
  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/
  void TTWAIN_SetTwainUsage(TTWAIN_USAGE_MODE um) {
    TTwainData.transferInfo.usageMode = um;
  }
  /*---------------------------------------------------------------------------*/
  extern void registerTwainCallback();

  void TTWAIN_SetOnDoneCallback(TTWAIN_ONDONE_CB * proc, void *arg) {
    TTwainData.callback.onDoneCb  = proc;
    TTwainData.callback.onDoneArg = arg;
#ifdef MACOSX
    registerTwainCallback();
#endif
  }
  /*---------------------------------------------------------------------------*/
  void TTWAIN_SetOnErrorCallback(TTWAIN_ONERROR_CB * proc, void *arg) {
    TTwainData.callback.onErrorCb  = proc;
    TTwainData.callback.onErrorArg = arg;
  }
  /*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
