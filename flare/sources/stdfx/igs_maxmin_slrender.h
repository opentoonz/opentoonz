#pragma once

#ifndef igs_maxmin_slrender_h
#define igs_maxmin_slrender_h

#include <vector>

namespace igs {
namespace maxmin {
namespace slrender {
void resize(const int odd_diameter, const int width, const bool alpha_ref_sw,
            std::vector<std::vector<double>> &tracks,
            std::vector<double> &alpha_ref, std::vector<double> &result);
void clear(std::vector<std::vector<double>> &tracks,
           std::vector<double> &alpha_ref, std::vector<double> &result);
void shift(std::vector<std::vector<double>> &tracks);
void render(const double radius, const double smooth_outer_range,
            const int polygon_number, const double roll_degree

            ,
            const bool min_sw

            ,
            std::vector<int> &lens_offsets, std::vector<int> &lens_sizes,
            std::vector<std::vector<double>> &lens_ratio

            ,
            const std::vector<std::vector<double>> &tracks,
            const std::vector<double> &alpha_ref, std::vector<double> &result);
}
}
}

#endif /* !igs_maxmin_slrender_h */
