#pragma once

// InputParam.h: interface for the CInputParam class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INPUTPARAM_H__41D42157_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
#define AFX_INPUTPARAM_H__41D42157_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_

#ifdef _WIN32
#include "windows.h"
#endif
#include <algorithm>
#include <string>

class CInputParam {
public:
  double m_scale;
  bool m_isEconf;
  std::basic_string<char> m_econfFN;

  CInputParam() : m_scale(0), m_isEconf(false), m_econfFN(""){};
  CInputParam(const CInputParam &p)
      : m_scale(p.m_scale), m_isEconf(p.m_isEconf), m_econfFN(p.m_econfFN){};
  virtual ~CInputParam(){};
};

#endif  // !defined(AFX_INPUTPARAM_H__41D42157_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
