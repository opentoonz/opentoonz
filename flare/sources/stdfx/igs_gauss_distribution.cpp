#include <cmath>
namespace {
/*
                                    center
                                      V
        sequence
          0   1   2 ...				     int_diameter-1
          v   v   v ...						  v
        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
        |<------------------- int_diameter(pixel) ----------------->|
          |<--- int_radius(pixel) --->|
           |<------ real_radius ----->|
              |<- int_radius(pixel) ->|
               |<---- real_radius --->|
                  |<-int_radius(pi) ->|
                   |<- real_radius -->|
                      |<-int_radius ->|
                       |<- real_rad ->|
                          |<- int   ->|
                           |<- real ->|
                              |<-   ->|
                               |<-  ->|
                                  |<->|
                                   |<>|
 */
void gauss_distribution_1d_(double *sequence,
                            const int int_diameter /* sequence配列の長さ */
                            ,
                            const int int_radius, const double real_radius,
                            const double sigma /* = 0.25 : 適当な値... */
                            ) {
  /* メモリクリア */
  for (int ii = 0; ii < int_diameter; ++ii) {
    sequence[ii] = 0.0;
  }

  /* 半径がゼロでも計算を可能にする、強引に... */
  const int int_radius2 = (0 < int_radius) ? int_radius : 1;
  const double real_radius2 =
      (0. < real_radius) ? real_radius : 0.000000000000001;

  /* 1pixel以下の調整
   * -1.0の位置を(real_radius2)の位置とする */
  double dd = -1.0 * int_radius2 / real_radius2;

  /* 1pixel幅の差分 */
  const double delta = -dd / int_radius2;

  /* 開始位置 */
  const int start_pos = (int_diameter - (int_radius2 * 2 + 1)) / 2;

  /* 終了位置 */
  const int end_pos = int_diameter - start_pos;

  /* ガウス分布 */
  for (int ii = start_pos; ii < end_pos; ++ii) {
    sequence[ii] = exp(-(dd * dd) / (2.0 * sigma * sigma));
    dd += delta;
  }
  /* 積算 */
  dd = 0.0;
  for (int ii = start_pos; ii < end_pos; ++ii) {
    dd += sequence[ii];
  }
  /* 正規化(積算結果が1となるように割算する) */
  for (int ii = start_pos; ii < end_pos; ++ii) {
    sequence[ii] /= dd;
  }
}
}
