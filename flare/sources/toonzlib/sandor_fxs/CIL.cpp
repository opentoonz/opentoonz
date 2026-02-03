

// CIL.cpp: implementation of the CCIL class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDef.h"
#include "CIL.h"

#include <algorithm>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

bool CCIL::isRange(const char *s) const {
  for (int i = 0; i < (int)strlen(s); i++)
    if (s[i] == '-') return true;
  return false;
}

int CCIL::getRangeBegin(const char *s) const {
  char ss[100];

  strcpy(ss, s);
  for (int i = 0; i < (int)strlen(ss); i++)
    if (ss[i] == '-') {
      ss[i] = '\0';
      break;
    }
  if (strlen(ss) == 0) return -1;
  return atoi(ss);
}

int CCIL::getRangeEnd(const char *s) const {
  char ss[100];

  int i = 0;
  for (i = strlen(s) - 1; i >= 0; i--)
    if (s[i] == '-') break;
  strcpy(ss, &s[i + 1]);
  if (strlen(ss) == 0) return -1;
  return atoi(ss);
}

void CCIL::strToColorIndex(const char *s, CCIL &cil, const int maxIndex) {
  /* If s=="-1", all of the indices are put into the ColorIndexList
*/
  if (strcmp(s, "-1") == 0) {
    for (int i             = 0; i <= maxIndex && cil.m_nb < MAXNBCI; i++)
      cil.m_ci[cil.m_nb++] = i;
    return;
  }

  if (isRange(s)) {
    int begin = getRangeBegin(s);
    int end   = getRangeEnd(s);
    if (begin >= 0 && end >= 0) {
      begin = std::min(begin, maxIndex);
      end   = std::min(end, maxIndex);
      for (int i = std::min(begin, end);
           i <= std::max(begin, end) && cil.m_nb < MAXNBCI; i++)
        cil.m_ci[cil.m_nb++] = i;
    }
  } else {
    if (cil.m_nb < MAXNBCI) {
      int q                                             = atoi(s);
      if (q >= 0 && q <= maxIndex) cil.m_ci[cil.m_nb++] = q;
    }
  }
}

static int cilCompare(const void *a, const void *b) {
  int *aa;
  int *bb;
  aa = (int *)a;
  bb = (int *)b;
  if (*aa == *bb) return 0;
  if (*aa < *bb) return -1;
  return 1;
}

void CCIL::set(const char *s, const int maxIndex) {
  int ln = strlen(s);
  int i, j;
  char ss[100];
  CCIL tmpCil;
  m_nb = tmpCil.m_nb = 0;

  if (strlen(s) == 0 || strstr(s, "all") != 0 || strstr(s, "ALL") != 0)
    strToColorIndex("-1", tmpCil, maxIndex);

  else
    for (i = 0; i < ln; i++) {
      if (s[i] != ',') {
        strcpy(ss, &s[i]);
        for (j = 0; j < (int)strlen(ss); j++)
          if (ss[j] == ',') {
            ss[j] = '\0';
            break;
          }
        i += (strlen(ss) - 1);
        strToColorIndex(ss, tmpCil, maxIndex);
      }
    }
  qsort(tmpCil.m_ci, tmpCil.m_nb, sizeof(int), cilCompare);
  if (tmpCil.m_nb > 0) {
    m_ci[0] = tmpCil.m_ci[0];
    m_nb    = 1;
    for (i = j = 1; i < tmpCil.m_nb; i++)
      if (tmpCil.m_ci[i] != tmpCil.m_ci[i - 1]) m_ci[m_nb++] = tmpCil.m_ci[i];
  } else
    m_nb = 0;
}

bool CCIL::isIn(const int ci) {
  for (int i = 0; i < m_nb && ci >= m_ci[i]; i++)
    if (ci == m_ci[i]) return true;
  return false;
}

void CCIL::print() { /*
                           tmsg_info("Nb=%d",m_nb);
                           for( int i=0; i<m_nb; i++ )
                                   tmsg_info("ci[%d]=%d",i,m_ci[i]);*/
}
