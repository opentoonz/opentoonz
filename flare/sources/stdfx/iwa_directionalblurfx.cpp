/*------------------------------------
 Iwa_DirectionalBlurFx
 ボケ足の伸ばし方を選択でき、参照画像を追加した DirectionalBlur
//------------------------------------*/

#include "iwa_directionalblurfx.h"

#include "tparamuiconcept.h"

enum FILTER_TYPE { Linear = 0, Gaussian, Flat };

/*------------------------------------------------------------
 参照画像の輝度を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_DirectionalBlurFx::setReferenceRaster(const RASTER srcRas,
                                               float *dstMem, TDimensionI dim) {
  float *dst_p = dstMem;

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, dst_p++) {
      (*dst_p) = ((float)pix->r * 0.3f + (float)pix->g * 0.59f +
                  (float)pix->b * 0.11f) /
                 (float)PIXEL::maxChannelValue;
      // clamp
      (*dst_p) = std::min(1.f, std::max(0.f, (*dst_p)));
    }
  }
}

/*------------------------------------------------------------
 ソース画像を０〜１に正規化してホストメモリに読み込む
------------------------------------------------------------*/

template <typename RASTER, typename PIXEL>
void Iwa_DirectionalBlurFx::setSourceRaster(const RASTER srcRas, float4 *dstMem,
                                            TDimensionI dim) {
  float4 *chann_p = dstMem;

  for (int j = 0; j < dim.ly; j++) {
    PIXEL *pix = srcRas->pixels(j);
    for (int i = 0; i < dim.lx; i++, pix++, chann_p++) {
      (*chann_p).x = (float)pix->r / (float)PIXEL::maxChannelValue;
      (*chann_p).y = (float)pix->g / (float)PIXEL::maxChannelValue;
      (*chann_p).z = (float)pix->b / (float)PIXEL::maxChannelValue;
      (*chann_p).w = (float)pix->m / (float)PIXEL::maxChannelValue;
    }
  }
}

/*------------------------------------------------------------
 出力結果をChannel値に変換してタイルに格納
------------------------------------------------------------*/
template <typename RASTER, typename PIXEL>
void Iwa_DirectionalBlurFx::setOutputRaster(float4 *srcMem, const RASTER dstRas,
                                            TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    PIXEL *pix     = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++, pix++, chan_p++) {
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
    }
  }
}

template <>
void Iwa_DirectionalBlurFx::setOutputRaster<TRasterFP, TPixelF>(
    float4 *srcMem, const TRasterFP dstRas, TDimensionI dim, int2 margin) {
  int out_j = 0;
  for (int j = margin.y; j < dstRas->getLy() + margin.y; j++, out_j++) {
    TPixelF *pix   = dstRas->pixels(out_j);
    float4 *chan_p = srcMem;
    chan_p += j * dim.lx + margin.x;
    for (int i = 0; i < dstRas->getLx(); i++, pix++, chan_p++) {
      pix->r = (*chan_p).x;
      pix->g = (*chan_p).y;
      pix->b = (*chan_p).z;
      pix->m = std::min((*chan_p).w, 1.f);
    }
  }
}

//------------------------------------

Iwa_DirectionalBlurFx::Iwa_DirectionalBlurFx()
    : m_angle(0.0)
    , m_intensity(10.0)
    , m_bidirectional(false)
    , m_filterType(new TIntEnumParam(Linear, "Linear")) {
  m_intensity->setMeasureName("fxLength");
  m_angle->setMeasureName("angle");

  bindParam(this, "angle", m_angle);
  bindParam(this, "intensity", m_intensity);
  bindParam(this, "bidirectional", m_bidirectional);
  bindParam(this, "filterType", m_filterType);

  addInputPort("Source", m_input);
  addInputPort("Reference", m_reference);
  m_intensity->setValueRange(0, (std::numeric_limits<double>::max)());

  m_filterType->addItem(Gaussian, "Gaussian");
  m_filterType->addItem(Flat, "Flat");

  enableComputeInFloat(true);
}

//------------------------------------

void Iwa_DirectionalBlurFx::doCompute(TTile &tile, double frame,
                                      const TRenderSettings &settings) {
  /*- 接続していない場合は処理しない -*/
  if (!m_input.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  TPointD blurVector;
  double angle       = m_angle->getValue(frame) * M_PI_180;
  double intensity   = m_intensity->getValue(frame);
  bool bidirectional = m_bidirectional->getValue();

  blurVector.x = intensity * cos(angle);
  blurVector.y = intensity * sin(angle);

  double shrink = (settings.m_shrinkX + settings.m_shrinkY) / 2.0;

  /*- 平行移動成分を消したアフィン変換をかけ、ボケベクトルを変形 -*/
  TAffine aff = settings.m_affine;
  aff.a13 = aff.a23 = 0;
  TPointD blur      = (1.0 / shrink) * (aff * blurVector);

  /*- ボケの実際の長さを求める -*/
  double blurValue = norm(blur);
  /*- ボケの実際の長さがほぼ０なら入力画像をそのまま return -*/
  if (areAlmostEqual(blurValue, 0, 1e-3)) {
    m_input->compute(tile, frame, settings);
    return;
  }

  /*-  表示の範囲を得る -*/
  TRectD bBox =
      TRectD(tile.m_pos /*- Render画像上(Pixel単位)の位置  -*/
             ,
             TDimensionD(/*- Render画像上(Pixel単位)のサイズ  -*/
                         tile.getRaster()->getLx(), tile.getRaster()->getLy()));

  int marginH = (int)ceil(std::abs(blur.x));
  int marginV = (int)ceil(std::abs(blur.y));

  TRectD enlargedBBox(bBox.x0 - (double)marginH, bBox.y0 - (double)marginV,
                      bBox.x1 + (double)marginH, bBox.y1 + (double)marginV);

  TDimensionI enlargedDimIn(/*- Pixel単位に四捨五入 -*/
                            (int)(enlargedBBox.getLx() + 0.5),
                            (int)(enlargedBBox.getLy() + 0.5));

  TTile enlarge_tile;
  m_input->allocateAndCompute(enlarge_tile, enlargedBBox.getP00(),
                              enlargedDimIn, tile.getRaster(), frame, settings);

  /*- 参照画像が有ったら、メモリに取り込む -*/
  float *reference_host = 0;
  TRasterGR8P reference_host_ras;
  if (m_reference.isConnected()) {
    TTile reference_tile;
    m_reference->allocateAndCompute(reference_tile, enlargedBBox.getP00(),
                                    enlargedDimIn, tile.getRaster(), frame,
                                    settings);
    /*- ホストのメモリ確保 -*/
    reference_host_ras =
        TRasterGR8P(sizeof(float) * enlargedDimIn.lx, enlargedDimIn.ly);
    reference_host_ras->lock();
    reference_host = (float *)reference_host_ras->getRawData();
    /*- 参照画像の輝度を０〜１に正規化してホストメモリに読み込む -*/
    TRaster32P ras32 = (TRaster32P)reference_tile.getRaster();
    TRaster64P ras64 = (TRaster64P)reference_tile.getRaster();
    TRasterFP rasF   = (TRasterFP)reference_tile.getRaster();
    if (ras32)
      setReferenceRaster<TRaster32P, TPixel32>(ras32, reference_host,
                                               enlargedDimIn);
    else if (ras64)
      setReferenceRaster<TRaster64P, TPixel64>(ras64, reference_host,
                                               enlargedDimIn);
    else if (rasF)
      setReferenceRaster<TRasterFP, TPixelF>(rasF, reference_host,
                                             enlargedDimIn);
  }

  //-------------------------------------------------------
  /*- 計算範囲 -*/
  TDimensionI dimOut(tile.getRaster()->getLx(), tile.getRaster()->getLy());
  TDimensionI filterDim(2 * marginH + 1, 2 * marginV + 1);

  doCompute_CPU(tile, frame, settings, blur, bidirectional, marginH, marginH,
                marginV, marginV, enlargedDimIn, enlarge_tile, dimOut,
                filterDim, reference_host);

  /*-  参照画像が刺さっている場合、メモリを解放する -*/
  if (reference_host) {
    reference_host_ras->unlock();
    reference_host = 0;
  }
}

//------------------------------------

void Iwa_DirectionalBlurFx::doCompute_CPU(
    TTile &tile, double frame, const TRenderSettings &settings, TPointD &blur,
    bool bidirectional, int marginLeft, int marginRight, int marginTop,
    int marginBottom, TDimensionI &enlargedDimIn, TTile &enlarge_tile,
    TDimensionI &dimOut, TDimensionI &filterDim, float *reference_host) {
  /*- メモリ確保 -*/
  TRasterGR8P in_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  in_ras->lock();
  float4 *in = (float4 *)in_ras->getRawData();

  TRasterGR8P out_ras(sizeof(float4) * enlargedDimIn.lx, enlargedDimIn.ly);
  out_ras->lock();
  float4 *out = (float4 *)out_ras->getRawData();

  TRasterGR8P filter_ras(sizeof(float) * filterDim.lx, filterDim.ly);
  filter_ras->lock();
  float *filter = (float *)filter_ras->getRawData();

  /*- ソース画像を０〜１に正規化してホストメモリに読み込む -*/
  TRaster32P ras32 = (TRaster32P)enlarge_tile.getRaster();
  TRaster64P ras64 = (TRaster64P)enlarge_tile.getRaster();
  TRasterFP rasF   = (TRasterFP)enlarge_tile.getRaster();
  if (ras32)
    setSourceRaster<TRaster32P, TPixel32>(ras32, in, enlargedDimIn);
  else if (ras64)
    setSourceRaster<TRaster64P, TPixel64>(ras64, in, enlargedDimIn);
  else if (rasF)
    setSourceRaster<TRasterFP, TPixelF>(rasF, in, enlargedDimIn);

  /*- フィルタ作る -*/
  makeDirectionalBlurFilter_CPU(filter, blur, bidirectional, marginLeft,
                                marginRight, marginTop, marginBottom,
                                filterDim);

  if (reference_host) /*- 参照画像がある場合 -*/
  {
    float4 *out_p = out + marginTop * enlargedDimIn.lx;
    float *ref_p  = reference_host + marginTop * enlargedDimIn.lx;
    /*- フィルタリング -*/
    for (int y = marginTop; y < dimOut.ly + marginTop; y++) {
      out_p += marginRight;
      ref_p += marginRight;
      for (int x = marginRight; x < dimOut.lx + marginRight;
           x++, out_p++, ref_p++) {
        /*- 参照画像が黒ならソースをそのまま返す -*/
        if ((*ref_p) == 0.0f) {
          (*out_p) = in[y * enlargedDimIn.lx + x];
          continue;
        }

        /*- 値を積算する入れ物を用意 -*/
        float4 value = {0.0f, 0.0f, 0.0f, 0.0f};

        float *filter_p = filter;

        if ((*ref_p) == 1.0f) {
          /*- フィルタのサイズでループ
             ただし、フィルタはサンプル点の画像を収集するように
                  用いるため、上下左右反転してサンプルする -*/
          for (int fily = -marginBottom; fily < filterDim.ly - marginBottom;
               fily++) {
            /*- サンプル座標 と インデックス の このスキャンラインのうしろ -*/
            int2 samplePos  = {x + marginLeft, y - fily};
            int sampleIndex = samplePos.y * enlargedDimIn.lx + samplePos.x;

            for (int filx = -marginLeft; filx < filterDim.lx - marginLeft;
                 filx++, filter_p++, sampleIndex--) {
              /*- フィルター値が０またはサンプルピクセルが透明ならcontinue -*/
              if ((*filter_p) == 0.0f || in[sampleIndex].w == 0.0f) continue;
              /*- サンプル点の値にフィルタ値を掛けて積算する -*/
              value.x += in[sampleIndex].x * (*filter_p);
              value.y += in[sampleIndex].y * (*filter_p);
              value.z += in[sampleIndex].z * (*filter_p);
              value.w += in[sampleIndex].w * (*filter_p);
            }
          }
        } else {
          for (int fily = -marginBottom; fily < filterDim.ly - marginBottom;
               fily++) {
            for (int filx = -marginLeft; filx < filterDim.lx - marginLeft;
                 filx++, filter_p++) {
              /*- フィルター値が０ならcontinue -*/
              if ((*filter_p) == 0.0f) continue;
              /*- サンプル座標 -*/
              int2 samplePos  = {tround((float)x - (float)filx * (*ref_p)),
                                 tround((float)y - (float)fily * (*ref_p))};
              int sampleIndex = samplePos.y * enlargedDimIn.lx + samplePos.x;

              /*- サンプルピクセルが透明ならcontinue -*/
              if (in[sampleIndex].w == 0.0f) continue;

              /*- サンプル点の値にフィルタ値を掛けて積算する -*/
              value.x += in[sampleIndex].x * (*filter_p);
              value.y += in[sampleIndex].y * (*filter_p);
              value.z += in[sampleIndex].z * (*filter_p);
              value.w += in[sampleIndex].w * (*filter_p);
            }
          }
        }

        /*- 値を格納 -*/
        (*out_p) = value;
      }
      out_p += marginLeft;
      ref_p += marginLeft;
    }

  } else /*- 参照画像が無い場合 -*/
  {
    float4 *out_p = out + marginTop * enlargedDimIn.lx;
    /*- フィルタリング -*/
    for (int y = marginTop; y < dimOut.ly + marginTop; y++) {
      out_p += marginRight;
      for (int x = marginRight; x < dimOut.lx + marginRight; x++, out_p++) {
        /*- 値を積算する入れ物を用意 -*/
        float4 value = {0.0f, 0.0f, 0.0f, 0.0f};
        /*- フィルタのサイズでループ
           ただし、フィルタはサンプル点の画像を収集するように
                用いるため、上下左右反転してサンプルする -*/
        int filterIndex = 0;
        for (int fily = -marginBottom; fily < filterDim.ly - marginBottom;
             fily++) {
          /*- サンプル座標 と インデックス の このスキャンラインのうしろ -*/
          int2 samplePos  = {x + marginLeft, y - fily};
          int sampleIndex = samplePos.y * enlargedDimIn.lx + samplePos.x;

          for (int filx = -marginLeft; filx < filterDim.lx - marginLeft;
               filx++, filterIndex++, sampleIndex--) {
            /*- フィルター値が０またはサンプルピクセルが透明ならcontinue -*/
            if (filter[filterIndex] == 0.0f || in[sampleIndex].w == 0.0f)
              continue;
            /*- サンプル点の値にフィルタ値を掛けて積算する -*/
            value.x += in[sampleIndex].x * filter[filterIndex];
            value.y += in[sampleIndex].y * filter[filterIndex];
            value.z += in[sampleIndex].z * filter[filterIndex];
            value.w += in[sampleIndex].w * filter[filterIndex];
          }
        }

        /*- 値を格納 -*/
        (*out_p) = value;
      }
      out_p += marginLeft;
    }
  }

  in_ras->unlock();
  filter_ras->unlock();

  /*- ラスタのクリア -*/
  tile.getRaster()->clear();
  TRaster32P outRas32 = (TRaster32P)tile.getRaster();
  TRaster64P outRas64 = (TRaster64P)tile.getRaster();
  TRasterFP outRasF   = (TRasterFP)tile.getRaster();
  int2 margin         = {marginRight, marginTop};
  if (outRas32)
    setOutputRaster<TRaster32P, TPixel32>(out, outRas32, enlargedDimIn, margin);
  else if (outRas64)
    setOutputRaster<TRaster64P, TPixel64>(out, outRas64, enlargedDimIn, margin);
  else if (outRasF)
    setOutputRaster<TRasterFP, TPixelF>(out, outRasF, enlargedDimIn, margin);

  out_ras->unlock();
}

//------------------------------------

void Iwa_DirectionalBlurFx::makeDirectionalBlurFilter_CPU(
    float *filter, TPointD &blur, bool bidirectional, int marginLeft,
    int marginRight, int marginTop, int marginBottom, TDimensionI &filterDim) {
  /*- 必要なら、ガウスフィルタを前計算 -*/
  FILTER_TYPE filterType = (FILTER_TYPE)m_filterType->getValue();
  std::vector<float> gaussian;
  if (filterType == Gaussian) {
    gaussian.reserve(101);
    for (int g = 0; g < 101; g++) {
      float x = (float)g / 100.0f;
      // sigma == 0.25
      gaussian.push_back(exp(-x * x / 0.125f));
    }
  }

  /*- フィルタを作る -*/
  TPointD p0 =
      (bidirectional) ? TPointD(-blur.x, -blur.y) : TPointD(0.0f, 0.0f);
  TPointD p1       = blur;
  float2 vec_p0_p1 = {static_cast<float>(p1.x - p0.x),
                      static_cast<float>(p1.y - p0.y)};

  float *fil_p        = filter;
  float intensity_sum = 0.0f;

  for (int fy = -marginBottom; fy <= marginTop; fy++) {
    for (int fx = -marginLeft; fx <= marginRight; fx++, fil_p++) {
      /*- 現在の座標とブラー直線の距離を求める -*/
      /*- P0->サンプル点とP0->P1の内積を求める -*/
      float2 vec_p0_sample = {static_cast<float>(fx - p0.x),
                              static_cast<float>(fy - p0.y)};
      float dot = vec_p0_sample.x * vec_p0_p1.x + vec_p0_sample.y * vec_p0_p1.y;
      /*- 軌跡ベクトルの長さの２乗を計算する -*/
      float length2 = vec_p0_p1.x * vec_p0_p1.x + vec_p0_p1.y * vec_p0_p1.y;

      /*- 距離の2乗を求める -*/
      float dist2;
      float framePosRatio;
      /*- P0より手前にある場合 -*/
      if (dot <= 0.0f) {
        dist2 = vec_p0_sample.x * vec_p0_sample.x +
                vec_p0_sample.y * vec_p0_sample.y;
        framePosRatio = 0.0f;
      } else {
        /*- P0〜P1間にある場合 -*/
        if (dot < length2) {
          float p0_sample_dist2 = vec_p0_sample.x * vec_p0_sample.x +
                                  vec_p0_sample.y * vec_p0_sample.y;
          dist2         = p0_sample_dist2 - dot * dot / length2;
          framePosRatio = dot / length2;
        }
        /*- P1より先にある場合 -*/
        else {
          float2 vec_p1_sample = {static_cast<float>(fx - p1.x),
                                  static_cast<float>(fy - p1.y)};
          dist2                = vec_p1_sample.x * vec_p1_sample.x +
                  vec_p1_sample.y * vec_p1_sample.y;
          framePosRatio = 1.0f;
        }
      }
      /*- 距離が(√2 + 1)/2より遠かったらreturn dist2との比較だから2乗している
       * -*/
      if (dist2 > 1.4571f) {
        (*fil_p) = 0.0f;
        continue;
      }

      /*-
         現在のピクセルのサブピクセル(16*16)が、近傍ベクトルからの距離0.5の範囲にどれだけ
               含まれているかをカウントする -*/
      int count = 0;

      for (int yy = 0; yy < 16; yy++) {
        /*- サブピクセルのY座標 -*/
        float subPosY = (float)fy + ((float)yy - 7.5f) / 16.0f;
        for (int xx = 0; xx < 16; xx++) {
          /*- サブピクセルのX座標 -*/
          float subPosX = (float)fx + ((float)xx - 7.5f) / 16.0f;

          float2 vec_p0_sub = {static_cast<float>(subPosX - p0.x),
                               static_cast<float>(subPosY - p0.y)};
          float sub_dot =
              vec_p0_sub.x * vec_p0_p1.x + vec_p0_sub.y * vec_p0_p1.y;
          /*- 距離の2乗を求める -*/
          float dist2;
          /*- P0より手前にある場合 -*/
          if (sub_dot <= 0.0f)
            dist2 = vec_p0_sub.x * vec_p0_sub.x + vec_p0_sub.y * vec_p0_sub.y;
          else {
            /*- P0〜P1間にある場合 -*/
            if (sub_dot < length2) {
              float p0_sub_dist2 =
                  vec_p0_sub.x * vec_p0_sub.x + vec_p0_sub.y * vec_p0_sub.y;
              dist2 = p0_sub_dist2 - sub_dot * sub_dot / length2;
            }
            /*-  P1より先にある場合 -*/
            else {
              float2 vec_p1_sub = {static_cast<float>(subPosX - p1.x),
                                   static_cast<float>(subPosY - p1.y)};
              dist2 = vec_p1_sub.x * vec_p1_sub.x + vec_p1_sub.y * vec_p1_sub.y;
            }
          }
          /*- 距離の２乗が0.25より近ければカウントをインクリメント -*/
          if (dist2 <= 0.25f) count++;
        }
      }
      /*- 保険 カウントが０の場合はフィールド値０でreturn -*/
      if (count == 0) {
        (*fil_p) = 0.0f;
        continue;
      }
      /*- countは Max256 -*/
      float countRatio = (float)count / 256.0f;

      /*- オフセット値を求める -*/
      float offset =
          (bidirectional) ? std::abs(framePosRatio * 2.0 - 1.0) : framePosRatio;

      /*- フィルタごとに分ける -*/
      float bokeAsiVal;
      switch (filterType) {
      case Linear:
        bokeAsiVal = 1.0f - offset;
        break;
      case Gaussian: {
        int index   = (int)floor(offset * 100.0f);
        float ratio = offset * 100.0f - (float)index;
        bokeAsiVal  = (ratio == 0.f) ? gaussian[index]
                                     : gaussian[index] * (1.0f - ratio) +
                                          gaussian[index + 1] * ratio;
      } break;
      case Flat:
        bokeAsiVal = 1.0f;
        break;
      default:
        bokeAsiVal = 1.0f - offset;
        break;
      }

      /*- フィールド値の格納 -*/
      (*fil_p) = bokeAsiVal * countRatio;
      intensity_sum += (*fil_p);
    }
  }

  /*- 正規化 -*/
  fil_p = filter;
  for (int f = 0; f < filterDim.lx * filterDim.ly; f++, fil_p++) {
    if ((*fil_p) == 0.0f) continue;
    (*fil_p) /= intensity_sum;
  }
}

//------------------------------------

bool Iwa_DirectionalBlurFx::doGetBBox(double frame, TRectD &bBox,
                                      const TRenderSettings &info) {
  if (false == this->m_input.isConnected()) {
    bBox = TRectD();
    return false;
  }

  bool ret = m_input->doGetBBox(frame, bBox, info);

  if (bBox == TConsts::infiniteRectD) return ret;

  TPointD blur;
  double angle       = m_angle->getValue(frame) * M_PI_180;
  double intensity   = m_intensity->getValue(frame);
  bool bidirectional = m_bidirectional->getValue();

  blur.x = intensity * cos(angle);
  blur.y = intensity * sin(angle);

  int marginH = (int)ceil(std::abs(blur.x));
  int marginV = (int)ceil(std::abs(blur.y));

  TRectD enlargedBBox(bBox.x0 - (double)marginH, bBox.y0 - (double)marginV,
                      bBox.x1 + (double)marginH, bBox.y1 + (double)marginV);

  bBox = enlargedBBox;

  return ret;
}

//------------------------------------

bool Iwa_DirectionalBlurFx::canHandle(const TRenderSettings &info,
                                      double frame) {
  return isAlmostIsotropic(info.m_affine) || m_intensity->getValue(frame) == 0;
}

//------------------------------------

void Iwa_DirectionalBlurFx::getParamUIs(TParamUIConcept *&concepts,
                                        int &length) {
  concepts = new TParamUIConcept[length = 1];

  concepts[0].m_type  = TParamUIConcept::POLAR;
  concepts[0].m_label = "Angle and Intensity";
  concepts[0].m_params.push_back(m_angle);
  concepts[0].m_params.push_back(m_intensity);
}

//------------------------------------

FX_PLUGIN_IDENTIFIER(Iwa_DirectionalBlurFx, "iwa_DirectionalBlurFx")
