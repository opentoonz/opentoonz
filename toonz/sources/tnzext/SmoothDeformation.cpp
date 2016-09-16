

#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif
#include "ext/SmoothDeformation.h"
#include "ext/StrokeDeformation.h"
//#include "ext/SquarePotential.h"
//#include "ext/StrokeParametricDeformer.h"
#include "ext/NotSymmetricBezierPotential.h"
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

REGISTER(SmoothDeformation, 1);

//-----------------------------------------------------------------------------

SmoothDeformation::SmoothDeformation() {
  setPotential(new NotSymmetricBezierPotential);
  shortcutKey_ = ContextStatus::ALT;
}

//-----------------------------------------------------------------------------

SmoothDeformation::~SmoothDeformation() {}

//-----------------------------------------------------------------------------

void SmoothDeformation::draw(Designer *designer) {
  StrokeDeformationImpl::draw(0);
  designer->draw(this);
}

//-----------------------------------------------------------------------------

bool SmoothDeformation::check_(const ContextStatus *status) {
  assert(status && "Not status available");

  if (!isASpireCorner(status->stroke2change_, status->w_, status->cornerSize_,
                      &this->getSpiresList()))
    return true;

  return false;
}

//-----------------------------------------------------------------------------

bool SmoothDeformation::findExtremes_(const ContextStatus *status,
                                      Interval &ret) {
  return ToonzExt::findNearestSpireCorners(status->stroke2change_, status->w_,
                                           ret, status->cornerSize_,
                                           &this->getSpiresList());
}

//-----------------------------------------------------------------------------

double SmoothDeformation::findActionLength() {
  // this means that all length needs to be used
  return 2.0 * stroke2manipulate_->getLength();
}

//-----------------------------------------------------------------------------

SmoothDeformation *SmoothDeformation::instance() {
  static SmoothDeformation singleton;
  return &singleton;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
