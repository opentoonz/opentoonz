

#include "ext/Potential.h"
#include <algorithm>

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(push)
#pragma warning(disable : 4290)
#endif

//-----------------------------------------------------------------------------

ToonzExt::Potential::Potential() { isValid_ = false; }

//-----------------------------------------------------------------------------

void ToonzExt::Potential::setParameters(const TStroke *ref, double w,
                                        double actionLength) {
  isValid_ = true;

  assert(ref);
  if (!ref) throw std::invalid_argument("Not valid stroke!!!");

  assert(actionLength != 0.0);
  if (actionLength == 0.0) actionLength = TConsts::epsilon;

  assert(0.0 <= w && w <= 1.0);

  if (0.0 > w || w > 1.0) {
    throw std::invalid_argument("Not valid parameter!!!");
    //  w = std::min(std::max(0.0,w),1.0);
  }

  this->setParameters_(ref, w, actionLength);
}

//-----------------------------------------------------------------------------

double ToonzExt::Potential::value(double at) const {
  if (!isValid_) throw std::range_error("Not yet initialized potential!");

  assert(0.0 <= at && at <= 1.0);

  if (0 > at || at > 1.0) at = std::min(std::max(at, 0.0), 1.0);

  return this->value_(at);
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------

#if defined(_MSC_VER) && (_MSC_VER <= 1200)
#pragma warning(pop)
#endif
