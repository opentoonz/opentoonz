#pragma once

#ifndef igs_maxmin_lens_matrix_h
#define igs_maxmin_lens_matrix_h

#include <vector>

namespace igs {
namespace maxmin {
const int diameter_from_outer_radius(const double outer_radius);
const double outer_radius_from_radius(const double radius,
                                      const double smooth_outer_range);
const int alloc_and_shape_lens_matrix(
    const double radius  // 0=<
    ,
    const double outer_radius, const int polygon_number  // =2
    ,
    const double roll_degree  // 0<= ... <=360
    ,
    std::vector<int> &lens_offsets, std::vector<int> &lens_sizes,
    std::vector<std::vector<double>> &lens_ratio);
/*
        lens_offsetsとlens_sizesで影響範囲を表わすmatrixを表わす
        radiusは影響円の半径
        matrix(縦横)サイズ(lens_offsets.size())は
                円(radius)が入る最小整数値でかつ、
                1以上の奇数(1,3,5,6)値
  */
const void reshape_lens_matrix(const double radius  // 0<=
                               ,
                               const double outer_radius,
                               const int odd_diameter,
                               const int polygon_number  // =2
                               ,
                               const double roll_degree  // 0<= ... <=360
                               ,
                               std::vector<int> &lens_offsets,
                               std::vector<int> &lens_sizes,
                               std::vector<std::vector<double>> &lens_ratio);
}
}

#endif /* !igs_maxmin_lens_matrix_h */
