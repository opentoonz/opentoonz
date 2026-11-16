// Real-renderer regression coverage for the opt-in native render grid.
#include "tbasefx.h"
#include "texception.h"
#include "trenderer.h"
#include "tthread.h"

#include <QApplication>
#include <QThread>

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

class RenderResult final : public TRenderPort {
public:
  std::atomic<bool> finished{false};
  TRasterP raster;
  std::string error;

  void onRenderRasterCompleted(const RenderData& data) override {
    raster = data.m_rasA->clone();
  }

  void onRenderFailure(const RenderData&, TException& exception) override {
    error = QString::fromStdWString(exception.getMessage()).toStdString();
  }

  void onRenderFinished(bool canceled) override {
    if (canceled) error = "render canceled";
    finished.store(true);
  }
};

class NativeGridTestFx final : public TBaseRasterFx {
  FX_DECLARATION(NativeGridTestFx)

public:
  bool enabled         = false;
  double selectedScale = 2.0;
  double selectedPhase = 0.0;
  TAffine expectedGrid;
  std::atomic<int> computeCalls{0};
  std::atomic<int> dryCalls{0};
  std::atomic<bool> receivedExpectedGrid{true};

  bool usesCustomRenderGrid() const override { return enabled; }

  bool canHandle(const TRenderSettings&, double) override { return false; }

  TAffine handledAffine(const TRenderSettings&, double) override {
    return TTranslation(selectedPhase, selectedPhase) * TScale(selectedScale);
  }

  bool doGetBBox(double, TRectD& box, const TRenderSettings&) override {
    box = TRectD(-12, -12, 12, 12);
    return true;
  }

  void observeGrid(const TRenderSettings& settings) {
    if (settings.m_affine != expectedGrid) receivedExpectedGrid.store(false);
  }

  void doDryCompute(TRectD&, double, const TRenderSettings& settings) override {
    ++dryCalls;
    observeGrid(settings);
  }

  void doCompute(TTile& tile, double,
                 const TRenderSettings& settings) override {
    ++computeCalls;
    observeGrid(settings);
    TRaster32P raster = tile.getRaster();
    if (!raster) throw std::runtime_error("expected 32-bit test output");
    raster->fill(TPixel32(255, 128, 64, 255));
  }

  std::string getAlias(double, const TRenderSettings&) const override {
    return "nativeGridTest:" + std::to_string(enabled) + ":" +
           std::to_string(selectedScale) + ":" + std::to_string(selectedPhase);
  }
};

FX_IDENTIFIER(NativeGridTestFx, "nativeGridRegressionTest")

void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

void render(const TRasterFxP& effect, const TRenderSettings& settings) {
  RenderResult result;
  result.setRenderArea(TRectD(-24, -24, 24, 24));
  TRenderer renderer(1);
  renderer.addPort(&result);
  renderer.enablePrecomputing(true);
  TRasterFxP secondField;
  renderer.startRendering(0, settings, TFxPair(effect, secondField));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (!result.finished.load()) {
    QApplication::processEvents();
    QThread::msleep(1);
    if (std::chrono::steady_clock::now() < deadline) continue;
    renderer.stopRendering(true);
    renderer.removePort(&result);
    throw std::runtime_error("native grid render timed out");
  }
  renderer.stopRendering(true);
  renderer.removePort(&result);
  QApplication::processEvents();
  require(result.error.empty(), "render failed: " + result.error);
  require(bool(result.raster), "renderer returned no output");
  TRaster32P raster = result.raster;
  require(bool(raster), "renderer returned an unexpected pixel type");
  require(raster->pixels(raster->getLy() / 2)[raster->getLx() / 2].m != 0,
          "renderer returned an empty central pixel");
}

void checkGrid(bool enabled, double selected, double phase, double requested,
               double shift) {
  auto* fixture          = new NativeGridTestFx;
  TRasterFxP effect      = fixture;
  fixture->enabled       = enabled;
  fixture->selectedScale = selected;
  fixture->selectedPhase = phase;
  fixture->expectedGrid =
      !enabled && requested == 1.0 && shift == std::floor(shift)
          ? TAffine()
          : TTranslation(phase, phase) * TScale(selected);
  TRenderSettings settings;
  settings.m_affine = TTranslation(shift, shift) * TScale(requested);
  render(effect, settings);
  const std::string context = "enabled=" + std::to_string(enabled) +
                              " selected=" + std::to_string(selected) +
                              " phase=" + std::to_string(phase) +
                              " requested=" + std::to_string(requested) +
                              " shift=" + std::to_string(shift);
  require(fixture->computeCalls.load() > 0, "compute not called: " + context);
  require(fixture->dryCalls.load() > 0, "dryCompute not called: " + context);
  require(fixture->receivedExpectedGrid.load(), "incorrect grid: " + context);
}

}  // namespace

int main(int argc, char** argv) {
  QApplication application(argc, argv);
  TThread::init();
  TRenderer::initialize();
  try {
    int cases = 0;
    for (bool enabled : {false, true})
      for (double selected : {0.5, 1.5, 2.0})
        for (double phase : {0.0, 0.5})
          for (double requested : {1.0, 2.0, 0.75})
            for (double shift : {0.0, 0.25, 3.75, -0.25, 3.0, -3.0}) {
              checkGrid(enabled, selected, phase, requested, shift);
              ++cases;
            }
    std::cout << cases << " native-grid routing cases passed\n";
  } catch (const TException& exception) {
    std::cerr << QString::fromStdWString(exception.getMessage()).toStdString()
              << '\n';
    return 1;
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';
    return 1;
  }
  return 0;
}
