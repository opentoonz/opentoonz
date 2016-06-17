

#include "ext/StrokeDeformationAdapter.h"

ToonzExt::StrokeDeformationAdapter::StrokeDeformationAdapter() : ref_(0) {}

ToonzExt::StrokeDeformationAdapter::~StrokeDeformationAdapter() {}

void ToonzExt::StrokeDeformationAdapter::changeDeformation(
    StrokeDeformation *new_deformation) {
  ref_ = new_deformation;
}

void ToonzExt::StrokeDeformationAdapter::setStrokeDeformation(
    StrokeDeformation *sd) {
  ref_ = sd;
}

ToonzExt::StrokeDeformation *
ToonzExt::StrokeDeformationAdapter::getStrokeDeformation() {
  return ref_;
}
