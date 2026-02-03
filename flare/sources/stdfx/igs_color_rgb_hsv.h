#pragma once

#ifndef igs_color_rgb_hsv_h
#define igs_color_rgb_hsv_h

namespace igs {
namespace color {
// Use add_hsv & noise_hsva
void rgb_to_hsv(const double red, const double gre, const double blu,
                double &hue, double &sat, double &val);
void hsv_to_rgb(const double hue, const double sat, const double val,
                double &red, double &gre, double &blu);
}
}

#endif /* !igs_color_rgb_hsv_h */
