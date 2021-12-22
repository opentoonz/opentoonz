

/*max@home*/
#ifdef __cplusplus
extern "C" {
#endif

#ifdef NOTE
ConvertOneValue2Enum() potrebbe essere fonte di errore se
    DCItemSize[one_value.ItemType] >
    DCItemSize[TW_INT8] e si utilizzano i bit piu significativi di one_value
        .Item
    ConvertEnumeration2Range() potrebbe restituire valori di StepSize non
    appropriati se enumeration.ItemType e TW_FIX32 potrebbe
    restituire valori MinValue non corretti se DCItemSize[xxx.ItemType] >
    DCItemSize[TW_INT8] e si utilizzano i bit piu significativi di xxx.Item
    ConvertEnum2OneValue ritorna(ovviamente) solo il valore corrente
#endif

#include <assert.h>
#include <string.h>

#include "ttwain_capability.h"
#include "ttwain_conversion.h"
#include "ttwain_error.h"
#include "ttwain_state.h"

#include "ttwain_global_def.h"

    /*---------------------------------------------------------------------------*/
    static const size_t DCItemSize[13] = {
    sizeof(TW_INT8),   sizeof(TW_INT16),  sizeof(TW_INT32), sizeof(TW_UINT8),
    sizeof(TW_UINT16), sizeof(TW_UINT32), sizeof(TW_BOOL),  sizeof(TW_FIX32),
    sizeof(TW_FRAME),  sizeof(TW_STR32),  sizeof(TW_STR64), sizeof(TW_STR128),
    sizeof(TW_STR255),
}; /* see twain.h */

/*---------------------------------------------------------------------------*/
#define TWON_TWON(TYPE1, TYPE2) (((TYPE1) << 8) | (TYPE2))
/*---------------------------------------------------------------------------*/
static int ConvertOneValue2Range(TW_ONEVALUE one_value, TW_RANGE *range);
static int ConvertEnumeration2Range(TW_ENUMERATION enumeration,
                                    TW_RANGE *range);
static int ConvertOneValue2Enum(TW_ONEVALUE one_value, TW_ENUMERATION *tw_enum);
static int ConvertEnum2OneValue(TW_ENUMERATION tw_enum, TW_ONEVALUE *one_value);
static int ConvertEnum2Array(TW_ENUMERATION tw_enum, TW_ARRAY *array);

static TUINT32 GetContainerSize(int nFormat, unsigned twty, TW_UINT32 nItems);
static int TTWAIN_GetCapability(TW_INT16 msgType, TW_UINT16 cap_id,
                                TW_UINT16 conType, void *data,
                                TUINT32 *cont_size);
/*------------------------------------------------------------------------*/
int TTWAIN_GetCap(TW_UINT16 cap_id, TW_UINT16 conType, void *data,
                  TUINT32 *cont_size) {
  return TTWAIN_GetCapability(MSG_GET, cap_id, conType, data, cont_size);
}
/*------------------------------------------------------------------------*/
int TTWAIN_GetCapCurrent(TW_UINT16 cap_id, TW_UINT16 conType, void *data,
                         TUINT32 *cont_size) {
  return TTWAIN_GetCapability(MSG_GETCURRENT, cap_id, conType, data, cont_size);
}
/*------------------------------------------------------------------------*/
int TTWAIN_GetCapQuery(TW_UINT16 cap_id, TW_UINT16 *pattern) {
  int rc;
  /* GCC9 during compilation shows that this code possible call
   * TTWAIN_GetCapability() with case TWON_TWON(TWON_RANGE, TWON_RANGE)
   * whitch cause stack corruption, so make 'data' big enough to store
   * TW_ONEVALUE. */
  TW_ONEVALUE data[1 + (sizeof(TW_RANGE) / sizeof(TW_ONEVALUE))];
  rc = TTWAIN_GetCapability(MSG_QUERYSUPPORT, cap_id, TWON_ONEVALUE, &data, 0);
  if (!rc) return FALSE;
  *pattern = (TW_UINT16)data[0].Item;
  return TRUE;
}
/*------------------------------------------------------------------------*/
static int TTWAIN_GetCapability(TW_INT16 msgType, TW_UINT16 cap_id,
                                TW_UINT16 conType, void *data,
                                TUINT32 *cont_size) {
  TW_CAPABILITY cap;
  void *pv;
  TW_ENUMERATION *my_enum;
  TW_ARRAY *my_array;
  TW_ONEVALUE *my_one;
  TW_RANGE *my_range;
  TUINT32 size = 0;

  if (!data && !cont_size) return FALSE;

  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    TTWAIN_ErrorBox("Attempt to get capability value below State 4.");
    return FALSE;
  }
  /* Fill in capability structure */
  cap.Cap = cap_id; /* capability id */
  cap.ConType =
      TWON_DONTCARE16; /* favorite type of container (should be ignored...) */
  cap.hContainer = NULL;

  if (TTWAIN_DS(DG_CONTROL, DAT_CAPABILITY, msgType, (TW_MEMREF)&cap) !=
      TWRC_SUCCESS)
    return FALSE;

  if (!cap.hContainer) return FALSE;

  if (msgType == MSG_QUERYSUPPORT) {
  }

  pv       = GLOBAL_LOCK(cap.hContainer);
  my_enum  = (TW_ENUMERATION *)pv;
  my_array = (TW_ARRAY *)pv;
  my_one   = (TW_ONEVALUE *)pv;
  my_range = (TW_RANGE *)pv;

  if (cont_size) {
    switch (TWON_TWON(cap.ConType, conType)) {
    case TWON_TWON(TWON_ENUMERATION, TWON_ENUMERATION):
      *cont_size = GetContainerSize(TWON_ENUMERATION, my_enum->ItemType,
                                    my_enum->NumItems);
      break;
    case TWON_TWON(TWON_ONEVALUE, TWON_ENUMERATION):
      *cont_size = GetContainerSize(TWON_ENUMERATION, my_one->ItemType, 1);
      break;
    case TWON_TWON(TWON_ARRAY, TWON_ARRAY):
      *cont_size =
          GetContainerSize(TWON_ARRAY, my_array->ItemType, my_array->NumItems);
      break;
    case TWON_TWON(TWON_ONEVALUE, TWON_ONEVALUE):
      *cont_size = GetContainerSize(TWON_ONEVALUE, my_one->ItemType, 1);
      break;
    case TWON_TWON(TWON_ENUMERATION, TWON_ARRAY):
      *cont_size =
          GetContainerSize(TWON_ARRAY, my_enum->ItemType, my_enum->NumItems);
      break;
    default:
      /*      tmsg_error("Unable to convert type %d to %d (cap 0x%x)\n",
       * cap.ConType, conType,cap_id);*/
      assert(0);
      GLOBAL_UNLOCK(cap.hContainer);
      GLOBAL_FREE(cap.hContainer);
      return FALSE;
    }
    GLOBAL_UNLOCK(cap.hContainer);
    GLOBAL_FREE(cap.hContainer);
    return TRUE;
  }

  switch (TWON_TWON(cap.ConType, conType)) {
  case TWON_TWON(TWON_ENUMERATION, TWON_ENUMERATION):
    size = GetContainerSize(cap.ConType, my_enum->ItemType, my_enum->NumItems);
    memcpy(data, my_enum, size);
    break;
  case TWON_TWON(TWON_ENUMERATION, TWON_RANGE):
    ConvertEnumeration2Range(*my_enum, (TW_RANGE *)data);
    break;
  case TWON_TWON(TWON_ENUMERATION, TWON_ONEVALUE):
    ConvertEnum2OneValue(*my_enum, (TW_ONEVALUE *)data);
    break;
  case TWON_TWON(TWON_ARRAY, TWON_ARRAY):
    size =
        GetContainerSize(cap.ConType, my_array->ItemType, my_array->NumItems);
    memcpy(data, my_array, size);
    break;
  case TWON_TWON(TWON_ONEVALUE, TWON_ONEVALUE):
    memcpy(data, my_one, sizeof(TW_ONEVALUE));
    break;
  case TWON_TWON(TWON_ONEVALUE, TWON_RANGE):
    ConvertOneValue2Range(*my_one, (TW_RANGE *)data);
    break;
  case TWON_TWON(TWON_ONEVALUE, TWON_ENUMERATION):
    ConvertOneValue2Enum(*my_one, (TW_ENUMERATION *)data);
    break;
  case TWON_TWON(TWON_RANGE, TWON_RANGE):
    memcpy(data, my_range, sizeof(TW_RANGE));
    break;
  case TWON_TWON(TWON_ENUMERATION, TWON_ARRAY):
    ConvertEnum2Array(*my_enum, (TW_ARRAY *)data);
    break;
  default:
    assert(0);
    GLOBAL_UNLOCK(cap.hContainer);
    GLOBAL_FREE(cap.hContainer);
    return FALSE;
  }
  GLOBAL_UNLOCK(cap.hContainer);
  GLOBAL_FREE(cap.hContainer);
  return TRUE;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static int ConvertOneValue2Range(TW_ONEVALUE one_value, TW_RANGE *range) {
  range->ItemType     = one_value.ItemType;
  range->MinValue     = one_value.Item;
  range->MaxValue     = one_value.Item;
  range->StepSize     = 0;
  range->DefaultValue = one_value.Item;
  range->CurrentValue = one_value.Item;
  return TRUE;
}
/*---------------------------------------------------------------------------*/
static int ConvertEnumeration2Range(TW_ENUMERATION enumeration,
                                    TW_RANGE *range) {
  range->ItemType = enumeration.ItemType;
  range->MinValue = enumeration.ItemList[0];
  range->MaxValue = enumeration.ItemList[enumeration.NumItems - 1];
  range->StepSize = (range->MaxValue - range->MinValue) / enumeration.NumItems;
  if (range->MaxValue < range->MinValue) {
    range->MaxValue = range->MinValue;
    range->StepSize = 0;
  }
  range->DefaultValue = enumeration.ItemList[enumeration.DefaultIndex];
  range->CurrentValue = enumeration.ItemList[enumeration.CurrentIndex];
  return TRUE;
}
/*---------------------------------------------------------------------------*/
static int ConvertOneValue2Enum(TW_ONEVALUE one_value,
                                TW_ENUMERATION *tw_enum) {
  tw_enum->ItemType     = one_value.ItemType;
  tw_enum->NumItems     = 1;
  tw_enum->CurrentIndex = 0; /* Current value is in ItemList[CurrentIndex] */
  tw_enum->DefaultIndex = 0; /* Powerup value is in ItemList[DefaultIndex] */
  tw_enum->ItemList[0]  = (TW_UINT8)one_value.Item;
  return TRUE;
}
/*---------------------------------------------------------------------------*/
static int ConvertEnum2OneValue(TW_ENUMERATION tw_enum,
                                TW_ONEVALUE *one_value) {
  unsigned char *base;
  TW_UINT32 ofs;
  TW_UINT32 itemSize;

  itemSize = DCItemSize[tw_enum.ItemType];
  base     = tw_enum.ItemList;
  ofs      = tw_enum.CurrentIndex * itemSize;

  one_value->ItemType = tw_enum.ItemType;
  one_value->Item     = 0;
  memcpy(&(one_value->Item), &(base[ofs]), itemSize);
  return TRUE;
}
/*---------------------------------------------------------------------------*/
static int ConvertEnum2Array(TW_ENUMERATION tw_enum, TW_ARRAY *array) {
  TW_UINT32 itemSize;
  TW_UINT32 listSize;

  itemSize        = DCItemSize[tw_enum.ItemType];
  listSize        = itemSize * tw_enum.NumItems;
  array->ItemType = tw_enum.ItemType;
  array->NumItems = tw_enum.NumItems;
  memcpy(&(array->ItemList), &(tw_enum.ItemList), listSize);
  return TRUE;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*	      SET CAP							     */
/*---------------------------------------------------------------------------*/
int TTWAIN_SetCap(TW_UINT16 cap_id, TW_UINT16 conType, TW_UINT16 itemType,
                  TW_UINT32 *value) {
  int rc = FALSE;
  TUINT32 size;
  TW_CAPABILITY *capability = 0;
  TW_HANDLE capabilityH     = 0;
  TW_ONEVALUE *container    = 0;
  TW_HANDLE containerH      = 0;

  size       = GetContainerSize(conType, itemType, 1);
  containerH = GLOBAL_ALLOC(GMEM_FIXED, size);
  if (!containerH) goto done;
  container = (TW_ONEVALUE *)GLOBAL_LOCK(containerH);

  container->ItemType = itemType;
  container->Item     = *value;
  capabilityH         = GLOBAL_ALLOC(GMEM_FIXED, sizeof(TW_CAPABILITY));
  if (!capabilityH) {
    GLOBAL_UNLOCK(containerH);
    GLOBAL_FREE(containerH);
    containerH = NULL;
    goto done;
  }

  capability             = (TW_CAPABILITY *)GLOBAL_LOCK(capabilityH);
  capability->ConType    = conType;
  capability->hContainer = containerH;

  if (TTWAIN_GetState() < TWAIN_SOURCE_OPEN) {
    /*TTWAIN_ErrorBox("Setting capability in State < 4.");*/
    TTWAIN_OpenSourceManager(0); /* Bring up to state 4 */
                                 /*goto done;*/
  }

  capability->Cap = cap_id; /* capability id */

  rc = (TTWAIN_DS(DG_CONTROL, DAT_CAPABILITY, MSG_SET, (TW_MEMREF)capability) ==
        TWRC_SUCCESS);

done:
  if (containerH) {
    GLOBAL_UNLOCK(containerH);
    GLOBAL_FREE(containerH);
  }

  if (capabilityH) {
    GLOBAL_UNLOCK(capabilityH);
    GLOBAL_FREE(capabilityH);
  }
  return rc;
}
/*---------------------------------------------------------------------------*/
static TUINT32 GetContainerSize(int nFormat, unsigned twty, TW_UINT32 nItems) {
  TUINT32 size;
  switch (nFormat) {
  case TWON_ONEVALUE:
    size = sizeof(TW_ONEVALUE);
    if (DCItemSize[twty] > sizeof(TW_UINT32)) {
      size += DCItemSize[twty] - sizeof(TW_UINT32);
    }
    break;
  case TWON_RANGE:
    size = sizeof(TW_RANGE);
    break;
  case TWON_ENUMERATION:
    size =
        sizeof(TW_ENUMERATION) + DCItemSize[twty] * nItems - sizeof(TW_UINT8);
    break;
  case TWON_ARRAY:
    size = sizeof(TW_ARRAY) + DCItemSize[twty] * nItems - sizeof(TW_UINT8);
    break;
  default:
    size = 0;
    break;
  } /* switch */
  return size;
}
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
