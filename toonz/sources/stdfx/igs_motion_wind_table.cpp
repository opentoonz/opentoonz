// #include <iostream>
#include <cmath> /* pow(), ceil() */
#include "igs_motion_wind_table.h"

int igs::motion_wind::table_size(const double length_min,
                                 const double length_max) {
  if (length_min < length_max) {
    return static_cast<int>(ceil(length_max));
  }
  return static_cast<int>(ceil(length_min));
}

/*
引数の(３つの)ランダムクラスは別々でもいいし、同じものでもいい。
別々でシード値を同じにすればlength,force,densityの３つとも
同じランダム列にすることができる。
同じクラスを使う場合は、length,force,densityを混ぜてランダムになる。
*/
int igs::motion_wind::make_table(
    std::vector<double> &table, igs::math::random &length_random,
    igs::math::random &force_random, igs::math::random &density_random,
    const double length_min, const double length_max,
    const double length_bias  // 0<...1...
    ,
    const double force_min, const double force_max,
    const double force_bias  // 0<...1...
    ,
    const double density_min, const double density_max,
    const double density_bias  // 0<...1...
    ) {
  /* length_minと、length_maxが同じ値なら固定長 */
  double length = length_min;
  if (length_min != length_max) {        /* minとmaxの間でrandomな長さ */
    double dif = length_random.next_d(); /* 0...1 */
    if (0.0 != length_bias) {            /* 長さの片寄り */
      dif = pow(dif, 1.0 / length_bias);
    }
    length += dif * (length_max - length_min); /* 実際の長さ */
  }
  // std::cout << "l " << length;

  /* force_minと、force_maxが同じ値なら固定カーブ */
  double force = force_min;
  if (force_min != force_max) { /* minとmaxの間でrandomなカーブ */
    double dif = force_random.next_d(); /* 0...1 */
    if (0.0 != force_bias) {            /* カーブの片寄り */
      dif = pow(dif, 1.0 / force_bias);
    }
    force += dif * (force_max - force_min); /* 実際のカーブ値 */
  }
  // std::cout << "  c " << force;

  /* density_minと、density_maxが同じ値なら固定高さ */
  double density = density_min;
  if (density_min != density_max) { /* minからmaxの間でrandomな高 */
    double dif = density_random.next_d(); /* 0...1 */
    if (0.0 != density_bias) {            /* 高さの片寄り */
      dif = pow(dif, 1.0 / density_bias);
    }
    density += dif * (density_max - density_min); /* 実際の高さ値 */
  }
  // std::cout << "  h " << density << "\n";

  const int table_len = static_cast<int>(ceil(length));
  /* 長さ:リニア減衰列を設定する
          table.at[0]          [1]            [2]
  length	ii
  3.0 --> 3 -->	1.0(3.0/3.0) 0.666(2.0/3.0) 0.333(1.0/3.0)
  2.5 --> 3 -->	1.0(2.5/2.5) 0.6(1.5/2.5)   0.2(0.5/2.5)
  2.0 --> 2 -->	1.0(2.0/2.0) 0.5(1.0/2.0)   [0.0(0.0/1.5)]
  1.5 --> 2 -->	1.0(1.5/1.5) 0.333(0.5/1.5) [-0.333(-0.5/1.5)]
  1.0 --> 1 -->	1.0(1.0/1.0) [0.0(0.0/1.0)]
  */
  for (int ii = 0; ii < table_len; ++ii) {
    table.at(ii) = (length - ii) / length;
  }

  /* 勢い:ガンマ計算によりカーブを設定する */
  if (1.0 != force) { /* 変化のないとき(1のとき)は動作しない */
    if (0.0 < force) {
      for (int ii = 1; ii < table_len; ++ii) {
        table.at(ii) = pow(table.at(ii), 1.0 / force);
      }
    } else { /* ゼロ以下はゼロを設定する */
      for (int ii = 1; ii < table_len; ++ii) {
        table.at(ii) = 0.0;
      }
    }
  }

  /* 濃さ:(高さ)の制御 */
  for (int ii = 1; ii < table_len; ++ii) {
    table.at(ii) *= density;
  }
  return table_len;
}
