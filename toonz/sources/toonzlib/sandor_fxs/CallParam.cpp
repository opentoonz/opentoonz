

// CallParam.cpp: implementation of the CCallParam class.
//
//////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdlib.h>
#include "SDef.h"
#include "CallParam.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCallParam::CCallParam()
    : m_thickness(0.0)
    , m_rH(0.0)
    , m_rV(0.0)
    , m_rLR(0.0)
    , m_rRL(0.0)
    , m_accuracy(0.0)
    , m_randomness(0.0)
    , m_ink()
    , m_paint() {}

CCallParam::CCallParam(const int argc, const char *argv[], const int shrink)
    : m_thickness(0.0)
    , m_rH(0.0)
    , m_rV(0.0)
    , m_rLR(0.0)
    , m_rRL(0.0)
    , m_accuracy(0.0)
    , m_randomness(0.0)
    , m_ink()
    , m_paint() {
  if (argc == 8) {
    int i        = 7;
    m_thickness  = atof(argv[i--]) / (double)shrink;
    m_rH         = atof(argv[i--]) / 100.0;
    m_rH         = D_CUT_0_1(m_rH);
    m_rLR        = atof(argv[i--]) / 100.0;
    m_rLR        = D_CUT_0_1(m_rLR);
    m_rV         = atof(argv[i--]) / 100.0;
    m_rV         = D_CUT_0_1(m_rV);
    m_rRL        = atof(argv[i--]) / 100.0;
    m_rRL        = D_CUT_0_1(m_rRL);
    m_accuracy   = atof(argv[i--]);
    m_randomness = atof(argv[i--]);
    m_ink.set(argv[i--], 4095);
  }
}

CCallParam::~CCallParam() {}

bool CCallParam::isOK() {
  if (m_thickness < 1.0) return false;
  if (m_rH < 0.01 && m_rLR < 0.01 && m_rV < 0.01 && m_rRL < 0.01) return false;
  return true;
}
