#include "flowdisplace.h"

#include <array>
#include <iostream>
#include <limits>
#include <string>

namespace {
using FlowDisplace::Color;
using FlowDisplace::Settings;

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

bool equal(const Color &a, const Color &b) {
  return std::abs(a.r - b.r) < 1e-12 && std::abs(a.g - b.g) < 1e-12 &&
         std::abs(a.b - b.b) < 1e-12 && std::abs(a.a - b.a) < 1e-12;
}

Color sourceAt(int x, int y) {
  if (x < 0 || y < 0 || x >= 64 || y >= 24) return {};
  return {double(x % 7) / 7, double(y % 5) / 5, 0.25, 1.0};
}

std::vector<Color> tile(std::array<int, 4> bounds, const Settings &settings,
                        const Color &field) {
  const auto [x, y, w, h] = bounds;
  const int halo          = FlowDisplace::halo(settings);
  const int width = w + 2 * halo, height = h + 2 * halo;
  std::vector<Color> source(width * height), fields(width * height, field);
  for (int row = 0; row < height; ++row)
    for (int column = 0; column < width; ++column)
      source[row * width + column] =
          sourceAt(x + column - halo, y + row - halo);
  std::vector<Color> result;
  require(FlowDisplace::render(source, fields, {}, width, height, x - halo,
                               y - halo, settings, 3.25, result),
          "tile render canceled unexpectedly");
  std::vector<Color> cropped(w * h);
  for (int row = 0; row < h; ++row)
    for (int column = 0; column < w; ++column)
      cropped[row * w + column] = result[(row + halo) * width + column + halo];
  return cropped;
}

void identityTest() {
  Settings settings;
  const auto result = tile({0, 0, 64, 24}, settings, {});
  for (int y = 0; y < 24; ++y)
    for (int x = 0; x < 64; ++x)
      require(equal(result[y * 64 + x], sourceAt(x, y)),
              "unconnected field must preserve the source");
  settings.useMatte = true;
  const auto masked = tile({0, 0, 64, 24}, settings, {1, 0, 0, 1});
  for (std::size_t i = 0; i < result.size(); ++i)
    require(equal(masked[i], result[i]),
            "empty matte must prevent displacement");
}

void fieldTest() {
  Settings settings;
  for (auto mode : {FlowDisplace::Mode::Flow, FlowDisplace::Mode::Map}) {
    settings.mode = mode;
    for (double red : {2.0, double(std::numeric_limits<float>::max())}) {
      const auto bounded = tile({0, 0, 64, 24}, settings, {1, 0.5, 0, 1});
      const auto hdr     = tile({0, 0, 64, 24}, settings, {red, 0.5, 0, 1});
      for (std::size_t i = 0; i < bounded.size(); ++i)
        require(equal(hdr[i], bounded[i]),
                "HDR field must stay within its full-scale motion");
    }
    const auto bounded  = tile({0, 0, 64, 24}, settings, {0, 0.5, 0, 1});
    const auto negative = tile({0, 0, 64, 24}, settings, {-2, 0.5, 0, 1});
    for (std::size_t i = 0; i < bounded.size(); ++i)
      require(equal(negative[i], bounded[i]),
              "negative field must stay within full-scale motion");
  }
}

void tilesTest() {
  Settings settings;
  settings.noiseAmplitude = 3.0;
  for (const Color field : {Color{0.75, 0.3, 0.1, 1}, Color{2, 0.5, 0, 1}}) {
    const auto whole = tile({0, 0, 64, 24}, settings, field);
    for (int y = 0; y < 24; y += 12)
      for (int x = 0; x < 64; x += 16) {
        const auto part = tile({x, y, 16, 12}, settings, field);
        for (int row = 0; row < 12; ++row)
          for (int column = 0; column < 16; ++column)
            require(equal(part[row * 16 + column],
                          whole[(y + row) * 64 + x + column]),
                    "render tiling changed a displaced pixel");
      }
  }
}

void scaleTest() {
  for (double size : {1.0, 20.0, 100.0}) {
    Settings settings;
    settings.noiseSize      = size;
    settings.noiseAmplitude = 8.0;
    for (double scale : {0.25, 0.5, 2.0, 4.0}) {
      Settings scaled = settings;
      scaled.noiseSize *= scale;
      scaled.noiseAmplitude *= scale;
      double dx, dy, scaledDx, scaledDy;
      FlowDisplace::displacement(settings, {}, nullptr, 51.25, 17.75, 3.5, dx,
                                 dy);
      FlowDisplace::displacement(scaled, {}, nullptr, 51.25 * scale,
                                 17.75 * scale, 3.5, scaledDx, scaledDy);
      require(std::abs(dx - scaledDx / scale) < 1e-12 &&
                  std::abs(dy - scaledDy / scale) < 1e-12,
              "noise changes its scene scale with the render resolution");
    }
  }
}

void invalidTest() {
  Settings settings;
  std::vector<Color> output;
  bool rejected = false;
  try {
    FlowDisplace::render({}, {}, {}, 1, 1, 0, 0, settings, 0, output);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "mismatched image size must be rejected");
  const int canceled = 1;
  require(!FlowDisplace::render({Color{}}, {}, {}, 1, 1, 0, 0, settings, 0,
                                output, &canceled) &&
              output.empty(),
          "canceled rendering must clear its output");
}
}  // namespace

int main(int argc, char **argv) {
  try {
    require(argc == 2, "provide a test case");
    const std::string test = argv[1];
    if (test == "identity")
      identityTest();
    else if (test == "field")
      fieldTest();
    else if (test == "tiles")
      tilesTest();
    else if (test == "scale")
      scaleTest();
    else if (test == "invalid")
      invalidTest();
    else
      throw std::runtime_error("unknown test case");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
