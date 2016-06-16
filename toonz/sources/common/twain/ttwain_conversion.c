

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "ttwain_conversion.h"

#ifdef __cplusplus
extern "C" {
#endif

float TTWAIN_Fix32ToFloat(TW_FIX32 fix) {
  TW_INT32 val;
  val = ((TW_INT32)fix.Whole << 16) | ((TW_UINT32)fix.Frac & 0xffff);
  return (float)(val / 65536.0);
}
/*---------------------------------------------------------------------------*/
TW_FIX32 TTWAIN_FloatToFix32(float fl) {
  TW_FIX32 fix;
  TW_INT32 val;
  assert(sizeof(TW_FIX32) == sizeof(float));
  assert(sizeof(TW_FIX32) == sizeof(long));

  /* Note 1: This round-away-from-0 is new in TWAIN 1.7
Note 2: ANSI C converts float to int by truncating toward 0.*/
  val       = (TW_INT32)(fl * 65536.0 + (fl < 0 ? -0.5 : +0.5));
  fix.Whole = (TW_INT16)(val >> 16);     /* most significant 16 bits */
  fix.Frac  = (TW_UINT16)(val & 0xffff); /* least */

  return fix;
}
/*---------------------------------------------------------------------------*/
void TTWAIN_ConvertRevStrToRevNum(const char *rev_str, TW_UINT16 *maj_num,
                                  TW_UINT16 *min_num) {
  char *maj_str;
  char *min_str;
  size_t maj_size;
  size_t min_size;

  *maj_num = *min_num = 0;
  maj_size            = strcspn(rev_str, ".");
  maj_str             = (char *)calloc(sizeof(char), maj_size + 1);
  if (!maj_str) return;
  memcpy(maj_str, rev_str, maj_size); /*already 0term*/
  *maj_num = (TW_UINT16)atoi(maj_str);

  min_size = strlen(rev_str) - maj_size + 1;
  min_str  = (char *)calloc(sizeof(char), min_size + 1);
  if (!min_str) return;
  memcpy(min_str, &(rev_str[maj_size + 1]), min_size); /*already 0term*/
  *min_num = (TW_UINT16)atoi(min_str);
  if (maj_str) free(maj_str);
  if (min_str) free(min_str);
}
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
