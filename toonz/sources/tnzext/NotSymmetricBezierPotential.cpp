#include "ext/NotSymmetricBezierPotential.h"
#include "tstroke.h"

#include <tmathutil.h>
#include <tcurves.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

//---------------------------------------------------------------------------

struct myBlendFunc final {
    TQuadratic curve;

public:
    myBlendFunc(double t = 0.0) {
        // Initialize the quadratic curve with control points
        curve.setP0(TPointD(0.0, 1.0));  // Start point
        curve.setP1(TPointD(0.5 * (1.0 - t), 1.0));  // Control point
        curve.setP2(TPointD(1.0, 0.0));  // End point
    }

    double operator()(double x) {
        double out = 0.0;
        x = std::fabs(x);  // Use absolute value of x
        if (x >= 1.0) return 0.0;  // Return 0 if x is outside the range
        out = curve.getPoint(x).y;  // Get the y-coordinate of the curve at x
        return out;
    }
};

}  // namespace

namespace ToonzExt {

NotSymmetricBezierPotential::~NotSymmetricBezierPotential() {}

//-----------------------------------------------------------------------------

void NotSymmetricBezierPotential::setParameters_(const TStroke* ref, double w, double al) {
    assert(ref);  // Ensure the reference stroke is valid
    ref_ = ref;
    par_ = w;  // Parameter along the stroke
    actionLength_ = al;  // Length of the action

    strokeLength_ = ref->getLength();  // Total length of the stroke
    lengthAtParam_ = ref->getLength(par_);  // Length at the parameter point

    // Length from the click point to the start of the curve
    leftFactor_ = std::min(lengthAtParam_, actionLength_ * 0.5);

    // Length from the click point to the end of the curve
    if (areAlmostEqual(strokeLength_, lengthAtParam_, 0.001))
        rightFactor_ = 0.0;  // If the click point is at the end, set rightFactor to 0
    else
        rightFactor_ = std::min(strokeLength_ - lengthAtParam_, actionLength_ * 0.5);
}

//-----------------------------------------------------------------------------

double NotSymmetricBezierPotential::value_(double value2test) const {
    assert(0.0 <= value2test && value2test <= 1.0);  // Ensure the input is within valid range
    return this->compute_value(value2test);  // Compute the value at the given parameter
}

//-----------------------------------------------------------------------------

// Normalization of the parameter within the range interval
double NotSymmetricBezierPotential::compute_shape(double value2test) const {
    double x = ref_->getLength(value2test);  // Get the length at the parameter
    double shape = this->actionLength_ * 0.5;  // Half of the action length
    if (isAlmostZero(shape)) shape = 1.0;  // Avoid division by zero
    x = (x - lengthAtParam_) / shape;  // Normalize the parameter
    return x;
}

//-----------------------------------------------------------------------------

double NotSymmetricBezierPotential::compute_value(double value2test) const {
    myBlendFunc me;  // Create a blending function

    // On extremes, use a quadratic falloff: 1 - x^2
    //     2
    //  1-x
    //

    double x = 0.0;
    double res = 0.0;

    // Length at the parameter
    x = ref_->getLength(value2test);

    const double tolerance = 0.0;  // Tolerance for comparison (pixel-based)

    // If the parameter is at an extreme (start or end of the stroke)
    if (std::max(lengthAtParam_, 0.0) < tolerance ||
        std::max(strokeLength_ - lengthAtParam_, 0.0) < tolerance) {
        double tmp_al = actionLength_ * 0.5;

        // Compute the correct parameter considering the offset
        // Use a quadratic falloff: 1 - x^2
        //
        //       2
        //  m = x
        //
        if (leftFactor_ <= tolerance)
            x = 1.0 - x / tmp_al;
        else
            x = (x - (strokeLength_ - tmp_al)) / tmp_al;

        if (x < 0.0) return 0.0;  // Ensure x is within bounds
        assert(0.0 <= x && x <= 1.0 + TConsts::epsilon);  // Debug check
        x = std::min(x, 1.0);  // Clamp x to 1.0
        res = x * x;  // Quadratic falloff
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
                if (rightFactor_ != 0.0)
                    x = (length_at_value2test - lengthAtParam_) / rightFactor_;
                else
                    x = 0.0;

                assert(0.0 - TConsts::epsilon <= x && x <= 1.0 + TConsts::epsilon);  // Debug check
                if (isAlmostZero(x)) x = 0.0;  // Handle edge cases
                if (areAlmostEqual(x, 1.0)) x = 1.0;

                // Compute the blending factor
                double how_many_of_shape = (strokeLength_ - lengthAtParam_) / (actionLength_ * 0.5);
                assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);  // Debug check

                // Apply the blending function
                myBlendFunc bf(how_many_of_shape);
                return bf(x);
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

                // Compute the blending factor
                double how_many_of_shape = lengthAtParam_ / (actionLength_ * 0.5);
                assert(0.0 <= how_many_of_shape && how_many_of_shape <= 1.0);  // Debug check

                // Apply the blending function
                myBlendFunc bf(how_many_of_shape);
                return bf(diff);
            }
        }

        // Default behavior is an exponential falloff
        x = this->compute_shape(value2test);
        res = me(x);
    }
    return res;
}

//-----------------------------------------------------------------------------

Potential* NotSymmetricBezierPotential::clone() {
    return new NotSymmetricBezierPotential;  // Return a new instance of the potential
}

}  // namespace ToonzExt
//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
