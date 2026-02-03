#pragma once

#ifndef igs_fog_h
#define igs_fog_h

#ifndef IGS_FOG_EXPORT
#define IGS_FOG_EXPORT
#endif

namespace igs {
namespace fog {
IGS_FOG_EXPORT bool have_change(
    const double radius  // 25.0(0 ... 100(DOUBLE_MAX))
    ,
    const double power  // 1.00(-2.00 ... 2.00)
    ,
    const double threshold_min  // 0.00(0.00 ... 1.01)
    );
IGS_FOG_EXPORT void convert(void *in, void *out, double *buffer

                            ,
                            const int height, const int width,
                            const int channels, const int bits

                            ,
                            const int number_of_thread = 1  // 1 ... INT_MAX

                            ,
                            const double radius = 25.0  // 0 ... 100(DOUBLE_MAX)
                            ,
                            const double curve = 1.0  // 0.01 ... 100
                            ,
                            const int polygon_number = 2  // 2 ... 16(INT_MAX)
                            ,
                            const double degree = 0  // 0 ... DOUBLE_MAX

                            ,
                            const double power = 1.0  // -2.00 ... 2.00
                            ,
                            const double threshold_min = 0.0  // 0.00 ... 1.01
                            ,
                            const double threshold_max = 0.0  // 0.00 ... 1.01
                            ,
                            const bool alpha_rendering_sw = false  // true,false
                            );
}
}

#endif /* !igs_fog_h */
