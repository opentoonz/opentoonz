#pragma once

#ifndef igs_line_blur_h
#define igs_line_blur_h

#ifndef IGS_LINE_BLUR_EXPORT
#define IGS_LINE_BLUR_EXPORT
#endif

namespace igs {
namespace line_blur {
IGS_LINE_BLUR_EXPORT void convert(
    /* 入出力画像 */
    const void *in  // no_margin
    ,
    void *out  // no_margin

    ,
    const int height  // no_margin
    ,
    const int width  // no_margin
    ,
    const int channels, const int bits

    /* Action Geometry */
    ,
    const int b_blur_count /* min=1   def=51   incr=1   max=100  */
    ,
    const double b_blur_power /* min=0.1 def=1    incr=0.1 max=10.0 */
    ,
    const int b_subpixel /* min=1   def=1    incr=1   max=8	 */
    ,
    const int b_blur_near_ref /* min=1   def=5    incr=1   max=100	 */
    ,
    const int b_blur_near_len /* min=1   def=160  incr=1   max=1000 */

    ,
    const int b_smudge_thick /* min=1   def=7    incr=1   max=100	 */
    ,
    const double b_smudge_remain
    /* min=0.0 def=0.85 incr=0.001 max=1.0 */

    ,
    const int v_smooth_retry /* min=0   def=100  incr=1   max=1000 */
    ,
    const int v_near_ref /* min=0   def=4    incr=1   max=100	 */
    ,
    const int v_near_len /* min=2   def=160  incr=1   max=1000 */

    ,
    const bool mv_sw /* false=OFF */
    ,
    const bool pv_sw /* false=OFF */
    ,
    const bool cv_sw /* false=OFF */
    ,
    const long reference_channel /* 3 =Alpha:RGBA orBGRA */
    ,
    const bool debug_save_sw /* false=OFF */
    ,
    const int brush_action /* 0 =Curve Blur ,1=Smudge Brush */
    );
}
}

#endif /* !igs_line_blur_h */
