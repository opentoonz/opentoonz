

#include "tfile_io.h"

#ifdef _WIN32

#include <windows.h>
#include <assert.h>

static LPSTR AtlW2AHelper(LPSTR lpa, LPCWSTR lpw, int nChars, UINT acp) {
  assert(lpw != NULL);
  assert(lpa != NULL);
  /*
   verify that no illegal character present
   since lpa was allocated based on the size of lpw
don t worry about the number of chars
*/
  lpa[0] = '\0';
  WideCharToMultiByte(acp, 0, lpw, -1, lpa, nChars, NULL, NULL);
  return lpa;
}

char *convertWCHAR2CHAR(const wchar_t *fname) {
  int size      = 0;
  LPCWSTR lpw   = fname;
  char *name    = NULL;
  char *outName = 0;
  if (lpw) {
    LPSTR pStr = 0;
    size       = (lstrlenW(lpw) + 1) * 2;
    pStr       = (LPSTR)malloc(size * sizeof(char));
    name       = AtlW2AHelper(pStr, lpw, size, 0);
  }
  return name;
}

#else
#include <stdlib.h>
char *convertWCHAR2CHAR(const wchar_t *wc) {
  int count          = 0;
  const wchar_t *ptr = wc;
  char *c            = 0;
  char *buff;
  while ((*ptr) != '\0') {
    ++count;
    ++ptr;
  }
  c    = (char *)malloc((count + 1) * sizeof(char));
  buff = c;
  ptr  = wc;
  while ((*ptr) != '\0') {
    *c = *ptr;
    ++c;
    ++ptr;
  }
  *c = 0;
  return buff;
}
#endif
/*-----------------------------------*/
#if defined(MACOSX) || defined(LINUX) || defined(FREEBSD) || defined(HAIKU)

FILE *_wfopen(const wchar_t *fname, const wchar_t *mode) {
  char *cfname = convertWCHAR2CHAR(fname);
  char *cmode  = convertWCHAR2CHAR(mode);

  FILE *f = fopen(cfname, cmode);
  free(cfname);
  free(cmode);
  return f;
}

/*-----------------------------------*/

int _wstat(const wchar_t *fname, struct stat *buf) {
  char *cfname = convertWCHAR2CHAR(fname);
  int rc       = stat(cfname, buf);

  free(cfname);
  return rc;
}

/*-----------------------------------*/

int _wremove(const wchar_t *fname) {
  char *cfname = convertWCHAR2CHAR(fname);
  int rc       = remove(cfname);

  free(cfname);
  return rc;
}

#endif
