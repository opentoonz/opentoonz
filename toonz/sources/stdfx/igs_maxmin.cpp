#include <string>
#include <limits>    /* std::numeric_limits */
#include <stdexcept> /* std::domain_error(-) */
#include "igs_maxmin_getput.h"
#include "igs_maxmin_multithread.h"
#include "igs_maxmin.h"

void igs::maxmin::convert(
    /* 入出力画像 */
    const unsigned char *inn, unsigned char *out

    ,
    const int height, const int width, const int channels, const int bits

    /* Pixel毎に効果の強弱 */
    ,
    const unsigned char *ref /* 求める画像(out)と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    /* Action Geometry */
    ,
    const double radius /* =1.0	 0...100...DOUBLE_MAX */
    ,
    const double smooth_outer_range /* =2.0  0...100...DOUBLE_MAX */
    ,
    const int polygon_number /* =2    2...16...INT_MAX */
    ,
    const double roll_degree /* =0.0  0...360...DOUBLE_MAX */

    /* Action Type */
    ,
    const bool min_sw /* =false */
    ,
    const bool alpha_rendering_sw /* =true  */
    ,
    const bool add_blend_sw /* =true  */

    /* Speed up */
    ,
    const int number_of_thread /* =1    1...24...INT_MAX */
    ) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
      ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    igs::maxmin::multithread<unsigned char, unsigned char> mthread(
        inn, out, height, width, channels

        ,
        ref, ref_mode

        ,
        radius, smooth_outer_range, polygon_number, roll_degree, min_sw,
        alpha_rendering_sw, add_blend_sw, number_of_thread);
    mthread.run();
    mthread.clear();
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    igs::maxmin::multithread<unsigned short, unsigned char> mthread(
        reinterpret_cast<const unsigned short *>(inn),
        reinterpret_cast<unsigned short *>(out), height, width, channels

        ,
        ref, ref_mode

        ,
        radius, smooth_outer_range, polygon_number, roll_degree, min_sw,
        alpha_rendering_sw, add_blend_sw, number_of_thread);
    mthread.run();
    mthread.clear();
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    igs::maxmin::multithread<unsigned short, unsigned short> mthread(
        reinterpret_cast<const unsigned short *>(inn),
        reinterpret_cast<unsigned short *>(out), height, width, channels

        ,
        reinterpret_cast<const unsigned short *>(ref), ref_mode

        ,
        radius, smooth_outer_range, polygon_number, roll_degree, min_sw,
        alpha_rendering_sw, add_blend_sw, number_of_thread);
    mthread.run();
    mthread.clear();
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    igs::maxmin::multithread<unsigned char, unsigned short> mthread(
        inn, out, height, width, channels

        ,
        reinterpret_cast<const unsigned short *>(ref), ref_mode

        ,
        radius, smooth_outer_range, polygon_number, roll_degree, min_sw,
        alpha_rendering_sw, add_blend_sw, number_of_thread);
    mthread.run();
    mthread.clear();
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
