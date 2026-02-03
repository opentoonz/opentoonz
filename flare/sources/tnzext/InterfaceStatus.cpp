

#include "ext/InterfaceStatus.h"

#include "tgeometry.h"
#include "tvectorimage.h"

#include "ext/Types.h"
#include "ext/StrokeParametricDeformer.h"

#include <vector>

namespace ToonzExt {

//---------------------------------------------------------------------------

InterfaceStatus::InterfaceStatus() {}

InterfaceStatus::InterfaceStatus(const TPointD &curr, const TPointD &prev,
                                 const TPointD &down, double lengthOfAction,
                                 double deformerSensibility, double pixelSize,
                                 int cornerSize)
    : curr_(curr)
    , prev_(prev)
    , down_(down)
    , lengthOfAction_(lengthOfAction)
    , deformerSensibility_(deformerSensibility)
    , pixelSize_(pixelSize)
    , cornerSize_(cornerSize) {}

InterfaceStatus::~InterfaceStatus() {}

double InterfaceStatus::getDeformationLength() const { return lengthOfAction_; }

void InterfaceStatus::setDeformationLength(double val) {
  lengthOfAction_ = val;
}

double InterfaceStatus::getSensibility() const { return deformerSensibility_; }

void InterfaceStatus::setSensibility(double val) { deformerSensibility_ = val; }

double InterfaceStatus::getPixelSize() const { return pixelSize_; }

void InterfaceStatus::setPixelSize(double val) { pixelSize_ = val; }

int InterfaceStatus::getCornerSize() const { return cornerSize_; }

void InterfaceStatus::setCornerSize(int val) { cornerSize_ = val; }

void InterfaceStatus::init() {
  curr_ = prev_ = down_ = TPointD(-1, -1);

  lengthOfAction_      = -1;
  deformerSensibility_ = -1;
  pixelSize_           = 1;
  cornerSize_          = 120;
}
//---------------------------------------------------------------------------
}
