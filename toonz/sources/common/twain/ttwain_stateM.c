

/*max@home*/
#include "twain.h"
#include "ttwain_state.h"
#include "ttwainP.h"
#include "ttwain_statePD.h"
#include "ttwain_util.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void TTWAIN_SetState(TWAINSTATE status);

int TTWAIN_LoadSourceManagerPD(void) {
  if (TTWAIN_GetState() >= TWAIN_SM_LOADED)
    return TRUE; /* DSM already loaded */
  TTwainData.DSM_Entry = DSM_Entry;
  if (TTwainData.DSM_Entry != 0 /*kUnresolveCFragSymbolAddress*/) {
    TTWAIN_SetAvailable(AVAIABLE_YES);
    TTWAIN_SetState(TWAIN_SM_LOADED);
  } else {
    printf("DSM Entry NOT found !\n");
    return FALSE;
  }

  return (TTWAIN_GetState() >= TWAIN_SM_LOADED);
}
int TTWAIN_UnloadSourceManagerPD(void) {
  if (TTWAIN_GetState() == TWAIN_SM_LOADED) {
    TTwainData.DSM_Entry = 0;
    TTWAIN_SetState(TWAIN_PRESESSION);
  }
  return (TTWAIN_GetState() == TWAIN_PRESESSION);
}

/*-----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif
