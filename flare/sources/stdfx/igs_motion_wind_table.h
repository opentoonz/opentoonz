#pragma once

#ifndef igs_motion_wind_table_h
#define igs_motion_wind_table_h

#include <vector>
#include "igs_math_random.h"

#ifndef IGS_MOTION_WIND_EXPORT
#define IGS_MOTION_WIND_EXPORT
#endif

namespace igs {
namespace motion_wind {
IGS_MOTION_WIND_EXPORT int table_size(const double length_min,
                                      const double length_max);
IGS_MOTION_WIND_EXPORT int
make_table(/* tableの有効長を返す */
           std::vector<double> &table, igs::math::random &length_random,
           igs::math::random &force_random, igs::math::random &density_random,
           const double length_min = 0.0, const double length_max = 1.0,
           const double length_bias = 1.0  // 0<...1...
           ,
           const double force_min = 0.0, const double force_max = 1.0,
           const double force_bias = 1.0  // 0<...1...
           ,
           const double density_min = 0.0, const double density_max = 1.0,
           const double density_bias = 1.0  // 0<...1...
           );
}
}

#endif /* !igs_motion_wind_table_h */
