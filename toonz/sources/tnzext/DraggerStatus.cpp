

#include "ext/DraggerStatus.h"
#include "ext/StrokeParametricDeformer.h"

///////////////////////////////////////////////////////////////////////////////

ToonzExt::DraggerStatus::DraggerStatus() { init(); }

///////////////////////////////////////////////////////////////////////////////

ToonzExt::DraggerStatus::~DraggerStatus() { delete deformer_; }

///////////////////////////////////////////////////////////////////////////////

void ToonzExt::DraggerStatus::init() {
  key_event_           = NONE;
  dist2_               = -1.0;
  n_                   = -1;
  w_                   = -1;
  strokeLenght_        = -1;
  deformerSensibility_ = -1;
  stroke2change_       = 0;
  lengthOfAction_      = 50;
  deformer_            = 0;
  leftActionLenght_    = -1;
  rightActionLenght_   = -1;
  pixelSize_           = 1.0;
  cornerSize_          = 90;
}

///////////////////////////////////////////////////////////////////////////////
