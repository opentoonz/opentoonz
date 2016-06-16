#pragma once

#ifndef igs_motion_wind_h
#define igs_motion_wind_h

#ifndef IGS_MOTION_WIND_EXPORT
#define IGS_MOTION_WIND_EXPORT
#endif

namespace igs {
namespace motion_wind {
IGS_MOTION_WIND_EXPORT void change(
    unsigned char *in

    ,
    const int hh, const int ww, const int cc, const int bb

    ,
    const unsigned char *ref

    ,
    const int rh, const int rw, const int rc, const int rb

    //, const int rz
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const int direction = 0  // 0(LtoR),1(BtoT),2(RtoL),3(TtoB)
    ,
    const bool blow_dark_sw = false  // false,true
    ,
    const bool blow_alpha_sw = true  // false,true

    ,
    const unsigned long length_random_seed = 0  // 0...
    ,
    const double length_min = 0.0, const double length_max = 18.0,
    const double length_bias = 0.0  // ...-1.0...0.0...1.0..
    ,
    const bool length_ref_sw = false  // false,true

    ,
    const unsigned long force_random_seed = 0  // 0...
    ,
    const double force_min = 0.0, const double force_max = 1.0,
    const double force_bias = 0.0  // ...-1.0...0.0...1.0..
    ,
    const bool force_ref_sw = false  // false,true

    ,
    const unsigned long density_random_seed = 0  // 0...
    ,
    const double density_min = 0.0, const double density_max = 1.0,
    const double density_bias = 0.0  // ...-1.0...0.0...1.0..
    ,
    const bool density_ref_sw = false  // false,true
    );
}
}

#endif /* !igs_motion_wind_h */
