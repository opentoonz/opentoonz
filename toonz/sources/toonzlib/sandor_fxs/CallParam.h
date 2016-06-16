#pragma once

// CallParam.h: interface for the CCallParam class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CALLPARAM_H__2B094D95_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_)
#define AFX_CALLPARAM_H__2B094D95_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_

#include "CIL.h"

class CCallParam {
public:
  double m_thickness;
  double m_rH, m_rLR, m_rV, m_rRL;
  double m_accuracy, m_randomness;
  CCIL m_ink, m_paint;

  CCallParam();
  CCallParam(const int argc, const char *argv[], const int shrink);
  virtual ~CCallParam();
  bool isOK();
};

#endif  // !defined(AFX_CALLPARAM_H__2B094D95_25D9_11D6_B9C6_0040F674BE6A__INCLUDED_)
