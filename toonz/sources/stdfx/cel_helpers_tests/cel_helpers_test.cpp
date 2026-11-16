#include "celdistance.h"
#include "celmotion.h"
#include "celnoise.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;

void require(bool condition, const char *message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Operation>
void rejects(Operation operation, const char *message) {
  try {
    operation();
  } catch (const Exception &) {
    ++checks;
    return;
  }
  throw std::runtime_error(message);
}

double distance(int dx, int dy, CelDistance::Metric metric) {
  dx = std::abs(dx);
  dy = std::abs(dy);
  if (metric == CelDistance::Metric::Euclidean) return std::hypot(dx, dy);
  if (metric == CelDistance::Metric::Box) return std::max(dx, dy);
  return (3.0 * std::max(dx, dy) + std::min(dx, dy)) / 3.0;
}

void compareDistances(const std::vector<std::uint8_t> &seeds,
                      CelDistance::Metric metric, bool exterior) {
  CelDistance::Field field;
  require(CelDistance::transform(seeds, 4, 3, metric, field, nullptr, exterior),
          "distance transform completed");
  for (int at = 0; at < 12; ++at) {
    double expected = std::numeric_limits<double>::infinity();
    int nearest     = -1;
    for (int seed = 0; seed < 12; ++seed) {
      if (!seeds[seed]) continue;
      const double candidate =
          distance(at % 4 - seed % 4, at / 4 - seed / 4, metric);
      if (candidate >= expected) continue;
      expected = candidate;
      nearest  = seed;
    }
    const int border = std::min(std::min(at % 4 + 1, 4 - at % 4),
                                std::min(at / 4 + 1, 3 - at / 4));
    if (exterior && border < expected) {
      expected = border;
      nearest  = -1;
    }
    require(field.distance[at] == expected,
            "distance matches exhaustive nearest-seed search");
    require(field.nearest[at] == nearest,
            "nearest seed preserves row-major tie order");
  }
}

void distanceCases() {
  for (unsigned bits = 0; bits < 4096; ++bits) {
    std::vector<std::uint8_t> seeds(12);
    for (unsigned i = 0; i < 12; ++i) seeds[i] = (bits >> i) & 1;
    for (const auto metric :
         {CelDistance::Metric::Box, CelDistance::Metric::Approximate,
          CelDistance::Metric::Euclidean}) {
      compareDistances(seeds, metric, false);
      compareDistances(seeds, metric, true);
    }
  }
  CelDistance::Field field;
  require(CelDistance::transform({}, 0, 0, CelDistance::Metric::Box, field),
          "empty distance field");
  rejects<std::invalid_argument>(
      [&] {
        CelDistance::transform({}, 1, 1, CelDistance::Metric::Box, field);
      },
      "mismatched distance storage must fail");
  rejects<std::invalid_argument>([] { CelDistance::checkedArea(-1, 2); },
                                 "negative dimensions must fail");
  rejects<std::length_error>([] { CelDistance::checkedArea(INT32_MAX, 2); },
                             "overflowing dimensions must fail");
  int canceled = 1;
  require(!CelDistance::transform({1}, 1, 1, CelDistance::Metric::Box, field,
                                  &canceled),
          "distance cancellation stops computation");
  require(field.distance.empty() && field.nearest.empty(),
          "canceled distance output is empty");
}

void regionCases() {
  const std::vector<std::uint8_t> mask{0, 0, 1, 0, 0};
  std::vector<std::uint8_t> expanded;
  require(CelDistance::thin(mask, 5, 1, 1, CelDistance::Metric::Box, expanded),
          "region expansion completes");
  require(expanded == std::vector<std::uint8_t>({0, 1, 1, 1, 0}),
          "unit expansion covers neighbors");
  require(
      CelDistance::thin(expanded, 5, 1, -1, CelDistance::Metric::Box, expanded),
      "in-place contraction completes");
  require(expanded == std::vector<std::uint8_t>(5),
          "contraction sees the exterior of a one-row image");
  std::vector<double> coverage;
  require(CelDistance::feather(mask, 5, 1, 0, CelDistance::Metric::Euclidean,
                               CelDistance::Direction::Around, coverage),
          "zero feather completes");
  require(coverage == std::vector<double>({0, 0, 1, 0, 0}),
          "zero feather is identity");
  require(CelDistance::feather(mask, 5, 1, 1, CelDistance::Metric::Euclidean,
                               CelDistance::Direction::Outside, coverage),
          "outside feather completes");
  require(coverage == std::vector<double>({0, 0.5, 1, 0.5, 0}),
          "feather includes pixel footprint");
}

void motionCases() {
  CelMotion::Image image;
  image.width   = 4;
  image.height  = 3;
  image.originX = -2.5;
  image.originY = 7.25;
  image.pixels.resize(12);
  image.pixels[0] = {2, 0.5, 0, 1};
  image.pixels[5] = {4, 1, 0, 1};
  image.pixels[3] = {0, 0, 1, 1};
  CelMotion::Shapes shapes;
  require(CelMotion::analyze(image, shapes), "motion components complete");
  require(shapes.groups.size() == 2 && shapes.groups[0].count == 2,
          "diagonally connected pixels form one component");
  require(shapes.groups[0].centerX == 0.5 && shapes.groups[0].centerY == 0.5,
          "component center is the covered-pixel centroid");
  require(CelMotion::sample(image, -2, 7.75) ==
              CelMotion::Pixel({1.5, 0.375, 0, 0.5}),
          "bilinear sampling preserves HDR and origin");
  require(CelMotion::sample(image, 100, 100) == CelMotion::Pixel{},
          "out-of-image sampling is clear");
  const int canceled = 1;
  require(
      !CelMotion::analyze(image, shapes, &canceled) && shapes.groups.empty(),
      "canceled analysis clears output");
  image.pixels.clear();
  rejects<std::runtime_error>([&] { CelMotion::analyze(image, shapes); },
                              "malformed motion image must fail");
  rejects<std::runtime_error>([] { CelMotion::checkedArea(INT32_MAX, 2); },
                              "oversized motion image must fail");
}

void noiseCases() {
  CelNoise::Settings settings;
  settings.seed   = 718;
  settings.offset = -2.25;
  for (const auto type : {CelNoise::Smooth, CelNoise::Block}) {
    settings.type = type;
    for (double thickness : {1.0, 10.0, 100.0}) {
      settings.thickness    = thickness;
      const double original = CelNoise::sample(settings, 23.25, -47.75);
      for (double scale : {0.125, 0.5, 2.0, 8.0}) {
        settings.thickness = thickness * scale;
        require(CelNoise::sample(settings, 23.25 * scale, -47.75 * scale) ==
                    original,
                "noise pattern must be independent of render scale");
      }
    }
    for (double thickness :
         {std::numeric_limits<double>::denorm_min(), 1e-300, 1e300}) {
      settings.thickness = thickness;
      const double value = CelNoise::sample(settings, 1e308, -1e308);
      require(std::isfinite(value) && value >= 0 && value <= 1,
              "extreme finite noise is bounded");
    }
  }
  for (double thickness :
       {0.0, -1.0, std::numeric_limits<double>::infinity()}) {
    settings.thickness = thickness;
    require(CelNoise::sample(settings, 2, 3) == 0,
            "invalid noise thickness produces zero");
  }
  settings.type = CelNoise::Layer;
  require(CelNoise::sample(settings, 0, 0, 0.25) == 0.25,
          "layer noise retains supplied luminance");
}
}  // namespace

int main() {
  try {
    distanceCases();
    regionCases();
    motionCases();
    noiseCases();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  std::cout << checks << " cel helper checks passed\n";
}
