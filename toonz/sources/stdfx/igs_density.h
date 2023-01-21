#pragma once

#ifndef igs_density_h
#define igs_density_h

#ifndef IGS_DENSITY_EXPORT
#define IGS_DENSITY_EXPORT
#endif

namespace igs {
namespace density {
IGS_DENSITY_EXPORT void change(
    float *image_array, const int height, const int width,
    const int channels, /* 4(=RGBAでなければならない) */
    const float *ref, const double density = 1.0);
}
}  // namespace igs

#endif /* !igs_density_h */
