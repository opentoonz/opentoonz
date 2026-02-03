#pragma once

#ifndef igs_warp_h
#define igs_warp_h

#ifndef IGS_WARP_EXPORT
#define IGS_WARP_EXPORT
#endif

namespace igs {
namespace warp {
IGS_WARP_EXPORT void hori_change(
    float *image, const int height, const int width, const int channels,
    const float *refer,  // by height,width,channels
    const int refchannels, const int refcc, const double maxlen = 1.0,
    const bool alpha_rendering_sw = true, const bool anti_aliasing_sw = true);
IGS_WARP_EXPORT void vert_change(
    float *image, const int height, const int width, const int channels,
    const float *refer,  // by height,width,channels
    const int refchannels, const int refcc, const double maxlen = 1.0,
    const bool alpha_rendering_sw = true, const bool anti_aliasing_sw = true);
}  // namespace warp
}  // namespace igs

#endif /* !igs_warp_h */
