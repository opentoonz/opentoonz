#include "celboil.h"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

std::vector<CelBoil::Color> source(int x0, int y0, int width, int height) {
  std::vector<CelBoil::Color> image(std::size_t(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int u = x0 + x, v = y0 + y;
      if (u < 0 || v < 0 || u >= 48 || v >= 32) continue;
      const bool line = std::abs(u - 12) < 2 || std::abs(v - 16) < 2;
      image[std::size_t(y) * width + x] =
          line ? CelBoil::Color{0.05, 0.05, 0.05, 1}
               : CelBoil::Color{0.6 + u / 200.0, 0.4 + v / 200.0, 0.2, 1};
    }
  }
  return image;
}

bool equal(const CelBoil::Color& a, const CelBoil::Color& b) {
  return std::abs(a.r - b.r) < 1e-12 && std::abs(a.g - b.g) < 1e-12 &&
         std::abs(a.b - b.b) < 1e-12 && std::abs(a.a - b.a) < 1e-12;
}

std::vector<CelBoil::Color> render(const CelBoil::Settings& settings,
                                   double frame) {
  const int halo  = CelBoil::halo(settings);
  const int width = 48 + 2 * halo, height = 32 + 2 * halo;
  std::vector<CelBoil::Color> output;
  require(CelBoil::render(source(-halo, -halo, width, height), width, height,
                          -halo, -halo, settings, frame, output),
          "render did not complete");
  return output;
}

void tileTest(CelBoil::Target target) {
  CelBoil::Settings settings;
  settings.target    = target;
  settings.amplitude = 4.25;
  settings.detail    = 0.7;
  settings.seed      = 17;
  const int halo = CelBoil::halo(settings), width = 48 + 2 * halo;
  const auto whole = render(settings, 3);
  for (int top : {0, 16}) {
    for (int left : {0, 24}) {
      const int w = 24 + 2 * halo, h = 16 + 2 * halo;
      std::vector<CelBoil::Color> tile;
      require(CelBoil::render(source(left - halo, top - halo, w, h), w, h,
                              left - halo, top - halo, settings, 3, tile),
              "tile render did not complete");
      for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 24; ++x)
          require(
              equal(
                  whole[std::size_t(top + y + halo) * width + left + x + halo],
                  tile[std::size_t(y + halo) * w + x + halo]),
              "splitting a frame changed a rendered pixel");
    }
  }
}

void timingTest() {
  CelBoil::Settings settings;
  const auto first = render(settings, 0), held = render(settings, 1);
  const auto next = render(settings, 2);
  bool changed    = false;
  for (std::size_t i = 0; i < first.size(); ++i) {
    require(equal(first[i], held[i]), "held frames changed their wobble");
    changed = changed || !equal(first[i], next[i]);
  }
  require(changed, "next cycle did not change the rendered image");
}

void cancellationTest() {
  CelBoil::Settings settings;
  int canceled = 1;
  std::vector<CelBoil::Color> output(1, {1, 1, 1, 1});
  require(!CelBoil::render(source(0, 0, 8, 8), 8, 8, 0, 0, settings, 0, output,
                           &canceled),
          "canceled render reported completion");
  require(output.empty(), "canceled render retained partial pixels");
}
}  // namespace

int main() {
  try {
    tileTest(CelBoil::Target::Image);
    tileTest(CelBoil::Target::Lines);
    timingTest();
    cancellationTest();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  std::cout << "Cel Boil tile, timing and cancellation checks passed\n";
}
