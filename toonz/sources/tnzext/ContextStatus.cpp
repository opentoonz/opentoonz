

#include "ext/ContextStatus.h"
//#include "ext/StrokeParametricDeformer.h"

//-----------------------------------------------------------------------------

ToonzExt::ContextStatus::ContextStatus() { init(); }

//-----------------------------------------------------------------------------

ToonzExt::ContextStatus::~ContextStatus() {}

//-----------------------------------------------------------------------------

void ToonzExt::ContextStatus::init() {
  key_event_           = NONE;
  w_                   = -1;
  deformerSensitivity_ = -1;
  stroke2change_       = 0;
  lengthOfAction_      = 50;
  deformer_            = 0;
  pixelSize_           = 1.0;
  cornerSize_          = 90;
  isManual_            = false;  // default is automatic
                                 //  spireCorners_ = 0;
                                 //  straightCorners_ =0;
}

//-----------------------------------------------------------------------------

ToonzExt::ContextStatus::ContextStatus(const ContextStatus &ob) { *this = ob; }

//-----------------------------------------------------------------------------

ToonzExt::ContextStatus &ToonzExt::ContextStatus::operator=(
    const ContextStatus &ob) {
  w_                   = ob.w_;
  isManual_            = ob.isManual_;
  deformer_            = ob.deformer_;
  key_event_           = ob.key_event_;
  pixelSize_           = ob.pixelSize_;
  cornerSize_          = ob.cornerSize_;
  stroke2change_       = ob.stroke2change_;
  lengthOfAction_      = ob.lengthOfAction_;
  deformerSensitivity_ = ob.deformerSensitivity_;
  // spireCorners_ = ob.spireCorners_;
  // straightCorners_ =ob.straightCorners_;
  return *this;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
