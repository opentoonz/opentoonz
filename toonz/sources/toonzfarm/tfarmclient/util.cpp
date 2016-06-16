

#include "util.h"
#include "tfilepath.h"
#include "texception.h"

#ifdef WIN32
#include <windows.h>
#endif

string convertToUncString(const TFilePath &fp) {
#ifdef WIN32

  DWORD cbBuff = 1024;  // Size of Buffer
  char szBuff[1024];    // Buffer to receive information
  REMOTE_NAME_INFO *prni = (REMOTE_NAME_INFO *)&szBuff;

  // Pointers to head of buffer
  UNIVERSAL_NAME_INFO *puni = (UNIVERSAL_NAME_INFO *)&szBuff;

  DWORD dwResult =
      WNetGetUniversalName(toString(fp.getWideString()).c_str(),
                           UNIVERSAL_NAME_INFO_LEVEL, (LPVOID)&szBuff, &cbBuff);

  switch (dwResult) {
  case NO_ERROR:
    return puni->lpUniversalName;

  case ERROR_NOT_CONNECTED:
    // The network connection does not exists.
    throw TException("The path specified doesn't refer to a shared folder");

  case ERROR_CONNECTION_UNAVAIL:
    // The device is not currently connected,
    // but it is a persistent connection.
    throw TException("The shared folder is not currently connected");

  default:
    throw TException("Cannot convert the path specified to UNC");
  }

#else
  return fp.getFullPath().c_str();
#endif
}
