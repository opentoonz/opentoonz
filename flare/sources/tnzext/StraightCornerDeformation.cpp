

#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif
#include "ext/StraightCornerDeformation.h"
#include "ext/StrokeDeformation.h"
#include "ext/LinearPotential.h"
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

#include <tstroke.h>

using namespace ToonzExt;

REGISTER(StraightCornerDeformation, 3);

//-----------------------------------------------------------------------------

StraightCornerDeformation::StraightCornerDeformation() {
  setPotential(new LinearPotential);
  shortcutKey_ = ContextStatus::CTRL;
}

//-----------------------------------------------------------------------------

StraightCornerDeformation::~StraightCornerDeformation() {}

//-----------------------------------------------------------------------------

bool StraightCornerDeformation::findExtremes_(const ContextStatus *status,
                                              Interval &ret) {
  bool found = ToonzExt::findNearestStraightCorners(
      status->stroke2change_, status->w_, ret, &this->getStraightsList());
  // it is not a forced solution
  if ((status->key_event_ != shortcutKey_) && found)
    return found;
  else {
    // it is forced then probably i want to find
    found = ToonzExt::findNearestSpireCorners(
        status->stroke2change_, status->w_, ret, status->cornerSize_,
        &this->getSpiresList());
  }
  return found;
}

//-----------------------------------------------------------------------------

void StraightCornerDeformation::draw(Designer *designer) {
  StrokeDeformationImpl::draw(0);
  designer->draw(this);
}

//-----------------------------------------------------------------------------

bool StraightCornerDeformation::check_(const ContextStatus *status) {
  assert(status && "Not status available");

  TStroke *s = status->stroke2change_;
  double w   = status->w_;

  // check extremes in another way.
  if ((!s->isSelfLoop() && areAlmostEqual(w, 0.0)) || areAlmostEqual(w, 1.0)) {
    return isAStraightCorner(s, w, &this->getStraightsList());
  }

  ToonzExt::Interval ret;
  if (ToonzExt::findNearestStraightCorners(status->stroke2change_, status->w_,
                                           ret, &this->getStraightsList()) &&
      isAStraightCorner(s, w, &this->getStraightsList())) {
    if (ret.first > ret.second) {
      assert(s->isSelfLoop());
      if ((ret.first < w && w <= 1.0) || (0.0 <= w && w < ret.second))
        return true;
    } else {
      if (ret.first < w && w < ret.second) return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

double StraightCornerDeformation::findActionLength() {
  return stroke2manipulate_->getLength();
}

//-----------------------------------------------------------------------------

StraightCornerDeformation *StraightCornerDeformation::instance() {
  static StraightCornerDeformation singleton;
  return &singleton;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
