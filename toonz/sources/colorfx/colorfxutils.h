#pragma once

#ifndef COLORFXUTILS_H
#define COLORFXUTILS_H

#include "tgeometry.h"
#include "tpixel.h"
#include "trandom.h"
#include "tregionoutline.h"
#include "tcurves.h"

class TRegion;
class TFlash;

class RubberDeform {
  std::vector<T3DPointD> *m_pPolyOri;
  std::vector<T3DPointD> m_polyLoc;

  void deformStep();
  double avgLength();
  void refinePoly(const double rf = -1.0);
  void getBBox(TRectD &bbox);

public:
  RubberDeform();
  RubberDeform(std::vector<T3DPointD> *pPolyOri, const double rf = -1.0);
  void copyLoc2Ori() { *m_pPolyOri = m_polyLoc; };
  void copyOri2Loc() { m_polyLoc = *m_pPolyOri; };

  virtual ~RubberDeform();
  void deform(const double n);
};

#endif
