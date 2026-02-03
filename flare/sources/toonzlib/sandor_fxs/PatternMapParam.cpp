

// PatternMapParam.cpp: implementation of the CPatternMapParam class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "PatternMapParam.h"
//#include "FNSequence.h"
#include "SDef.h"
#include "SError.h"
#include "Pattern.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPatternMapParam::CPatternMapParam() { null(); }

CPatternMapParam::CPatternMapParam(const int argc, const char *argv[],
                                   const int shrink) {
  try {
    null();
    if (argc == 12) {
      double scale = shrink > 0 ? 1.0 / (double)shrink : 1.0;

      // strcpy(m_patternFn, argv[0]);
      // CFNSequence fns(argv[0]);
      // fns.getFn(m_patternFn,drawingFn);

      m_ink.set(argv[0], 4095);

      m_maxScale = std::max(atof(argv[1]) * scale, atof(argv[2]) * scale);
      m_minScale = std::min(atof(argv[1]) * scale, atof(argv[2]) * scale);

      m_maxDirAngle = std::max(atof(argv[3]), atof(argv[4]));
      m_minDirAngle = std::min(atof(argv[3]), atof(argv[4]));
      m_isRandomDir = atoi(argv[5]) > 0 ? true : false;

      double dmax = std::max(atof(argv[6]) * scale, 1.0);
      double dmin = std::max(atof(argv[7]) * scale, 1.0);
      m_maxDist   = std::max(dmax, dmin);
      m_minDist   = std::min(dmax, dmin);
      m_density   = shrink > 0 ? atof(argv[8]) * (double)shrink : atof(argv[8]);

      m_isKeepContour  = atoi(argv[9]) > 0 ? true : false;
      m_isUseInkColor  = atoi(argv[10]) > 0 ? true : false;
      m_isIncludeAlpha = atoi(argv[11]) > 0 ? true : false;
    }
  } catch (SError) {
    throw;
  }
}

CPatternMapParam::~CPatternMapParam() {}

void CPatternMapParam::null() {
  m_patternFn[0]   = '\0';
  m_ink.m_nb       = 0;
  m_isKeepContour  = false;
  m_isRandomDir    = false;
  m_minDirAngle    = 0.0;
  m_maxDirAngle    = 0.0;
  m_minScale       = 0.2;
  m_maxScale       = 0.2;
  m_isUseInkColor  = true;
  m_isIncludeAlpha = true;
  m_density        = 0.2;
  m_minDist        = 3.0;
  m_maxDist        = 3.0;
}

void CPatternMapParam::testValue() {
  strcpy(m_patternFn, "d:\\toonz_fx\\test\\artcontour\\e.ttt");

  m_ink.m_nb    = 1;
  m_ink.m_ci[0] = 0;

  m_isKeepContour = false;

  m_isRandomDir = false;
  m_minDirAngle = 0.0;
  m_maxDirAngle = 0.0;

  m_minScale = 0.3;
  m_maxScale = 0.4;

  m_isUseInkColor  = true;
  m_isIncludeAlpha = true;
  m_density        = 0.0;
  m_minDist        = 3.0;
  m_maxDist        = 3.0;
}

bool CPatternMapParam::isOK() { return true; }
