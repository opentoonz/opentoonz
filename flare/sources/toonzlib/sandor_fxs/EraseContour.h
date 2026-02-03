#pragma once

// EraseContour.h: interface for the CEraseContour class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(                                                                  \
    AFX_ERASECONTOUR_H__53513372_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_)
#define AFX_ERASECONTOUR_H__53513372_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_

#include <memory>
#include <array>

#include "STColSelPic.h"
#include "toonz4.6/raster.h"
#include "SDef.h"

#define MAXNB_NEIGHBOURS 1500

class CEraseContour {
  UC_PIXEL *m_picUC;
  US_PIXEL *m_picUS;
  const RASTER *m_ras;
  std::shared_ptr<UCHAR> m_sel;
  int m_lX, m_lY;
  CCIL m_cil;
  std::array<SXYDW, MAXNB_NEIGHBOURS> m_neighbours;
  int m_nbNeighbours;

  void null();
  int makeSelectionCMAP32();
  int makeSelection();
  void eraseInkColors();
  void prepareNeighbours();
  bool findClosestPaint(const int xx, const int yy, I_PIXEL &ip);
  void sel0123To01();

public:
  CEraseContour()
      : m_picUC(0), m_picUS(0), m_ras(0), m_lX(0), m_lY(0), m_cil() {}
  virtual ~CEraseContour();
  int makeSelection(const CCIL &iil);
  int doIt(const CCIL &iil);

  template <class P>
  CEraseContour(CSTColSelPic<P> &p) {
    null();
    if (p.m_lX > 0 && p.m_lY > 0 && p.m_sel && p.m_pic && p.m_ras) {
      m_lX  = p.m_lX;
      m_lY  = p.m_lY;
      m_sel = p.m_sel;
      m_ras = p.m_ras;
      if (p.getType() == ST_RGBM) {
        m_picUC = (UC_PIXEL *)p.m_pic;
        m_picUS = 0;
      } else if (p.getType() == ST_RGBM64) {
        m_picUC = 0;
        m_picUS = (US_PIXEL *)p.m_pic;
      }
    }
  }
};

#endif  // !defined(AFX_ERASECONTOUR_H__53513372_34D4_11D6_B9E7_0040F674BE6A__INCLUDED_)
