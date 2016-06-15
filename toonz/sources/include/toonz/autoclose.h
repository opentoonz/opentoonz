#pragma once

#ifndef _TAUTOCLOSE_H_
#define _TAUTOCLOSE_H_

#include <memory>

#include "tgeometry.h"
#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TAutocloser {
public:
  typedef std::pair<TPoint, TPoint> Segment;

  TAutocloser(const TRasterP &r, int distance, double angle, int index,
              int opacity);
  ~TAutocloser();

  // calcola i segmenti e li disegna sul raster
  void exec();

  // non modifica il raster. Si limita a calcolare i segmenti
  void compute(std::vector<Segment> &segments);

  // disegna sul raster i segmenti
  void draw(const std::vector<Segment> &segments);

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

  // not implemented
  TAutocloser();
  TAutocloser(const TAutocloser &a);
  TAutocloser &operator=(const TAutocloser &a);
};

#endif
