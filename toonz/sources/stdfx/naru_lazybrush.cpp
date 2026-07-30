#include "trop.h"
#include "tfxparam.h"
#include <math.h>
#include "stdfx.h"
#include <cstdlib>
#include <memory>

#include "tparamset.h"
#include "trasterfx.h"
#include "tpixelutils.h"
#include "tdoublekeyframe.h"

#include "naru_graph.h"
#include <opencv2/opencv.hpp>

using namespace std;
using GraphType = reimpls::Graph2<int, int, int>;
using namespace cv;

struct RenderContext {
  int mode;
  int scribbleType;
  int width, height, rasSize;
  float s, K, lambda, weightSoft;
  float autoScribbleTh;
  int terminalCap;
  bool enableAutoScribble;
  bool enableAutoBgScribble;

  int minX, minY, maxX, maxY;

  TPixelF refBGColor;
  TPixelF autoScribbleColor;
  vector<TPixelF> colorPalette;

  vector<float> procImgData;
  vector<float> refImgData;
  Mat refMat;
  vector<int> scribbleData;
  vector<int> mincutData;

  unique_ptr<GraphType> graph;
  int bgClusterId    = 0;
  int autoScribbleId = 1;
  int blendMode;
};

class naru_lazybrush final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(naru_lazybrush)

  TRasterFxPort m_input;
  TRasterFxPort m_ref;

  TIntEnumParamP m_mode;
  TPixelParamP m_bg_color;

  TBoolParamP m_enable_auto_scribble;
  TPixelParamP m_auto_scribble_color;
  TDoubleParamP m_auto_scribble_threshold;

  TIntEnumParamP m_scribble_type;
  TDoubleParamP m_log_scale;
  TDoubleParamP m_lambda;
  TIntEnumParamP m_blend_mode;
  TBoolParamP m_enable_auto_bg_scribble;

  // Obsolete parameters for backward compatibility
  TPixelParamP m_mask_color;
  TDoubleParamP m_sigma;
  TDoubleParamP m_alpha;
  TDoubleParamP m_autoscrlen;
  TDoubleParamP m_lineweight;
  TBoolParamP m_fillHole;
  TIntEnumParamP m_sinkpos;
  TBoolParamP m_autoScribble;

public:
  naru_lazybrush()
      : m_mode(new TIntEnumParam(0, "Mask+Line"))
      , m_bg_color(TPixel32(0, 0, 255))
      , m_enable_auto_scribble(true)
      , m_auto_scribble_color(TPixel32(200, 200, 200))
      , m_auto_scribble_threshold(0.5f)
      , m_scribble_type(new TIntEnumParam(0, "Soft"))
      , m_log_scale(10.0f)
      , m_lambda(0.05f)
      , m_blend_mode(new TIntEnumParam(0, "Multiply"))
      , m_enable_auto_bg_scribble(true)
      , m_mask_color(TPixel32(0, 0, 0))
      , m_sigma(0.0)
      , m_alpha(0.0)
      , m_autoscrlen(0.0)
      , m_lineweight(0.0)
      , m_fillHole(false)
      , m_sinkpos(new TIntEnumParam(0, "All"))
      , m_autoScribble(false) {
    bindParam(this, "mode", m_mode);

    this->m_mode->addItem(1, "Mask");
    this->m_mode->addItem(2, "LoG Filter");
    this->m_mode->addItem(4, "Scribble");

    bindParam(this, "blend_mode", m_blend_mode);
    this->m_blend_mode->addItem(1, "Normal");
    this->m_blend_mode->addItem(2, "Screen");
    this->m_blend_mode->addItem(3, "Overlay");
    this->m_blend_mode->addItem(4, "Darken");
    this->m_blend_mode->addItem(5, "Lighten");

    bindParam(this, "enable_auto_scribble", m_enable_auto_scribble);
    bindParam(this, "auto_scribble_color", m_auto_scribble_color);
    bindParam(this, "enable_auto_bg_scribble", m_enable_auto_bg_scribble);
    bindParam(this, "ref_bgcolor", m_bg_color);
    bindParam(this, "auto_scribble_threshold", m_auto_scribble_threshold);
    this->m_auto_scribble_threshold->setValueRange(0.f, 1.f);

    bindParam(this, "scr_type", m_scribble_type);
    this->m_scribble_type->addItem(1, "Hard");

    bindParam(this, "log_scale", m_log_scale);
    this->m_log_scale->setValueRange(0.f, 100.f);

    bindParam(this, "lambda", m_lambda);
    this->m_lambda->setValueRange(0.0f, 1.f);

    bindParam(this, "mask_color", m_mask_color, true, true);
    bindParam(this, "sigma", m_sigma, true, true);
    bindParam(this, "alpha", m_alpha, true, true);
    bindParam(this, "auto_scribble_length", m_autoscrlen, true, true);
    bindParam(this, "line_weight", m_lineweight, true, true);
    bindParam(this, "undef_is_sink", m_fillHole, true, true);
    bindParam(this, "sink_pos", m_sinkpos, true, true);
    bindParam(this, "auto_scribble", m_autoScribble, true, true);

    this->m_sinkpos->addItem(1, "Bottom-Left");
    this->m_sinkpos->addItem(2, "Bottom-Center");
    this->m_sinkpos->addItem(3, "Bottom-Right");
    this->m_sinkpos->addItem(4, "Center-Right");
    this->m_sinkpos->addItem(5, "Top-Right");
    this->m_sinkpos->addItem(6, "Top-Center");
    this->m_sinkpos->addItem(7, "Top-Left");
    this->m_sinkpos->addItem(8, "Center-Left");

    setFxVersion(2);

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

  void loadData(TIStream& is) override;
  void onObsoleteParamLoaded(const std::string& paramName) override;
  void onFxVersionSet() override;

private:
  //// Params
  array<array<float, 5>, 5> gauKernel = {
      {{1.f / 256.f, 4.f / 256.f, 6.f / 256.f, 4.f / 256.f, 1.f / 256.f},
       {4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f},
       {6.f / 256.f, 24.f / 256.f, 36.f / 256.f, 24.f / 256.f, 6.f / 256.f},
       {4.f / 256.f, 16.f / 256.f, 24.f / 256.f, 16.f / 256.f, 4.f / 256.f},
       {1.f / 256.f, 4.f / 256.f, 6.f / 256.f, 4.f / 256.f, 1.f / 256.f}}};

  array<array<float, 3>, 3> lapKernel = {
      {{0.f, 1.f, 0.f}, {1.f, -4.f, 1.f}, {0.f, 1.f, 0.f}}};

  //// functions
  // generate Kernel Functions
  template <size_t N>
  void convolve(vector<float>& data, const array<array<float, N>, N>& kernel,
                const RenderContext& ctx);
  void logFilter(vector<float>& data, RenderContext& ctx);

  // Intensity Culc
  // I_f = 1 - max(0, s * LoG(I))
  void I_f(vector<float>& arg, const RenderContext& ctx) {  // arg = LoG result
    for (int i = 0; i < ctx.rasSize; ++i) {
      arg[i] = fmax(0, 1.f - fmax(0, ctx.s * arg[i]));
    }
  }

  // I_p = K * (I_f ^ 2) + 1
  void I_p(vector<float>& arg, const RenderContext& ctx) {  // arg = I_f result
    for (int i = 0; i < ctx.rasSize; ++i) {
      arg[i] = ctx.K * arg[i] * arg[i] + 1.f;
    }
  }

  //-------------------------------------------------------------------

  void initRasters(TRasterP& srcRas, TRasterP& refRas, TTile& fullTile,
                   TTile& tile, const TRenderSettings& ri, double frame);

  template <typename PIXEL>
  void createScribbleIndexMap(TRasterPT<PIXEL>, RenderContext& ctx);
  template <typename PIXEL>
  void setMat(TRasterPT<PIXEL>, Mat&, const RenderContext& ctx);
  inline int colorToInt(const Vec4f& pix) {
    int r = static_cast<int>(pix[0] * 255 + 0.5f);
    int g = static_cast<int>(pix[1] * 255 + 0.5f);
    int b = static_cast<int>(pix[2] * 255 + 0.5f);
    int a = static_cast<int>(pix[3] * 255 + 0.5f);
    return (r << 24) | (g << 16) | (b << 8) | a;
  }
  void createPalette(const Mat&, RenderContext& ctx);

  template <typename PIXEL>
  void rasterToGrayVector(TRasterPT<PIXEL> ras, vector<float>& vec,
                          const RenderContext& ctx);

  void setBaundaryScribble(RenderContext& ctx);
  void autoScribble(RenderContext& ctx);
  void autoScribbleScan(int x, int y, int dx, int dy, RenderContext& ctx);

  void setCapacity(RenderContext& ctx);
  void setTerminalCapacity(int refInd, RenderContext& ctx);
  void updateMincutData(int refInd, RenderContext& ctx);

  template <typename PIXEL>
  void setMask(TRasterPT<PIXEL>& mask, const RenderContext& ctx);

  template <typename PIXEL>
  void drawImgFromInt(TRasterPT<PIXEL> ras, const vector<int>& data,
                      const RenderContext& ctx);

  template <typename PIXEL>
  void drawImgFromFloat(TRasterPT<PIXEL> ras, const vector<float>& data,
                        const RenderContext& ctx);

  template <typename PIXEL>
  void drawMaskAndLine(TRasterPT<PIXEL> ras, TRasterPT<PIXEL> mask,
                       const RenderContext& ctx);

  template <typename PIXEL>
  void process(TRasterPT<PIXEL> ras, double frame, RenderContext& ctx);

  // utils
  int idx(int x, int y, const RenderContext& ctx) {
    return max(0, min(y * ctx.width + x, ctx.rasSize - 1));
  }

  template <typename PIXEL>
  inline float pixelToNormalizedGray(const PIXEL& pix) {
    float invMax = 1.f / (float)PIXEL::maxChannelValue;
    float L_premult =
        (0.299f * pix.r + 0.587f * pix.g + 0.114f * pix.b) * invMax;
    float m = (float)pix.m * invMax;
    return L_premult + 1.f - m;
  }
};

//------------------------------------------------------------------------------

void naru_lazybrush::doCompute(TTile& tile, double frame,
                               const TRenderSettings& ri) {
  if (!m_input.isConnected()) return;

  TRasterP srcRas;
  TRasterP refRas;
  TTile fullTile;
  initRasters(srcRas, refRas, fullTile, tile, ri, frame);

  RenderContext ctx;
  ctx.width   = srcRas->getLx();
  ctx.height  = srcRas->getLy();
  ctx.rasSize = ctx.width * ctx.height;
  ctx.mode    = m_mode->getValue();

  ctx.refBGColor           = toPixelF(m_bg_color->getValueD(frame));
  ctx.enableAutoScribble   = m_enable_auto_scribble->getValue();
  ctx.autoScribbleColor    = toPixelF(m_auto_scribble_color->getValueD(frame));
  ctx.blendMode            = m_blend_mode->getValue();
  ctx.enableAutoBgScribble = m_enable_auto_bg_scribble->getValue();

  ctx.scribbleData.resize(ctx.rasSize, -1);

  // default ColorPalette
  ctx.bgClusterId    = 0;
  ctx.autoScribbleId = 1;
  ctx.colorPalette.clear();
  ctx.colorPalette.resize(2);
  ctx.colorPalette[0] = TPixelF(0, 0, 0, 0);
  ctx.colorPalette[1] = ctx.autoScribbleColor;

  // pre process reference scribble data
  if (m_ref.isConnected()) {
    if (TRaster32P ras32 = refRas)
      createScribbleIndexMap<TPixel32>(ras32, ctx);
    else if (TRaster64P ras64 = refRas)
      createScribbleIndexMap<TPixel64>(ras64, ctx);
    else if (TRasterFP rasF = refRas)
      createScribbleIndexMap<TPixelF>(rasF, ctx);
    else
      throw TException("unsupported Pixel Type");
  }

  // main process
  if (TRaster32P ras32 = srcRas)
    process<TPixel32>(ras32, frame, ctx);
  else if (TRaster64P ras64 = srcRas)
    process<TPixel64>(ras64, frame, ctx);
  else if (TRasterFP rasF = srcRas)
    process<TPixelF>(rasF, frame, ctx);
  else
    throw TException("unsupported Pixel Type");

  // copy result to tile
  tile.getRaster()->copy(srcRas);
}

template <typename PIXEL>
void naru_lazybrush::process(TRasterPT<PIXEL> ras, double frame,
                             RenderContext& ctx) {
  ctx.K              = 2 * (ctx.width + ctx.height);
  ctx.s              = m_log_scale->getValue(frame);
  ctx.lambda         = m_lambda->getValue(frame);
  ctx.autoScribbleTh = 1.0f - m_auto_scribble_threshold->getValue(frame);
  ctx.scribbleType   = m_scribble_type->getValue();
  ctx.weightSoft     = floorf((1.0f - ctx.lambda) * ctx.K);
  ctx.terminalCap    = ctx.scribbleType == 0 ? 4 * ctx.weightSoft : 5 * ctx.K;
  ctx.procImgData.resize(ctx.rasSize, 0.f);
  ctx.refImgData.resize(ctx.rasSize, 0.f);

  //// process image data
  // Gray scale [I]
  rasterToGrayVector(ras, ctx.procImgData, ctx);

  // LoG Filter [LoG(I)]
  logFilter(ctx.procImgData, ctx);

  // [max(0, 1 - max(0, s * LoG))]
  I_f(ctx.procImgData, ctx);

  // Draw LoG Filter
  if (ctx.mode == 2) {
    drawImgFromFloat(ras, ctx.procImgData, ctx);
    return;
  }

  // --- BBox calculation: fast search from the boundaries & union with
  // reference scribbles ---
  ctx.minX = ctx.width;
  ctx.minY = ctx.height;
  ctx.maxX = -1;
  ctx.maxY = -1;

  // 1. Search from the top edge (minY)
  bool found = false;
  for (int y = 0; y < ctx.height; ++y) {
    for (int x = 0; x < ctx.width; ++x) {
      int p = idx(x, y, ctx);
      if ((ctx.procImgData[p] < ctx.autoScribbleTh) ||
          (ctx.scribbleData[p] >= 1)) {
        ctx.minY = y;
        found    = true;
        break;
      }
    }
    if (found) break;
  }

  if (found) {
    // 2. Search from the bottom edge (maxY)
    found = false;
    for (int y = ctx.height - 1; y >= 0; --y) {
      for (int x = 0; x < ctx.width; ++x) {
        int p = idx(x, y, ctx);
        if ((ctx.procImgData[p] < ctx.autoScribbleTh) ||
            (ctx.scribbleData[p] >= 1)) {
          ctx.maxY = y;
          found    = true;
          break;
        }
      }
      if (found) break;
    }

    // 3. Search from the left edge (minX)
    found = false;
    for (int x = 0; x < ctx.width; ++x) {
      for (int y = 0; y < ctx.height; ++y) {
        int p = idx(x, y, ctx);
        if ((ctx.procImgData[p] < ctx.autoScribbleTh) ||
            (ctx.scribbleData[p] >= 1)) {
          ctx.minX = x;
          found    = true;
          break;
        }
      }
      if (found) break;
    }

    // 4. Search from the right edge (maxX)
    found = false;
    for (int x = ctx.width - 1; x >= 0; --x) {
      for (int y = 0; y < ctx.height; ++y) {
        int p = idx(x, y, ctx);
        if ((ctx.procImgData[p] < ctx.autoScribbleTh) ||
            (ctx.scribbleData[p] >= 1)) {
          ctx.maxX = x;
          found    = true;
          break;
        }
      }
      if (found) break;
    }
  } else {
    // If no target is detected, the BBox is considered invalid.
    ctx.minX = ctx.width;
    ctx.minY = ctx.height;
    ctx.maxX = -1;
    ctx.maxY = -1;
  }

  // Execute the auto-scribble process prior to applying I_p, and evaluate it
  // against the unmodified I_f.
  if (ctx.enableAutoScribble) {
    autoScribble(ctx);
  }
  // Set Baundary Scribble
  if (ctx.enableAutoBgScribble) {
    setBaundaryScribble(ctx);
  }

  // [K * (1 - max(0, s * LoG(I))) ^ 2 + 1]
  I_p(ctx.procImgData, ctx);

  // Draw Capacity Mode
  if (ctx.mode == 3) {
    vector<float> capDisp(ctx.rasSize, 0.f);
    for (int i = 0; i < ctx.rasSize; ++i) {
      capDisp[i] = (ctx.procImgData[i] - 1.f) / ctx.K;
    }
    drawImgFromFloat(ras, capDisp, ctx);
    return;
  }

  // Draw Scribble Map
  if (ctx.mode == 4) {
    drawImgFromInt(ras, ctx.scribbleData, ctx);
    return;
  }

  //// alpha expansion
  ctx.mincutData.assign(ctx.rasSize, ctx.bgClusterId);
  for (int t = 0; t < ctx.colorPalette.size(); ++t) {
    if (t == ctx.bgClusterId) continue;
    // Init Graph
    ctx.graph = make_unique<GraphType>(ctx.rasSize, ctx.rasSize * 4);
    ctx.graph->add_node(ctx.rasSize);

    // Capacity
    setCapacity(ctx);

    // Set Terminal Capacity
    setTerminalCapacity(t, ctx);

    // Process Graph Algorithm
    ctx.graph->init_maxflow();
    int flow = ctx.graph->maxflow();

    // update mincut data
    updateMincutData(t, ctx);
  }

  // Set Mask
  TRasterPT<PIXEL> mask = TRasterPT<PIXEL>(ctx.width, ctx.height);
  setMask<PIXEL>(mask, ctx);

  // draw
  if (ctx.mode == 0) {
    drawMaskAndLine(ras, mask, ctx);
  } else {
    ras->copy(mask);
  }
}

//-------------------------------------------------------------------
void naru_lazybrush::initRasters(TRasterP& srcRas, TRasterP& refRas,
                                 TTile& fullTile, TTile& tile,
                                 const TRenderSettings& ri, double frame) {
  // get BBox of the input image
  TRectD calcArea(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                          tile.getRaster()->getLy()));

  // get input raster
  TDimension size = tile.getRaster()->getSize();
  m_input->allocateAndCompute(fullTile, calcArea.getP00(), size,
                              tile.getRaster(), frame, ri);
  srcRas = fullTile.getRaster();

  // get reference raster
  if (m_ref.isConnected()) {
    TTile refTile;
    m_ref->allocateAndCompute(refTile, calcArea.getP00(), size, srcRas, frame,
                              ri);
    refRas = refTile.getRaster();
  }
}

template <typename PIXEL>
void naru_lazybrush::createScribbleIndexMap(TRasterPT<PIXEL> refRas,
                                            RenderContext& ctx) {
  // Cast refColors to refMat
  setMat(refRas, ctx.refMat, ctx);

  // Create Palette
  createPalette(ctx.refMat, ctx);
}

template <typename PIXEL>
void naru_lazybrush::setMat(TRasterPT<PIXEL> ras, Mat& mat,
                            const RenderContext& ctx) {
  mat.create(ctx.rasSize, 1, CV_32FC4);
  ras->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < ctx.width; ++x) {
      int i       = idx(x, y, ctx);
      float alpha = (float)pix[x].m / (float)PIXEL::maxChannelValue;
      if (alpha < 1.f) {
        mat.at<Vec4f>(i) = Vec4f(0.f, 0.f, 0.f, 0.f);
      } else {
        mat.at<Vec4f>(i) =
            Vec4f((float)pix[x].r / (float)PIXEL::maxChannelValue,
                  (float)pix[x].g / (float)PIXEL::maxChannelValue,
                  (float)pix[x].b / (float)PIXEL::maxChannelValue, 1.f);
      }
    }
  }
  ras->unlock();
}

void naru_lazybrush::createPalette(const Mat& mat, RenderContext& ctx) {
  ctx.colorPalette.clear();
  vector<int> colorInts;

  // Define BG Color
  ctx.bgClusterId = 0;
  Vec4f bgColor(ctx.refBGColor.r, ctx.refBGColor.g, ctx.refBGColor.b, 1.f);
  colorInts.push_back(colorToInt(bgColor));
  ctx.colorPalette.push_back(TPixelF(0, 0, 0, 0));

  // Define Auto Scribble Color
  if (ctx.enableAutoScribble) {
    ctx.autoScribbleId = 1;
    Vec4f asColor(ctx.autoScribbleColor.r, ctx.autoScribbleColor.g,
                  ctx.autoScribbleColor.b, 1.f);
    int cInt = colorToInt(asColor);
    if (colorInts.size() == 0 || colorInts[0] != cInt) {
      colorInts.push_back(cInt);
      ctx.colorPalette.push_back(TPixelF(ctx.autoScribbleColor));
    }
  }

  // Define Scribble Colors from Reference
  int pos = 0;
  for (int pi = 0; pi < ctx.rasSize; ++pi) {
    Vec4f p = mat.at<Vec4f>(pi, 0);
    if (p[3] == 0) continue;
    p[3] = 1.f;

    int cInt = colorToInt(p);

    // Exist in Palette
    int found = -1;
    for (size_t i = 0; i < colorInts.size(); ++i) {
      if (colorInts[i] == cInt) {
        found = static_cast<int>(i);
        break;
      }
    }

    if (found == -1) {
      // New Color
      ctx.colorPalette.push_back(TPixelF(p[0], p[1], p[2], 1.f));
      colorInts.push_back(cInt);
      found = static_cast<int>(colorInts.size() - 1);
    }
    ctx.scribbleData[pi] = found;
  }
}

template <typename PIXEL>
void naru_lazybrush::rasterToGrayVector(TRasterPT<PIXEL> ras,
                                        vector<float>& vec,
                                        const RenderContext& ctx) {
  ras->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < ctx.width; ++x) {
      vec[idx(x, y, ctx)] = pixelToNormalizedGray<PIXEL>(pix[x]);
    }
  }
  ras->unlock();
}

template <size_t N>
void naru_lazybrush::convolve(vector<float>& data,
                              const array<array<float, N>, N>& kernel,
                              const RenderContext& ctx) {
  int kSize               = kernel.size();
  int half                = kSize / 2;
  vector<float> inputData = data;
  for (int y = 0; y < ctx.height; ++y) {
    for (int x = 0; x < ctx.width; ++x) {
      float sum = 0.f;
      for (int ky = -half; ky <= half; ++ky) {
        for (int kx = -half; kx <= half; ++kx) {
          int ix = std::min(std::max(x + kx, 0), ctx.width - 1);
          int iy = std::min(std::max(y + ky, 0), ctx.height - 1);
          sum += inputData[idx(ix, iy, ctx)] * kernel[ky + half][kx + half];
        }
      }
      data[idx(x, y, ctx)] = sum;
    }
  }
}

void naru_lazybrush::logFilter(vector<float>& data, RenderContext& ctx) {
  // Gaussian filter
  convolve(data, gauKernel, ctx);

  // Laplacian filter
  convolve(data, lapKernel, ctx);
}

void naru_lazybrush::setCapacity(RenderContext& ctx) {
  for (int y = 0; y < ctx.height; ++y) {
    for (int x = 0; x < ctx.width; ++x) {
      int p = idx(x, y, ctx);
      if (x < ctx.width - 1) {
        int q        = idx(x + 1, y, ctx);
        float weight = (ctx.procImgData[p] + ctx.procImgData[q]) * 0.5f;
        ctx.graph->add_edge(p, q, weight, weight);
      }
      if (y < ctx.height - 1) {
        int q        = idx(x, y + 1, ctx);
        float weight = (ctx.procImgData[p] + ctx.procImgData[q]) * 0.5f;
        ctx.graph->add_edge(p, q, weight, weight);
      }
    }
  }
}

void naru_lazybrush::autoScribble(RenderContext& ctx) {
  // Perform scanning only when a valid bounding box exists.
  if (ctx.maxX >= 0 && ctx.maxY >= 0) {
    // Scan inward from the four boundaries of the bounding box.
    // From the top edge downward
    for (int x = ctx.minX; x <= ctx.maxX; ++x) {
      autoScribbleScan(x, ctx.minY, 0, 1, ctx);
    }
    // From the bottom edge upward
    for (int x = ctx.minX; x <= ctx.maxX; ++x) {
      autoScribbleScan(x, ctx.maxY, 0, -1, ctx);
    }
    // From the left edge rightward
    for (int y = ctx.minY; y <= ctx.maxY; ++y) {
      autoScribbleScan(ctx.minX, y, 1, 0, ctx);
    }
    // From the right edge leftward
    for (int y = ctx.minY; y <= ctx.maxY; ++y) {
      autoScribbleScan(ctx.maxX, y, -1, 0, ctx);
    }
  }
}

void naru_lazybrush::autoScribbleScan(int x, int y, int dx, int dy,
                                      RenderContext& ctx) {
  bool foundLine = false;
  while (x >= 0 && x < ctx.width && y >= 0 && y < ctx.height) {
    int cp      = idx(x, y, ctx);
    bool isLine = ctx.procImgData[cp] < ctx.autoScribbleTh;
    if (isLine) {
      foundLine = true;
    } else if (foundLine) {
      // Mark the inner region once the scan has crossed the stroke boundary and
      // the intensity returns above the threshold (i.e., isLine becomes false).
      ctx.scribbleData[cp] = ctx.autoScribbleId;
      break;
    }
    x += dx;
    y += dy;
  }
}

void naru_lazybrush::setBaundaryScribble(RenderContext& ctx) {
  int padding = 3;
  if (ctx.maxX >= 0 && ctx.maxY >= 0) {
    // extend Bounding Box by padding
    int minX = ctx.minX - padding;
    int minY = ctx.minY - padding;
    int maxX = ctx.maxX + padding;
    int maxY = ctx.maxY + padding;

    // set scribble
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        if (x < 0 || x >= ctx.width || y < 0 || y >= ctx.height) continue;
        bool isBBEdge = (x == minX || x == maxX || y == minY || y == maxY);
        if (isBBEdge) {
          int p               = idx(x, y, ctx);
          ctx.scribbleData[p] = -2;
        }
      }
    }
  }
}

void naru_lazybrush::setTerminalCapacity(int refInd, RenderContext& ctx) {
  int hardCap = 5 * ctx.K;
  for (int y = 0; y < ctx.height; ++y) {
    for (int x = 0; x < ctx.width; ++x) {
      int p = idx(x, y, ctx);
      if (ctx.scribbleData[p] == -2) {
        // scribbleData[p] == -2 represents the outer boundary of the image.
        // Connect to the sink (T) to establish a strict boundary constraint.
        ctx.graph->add_tweights(p, 0, hardCap);
      } else if (ctx.scribbleData[p] == -1) {
        // No scribble present: addition of a t-link is not required.
        continue;
      } else if (ctx.scribbleData[p] == refInd) {
        // Connect the target scribble color to the source (S).
        ctx.graph->add_tweights(p, ctx.terminalCap, 0);
      } else {
        // Connect all other scribble colors to the sink (T) collectively.
        ctx.graph->add_tweights(p, 0, ctx.terminalCap);
      }
    }
  }
}

void naru_lazybrush::updateMincutData(int refInd, RenderContext& ctx) {
  for (int y = 0; y < ctx.height; ++y) {
    for (int x = 0; x < ctx.width; ++x) {
      int p = idx(x, y, ctx);
      if (ctx.graph->what_segment(p, ctx.graph->SOURCE) == ctx.graph->SOURCE) {
        ctx.mincutData[p] = refInd;
      }
    }
  }
}

template <typename PIXEL>
void naru_lazybrush::setMask(TRasterPT<PIXEL>& mask, const RenderContext& ctx) {
  mask->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* maskPix = mask->pixels(y);
    for (int x = 0; x < ctx.width; ++x) {
      int p     = idx(x, y, ctx);
      TPixelF c = ctx.colorPalette[ctx.mincutData[p]];

      maskPix[x].r =
          (typename PIXEL::Channel)(c.r * (float)PIXEL::maxChannelValue);
      maskPix[x].g =
          (typename PIXEL::Channel)(c.g * (float)PIXEL::maxChannelValue);
      maskPix[x].b =
          (typename PIXEL::Channel)(c.b * (float)PIXEL::maxChannelValue);
      maskPix[x].m =
          (typename PIXEL::Channel)(c.m * (float)PIXEL::maxChannelValue);

      if (maskPix[x].m == 0) {
        maskPix[x].r = 0;
        maskPix[x].g = 0;
        maskPix[x].b = 0;
      }
    }
  }
  mask->unlock();
}

template <typename PIXEL>
void naru_lazybrush::drawImgFromFloat(TRasterPT<PIXEL> ras,
                                      const vector<float>& data,
                                      const RenderContext& ctx) {
  ras->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < ctx.width; ++x) {
      int p    = idx(x, y, ctx);
      pix[x].r = (typename PIXEL::Channel)(fmin(data[p], 1.f) *
                                           (float)PIXEL::maxChannelValue);
      pix[x].g = (typename PIXEL::Channel)(fmin(data[p], 1.f) *
                                           (float)PIXEL::maxChannelValue);
      pix[x].b = (typename PIXEL::Channel)(fmin(data[p], 1.f) *
                                           (float)PIXEL::maxChannelValue);
      pix[x].m = (typename PIXEL::Channel)(1.f * (float)PIXEL::maxChannelValue);
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void naru_lazybrush::drawImgFromInt(TRasterPT<PIXEL> ras,
                                    const vector<int>& data,
                                    const RenderContext& ctx) {
  ras->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* pix = ras->pixels(y);
    for (int x = 0; x < ctx.width; ++x) {
      int p = idx(x, y, ctx);
      TPixelF c;
      if (data[p] == -2 || data[p] == ctx.bgClusterId)
        c = ctx.refBGColor;
      else if (data[p] == -1)
        c = TPixelF(1.f, 1.f, 1.f, 1.f);
      else if (data[p] < ctx.colorPalette.size())
        c = ctx.colorPalette[data[p]];
      else
        c = TPixelF(1.f, 0, 1.f, 1.f);
      pix[x].r = (typename PIXEL::Channel)(c.r * (float)PIXEL::maxChannelValue);
      pix[x].g = (typename PIXEL::Channel)(c.g * (float)PIXEL::maxChannelValue);
      pix[x].b = (typename PIXEL::Channel)(c.b * (float)PIXEL::maxChannelValue);
      pix[x].m = (typename PIXEL::Channel)(c.m * (float)PIXEL::maxChannelValue);
    }
  }
  ras->unlock();
}

template <typename PIXEL>
void naru_lazybrush::drawMaskAndLine(TRasterPT<PIXEL> ras,
                                     TRasterPT<PIXEL> mask,
                                     const RenderContext& ctx) {
  ras->lock();
  mask->lock();
  for (int y = 0; y < ctx.height; ++y) {
    PIXEL* basePix = ras->pixels(y);
    PIXEL* maskPix = mask->pixels(y);

    for (int x = 0; x < ctx.width; ++x) {
      // 1. Get original color and normalize to [0, 1]
      float br = (float)basePix[x].r / (float)PIXEL::maxChannelValue;
      float bg = (float)basePix[x].g / (float)PIXEL::maxChannelValue;
      float bb = (float)basePix[x].b / (float)PIXEL::maxChannelValue;
      float ba = (float)basePix[x].m / (float)PIXEL::maxChannelValue;

      // 2. Get mask color and normalize to [0, 1]
      float mr = (float)maskPix[x].r / (float)PIXEL::maxChannelValue;
      float mg = (float)maskPix[x].g / (float)PIXEL::maxChannelValue;
      float mb = (float)maskPix[x].b / (float)PIXEL::maxChannelValue;
      float ma = (float)maskPix[x].m / (float)PIXEL::maxChannelValue;

      // 3. Compute original image line alpha
      float a_line = ba;

      // 4. Calculate output alpha
      float a_out = a_line + ma * (1.f - a_line);

      // 5. Apply blend mode f(C_b, C_s) where C_b = mask (bottom), C_s =
      // original (top)
      float r_blend = 0.f, g_blend = 0.f, b_blend = 0.f;

      switch (ctx.blendMode) {
      case 0:  // Multiply
        r_blend = mr * br;
        g_blend = mg * bg;
        b_blend = mb * bb;
        break;
      case 1:  // Normal
        r_blend = br;
        g_blend = bg;
        b_blend = bb;
        break;
      case 2:  // Screen
        r_blend = mr + br - mr * br;
        g_blend = mg + bg - mg * bg;
        b_blend = mb + bb - mb * bb;
        break;
      case 3:  // Overlay
        r_blend = (mr < 0.5f) ? (2.f * mr * br)
                              : (1.f - 2.f * (1.f - mr) * (1.f - br));
        g_blend = (mg < 0.5f) ? (2.f * mg * bg)
                              : (1.f - 2.f * (1.f - mg) * (1.f - bg));
        b_blend = (mb < 0.5f) ? (2.f * mb * bb)
                              : (1.f - 2.f * (1.f - mb) * (1.f - bb));
        break;
      case 4:  // Darken
        r_blend = fmin(mr, br);
        g_blend = fmin(mg, bg);
        b_blend = fmin(mb, bb);
        break;
      case 5:  // Lighten
        r_blend = fmax(mr, br);
        g_blend = fmax(mg, bg);
        b_blend = fmax(mb, bb);
        break;
      default:
        r_blend = br;
        g_blend = bg;
        b_blend = bb;
        break;
      }

      // 6. Composite with alpha
      float r_out = 0.f, g_out = 0.f, b_out = 0.f;
      if (a_out > 0.f) {
        r_out = (a_line * (1.f - ma) * br + ma * (1.f - a_line) * mr +
                 a_line * ma * r_blend) /
                a_out;
        g_out = (a_line * (1.f - ma) * bg + ma * (1.f - a_line) * mg +
                 a_line * ma * g_blend) /
                a_out;
        b_out = (a_line * (1.f - ma) * bb + ma * (1.f - a_line) * mb +
                 a_line * ma * b_blend) /
                a_out;
      }

      // Clamp and write back
      basePix[x].r = static_cast<typename PIXEL::Channel>(
          fmax(0.f, fmin(r_out, 1.f)) * (float)PIXEL::maxChannelValue);
      basePix[x].g = static_cast<typename PIXEL::Channel>(
          fmax(0.f, fmin(g_out, 1.f)) * (float)PIXEL::maxChannelValue);
      basePix[x].b = static_cast<typename PIXEL::Channel>(
          fmax(0.f, fmin(b_out, 1.f)) * (float)PIXEL::maxChannelValue);
      basePix[x].m = static_cast<typename PIXEL::Channel>(
          fmax(0.f, fmin(a_out, 1.f)) * (float)PIXEL::maxChannelValue);
    }
  }
  ras->unlock();
  mask->unlock();
}

//------------------------------------------------------------------

void naru_lazybrush::onObsoleteParamLoaded(const std::string& paramName) {
  if (paramName == "mask_color") {
    m_auto_scribble_color->copy(m_mask_color.getPointer());
  } else if (paramName == "auto_scribble") {
    m_enable_auto_scribble->setValue(m_autoScribble->getValue());
  }
}

void naru_lazybrush::loadData(TIStream& is) {
  TStandardRasterFx::loadData(is);
  if (getFxVersion() < 2) {
    if (m_log_scale->getKeyframeCount() > 0) {
      for (int i = 0; i < m_log_scale->getKeyframeCount(); ++i) {
        TDoubleKeyframe k = m_log_scale->getKeyframe(i);
        k.m_value *= 200.0;
        k.m_speedIn.y *= 200.0;
        k.m_speedOut.y *= 200.0;
        m_log_scale->setKeyframe(i, k);
      }
    } else {
      m_log_scale->setDefaultValue(m_log_scale->getDefaultValue() * 200.0);
    }
    setFxVersion(2);
  }
}

void naru_lazybrush::onFxVersionSet() {}

//------------------------------------------------------------------

FX_PLUGIN_IDENTIFIER(naru_lazybrush, "naru_LazyBrushFx")
