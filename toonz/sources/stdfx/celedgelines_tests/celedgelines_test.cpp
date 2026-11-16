#include "celedgelines.h"

#include <array>
#include <iostream>
#include <string>

namespace {
using CelEdge::Color;
using CelEdge::Rule;
using CelEdge::Settings;

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

bool equal(const Color &a, const Color &b) {
  return std::abs(a.r - b.r) < 1e-12 && std::abs(a.g - b.g) < 1e-12 &&
         std::abs(a.b - b.b) < 1e-12 && std::abs(a.a - b.a) < 1e-12;
}

Color sourceAt(int x, int y) {
  if (x < 0 || y < 0 || x >= 32 || y >= 24) return {};
  return x < 16 ? Color{1, 0, 0, 1} : Color{0, 0, 1, 1};
}

std::vector<Color> tile(std::array<int, 4> bounds, const Settings &settings,
                        const std::vector<Rule> &rules) {
  const auto [x, y, w, h] = bounds;
  const int halo          = CelEdge::halo(settings, rules);
  const int width = w + 2 * halo, height = h + 2 * halo;
  std::vector<Color> source(width * height);
  for (int row = 0; row < height; ++row)
    for (int column = 0; column < width; ++column)
      source[row * width + column] =
          sourceAt(x + column - halo, y + row - halo);
  std::vector<Color> result;
  require(CelEdge::render(source, width, height, settings, rules, result),
          "edge rendering canceled unexpectedly");
  std::vector<Color> cropped(w * h);
  for (int row = 0; row < h; ++row)
    for (int column = 0; column < w; ++column)
      cropped[row * w + column] = result[(row + halo) * width + column + halo];
  return cropped;
}

void identityTest() {
  Settings settings;
  for (bool linesOnly : {false, true}) {
    settings.linesOnly = linesOnly;
    const auto result  = tile({0, 0, 32, 24}, settings, {});
    for (int y = 0; y < 24; ++y)
      for (int x = 0; x < 32; ++x)
        require(equal(result[y * 32 + x], linesOnly ? Color{} : sourceAt(x, y)),
                "no rules must produce source or empty lines");
  }
}

void rulesTest() {
  Settings settings;
  settings.linesOnly = true;
  Rule rule;
  rule.enabled      = true;
  rule.weight       = 4;
  rule.line         = {0, 1, 0, 1};
  rule.first        = {CelEdge::Kind::Color, 1, 0, 0};
  rule.second       = {CelEdge::Kind::Color, 0, 0, 1};
  const auto result = tile({0, 0, 32, 24}, settings, {rule});
  require(equal(result[12 * 32 + 15], {0, 1, 0, 1}),
          "matching region boundary must receive the specified line color");
  require(result[12 * 32].a == 0,
          "color rule must not match transparent border");
  std::swap(rule.first, rule.second);
  const auto reversed = tile({0, 0, 32, 24}, settings, {rule});
  for (std::size_t i = 0; i < result.size(); ++i)
    require(equal(result[i], reversed[i]),
            "rule direction must not change its result");
  Rule later          = rule;
  later.line          = {1, 0, 0, 1};
  const auto priority = tile({0, 0, 32, 24}, settings, {rule, later});
  for (std::size_t i = 0; i < result.size(); ++i)
    require(equal(result[i], priority[i]),
            "first matching rule must take precedence");
}

void tilesTest() {
  Settings settings;
  Rule boundary;
  boundary.enabled    = true;
  boundary.weight     = 4.5;
  Rule outline        = boundary;
  outline.second.kind = CelEdge::Kind::Transparent;
  outline.weight      = 6.5;
  const std::vector<Rule> rules{boundary, outline};
  for (bool linesOnly : {false, true}) {
    settings.linesOnly = linesOnly;
    const auto whole   = tile({-4, -4, 40, 32}, settings, rules);
    for (int y = 0; y < 32; y += 8)
      for (int x = 0; x < 40; x += 10) {
        const auto part = tile({x - 4, y - 4, 10, 8}, settings, rules);
        for (int row = 0; row < 8; ++row)
          for (int column = 0; column < 10; ++column)
            require(equal(part[row * 10 + column],
                          whole[(y + row) * 40 + x + column]),
                    "render tiling changed an edge line pixel");
      }
  }
}

void invalidTest() {
  Settings settings;
  std::vector<Color> output;
  bool rejected = false;
  try {
    CelEdge::render({}, 1, 1, settings, {}, output);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  require(rejected, "mismatched image size must be rejected");
  const int canceled = 1;
  require(!CelEdge::render({Color{}}, 1, 1, settings, {}, output, &canceled) &&
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
    else if (test == "rules")
      rulesTest();
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
