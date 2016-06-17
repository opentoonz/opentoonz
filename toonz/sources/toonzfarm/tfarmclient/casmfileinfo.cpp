

#include "casmfileinfo.h"

namespace {

//==============================================================================

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/

#define SKIP_BLANKS(s)                                                         \
  {                                                                            \
    while (*s && (*s == ' ' || *s == '\t') && *s != 10 && *s != 13) s++;       \
  }

#define GET_WORD(s, p)                                                         \
  {                                                                            \
    while (*s && *s != ' ' && *s != '\t' && *s != 10 && *s != 13)              \
      *(p++) = *(s++);                                                         \
    *p       = '\0';                                                           \
  }

#define SKIP_WORD(s)                                                           \
  {                                                                            \
    while (*s && *s != ' ' && *s != '\t' && *s != 10 && *s != 13) s++;         \
  }

/*------------------------------------------------------------------------*/

#ifdef WIN32
#include <stdlib.h>
#define TOONZMAXPATHLEN _MAX_PATH
#else
#include <sys/param.h>
#define TOONZMAXPATHLEN MAXPATHLEN
#endif

/*------------------------------------------------------------------------*/

int get_frame_number(char *s) {
  int j, i = 0;
  while (s[i] && (s[i] < '0' || s[i] > '9')) i++;
  if (!s[i]) return 0;
  j = i;
  while (s[i] && (s[i] >= '0' && s[i] <= '9')) i++;
  s[i] = '\0';

  return (atoi(s + j));
}

//==============================================================================

int compute_casm_range(const char *filename, int &start, int &end,
                       bool &interlaced) {
  FILE *fp;
  bool first_flag = true;
  int cnt, frame = 1;
  char buffer[TOONZMAXPATHLEN], *s;

  start      = 0;
  end        = 0;
  interlaced = false;

  if (filename)
    fp = fopen(filename, "r");
  else
    fp = fdopen(0, "r");

  if (!fp) return 0;

  cnt = 0;

  while (fgets(buffer, TOONZMAXPATHLEN, fp)) {
    s = buffer;
    SKIP_BLANKS(s)
    if (strncmp(s, "FRAME", 5) == 0) {
      frame = get_frame_number(s);
      if (first_flag) {
        start      = frame;
        first_flag = false;
      }
      cnt++;
    } else if (strncmp(s, "FIELD", 5) == 0)
      interlaced = true;
  }

  if (!first_flag)
    end = frame;
  else {
    start = 0;
    end   = 0;
  }

  if (fp != stdin) fclose(fp);

  if (interlaced) {
    start = ((start) + 1) / 2;
    end   = ((end) + 1) / 2;
  }

  return cnt;
}
}

//==============================================================================

CasmFileInfo::CasmFileInfo(const TFilePath &fp) : m_fp(fp) {}

//------------------------------------------------------------------------------

void CasmFileInfo::getFrameRange(int &startFrame, int &endFrame,
                                 bool &interlaced) {
  int rc = compute_casm_range(toString(m_fp.getWideString()).c_str(),
                              startFrame, endFrame, interlaced);
}
