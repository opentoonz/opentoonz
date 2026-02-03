

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ttwain_state.h"
#include "ttwain_error.h"
#include "ttwain_util.h"

#ifdef __cplusplus
extern "C" {
#endif

static char Msg_out[1024];

#define HUMAN_MESSAGES
#ifdef HUMAN_MESSAGES
const char *RC_msg[] = {"SUCCESS",
                        "FAILURE",
                        "CHECK STATUS ('tried hard')",
                        "CANCEL",
                        "DS EVENT",
                        "NOT DSEVENT",
                        "TRANSFER DONE",
                        "END OF LIST",
                        "INFO NOT SUPPORTED",
                        "DATA NOT AVAILABLE"};

const char *CC_msg[] = {
    "SUCCESS",
    "FAILURE DUE TO UNKNOWN CAUSES",
    "LOW MEMORY",
    "NO DATA SOURCE",
    "DS IS CONNECTED TO MAX POSSIBLE APPS",
    "OPERATION ERROR, DS/DSM REPORTED ERROR",
    "UNKNOWN CAPABILITY",
    "undefined",
    "undefined",
    "UNRECOGNIZED TRIPLET",
    "DATA PARAMETER OUT OF RANGE",
    "TRIPLET OUT OF SEQUENCE",
    "UNKNOWN DESTINATION APP/SRC IN DSM_ESNTRY",
    "CAP NOT SUPPORTED BY SOURCE",
    "OPERATION NOT SUPPORTED BY CAP",
    "CAP HAS DEPENDANCY ON OTHER CAP",
    "FILE SYSTEM OPERATION IS DENIED (FILE IS PROTECTED)",
    "OPERATION FAILED BECAUSE FILE ALREADY EXISTS",
    "FILE NOT FOUND",
    "OPERATION FAILED BECAUSE DIRECTORY IS NOT EMPTY",
    "THE FEEDER IS JAMMED",
    "THE FEEDER DETECTED MULTIPLE PAGES",
    "ERROR WRITING THE FILE (MEANT FOR THINGS LIKE DISK FULL CONDITIONS)",
    "THE DEVICE WENT OFFLINE PRIOR TO OR DURING THIS OPERATION"};
#else
const char *RC_msg[] = {
    "TWRC_SUCCESS",         "TWRC_FAILURE",   "TWRC_CHECKSTATUS ('tried hard')",
    "TWRC_CANCEL",          "TWRC_DSEVENT",   "TWRC_NOTDSEVENT",
    "TWRC_XFERDONE",        "TWRC_ENDOFLIST", "TWRC_INFONOTSUPPORTED",
    "TWRC_DATANOTAVAILABLE"};

const char *CC_msg[] = {
    "TWCC_SUCCESS", "TWCC_BUMMER (Failure due to unknown causes)",
    "TWCC_LOWMEMORY", "TWCC_NODS (No Data Source)",
    "TWCC_MAXCONNECTIONS (DS is connected to max possible apps)",
    "TWCC_OPERATIONERROR (DS/DSM reported error, app shouldn't)",
    "TWCC_BADCAP (Unknown capability)", "7 (undefined)", "8 (undefined)",
    "TWCC_BADPROTOCOL (Unrecognized triplet)",
    "TWCC_BADVALUE (Data parameter out of range)",
    "TWCC_SEQERROR (Triplet out of sequence)",
    "TWCC_BADDEST (Unknown dest. App/Src in DSM_Esntry)",
    "TWCC_CAPUNSUPPORTED (Cap not supported by source)",
    "TWCC_CAPBADOPERATION (Operation not supported by cap)",
    "TWCC_CAPSEQERROR (Cap has dependancy on other cap)",
    "TWCC_DENIED      (File System operation is denied (file is protected))",
    "TWCC_FILEEXISTS (Operation failed because file already exists)",
    "TWCC_FILENOTFOUND (File not found)",
    "TWCC_NOTEMPTY (Operation failed because directory is not empty)",
    "TWCC_PAPERJAM (The feeder is jammed)",
    "TWCC_PAPERDOUBLEFEED (The feeder detected multiple pages)",
    "TWCC_FILEWRITEERROR (Error writing the file (meant for things like disk "
    "full conditions))",
    "TWCC_CHECKDEVICEONLINE (The device went offline prior to or during this "
    "operation)"};

#endif
/*---------------------------------------------------------------------------*/
void TTWAIN_RecordError(void) {
  char tmp[1024];
  TTwainData.ErrRC = TTWAIN_GetResultCode();
  if ((TTwainData.ErrRC == TWRC_FAILURE) ||
      (TTwainData.ErrRC == TWRC_CHECKSTATUS))
    TTwainData.ErrCC = TTWAIN_GetConditionCode();
  else
    TTwainData.ErrCC = -1;

  if (TTwainData.ErrRC < (sizeof(RC_msg) / sizeof(RC_msg[0]))) {
    snprintf(Msg_out, sizeof(Msg_out), "RC: %s(%d)",
             RC_msg[TTwainData.ErrRC], (int)TTwainData.ErrRC);
  } else {
    snprintf(Msg_out, sizeof(Msg_out), "RC: %s(%d)", "unknown",
             (int)TTwainData.ErrRC);
  }

  if (TTwainData.ErrCC < (sizeof(CC_msg) / sizeof(CC_msg[0]))) {
    snprintf(tmp, sizeof(tmp), "CC: %s(%d)", CC_msg[TTwainData.ErrCC],
             (int)TTwainData.ErrCC);
    strcat(Msg_out, tmp);
  } else {
    snprintf(tmp, sizeof(tmp), "CC: %s(%d)", "unknown",
             (int)TTwainData.ErrCC);
    strcat(Msg_out, tmp);
  }

  if (TTwainData.ErrRC == TWRC_FAILURE &&
      TTwainData.ErrCC == TWCC_OPERATIONERROR) {
#ifdef _WIN32
    OutputDebugString(Msg_out);
#else
#ifdef TOONZDEBUG
    printf("%s\n", Msg_out);
#endif
#endif
  }
  return;
}
/*---------------------------------------------------------------------------*/
char *TTWAIN_GetLastError(TUINT32 *rc, TUINT32 *cc) {
  assert(rc && cc);
  *rc = TTwainData.ErrRC;
  *cc = TTwainData.ErrCC;
  return Msg_out;
}
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
