

// YOMBParam.cpp: implementation of the CYOMBParam class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#include "windows.h"
#endif
#include <iostream>
#include <fstream>

#include <vector>
//#include <stdio.h>
#include <string.h>
#include "YOMBParam.h"
#include "InputParam.h"
#include "YOMBInputParam.h"
#include "STColSelPic.h"
#include "BlurMatrix.h"
#include "SDef.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

void CYOMBParam::print() {
  /*
  char s[1024];

  OutputDebugString("   ----- YOMB Param -----\n");
  sprintf(s,"   Name=%s\n",m_name.c_str());
  OutputDebugString(s);
  sprintf(s,"   RandomSampling=%d\n",m_isRandomSampling);
  OutputDebugString(s);
  sprintf(s,"   ShowSelection=%d\n",m_isShowSelection);
  OutputDebugString(s);
  sprintf(s,"   StopAtContour=%d\n",m_isStopAtContour);
  OutputDebugString(s);
  sprintf(s,"   dSample=%f\n",m_dSample);
  OutputDebugString(s);
  sprintf(s,"   nbSample=%d\n",m_nbSample);
  OutputDebugString(s);
  sprintf(s,"   dA=%f\n",m_dA);
  OutputDebugString(s);
  sprintf(s,"   dAB=%f\n",m_dAB);
  OutputDebugString(s);

  for( vector<I_PIXEL>::iterator p=m_color.begin();
           p!=m_color.end();
           ++p ) {
          sprintf(s,"   RGBM=(%d,%d,%d,%d)\n",p->r,p->g,p->b,p->m);
          OutputDebugString(s);
  }
  OutputDebugString("   ----------------------\n");
*/
}

void CYOMBParam::null() {
  m_name             = "";
  m_isRandomSampling = false;
  m_isShowSelection  = false;
  m_isStopAtContour  = false;
  m_dSample          = 0.0;
  m_nbSample         = 0;
  m_dA               = 0.0;
  m_dAB              = 0.0;
  m_color.resize(0);
}

#ifdef _WIN32
bool CYOMBParam::read(basic_ifstream<char> &in) {
  char token[1000] = "";
  bool isBegin     = false;

  in >> token;
  while (in.good() || (!in.good() && isBegin && strcmp(token, "END") == 0)) {
    if (strcmp(token, "BEGIN_YOMB") == 0) isBegin = true;
    if (isBegin && strcmp(token, "NAME") == 0) in >> m_name;
    if (isBegin && strcmp(token, "STOP_AT_CONTOUR") == 0)
      m_isStopAtContour = true;
    if (isBegin && strcmp(token, "RANDOM_SAMPLING") == 0)
      m_isRandomSampling = true;
    if (isBegin && strcmp(token, "SHOW_SELECTION") == 0)
      m_isShowSelection = true;
    if (isBegin && strcmp(token, "BLUR_ON_PAIR") == 0) m_isBlurOnPair = true;
    if (isBegin && strcmp(token, "DSAMPLE") == 0) in >> m_dSample;
    if (isBegin && strcmp(token, "NBSAMPLE") == 0) in >> m_nbSample;
    if (isBegin && strcmp(token, "DA") == 0) in >> m_dA;
    if (isBegin && strcmp(token, "DAB") == 0) in >> m_dAB;
    if (isBegin && strcmp(token, "END") == 0) return true;
    if (isBegin && strcmp(token, "COLOR") == 0) {
      m_color.resize(m_color.size() + 1);
      vector<I_PIXEL>::iterator p = m_color.end();
      p--;
      p->r = p->g = p->b = p->m = 0;
      if (in.good()) in >> p->r;
      if (in.good()) in >> p->g;
      if (in.good()) in >> p->b;
    }
    if (in.good()) {
      token[0] = '\0';
      in >> token;
    }
  }
  return false;
}
#endif

void CYOMBParam::adjustToMatte(I_PIXEL &p) {
  double q = (double)p.m / 255.0;
  double r = (double)p.r * q;
  double g = (double)p.g * q;
  double b = (double)p.b * q;
  p.r      = I_ROUNDP(r);
  p.g      = I_ROUNDP(g);
  p.b      = I_ROUNDP(b);
}

void CYOMBParam::read(const CInputParam &ip) {
  const CYOMBInputParam *yombIP;

  yombIP             = static_cast<const CYOMBInputParam *>(&ip);
  m_name             = "";
  m_isRandomSampling = yombIP->m_isRandomSampling;
  m_isShowSelection  = yombIP->m_isShowSelection;
  m_isStopAtContour  = yombIP->m_isStopAtContour;
  m_dSample          = yombIP->m_dSample;
  m_nbSample         = yombIP->m_nbSample;
  m_dA               = yombIP->m_dA;
  m_dAB              = yombIP->m_dAB;
  m_isCM             = yombIP->m_isCM;

  if (m_isCM) {
    m_ink.nb = yombIP->m_ink.nb;
    int i;
    for (i = 0; i < m_ink.nb; i++) m_ink.ci[i] = yombIP->m_ink.ci[i];
    m_paint.nb                                 = yombIP->m_paint.nb;
    for (i = 0; i < m_paint.nb; i++) m_paint.ci[i] = yombIP->m_paint.ci[i];
  } else if (yombIP->m_nbColor > 1) {
    m_color.resize(yombIP->m_nbColor);
    for (int i = 0; i < yombIP->m_nbColor; i++) {
      m_color[i].r = yombIP->m_color[i][0];
      m_color[i].g = yombIP->m_color[i][1];
      m_color[i].b = yombIP->m_color[i][2];
      m_color[i].m = yombIP->m_color[i][3];
      adjustToMatte(m_color[i]);
    }
  }
}

void CYOMBParam::addPixel(I_PIXEL &p, const I_PIXEL &pp) {
  p.r += pp.r;
  p.g += pp.g;
  p.b += pp.b;
  p.m += pp.m;
}

int CYOMBParam::getColorIndex(const UCHAR c) {
  return (c > (UCHAR)155 ? 255 - (int)c : 155 - (int)c + m_color.size() / 2);
}

void CYOMBParam::scale(const double d) {
  if (d >= 0) {
    m_dSample *= d;
    int s = (int)((double)m_nbSample * d + 0.5);
    if (d < 0.99) {
      m_nbSample = s <= 1 ? 3 : m_nbSample;
    } else
      m_nbSample = s <= 1 ? 1 : m_nbSample;
  }
}

void CYOMBParam::makeItUS() {
  m_dA *= 200.0;
  m_dAB *= 200.0;
  makeColorsUS();
}

void CYOMBParam::makeColorsUS() {
  for (vector<I_PIXEL>::iterator p = m_color.begin(); p != m_color.end(); ++p) {
    p->r = PIX_USHORT_FROM_BYTE((UCHAR)p->r);
    p->g = PIX_USHORT_FROM_BYTE((UCHAR)p->g);
    p->b = PIX_USHORT_FROM_BYTE((UCHAR)p->b);
    p->m = PIX_USHORT_FROM_BYTE((UCHAR)p->m);
  }
}
