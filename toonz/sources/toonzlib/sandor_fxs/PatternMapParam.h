#pragma once

// PatternMapParam.h: interface for the CPatternMapParam class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(                                                                  \
    AFX_PATTERNMAPPARAM_H__53513371_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_)
#define AFX_PATTERNMAPPARAM_H__53513371_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_

#include "CIL.h"

class CPatternMapParam {
public:
  char m_patternFn[1024];
  CCIL m_ink;
  bool m_isKeepContour;
  bool m_isRandomDir;
  double m_minDirAngle, m_maxDirAngle;
  double m_minScale, m_maxScale;
  bool m_isUseInkColor, m_isIncludeAlpha;
  double m_density, m_minDist, m_maxDist;

  CPatternMapParam();
  CPatternMapParam(const int argc, const char *argv[], const int shrink);
  virtual ~CPatternMapParam();

  void null();
  void testValue();
  bool isOK();
};

#endif  // !defined(AFX_PATTERNMAPPARAM_H__53513371_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_)
