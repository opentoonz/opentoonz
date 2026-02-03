

#include "ext/SelfLoopDeformation.h"
#include "DeformationSelector.h"
#include <tstroke.h>

using namespace ToonzExt;

// REGISTER(SelfLoopDeformation);

SelfLoopDeformation::SelfLoopDeformation() {}

//--------------------------------------------------------------------

SelfLoopDeformation::~SelfLoopDeformation() {}

//--------------------------------------------------------------------

void SelfLoopDeformation::activate_impl(Context *, DraggerStatus *) {
  assert(!"SelfLoopDeformation::activate not yet implemented!");
}

//--------------------------------------------------------------------

bool SelfLoopDeformation::check(Context *dragger, DraggerStatus *status) {
  assert(status && dragger && "Not dragger or status available");
  assert(!"SelfLoopDeformation::check not yet implemented!");
  //  lengthOfAction_ = status->lengthOfAction_;
  //  deformerSensibility_ = status->deformerSensibility_;
  //  stroke2move_ = status->stroke2change_;
  TStroke *s = stroke2move_;
  //  double &w = status->w_;

  if (s->isSelfLoop()) {
    //    dragger->changeDeformation(SelfLoopDeformation::instance());
    //    SelfLoopDeformation::instance()->activate(dragger,
    //                                              status);
    return true;
  }
  return false;
}

//--------------------------------------------------------------------

void SelfLoopDeformation::update_impl(Context *, const TPointD &delta) {}

//--------------------------------------------------------------------

void SelfLoopDeformation::deactivate_impl(Context *) {}

//--------------------------------------------------------------------

void SelfLoopDeformation::draw(Designer *dr) {}

//--------------------------------------------------------------------

SelfLoopDeformation *SelfLoopDeformation::instance() {
  static SelfLoopDeformation singleton;
  return &singleton;
}
