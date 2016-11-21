

#include "ext/NotSymmetricExpPotential.h"

#include <tmathutil.h>
#include <algorithm>

using namespace std;

//-----------------------------------------------------------------------------

namespace {
typedef unary_function<double, double> unary_functionDD;

//---------------------------------------------------------------------------

class mySqr : unary_functionDD {
public:
  result_type operator()(argument_type x) { return 1.0 - sq(x); }
};

//---------------------------------------------------------------------------

class myExp : unary_functionDD {
public:
  result_type operator()(argument_type x) { return exp(-sq(x)); }
};

//---------------------------------------------------------------------------
struct blender {
  double operator()(double a, double b, double t) {
    assert(0.0 <= t && t <= 1.0);

    return (a * (1.0 - t) + b * t);
  }
};

//---------------------------------------------------------------------------

struct blender_2 {
  double operator()(double a, double b, double t) {
    assert(0.0 <= t && t <= 1.0);

    // a(1-t)^2 + 2t*(1-t) + t^2
    double one_t = 1.0 - t;
    double num   = 3.0;
    double den   = 4.0;
    // solution of a(1-t)^2 + P*2t(1-t) + b*t^2 = (a+b)/2
    // with t = num/den
    double middle =
        sq(den) / (2.0 * num) * ((a + b) * 0.5 - (a + sq(num) * b) / sq(den));

    return a * sq(one_t) + middle * t * one_t + b * sq(t);
  }
};
}

//-----------------------------------------------------------------------------

void ToonzExt::NotSymmetricExpPotential::setParameters_(const TStroke *ref,
                                                        double par, double al) {
  ref_          = ref;
  par_          = par;
  actionLength_ = al;

  assert(ref_);

  strokeLength_  = ref->getLength();
  lengthAtParam_ = ref->getLength(par);

  // lunghezza dal pto di click all'inizio della curva
  leftFactor_ = min(lengthAtParam_,
                    actionLength_ * 0.5);  // lengthAtParam_ / strokeLength_;

  // lunghezza dal pto di click alla fine
  rightFactor_ = min(strokeLength_ - lengthAtParam_, actionLength_ * 0.5);

  // considero come intervallo di mapping [-range,range].
  //  4 ha come valore c.a. 10exp-6
  //  cioé sposterei un pixel su un movimento di un milione di pixel
  range_ = 2.8;
}

//-----------------------------------------------------------------------------

ToonzExt::NotSymmetricExpPotential::~NotSymmetricExpPotential() {}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricExpPotential::value_(double value2test) const {
  assert(0.0 <= value2test && value2test <= 1.0);
  return this->compute_value(value2test);
}

//-----------------------------------------------------------------------------

// normalization of parameter in range interval
double ToonzExt::NotSymmetricExpPotential::compute_shape(
    double value2test) const {
  double x                       = ref_->getLength(value2test);
  double shape                   = this->actionLength_ * 0.5;
  if (isAlmostZero(shape)) shape = 1.0;
  x                              = ((x - lengthAtParam_) * range_) / shape;
  return x;
}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricExpPotential::compute_value(
    double value2test) const {
  myExp me;
  mySqr ms;
  blender op;

  // usually use the form below
  //     (                ) 2
  //     ( (x - l)*range_ )
  //   - (--------------- )
  //     ( factor         )
  //            l,r
  //  e
  //
  //  where factor is computed like in constructor

  // on extremes use
  //     2
  //  1-x
  //

  // when is near to extreme uses a mix notation
  double x   = 0.0;
  double res = 0.0;

  // length  at parameter
  x = ref_->getLength(value2test);

  const double tolerance = 2.0;  // need to be pixel based
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
        x = (length_at_value2test - lengthAtParam_) / rightFactor_;
        assert(0.0 <= x && x <= 1.0);

        // then movement use another shape
        double exp_val = me(x * range_);

        double how_many_of_shape =
            (strokeLength_ - lengthAtParam_) / (actionLength_ * 0.5);
        assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);

        // return ms(x);
        return op(ms(x), exp_val, how_many_of_shape);
      }
    } else {
      // leftFactor_
      double tmp_x   = this->compute_shape(0.0);
      double tmp_res = me(tmp_x);
      if (tmp_res > min_level) {
        double x = length_at_value2test / leftFactor_;
        assert(0.0 <= x && x <= 1.0);

        // then movement use another shape
        double diff              = x - 1.0;
        double exp_val           = me(diff * range_);
        double how_many_of_shape = lengthAtParam_ / (actionLength_ * 0.5);
        assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);

        // return ms(diff);
        return op(ms(diff), exp_val, how_many_of_shape);
      }
    }

    // default behaviour is an exp
    x   = this->compute_shape(value2test);
    res = me(x);
  }
  return res;
}

//-----------------------------------------------------------------------------

ToonzExt::Potential *ToonzExt::NotSymmetricExpPotential::clone() {
  return new NotSymmetricExpPotential;
}

// DEL double  ToonzExt::NotSymmetricExpPotential::gradient(double value2test)
// const
// DEL {
// DEL   assert(false);
// DEL   //double x = this->compute_shape(value2test);
// DEL   //double res = -(2.0*range_) / actionLength_ * x * exp(-sq(x));
// DEL   //return res;
// DEL   return 0;
// DEL }

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
