#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

// Native pixel-center distances. All transforms are linear in the image area;
// neither running time nor scratch storage grows with the requested radius.
namespace CelDistance {

enum class Metric { Box, Approximate, Euclidean };
enum class Direction { Inside, Around, Outside };

struct Field {
  int width  = 0;
  int height = 0;
  std::vector<double> distance;
  std::vector<int> nearest;
};

inline bool isCanceled(const int *canceled) {
  return canceled && *canceled != 0;
}

inline std::size_t checkedArea(int width, int height) {
  if (width < 0 || height < 0)
    throw std::invalid_argument("Negative distance-field dimensions");
  const std::size_t w = static_cast<std::size_t>(width);
  const std::size_t h = static_cast<std::size_t>(height);
  // The nearest-source representation is a signed, row-major integer index.
  if (h && w > static_cast<std::size_t>(std::numeric_limits<int>::max()) / h)
    throw std::length_error("Distance field exceeds its index range");
  return w * h;
}

namespace detail {

inline bool validMetric(Metric metric) {
  return metric == Metric::Box || metric == Metric::Approximate ||
         metric == Metric::Euclidean;
}

inline bool abort(Field &field, const int *canceled) {
  if (!isCanceled(canceled)) return false;
  field = Field();
  return true;
}

inline int borderDistance(int x, int y, int width, int height) {
  return std::min(std::min(x + 1, width - x), std::min(y + 1, height - y));
}

inline std::int64_t square(std::int64_t value) { return value * value; }

inline std::int64_t floorDiv(std::int64_t numerator, std::int64_t denominator) {
  const std::int64_t quotient = numerator / denominator;
  return quotient - (numerator % denominator < 0 ? 1 : 0);
}

inline void relax(Field &field, int destination, int source, int cost) {
  const int label = field.nearest[source];
  if (label < 0) return;
  const double candidate = field.distance[source] + cost;
  if (candidate < field.distance[destination] ||
      (candidate == field.distance[destination] &&
       (field.nearest[destination] < 0 ||
        label < field.nearest[destination]))) {
    field.distance[destination] = candidate;
    field.nearest[destination]  = label;
  }
}

}  // namespace detail

// Nonzero mask cells are seeds. No seed means infinity and nearest == -1.
// Equal-distance in-domain seeds choose the lowest row-major index. When
// outsideIsSeed is true, off-domain pixel centers are seeds with index -1;
// actual in-domain seeds win ties with them. Box is Chebyshev distance;
// Approximate is the 3-4 chamfer metric (axis 1, diagonal 4/3).
// Cancellation clears the result and returns false. Invalid inputs throw.
inline bool transform(const std::vector<std::uint8_t> &seeds, int width,
                      int height, Metric metric, Field &out,
                      const int *canceled = nullptr,
                      bool outsideIsSeed  = false) {
  const std::size_t area = checkedArea(width, height);
  if (seeds.size() != area || !detail::validMetric(metric))
    throw std::invalid_argument("Invalid distance-field input");
  if (detail::abort(out, canceled)) return false;
  out.width  = width;
  out.height = height;
  out.distance.assign(area, std::numeric_limits<double>::infinity());
  out.nearest.assign(area, -1);
  if (!area) return !detail::abort(out, canceled);

  if (metric != Metric::Euclidean) {
    for (std::size_t i = 0; i < area; ++i) {
      if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
      if (seeds[i]) {
        out.distance[i] = 0.0;
        out.nearest[i]  = static_cast<int>(i);
      }
    }
    const int diagonal = metric == Metric::Box ? 3 : 4;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int i = y * width + x;
        if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
        if (x) detail::relax(out, i, i - 1, 3);
        if (y) {
          detail::relax(out, i, i - width, 3);
          if (x) detail::relax(out, i, i - width - 1, diagonal);
          if (x + 1 < width) detail::relax(out, i, i - width + 1, diagonal);
        }
      }
    }
    for (int y = height - 1; y >= 0; --y) {
      for (int x = width - 1; x >= 0; --x) {
        const int i = y * width + x;
        if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
        if (x + 1 < width) detail::relax(out, i, i + 1, 3);
        if (y + 1 < height) {
          detail::relax(out, i, i + width, 3);
          if (x) detail::relax(out, i, i + width - 1, diagonal);
          if (x + 1 < width) detail::relax(out, i, i + width + 1, diagonal);
        }
      }
    }
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int i = y * width + x;
        if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
        if (outsideIsSeed) {
          const double exterior =
              3.0 * detail::borderDistance(x, y, width, height);
          if (exterior < out.distance[i]) {
            out.distance[i] = exterior;
            out.nearest[i]  = -1;
          }
        }
        out.distance[i] /= 3.0;
      }
    }
    return !detail::abort(out, canceled);
  }

  // First find the nearest seed in each row. A horizontal tie goes left.
  for (int y = 0; y < height; ++y) {
    int last = -1;
    for (int x = 0; x < width; ++x) {
      const int i = y * width + x;
      if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
      if (seeds[i]) last = x;
      if (last >= 0) out.nearest[i] = y * width + last;
    }
    last = -1;
    for (int x = width - 1; x >= 0; --x) {
      const int i = y * width + x;
      if ((i & 1023) == 0 && detail::abort(out, canceled)) return false;
      if (seeds[i]) last = x;
      if (last >= 0 &&
          (out.nearest[i] < 0 || last - x < x - out.nearest[i] % width))
        out.nearest[i] = y * width + last;
    }
  }

  std::vector<int> labels(height), sites(height);
  std::vector<std::int64_t> costs(height), starts(height);
  for (int x = 0; x < width; ++x) {
    if (detail::abort(out, canceled)) return false;
    int count = 0;
    for (int q = 0; q < height; ++q) {
      if ((q & 1023) == 0 && detail::abort(out, canceled)) return false;
      labels[q] = out.nearest[q * width + x];
      if (labels[q] < 0) continue;
      costs[q] =
          detail::square(static_cast<std::int64_t>(x) - labels[q] % width);
      std::int64_t start = std::numeric_limits<std::int64_t>::min();
      while (count) {
        const int previous           = sites[count - 1];
        const std::int64_t numerator = costs[q] + detail::square(q) -
                                       costs[previous] -
                                       detail::square(previous);
        // The later row must be strictly closer. At equality the earlier row
        // contains the smaller row-major source index. Integer boundaries
        // avoid floating-point tie errors in large images.
        start = detail::floorDiv(
                    numerator, 2 * (static_cast<std::int64_t>(q) - previous)) +
                1;
        if (start > starts[count - 1]) break;
        --count;
      }
      if (!count) start = std::numeric_limits<std::int64_t>::min();
      sites[count]  = q;
      starts[count] = start;
      ++count;
    }
    int selected = 0;
    for (int y = 0; y < height; ++y) {
      if ((y & 1023) == 0 && detail::abort(out, canceled)) return false;
      const int i          = y * width + x;
      std::int64_t squared = std::numeric_limits<std::int64_t>::max();
      out.nearest[i]       = -1;
      if (count) {
        while (selected + 1 < count && starts[selected + 1] <= y) ++selected;
        const int q = sites[selected];
        squared = costs[q] + detail::square(static_cast<std::int64_t>(y) - q);
        out.nearest[i] = labels[q];
      }
      if (outsideIsSeed) {
        const int exterior = detail::borderDistance(x, y, width, height);
        if (detail::square(exterior) < squared) {
          squared        = detail::square(exterior);
          out.nearest[i] = -1;
        }
      }
      out.distance[i] = squared == std::numeric_limits<std::int64_t>::max()
                            ? std::numeric_limits<double>::infinity()
                            : std::sqrt(static_cast<double>(squared));
    }
  }
  return !detail::abort(out, canceled);
}

// Positive values expand the selected region; negative values contract it.
// The exterior is unselected, including for a completely selected image.
inline bool thin(const std::vector<std::uint8_t> &mask, int width, int height,
                 double amount, Metric metric, std::vector<std::uint8_t> &out,
                 const int *canceled = nullptr) {
  const std::size_t area = checkedArea(width, height);
  if (mask.size() != area || !std::isfinite(amount) ||
      !detail::validMetric(metric))
    throw std::invalid_argument("Invalid region adjustment input");
  if (isCanceled(canceled)) {
    out.clear();
    return false;
  }
  std::vector<std::uint8_t> result(area);
  Field field;
  if (amount != 0.0) {
    if (amount > 0.0) {
      if (!transform(mask, width, height, metric, field, canceled)) {
        out.clear();
        return false;
      }
    } else {
      for (std::size_t i = 0; i < area; ++i) {
        if ((i & 1023) == 0 && isCanceled(canceled)) {
          out.clear();
          return false;
        }
        result[i] = !mask[i];
      }
      if (!transform(result, width, height, metric, field, canceled, true)) {
        out.clear();
        return false;
      }
    }
  }
  for (std::size_t i = 0; i < area; ++i) {
    if ((i & 1023) == 0 && isCanceled(canceled)) {
      out.clear();
      return false;
    }
    result[i] = amount == 0.0  ? mask[i] != 0
                : amount > 0.0 ? field.distance[i] <= amount
                               : mask[i] && field.distance[i] > -amount;
  }
  if (isCanceled(canceled)) {
    out.clear();
    return false;
  }
  out.swap(result);
  return true;
}

namespace detail {
inline double unit(double value) { return std::max(0.0, std::min(1.0, value)); }
}  // namespace detail

// Native feathering includes the pixel footprint: zero radius is identity,
// and fractional radii change the boundary continuously. Inside uses
// d(background)/(radius+1), Outside uses 1-d(foreground)/(radius+1), and
// Around uses 1/2 +/- (d-1/2)/(2*radius+1). Coverage is clamped to [0,1].
// These are explicit native conventions, not binary compatibility claims.
inline bool feather(const std::vector<std::uint8_t> &mask, int width,
                    int height, double radius, Metric metric,
                    Direction direction, std::vector<double> &out,
                    const int *canceled = nullptr) {
  const std::size_t area = checkedArea(width, height);
  if (mask.size() != area || !std::isfinite(radius) || radius < 0.0 ||
      !detail::validMetric(metric) ||
      (direction != Direction::Inside && direction != Direction::Around &&
       direction != Direction::Outside))
    throw std::invalid_argument("Invalid feather input");
  if (isCanceled(canceled)) {
    out.clear();
    return false;
  }
  out.resize(area);
  for (std::size_t i = 0; i < area; ++i) {
    if ((i & 1023) == 0 && isCanceled(canceled)) {
      out.clear();
      return false;
    }
    out[i] = mask[i] ? 1.0 : 0.0;
  }
  if (radius == 0.0 || area == 0) {
    if (!isCanceled(canceled)) return true;
    out.clear();
    return false;
  }
  // This form avoids overflowing 2*radius for an otherwise finite radius.
  const double aroundScale = 0.5 / (radius + 0.5);
  Field field;
  if (direction != Direction::Outside) {
    std::vector<std::uint8_t> background(area);
    for (std::size_t i = 0; i < area; ++i) {
      if ((i & 1023) == 0 && isCanceled(canceled)) {
        out.clear();
        return false;
      }
      background[i] = !mask[i];
    }
    if (!transform(background, width, height, metric, field, canceled, true)) {
      out.clear();
      return false;
    }
    for (std::size_t i = 0; i < area; ++i) {
      if ((i & 1023) == 0 && isCanceled(canceled)) {
        out.clear();
        return false;
      }
      if (mask[i])
        out[i] =
            detail::unit(direction == Direction::Inside
                             ? field.distance[i] / (radius + 1.0)
                             : 0.5 + (field.distance[i] - 0.5) * aroundScale);
    }
  }
  if (direction != Direction::Inside) {
    if (!transform(mask, width, height, metric, field, canceled)) {
      out.clear();
      return false;
    }
    for (std::size_t i = 0; i < area; ++i) {
      if ((i & 1023) == 0 && isCanceled(canceled)) {
        out.clear();
        return false;
      }
      if (!mask[i])
        out[i] =
            detail::unit(direction == Direction::Outside
                             ? 1.0 - field.distance[i] / (radius + 1.0)
                             : 0.5 - (field.distance[i] - 0.5) * aroundScale);
    }
  }
  if (isCanceled(canceled)) {
    out.clear();
    return false;
  }
  return true;
}

}  // namespace CelDistance
