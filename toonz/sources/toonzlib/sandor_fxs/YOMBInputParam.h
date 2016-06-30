#pragma once

// YOMBInputParam.h: interface for the CYOMBInputParam class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(                                                                  \
    AFX_YOMBINPUTPARAM_H__41D42153_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
#define AFX_YOMBINPUTPARAM_H__41D42153_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_

#include "SDef.h"
#include "InputParam.h"

#define MAXNBCIL 4096  // 512

typedef struct color_index_list {
  int nb;
  unsigned short ci[MAXNBCIL];
} COLOR_INDEX_LIST;

class CYOMBInputParam final : public CInputParam {
public:
  bool m_isRandomSampling;
  bool m_isShowSelection;
  bool m_isStopAtContour;
  double m_dSample;
  int m_nbSample;
  double m_dA, m_dAB;
  // RGB parameters. List of RGB colors.
  int m_color[5][4];
  int m_nbColor;
  // CMAP Parameters. The list of ink & paint indices.
  COLOR_INDEX_LIST m_ink, m_paint;
  bool m_isCM;

  CYOMBInputParam()
      : m_isRandomSampling(false)
      , m_isShowSelection(false)
      , m_isStopAtContour(false)
      , m_dSample(0.0)
      , m_nbSample(0)
      , m_dA(0.0)
      , m_dAB(0.0)
      , m_nbColor(0)
      , m_isCM(false) {
    for (int i      = 0; i < 5; i++)
      m_color[i][0] = m_color[i][1] = m_color[i][2] = m_color[i][3] = 0;
    m_ink.nb = m_paint.nb = 0;
  };
  CYOMBInputParam(const CYOMBInputParam &p)
      : CInputParam(p)
      , m_isRandomSampling(p.m_isRandomSampling)
      , m_isShowSelection(p.m_isShowSelection)
      , m_isStopAtContour(p.m_isStopAtContour)
      , m_dSample(p.m_dSample)
      , m_nbSample(p.m_nbSample)
      , m_dA(p.m_dA)
      , m_dAB(p.m_dAB)
      , m_nbColor(p.m_nbColor)
      , m_isCM(p.m_isCM) {
    if (m_isCM) {
      m_ink.nb = p.m_ink.nb;
      int i    = 0;
      for (i = 0; i < m_ink.nb; i++) m_ink.ci[i] = p.m_ink.ci[i];
      m_paint.nb                                 = p.m_paint.nb;
      for (i = 0; i < m_paint.nb; i++) m_paint.ci[i] = p.m_paint.ci[i];
    } else
      for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++) m_color[i][j] = p.m_color[i][j];
  };

  CYOMBInputParam(const int argc, const char *argv[], const int shrink);
  CYOMBInputParam(const int argc, const char *argv[], const int shrink,
                  const bool isCM16);

  virtual ~CYOMBInputParam(){};

  void makeColorIndexList(const char *s, COLOR_INDEX_LIST &cil,
                          const int maxIndex);
  bool isRange(const char *s) const;
  int getRangeBegin(const char *s) const;
  int getRangeEnd(const char *s) const;
  void strToColorIndex(const char *s, COLOR_INDEX_LIST &cil,
                       const int maxIndex);
  void print(COLOR_INDEX_LIST &cil);

  //	void makeBlendString(const char *ss);
  bool isOK();
};

#endif  // !defined(AFX_YOMBINPUTPARAM_H__41D42153_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
