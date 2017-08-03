

#include "toonz4_6staff.h"
#include "trop.h"
#include "tconvert.h"
#include "toonz4.6/toonz.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
//---------------------------------------------------------------------

int isTTT(const char *fn) {
  int ln, i;
  char lfn[1024];

  strcpy(lfn, fn);
  ln = strlen(lfn);
  for (i = ln - 1; i >= 0; i--)
    if (lfn[i] == ' ') {
      lfn[i] = '\0';
    } else
      break;
  ln = strlen(lfn);
  if (toupper(lfn[ln - 1]) == (int)'T' && toupper(lfn[ln - 2]) == (int)'T' &&
      toupper(lfn[ln - 3]) == (int)'T')
    return 1;
  return 0;
}
//-----------------------------------------------------------------------

const char *strsave(const char *t) {
  char *s;
  s = (char *)malloc(strlen(t) + 1);
  strcpy(s, t);
  return s;
}

//-----------------------------------------------------------------------

void convertParam(double param[], const char *cParam[], int cParamLen) {
  std::string app;
  for (int i = 1; i < 12; i++) {
    app       = std::to_string(param[i]);
    cParam[i] = strsave(app.c_str());
  }
}

//-----------------------------------------------------------------------
