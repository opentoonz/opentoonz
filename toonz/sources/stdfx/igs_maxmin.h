#pragma once

#ifndef igs_maxmin_h
#define igs_maxmin_h

#ifndef IGS_MAXMIN_EXPORT
#define IGS_MAXMIN_EXPORT
#endif

namespace igs {
namespace maxmin {
IGS_MAXMIN_EXPORT void convert(
    /* 入出力画像 */
    const unsigned char *inn, unsigned char *out

    /* inn,out,ref(bits以外)の形状(geometry) */
    ,
    const int height, const int width, const int channels, const int bits

    /* Pixel毎に効果の強弱(pixel reference) */
    ,
    const unsigned char *ref /* 求める画像(out)と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    /* 処理のための形状値(action geometry) */
    ,
    const double radius /* =1.0	 0...100...DOUBLE_MAX */
    ,
    const double smooth_outer_range /* =2.0  0...100...DOUBLE_MAX */
                                    /*	smooth_outer_rangeは
0だとスムースさはなく、隣接ピクセルが現れた瞬間変化
1だとスムースな変化となる。元の形状を維持
1より大きいと影響の範囲がひろがり絵はぼやけてくる
*/
    ,
    const int polygon_number /* =2    2...16...INT_MAX */
                             /*	polygon_numberで3以上の値で
半径radiusの円に内接する多角形となる
絵では、円の右端が開始点
*/
    ,
    const double roll_degree /* =0.0  0...360...DOUBLE_MAX */
    /*	roll_degreeがプラスで時計回り方向に回転する
*/

    /* 処理の方法、動作スイッチ(action type/sw) */
    ,
    const bool min_sw /* =false */
    /*	min_sw==trueだと、小さい値のピクセルを拡大する
falseなら、大きい値のピクセルを拡大する
*/
    ,
    const bool alpha_rendering_sw /* =true  */
                                  /*	alpha_rendering_sw==trueなら、
alphaチャンネルにも処理を行う。
*/
    ,
    const bool add_blend_sw /* =false  */
    /*	add_blend_sw==trueだと、黒が入り込むべきところに、
alphaのマスクによってエッジが残ってしまう。
よってfalse固定 --> でなく、、、2013-12-21
*/

    /* 高速化のためのスレッド指定(thread count for speed up) */
    ,
    const int number_of_thread /* =1    1...24...INT_MAX */
    );
}
}

#endif /* !igs_maxmin_h */
