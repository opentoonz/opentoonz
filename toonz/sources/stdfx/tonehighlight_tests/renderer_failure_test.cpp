#include "tonehighlightfx.cpp"
#include "trenderer.h"
#include "tthread.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

std::atomic<int> warnings{0};

void testMessages(QtMsgType type, const QMessageLogContext &,
                  const QString &message) {
  if (type == QtWarningMsg || type == QtCriticalMsg) ++warnings;
  std::cerr << message.toStdString() << '\n';
}

class RenderSource final : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(RenderSource)

public:
  std::atomic<int> dryCalls{0};
  std::atomic<int> computeCalls{0};

  RenderSource() { enableComputeInFloat(true); }
  bool doGetBBox(double, TRectD &rect, const TRenderSettings &) override {
    rect = TRectD(-1000, -1000, 1000, 1000);
    return true;
  }
  bool canHandle(const TRenderSettings &, double) override { return true; }
  void doDryCompute(TRectD &, double, const TRenderSettings &) override {
    ++dryCalls;
  }
  void doCompute(TTile &tile, double, const TRenderSettings &) override {
    ++computeCalls;
    TRaster32P eight   = tile.getRaster();
    TRaster64P sixteen = tile.getRaster();
    TRasterFP floating = tile.getRaster();
    if (eight)
      eight->fill(TPixel32(100, 80, 40, 128));
    else if (sixteen)
      sixteen->fill(TPixel64(50000, 40000, 20000, 60000));
    else if (floating)
      floating->fill(TPixelF(2.25f, -0.125f, 0.375f, 0.33333334f));
  }
};

FX_PLUGIN_IDENTIFIER(RenderSource, "toneHighlightQueuedRenderSource")

bool validResult(const TRasterP &raster) {
  if (!raster || raster->getSize() != TDimension(8, 8)) return false;
  const TRaster32P eight   = raster;
  const TRaster64P sixteen = raster;
  const TRasterFP floating = raster;
  if (eight) return eight->pixels(0)[0].m == 128;
  if (sixteen) return sixteen->pixels(0)[0].m == 60000;
  if (!floating) return false;
  const TPixelF pixel = floating->pixels(0)[0];
  return std::isfinite(pixel.r) && std::isfinite(pixel.g) &&
         std::isfinite(pixel.b) && pixel.m == 0.33333334f;
}

class ResultPort final : public TRenderPort {
  QEventLoop &m_loop;
  mutable std::mutex m_mutex;
  int m_completed = 0;
  int m_failed    = 0;
  bool m_valid    = false;
  std::wstring m_message;

public:
  explicit ResultPort(QEventLoop &loop) : m_loop(loop) {
    setRenderArea(TRectD(0, 0, 8, 8));
  }

  void onRenderRasterCompleted(const RenderData &data) override {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_completed;
    m_valid = validResult(data.m_rasA);
  }

  void onRenderFailure(const RenderData &, TException &error) override {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_failed;
    m_message = error.getMessage();
  }

  void onRenderFinished(bool canceled) override {
    if (canceled) qFatal("Queued render was unexpectedly canceled");
    QMetaObject::invokeMethod(&m_loop, "quit", Qt::QueuedConnection);
  }

  void verify(bool rejected) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    require(m_failed == (rejected ? 1 : 0),
            "wrong frame-failure notification count");
    require(m_completed == (rejected ? 0 : 1),
            "wrong successful-frame notification count");
    if (rejected)
      require(m_message.find(L"reduce Softness or render scale") !=
                  std::wstring::npos,
              "frame failure omitted the actionable Softness error");
    else
      require(m_valid,
              "successful render lost source alpha or produced invalid pixels");
  }
};

class Fixture {
  TFxP m_sourceOwner;
  TFxP m_matteOwner;
  TRasterFxP m_fx;
  TRenderer m_renderer;
  TRenderSettings m_settings;

public:
  RenderSource *source;
  RenderSource *matte;

  explicit Fixture(int bpp)
      : m_sourceOwner(new RenderSource)
      , m_matteOwner(new RenderSource)
      , m_fx(new ToneHighlightFx)
      , source(static_cast<RenderSource *>(m_sourceOwner.getPointer()))
      , matte(static_cast<RenderSource *>(m_matteOwner.getPointer())) {
    require(m_fx->connect("Source", source), "cannot connect Source");
    require(m_fx->connect("Matte", matte), "cannot connect Matte");
    m_settings.m_bpp         = bpp;
    m_settings.m_maxTileSize = 2;
    m_renderer.enablePrecomputing(true);
  }

  void render(double softness, bool rejected) {
    auto *parameter =
        dynamic_cast<TDoubleParam *>(m_fx->getParams()->getParam("softness"));
    require(parameter != nullptr, "Softness parameter missing");
    parameter->setDefaultValue(softness);
    QEventLoop loop;
    ResultPort port(loop);
    m_renderer.addPort(&port);
    TFxPair roots;
    roots.m_frameA = m_fx;
    m_renderer.startRendering(0, m_settings, roots);
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [] {
      qFatal("Queued render did not finish within 10 seconds");
    });
    watchdog.start(10000);
    loop.exec();
    m_renderer.stopRendering(true);
    m_renderer.removePort(&port);
    port.verify(rejected);
  }
};

void runCase(int bpp, const std::string &name) {
  Fixture fixture(bpp);
  if (name != "success") {
    const double softness =
        name == "budget" ? 256.0 : std::numeric_limits<double>::infinity();
    require(name == "budget" || name == "nonfinite",
            "unknown render test case");
    fixture.render(softness, true);
    require(fixture.source->dryCalls == 0 && fixture.matte->dryCalls == 0,
            "rejected render precomputed an unsafe input region");
    require(
        fixture.source->computeCalls == 0 && fixture.matte->computeCalls == 0,
        "rejected render computed input pixels");
  }
  fixture.render(2.125, false);
  require(fixture.source->dryCalls > 0 && fixture.matte->dryCalls > 0,
          "valid render did not precompute its input regions");
  require(fixture.source->computeCalls + fixture.matte->computeCalls > 0,
          "valid render did not compute input pixels");
  require(warnings == 0, "queued render emitted a Qt warning");
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  TThread::init();
  TRenderer::initialize();
  const auto previousHandler = qInstallMessageHandler(testMessages);
  try {
    require(argc == 3,
            "usage: renderer_failure_test BPP success|budget|nonfinite");
    const int bpp = std::stoi(argv[1]);
    require(bpp == 32 || bpp == 64 || bpp == 128,
            "unsupported test pixel depth");
    runCase(bpp, argv[2]);
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
  qInstallMessageHandler(previousHandler);
  std::cout << "queued render, frame failure, and recovery passed\n";
  return 0;
}
