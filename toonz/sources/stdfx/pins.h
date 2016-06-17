#pragma once

#ifndef PINS_H
#define PINS_H

//#include "tfxparam.h"
//#include "trop.h"

#include "trasterfx.h"

//#include "stdfx.h"
//#include "tfx.h"
//#include "tparamset.h"
#include "tgl.h"

#ifdef _WIN32
#include <GL/gl.h>
#include <GL/glu.h>
#endif

//#ifdef MACOSX
//#include <GLUT/glut.h>
//#define GLUT_NO_LIB_PRAGMA
//#define GLUT_NO_WARNING_DISABLE
//#endif

///#include "offscreengl.h"

class FourPoints {
public:
  TPointD m_p00;
  TPointD m_p01;
  TPointD m_p10;
  TPointD m_p11;

  FourPoints() : m_p00(), m_p01(), m_p10(), m_p11() {}

  FourPoints(const TPointD &p00, const TPointD &p01, const TPointD &p10,
             const TPointD &p11)
      : m_p00(p00), m_p01(p01), m_p10(p10), m_p11(p11) {}

  bool operator==(const FourPoints &fp) const {
    return m_p00 == fp.m_p00 && m_p10 == fp.m_p10 && m_p01 == fp.m_p01 &&
           m_p11 == fp.m_p11;
  }
  bool operator!=(const FourPoints &fp) const { return !(*this == fp); }
};

// this function computes the perspective trasformation defined with
//-transfFrom, transfTo- (the points in transfFrom goes in transfTo)
// and applies it on the fourPoints from, and returns the result)
FourPoints computeTransformed(const FourPoints &transfFrom,
                              const FourPoints &transfTo,
                              const FourPoints &from);

//--------------------------------------------------------------------------------

void subdivision(const TPointD &p00, const TPointD &p10, const TPointD &p11,
                 const TPointD &p01, const TPointD &tex00, const TPointD &tex10,
                 const TPointD &tex11, const TPointD &tex01,
                 const TRectD &clippingRect, int details);

void subCompute(TRasterFxPort &m_input, TTile &tile, double frame,
                const TRenderSettings &ri, TPointD p00, TPointD p01,
                TPointD p11, TPointD p10, int details, bool wireframe,
                TDimension m_offScreenSize, bool isCast = false);
#endif
