#include <cmath>      // sqrt() sin() cos()
#include <vector>     // std::vector
#include <stdexcept>  // std::domain_error()

namespace {
bool inside_polygon_(double radius, int odd_diameter, double xp, double yp,
                     int polygon_num, double degree) {
  double radian = degree * (M_PI / 180), add_radian = 2.0 * M_PI / polygon_num,
         x1 = 0, y1 = 0, x2, y2, xa = -odd_diameter, xb = -odd_diameter;

  for (int ii = 0; ii <= polygon_num;
       ++ii, radian += add_radian, x1 = x2, y1 = y2) {
    /* (x2,y2)は(0,0)を原点とした正多角形の各頂点 */
    /* 注意:参照の図形と 計算結果の図形は左右反転する。
            そのため結果画像は左からCW回転となってしまう。
            数学的開始点(右)及び回転方向(CCW)とするため、
            内部で上下左右反転する。
    */
    x2 = -radius * cos(radian);
    y2 = -radius * sin(radian);

    /* 線分をみるので、loop次へ */
    if (ii <= 0) {
      continue;
    }

    /* ypのy scanline上で交差
    ((y2==y1==yp)の場合も)しない線分なら次へ */
    if (!(((y1 <= yp) && (yp <= y2)) || ((y2 <= yp) && (yp <= y1)))) {
      continue;
    }

    /* 水平線分上 */
    if (y2 == y1) {
      if (((x1 <= xp) && (xp <= x2)) || ((x2 <= xp) && (xp <= x1))) {
        return true;
      } else {
        return false;
      } /* 水平範囲外 */
    }

    /* 初めの交差 */
    if (xa == -odd_diameter) {
      /* (x2 - x1)/(y2 - y1)=(xa - x1)/(yp - y1);
(xa - x1)=(yp - y1)*(x2 - x1)/(y2 - y1); */
      xa = (yp - y1) * (x2 - x1) / (y2 - y1) + x1;
    } else
        /* 2番目の交差 */
        if (xb == -odd_diameter) {
      xb = (yp - y1) * (x2 - x1) / (y2 - y1) + x1;
      if (((xa <= xp) && (xp <= xb)) || ((xb <= xp) && (xp <= xa))) {
        return true;
      } else {
        return false;
      } /* 水平範囲外 */
    }
  }
  return false;
}
/* 円錐型(1==curve)に減衰分布する２次元配列を生成する */
// bool attenuation_distribution_(
void attenuation_distribution_(
    std::vector<std::vector<double>> &lens_matrix,
    std::vector<int> &lens_offsets, std::vector<double *> &lens_starts,
    std::vector<int> &lens_sizes, int &odd_diameter,
    const double radius /* =1.01 円半径(Pixel単位)1より大きい値 */
    ,
    const double curve /* =1. 0:無 0<.<1.0:減 1.0:リニア 1.0<:増 */
    ,
    const int polygon_num /* =2  2:円 3<=:円内接多角形(中心右始) */
    ,
    const double degree /* =0. 多角形開始角度(傾き)(反時計回り) */
    ) {
  /*
  double radius
          影響の半径
          1より大きい浮動小数値
          diameterはこの値から自動設定する
  double curve
          中心から周辺への下り具合(デフォルトは1)
          光り拡散の強弱
          zeroでない浮動小数値
*/
  /*
  lens_offsetsとlens_sizesで影響範囲を表わすmatrixを表わす
  radiusは影響円の半径
  matrix(縦横)サイズ(lens_offsets.size())は
          円(radius)が入る最小整数値でかつ、
          1以上の奇数(1,3,5,...)値

  影響は上下反転、左右反転するので注意、以下説明図
  y

  ^
  |	        |\
  |	        |   \
  |	        |  *   >
  |	|\      |   /
  |	|   \   |/
  |	|  *   >P
  |	|   /   |\
  |	|/      |   \
  |	        |  *   >
  |	        |   /
  |	        |/
--+--------------------------------------> x
  y

  ^
  |	   -----------
  |	    \   *   /
  |	      \   /
  |     ----------P----------
  |      \   *   / \   *   /
  |	 \   /     \   /
  |	   v         v
--+--------------------------------------> x
  |   P       : Target pixel
  |   <,/,\,| : 影響範囲
  |   *       : 描画の結果
*/
  /* --- 散光の力がゼロだと画像に対する変化はない ----- */
  if (0.0 == curve) {
    // return false;
    throw std::domain_error("curve is zero");
  }

  /* --- 直径は、半径(radius)の2倍より大きい整数値 ---- */
  odd_diameter = static_cast<int>(ceil(radius * 2.0));

  /* --- 直径が1Pixel以下は画像に対する変化はない ----- */
  if (odd_diameter <= 1) {
    // return false;
    throw std::domain_error("diameter is equal less than 1");
  }

  /* --- 中心pixelが必要なので、奇数(1,3,5...)にする -- */
  if (0 == (odd_diameter % 2)) {
    ++odd_diameter;
  }

  /* --- Memory確保or再利用 --------------------------- */
  lens_matrix.resize(odd_diameter);
  for (int yy = 0; yy < odd_diameter; ++yy) {
    lens_matrix.at(yy).resize(odd_diameter);
  }
  lens_offsets.resize(odd_diameter);
  lens_starts.resize(odd_diameter);
  lens_sizes.resize(odd_diameter);

  /* ---  1.円のレンズmatrix生成
          2.scanlineスタート位置とスタートポインタのセット
          3.scanlineサイズのセット -------------------- */

  /* --- scanlineスタート位置とscanlineサイズ生成 --- */
  double yp = 0.5 - (odd_diameter / 2.0); /* matrix中心からのy距離 */
  for (int yy = 0; yy < odd_diameter; ++yy, yp += 1.0) {
    lens_offsets.at(yy) = -1;               /* 初期値 */
    lens_starts.at(yy)  = 0;                /* 初期値 */
    lens_sizes.at(yy)   = -1;               /* 初期値 */
    double xp = 0.5 - (odd_diameter / 2.0); /* matrix中心からのx距離 */
    for (int xx = 0; xx < odd_diameter; ++xx, xp += 1.0) {
      const double length = sqrt(xp * xp + yp * yp);
      if ((length <= radius) &&
          ((polygon_num < 3) ||
           inside_polygon_(radius, odd_diameter, xp, yp, polygon_num,
                           degree))) { /* 影響内 */

        /* 1.円のレンズmatrix生成 */

        /* 中心が1で外郭が0その間をリニアに */
        double val = 1.0 - (length / radius);

        /* 光の減衰の様子をGamma曲線で指定する */
        /* 0<.<1.0:光減 1.0:リニア 1.0<:光増 */
        val = pow(val, 1.0 / curve);

        /* 影響内は値を設定 */
        lens_matrix.at(yy).at(xx) = val;

        /* 2.scanlineスタート位置(ポインタ)のセット */

        if (lens_offsets.at(yy) < 0) {
          /* 半径内に入った瞬間その位置を記録 */
          lens_offsets.at(yy) = xx;
          lens_starts.at(yy)  = &lens_matrix.at(yy).at(xx);
        }
      } else { /* 影響外 */
        /* 1.円のレンズmatrix生成 */

        /* 影響外はゼロ */
        lens_matrix.at(yy).at(xx) = 0.0;

        /* 3.scalineサイズのセット */

        if ((0 <= lens_offsets.at(yy)) && (lens_sizes.at(yy) < 0)) {
          /* 半径内を出た瞬間そこまでのサイズを記録 */
          lens_sizes.at(yy) = xx - lens_offsets.at(yy);
        }
      }
    }
    /* scanlineの最後までが半径内のとき */
    if ((0 <= lens_offsets.at(yy)) && (lens_sizes.at(yy) < 0)) {
      lens_sizes.at(yy) = odd_diameter - lens_offsets.at(yy);
    }
  }

  /* matrixの値を正規化 */
  double total = 0.0;
  for (unsigned int yy = 0; yy < lens_matrix.size(); ++yy) {
    for (unsigned int xx = 0; xx < lens_matrix.at(yy).size(); ++xx) {
      total += lens_matrix.at(yy).at(xx);
    }
  }
  for (unsigned int yy = 0; yy < lens_matrix.size(); ++yy) {
    for (unsigned int xx = 0; xx < lens_matrix.at(yy).size(); ++xx) {
      lens_matrix.at(yy).at(xx) /= total;
    }
  }
  // return true;
}
}
