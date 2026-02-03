#pragma once

#ifndef igs_negate_h
#define igs_negate_h

#ifndef IGS_NEGATE_EXPORT
#define IGS_NEGATE_EXPORT
#endif

namespace igs {
namespace negate {
IGS_NEGATE_EXPORT void change(unsigned char *image_array, const int height,
                              const int width, const int channels,
                              const int bits,
                              const bool *sw_array /* channels size array */
                              );
}
}

#endif /* !igs_negate_h */
