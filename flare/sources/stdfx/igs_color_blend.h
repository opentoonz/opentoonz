#pragma once

#ifndef igs_color_blend_h
#define igs_color_blend_h

namespace igs {
namespace color {
// 01
void over(/* アルファ合成（通常) */
          double &dn_r, double &dn_g, double &dn_b, double &dn_a,
          const double up_r, double up_g, double up_b, double up_a,
          const double up_opacity, const bool do_clamp = true);
// 02
void darken(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
            const double up_r, double up_g, double up_b, double up_a,
            const double up_opacity, const bool do_clamp = true);
// 03
void multiply(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
              const double up_r, double up_g, double up_b, double up_a,
              const double up_opacity, const bool do_clamp = true);
// 04
void color_burn(/* 焼き込みカラー */
                double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                const double up_r, double up_g, double up_b, double up_a,
                const double up_opacity, const bool do_clamp = true);
// 05
void linear_burn(/* 焼き込みリニア */
                 double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                 const double up_r, double up_g, double up_b, double up_a,
                 const double up_opacity, const bool do_clamp = true);
// 06
void darker_color(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                  const double up_r, double up_g, double up_b, double up_a,
                  const double up_opacity, const bool do_clamp = true);
// 07
void lighten(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
             const double up_r, double up_g, double up_b, double up_a,
             const double up_opacity, const bool do_clamp = true);
// 08
void screen(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
            const double up_r, double up_g, double up_b, double up_a,
            const double up_opacity, const bool do_clamp = true);
// 09
void color_dodge(/* 覆い焼きカラー */
                 double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                 const double up_r, double up_g, double up_b, double up_a,
                 const double up_opacity, const bool do_clamp = true);
// 10
void linear_dodge(/* 覆い焼きリニア(単純加算ではない) */
                  double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                  const double up_r, double up_g, double up_b, double up_a,
                  const double up_opacity, const bool do_clamp = true);
// 11
void lighter_color(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                   const double up_r, double up_g, double up_b, double up_a,
                   const double up_opacity, const bool do_clamp = true);
// 12
void overlay(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
             const double up_r, double up_g, double up_b, double up_a,
             const double up_opacity, const bool do_clamp = true);
// 13
void soft_light(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                const double up_r, double up_g, double up_b, double up_a,
                const double up_opacity, const bool do_clamp = true);
// 14
void hard_light(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                const double up_r, double up_g, double up_b, double up_a,
                const double up_opacity, const bool do_clamp = true);
// 15
void vivid_light(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                 const double up_r, double up_g, double up_b, double up_a,
                 const double up_opacity, const bool do_clamp = true);
// 16
void linear_light(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                  const double up_r, double up_g, double up_b, double up_a,
                  const double up_opacity, const bool do_clamp = true);
// 17
void pin_light(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
               const double up_r, double up_g, double up_b, double up_a,
               const double up_opacity, const bool do_clamp = true);
// 18
void hard_mix(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
              const double up_r, double up_g, double up_b, double up_a,
              const double up_opacity, const bool do_clamp = true);
//-----------------
// 19
void cross_dissolve(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                    const double up_r, double up_g, double up_b, double up_a,
                    const double up_opacity, const bool do_clamp = true);
// 20
void subtract(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
              const double up_r, double up_g, double up_b, double up_a,
              const double up_opacity, const bool alpha_rendering_sw,
              const bool do_clamp = true);
// 21
void add(/* 単純加算 */
         double &dn_r, double &dn_g, double &dn_b, double &dn_a,
         const double up_r, double up_g, double up_b, double up_a,
         const double up_opacity, const bool do_clamp = true);
// 22
void divide(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
            const double up_r, double up_g, double up_b, double up_a,
            const double up_opacity, const bool do_clamp = true);
}  // namespace color
}  // namespace igs
#endif /* !igs_color_blend_h */
