#include "ext/NotSymmetricExpPotential.h"

#include <tmathutil.h>
#include <algorithm>
#include <cassert>
#include <cmath>

//-----------------------------------------------------------------------------

namespace {

//---------------------------------------------------------------------------

struct mySqr final {
public:
    double operator()(double x) { return 1.0 - sq(x); }  // Compute 1 - x^2
};

//---------------------------------------------------------------------------

struct myExp final {
public:
    double operator()(double x) { return exp(-sq(x)); }  // Compute exp(-x^2)
};

//---------------------------------------------------------------------------

struct blender {
    double operator()(double a, double b, double t) {
        assert(0.0 <= t && t <= 1.0);  // Ensure t is in the range [0, 1]
        return (a * (1.0 - t) + b * t);  // Linear interpolation between a and b
    }
};

//---------------------------------------------------------------------------

struct blender_2 {
    double operator()(double a, double b, double t) {
        assert(0.0 <= t && t <= 1.0);  // Ensure t is in the range [0, 1]

        // Quadratic interpolation: a(1-t)^2 + 2t*(1-t) + t^2
        double one_t = 1.0 - t;
        double num = 3.0;
        double den = 4.0;

        // Solve for the middle point to ensure the average of a and b at t = num/den
        double middle =
            sq(den) / (2.0 * num) * ((a + b) * 0.5 - (a + sq(num) * b) / sq(den));

        return a * sq(one_t) + middle * t * one_t + b * sq(t);  // Quadratic blend
    }
};

}  // namespace

//-----------------------------------------------------------------------------

void ToonzExt::NotSymmetricExpPotential::setParameters_(const TStroke* ref, double par, double al) {
    ref_ = ref;
    par_ = par;  // Parameter along the stroke
    actionLength_ = al;  // Length of the action

    assert(ref_);  // Ensure the reference stroke is valid

    strokeLength_ = ref->getLength();  // Total length of the stroke
    lengthAtParam_ = ref->getLength(par);  // Length at the parameter point

    // Length from the click point to the start of the curve
    leftFactor_ = std::min(lengthAtParam_, actionLength_ * 0.5);

    // Length from the click point to the end of the curve
    if (areAlmostEqual(strokeLength_, lengthAtParam_, 0.001))
        rightFactor_ = 0.0;  // If the click point is at the end, set rightFactor to 0
    else
        rightFactor_ = std::min(strokeLength_ - lengthAtParam_, actionLength_ * 0.5);

    // Consider the mapping interval as [-range_, range_].
    // A value of 2.8 corresponds to approximately 10^-6,
    // meaning a movement of 1 pixel in a million pixels.
    range_ = 2.8;
}

//-----------------------------------------------------------------------------

ToonzExt::NotSymmetricExpPotential::~NotSymmetricExpPotential() {}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricExpPotential::value_(double value2test) const {
    assert(0.0 <= value2test && value2test <= 1.0);  // Ensure the input is within valid range
    return this->compute_value(value2test);  // Compute the value at the given parameter
}

//-----------------------------------------------------------------------------

// Normalization of the parameter within the range interval
double ToonzExt::NotSymmetricExpPotential::compute_shape(double value2test) const {
    double x = ref_->getLength(value2test);  // Get the length at the parameter
    double shape = this->actionLength_ * 0.5;  // Half of the action length
    if (isAlmostZero(shape)) shape = 1.0;  // Avoid division by zero
    x = ((x - lengthAtParam_) * range_) / shape;  // Normalize the parameter
    return x;
}

//-----------------------------------------------------------------------------

double ToonzExt::NotSymmetricExpPotential::compute_value(double value2test) const {
    myExp me;  // Exponential function
    mySqr ms;  // Quadratic function
    blender op;  // Linear blending function

    // On extremes, use a quadratic falloff: 1 - x^2
    //     (                ) 2
    //     ( (x - l)*range_ )
    //   - (--------------- )
    //     ( factor         )
    //            l,r
    // When near an extreme, use a mix of quadratic and exponential functions

    double x = 0.0;
    double res = 0.0;

    // Length at the parameter
    x = ref_->getLength(value2test);

    const double tolerance = 2.0;  // Tolerance for comparison (pixel-based)

    // If the parameter is at an extreme (start or end of the stroke)
    if (std::max(lengthAtParam_, 0.0) < tolerance ||
        std::max(strokeLength_ - lengthAtParam_, 0.0) < tolerance) {
        double tmp_al = actionLength_ * 0.5;

        // Compute the correct parameter considering the offset
        // Use a quadratic falloff: 1 - x^2
        if (leftFactor_ <= tolerance)
            x = 1.0 - x / tmp_al;
        else
            x = (x - (strokeLength_ - tmp_al)) / tmp_al;

        if (x < 0.0) return 0.0;  // Ensure x is within bounds
        assert(0.0 <= x && x <= 1.0 + TConsts::epsilon);  // Debug check
        res = sq(x);  // Quadratic falloff
    } else {
        // When the parameter is not at an extreme
        double length_at_value2test = ref_->getLength(value2test);
        const double min_level = 0.01;  // Minimum level for blending

        // If the parameter is after the click point
        if (length_at_value2test >= lengthAtParam_) {
            // Check if the extreme can be moved from this parameter configuration
            double tmp_x = this->compute_shape(1.0);
            double tmp_res = me(tmp_x);
            if (tmp_res > min_level) {
                // Compute the normalized parameter
                x = (length_at_value2test - lengthAtParam_) / rightFactor_;
                assert(0.0 <= x && x <= 1.0);  // Debug check

                // Apply the exponential function
                double exp_val = me(x * range_);

                // Compute the blending factor
                double how_many_of_shape = (strokeLength_ - lengthAtParam_) / (actionLength_ * 0.5);
                assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);  // Debug check

                // Blend the quadratic and exponential functions
                return op(ms(x), exp_val, how_many_of_shape);
            }
        } else {
            // If the parameter is before the click point
            double tmp_x = this->compute_shape(0.0);
            double tmp_res = me(tmp_x);
            if (tmp_res > min_level) {
                double x = length_at_value2test / leftFactor_;
                assert(0.0 <= x && x <= 1.0);  // Debug check

                // Compute the difference
                double diff = x - 1.0;

                // Apply the exponential function
                double exp_val = me(diff * range_);

                // Compute the blending factor
                double how_many_of_shape = lengthAtParam_ / (actionLength_ * 0.5);
                assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);  // Debug check

                // Blend the quadratic and exponential functions
                return op(ms(diff), exp_val, how_many_of_shape);
            }
        }

        // Default behavior is an exponential falloff
        x = this->compute_shape(value2test);
        res = me(x);
    }
    return res;
}

//-----------------------------------------------------------------------------

ToonzExt::Potential* ToonzExt::NotSymmetricExpPotential::clone() {
    return new NotSymmetricExpPotential;  // Return a new instance of the potential
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
