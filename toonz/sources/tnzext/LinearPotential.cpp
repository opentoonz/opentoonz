

#include "ext/LinearPotential.h"
#include <tmathutil.h>
#include <algorithm>
#include "tstroke.h"

using namespace std;

//-----------------------------------------------------------------------------

void ToonzExt::LinearPotential::setParameters_(const TStroke *ref, double par,
                                               double al) {
  ref_          = ref;
  par_          = par;
  actionLength_ = al;

  strokeLength_  = ref->getLength();
  lengthAtParam_ = ref->getLength(par);

  // lunghezza dal pto di click all'inizio della curva
  leftFactor_ = lengthAtParam_;

  // lunghezza dal pto di click alla fine
  rightFactor_ = strokeLength_ - lengthAtParam_;

  // considero come intervallo di mapping [-range,range].
  //  4 ha come valore c.a. 10exp-6
  //  cioé sposterei un pixel su un movimento di un milione di pixel
  range_ = 2.8;
}

//-----------------------------------------------------------------------------

ToonzExt::LinearPotential::~LinearPotential() {}

//-----------------------------------------------------------------------------

double ToonzExt::LinearPotential::value_(double value2test) const {
  assert(0.0 <= value2test && value2test <= 1.0);
  return this->compute_value(value2test);
}

//-----------------------------------------------------------------------------

// normalization of parameter in range interval
double ToonzExt::LinearPotential::compute_shape(double value2test) const {
  double x                       = ref_->getLength(value2test);
  double shape                   = this->actionLength_ * 0.5;
  if (isAlmostZero(shape)) shape = 1.0;
  x                              = ((x - lengthAtParam_) * range_) / shape;
  return x;
}

//-----------------------------------------------------------------------------

double ToonzExt::LinearPotential::compute_value(double value2test) const {
  // use
  //
  //  1-x
  //

  double x   = 0.0;
  double res = 0.0;

  // length  at parameter
  x = ref_->getLength(value2test);

  double tmp_al = actionLength_ * 0.5;

  if (isAlmostZero(par_) || areAlmostEqual(par_, 1.0)) tmp_al = actionLength_;

  // this is ckecked from wrapper class
  assert(tmp_al != 0.0);

  // compute correct parameter considering offset
  // try to have a square curve like shape
  //
  //  m = x
  //
  if (leftFactor_ == 0.0)
    x = 1.0 - x / tmp_al;
  else if (rightFactor_ == 0.0)
    x = (x - (strokeLength_ - tmp_al)) / tmp_al;
  else {
    if (x <= lengthAtParam_ && ((lengthAtParam_ - x) <= leftFactor_))
      x = (x - (lengthAtParam_ - leftFactor_)) / leftFactor_;
    else if (x > lengthAtParam_ && ((x - lengthAtParam_) < rightFactor_))
      x = (rightFactor_ - (x - lengthAtParam_)) / rightFactor_;
    else
      x = -1;
  }

  if (x < 0.0) return 0.0;
  // assert( 0.0 <= x &&
  //  x <= 1.0 + TConsts::epsilon );
  res = x;

  return res;
}

//-----------------------------------------------------------------------------

ToonzExt::Potential *ToonzExt::LinearPotential::clone() {
  return new LinearPotential;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
