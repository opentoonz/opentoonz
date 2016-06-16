

#include "ext/StrokeStatus.h"

#include "tgeometry.h"
#include "tvectorimage.h"

#include "ext/Types.h"
#include "ext/StrokeParametricDeformer.h"

#include <vector>

namespace ToonzExt {

//---------------------------------------------------------------------------
/**
   *@class StrokeStatus
   *@brief This class mantains interal data for Dragger manipulator.
   */

StrokeStatus::StrokeStatus(TStroke *stroke2change, unsigned int n, double w,
                           double strokeLength)
    : stroke2change_(stroke2change)
    , n_(n)
    , w_(w)
    , strokeLength_(strokeLength) {}

//---------------------------------------------------------------------------

StrokeStatus::StrokeStatus() {}

StrokeStatus::~StrokeStatus() {}

//---------------------------------------------------------------------------

TStroke *StrokeStatus::getItself() const { return stroke2change_; }

double StrokeStatus::getW() const { return w_; }

unsigned int StrokeStatus::getId() const { return n_; }

double StrokeStatus::getLength() const { return strokeLength_; }

void StrokeStatus::setItself(TStroke *s) { stroke2change_ = s; }

void StrokeStatus::setW(double w) { w_ = w; }

void StrokeStatus::setId(unsigned int n) { n_ = n; }

void StrokeStatus::setLength(double l) { strokeLength_ = l; }

void StrokeStatus::init() {
  n_             = -1;
  w_             = -1;
  stroke2change_ = 0;
  strokeLength_  = -1;
}
//---------------------------------------------------------------------------
}
