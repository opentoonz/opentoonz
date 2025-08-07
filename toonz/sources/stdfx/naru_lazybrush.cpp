#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"

#include "naru_graph.h"

#include "tparamset.h"
#include "globalcontrollablefx.h"
#include "tpixelutils.h"

class naru_lazybrush final : public GlobalControllableFx {
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
  TBoolParamP m_fillHole;

public:
  naru_lazybrush()
      : m_mode(new TIntEnumParam(0, "Mask+Line"))
      , m_maskcolor(TPixel32(200, 200, 200, 255))
      , m_scrtype(new TIntEnumParam(0, "Soft"))
      , m_minlightness(0.02f)
      , m_logs(0.05f)
      , m_sigma(1.0f)
      , m_lambda(0.9f)
      , m_alpha(100.f)
      , m_autoscrlen(4.f)
      , m_autoscrthresh(0.1f)
      , m_fillHole(true) {
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
    bindParam(this, "undef_is_sink", m_fillHole);

    this->m_mode->addItem(1, "Mask");
    this->m_mode->addItem(2, "LoG Filter");
    this->m_mode->addItem(3, "Capacity Map");
    this->m_mode->addItem(4, "Scribble Map");
    this->m_scrtype->addItem(1, "Hard");

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
      m_input->doGetBBox(frame, bBox, info);
      return true;
    }

    return false;
  };

  void doCompute(TTile& tile, double frame, const TRenderSettings&) override;

  bool canHandle(const TRenderSettings& info, double frame) override {
    return false;
  }
};

//-------------------------------------------------------------------

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

int mode = 0;  // 0:Mask, 1:Mask+Line, 2:LoG Filter, 3:Scribble Map
TPixelF maskColor =
    TPixelF(200.f / 255.f, 200.f / 255.f, 200.f / 255.f, 1.f);  // マスクの色
int scribbleType = 0;  // スクリブルタイプ 0:Soft, 1:Hard

float minLightness         = 0.02f;  // 最小濃さ
const float LoG_draw_scale = 20.0f;  // LoGフィルタの描画スケール
float LoG_s                = 0.05f;  // LoGフィルタのスケール
float sigma                = 1.0f;   // 強度/容量 変換係数
float lambda               = 0.9f;   // ソフトスクリブル係数
float alpha                = 100.f;  // 容量スケーリング係数

// float circleSize = 0.4f; // 自動スクリブルサイズ

float autoScribbleLength    = 4.f;   // オートスクリブル マージン距離
float autoScribbleThreshold = 0.1f;  // オートスクリブル 感度閾値

bool fillHole = false;  // 未定義領域を背景とするか？

int idx(int x, int y, int w) { return y * w + x; }

template <typename PIXEL>
void doDraw(TRasterPT<PIXEL> ras, std::vector<float>& r, std::vector<float>& g,
            std::vector<float>& b, std::vector<float>& a) {
  int width  = ras->getLx();
  int height = ras->getLy();

  ras->lock();
  for (int y = 0; y < height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < width; ++x) {
      pix->r = (PIXEL::Channel)(maskColor.m * maskColor.r *
                                fmin(r[y * width + x], 1.f) *
                                (float)PIXEL::maxChannelValue);
      pix->g = (PIXEL::Channel)(maskColor.m * maskColor.g *
                                fmin(g[y * width + x], 1.f) *
                                (float)PIXEL::maxChannelValue);
      pix->b = (PIXEL::Channel)(maskColor.m * maskColor.b *
                                fmin(b[y * width + x], 1.f) *
                                (float)PIXEL::maxChannelValue);
      pix->m = (PIXEL::Channel)(maskColor.m * fmin(a[y * width + x], 1.f) *
                                (float)PIXEL::maxChannelValue);
      pix++;
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void doGrayScale(TRasterPT<PIXEL> ras, std::vector<float>& temp) {
  int width  = ras->getLx();
  int height = ras->getLy();

  ras->lock();
  for (int y = 0; y < height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < width; ++x) {
      float a = (float)pix->m / (float)PIXEL::maxChannelValue;
      if (a != 0) {
        float r = (float)pix->r / (a * (float)PIXEL::maxChannelValue);
        float g = (float)pix->g / (a * (float)PIXEL::maxChannelValue);
        float b = (float)pix->b / (a * (float)PIXEL::maxChannelValue);
        temp[y * width + x] =
            fmax(0.299f * r + 0.587f * g + 0.114f * b, minLightness);
      } else
        temp[y * width + x] = 1.0f;
      pix++;
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void doLoG(TRasterPT<PIXEL> ras, std::vector<float>& gray,
           std::vector<float>& lap) {
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
void doGraph(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> refRas, bool refer_sw,
             std::vector<float>& lap, Graph& g) {
  int width   = ras->getLx();
  int height  = ras->getLy();
  int rasSize = width * height;

  // 境界用フラグを計算
  std::vector<bool> is_boundary(rasSize, false);
  for (int x = 0; x < width; ++x) {
    is_boundary[idx(x, 0, width)]          = true;
    is_boundary[idx(x, height - 1, width)] = true;
  }
  for (int y = 0; y < height; ++y) {
    is_boundary[idx(0, y, width)]         = true;
    is_boundary[idx(width - 1, y, width)] = true;
  }

  // LoG to intensity
  std::vector<float> intensity(rasSize, 0.0f);
  float K = 2.f * (width + height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int p = idx(x, y, width);
      if (is_boundary[p]) {
        intensity[p] = 1.f;
      } else {
        intensity[p] = K * lap[p] + 1.f;
        lap[p]       = lap[p] * LoG_draw_scale;
      }
    }
  }

  // set capasity
  std::vector<float> weights(rasSize, 0.0f);
  std::vector<float> capacity(rasSize, 0.0f);
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
  float softCapacity = floorf(lambda * alpha);
  int tLinkCap       = scribbleType == 0 ? softCapacity : K;
  int sLinkCap       = scribbleType == 0 ? alpha - softCapacity : 0;
  // リファレンスあり
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
  // リファレンスなし
  else {
    // オートスクリブル 楕円
    /*
    float cX = width * 0.5f;
    float cY = height * 0.5f;
    float aspx = 1.f / (cX * circleSize);
    float aspy = 1.f / (cY * circleSize);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        float dx = (x - cX) * aspx;
        float dy = (y - cY) * aspy;
        if (dx * dx + dy * dy < 1.f) {
          int p = idx(x, y, width);
          g.addTerminal(p, tLinkCap, 0);
          scribbleR[p] = 1.0f;
        }
      }
    }
    */

    // オートスクリブル 輪郭線シフト
    /*
    float cX = width * 0.5f;
    float cY = height * 0.5f;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        float rx = x - cX;
        float ry = y - cY;
        float rDst = sqrt(rx * rx + ry * ry);
        int cx = x + rx / rDst * autoScribbleLength;
        int cy = y + ry / rDst * autoScribbleLength;
        if (cx < 0 || cx >= width || cy < 0 || cy >= height) continue;
        int p = idx(x, y, width);
        int cp = idx(cx, cy, width);
        if (weights[cp] > autoScribbleThreshold) {
          g.addTerminal(p, tLinkCap, 0);
          scribbleR[p] = 1.0f;
        }
      }
    }
    */

    // オートスクリブル 最外輪郭線シフト
    float cX = width * 0.5f;
    float cY = height * 0.5f;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int p = idx(x, y, width);
        if (!is_boundary[p]) continue;
        float fcx    = x;
        float fcy    = y;
        float rx     = fcx - cX;
        float ry     = fcy - cY;
        float rDst   = sqrt(rx * rx + ry * ry);
        float nx     = rx / rDst;
        float ny     = ry / rDst;
        bool onLine  = false;
        bool pOnLine = false;
        fcx -= nx * autoScribbleLength;
        fcy -= ny * autoScribbleLength;
        for (int i = 0; i < 10000; ++i) {
          fcx -= nx;
          fcy -= ny;
          int cx = fcx;
          int cy = fcy;
          int cp = idx(cx, cy, width);
          if (fcx < 0 || fcx >= width || fcy < 0 || fcy >= height) break;
          if (weights[cp] > autoScribbleThreshold)
            onLine = true;
          else
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
  }

  // 境界値を設定
  for (int i = 0; i < rasSize; ++i) {
    if (is_boundary[i]) {
      g.addTerminal(i, -K);
      scribbleB[i] = 1.f;
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
void doColorize(TRasterPT<PIXEL> ras, Graph& g, std::vector<float>& gray) {
  int width  = ras->getLx();
  int height = ras->getLy();

  g.mincut();
  if (mode == 1) {  // Draw Mask
    ras->lock();
    for (int y = 0; y < height; ++y) {
      PIXEL* pix = ras->pixels(y);
      for (int x = 0; x < width; ++x) {
        int p = idx(x, y, width);
        if (!g.getSegment(p, fillHole ? g.SOURCE : g.SINK)) {  // SOURCE
          pix->r = maskColor.m * maskColor.r * PIXEL::maxChannelValue;
          pix->g = maskColor.m * maskColor.g * PIXEL::maxChannelValue;
          pix->b = maskColor.m * maskColor.b * PIXEL::maxChannelValue;
          pix->m = maskColor.m * PIXEL::maxChannelValue;
        } else {  // SINK
          pix->r = 0.0f;
          pix->g = 0.0f;
          pix->b = 0.0f;
          pix->m = 0.0f;
        }
        pix++;
      }
    }
    ras->unlock();
  } else if (mode == 0) {  // Draw Mask + Line
    std::vector<float> lineR(width * height, 0.0f);
    std::vector<float> lineG(width * height, 0.0f);
    std::vector<float> lineB(width * height, 0.0f);
    std::vector<float> lineM(width * height, 0.0f);
    // line => (r, g, b, 1.), noLine => (r, g, b, 0.)
    ras->lock();
    for (int y = 1; y < height - 1; ++y) {
      PIXEL* pix = ras->pixels(y);
      for (int x = 1; x < width - 1; ++x) {
        int p    = idx(x, y, width);
        lineR[p] = (float)pix->r / (float)PIXEL::maxChannelValue;
        lineG[p] = (float)pix->g / (float)PIXEL::maxChannelValue;
        lineB[p] = (float)pix->b / (float)PIXEL::maxChannelValue;
        lineM[p] = 1.f - (float)fmin(fmin(pix->r, pix->g), pix->b) /
                             (float)PIXEL::maxChannelValue;
        pix++;
      }
    }
    // result => line * mask
    for (int y = 0; y < height; ++y) {
      PIXEL* pix = ras->pixels(y);
      for (int x = 0; x < width; ++x) {
        int p = idx(x, y, width);
        if (!g.getSegment(p, fillHole ? g.SOURCE : g.SINK)) {  // SOURCE
          pix->r =
              maskColor.m * lineR[p] * maskColor.r * PIXEL::maxChannelValue;
          pix->g =
              maskColor.m * lineG[p] * maskColor.g * PIXEL::maxChannelValue;
          pix->b =
              maskColor.m * lineB[p] * maskColor.b * PIXEL::maxChannelValue;
          pix->m = (maskColor.m + (1 - maskColor.m) * lineM[p]) *
                   PIXEL::maxChannelValue;
        } else {  // SINK
          pix->r = 0.0f;
          pix->g = 0.0f;
          pix->b = 0.0f;
          pix->m = lineM[p] * PIXEL::maxChannelValue;
        }
        pix++;
      }
    }
    ras->unlock();
  }
}

//------------------------------------------------------------------------------

template <typename PIXEL>
void process(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> refRas, bool refer_sw) {
  int width   = ras->getLx();
  int height  = ras->getLy();
  int rasSize = width * height;
  std::vector<float> gray(rasSize);
  std::vector<float> lap(rasSize);

  // create graph
  Graph g = Graph(rasSize);

  doGrayScale<PIXEL>(ras, gray);
  doLoG<PIXEL>(ras, gray, lap);
  doGraph<PIXEL>(ras, refRas, refer_sw, lap, g);
  if (mode == 0 || mode == 1) {
    doColorize<PIXEL>(ras, g, gray);
  }
}

void naru_lazybrush::doCompute(TTile& tile, double frame,
                               const TRenderSettings& ri) {
  if (!m_input.isConnected()) return;

  m_input->compute(tile, frame, ri);

  TTile refer_tile;
  TRasterP inRas, refRas;
  inRas = tile.getRaster();

  bool refer_sw = false;
  if (this->m_ref.isConnected()) {
    refer_sw = true;
    this->m_ref->allocateAndCompute(refer_tile, tile.m_pos,
                                    tile.getRaster()->getSize(),
                                    tile.getRaster(), frame, ri);
    refRas = refer_tile.getRaster();
  }

  // params
  mode                  = m_mode->getValue();
  maskColor             = toPixelF(m_maskcolor->getValueD(frame));
  scribbleType          = m_scrtype->getValue();
  minLightness          = m_minlightness->getValue(frame);
  LoG_s                 = m_logs->getValue(frame);
  sigma                 = m_sigma->getValue(frame);
  lambda                = m_lambda->getValue(frame);
  alpha                 = m_alpha->getValue(frame);
  autoScribbleLength    = m_autoscrlen->getValue(frame);
  autoScribbleThreshold = m_autoscrthresh->getValue(frame);
  fillHole              = m_fillHole->getValue();
  scribbleType          = m_scrtype->getValue();

  if (TRaster32P ras32 = inRas)
    process<TPixel32>(ras32, refRas, refer_sw);
  else if (TRaster64P ras64 = inRas)
    process<TPixel64>(ras64, refRas, refer_sw);
  else if (TRasterFP rasF = inRas)
    process<TPixelF>(rasF, refRas, refer_sw);
  else
    throw TException("naru_LazyBrushFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(naru_lazybrush, "naru_LazyBrushFx")
