#include "printprocess.h"

#include <array>
#include <iostream>
#include <string>

namespace {
using PrintProcess::Color;
using PrintProcess::Settings;

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

bool equal(const Color &a, const Color &b) {
  return std::abs(a.r - b.r) < 1e-12 && std::abs(a.g - b.g) < 1e-12 &&
         std::abs(a.b - b.b) < 1e-12 && std::abs(a.a - b.a) < 1e-12;
}

Color sourceAt(int x, int y) {
  if (x < 0 || y < 0 || x >= 32 || y >= 24) return {};
  const double alpha = x < 16 ? 0.5 : 1;
  return {alpha * double(x % 7) / 7, alpha * double(y % 5) / 5, 0.25 * alpha,
          alpha};
}

std::vector<Color> tile(std::array<int, 4> bounds, const Settings &settings) {
  const auto [x, y, w, h] = bounds;
  const int halo          = PrintProcess::halo(settings);
  const int width = w + 2 * halo, height = h + 2 * halo;
  std::vector<Color> source(width * height);
  for (int row = 0; row < height; ++row)
    for (int column = 0; column < width; ++column)
      source[row * width + column] =
          sourceAt(x + column - halo, y + row - halo);
  std::vector<Color> result;
  require(PrintProcess::render(source, width, height, x - halo, y - halo,
                               settings, result),
          "print canceled unexpectedly");
  std::vector<Color> cropped(w * h);
  for (int row = 0; row < h; ++row)
    for (int column = 0; column < w; ++column)
      cropped[row * w + column] = result[(row + halo) * width + column + halo];
  return cropped;
}

void platesTest() {
  Settings settings;
  settings.grain = 0;
  for (auto &plate : settings.plates) plate.enabled = false;
  const auto paper = tile({0, 0, 32, 24}, settings);
  for (const auto &pixel : paper)
    require(
        equal(pixel, {settings.paper.r, settings.paper.g, settings.paper.b, 1}),
        "disabled plates must leave paper unchanged");
  for (int plate = 0; plate < PrintProcess::plateCount; ++plate) {
    settings.output = PrintProcess::Output(plate + 1);
    for (const auto &pixel : tile({0, 0, 32, 24}, settings))
      require(equal(pixel, {1, 1, 1, 1}),
              "disabled plate output must be white");
  }
  settings.plates[0].enabled = true;
  settings.paper             = {1, 1, 1};
  settings.output            = PrintProcess::Output::Print;
  std::vector<Color> cyan(25, {0, 1, 1, 1}), printed;
  require(PrintProcess::render(cyan, 5, 5, 0, 0, settings, printed),
          "solid cyan rendering failed");
  const auto ink = settings.plates[0].ink;
  require(equal(printed[12], {ink.r, ink.g, ink.b, 1}),
          "solid cyan must fully cover the paper with cyan ink");
  settings.output = PrintProcess::Output::Cyan;
  require(PrintProcess::render(cyan, 5, 5, 0, 0, settings, printed),
          "solid cyan plate rendering failed");
  require(equal(printed[12], {0, 0, 0, 1}),
          "solid cyan plate must print black in the coverage output");
}

void transparentTest() {
  Settings settings;
  settings.transparentBackground = true;
  settings.plates[0].offsetX     = 3.25;
  for (auto output :
       {PrintProcess::Output::Print, PrintProcess::Output::Cyan}) {
    settings.output  = output;
    const auto image = tile({-4, -4, 40, 32}, settings);
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 40; ++x) {
        const Color &pixel = image[y * 40 + x];
        require(pixel.a == sourceAt(x - 4, y - 4).a,
                "transparent print must preserve source alpha");
        require(pixel.r >= 0 && pixel.g >= 0 && pixel.b >= 0 &&
                    pixel.r <= pixel.a && pixel.g <= pixel.a &&
                    pixel.b <= pixel.a,
                "transparent output must stay premultiplied");
      }
  }
}

void tilesTest() {
  Settings settings;
  settings.plates[0].offsetX = 3.25;
  settings.plates[1].offsetY = -2.75;
  for (auto shape : {PrintProcess::Shape::Round, PrintProcess::Shape::Diamond,
                     PrintProcess::Shape::Line}) {
    settings.shape   = shape;
    const auto whole = tile({0, 0, 32, 24}, settings);
    for (int y = 0; y < 24; y += 12)
      for (int x = 0; x < 32; x += 16) {
        const auto part = tile({x, y, 16, 12}, settings);
        for (int row = 0; row < 12; ++row)
          for (int column = 0; column < 16; ++column)
            require(equal(part[row * 16 + column],
                          whole[(y + row) * 32 + x + column]),
                    "render tiling changed a printed pixel");
      }
  }
}

void invalidTest() {
  Settings settings;
  std::vector<Color> output;
  bool rejected = false;
  try {
    PrintProcess::render({}, 1, 1, 0, 0, settings, output);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "mismatched image size must be rejected");
  const int canceled = 1;
  require(!PrintProcess::render({Color{}}, 1, 1, 0, 0, settings, output,
                                &canceled) &&
              output.empty(),
          "canceled rendering must clear its output");
}
}  // namespace

int main(int argc, char **argv) {
  try {
    require(argc == 2, "provide a test case");
    const std::string test = argv[1];
    if (test == "plates")
      platesTest();
    else if (test == "transparent")
      transparentTest();
    else if (test == "tiles")
      tilesTest();
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
