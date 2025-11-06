#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"

#include "naru_graph.h"

#include "tparamset.h"
#include "trasterfx.h"
#include "tpixelutils.h"

class naru_lazybrush final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(naru_lazybrush)

  TRasterFxPort m_input;
  TRasterFxPort m_ref;

  TIntEnumParamP m_mode;
  TPixelParamP m_maskcolor;
  TIntEnumParamP m_scrtype;

  TDoubleParamP m_minlightness;
  TDoubleParamP m_logs;
  TDoubleParamP m_sigma;
  TDoubleParamP m_lambda;
  TDoubleParamP m_alpha;
  TDoubleParamP m_autoscrlen;
  TDoubleParamP m_autoscrthresh;
  TDoubleParamP m_lineweight;
  TBoolParamP m_fillHole;
  TIntEnumParamP m_sinkpos;
  TBoolParamP m_autoScribble;

public:
  naru_lazybrush()
      : m_mode(new TIntEnumParam(0, "Mask+Line"))
      , m_maskcolor(TPixel32(200, 200, 200, 255))
      , m_scrtype(new TIntEnumParam(0, "Soft"))
      , m_minlightness(0.02f)
      , m_logs(0.05f)
      , m_sigma(1.5f)
      , m_lambda(0.6f)
      , m_alpha(100.f)
      , m_autoscrlen(4.f)
      , m_autoscrthresh(0.1f)
      , m_lineweight(4.f)
      , m_fillHole(true)
      , m_sinkpos(new TIntEnumParam(0, "All"))
      , m_autoScribble(true) {
    bindParam(this, "mode", m_mode);
    bindParam(this, "mask_color", m_maskcolor);
    bindParam(this, "scr_type", m_scrtype);

    bindParam(this, "min_lightness", m_minlightness);
    bindParam(this, "log_scale", m_logs);
    bindParam(this, "sigma", m_sigma);
    bindParam(this, "lambda", m_lambda);
    bindParam(this, "alpha", m_alpha);
    bindParam(this, "auto_scribble_length", m_autoscrlen);
    bindParam(this, "auto_scribble_threshold", m_autoscrthresh);
    bindParam(this, "line_weight", m_lineweight);
    bindParam(this, "undef_is_sink", m_fillHole);
    bindParam(this, "sink_pos", m_sinkpos);
    bindParam(this, "auto_scribble", m_autoScribble);

    this->m_mode->addItem(1, "Mask");
    this->m_mode->addItem(2, "LoG Filter");
    this->m_mode->addItem(3, "Capacity Map");
    this->m_mode->addItem(4, "Scribble Map");
    this->m_scrtype->addItem(1, "Hard");

    this->m_sinkpos->addItem(1, "Bottom-Left");
    this->m_sinkpos->addItem(2, "Bottom-Center");
    this->m_sinkpos->addItem(3, "Bottom-Right");
    this->m_sinkpos->addItem(4, "Center-Right");
    this->m_sinkpos->addItem(5, "Top-Right");
    this->m_sinkpos->addItem(6, "Top-Center");
    this->m_sinkpos->addItem(7, "Top-Left");
    this->m_sinkpos->addItem(8, "Center-Left");

    this->m_minlightness->setValueRange(0.f, 1.f);
    this->m_logs->setValueRange(0.f, 5.f);
    this->m_sigma->setValueRange(0.f, 5.f);
    this->m_lambda->setValueRange(0.5f, 5.f);
    this->m_alpha->setValueRange(1.f, 10000.f);
    this->m_autoscrlen->setValueRange(1.f, 10.f);
    this->m_autoscrthresh->setValueRange(0.f, 1.f);

    addInputPort("Source", m_input);
    addInputPort("Reference", m_ref);
    enableComputeInFloat(true);
  }

  ~naru_lazybrush() {};

  bool doGetBBox(double frame, TRectD& bBox,
                 const TRenderSettings& info) override {
    if (m_input.isConnected()) {
      bBox = TConsts::infiniteRectD;
      return true;
    } else {
      bBox = TRectD();
      return false;
    }
  };

  void doCompute(TTile& tile, double frame, const TRenderSettings&) override;

  bool canHandle(const TRenderSettings& info, double frame) override {
    return false;
  }

private:
  const float gauKernel[3][3] = {
      {1, 2, 1},
      {2, 4, 2},
      {1, 2, 1},
  };
  const float weightSum       = 16.f;
  const float lapKernel[3][3] = {
      {0, 1, 0},
      {1, -4, 1},
      {0, 1, 0},
  };

  int mode = 0;       // 0:Mask+Line, 1:Mask, 2:LoG Filter, 3:Scribble Map
  TPixelF maskColor;  // マスクの色

  const float LoG_draw_scale = 20.f;   // LoGフィルタの描画スケール
  float LoG_s                = 0.05f;  // LoGフィルタのスケール

  int idx(int x, int y, int w) { return y * w + x; }

  //-------------------------------------------------------------------

  template <typename PIXEL>
  void doDraw(TRasterPT<PIXEL> ras, std::vector<float>& r,
              std::vector<float>& g, std::vector<float>& b,
              std::vector<float>& a);
  template <typename PIXEL>
  void doGrayScale(TRasterPT<PIXEL> ras, double frame,
                   std::vector<float>& temp);
  template <typename PIXEL>
  void doLoG(TRasterPT<PIXEL> ras, double frame, std::vector<float>& gray,
             std::vector<float>& lap);
  template <typename PIXEL>
  void doGraph(TRasterPT<PIXEL> ras, double frame, TRasterPT<PIXEL> refRas,
               bool refer_sw, std::vector<float>& lap, Graph& g);
  template <typename PIXEL>
  void doColorize(TRasterPT<PIXEL> ras, double frame, Graph& g,
                  std::vector<float>& gray);
  template <typename PIXEL>
  void process(TRasterPT<PIXEL> ras, double frame, TRasterPT<PIXEL> refRas,
               bool refer_sw);
};

template <typename PIXEL>
void naru_lazybrush::doDraw(TRasterPT<PIXEL> ras, std::vector<float>& r,
                            std::vector<float>& g, std::vector<float>& b,
                            std::vector<float>& a) {
  int width  = ras->getLx();
  int height = ras->getLy();

  ras->lock();
  for (int y = 0; y < height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < width; ++x) {
      int p  = idx(x, y, width);
      pix->r = (typename PIXEL::Channel)(maskColor.m * maskColor.r *
                                         fmin(r[p], 1.f) *
                                         (float)PIXEL::maxChannelValue);
      pix->g = (typename PIXEL::Channel)(maskColor.m * maskColor.g *
                                         fmin(g[p], 1.f) *
                                         (float)PIXEL::maxChannelValue);
      pix->b = (typename PIXEL::Channel)(maskColor.m * maskColor.b *
                                         fmin(b[p], 1.f) *
                                         (float)PIXEL::maxChannelValue);
      pix->m = (typename PIXEL::Channel)(maskColor.m * fmin(a[p], 1.f) *
                                         (float)PIXEL::maxChannelValue);
      pix++;
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void naru_lazybrush::doGrayScale(TRasterPT<PIXEL> ras, double frame,
                                 std::vector<float>& gray) {
  int width  = ras->getLx();
  int height = ras->getLy();

  float minLightness = m_minlightness->getValue(frame);

  ras->lock();
  for (int y = 0; y < height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < width; ++x) {
      float m = (float)pix->m / (float)PIXEL::maxChannelValue;
      if (m != 0) {
        float r = (float)pix->r / (m * (float)PIXEL::maxChannelValue);
        float g = (float)pix->g / (m * (float)PIXEL::maxChannelValue);
        float b = (float)pix->b / (m * (float)PIXEL::maxChannelValue);
        gray[idx(x, y, width)] =
            fmax(0.299f * r + 0.587f * g + 0.114f * b, minLightness);
      } else {
        gray[idx(x, y, width)] = 1.0f;
      }
      pix++;
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void naru_lazybrush::doLoG(TRasterPT<PIXEL> ras, double frame,
                           std::vector<float>& gray, std::vector<float>& lap) {
  int width  = ras->getLx();
  int height = ras->getLy();

  // ガウシアンフィルタ
  std::vector<float> blurred(width * height, 0.0f);
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      float sum = 0.f;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          sum += gray[idx(x + dx, y + dy, width)] * gauKernel[dy + 1][dx + 1];
        }
      }
      blurred[idx(x, y, width)] = sum / weightSum;
    }
  }

  // 境界値を設定
  for (int x = 0; x < width; ++x) {
    blurred[idx(x, 0, width)]          = blurred[idx(x, 1, width)];
    blurred[idx(x, height - 1, width)] = blurred[idx(x, height - 2, width)];
  }
  for (int y = 0; y < height; ++y) {
    blurred[idx(0, y, width)]         = blurred[idx(1, y, width)];
    blurred[idx(width - 1, y, width)] = blurred[idx(width - 2, y, width)];
  }

  // ラプラシアンフィルタ
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      float sum = 0.f;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          sum +=
              blurred[idx(x + dx, y + dy, width)] * lapKernel[dy + 1][dx + 1];
        }
      }
      lap[idx(x, y, width)] = fmax(LoG_s * sum, 0.0f);
    }
  }

  for (int x = 0; x < width; ++x) {
    lap[idx(x, 0, width)]          = lap[idx(x, 1, width)];
    lap[idx(x, height - 1, width)] = lap[idx(x, height - 2, width)];
  }
  for (int y = 0; y < height; ++y) {
    lap[idx(0, y, width)]         = lap[idx(1, y, width)];
    lap[idx(width - 1, y, width)] = lap[idx(width - 2, y, width)];
  }
}

template <typename PIXEL>
void naru_lazybrush::doGraph(TRasterPT<PIXEL> ras, double frame,
                             TRasterPT<PIXEL> refRas, bool refer_sw,
                             std::vector<float>& lap, Graph& g) {
  int width   = ras->getLx();
  int height  = ras->getLy();
  int rasSize = width * height;

  // LoG to intensity
  std::vector<float> intensity(rasSize, 0.0f);
  float K = 2.f * (width + height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int p        = idx(x, y, width);
      intensity[p] = K * lap[p] + 1.f;
      lap[p]       = lap[p] * LoG_draw_scale;
    }
  }

  // set capasity
  std::vector<float> weights(rasSize, 0.0f);
  std::vector<float> capacity(rasSize, 0.0f);
  float alpha        = m_alpha->getValue(frame);
  float sigma        = m_sigma->getValue(frame);
  float inv_sigmaSq2 = 1.0 / (2 * sigma * sigma);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int p = idx(x, y, width);
      if (x < width - 1) {
        int q = idx(x + 1, y, width);
        float weight =
            alpha * exp(-(intensity[p] + intensity[q]) * inv_sigmaSq2);
        g.addEdge(p, q, weight, weight);
      }
      if (y < height - 1) {
        int q = idx(x, y + 1, width);
        float weight =
            alpha * exp(-(intensity[p] + intensity[q]) * inv_sigmaSq2);
        g.addEdge(p, q, weight, weight);
      }
      weights[p]  = intensity[p] / (K * LoG_s);
      capacity[p] = exp(-intensity[p] * inv_sigmaSq2);
    }
  }

  // Scribble
  std::vector<float> refR(rasSize, 0.0f);
  std::vector<float> refG(rasSize, 0.0f);
  std::vector<float> refB(rasSize, 0.0f);
  std::vector<float> scribbleR(rasSize, 0.0f);
  std::vector<float> scribbleB(rasSize, 0.0f);
  float lambda       = m_lambda->getValue(frame);
  float softCapacity = floorf(lambda * alpha);
  int scribbleType = m_scrtype->getValue();  // スクリブルタイプ 0:Soft, 1:Hard
  int tLinkCap     = scribbleType == 0 ? softCapacity : K;
  int sLinkCap     = scribbleType == 0 ? alpha - softCapacity : 0;
  // Exist Reference
  if (refer_sw) {
    refRas->lock();
    for (int y = 0; y < height; ++y) {
      PIXEL* pix = refRas->pixels(y);
      for (int x = 0; x < width; ++x) {
        int p   = idx(x, y, width);
        refR[p] = (float)pix->r / (float)PIXEL::maxChannelValue;
        refG[p] = (float)pix->g / (float)PIXEL::maxChannelValue;
        refB[p] = (float)pix->b / (float)PIXEL::maxChannelValue;
        pix++;
      }
    }
    refRas->unlock();
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int p = idx(x, y, width);
        if (refR[p] > 0.5f && refG[p] < 0.5f && refB[p] < 0.5f) {
          g.addTerminal(p, tLinkCap - sLinkCap);
          scribbleR[p] = 1.f;
        } else if (refR[p] < 0.5f && refG[p] < 0.5f && refB[p] > 0.5f) {
          g.addTerminal(p, sLinkCap - tLinkCap);
          scribbleB[p] = 1.f;
        }
      }
    }
  }

  float autoScribbleLength    = m_autoscrlen->getValue(frame);
  float autoScribbleThreshold = m_autoscrthresh->getValue(frame);
  float lineWeight            = m_lineweight->getValue(frame);
  int sinkPos                 = m_sinkpos->getValue();
  bool autoScribble           = m_autoScribble->getValue();
  if (autoScribble) {
    // Auto Scribble
    for (int i = 0; i < width + height; ++i) {
      float fcx0, fcx1, fcy0, fcy1, dx, dy;
      if (i < width) {
        fcx0 = i;
        fcy0 = 0;
        fcx1 = i;
        fcy1 = height - 1;
        dx   = 0;
        dy   = 1;
      } else {
        fcx0 = 0;
        fcy0 = i - width + 1;
        fcx1 = width - 1;
        fcy1 = i - width + 1;
        dx   = 1;
        dy   = 0;
      }
      bool onLine, pOnLine;
      onLine  = false;
      pOnLine = false;
      fcx0 += dx * autoScribbleLength;
      fcy0 += dy * autoScribbleLength;
      for (int i = 0; i < 10000; ++i) {
        fcx0 += dx;
        fcy0 += dy;
        int cx = fcx0;
        int cy = fcy0;
        if (cx < 0 || cx >= width - 1 || cy < 0 || cy >= height - 1) break;
        int cp = idx(cx, cy, width);
        if (weights[cp] > autoScribbleThreshold) {
          fcx0 += dx * lineWeight;
          fcy0 += dy * lineWeight;
          onLine = true;
        } else
          onLine = false;
        if (!onLine && pOnLine) {
          g.addTerminal(cp, tLinkCap - sLinkCap);
          scribbleR[cp] = 1.f;
          break;
        }
        pOnLine = onLine;
      }

      onLine  = false;
      pOnLine = false;
      fcx1 -= dx * autoScribbleLength;
      fcy1 -= dy * autoScribbleLength;
      for (int i = 0; i < 10000; ++i) {
        fcx1 -= dx;
        fcy1 -= dy;
        int cx = fcx1;
        int cy = fcy1;
        if (cx < 0 || cx >= width - 1 || cy < 0 || cy >= height - 1) break;
        int cp = idx(cx, cy, width);
        if (weights[cp] > autoScribbleThreshold) {
          fcx1 -= dx * lineWeight;
          fcy1 -= dy * lineWeight;
          onLine = true;
        } else
          onLine = false;
        if (!onLine && pOnLine) {
          g.addTerminal(cp, tLinkCap - sLinkCap);
          scribbleR[cp] = 1.f;
          break;
        }
        pOnLine = onLine;
      }
    }
  }

  // 境界値を設定
  int bdSize     = 2 * (width + height - 2);
  int initInds[] = {0,
                    width / 2,
                    width,
                    width + height / 2,
                    width + height,
                    width * 3 / 2 + height,
                    width * 2 + height,
                    width * 2 + height * 3 / 2};
  std::vector<int> bdIdx(bdSize, false);
  // 1 "Bottom-Left"
  // 2 "Bottom-Center"
  // 3 "Bottom-Right"
  // 4 "Center-Right"
  // 5 "Top-Right"
  // 6 "Top-Center"
  // 7 "Top-Left"
  // 8 "Center-Left"
  for (int i = 0; i < bdSize; ++i) {
    int p;
    if (i < width - 1)
      p = idx(i, 0, width);
    else if (i < width + height - 2)
      p = idx(width - 1, i - width + 1, width);
    else if (i < 2 * width + height - 3)
      p = idx(width - 1 - (i - width - height + 2), height - 1, width);
    else
      p = idx(0, height - 1 - (i - 2 * width - height + 3), width);
    bdIdx[i] = p;
  }

  if (sinkPos == 0) {
    for (int i = 0; i < bdSize; ++i) {
      int p = bdIdx[i];
      g.addTerminal(p, -K);
      scribbleB[p] = 1.f;
    }
  } else {
    int initIdx = initInds[(sinkPos - 1)];
    bool inside = false;
    for (int i = 0; i < bdSize; ++i) {
      int p = bdIdx[(initIdx + i + bdSize) % bdSize];
      if (scribbleR[p] == 1.f) {
        inside = true;
        break;
      }
      g.addTerminal(p, -K);
      scribbleB[p] = 1.f;
    }
    if (inside) {
      for (int i = 0; i < bdSize; ++i) {
        int p = bdIdx[(initIdx - i + bdSize) % bdSize];
        if (scribbleR[p] == 1.f) break;
        g.addTerminal(p, -K);
        scribbleB[p] = 1.f;
      }
    }
  }

  std::vector<float> drawZero(rasSize, 0.0f);
  std::vector<float> drawOne(rasSize, 1.0f);
  if (mode == 2) {  // Draw LoG Filter
    doDraw(ras, lap, lap, lap, drawOne);
  } else if (mode == 3) {  // Draw Capacity Map
    doDraw(ras, capacity, capacity, capacity, drawOne);
  } else if (mode == 4) {  // Draw Scribble Map
    doDraw(ras, scribbleR, drawZero, scribbleB, drawOne);
  }
}

template <typename PIXEL>
void naru_lazybrush::doColorize(TRasterPT<PIXEL> ras, double frame, Graph& g,
                                std::vector<float>& gray) {
  int width  = ras->getLx();
  int height = ras->getLy();

  bool fillHole = m_fillHole->getValue();
  g.mincut();
  std::vector<float> maskR(width * height, 0.0f);
  std::vector<float> maskG(width * height, 0.0f);
  std::vector<float> maskB(width * height, 0.0f);
  std::vector<float> maskM(width * height, 0.0f);
  // Mask
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int p = idx(x, y, width);
      if (!g.getSegment(p, fillHole ? g.SOURCE : g.SINK)) {  // SOURCE
        maskR[p] = maskColor.m * maskColor.r;
        maskG[p] = maskColor.m * maskColor.g;
        maskB[p] = maskColor.m * maskColor.b;
        maskM[p] = maskColor.m;
      }
    }
  }

  std::vector<float> lineR(width * height, 0.0f);
  std::vector<float> lineG(width * height, 0.0f);
  std::vector<float> lineB(width * height, 0.0f);
  std::vector<float> lineM(width * height, 0.0f);
  // Line
  if (mode == 0) {
    // line => (r, g, b, 1.), noLine => (r, g, b, 0.)
    ras->lock();
    for (int y = 0; y < height; ++y) {
      PIXEL* pix = ras->pixels(y);
      for (int x = 0; x < width; ++x) {
        int p   = idx(x, y, width);
        float m = (float)pix->m / (float)PIXEL::maxChannelValue;
        if (m != 0) {
          float r  = (float)pix->r / (float)PIXEL::maxChannelValue;
          float g  = (float)pix->g / (float)PIXEL::maxChannelValue;
          float b  = (float)pix->b / (float)PIXEL::maxChannelValue;
          lineM[p] = (1.f - fmin(fmin(r, g), b)) * m;
          lineR[p] = r;
          lineG[p] = g;
          lineB[p] = b;
        }
        pix++;
      }
    }
    ras->unlock();
  }
  ras->lock();

  // result => line + mask
  for (int y = 0; y < height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < width; ++x) {
      int p  = idx(x, y, width);
      pix->r = (typename PIXEL::Channel)(
          (maskR[p] * (1.f - lineM[p]) + lineM[p] * lineR[p]) *
          (float)PIXEL::maxChannelValue);
      pix->g = (typename PIXEL::Channel)(
          (maskG[p] * (1.f - lineM[p]) + lineM[p] * lineG[p]) *
          (float)PIXEL::maxChannelValue);
      pix->b = (typename PIXEL::Channel)(
          (maskB[p] * (1.f - lineM[p]) + lineM[p] * lineB[p]) *
          (float)PIXEL::maxChannelValue);
      pix->m = (typename PIXEL::Channel)(fmax(maskM[p], lineM[p]) *
                                         (float)PIXEL::maxChannelValue);
      pix++;
    }
    ras->unlock();
  }
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void naru_lazybrush::process(TRasterPT<PIXEL> ras, double frame,
                             TRasterPT<PIXEL> refRas, bool refer_sw) {
  int width   = ras->getLx();
  int height  = ras->getLy();
  int rasSize = width * height;
  std::vector<float> gray(rasSize);
  std::vector<float> lap(rasSize);

  // params
  mode      = m_mode->getValue();
  maskColor = toPixelF(m_maskcolor->getValueD(frame));
  LoG_s     = m_logs->getValue(frame);

  // create graph
  Graph g = Graph(rasSize);

  doGrayScale<PIXEL>(ras, frame, gray);
  doLoG<PIXEL>(ras, frame, gray, lap);
  doGraph<PIXEL>(ras, frame, refRas, refer_sw, lap, g);
  if (mode == 0 || mode == 1) {
    doColorize<PIXEL>(ras, frame, g, gray);
  }
}

void naru_lazybrush::doCompute(TTile& tile, double frame,
                               const TRenderSettings& ri) {
  if (!m_input.isConnected()) return;

  // 1. 元画像全域のBBoxを取得
  TRectD fullBBox;
  m_input->getBBox(frame, fullBBox, ri);

  // 2. 元画像全体ラスタを確保
  TDimension size(0, 0);
  size.lx = tceil(fullBBox.getLx());
  size.ly = tceil(fullBBox.getLy());
  TTile fullTile;
  m_input->allocateAndCompute(fullTile, fullBBox.getP00(), size,
                              tile.getRaster(), frame, ri);
  TRasterP fullRas = fullTile.getRaster();

  // 3. グラフ処理をフルラスタで実行
  TRasterP refRas;
  bool refer_sw = false;
  if (m_ref.isConnected()) {
    refer_sw = true;
    TTile refTile;
    m_ref->allocateAndCompute(refTile, fullBBox.getP00(), size, fullRas, frame,
                              ri);
    refRas = refTile.getRaster();
  }

  if (TRaster32P ras32 = fullRas)
    process<TPixel32>(ras32, frame, refRas, refer_sw);
  else if (TRaster64P ras64 = fullRas)
    process<TPixel64>(ras64, frame, refRas, refer_sw);
  else if (TRasterFP rasF = fullRas)
    process<TPixelF>(rasF, frame, refRas, refer_sw);
  else
    throw TException("unsupported Pixel Type");

  // 4. tile に結果をコピー
  TRasterP tileRas      = tile.getRaster();
  TPoint startTilingPos = convert(fullTile.m_pos - tile.m_pos);
  tileRas->copy(fullRas, startTilingPos);
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(naru_lazybrush, "naru_LazyBrushFx")
