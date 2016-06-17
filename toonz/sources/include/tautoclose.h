#pragma once

#ifndef _TAUTOCLOSE_H_
#define _TAUTOCLOSE_H_

#include "tgeometry.h"
#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TROP_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TAutocloser {
public:
  typedef std::pair<TPoint, TPoint> Segment;

  TAutocloser(const TRasterP &r);
  ~TAutocloser();

  // if this function is never used, use default values
  void setClosingDistance(int d);
  int getClosingDistance() const;

  void setSpotAngle(double degrees);
  double getSpotAngle() const;

  void setInkIndex(int inkIndex);
  int getInkIndex() const;

  void compute(vector<Segment> &closingSegmentArray);

private:
  class Imp;
  Imp *m_imp;

  // not implemented
  TAutocloser();
  TAutocloser(const TAutocloser &a);
  TAutocloser &operator=(const TAutocloser &a);
};

#endif
