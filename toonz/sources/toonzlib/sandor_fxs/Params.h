#pragma once

// Params.h: interface for the CParams class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PARAMS_H__40D9A921_F329_11D5_B92E_0040F674BE6A__INCLUDED_)
#define AFX_PARAMS_H__40D9A921_F329_11D5_B92E_0040F674BE6A__INCLUDED_

#ifdef _WIN32
#include "windows.h"
#endif
#include <fstream>
#include <vector>

#include "InputParam.h"

#define P(d) tmsg_info(" - %d -\n", d)

template <class EParam>
class CParams {
public:
  std::vector<EParam> m_params;
  double m_scale;

  CParams() : m_params(0), m_scale(1.0){};
  CParams(const CParams &cp) : m_params(cp.m_params), m_scale(cp.m_scale){};
  virtual ~CParams(){};

  CParams(const CInputParam &ip) : m_scale(ip.m_scale) {
    if (ip.m_isEconf) {
      // read(ip.m_econfFN);
    } else {
      m_params.resize(1);
      m_params[0].read(ip);
    }
  }
  /*
void print()
{	char s[1024];
  int i;

  OutputDebugString("===== PARAMS =====\n");
  snprintf(s, sizeof(s), "Scale=%f\n",m_scale);
  OutputDebugString(s);
  i=0;
  for( vector<EParam>::iterator p=m_params.begin();
          p!=m_params.end(); ++p,++i ){
          snprintf(s, sizeof(s), "--- %d. Param ---\n",i);
          OutputDebugString(s);
          p->print();
  }
  OutputDebugString("==================\n");
}
*/
  /*
void read(const basic_string<char>& name)
{	basic_ifstream<char> in(name.c_str());

  for( bool isOK=true; isOK; ) {
          m_params.resize(m_params.size()+1);
          vector<EParam>::iterator p=m_params.end();
          p--;
          isOK=p->read(in);
          if ( !isOK )
                  m_params.resize(m_params.size()-1);
  }
  in.close();
}
*/

  void scale(const double sc) {
    for (int i = 0; i < m_params.size(); ++i) m_params[i]->scale(sc);
  }

  void scale() {
    for (int i = 0; i < m_params.size(); ++i) m_params[i]->scale(m_scale);
  }
};

#endif  // !defined(AFX_PARAMS_H__40D9A921_F329_11D5_B92E_0040F674BE6A__INCLUDED_)
