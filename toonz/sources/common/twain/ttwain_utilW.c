

#include <windows.h>
#include "assert.h"

#include "ttwainP.h"
#include "ttwain_utilPD.h"
#include "ttwain_global_def.h"

#ifdef __cplusplus
extern "C" {
#endif

static int getColorCount(LPBITMAPINFOHEADER pbi) {
  if (pbi->biSize == sizeof(BITMAPCOREHEADER)) {
    LPBITMAPCOREHEADER lpbc = ((LPBITMAPCOREHEADER)pbi);
    return 1 << lpbc->bcBitCount;
  } else if (pbi->biClrUsed == 0)
    return 0xFFF & (1 << pbi->biBitCount);
  else
    return (int)pbi->biClrUsed;
}
/*---------------------------------------------------------------------------*/
static size_t BmiColorTableBytes(LPBITMAPINFOHEADER pbi) {
  return getColorCount(pbi) * sizeof(RGBQUAD);
}
/*---------------------------------------------------------------------------*/
static LPBYTE get_buffer(LPBITMAPINFOHEADER lpbi, int *iud) {
  LPBYTE buf = (LPBYTE)lpbi + lpbi->biSize + BmiColorTableBytes(lpbi);
  *iud       = (lpbi->biHeight > 0);
  return buf;
}
/*---------------------------------------------------------------------------*/
#define PelsPerMeter2DPI(ppm) (((float)ppm * 2.54) / 100.0)
/*---------------------------------------------------------------------------*/
int TTWAIN_Native2RasterPD(void *handle, void *the_ras, int *lx, int *ly) {
  assert(!"DAFARE");
  return 1;
}

#ifdef __cplusplus
}
#endif
