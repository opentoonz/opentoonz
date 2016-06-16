#pragma once

#ifndef igs_color_rgb_hls_h
#define igs_color_rgb_hls_h

namespace igs {
namespace color {
// Use add_hls & noise_hlsa
void rgb_to_hls(const double red, const double gre, const double blu,
                double &hue, double &lig, double &sat);
void hls_to_rgb(const double hue, const double lig, const double sat,
                double &red, double &gre, double &blu);
}
}

#endif /* !igs_color_rgb_hls_h */
