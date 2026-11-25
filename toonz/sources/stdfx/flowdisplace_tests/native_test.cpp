#include "flowdisplacefx.cpp"
#include "trenderer.h"
#include "tthread.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <iostream>
#include <mutex>

namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

class Source final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Source)

public:
  bool field = false;
  std::atomic<int> dryCalls{0};
  Source() { enableComputeInFloat(true); }
  bool canHandle(const TRenderSettings &, double) override { return true; }
  bool doGetBBox(double, TRectD &box, const TRenderSettings &) override {
    box = TConsts::infiniteRectD;
    return true;
  }
  void doDryCompute(TRectD &, double, const TRenderSettings &) override {
    ++dryCalls;
  }
  std::string getAlias(double, const TRenderSettings &) const override {
    return field ? "flowdisplace-field" : "flowdisplace-source";
  }
  void doCompute(TTile &tile, double, const TRenderSettings &) override {
    if (TRaster32P raster = tile.getRaster())
      raster->fill(field ? TPixel32(255, 128, 0) : TPixel32(64, 128, 192));
    else if (TRaster64P raster = tile.getRaster())
      raster->fill(field ? TPixel64(65535, 32768, 0)
                         : TPixel64(16384, 32768, 49152));
    else if (TRasterFP raster = tile.getRaster())
      raster->fill(field
                       ? TPixelF(std::numeric_limits<float>::max(), 0.5f, 0, 1)
                       : TPixelF(0.25f, 0.5f, 0.75f, 1));
  }
};
FX_PLUGIN_IDENTIFIER(Source, "flowdisplaceRegressionSource")

class Result final : public TRenderPort {
  QEventLoop &m_loop;
  std::mutex m_mutex;
  TRasterP m_raster;
  bool m_failed = false;

public:
  explicit Result(QEventLoop &loop) : m_loop(loop) {
    setRenderArea(TRectD(0, 0, 64, 24));
  }
  void onRenderRasterCompleted(const RenderData &data) override {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_raster = data.m_rasA->clone();
  }
  void onRenderFailure(const RenderData &, TException &) override {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failed = true;
  }
  void onRenderFinished(bool canceled) override {
    if (canceled) qFatal("Unexpected render cancellation");
    QMetaObject::invokeMethod(&m_loop, "quit", Qt::QueuedConnection);
  }
  void verify() {
    std::lock_guard<std::mutex> lock(m_mutex);
    require(!m_failed && m_raster, "native render failed");
    const auto pixels = CelPixels::read<FlowDisplace::Color>(m_raster);
    for (const auto &pixel : pixels)
      require(std::isfinite(pixel.r) && std::isfinite(pixel.g) &&
                  std::isfinite(pixel.b) && pixel.a == 1,
              "native displaced tile lost opaque pixels or produced nonfinite "
              "colors");
  }
};

void render(int bpp) {
  TFxP source = new Source, field = new Source;
  static_cast<Source *>(field.getPointer())->field = true;
  TRasterFxP effect                                = new FlowDisplaceFx;
  require(effect->connect("Source", source.getPointer()),
          "source connection failed");
  require(effect->connect("Displacement", field.getPointer()),
          "field connection failed");
  QEventLoop loop;
  Result result(loop);
  TRenderer renderer;
  renderer.enablePrecomputing(true);
  renderer.addPort(&result);
  TRenderSettings settings;
  settings.m_bpp         = bpp;
  settings.m_maxTileSize = 1;
  TFxPair roots;
  roots.m_frameA = effect;
  renderer.startRendering(0, settings, roots);
  QTimer watchdog;
  watchdog.setSingleShot(true);
  QObject::connect(&watchdog, &QTimer::timeout, &loop,
                   [] { qFatal("Native render timed out"); });
  watchdog.start(10000);
  loop.exec();
  renderer.stopRendering(true);
  renderer.removePort(&result);
  result.verify();
  require(static_cast<Source *>(source.getPointer())->dryCalls > 0 &&
              static_cast<Source *>(field.getPointer())->dryCalls > 0,
          "native render did not precompute its inputs");
}
}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  try {
    require(argc == 2, "provide output bit depth");
    render(std::stoi(argv[1]));
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
