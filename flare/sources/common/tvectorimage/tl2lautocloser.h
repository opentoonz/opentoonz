#pragma once

#ifndef T_L2LAUTOCLOSER_H
#define T_L2LAUTOCLOSER_H

#include <memory>

#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

// forward declaration
class TStroke;

//-----------------------------------------------------------------------------

//! Line-to-line autocloser
class DVAPI TL2LAutocloser {
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TL2LAutocloser();
  ~TL2LAutocloser();

  struct Segment {
    //! the segments returned by the Line-to-line autocloser
    TStroke *stroke0, *stroke1;  // the two involved strokes
    double w0, w1;               // the w-parameters
    TThickPoint p0, p1;          // the related points
    double dist2;                // the (squared) points distance
  };

  //! segments longer than sqrt(dist2) are not considered. the default is 50^2
  void setMaxDistance2(double dist2);
  double getMaxDistance2() const;

  //! search line-to-line autoclosing segments.
  //! Note: it computes and caches some information about stroke0 and stroke1
  //! you are not supposed to change strokes between two subsequent calls to
  //! search()
  void search(std::vector<Segment> &segments, TStroke *stroke0,
              TStroke *stroke1);

  //! use this method if you have already computed the intersections
  void search(std::vector<Segment> &segments, TStroke *stroke0,
              TStroke *stroke1, const std::vector<DoublePair> &intersection);

  //! debug only. show the internal state related to the last performed search
  void draw();

private:
  // not implemented
  TL2LAutocloser(const TL2LAutocloser &);
  const TL2LAutocloser &operator=(const TL2LAutocloser &);
};

//---------------------------------------------------------------------------------------

#endif  // T_L2LAUTOCLOSER_H
