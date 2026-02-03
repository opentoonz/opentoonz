

// YOMBInputParam.cpp: implementation of the CYOMBInputParam class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include "YOMBInputParam.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

bool CYOMBInputParam::isRange(const char *s) const {
  for (int i = 0; i < (int)strlen(s); i++)
    if (s[i] == '-') return true;
  return false;
}

int CYOMBInputParam::getRangeBegin(const char *s) const {
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

int CYOMBInputParam::getRangeEnd(const char *s) const {
  char ss[100];
  int i = 0;
  for (i = strlen(s) - 1; i >= 0; i--)
    if (s[i] == '-') break;
  strcpy(ss, &s[i + 1]);
  if (strlen(ss) == 0) return -1;
  return atoi(ss);
}

void CYOMBInputParam::strToColorIndex(const char *s, COLOR_INDEX_LIST &cil,
                                      const int maxIndex) {
  /* If s=="-1", all of the indices are put into the ColorIndexList
*/
  if (strcmp(s, "-1") == 0) {
    for (int i = 0; i <= maxIndex; i++)
      if (cil.nb < MAXNBCIL) {
        cil.ci[cil.nb] = (unsigned short)i;
        cil.nb++;
      } else
        return;
    return;
  }

  if (isRange(s)) {
    int begin = getRangeBegin(s);
    int end   = getRangeEnd(s);
    if (begin >= 0 && end >= 0) {
      begin = std::min(begin, maxIndex);
      end   = std::min(end, maxIndex);
      for (int i = std::min(begin, end);
           i <= std::max(begin, end) && cil.nb < MAXNBCIL; i++)
        cil.ci[cil.nb++] = (unsigned short)i;
    }
    // If there is no 'begin' or 'end' of range, it is considered
    // 'bad parameter'. For example: '-3' or '5-'.
    /*		else if ( begin>=0 && end<0 ) {
            if ( cil.nb<MAXNBCIL ) {
                    cil.ci[cil.nb]=begin;
                    cil.nb++;
            }
    } else if ( begin<0 && end>=0 ) {
            if ( cil.nb<MAXNBCIL ) {
                    cil.ci[cil.nb]=end;
                    cil.nb++;
            }
    }
*/
  } else {
    if (cil.nb < MAXNBCIL) {
      int q                                         = atoi(s);
      if (q >= 0 && q <= maxIndex) cil.ci[cil.nb++] = (unsigned short)q;
    }
  }
}

static int ushortCompare(const void *a, const void *b) {
  unsigned short *aa;
  unsigned short *bb;
  aa = (unsigned short *)a;
  bb = (unsigned short *)b;
  if (*aa == *bb) return 0;
  if (*aa < *bb) return -1;
  return 1;
}

void CYOMBInputParam::makeColorIndexList(const char *s, COLOR_INDEX_LIST &cil,
                                         const int maxIndex) {
  int ln = strlen(s);
  int i, j;
  char ss[100];
  COLOR_INDEX_LIST tmpCil;

  cil.nb = tmpCil.nb = 0;
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
  qsort(tmpCil.ci, tmpCil.nb, sizeof(unsigned short), ushortCompare);

  if (tmpCil.nb > 0) {
    cil.ci[0] = tmpCil.ci[0];
    cil.nb    = 1;
    for (i = j = 1; i < tmpCil.nb; i++)
      if (tmpCil.ci[i] != tmpCil.ci[i - 1]) cil.ci[cil.nb++] = tmpCil.ci[i];
  } else
    cil.nb = 0;
}

// Constructor of RGBM input parameters.
// It supposes 30 parameters:
//		0,1,2,3,4		isColor4, color4 (RGBM)
//		5,6,7,8,9		isColor3, color3 (RGBM)
//		10,11,12,13,14		isColor2, color2 (RGBM)
//		15,16,17,18,19		isColor1, color1 (RGBM)
//		20,21,22,23,24		isColor0, color0 (RGBM)
//		25. sensitivity of selection (dAB)
//		26. nbSamples (Quality). The number of sampling pixels.
//		27. dSamples (Amount). The distance of sampling.
//		28. isStopAtContour (Y/N) Stops the sampling at contour line.
//		29. isRandomSampling (Y/N) Random or equal sampling.
CYOMBInputParam::CYOMBInputParam(const int argc, const char *argv[],
                                 const int shrink) {
  m_nbColor = m_nbSample = 0;
  m_dSample              = 0.0;
  m_scale                = shrink > 0 ? 1.0 / (double)shrink : 1.0;
  m_isEconf              = false;
  m_isShowSelection      = false;
  if (argc == 30) {
    m_isCM             = false;
    m_isRandomSampling = argv[29][0] == '0' ? false : true;
    m_isStopAtContour  = argv[28][0] == '0' ? false : true;
    // Amount - dSample
    m_dSample = atof(argv[27]) * m_scale;
    // Quality - nbSamples
    m_nbSample = (int)atof(argv[26]);
    if (shrink > 1) {
      //			if ( m_nbSample>2 ) {
      //				m_nbSample=
      //(int)((double)m_nbSample*sqrt(m_scale)+0.5);
      //				m_nbSample= m_nbSample<3 ? 3 :
      // m_nbSample;
      //			}
    } else
      m_nbSample = m_nbSample < 1 ? 1 : m_nbSample;
    if ((int)(m_dSample * m_dSample * 2.5) < m_nbSample)
      m_nbSample = (int)(m_dSample * m_dSample * 2.5);

    // Sensitivity - dist A
    m_dA = 3.0 * atof(argv[25]) / 50.0;
    m_dA = m_dA <= 0.001 ? 0.001 : m_dA;
    // Sensitivity - dist AB
    m_dAB = atof(argv[25]) / 50.0;
    m_dAB = m_dAB <= 0.001 ? 0.001 : m_dAB;

    int j     = 24;
    m_nbColor = 0;
    for (int i = 0; i < 5; i++) {
      if (atoi(argv[j--]) > 0) {
        m_color[m_nbColor][0] = atoi(argv[j--]);
        m_color[m_nbColor][1] = atoi(argv[j--]);
        m_color[m_nbColor][2] = atoi(argv[j--]);
        m_color[m_nbColor][3] = atoi(argv[j--]);
        m_nbColor++;
      } else
        j -= 4;
    }
  }
}

// Constructor of RGBM input parameters.
// It supposes 6 parameters:
//		0. Paint index list
//		1. Ink index list
//		2. nbSamples (Quality). The number of sampling pixels.
//		3. dSamples (Amount). The distance of sampling.
//		4. isStopAtContour (Y/N) Stops the sampling at contour line.
//		5. isRandomSampling (Y/N) Random or equal sampling.
// isCM16 - informs about the type of colormap CM16/CM24.
CYOMBInputParam::CYOMBInputParam(const int argc, const char *argv[],
                                 const int shrink, const bool isCM16) {
  int nbInk   = isCM16 ? 31 : 4095;
  int nbPaint = isCM16 ? 127 : 4095;

  m_nbColor = m_nbSample = 0;
  m_dSample              = 0.0;
  m_scale                = shrink > 0 ? 1.0 / (double)shrink : 1.0;
  m_isEconf              = false;
  m_isShowSelection      = false;
  m_dA = m_dAB = 0.01;
  if (argc == 6) {
    m_isCM             = true;
    m_isRandomSampling = argv[5][0] == '0' ? false : true;
    m_isStopAtContour  = argv[4][0] == '0' ? false : true;
    // Amount - dSample
    m_dSample = atof(argv[3]) * m_scale;
    // Quality - nbSamples
    m_nbSample = (int)atof(argv[2]);
    if (shrink > 1) {
      if (m_nbSample > 2) {
        m_nbSample = (int)((double)m_nbSample * sqrt(m_scale) + 0.5);
        m_nbSample = m_nbSample < 2 ? 2 : m_nbSample;
      }
    } else
      m_nbSample = m_nbSample < 1 ? 1 : m_nbSample;
    if ((int)(m_dSample * m_dSample * 2.5) < m_nbSample)
      m_nbSample = (int)(m_dSample * m_dSample * 2.5);

    // Preparing of INK and PAINT index lists
    makeColorIndexList(&argv[1][0], m_ink, nbInk);
    makeColorIndexList(&argv[0][0], m_paint, nbPaint);
  }
}

bool CYOMBInputParam::isOK() {
  if (!m_isCM && m_nbColor <= 1) return false;
  if (m_isCM && m_paint.nb <= 1) return false;
  if (m_dSample < 1.1) return false;
  if (m_nbSample <= 1) return false;
  //	if ( (int)(m_dSample*m_dSample*3.1)<m_nbSample)
  //		return false;

  return true;
}

void CYOMBInputParam::print(COLOR_INDEX_LIST &cil) {
  /*	tmsg_info("nb=%d ci=",cil.nb);
  for( int i=0; i<cil.nb; i++ )
          tmsg_info("i=%d %d",i,(int)cil.ci[i]);*/
}
