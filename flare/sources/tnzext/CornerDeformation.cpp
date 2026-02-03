

#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif
#include "ext/CornerDeformation.h"
#include "ext/StrokeDeformation.h"
#include "ext/SquarePotential.h"
//#include "ext/StrokeParametricDeformer.h"
//#include "ext/NotSymmetricBezierPotential.h"
#include "ext/ContextStatus.h"
#include "ext/Designer.h"
//#include "ext/TriParam.h"
#include "ext/ExtUtil.h"

#include "DeformationSelector.h"

//#include <tcurves.h>
//#include <tstrokeutil.h>

#include <algorithm>
#include <iterator>
#include <vector>

//#include "tstroke.h"
#include "tmathutil.h"

using namespace ToonzExt;

REGISTER(CornerDeformation, 2);

//-----------------------------------------------------------------------------

CornerDeformation::CornerDeformation() {
  this->setPotential(new SquarePotential);
  shortcutKey_ = ContextStatus::SHIFT;
}

//-----------------------------------------------------------------------------

CornerDeformation::~CornerDeformation() {}

//-----------------------------------------------------------------------------

void CornerDeformation::draw(Designer *designer) {
  StrokeDeformationImpl::draw(0);
  designer->draw(this);
}

//-----------------------------------------------------------------------------

bool CornerDeformation::findExtremes_(const ContextStatus *status,
                                      Interval &ret) {
  return ToonzExt::findNearestSpireCorners(status->stroke2change_, status->w_,
                                           ret, status->cornerSize_,
                                           &this->getSpiresList());
}

//-----------------------------------------------------------------------------

bool CornerDeformation::check_(const ContextStatus *status) {
  assert(status && "Not status available");

  TStroke *s = status->stroke2change_;
  double w   = status->w_;

  if (isASpireCorner(s, w, status->cornerSize_, &this->getSpiresList()))
    return true;

#ifdef USE_TOLERANCE_IN_SELECTION

  // analyse if the selected distance is close to an extreme if an
  //  extreme is almost an extreme prefer to select extreme
  const TPointD pressed = status->stroke2change_->getPoint(status->w_);

  double pixelTolerance2 = sq(5 * status->pixelSize_);

  // array of corners
  std::vector<double> corners;

  // also if an straight extreme was not found prefer smooth
  if (cornersDetector(status->stroke2change_, status->cornerSize_, corners)) {
    while (!corners.empty()) {
      TPointD actual = status->stroke2change_->getPoint(corners.back());
      if (tdistance2(actual, pressed) < pixelTolerance2) {
        // status->w_ = corners.back();
        // out = CornerDeformation::instance();
        // break;
        this->w_ = corners.back();
        return true;
      }
      corners.pop_back();
    }
  }
#endif

  return false;
}

//-----------------------------------------------------------------------------

double CornerDeformation::findActionLength() {
  // just to limit action until parameters are not computed
  //  return  lengthOfAction_ < stroke2transform_->getLength() ?
  //          lengthOfAction_ : stroke2transform_->getLength();
  // return -1;//
  return stroke2manipulate_->getLength();
}

//-----------------------------------------------------------------------------

CornerDeformation *CornerDeformation::instance() {
  static CornerDeformation singleton;
  return &singleton;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
