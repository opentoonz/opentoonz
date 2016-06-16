/*------------------------------------
Iwa_MotionBlurCompFx
露光量／オブジェクトの軌跡を考慮したモーションブラー
背景との露光値合成を可能にする
//------------------------------------*/

#include "iwa_motionblurfx.h"
#include "tfxattributes.h"

#include "toonz/tstageobject.h"

#include "trop.h"

/*- ソース画像を０〜１に正規化してホストメモリに読み込む
        ソース画像がPremultipyされているか、コンボボックスで指定されていない場合は
        ここで判定する -*/
template <typename RASTER, typename PIXEL>
bool Iwa_MotionBlurCompFx::setSourceRaster(const RASTER srcRas, float4 *dstMem,
                                           TDimensionI dim,
                                           PremultiTypes type) {
  bool isPremultiplied = (type == SOURCE_IS_NOT_PREMUTIPLIED) ? false : true;

  float4 *chann_p = dstMem;

  float threshold = 100.0 / (float)TPixel64::maxChannelValue;

  int max = 0;
  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;

      /*- RGB値がアルファチャンネルより大きいピクセルがあれば、
              Premutiplyされていないと判断する -*/
      if (type == AUTO && isPremultiplied &&
          (((*chann_p).x > (*chann_p).w && (*chann_p).x > threshold) ||
           ((*chann_p).y > (*chann_p).w && (*chann_p).y > threshold) ||
           ((*chann_p).z > (*chann_p).w && (*chann_p).z > threshold)))
        isPremultiplied = false;

      pix++;
      chann_p++;
    }
  }

  if (isPremultiplied) {
    chann_p = dstMem;
    for (int i = 0; i < dim.lx * dim.ly; i++, chann_p++) {
      if ((*chann_p).x > (*chann_p).w) (*chann_p).x = (*chann_p).w;
      if ((*chann_p).y > (*chann_p).w) (*chann_p).y = (*chann_p).w;
      if ((*chann_p).z > (*chann_p).w) (*chann_p).z = (*chann_p).w;
    }
  }

  return isPremultiplied;
}

/*------------------------------------------------------------
 出力結果をChannel値に変換してタイルに格納
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_MotionBlurCompFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                           TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    PIXEL *pix     = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++) {
      float val;
      val    = (*chan_p).x * (float)PIXEL::maxChannelValue + 0.5f;
      pix->r = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).y * (float)PIXEL::maxChannelValue + 0.5f;
      pix->g = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).z * (float)PIXEL::maxChannelValue + 0.5f;
      pix->b = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      val    = (*chan_p).w * (float)PIXEL::maxChannelValue + 0.5f;
      pix->m = (typename PIXEL::Channel)((val > (float)PIXEL::maxChannelValue)
                                             ? (float)PIXEL::maxChannelValue
                                             : val);
      pix++;
      chan_p++;
    }
  }
}

/*------------------------------------------------------------
 フィルタをつくり、正規化する
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::makeMotionBlurFilter_CPU(
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    float4 *pointsTable, int pointAmount, float startValue, float startCurve,
    float endValue, float endCurve) {
  /*- フィルタ値を足しこむための変数 -*/
  float fil_val_sum = 0.0f;
  /*- for文の中で回す、現在のフィルタ座標 -*/
  float *current_fil_p = filter_p;
  /*- 各フィルタの座標について -*/
  for (int fily = 0; fily < filterDim.ly; fily++) {
    for (int filx = 0; filx < filterDim.lx; filx++, current_fil_p++) {
      /*- フィルタ座標を得る -*/
      float2 pos = {static_cast<float>(filx - marginLeft),
                    static_cast<float>(fily - marginBottom)};
      /*- 更新してゆく値 -*/
      float nearestDist2         = 100.0f;
      int nearestIndex           = -1;
      float nearestFramePosRatio = 0.0f;

      /*- 各サンプル点のペアについて、一番近い点を探す -*/
      for (int v = 0; v < pointAmount - 1; v++) {
        float4 p0 = pointsTable[v];
        float4 p1 = pointsTable[v + 1];

        /*- 範囲内に無ければcontinue -*/
        if (pos.x < std::min(p0.x, p1.x) - 1.0f ||
            pos.x > std::max(p0.x, p1.x) + 1.0f ||
            pos.y < std::min(p0.y, p1.y) - 1.0f ||
            pos.y > std::max(p0.y, p1.y) + 1.0f)
          continue;

        /*- 範囲内にあるので、線分と点の距離を得る -*/
        /*- P0->サンプル点とP0->P1の内積を求める -*/
        float2 vec_p0_sample = {static_cast<float>(pos.x - p0.x),
                                static_cast<float>(pos.y - p0.y)};
        float2 vec_p0_p1 = {static_cast<float>(p1.x - p0.x),
                            static_cast<float>(p1.y - p0.y)};
        float dot =
            vec_p0_sample.x * vec_p0_p1.x + vec_p0_sample.y * vec_p0_p1.y;
        /*- 距離の2乗を求める -*/
        float dist2;
        float framePosRatio;
        /*- P0より手前にある場合 -*/
        if (dot <= 0.0f) {
          dist2 = vec_p0_sample.x * vec_p0_sample.x +
                  vec_p0_sample.y * vec_p0_sample.y;
          framePosRatio = 0.0f;
        } else {
          /*- 軌跡ベクトルの長さの２乗を計算する -*/
          float length2 = p0.z * p0.z;

          /*-  P0〜P1間にある場合
                   pでの軌跡が点のとき、length2は０になるので、この条件の中に入ることはない。
                   ので、ZeroDivideになる心配はないはず。 -*/
          if (dot < length2) {
            float p0_sample_dist2 = vec_p0_sample.x * vec_p0_sample.x +
                                    vec_p0_sample.y * vec_p0_sample.y;
            dist2         = p0_sample_dist2 - dot * dot / length2;
            framePosRatio = dot / length2;
          }
          /*- P1より先にある場合 -*/
          else {
            float2 vec_p1_sample = {pos.x - p1.x, pos.y - p1.y};
            dist2                = vec_p1_sample.x * vec_p1_sample.x +
                    vec_p1_sample.y * vec_p1_sample.y;
            framePosRatio = 1.0f;
          }
        }
        /*- 距離が(√2 + 1)/2より遠かったらcontinue
         * dist2との比較だから2乗している -*/
        if (dist2 > 1.4571f) continue;

        /*- 距離がより近かったら更新する -*/
        if (dist2 < nearestDist2) {
          nearestDist2         = dist2;
          nearestIndex         = v;
          nearestFramePosRatio = framePosRatio;
        }
      }

      /*-
       * 現在のピクセルの、近傍ベクトルが見つからなかった場合、フィルタ値は０でreturn
       * -*/
      if (nearestIndex == -1) {
        *current_fil_p = 0.0f;
        continue;
      }

      /*-
         現在のピクセルのサブピクセル(16*16)が、近傍ベクトルからの距離0.5の範囲にどれだけ
              含まれているかをカウントする。 -*/
      int count  = 0;
      float4 np0 = pointsTable[nearestIndex];
      float4 np1 = pointsTable[nearestIndex + 1];
      for (int yy = 0; yy < 16; yy++) {
        /*- サブピクセルのY座標 -*/
        float subPosY = pos.y + ((float)yy - 7.5f) / 16.0f;
        for (int xx = 0; xx < 16; xx++) {
          /*- サブピクセルのX座標 -*/
          float subPosX = pos.x + ((float)xx - 7.5f) / 16.0f;

          float2 vec_np0_sub = {subPosX - np0.x, subPosY - np0.y};
          float2 vec_np0_np1 = {np1.x - np0.x, np1.y - np0.y};
          float dot =
              vec_np0_sub.x * vec_np0_np1.x + vec_np0_sub.y * vec_np0_np1.y;
          /*- 距離の2乗を求める -*/
          float dist2;
          /*- P0より手前にある場合 -*/
          if (dot <= 0.0f)
            dist2 =
                vec_np0_sub.x * vec_np0_sub.x + vec_np0_sub.y * vec_np0_sub.y;
          else {
            /*- 軌跡ベクトルの長さの２乗を計算する -*/
            float length2 = np0.z * np0.z;
            /*-  P0〜P1間にある場合 -*/
            if (dot < length2) {
              float np0_sub_dist2 =
                  vec_np0_sub.x * vec_np0_sub.x + vec_np0_sub.y * vec_np0_sub.y;
              dist2 = np0_sub_dist2 - dot * dot / length2;
            }
            /*-  P1より先にある場合 -*/
            else {
              float2 vec_np1_sub = {subPosX - np1.x, subPosY - np1.y};
              dist2 =
                  vec_np1_sub.x * vec_np1_sub.x + vec_np1_sub.y * vec_np1_sub.y;
            }
          }
          /*- 距離の２乗が0.25より近ければカウントをインクリメント -*/
          if (dist2 <= 0.25f) count++;
        }
      }
      /*- 保険 カウントが０の場合はフィールド値０でreturn -*/
      if (count == 0) {
        *current_fil_p = 0.0f;
        continue;
      }

      /*- countは Max256 -*/
      float countRatio = (float)count / 256.0f;

      /*- フィルタ値の明るさは、ベクトルが作る幅１の線の面積に反比例する。
              ベクトルの前後には半径0.5の半円のキャップがあるので、長さ０のベクトルでも
              0-divideになることはない。-*/

      /*- 近傍ベクトル、幅１のときの面積 -*/
      float vecMenseki = 0.25f * 3.14159265f + np0.z;

      //-----------------
      /*- 続いて、ガンマ強弱の値を得る -*/
      /*- 近傍点のフレームのオフセット値 -*/
      float curveValue;
      float frameOffset =
          np0.w * (1.0f - nearestFramePosRatio) + np1.w * nearestFramePosRatio;
      /*- フレームがちょうどフレーム原点、又は減衰値が無い場合はcurveValue = 1
       * -*/
      if (frameOffset == 0.0f || (frameOffset < 0.0f && startValue == 1.0f) ||
          (frameOffset > 0.0f && endValue == 1.0f))
        curveValue = 1.0f;
      else {
        /*- オフセットの正負によって変える -*/
        float value, curve, ratio;
        if (frameOffset < 0.0f) /*- start側 -*/
        {
          value = startValue;
          curve = startCurve;
          ratio = 1.0f - (frameOffset / pointsTable[0].w);
        } else {
          value = endValue;
          curve = endCurve;
          ratio = 1.0f - (frameOffset / pointsTable[pointAmount - 1].w);
        }

        curveValue = value + (1.0f - value) * powf(ratio, 1.0f / curve);
      }
      //-----------------

      /*- フィールド値の格納 -*/
      *current_fil_p = curveValue * countRatio / vecMenseki;
      fil_val_sum += *current_fil_p;
    }
  }

  /*- 正規化 -*/
  current_fil_p = filter_p;
  for (int f = 0; f < filterDim.lx * filterDim.ly; f++, current_fil_p++) {
    *current_fil_p /= fil_val_sum;
  }
}

/*------------------------------------------------------------
 残像フィルタをつくり、正規化する
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::makeZanzoFilter_CPU(
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    float4 *pointsTable, int pointAmount, float startValue, float startCurve,
    float endValue, float endCurve) {
  /*-フィルタ値足しこむための変数-*/
  float fil_val_sum = 0.0f;
  /*- for文の中で回す、現在のフィルタ座標 -*/
  float *current_fil_p = filter_p;
  /*- 各フィルタの座標について -*/
  for (int fily = 0; fily < filterDim.ly; fily++) {
    for (int filx = 0; filx < filterDim.lx; filx++, current_fil_p++) {
      /*- フィルタ座標を得る -*/
      float2 pos = {(float)(filx - marginLeft), (float)(fily - marginBottom)};
      /*- これから積算する変数 -*/
      float filter_sum = 0.0f;
      /*- 各サンプル点について距離を測り、濃度を積算していく -*/
      for (int v = 0; v < pointAmount; v++) {
        float4 p0 = pointsTable[v];
        /*- p0の座標を中心として、距離 1 以内に無ければcontinue -*/
        if (pos.x < p0.x - 1.0f || pos.x > p0.x + 1.0f || pos.y < p0.y - 1.0f ||
            pos.y > p0.y + 1.0f)
          continue;

        /*- 近傍４ピクセルで線形補間する -*/
        float xRatio = 1.0f - abs(pos.x - p0.x);
        float yRatio = 1.0f - abs(pos.y - p0.y);

        /*- 続いて、ガンマ強弱の値を得る -*/
        /*- 近傍点のフレームのオフセット値 -*/
        float curveValue;
        float frameOffset = p0.w;
        /*- フレームがちょうどフレーム原点、又は減衰値が無い場合はcurveValue = 1
         * -*/
        if (frameOffset == 0.0f || (frameOffset < 0.0f && startValue == 1.0f) ||
            (frameOffset > 0.0f && endValue == 1.0f))
          curveValue = 1.0f;
        else {
          /*- オフセットの正負によって変える -*/
          float value, curve, ratio;
          if (frameOffset < 0.0f) /*- start側 -*/
          {
            value = startValue;
            curve = startCurve;
            ratio = 1.0f - (frameOffset / pointsTable[0].w);
          } else {
            value = endValue;
            curve = endCurve;
            ratio = 1.0f - (frameOffset / pointsTable[pointAmount - 1].w);
          }

          curveValue = value + (1.0f - value) * powf(ratio, 1.0f / curve);
        }
        //-----------------

        /*- フィルタ値積算 -*/
        filter_sum += xRatio * yRatio * curveValue;
      }

      /*- 値を格納 -*/
      *current_fil_p = filter_sum;
      fil_val_sum += *current_fil_p;
    }
  }

  /*- 正規化 -*/
  current_fil_p = filter_p;
  for (int f = 0; f < filterDim.lx * filterDim.ly; f++, current_fil_p++) {
    *current_fil_p /= fil_val_sum;
  }
}

/*------------------------------------------------------------
 露光値をdepremultipy→RGB値(０〜１)に戻す→premultiply
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::convertRGBtoExposure_CPU(
    float4 *in_tile_p, TDimensionI &dim, float hardness,
    bool sourceIsPremultiplied) {
  float4 *cur_tile_p = in_tile_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, cur_tile_p++) {
    /*- アルファが０ならreturn -*/
    if (cur_tile_p->w == 0.0f) {
      cur_tile_p->x = 0.0f;
      cur_tile_p->y = 0.0f;
      cur_tile_p->z = 0.0f;
      continue;
    }

    /*- 通常のLevelなど、Premultiplyされている素材に対し、depremultiplyを行う
            DigiBookなどには行わない -*/
    if (sourceIsPremultiplied) {
      /*- depremultiply -*/
      cur_tile_p->x /= cur_tile_p->w;
      cur_tile_p->y /= cur_tile_p->w;
      cur_tile_p->z /= cur_tile_p->w;
    }

    /*- RGBはExposureにする -*/
    cur_tile_p->x = powf(10, (cur_tile_p->x - 0.5f) * hardness);
    cur_tile_p->y = powf(10, (cur_tile_p->y - 0.5f) * hardness);
    cur_tile_p->z = powf(10, (cur_tile_p->z - 0.5f) * hardness);

    /*- その後、アルファチャンネルでmultiply -*/
    cur_tile_p->x *= cur_tile_p->w;
    cur_tile_p->y *= cur_tile_p->w;
    cur_tile_p->z *= cur_tile_p->w;
  }
}

/*------------------------------------------------------------
 露光値をフィルタリングしてぼかす
 outDim の範囲だけループで回す
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::applyBlurFilter_CPU(
    float4 *in_tile_p, float4 *out_tile_p, TDimensionI &enlargedDim,
    float *filter_p, TDimensionI &filterDim, int marginLeft, int marginBottom,
    int marginRight, int marginTop, TDimensionI &outDim) {
  for (int i = 0; i < outDim.lx * outDim.ly; i++) {
    /*- in_tile_devとout_tile_devはlx * ly の寸法でデータが入っている。
            ので、i を出力用の座標に変換する -*/
    int2 outPos  = {i % outDim.lx + marginRight, i / outDim.lx + marginTop};
    int outIndex = outPos.y * enlargedDim.lx + outPos.x;
    /*- out_tile_dev[outIndex]に結果をおさめていく -*/

    /*- 値を積算する入れ物を用意 -*/
    float4 value = {0.0f, 0.0f, 0.0f, 0.0f};

    /*- フィルタのサイズでループ
       ただし、フィルタはサンプル点の画像を収集するように
            用いるため、上下左右反転してサンプルする -*/
    int filterIndex = 0;
    for (int fily = -marginBottom; fily < filterDim.ly - marginBottom; fily++) {
      /*- サンプル座標 と インデックス の このスキャンラインのうしろ -*/
      int2 samplePos  = {outPos.x + marginLeft, outPos.y - fily};
      int sampleIndex = samplePos.y * enlargedDim.lx + samplePos.x;

      for (int filx = -marginLeft; filx < filterDim.lx - marginLeft;
           filx++, filterIndex++, sampleIndex--) {
        /*- フィルター値が０またはサンプルピクセルが透明ならcontinue -*/
        if (filter_p[filterIndex] == 0.0f || in_tile_p[sampleIndex].w == 0.0f)
          continue;
        /*- サンプル点の値にフィルタ値を掛けて積算する -*/
        value.x += in_tile_p[sampleIndex].x * filter_p[filterIndex];
        value.y += in_tile_p[sampleIndex].y * filter_p[filterIndex];
        value.z += in_tile_p[sampleIndex].z * filter_p[filterIndex];
        value.w += in_tile_p[sampleIndex].w * filter_p[filterIndex];
      }
    }

    out_tile_p[outIndex].x = value.x;
    out_tile_p[outIndex].y = value.y;
    out_tile_p[outIndex].z = value.z;
    out_tile_p[outIndex].w = value.w;
  }
}

/*------------------------------------------------------------
 露光値をdepremultipy→RGB値(０〜１)に戻す→premultiply
------------------------------------------------------------*/

void Iwa_MotionBlurCompFx::convertExposureToRGB_CPU(float4 *out_tile_p,
                                                    TDimensionI &dim,
                                                    float hardness) {
  float4 *cur_tile_p = out_tile_p;
  for (int i = 0; i < dim.lx * dim.ly; i++, cur_tile_p++) {
    /*- アルファが０ならreturn -*/
    if (cur_tile_p->w == 0.0f) {
      cur_tile_p->x = 0.0f;
      cur_tile_p->y = 0.0f;
      cur_tile_p->z = 0.0f;
      continue;
    }

    // depremultiply
    cur_tile_p->x /= cur_tile_p->w;
    cur_tile_p->y /= cur_tile_p->w;
    cur_tile_p->z /= cur_tile_p->w;

    /*- ExposureをRGB値にする -*/
    cur_tile_p->x = log10f(cur_tile_p->x) / hardness + 0.5f;
    cur_tile_p->y = log10f(cur_tile_p->y) / hardness + 0.5f;
    cur_tile_p->z = log10f(cur_tile_p->z) / hardness + 0.5f;

    // multiply
    cur_tile_p->x *= cur_tile_p->w;
    cur_tile_p->y *= cur_tile_p->w;
    cur_tile_p->z *= cur_tile_p->w;

    /*- クランプ -*/
    cur_tile_p->x = (cur_tile_p->x > 1.0f)
                        ? 1.0f
                        : ((cur_tile_p->x < 0.0f) ? 0.0f : cur_tile_p->x);
    cur_tile_p->y = (cur_tile_p->y > 1.0f)
                        ? 1.0f
                        : ((cur_tile_p->y < 0.0f) ? 0.0f : cur_tile_p->y);
    cur_tile_p->z = (cur_tile_p->z > 1.0f)
                        ? 1.0f
                        : ((cur_tile_p->z < 0.0f) ? 0.0f : cur_tile_p->z);
  }
}

/*------------------------------------------------------------
 背景があり、前景が動かない場合、単純にOverする
------------------------------------------------------------*/
void Iwa_MotionBlurCompFx::composeWithNoMotion(
    TTile &tile, double frame, const TRenderSettings &settings) {
  assert(m_background.isConnected());

  m_background->compute(tile, frame, settings);

  TTile fore_tile;
  m_input->allocateAndCompute(fore_tile, tile.m_pos,
                              tile.getRaster()->getSize(), tile.getRaster(),
                              frame, settings);

  TRasterP up(fore_tile.getRaster()), down(tile.getRaster());
  TRop::over(down, up);
}

/*------------------------------------------------------------
 背景を露光値にして通常合成
------------------------------------------------------------*/
void Iwa_MotionBlurCompFx::composeBackgroundExposure_CPU(
    float4 *out_tile_p, TDimensionI &enlargedDimIn, int marginRight,
    int marginTop, TTile &back_tile, TDimensionI &dimOut, float hardness) {
  /*- ホストのメモリ確保 -*/
  TRasterGR8P background_host_ras(sizeof(float4) * dimOut.lx, dimOut.ly);
  background_host_ras->lock();
  float4 *background_host = (float4 *)background_host_ras->getRawData();

  bool bgIsPremultiplied;

  /*- 背景画像を０〜１に正規化してホストメモリに読み込む -*/
  TRaster32P backRas32 = (TRaster32P)back_tile.getRaster();
  TRaster64P backRas64 = (TRaster64P)back_tile.getRaster();
  if (backRas32)
    bgIsPremultiplied = setSourceRaster<TRaster32P, TPixel32>(
        backRas32, background_host, dimOut);
  else if (backRas64)
    bgIsPremultiplied = setSourceRaster<TRaster64P, TPixel64>(
        backRas64, background_host, dimOut);

  float4 *bg_p = background_host;
  float4 *out_p;

  for (int j = 0; j < dimOut.ly; j++) {
    out_p = out_tile_p + ((marginTop + j) * enlargedDimIn.lx + marginRight);
    for (int i = 0; i < dimOut.lx; i++, bg_p++, out_p++) {
      /*- 上レイヤが完全に不透明ならcontinue -*/
      if ((*out_p).w >= 1.0f) continue;

      /*- 下レイヤが完全に透明でもcontinue -*/
      if ((*bg_p).w < 0.0001f) continue;

      float3 bgExposure = {(*bg_p).x, (*bg_p).y, (*bg_p).z};

      /*- 通常のLevelなど、Premultiplyされている素材に対し、depremultiplyを行う
              DigiBookなどには行わない -*/
      if (bgIsPremultiplied) {
        // demultiply
        bgExposure.x /= (*bg_p).w;
        bgExposure.y /= (*bg_p).w;
        bgExposure.z /= (*bg_p).w;
      }

      /*- ExposureをRGB値にする -*/
      bgExposure.x = powf(10, (bgExposure.x - 0.5f) * hardness);
      bgExposure.y = powf(10, (bgExposure.y - 0.5f) * hardness);
      bgExposure.z = powf(10, (bgExposure.z - 0.5f) * hardness);

      // multiply
      bgExposure.x *= (*bg_p).w;
      bgExposure.y *= (*bg_p).w;
      bgExposure.z *= (*bg_p).w;

      /*- 手前とOver合成 -*/
      (*out_p).x = (*out_p).x + bgExposure.x * (1.0f - (*out_p).w);
      (*out_p).y = (*out_p).y + bgExposure.y * (1.0f - (*out_p).w);
      (*out_p).z = (*out_p).z + bgExposure.z * (1.0f - (*out_p).w);
      /*- アルファ値もOver合成 -*/
      (*out_p).w = (*out_p).w + (*bg_p).w * (1.0f - (*out_p).w);
    }
  }
  background_host_ras->unlock();
}

//------------------------------------------------------------

Iwa_MotionBlurCompFx::Iwa_MotionBlurCompFx()
    : m_hardness(0.3)
    /*- 左右をぼかすためのパラメータ -*/
    , m_startValue(1.0)
    , m_startCurve(1.0)
    , m_endValue(1.0)
    , m_endCurve(1.0)
    , m_zanzoMode(false)
    , m_premultiType(new TIntEnumParam(AUTO, "Auto")) {
  /*- 共通パラメータのバインド -*/
  addInputPort("Source", m_input);
  addInputPort("Back", m_background);

  bindParam(this, "hardness", m_hardness);
  bindParam(this, "shutterStart", m_shutterStart);
  bindParam(this, "shutterEnd", m_shutterEnd);
  bindParam(this, "traceResolution", m_traceResolution);
  bindParam(this, "motionObjectType", m_motionObjectType);
  bindParam(this, "motionObjectIndex", m_motionObjectIndex);

  bindParam(this, "startValue", m_startValue);
  bindParam(this, "startCurve", m_startCurve);
  bindParam(this, "endValue", m_endValue);
  bindParam(this, "endCurve", m_endCurve);

  bindParam(this, "zanzoMode", m_zanzoMode);

  bindParam(this, "premultiType", m_premultiType);

  /*- 共通パラメータの範囲設定 -*/
  m_hardness->setValueRange(0.05, 10.0);
  m_startValue->setValueRange(0.0, 1.0);
  m_startCurve->setValueRange(0.1, 10.0);
  m_endValue->setValueRange(0.0, 1.0);
  m_endCurve->setValueRange(0.1, 10.0);

  m_premultiType->addItem(SOURCE_IS_PREMULTIPLIED, "Source is premultiplied");
  m_premultiType->addItem(SOURCE_IS_NOT_PREMUTIPLIED,
                          "Source is NOT premultiplied");

  getAttributes()->setIsSpeedAware(true);
}

//------------------------------------------------------------

void Iwa_MotionBlurCompFx::doCompute(TTile &tile, double frame,
                                     const TRenderSettings &settings) {
  /*- 接続していない場合は処理しない -*/
  if (!m_input.isConnected() && !m_background.isConnected()) {
    tile.getRaster()->clear();
    return;
  }
  /*- BGのみ接続の場合 -*/
  if (!m_input.isConnected()) {
    m_background->compute(tile, frame, settings);
    return;
  }

  /*- 動作パラメータを得る -*/
  QList<TPointD> points = getAttributes()->getMotionPoints();
  double hardness       = m_hardness->getValue(frame);
  double shutterStart   = m_shutterStart->getValue(frame);
  double shutterEnd     = m_shutterEnd->getValue(frame);
  int traceResolution   = m_traceResolution->getValue();
  float startValue      = (float)m_startValue->getValue(frame);
  float startCurve      = (float)m_startCurve->getValue(frame);
  float endValue        = (float)m_endValue->getValue(frame);
  float endCurve        = (float)m_endCurve->getValue(frame);

  /*- 軌跡データが２つ以上無い場合は、処理しない -*/
  if (points.size() < 2) {
    if (!m_background.isConnected()) m_input->compute(tile, frame, settings);
    /*- 背景があり、前景が動かない場合、単純にOverする -*/
    else
      composeWithNoMotion(tile, frame, settings);
    return;
  }
  /*-  表示の範囲を得る -*/
  TRectD bBox =
      TRectD(tile.m_pos /*- Render画像上(Pixel単位)の位置 -*/
             ,
             TDimensionD(/*- Render画像上(Pixel単位)のサイズ -*/
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));

  /*- 上下左右のマージンを得る -*/
  double minX = 0.0;
  double maxX = 0.0;
  double minY = 0.0;
  double maxY = 0.0;
  for (int p = 0; p < points.size(); p++) {
    if (points.at(p).x > maxX) maxX = points.at(p).x;
    if (points.at(p).x < minX) minX = points.at(p).x;
    if (points.at(p).y > maxY) maxY = points.at(p).y;
    if (points.at(p).y < minY) minY = points.at(p).y;
  }
  int marginLeft   = (int)ceil(abs(minX));
  int marginRight  = (int)ceil(abs(maxX));
  int marginTop    = (int)ceil(abs(maxY));
  int marginBottom = (int)ceil(abs(minY));

  /*- 動かない（＝フィルタマージンが全て０）場合、入力タイルをそのまま返す -*/
  if (marginLeft == 0 && marginRight == 0 && marginTop == 0 &&
      marginBottom == 0) {
    if (!m_background.isConnected()) m_input->compute(tile, frame, settings);
    /*- 背景があり、前景が動かない場合、単純にOverする -*/
    else
      composeWithNoMotion(tile, frame, settings);
    return;
  }

  /*- マージンは、フィルタの上下左右を反転した寸法になる -*/
  TRectD enlargedBBox(bBox.x0 - (double)marginRight,
                      bBox.y0 - (double)marginTop, bBox.x1 + (double)marginLeft,
                      bBox.y1 + (double)marginBottom);

  // std::cout<<"Margin Left:"<<marginLeft<<" Right:"<<marginRight<<
  //	" Bottom:"<<marginBottom<<" Top:"<<marginTop<<std::endl;

  TDimensionI enlargedDimIn(/*- Pixel単位に四捨五入 -*/
                            (int)(enlargedBBox.getLx() + 0.5),
                            (int)(enlargedBBox.getLy() + 0.5));

  TTile enlarge_tile;
  m_input->allocateAndCompute(enlarge_tile, enlargedBBox.getP00(),
                              enlargedDimIn, tile.getRaster(), frame, settings);

  /*- 背景が必要な場合 -*/
  TTile back_Tile;
  if (m_background.isConnected()) {
    m_background->allocateAndCompute(back_Tile, tile.m_pos,
                                     tile.getRaster()->getSize(),
                                     tile.getRaster(), frame, settings);
  }

  //-------------------------------------------------------
  /*- 計算範囲 -*/
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TDimensionI filterDim(marginLeft + marginRight + 1,
                        marginTop + marginBottom + 1);

  /*- pointsTableの解放は各doCompute内でやっている -*/
  int pointAmount     = points.size();
  float4 *pointsTable = new float4[pointAmount];
  float dt = (float)(shutterStart + shutterEnd) / (float)traceResolution;
  for (int p = 0; p < pointAmount; p++) {
    pointsTable[p].x = (float)points.at(p).x;
    pointsTable[p].y = (float)points.at(p).y;
    /*- zにはp→p+1のベクトルの距離を格納 -*/
    if (p < pointAmount - 1) {
      float2 vec = {(float)(points.at(p + 1).x - points.at(p).x),
                    (float)(points.at(p + 1).y - points.at(p).y)};
      pointsTable[p].z = sqrtf(vec.x * vec.x + vec.y * vec.y);
    }
    /*- wにはシャッター時間のオフセットを格納 -*/
    pointsTable[p].w = -(float)shutterStart + (float)p * dt;
  }

  doCompute_CPU(tile, frame, settings, pointsTable, pointAmount, hardness,
                shutterStart, shutterEnd, traceResolution, startValue,
                startCurve, endValue, endCurve, marginLeft, marginRight,
                marginTop, marginBottom, enlargedDimIn, enlarge_tile, dimOut,
                filterDim, back_Tile);
}

//------------------------------------------------------------

void Iwa_MotionBlurCompFx::doCompute_CPU(
    TTile &tile, double frame, const TRenderSettings &settings,
    float4 *pointsTable, int pointAmount, double hardness, double shutterStart,
    double shutterEnd, int traceResolution, float startValue, float startCurve,
    float endValue, float endCurve, int marginLeft, int marginRight,
    int marginTop, int marginBottom, TDimensionI &enlargedDimIn,
    TTile &enlarge_tile, TDimensionI &dimOut, TDimensionI &filterDim,
    TTile &back_tile) {
  /*- 処理を行うメモリ -*/
  float4 *in_tile_p;  /*- マージンあり -*/
  float4 *out_tile_p; /*- マージンあり -*/
  /*- フィルタ -*/
  float *filter_p;

  /*- メモリ確保 -*/
  TRasterGR8P in_tile_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  in_tile_ras->lock();
  in_tile_p = (float4 *)in_tile_ras->getRawData();
  TRasterGR8P out_tile_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  out_tile_ras->lock();
  out_tile_p = (float4 *)out_tile_ras->getRawData();
  TRasterGR8P filter_ras(sizeof(float) * filterDim.lx, filterDim.ly);
  filter_ras->lock();
  filter_p = (float *)filter_ras->getRawData();

  bool sourceIsPremultiplied;
  /*- ソース画像を０〜１に正規化してメモリに読み込む -*/
  TRaster32P ras32 = (TRaster32P)enlarge_tile.getRaster();
  TRaster64P ras64 = (TRaster64P)enlarge_tile.getRaster();
  if (ras32)
    sourceIsPremultiplied = setSourceRaster<TRaster32P, TPixel32>(
        ras32, in_tile_p, enlargedDimIn,
        (PremultiTypes)m_premultiType->getValue());
  else if (ras64)
    sourceIsPremultiplied = setSourceRaster<TRaster64P, TPixel64>(
        ras64, in_tile_p, enlargedDimIn,
        (PremultiTypes)m_premultiType->getValue());

  /*- 残像モードがオフのとき -*/
  if (!m_zanzoMode->getValue()) {
    /*- フィルタをつくり、正規化する -*/
    makeMotionBlurFilter_CPU(filter_p, filterDim, marginLeft, marginBottom,
                             pointsTable, pointAmount, startValue, startCurve,
                             endValue, endCurve);
  }
  /*- 残像モードがオンのとき -*/
  else {
    /*- 残像フィルタをつくる/正規化する -*/
    makeZanzoFilter_CPU(filter_p, filterDim, marginLeft, marginBottom,
                        pointsTable, pointAmount, startValue, startCurve,
                        endValue, endCurve);
  }

  delete[] pointsTable;

  /*- RGB値(０〜１)をdepremultiply→露光値に変換→再びpremultiply -*/
  convertRGBtoExposure_CPU(in_tile_p, enlargedDimIn, hardness,
                           sourceIsPremultiplied);

  /*- 露光値をフィルタリングしてぼかす -*/
  applyBlurFilter_CPU(in_tile_p, out_tile_p, enlargedDimIn, filter_p, filterDim,
                      marginLeft, marginBottom, marginRight, marginTop, dimOut);
  /*- メモリ解放 -*/
  in_tile_ras->unlock();
  filter_ras->unlock();

  /*- 背景がある場合、Exposureの乗算を行う -*/
  if (m_background.isConnected()) {
    composeBackgroundExposure_CPU(out_tile_p, enlargedDimIn, marginRight,
                                  marginTop, back_tile, dimOut,
                                  (float)hardness);
  }
  /*- 露光値をdepremultipy→RGB値(０〜１)に戻す→premultiply -*/
  convertExposureToRGB_CPU(out_tile_p, enlargedDimIn, hardness);

  /*- ラスタのクリア -*/
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  int2 margin         = {marginRight, marginTop};
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(out_tile_p, outRas32, enlargedDimIn,
                                          margin);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(out_tile_p, outRas64, enlargedDimIn,
                                          margin);

  /*- メモリ解放 -*/
  out_tile_ras->unlock();
}

//------------------------------------------------------------

bool Iwa_MotionBlurCompFx::doGetBBox(double frame, TRectD &bBox,
                                     const TRenderSettings &info) {
  if (!m_input.isConnected() && !m_background.isConnected()) {
    bBox = TRectD();
    return false;
  }

  /*- 取り急ぎ、背景が繋がっていたら無限サイズにする -*/
  if (m_background.isConnected()) {
    bool _ret = m_background->doGetBBox(frame, bBox, info);
    bBox      = TConsts::infiniteRectD;
    return _ret;
  }

  bool ret = m_input->doGetBBox(frame, bBox, info);

  if (bBox == TConsts::infiniteRectD) return true;

  QList<TPointD> points = getAttributes()->getMotionPoints();
  /*- 移動した軌跡のバウンディングボックスからマージンを求める -*/
  /*- 各軌跡点の座標の絶対値の最大値を得る -*/
  /*- 上下左右のマージンを得る -*/
  double minX = 0.0;
  double maxX = 0.0;
  double minY = 0.0;
  double maxY = 0.0;
  for (int p = 0; p < points.size(); p++) {
    if (points.at(p).x > maxX) maxX = points.at(p).x;
    if (points.at(p).x < minX) minX = points.at(p).x;
    if (points.at(p).y > maxY) maxY = points.at(p).y;
    if (points.at(p).y < minY) minY = points.at(p).y;
  }
  int marginLeft   = (int)ceil(abs(minX));
  int marginRight  = (int)ceil(abs(maxX));
  int marginTop    = (int)ceil(abs(maxY));
  int marginBottom = (int)ceil(abs(minY));

  TRectD enlargedBBox(
      bBox.x0 - (double)marginLeft, bBox.y0 - (double)marginBottom,
      bBox.x1 + (double)marginRight, bBox.y1 + (double)marginTop);

  bBox = enlargedBBox;

  return ret;
}

//------------------------------------------------------------

bool Iwa_MotionBlurCompFx::canHandle(const TRenderSettings &info,
                                     double frame) {
  return true;
}

/*------------------------------------------------------------
 参考にしているオブジェクトが動いている可能性があるので、
 エイリアスは毎フレーム変える
------------------------------------------------------------*/

std::string Iwa_MotionBlurCompFx::getAlias(double frame,
                                           const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // alias degli effetti connessi alle porte di input separati da virgole
  // una porta non connessa da luogo a un alias vuoto (stringa vuota)
  int i;
  for (i = 0; i < getInputPortCount(); i++) {
    TFxPort *port = getInputPort(i);
    if (port->isConnected()) {
      TRasterFxP ifx = port->getFx();
      assert(ifx);
      alias += ifx->getAlias(frame, info);
    }
    alias += ",";
  }

  std::string paramalias("");
  for (i = 0; i < getParams()->getParamCount(); i++) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }
  unsigned long id = getIdentifier();
  return alias + std::to_string(frame) + "," + std::to_string(id) + paramalias +
         "]";
}

FX_PLUGIN_IDENTIFIER(Iwa_MotionBlurCompFx, "iwa_MotionBlurCompFx")
