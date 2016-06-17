#include <cmath>
#include "igs_color_blend.h"
/* --------------------
2010-10-4
                        Toonz6.1sp1 FXino(This library) ------+
        PDF Blend Modes: Addendum January 23, 2006		|
toonz6.1sp1 FX layer blending --+		|		|
photoshop調整レイヤ合成モード	|		|		|
                |		|		|		|
                V		V		V		V
------ Normal Mode (Basic) ------
通常		Normal		(=)Over(?)	Normal	     01 over
ディザ合成	Dissolve	-		-		-
------ Darken Mode (Darken) ------
比較(暗)	Darken		(!=)Darken	Darken	     02 darken
乗算		Multiply	(!=)Multiply	Multiply     03 multiply
焼き込みカラー	Color Burn	(!=)Color Burn	ColorBurn    04 color_burn
焼き込み(リニア)Linear Burn	-		-	     05 linear_burn
カラー比較(暗)	Darker Color	-		-	     06 darker_color
------ Lighten Mode (Lighten) ------
比較(明)	Lighten		(=)Lighten	Lighten	     07 lighten
スクリーン	Screen		(=)Screen	Screen	     08 screen
覆い焼きカラー	Color Dodge	(!=)Color Dodge	ColorDodge   09 color_dodge
覆い焼き(リニア)-加算	Linear Dodge(Add) -		-    10 linear_dodge
カラー比較(明)	Lighter Color	-		-	     11 lighten_color
------ Light Mode (Contrast) ------
オーバーレイ	Overlay		-		Overlay	     12 overlay
ソフトライト	Soft Light	-		SoftLight    13 soft_light
ハードライト	Hard Light	-		HardLight    14 hard_light
ビビッドライト	Vivid Light	-		-	     15 vivid_light
リニアライト	Linear Light	-		-	     16 linear_light
ピンライト	Pin Light	-		-	     17 pin_light
ハードミックス	Hard Mix	-		-	     18 hard_mix
------ Invert Mode (Comparative) ------
差の絶対値	Difference	-		Difference
除外		Exclusion	-		Exclusion
------ Color Mode (HSL) ------
色相		Hue		-		-
彩度		Saturation	-		-
カラー		Color		-		-
輝度		Luminosity	-		-
                                Add			     19 add
                                Cross Dissolve		     20 cross_dissolve
                                Local Transparency
                                Premultiply
                                Substract		     21 subtract
                                Transparency
                                                             22 divide
------
25		25		13		12
-------------------- */
namespace {
//--------------------------------------------------------------------
double clamp_min1_ch_(const double val) { return ((1.0 < val) ? 1.0 : val); }
double clamp_ch_(const double val) {
  return (val < 0) ? 0 : ((1.0 < val) ? 1.0 : val);
}
void clamp_rgba_(double &red, double &gre, double &blu, double &alp) {
  red = clamp_ch_(red);
  gre = clamp_ch_(gre);
  blu = clamp_ch_(blu);
  alp = clamp_ch_(alp);
}
void dn_set_up_opacity_(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                        const double up_r, double up_g, double up_b,
                        double up_a, const double up_opacity) {
  dn_r = up_r * up_opacity;
  dn_g = up_g * up_opacity;
  dn_b = up_b * up_opacity;
  dn_a = up_a * up_opacity;
}
//--------------------------------------------------------------------
double up_add_dn_ch_(const double dn, const double up, const double up_a,
                     const double up_opacity) {
  /*
参考  半透明同士の重ね塗り計算式  (up:前面, dn:背面)
Alpha (0...1.0)
  a = (up + dn) - up * dn = up + dn - up * dn = up + dn * (1 - up)
Color(?) (0...1.0)
  c = (c1 * up + c2 * dn * (1.0 - up)) / a
*/
  /* up値に、down値のupマスク透過分を加える */
  return up * up_opacity + dn * (1.0 - up_a * up_opacity);
  /* upはMultiply値、dnはUntiMultiply値 */
}
double dn_add_up_ch_(const double dn, const double dn_a, const double up,
                     const double up_opacity) {
  /* down値に、up値のdownマスク透過分を加える */
  return dn + up * up_opacity * (1.0 - dn_a);
}

double darken_ch_(const double dn, const double dn_a, const double up,
                  const double up_a, const double up_opacity) {
  return (up / up_a < dn / dn_a) ? up_add_dn_ch_(dn, up, up_a, up_opacity)
                                 : dn_add_up_ch_(dn, dn_a, up, up_opacity);
}
double blend_transp_(const double bl, const double dn, const double dn_a,
                     const double up, const double up_a,
                     const double up_opacity) {
  double bl2 = bl * ((dn_a < up_a) ? dn_a / up_a : up_a / dn_a);  // blend color
  bl2 += (up_a < dn_a) ? (dn / dn_a * (dn_a - up_a) / dn_a) : 0.0;  // dn color
  bl2 += (dn_a < up_a) ? (up / up_a * (up_a - dn_a) / up_a) : 0.0;  // up color
  bl2 *= up_a + dn_a * (1.0 - up_a);                                // Multiply
                                                                    /*
// 2013-01-24
// up_opacityをdn_aでマスクすることで、クリッピングマスクとなる?
up_opacity *= dn_a;
*/
  return dn * (1.0 - up_opacity) + bl2 * up_opacity;  // up opacity
}
double multiply_(const double dn, const double up) { return dn * up; }
double multiply_ch_(const double dn, const double dn_a, const double up,
                    const double up_a, const double up_opacity) {
  return blend_transp_(multiply_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double divide_(const double dn, const double up) {
  return (up <= 0) ? 1.0 : dn / up;
}
double divide_ch_(const double dn, const double dn_a, const double up,
                  const double up_a, const double up_opacity) {
  return blend_transp_(divide_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double color_burn_(const double dn, const double up) {
  return (up <= 0) ? 0 : (1.0 - clamp_min1_ch_((1.0 - dn) / up));
}
double color_burn_ch_(const double dn, const double dn_a, const double up,
                      const double up_a, const double up_opacity) {
  return blend_transp_(color_burn_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double linear_burn_(const double dn, const double up) {
  // return clamp_min1_ch_(dn + up - 1.0);
  return clamp_ch_(dn + up - 1.0);
}
double linear_burn_ch_(const double dn, const double dn_a, const double up,
                       const double up_a, const double up_opacity) {
  return blend_transp_(linear_burn_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double darker_color_ch_(const double dn, const double dn_a, const double up,
                        const double up_a, const double up_opacity,
                        const bool up_lt_sw) {
  return (!up_lt_sw) ? up_add_dn_ch_(dn, up, up_a, up_opacity)
                     : dn_add_up_ch_(dn, dn_a, up, up_opacity);
}

double lighten_ch_(const double dn, const double dn_a, const double up,
                   const double up_a, const double up_opacity) {
  return (up / up_a > dn / dn_a) ? up_add_dn_ch_(dn, up, up_a, up_opacity)
                                 : dn_add_up_ch_(dn, dn_a, up, up_opacity);
}
double screen_(const double dn, const double up) {
  return 1.0 - (1.0 - dn) * (1.0 - up);
}
double color_dodge_(const double dn, const double up) {
  return (1.0 <= up) ? 1.0 : clamp_min1_ch_(dn / (1.0 - up));
}
double color_dodge_ch_(const double dn, const double dn_a, const double up,
                       const double up_a, const double up_opacity) {
  return blend_transp_(color_dodge_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
/*** double linear_dodge_ch_(
        const double dn,const double dn_a
        ,const double up,const double up_a,const double up_opacity
 ) {
        return up_add_dn_ch_(
                //dn + up * dn_a * up_a * up_opacity, up,up, up_opacity
                dn + up * dn_a * up_a * up_opacity * 0.75, up,up, up_opacity
        );
 }***/
double linear_dodge_(const double dn, const double up) {
  return clamp_min1_ch_(dn + up);
}
double linear_dodge_ch_(const double dn, const double dn_a, const double up,
                        const double up_a, const double up_opacity) {
  return blend_transp_(linear_dodge_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double lighter_color_ch_(const double dn, const double dn_a, const double up,
                         const double up_a, const double up_opacity,
                         const bool up_lt_sw) {
  return (up_lt_sw) ? up_add_dn_ch_(dn, up, up_a, up_opacity)
                    : dn_add_up_ch_(dn, dn_a, up, up_opacity);
}

double overlay_ch_(const double dn, const double dn_a, const double up,
                   const double up_a, const double up_opacity) {
  return blend_transp_(
      ((dn / dn_a < 0.5) ? multiply_(up / up_a, 2.0 * dn / dn_a)
                         : screen_(up / up_a, 2.0 * dn / dn_a - 1.0)),
      dn, dn_a, up, up_a, up_opacity);
}
double soft_light_(const double dn, const double up) {
  return (
      (up < 0.5)
          ? (dn + (dn - dn * dn) * (2 * up - 1))
          : ((dn < 0.25)
                 ? (dn + (2 * up - 1) * (((16 * dn - 12) * dn + 4) * dn - dn))
                 : (dn + (2 * up - 1) * (sqrt(dn) - dn))));
}
double soft_light_ch_(const double dn, const double dn_a, const double up,
                      const double up_a, const double up_opacity) {
  return blend_transp_(soft_light_(dn / dn_a, up / up_a)  // UntiMultiply
                       ,
                       dn, dn_a, up, up_a, up_opacity);
}
double hard_light_ch_(const double dn, const double dn_a, const double up,
                      const double up_a, const double up_opacity) {
  return blend_transp_(
      ((up / up_a < 0.5) ? multiply_(dn / dn_a, 2.0 * up / up_a)
                         : screen_(dn / dn_a, 2.0 * up / up_a - 1.0)),
      dn, dn_a, up, up_a, up_opacity);
}

double vivid_light_ch_(const double dn, const double dn_a, const double up,
                       const double up_a, const double up_opacity) {
  return blend_transp_(
      ((up / up_a < 0.5) ? color_burn_(dn / dn_a, 2.0 * up / up_a)
                         : color_dodge_(dn / dn_a, 2.0 * up / up_a - 1.0)),
      dn, dn_a, up, up_a, up_opacity);
}
double linear_light_ch_(const double dn, const double dn_a, const double up,
                        const double up_a, const double up_opacity) {
  return blend_transp_(
      ((up / up_a < 0.5) ? linear_burn_(dn / dn_a, 2.0 * up / up_a)
                         : linear_dodge_(dn / dn_a, 2.0 * up / up_a - 1.0)),
      dn, dn_a, up, up_a, up_opacity);
}
double pin_light_ch_(const double dn, const double dn_a, const double up,
                     const double up_a, const double up_opacity) {
  return blend_transp_(
      ((up / up_a < 0.5)
           ? (((2.0 * up / up_a) < dn / dn_a) ? (2.0 * up / up_a)
                                              : dn / dn_a)  // darken
           : (((2.0 * up / up_a - 1.0) > dn / dn_a) ? (2.0 * up / up_a - 1.0)
                                                    : dn / dn_a)  // lighten
       ),
      dn, dn_a, up, up_a, up_opacity);
}
double hard_mix_ch_(const double dn, const double dn_a, const double up,
                    const double up_a, const double up_opacity) {
  return blend_transp_(
      ((((up / up_a < 0.5)
             ? color_burn_(dn / dn_a, 2.0 * up / up_a)
             : color_dodge_(dn / dn_a, 2.0 * up / up_a - 1.0)) < 0.5)
           ? 0
           : 1.0),
      dn, dn_a, up, up_a, up_opacity);
}
bool up_is_lighter_(const double dn_r, const double dn_g, const double dn_b,
                    const double up_r, const double up_g, const double up_b) {
  /* Photoshop CS4 helpより、"合計の比較"とあるのでそのとおりにする... */
  //	return	(dn_r + dn_g + dn_b) < (up_r + up_g + up_b);
  /* しかし、Photoshop CS4の実際を見ると"合計の比較"はうそであった...
NTSC係数による加重平均法(NTSC Coefficients method )、これは、
R,G,Bそれぞれの値に、重み付けをして3で割り、
平均を取りグレースケール化する方法で、
輝度計算をするとぴったり合うのでこちらにする(2010-09-03) */
  return (0.298912 * dn_r + 0.586611 * dn_g + 0.114478 * dn_b) <
         (0.298912 * up_r + 0.586611 * up_g + 0.114478 * up_b);
}
}
//--------------------------------------------------------------------
// 01
void igs::color::over(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                      const double up_r, double up_g, double up_b, double up_a,
                      const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = up_add_dn_ch_(dn_r, up_r, up_a, up_opacity);
  dn_g = up_add_dn_ch_(dn_g, up_g, up_a, up_opacity);
  dn_b = up_add_dn_ch_(dn_b, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 02
void igs::color::darken(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                        const double up_r, double up_g, double up_b,
                        double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = darken_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = darken_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = darken_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 03
void igs::color::multiply(double &dn_r, double &dn_g, double &dn_b,
                          double &dn_a, const double up_r, double up_g,
                          double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  /*
  // 2013-01-24
  // up_opacityをdn_aでマスクすることで、クリッピングマスクとなる?
  up_opacity *= dn_a;
*/
  dn_r = multiply_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = multiply_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = multiply_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 04
void igs::color::color_burn(/* 焼き込みカラー */
                            double &dn_r, double &dn_g, double &dn_b,
                            double &dn_a, const double up_r, double up_g,
                            double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = color_burn_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = color_burn_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = color_burn_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 05
void igs::color::linear_burn(/* 焼き込みリニア */
                             double &dn_r, double &dn_g, double &dn_b,
                             double &dn_a, const double up_r, double up_g,
                             double up_b, double up_a,
                             const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = linear_burn_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = linear_burn_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = linear_burn_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 06
void igs::color::darker_color(double &dn_r, double &dn_g, double &dn_b,
                              double &dn_a, const double up_r, double up_g,
                              double up_b, double up_a,
                              const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  bool up_lt_sw = up_is_lighter_(dn_r / dn_a, dn_g / dn_a, dn_b / dn_a,
                                 up_r / up_a, up_g / up_a, up_b / up_a);
  dn_r = darker_color_ch_(dn_r, dn_a, up_r, up_a, up_opacity, up_lt_sw);
  dn_g = darker_color_ch_(dn_g, dn_a, up_g, up_a, up_opacity, up_lt_sw);
  dn_b = darker_color_ch_(dn_b, dn_a, up_b, up_a, up_opacity, up_lt_sw);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 07
void igs::color::lighten(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                         const double up_r, double up_g, double up_b,
                         double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = lighten_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = lighten_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = lighten_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 08
void igs::color::screen(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                        const double up_r, double up_g, double up_b,
                        double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = screen_(dn_r, up_r * up_opacity);
  dn_g = screen_(dn_g, up_g * up_opacity);
  dn_b = screen_(dn_b, up_b * up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 09
void igs::color::color_dodge(/* 覆い焼きカラー */
                             double &dn_r, double &dn_g, double &dn_b,
                             double &dn_a, const double up_r, double up_g,
                             double up_b, double up_a,
                             const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = color_dodge_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = color_dodge_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = color_dodge_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 10
void igs::color::linear_dodge(/* 覆い焼きリニア(単純加算ではない) */
                              double &dn_r, double &dn_g, double &dn_b,
                              double &dn_a, const double up_r, double up_g,
                              double up_b, double up_a,
                              const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = linear_dodge_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = linear_dodge_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = linear_dodge_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 11
void igs::color::lighter_color(double &dn_r, double &dn_g, double &dn_b,
                               double &dn_a, const double up_r, double up_g,
                               double up_b, double up_a,
                               const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  bool up_lt_sw = up_is_lighter_(dn_r / dn_a, dn_g / dn_a, dn_b / dn_a,
                                 up_r / up_a, up_g / up_a, up_b / up_a);
  dn_r = lighter_color_ch_(dn_r, dn_a, up_r, up_a, up_opacity, up_lt_sw);
  dn_g = lighter_color_ch_(dn_g, dn_a, up_g, up_a, up_opacity, up_lt_sw);
  dn_b = lighter_color_ch_(dn_b, dn_a, up_b, up_a, up_opacity, up_lt_sw);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 12
void igs::color::overlay(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                         const double up_r, double up_g, double up_b,
                         double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = overlay_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = overlay_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = overlay_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 13
void igs::color::soft_light(double &dn_r, double &dn_g, double &dn_b,
                            double &dn_a, const double up_r, double up_g,
                            double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = soft_light_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = soft_light_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = soft_light_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 14
void igs::color::hard_light(double &dn_r, double &dn_g, double &dn_b,
                            double &dn_a, const double up_r, double up_g,
                            double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = hard_light_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = hard_light_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = hard_light_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 15
void igs::color::vivid_light(double &dn_r, double &dn_g, double &dn_b,
                             double &dn_a, const double up_r, double up_g,
                             double up_b, double up_a,
                             const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = vivid_light_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = vivid_light_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = vivid_light_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 16
void igs::color::linear_light(double &dn_r, double &dn_g, double &dn_b,
                              double &dn_a, const double up_r, double up_g,
                              double up_b, double up_a,
                              const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = linear_light_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = linear_light_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = linear_light_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 17
void igs::color::pin_light(double &dn_r, double &dn_g, double &dn_b,
                           double &dn_a, const double up_r, double up_g,
                           double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = pin_light_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = pin_light_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = pin_light_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 18
void igs::color::hard_mix(double &dn_r, double &dn_g, double &dn_b,
                          double &dn_a, const double up_r, double up_g,
                          double up_b, double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = hard_mix_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = hard_mix_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = hard_mix_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
//------------------------------------------------------------------------
namespace {
double cross_dissolve_ch_(const double dn, const double up,
                          const double up_opacity) {
  return dn * (1.0 - up_opacity) + up * up_opacity;
}
}
// 19
void igs::color::cross_dissolve(double &dn_r, double &dn_g, double &dn_b,
                                double &dn_a, const double up_r, double up_g,
                                double up_b, double up_a,
                                const double up_opacity) {
  if ((up_a <= 0) && (dn_a <= 0)) {
    return;
  } /* up/dnとも透明ならdn値を表示 */

  /* upとdown、半透明or不透明 */
  dn_r = cross_dissolve_ch_(dn_r, up_r, up_opacity);
  dn_g = cross_dissolve_ch_(dn_g, up_g, up_opacity);
  dn_b = cross_dissolve_ch_(dn_b, up_b, up_opacity);
  dn_a = cross_dissolve_ch_(dn_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 20
void igs::color::subtract(double &dn_r, double &dn_g, double &dn_b,
                          double &dn_a, const double up_r, double up_g,
                          double up_b, double up_a, const double up_opacity,
                          const bool alpha_rendering_sw) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  /* upとdown、半透明or不透明 */
  dn_r -= up_r * up_opacity;
  dn_g -= up_g * up_opacity;
  dn_b -= up_b * up_opacity;
  if (alpha_rendering_sw) {
    dn_a -= up_a * up_opacity;
  }

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 21
void igs::color::add(/* 覆い焼きリニア */
                     double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                     const double up_r, double up_g, double up_b, double up_a,
                     const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  /* 単純加算 */
  dn_r += up_r * up_opacity;
  dn_g += up_g * up_opacity;
  dn_b += up_b * up_opacity;
  dn_a += up_a * up_opacity;

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
// 22
void igs::color::divide(double &dn_r, double &dn_g, double &dn_b, double &dn_a,
                        const double up_r, double up_g, double up_b,
                        double up_a, const double up_opacity) {
  if (up_a <= 0) {
    return;
  }                /* upが透明のときはdown値を表示 */
  if (dn_a <= 0) { /* downが透明のときはup値を表示 */
    dn_set_up_opacity_(dn_r, dn_g, dn_b, dn_a, up_r, up_g, up_b, up_a,
                       up_opacity);
    return;
  }

  dn_r = divide_ch_(dn_r, dn_a, up_r, up_a, up_opacity);
  dn_g = divide_ch_(dn_g, dn_a, up_g, up_a, up_opacity);
  dn_b = divide_ch_(dn_b, dn_a, up_b, up_a, up_opacity);
  dn_a = up_add_dn_ch_(dn_a, up_a, up_a, up_opacity);

  clamp_rgba_(dn_r, dn_g, dn_b, dn_a); /* 0と1で範囲制限 */
}
