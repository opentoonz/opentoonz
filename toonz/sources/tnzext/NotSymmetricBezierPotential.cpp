#include "ext/NotSymmetricBezierPotential.h"
#include "tstroke.h"

#include <tmathutil.h>
#include <tcurves.h>

#include <algorithm>

using namespace std;

using namespace std;
using namespace ToonzExt;
//-----------------------------------------------------------------------------

namespace {
typedef unary_function<double, double> unary_functionDD;

//---------------------------------------------------------------------------

class myBlendFunc : unary_functionDD {
  // TCubic  c;
  TQuadratic curve;

public:
  myBlendFunc(double t = 0.0) {
    curve.setP0(TPointD(0.0, 1.0));
    curve.setP1(TPointD(0.5 * (1.0 - t), 1.0));
    curve.setP2(TPointD(1.0, 0.0));
  }

  result_type operator()(argument_type x) {
    result_type out = 0.0;
    x               = fabs(x);
    if (x >= 1.0) return 0.0;
    out = curve.getPoint(x).y;
    return out;
  }
};
}

//-----------------------------------------------------------------------------

ToonzExt::NotSymmetricBezierPotential::~NotSymmetricBezierPotential() {}

//-----------------------------------------------------------------------------

void ToonzExt::NotSymmetricBezierPotential::setParameters_(const TStroke *ref,
                                                           double w,
                                                           double al) {
  assert(ref);
  ref_          = ref;
  par_          = w;
  actionLength_ = al;

  strokeLength_  = ref->getLength();
  lengthAtParam_ = ref->getLength(par_);

  // lunghezza dal pto di click all'inizio della curva
  leftFactor_ = min(lengthAtParam_,
                    actionLength_ * 0.5);  // lengthAtParam_ / strokeLength_;

  // lunghezza dal pto di click alla fine
  rightFactor_ = min(strokeLength_ - lengthAtParam_, actionLength_ * 0.5);
}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricBezierPotential::value_(double value2test) const {
  assert(0.0 <= value2test && value2test <= 1.0);
  return this->compute_value(value2test);
}

//-----------------------------------------------------------------------------

// normalization of parameter in range interval
double ToonzExt::NotSymmetricBezierPotential::compute_shape(
    double value2test) const {
  double x                       = ref_->getLength(value2test);
  double shape                   = this->actionLength_ * 0.5;
  if (isAlmostZero(shape)) shape = 1.0;
  x                              = (x - lengthAtParam_) / shape;
  return x;
}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricBezierPotential::compute_value(
    double value2test) const {
  myBlendFunc me;

  // on extremes use
  //     2
  //  1-x
  //

  // when is near to extreme uses a mix notation
  double x   = 0.0;
  double res = 0.0;

  // length  at parameter
  x = ref_->getLength(value2test);

  const double tolerance = 0.0;  // need to be pixel based

  // if is an extreme
  if (max(lengthAtParam_, 0.0) < tolerance ||
      max(strokeLength_ - lengthAtParam_, 0.0) < tolerance) {
    double tmp_al = actionLength_ * 0.5;

    // compute correct parameter considering offset
    // try to have a square curve like shape
    //
    //       2
    //  m = x
    //
    if (leftFactor_ <= tolerance)
      x = 1.0 - x / tmp_al;
    else
      x = (x - (strokeLength_ - tmp_al)) / tmp_al;

    if (x < 0.0) return 0.0;
    assert(0.0 <= x && x <= 1.0 + TConsts::epsilon);
    x   = std::min(x, 1.0);  // just to avoid problem in production code
    res = sq(x);
  } else  // when is not an extreme
  {
    double length_at_value2test = ref_->getLength(value2test);

    const double min_level = 0.01;
    // if check a parameter over click point
    if (length_at_value2test >= lengthAtParam_) {
      // check if extreme can be moved from this parameter configuration
      double tmp_x   = this->compute_shape(1.0);
      double tmp_res = me(tmp_x);
      if (tmp_res > min_level) {
        // please note that in this case
        //  lengthAtParam_ + rightFactor_ == strokeLength_
        // (by ctor).
        if (rightFactor_ != 0.0)
          x = (length_at_value2test - lengthAtParam_) / rightFactor_;
        else
          x = 0.0;

        assert(0.0 - TConsts::epsilon <= x && x <= 1.0 + TConsts::epsilon);
        if (isAlmostZero(x)) x = 0.0;
        if (areAlmostEqual(x, 1.0)) x = 1.0;

        double how_many_of_shape =
            (strokeLength_ - lengthAtParam_) / (actionLength_ * 0.5);
        assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);

        myBlendFunc bf(how_many_of_shape);

        return bf(x);
      }
    } else {
      // leftFactor_
      double tmp_x   = this->compute_shape(0.0);
      double tmp_res = me(tmp_x);
      if (tmp_res > min_level) {
        double x = length_at_value2test / leftFactor_;
        assert(0.0 <= x && x <= 1.0);

        // then movement use another shape
        double diff = x - 1.0;

        double how_many_of_shape = lengthAtParam_ / (actionLength_ * 0.5);
        assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);

        myBlendFunc bf(how_many_of_shape);
        return bf(diff);
      }
    }

    // default behaviour is an exp
    x   = this->compute_shape(value2test);
    res = me(x);
  }
  return res;
}

//-----------------------------------------------------------------------------

ToonzExt::Potential *ToonzExt::NotSymmetricBezierPotential::clone() {
  return new NotSymmetricBezierPotential;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
