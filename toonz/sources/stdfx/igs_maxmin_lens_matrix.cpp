#include <cmath>  // sqrt() sin() cos()
#ifndef M_PI      /* only Microsoft C++ */
#define M_PI 3.14159265358979323846
#endif
#include <iostream>  // std::cout
#include "igs_maxmin_lens_matrix.h"

namespace {
/*
                        y
                        ^
                        |   <-
                        |      \
                        |	|
        <---------------+----------------> x
                        |
                        |
                        |
                        v
 */
bool inside_polygon_(double radius, int odd_diameter, double xp, double yp,
                     int polygon_number, double roll_degree) {
  if (polygon_number < 3) { /* equal less than 2  is circle */
    return true;
  }
  double radian     = roll_degree * (M_PI / 180),
         add_radian = 2.0 * M_PI / polygon_number;
  double x1 = radius * cos(radian), y1 = radius * sin(radian), x2 = 0, y2 = 0,
         xa = odd_diameter, xb = odd_diameter;
  radian += add_radian;

  /* 線分の回数loop回す */
  for (int ii = 0; ii < polygon_number;
       ++ii, radian += add_radian, x1 = x2, y1 = y2) {
    /* (0,0)を原点とした正多角形の各頂点
    数学的開始点(右)及び回転方向(CCW)とするため、
    内部では上下左右反転する。
    これをしないと左からCW回転となる
    計算の考えを単純にするため反転しない2013-12-10
    */
    x2 = radius * cos(radian);
    y2 = radius * sin(radian);

    /* scanline方向でy1とy2の間で区切った領域外なら次へ
    ((y2==y1==yp)の場合も)しない線分なら次へ */
    if (!(((y1 <= yp) && (yp <= y2)) || ((y2 <= yp) && (yp <= y1)))) {
      continue;
    }

    /* scanline方向でy1とy2の間で区切った領域 */

    /* 水平線分 */
    if (y2 == y1) {
      if (((x1 <= xp) && (xp <= x2)) || ((x2 <= xp) && (xp <= x1))) {
        return true;
      } /* 水平線分上 */
      else {
        return false;
      } /* 水平範囲外 */
    }

    /* 水平方向の初めの交差位置(xa) */
    if (xa == odd_diameter) {
      /* (x2 - x1)/(y2 - y1)=(xa - x1)/(yp - y1);
(xa - x1)=(yp - y1)*(x2 - x1)/(y2 - y1); */
      xa = (yp - y1) * (x2 - x1) / (y2 - y1) + x1;
    } else
        /* 2番目の交差位置(xb) */
        if (xb == odd_diameter) {
      xb = (yp - y1) * (x2 - x1) / (y2 - y1) + x1;
      if (((xa <= xp) && (xp <= xb)) || ((xb <= xp) && (xp <= xa))) {
        return true;
      } /* 水平範囲内 */
      else {
        return false;
      } /* 水平範囲外 */
    }
  }
  return false;
}
/*
        二点(x1,y1),(x2,y2)を通る直線
         x - x1    y - y1
        ------- = -------
        x2 - x1   y2 - y1
    -->	(x - x1) * (y2 - y1) = (y - y1) * (x2 - x1)
    -->	x * (y2 - y1) - x1 * (y2 - y1) = y * (x2 - x1) - y1 * (x2 - x1)
    --> y * (x2 - x1) - y1 * (x2 - x1) = x * (y2 - y1) - x1 * (y2 - y1)
    -->	y * (x2 - x1) + x1 * (y2 - y1) = x * (y2 - y1) + y1 * (x2 - x1)
    -->	(y1 - y2) * x + (x2 - x1) * y + x1 * (y2 - y1) - y1 * (x2 - x1) = 0
    --> (y1 - y2) * x + (x2 - x1) * y + x1*y2 - x1*y1 - y1*x2 + y1*x1 = 0
    --> (y1 - y2) * x + (x2 - x1) * y + x1*y2 - y1*x2 = 0

        点(x0,y0)と直線との距離(垂線の長さ)(h)
        a * x + b * y + c = 0 ならば、
                | a * x0 + b * y0 + c |
        h =	-----------------------
                  sqrt(a * a + b * b)
                | (y1 - y2) * x0 + (x2 - x1) * y0 + x1*y2 - y1*x2 |
        h =	---------------------------------------------------
                sqrt((y1 - y2) * (y1 - y2) + (x2 - x1) * (x2 - x1))
 */
double length_to_polygon_(double radius, double xp, double yp,
                          int polygon_number, double roll_degree) {
  /* 現在位置での角度 */
  double radian = atan2(yp, xp); /* 0 ... M_PI , -M_PI ... 0 */
  if (radian < 0) {
    radian += M_PI * 2.0;
  } /* 0 ... M_PI*2 */

  /* 多角形の辺の2点 */
  double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
  const double add_radian = 2.0 * M_PI / polygon_number;
  double radian1          = roll_degree * (M_PI / 180);
  /* rad1だとMS-VC++_v10でエラー!!! */
  while (radian1 < 0.0) {
    radian1 += add_radian;
  }
  double radian2 = radian1 + add_radian;
  /* rad2だとMS-VC++_v10でエラー!!! */

  /* 開始角度より現在位置の角度が小さい場合360先に進める */
  if (radian < radian1) {
    radian += M_PI * 2.0;
  }

  for (int ii = 0; ii < polygon_number;
       ++ii, radian1 = radian2, radian2 += add_radian) {
    if (radian1 <= radian && radian <= radian2) {
      x1 = radius * cos(radian1);
      y1 = radius * sin(radian1);
      x2 = radius * cos(radian2);
      y2 = radius * sin(radian2);
      break;
    }
  }
  if (x1 == 0.0) {
    /* エラー */
    return -1.0;  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  }
  return fabs((y1 - y2) * xp + (x2 - x1) * yp + x1 * y2 - y1 * x2) /
         sqrt((y1 - y2) * (y1 - y2) + (x2 - x1) * (x2 - x1));
}
void alloc_lens_matrix_(const int odd_diameter, std::vector<int> &lens_offsets,
                        std::vector<int> &lens_sizes,
                        std::vector<std::vector<double>> &lens_ratio) {
  lens_offsets.resize(odd_diameter);
  lens_sizes.resize(odd_diameter);
  lens_ratio.resize(odd_diameter);
  for (int yy = 0; yy < odd_diameter; ++yy) {
    lens_ratio.at(yy).resize(odd_diameter);
  }
}
void free_lens_matrix_(std::vector<int> &lens_offsets,
                       std::vector<int> &lens_sizes,
                       std::vector<std::vector<double>> &lens_ratio) {
  lens_ratio.clear();
  lens_sizes.clear();
  lens_offsets.clear();
}
}
const int igs::maxmin::diameter_from_outer_radius(const double outer_radius) {
  /* -------- 半径(radius)を含むピクセル直径 --------
  中心pixelが必要、奇数(1,3,5...)となる
           ---+---|---+---|---+---|---+---|---+---
outer_radius-->		    +---->
Pixelの半分幅足す(+0.5)	|-------->
全体が入るPixel数幅(ceil())	|--------------->
整数化時誤差防ぐ(+0.5)		|------------------->
整数化(static_cast<int>)	|-------|------->
中心を除く半径Pixel幅(-1)		|------->
中心除く直径(*2)	<-------|	|------->
直径(+1)		<-------|-------|------->
-->odd_diameter
  */
  return (static_cast<int>(ceil(outer_radius + 0.5) + 0.5) - 1) * 2 + 1;
}
const double igs::maxmin::outer_radius_from_radius(
    const double radius, const double smooth_outer_range) {
  if (radius < 1.0) {
    /*
    半径ゼロでは処理しない絵となり、
    わずかな半径に変化したとき、
    いきなり大きなエッジ足(smooth_outer_range)が
    飛び出さないように

    radiusがゼロなら足長さを1.0(影響のない最小値)に、
    radiusが1ならradius + smooth_outer_rangeになり、
    影響が反映される

    smooth_outer_rangeは1以上でないとsmoothにならない。
*/
    return 1.0 + radius * (radius + smooth_outer_range - 1.0);
  }
  return radius + smooth_outer_range;
}
const int igs::maxmin::alloc_and_shape_lens_matrix(
    const double radius  // 0<=
    ,
    const double outer_radius, const int polygon_number  // =2
    ,
    const double roll_degree  // 0<= ... <=360
    ,
    std::vector<int> &lens_offsets, std::vector<int> &lens_sizes,
    std::vector<std::vector<double>> &lens_ratio) {
  /*------ 大きさが無い指定のときMemory解放 ------*/
  if (radius <= 0.0) {
    free_lens_matrix_(lens_offsets, lens_sizes, lens_ratio);
    return 0;
  }

  /*------ 半径から必要なピクセル単位の直径を求める ------*/
  const int odd_diameter =
      igs::maxmin::diameter_from_outer_radius(outer_radius);

  /*------ Memory確保or再利用 ------*/
  alloc_lens_matrix_(odd_diameter, lens_offsets, lens_sizes, lens_ratio);

  /*------ lens matrix情報を書き込む ------*/
  igs::maxmin::reshape_lens_matrix(
      radius,
      igs::maxmin::outer_radius_from_radius(radius, outer_radius - radius),
      odd_diameter, polygon_number, roll_degree, lens_offsets, lens_sizes,
      lens_ratio);
  return odd_diameter;
}
const void igs::maxmin::reshape_lens_matrix(
    const double radius  // 0<=
    ,
    const double outer_radius, const int odd_diameter /* 最大直径 */
    ,
    const int polygon_number  // =2
    ,
    const double roll_degree  // 0<= ... <=360
    ,
    std::vector<int> &lens_offsets /* 最大直径分の配列 */
    ,
    std::vector<int> &lens_sizes /* 最大直径分の配列 */
    ,
    std::vector<std::vector<double>> &lens_ratio /* 最大直径分の配列 */
    ) {
  /***std::cout
<< "ra=" << radius
<< " outr=" << outer_radius
<< " dia=" << odd_diameter
<< " pol=" << polygon_number
<< " deg=" << roll_degree
<< " lens_offsets=" << lens_offsets.size()
<< " lens_sizes=" << lens_sizes.size()
<< " lens_ratio=" << lens_ratio.size()
<< std::endl;***/

  /* --- scanlineスタート位置とscanlineサイズ生成 --- */
  double yp = 0.5 - (odd_diameter / 2.0); /* matrix中心からのy距離 */
  for (int yy = 0; yy < odd_diameter; ++yy, yp += 1.0) {
    /* --- 開始位置とサイズを計算 ------------------- */
    lens_offsets.at(yy) = -1;               /* 初期化 */
    lens_sizes.at(yy)   = 0;                /* 初期化 */
    double xp = 0.5 - (odd_diameter / 2.0); /* matrix中心からのx距離 */
    for (int xx = 0; xx < odd_diameter; ++xx, xp += 1.0) {
      const double current_radius = sqrt(xp * xp + yp * yp);
      /* 外枠影響内 */
      if ((current_radius <= outer_radius) &&
          inside_polygon_(outer_radius, odd_diameter, xp, yp, polygon_number,
                          roll_degree)) {
        /* scanlineスタート位置(ポインタ)のセット */
        if (lens_offsets.at(yy) < 0) {
          /* 半径内に入った瞬間その位置を記録 */
          lens_offsets.at(yy) = xx;
        }

      } else { /* 影響外 */
        /* scalineサイズのセット */
        if ((0 <= lens_offsets.at(yy)) && (lens_sizes.at(yy) == 0)) {
          /* 半径内を出た瞬間そこまでのサイズを記録 */
          lens_sizes.at(yy) = xx - lens_offsets.at(yy);
        }
      }
    }
    /* scanlineの最後までが半径内のとき */
    if ((0 <= lens_offsets.at(yy)) && (lens_sizes.at(yy) == 0)) {
      lens_sizes.at(yy) = odd_diameter - lens_offsets.at(yy);
    }

    /* --- 外枠から内枠へのリニア変化 --------------- */
    /* 円の外 */
    if (lens_sizes.at(yy) <= 0) {
      continue;
    }
    /* ratio設定 */
    xp     = 0.5 - (odd_diameter / 2.0); /* matrix中心からのx距離 */
    int xr = 0;
    for (int xx = 0; xx < odd_diameter; ++xx, xp += 1.0) {
      const double current_radius = sqrt(xp * xp + yp * yp);
      /* 外枠内 */
      if ((current_radius <= outer_radius) &&
          inside_polygon_(outer_radius, odd_diameter, xp, yp, polygon_number,
                          roll_degree)) {
        /* 内枠内 */
        if ((current_radius <= radius) &&
            inside_polygon_(radius, odd_diameter, xp, yp, polygon_number,
                            roll_degree)) {
          lens_ratio.at(yy).at(xr++) = 1.0;
        }
        /* 外枠と内枠の間 */
        else {
          if (polygon_number < 3) {
            lens_ratio.at(yy).at(xr++) =
                (outer_radius - current_radius) / (outer_radius - radius);
          } else {
            const double leninn =
                length_to_polygon_(radius, xp, yp, polygon_number, roll_degree);
            const double lenout = length_to_polygon_(
                outer_radius, xp, yp, polygon_number, roll_degree);
            lens_ratio.at(yy).at(xr++) = lenout / (leninn + lenout);
          }
        }
      }
    }
  }
}
