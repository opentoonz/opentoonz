

#pragma warning(disable : 4996)

#include <windows.h>
#include <stdlib.h>

#include "ttwain_state.h"
#include "ttwainP.h"
#include "ttwain_statePD.h"
#include "ttwain_util.h"

#ifdef __cplusplus
extern "C" {
#endif

static void *hDSMLib; /* handle of DSM */
extern void TTWAIN_SetState(TWAINSTATE status);

int TTWAIN_LoadSourceManagerPD(void) {
  char winDir[_MAX_PATH];

  if (TTWAIN_GetState() >= TWAIN_SM_LOADED)
    return TRUE; /* DSM already loaded */

  GetWindowsDirectory(winDir, _MAX_PATH);
  if (!winDir[0]) return FALSE;

  strcat(winDir, "\\system32\\");
  // strcat(winDir, "\\");
  strcat(winDir, DSM_FILENAME);

  hDSMLib = LoadLibrary(winDir);

  /*
if (tnz_access(winDir, 0x00) != -1)
hDSMLib = LoadLibrary(winDir);
else
{
hDSMLib = 0;
return FALSE;
}
*/

  if (hDSMLib) {
    TTwainData.DSM_Entry =
        (DSMENTRYPROC)GetProcAddress(hDSMLib, DSM_ENTRYPOINT);
    if (TTwainData.DSM_Entry) {
      TTWAIN_SetAvailable(AVAIABLE_YES);
      TTWAIN_SetState(TWAIN_SM_LOADED);
    } else {
      FreeLibrary(hDSMLib);
      hDSMLib = NULL;
    }
  } else {
    DWORD err            = GetLastError();
    TTwainData.DSM_Entry = 0;
  }
  return (TTWAIN_GetState() >= TWAIN_SM_LOADED);
}
/*---------------------------------------------------------------------------*/
int TTWAIN_UnloadSourceManagerPD(void) {
  if (TTWAIN_GetState() == TWAIN_SM_LOADED) {
    if (hDSMLib) {
      FreeLibrary(hDSMLib);
      hDSMLib = NULL;
    }
    TTwainData.DSM_Entry = NULL;
    TTWAIN_SetState(TWAIN_PRESESSION);
  }
  return (TTWAIN_GetState() == TWAIN_PRESESSION);
}
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
