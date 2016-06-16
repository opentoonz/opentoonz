#include <vector>
#include <stdexcept>        /* std::domain_error(-) */
#include <limits>           /* std::numeric_limits */
#include <cmath>            /* pow(-),abs(-) */
#include <cstring>          /* memmove */
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_motion_blur.h"

// #include <numeric> /* std::accumulate(-) */
#if 0   //------comment out-----------------------------------------------
namespace {
 void dda_array_(
	const int dx,	/* = abs(x_vector = x2 - x1) */
	const int dy, /* = abs(y_vector = y2 - y1) = array_size - 1 */
	std::vector<int>&y_array	/* array_size(0...dy)個ある */
 ) {
	/* ここではy方向に傾いている場合(dx <= dy)のみ扱う
	    y +      *
	  (dy)|     *
	      |    *
	      |   *
	  OUT +   *
	      |  *
	      | *
	      |*
	    0 +---+---+---+
	      0           x(dx)
		   IN
	*/
	if (dy < dx) { return; } /* x方向に傾いていると処理できない */
	if (dy <= 0) { return; } /* y長さがないと処理しない */

	y_array.at(0) = 0; /* 始めは原点なのでインクリメントはゼロ */

	/* 線分ループ */
	const int incr1 = 2 * dx;
	const int incr2 = 2 * (dx - dy);
	int dd = (2 * dx) - dy;
	for (int ii = 1; ii <= dy; ++ii) {
		if (dd < 0) { dd += incr1; y_array.at(ii) = 0; }
		else        { dd += incr2; y_array.at(ii) = 1; }
	}
 }
}
namespace {
 void set_position_(
	const int x_vector,
	const int y_vector,/* 絶対値の大きいほうが"size-1" */
	std::vector<int>&x_array,
	std::vector<int>&y_array /* 位置格納配列size個ある */
 ) {
	int dx = abs(x_vector);/* 線分の幅、ゼロ以上の大きさの数 */
	int dy = abs(y_vector);/* 線分の高さ、ゼロ以上の大きさの数 */
	int count = 0;

	/* x方向に傾いている場合(dy < dx)
	     y +        *
	   (dy)|       *
	       |      *
	       |     *
	   OUT +   *
	       |  *
	       | *
	       |*
	     0 +---+---+---+
	       0           x(dx)
                    IN
	*/
	if (dy < dx) {
	 for (int ii=0;ii<=dx;++ii) {x_array.at(ii) = ii;}
	 dda_array_( dy, dx, y_array );
	 for (int ii=1;ii<=dx;++ii) {y_array.at(ii)+=y_array.at(ii-1);}
	 count = dx;
	}

	/* y方向に傾いている場合(dx <= dy)
	    y +      *
	  (dy)|     *
	      |    *
	      |   *
	  OUT +   *
	      |  *
	      | *
	      |*
	    0 +---+---+---+
	      0           x(dx)
                   IN
	*/
	else {
	 for (int ii=0;ii<=dy;++ii) {y_array.at(ii) = ii;}
	 dda_array_( dx, dy, x_array );
	 for (int ii=1;ii<=dy;++ii) {x_array.at(ii)+=x_array.at(ii-1);}
	 count = dy;
	}

	/* マイナス方向の場合は反転 */
	if (x_vector < 0) {
	 for (int ii=1;ii<=count;++ii){x_array.at(ii)=-x_array.at(ii);}
	}
	if (y_vector < 0) {
	 for (int ii=1;ii<=count;++ii){y_array.at(ii)=-y_array.at(ii);}
	}
 }
 void set_ratio_(
	const double curve,
	std::vector<double>&ratio_array
 ) {
	const int size = ratio_array.size();
	/* リニア配置をする */
	for (int ii = 0; ii < size; ++ii) {
		ratio_array.at(ii) = ((double)(size-ii))/(double)(size);
	}
	/* カーブを設定(ガンマ計算) */
	if (curve <= 0.0) {
	 for (int ii = 1; ii < size; ++ii) { ratio_array.at(ii) = 0.0; }
	} else
	if (1.0 != curve) {
	 for (int ii = 1; ii < size; ++ii) {
		ratio_array.at(ii)=pow(ratio_array.at(ii),1.0/curve);
	 }
	}
	/* 正規化 */
	double accum = 0;
	for (int ii = 0; ii < size; ++ii) { accum+=ratio_array.at(ii);}
	for (int ii = 0; ii < size; ++ii) { ratio_array.at(ii)/=accum;}
 }
}
namespace {
 void set_jaggy_(
	const double x_vector,
	const double y_vector,
	const double vector_scale,
	const double curve,
	std::vector<double>&ratio_array,
	std::vector<int>&x_array,
	std::vector<int>&y_array
 ) {
	if (curve <= 0.0) { return; } /* blur効果なしならなにもしない */

	int xv = static_cast<int>(x_vector * vector_scale + 0.5);
	int yv = static_cast<int>(y_vector * vector_scale + 0.5);

	int size;
	if (abs(xv) <= abs(yv))	{ size = abs(yv) + 1; }
	else			{ size = abs(xv) + 1; }

	ratio_array.resize(size);
	x_array.resize(size);
	y_array.resize(size);

	set_ratio_( curve, ratio_array );
	set_position_( xv, yv, x_array,y_array );
 }
}
#endif  //------comment out----------------------------------------------

namespace {
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
void vec_poi_to_len_pos_(
    const double xv, const double yv, /* vector */
    const double xp, const double yp, /* point */
    double &len,                      /* vectorからpointまでの距離 */
    double &pos /* vector方向でvector原点からpointまでの距離 */
    ) {
  /*
   * ベクトルの角度を求める
   */

  double radian = 0.0;
  /* ゼロエラー */
  if ((0.0 == xv) && (0.0 == yv)) {
    len = 0.0;
    pos = 0.0;
    return;
  }
  /* 第1象限(0 <= degree < 90)*/
  else if ((0.0 < xv) && (0.0 <= yv)) {
    radian = atan(yv / xv);
  }
  /* 第2象限(90 <= degree < 180) */
  else if ((xv <= 0.0) && (0.0 < yv)) {
    radian = atan(-xv / yv) + M_PI / 2.0;
  }
  /* 第3象限(180 <= degree < 270) */
  else if ((xv < 0.0) && (yv <= 0.0)) {
    radian = atan(yv / xv) + M_PI;
  }
  /* 第4象限(270 <= degree < 360(=0)) */
  else if ((0.0 <= xv) && (yv < 0.0)) {
    radian = atan(xv / -yv) + M_PI + M_PI / 2.0;
  }

  /*
   * 逆回転し、ベクトルを+X軸上に置く
   */

  /* ベクトルを逆回転し、x軸プラス上に置く */
  double xv_rot = xv * cos(-radian) - yv * sin(-radian);
  // double yv_rot = xv * sin(-radian) + yv * cos(-radian);
  /* ターゲット点も同様に逆回転する */
  double xp_rot = xp * cos(-radian) - yp * sin(-radian);
  double yp_rot = xp * sin(-radian) + yp * cos(-radian);

  /*
   * vectorからpointまでの距離
   */

  /* マイナス(原点より小さい)の場合、ベクトル始点からの距離 */
  if (xp_rot < 0.0) {
    len = sqrt((xp * xp) + (yp * yp));
  }
  /* ベクトルより大きい場合、ベクトル終点からの距離 */
  else if (xv_rot < xp_rot) {
    len = sqrt((xp - xv) * (xp - xv) + (yp - yv) * (yp - yv));
  }
  /* ベクトルの横範囲内なら、上下位置が距離となる */
  else {
    len = fabs(yp_rot);
  }

  /*
   * vector方向でvector原点からpointまでの距離
   */

  pos = xp_rot;
}
}

namespace {
/* ベクトルからの指定範囲でサブピクセルがどのくらい含むかカウントする
        ピクセルの位置は以下の図参照
          y
          ^
          |
          |
          |
          |
        +---+
        | | |
        | +-|-------------------> x
        |   |
        +---+
 */
int count_nearly_vector_(
    const double xv, const double yv, /* vector */
    const double x_tgt,
    const double y_tgt, /* 調査(vectorの原点からの)pixel位置 */
    const long x_div, const long y_div, /* 調査pixelを分割する(サブpixel)数 */
    const double valid_len              /* vectorからの有効距離 */
    ) {
  int count = 0;
  for (int yy = 0; yy < y_div; ++yy) {
    for (int xx = 0; xx < x_div; ++xx) {
      double xp  = x_tgt + (double)xx / x_div - (0.5 - 0.5 / x_div);
      double yp  = y_tgt + (double)yy / y_div - (0.5 - 0.5 / y_div);
      double len = 0.0;
      double pos = 0.0;
      vec_poi_to_len_pos_(xv, yv, xp, yp, len, pos);
      if (len < valid_len) {
        ++count;
      }
    }
  }
  return count;
}
/* リニア減衰計算 */
double liner_decrement_(
    const double xv, const double yv, /* vector */
    const double x_tgt,
    const double y_tgt, /* 調査(vectorの原点からの)pixel位置 */
    const long x_div, const long y_div, /* 調査pixelを分割する(サブpixel)数 */
    const double valid_len              /* vectorからの有効距離 */
    ) {
  int count             = 0;
  double accum          = 0.0;
  const double line_len = sqrt(xv * xv + yv * yv) + valid_len;
  for (int yy = 0; yy < y_div; ++yy) {
    for (int xx = 0; xx < x_div; ++xx) {
      double xp  = x_tgt + (double)xx / x_div - (0.5 - 0.5 / x_div);
      double yp  = y_tgt + (double)yy / y_div - (0.5 - 0.5 / y_div);
      double len = 0.0;
      double pos = 0.0;
      vec_poi_to_len_pos_(xv, yv, xp, yp, len, pos);
      if (len < valid_len) {
        ++count;
        /* 下の式ではマイナスにはならない */
        accum += 1.0 - fabs(pos) / line_len;
      }
    }
  }
  if (count <= 0) {
    return 0.0;
  } /* 念のためチェック */
  return accum / (double)count;
}
/* ぶれ画像計算 */
double bure_decrement_(
    const double xv, const double yv, /* vector */
    const double x_tgt,
    const double y_tgt, /* 調査(vectorの原点からの)pixel位置 */
    const long x_div, const long y_div, /* 調査pixelを分割する(サブpixel)数 */
    const double valid_len,             /* vectorからの有効距離 */
    const int zanzo_length) {
  long count_in  = 0;
  long count_out = 0;
  double pos     = 0.0;
  for (int yy = 0; yy < y_div; ++yy) {
    for (int xx = 0; xx < x_div; ++xx) {
      double xp  = x_tgt + (double)xx / x_div - (0.5 - 0.5 / x_div);
      double yp  = y_tgt + (double)yy / y_div - (0.5 - 0.5 / y_div);
      double len = 0.0;
      pos        = 0.0;
      vec_poi_to_len_pos_(xv, yv, xp, yp, len, pos);
      if (len < valid_len) {
        if (0 == ((int)pos % zanzo_length)) {
          ++count_in;
        } else {
          ++count_out;
        }
      }
    }
  }

  /* 有効距離内にない場合ゼロを返す。念のためチェック */
  if ((count_in + count_out) <= 0) {
    return 0.0;
  }

  /* 距離で計算するのでマイナス位置では意味がない */
  if (pos < 0.0) {
    pos = -pos;
  }

  /*
          pos が line_len より大きい場合がある。
          そのため、pos / line_lenが、1より大きくなり、
          それゆえ、1.0 - pos / line_lenが、マイナスとなる。
          マイナスを返してはいけないのでそのための処理
  */
  const double line_len = sqrt(xv * xv + yv * yv) + valid_len;
  if (line_len < pos) {
    pos = line_len;
  }

  /* ????????????????? */
  return (double)count_in / (count_in + count_out) * (1.0 - pos / line_len);
}
}

namespace {
void set_smooth_(const double x_vector, const double y_vector,
                 const double vector_scale, const double curve,
                 const int zanzo_length, const double zanzo_power,
                 std::vector<double> &ratio_array, std::vector<int> &x_array,
                 std::vector<int> &y_array) {
  /* blur効果なしならなにもしない */
  if (curve <= 0.0) {
    return;
  }

  /* ベクトルの実の長さ */
  double xv = x_vector * vector_scale;
  double yv = y_vector * vector_scale;

  /* vectorの長さがゼロならなにもしない */
  if ((0.0 == xv) && (0.0 == yv)) {
    return;
  }

  /* vectorの始点と終点のpixel位置 */
  int x1, y1, x2, y2;
  if (0.0 <= xv) {
    x1 = 0;
    x2 = (int)xv + 2;
  } else {
    x1 = (int)xv - 2;
    x2 = 0;
  }
  if (0.0 <= yv) {
    y1 = 0;
    y2 = (int)yv + 2;
  } else {
    y1 = (int)yv - 2;
    y2 = 0;
  }

  /* smooth線にかかるsubpixel数をカウント */
  int size = 0;
  for (int yy = y1; yy <= y2; ++yy) {
    for (int xx = x1; xx <= x2; ++xx) {
      if (0 <
          count_nearly_vector_(xv, yv, (double)xx, (double)yy, 16, 16, 0.5)) {
        ++size;
      }
    }
  }

  /* カウント0以下では、blur効果がない */
  if (size <= 0) {
    return;
  }

  /* バッファメモリ確保 */
  ratio_array.resize(size);
  x_array.resize(size);
  y_array.resize(size);

  /* 位置とピクセル値(0<val<=1)を格納 */
  int ii = 0;
  for (int yy = y1; yy <= y2; ++yy) {
    for (int xx = x1; xx <= x2; ++xx) {
      const int count =
          count_nearly_vector_(xv, yv, (double)xx, (double)yy, 16, 16, 0.5);
      if (0 < count) {
        ratio_array.at(ii) = (double)count / (16 * 16);
        x_array.at(ii)     = xx;
        y_array.at(ii)     = yy;
        ++ii;
      }
    }
  }

  /* リニア減衰効果 */
  for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
    ratio_array.at(ii) *=
        liner_decrement_(xv, yv, (double)(x_array.at(ii)),
                         (double)(y_array.at(ii)), 16, 16, 0.5);
  }

  /* 段々の分割指定があれば設定する */
  if (0 < zanzo_length) {
    /* ぶれ残像減衰効果 */
    for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
      ratio_array.at(ii) =
          (1.0 - zanzo_power) * ratio_array.at(ii) +
          zanzo_power * bure_decrement_(xv, yv, (double)(x_array.at(ii)),
                                        (double)(y_array.at(ii)), 16, 16, 0.5,
                                        zanzo_length);
    }
  }

  /* vector始点は1とする */
  for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
    if ((0 == x_array.at(ii)) && (0 == y_array.at(ii))) {
      ratio_array.at(ii) = 1.0;
      break;
    }
  }

  /* ガンマ強弱計算 */
  if (1.0 != curve) { /* 0.0以下の値のとき既にreturnしてる */
    for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
      ratio_array.at(ii) = pow(ratio_array.at(ii), 1.0 / curve);
    }
  }
  /* 正規化 */
  double d_accum = 0.0;
  for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
    d_accum += ratio_array.at(ii);
  }
  for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
    ratio_array.at(ii) /= d_accum;
  }
}
}

namespace {
template <class T>
T pixel_value(const T *image_array, const int height, const int width,
              const int channels, const int xx, const int yy, const int zz,
              const std::vector<double> &ratio_array,
              const std::vector<int> &x_array,
              const std::vector<int> &y_array) {
  double ratio_accum = 0.0;
  double accum       = 0.0;
  for (unsigned int ii = 0; ii < ratio_array.size(); ++ii) {
    const int xp = xx + x_array.at(ii);
    const int yp = yy + y_array.at(ii);

    if (xp < 0) {
      continue;
    }
    if (yp < 0) {
      continue;
    }
    if (width <= xp) {
      continue;
    }
    if (height <= yp) {
      continue;
    }

    ratio_accum += ratio_array.at(ii);
    accum += ratio_array.at(ii) *
             static_cast<double>(
                 *(image_array + channels * width * yp + channels * xp + zz));
  }

  /* can not calculate */
  if (0.0 == ratio_accum) {
    return 0;
  }

  /* overflowはありえない(ratio_arrayを正規化してあるため) */
  /*if ((accum/ratio_accum) <= 0.0) { return 0; }
  if ((USHORT)0xffff <= (USHORT)(accum/ratio_accum)) {
          return (unsigned short)0xffff;
  }*/

  return static_cast<T>(accum / ratio_accum + 0.5);
}
template <class T>
void convert_template_(const T *in, T *image_out, const int hh, const int ww,
                       const int cc, const std::vector<double> &ra,
                       const std::vector<int> &xa, const std::vector<int> &ya,
                       const bool alpha_rend_sw) {
  /* 効果なしならimageをcopyして終る */
  if (ra.size() <= 0) {
    memmove(image_out, in, hh * ww * cc * sizeof(T));
    return;
  }

  if (igs::image::rgba::siz == cc) { /* Alphaがある */
    using namespace igs::image::rgba;
    if (alpha_rend_sw) { /* Alphaも処理する */
      const T *p_in = in;
      T *pout       = image_out;
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, p_in += cc, pout += cc) {
          /*Alpha処理-->*/ pout[alp] =
              pixel_value(in, hh, ww, cc, xx, yy, alp, ra, xa, ya);
          if (0 == pout[alp]) { /* AlphaがゼロならRGB処理しない */
            pout[red] = p_in[red];
            pout[gre] = p_in[gre];
            pout[blu] = p_in[blu];
          } else { /* AlphaがゼロでないときRGB処理する */
            pout[red] = pixel_value(in, hh, ww, cc, xx, yy, red, ra, xa, ya);
            pout[gre] = pixel_value(in, hh, ww, cc, xx, yy, gre, ra, xa, ya);
            pout[blu] = pixel_value(in, hh, ww, cc, xx, yy, blu, ra, xa, ya);
          }
        }
      }
    } else { /* Alpha処理せず保存のみ */
      const T *p_in              = in;
      T *pout                    = image_out;
      const unsigned int val_max = std::numeric_limits<T>::max();
      for (int yy = 0; yy < hh; ++yy) {
        for (int xx = 0; xx < ww; ++xx, p_in += cc, pout += cc) {
          /*Alpha保存-->*/ pout[alp] = p_in[alp];
          if (0 == pout[alp]) { /* AlphaがゼロならRGB処理しない */
            pout[red] = p_in[red];
            pout[gre] = p_in[gre];
            pout[blu] = p_in[blu];
          } else { /* AlphaがゼロでないときRGB処理する */
            pout[red] = pixel_value(in, hh, ww, cc, xx, yy, red, ra, xa, ya);
            pout[gre] = pixel_value(in, hh, ww, cc, xx, yy, gre, ra, xa, ya);
            pout[blu] = pixel_value(in, hh, ww, cc, xx, yy, blu, ra, xa, ya);
            /* Alphaが0より大きくMaxより小さいとき増分をMaskする */
            if (p_in[alp] < val_max) {
              const unsigned int aa = static_cast<unsigned int>(p_in[alp]);
              if (p_in[red] < pout[red]) { /* 増分のみMask! */
                const unsigned int dif =
                    static_cast<unsigned int>(pout[red] - p_in[red]);
                pout[red] = static_cast<T>(p_in[red] + dif * aa / val_max);
              }
              if (p_in[gre] < pout[gre]) { /* 増分のみMask! */
                const unsigned int dif =
                    static_cast<unsigned int>(pout[gre] - p_in[gre]);
                pout[gre] = static_cast<T>(p_in[gre] + dif * aa / val_max);
              }
              if (p_in[blu] < pout[blu]) { /* 増分のみMask! */
                const unsigned int dif =
                    static_cast<unsigned int>(pout[blu] - p_in[blu]);
                pout[blu] = static_cast<T>(p_in[blu] + dif * aa / val_max);
              }
            }
          }
        }
      }
    }
  } else { /* Alphaがない, RGB/Grayscale... */
    T *pout = image_out;
    for (int yy = 0; yy < hh; ++yy) {
      for (int xx = 0; xx < ww; ++xx, pout += cc) {
        for (int zz = 0; zz < cc; ++zz) {
          pout[zz] = pixel_value(in, hh, ww, cc, xx, yy, zz, ra, xa, ya);
        }
      }
    }
  }
}
}
void igs::motion_blur::convert(const unsigned char *image_in,
                               unsigned char *image_out,

                               const int height, const int width,
                               const int channels, const int bits,

                               const double x_vector, const double y_vector,
                               const double vector_scale, const double curve,
                               const int zanzo_length, const double zanzo_power,
                               const bool alpha_rend_sw) {
  std::vector<double> ratio_array;
  std::vector<int> x_array;
  std::vector<int> y_array;

  set_smooth_(x_vector, y_vector, vector_scale, curve, zanzo_length,
              zanzo_power, ratio_array, x_array, y_array);
  /***set_jaggy_(
          x_vector, y_vector, vector_scale, curve,
          ratio_array, x_array, y_array
  );***/

  if (std::numeric_limits<unsigned char>::digits == bits) {
    convert_template_(image_in, image_out, height, width, channels, ratio_array,
                      x_array, y_array, alpha_rend_sw);
    y_array.clear();
    x_array.clear();
    ratio_array.clear();
  } else if (std::numeric_limits<unsigned short>::digits == bits) {
    convert_template_(reinterpret_cast<const unsigned short *>(image_in),
                      reinterpret_cast<unsigned short *>(image_out), height,
                      width, channels, ratio_array, x_array, y_array,
                      alpha_rend_sw);
    y_array.clear();
    x_array.clear();
    ratio_array.clear();
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
