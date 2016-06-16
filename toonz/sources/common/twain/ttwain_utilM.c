

#include "ttwain_utilPD.h"
#include "ttwain.h"

#ifdef __cplusplus
extern "C" {
#endif

TW_HANDLE GLOBAL_LOCK(TW_HANDLE S) {
  HLock(S);
  return (TW_HANDLE)*S;
}

int TTWAIN_Native2RasterPD(void *handle, void *the_ras, int *lx, int *ly) {
  return 0;
}

#ifdef __cplusplus
}
#endif
