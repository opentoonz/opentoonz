#pragma once

#include "tpixel.h"
#include "tpixelcm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <vector>

// The reducer works on palette entries, not rendered RGB pixels. Keeping an
// existing style per cluster leaves palette definitions and tone data intact.
namespace PaletteColorReduction {

using StyleMap = std::array<int, 4096>;
using Usage    = std::array<double, 4096>;
using Used     = std::array<bool, 4096>;

struct Color {
  int id;
  TPixel32 rgba;
};

struct Plan {
  StyleMap styles;
  std::vector<int> protectedStyles;
  double tolerance = 0;
  int before = 0, after = 0, minimum = 0;
  bool canceled = false;

  Plan() { std::iota(styles.begin(), styles.end(), 0); }
};

inline bool remap(TPixelCM32 &pixel, const StyleMap &styles) {
  const int ink = styles[pixel.getInk()], paint = styles[pixel.getPaint()];
  if (ink == pixel.getInk() && paint == pixel.getPaint()) return false;
  // Include hidden slots: leaving an old ID under solid ink would prevent
  // Delete Unused Styles from recognizing that the style is unused.
  pixel.setInk(ink);
  pixel.setPaint(paint);
  return true;
}

inline void count(const TPixelCM32 &pixel, Usage &usage, Used &used) {
  used[pixel.getInk()] = used[pixel.getPaint()] = true;
  usage[pixel.getInk()] += TPixelCM32::getMaxTone() - pixel.getTone();
  usage[pixel.getPaint()] += pixel.getTone();
}

// Predict current-level usage before applying any edits. Other levels sharing
// the palette must contribute their original IDs to this result, not remapped
// IDs: their drawings are not part of the reduction.
inline Used remappedUsage(const Used &used, const StyleMap &styles) {
  Used result{};
  for (size_t id = 0; id < used.size(); ++id)
    if (used[id]) result[styles[id]] = true;
  return result;
}

// Compact visible chips in page order while keeping reserved IDs fixed.
// Retain unpaged slots at the end, including any hidden pixel references.
// A full permutation lets Undo restore the original indices without loss.
inline StyleMap makeRenumberMap(const std::vector<std::vector<int>> &pages,
                                int styleCount, const Used &removed) {
  StyleMap map;
  std::iota(map.begin(), map.end(), 0);
  Used assigned{};
  assigned[0] = assigned[1] = true;
  int next                  = 2;
  const auto append         = [&](int id) {
    if (id < 2 || id >= styleCount || id >= int(map.size()) || assigned[id])
      return;
    assigned[id] = true;
    map[id]      = next++;
  };
  for (const auto &page : pages)
    for (int id : page)
      if (id >= 0 && id < int(removed.size()) && !removed[id]) append(id);
  for (int id = 2; id < styleCount && id < int(map.size()); ++id) append(id);
  return map;
}

namespace detail {

using Lab = std::array<double, 3>;

inline Lab toLab(const TPixel32 &c) {
  const auto linear = [](int value) {
    double v = value / 255.0;
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
  };
  const double r = linear(c.r), g = linear(c.g), b = linear(c.b);
  // Oklab by Bjorn Ottosson (public-domain reference implementation):
  // https://bottosson.github.io/posts/oklab/#converting-from-linear-srgb-to-oklab
  const double l =
      std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const double m =
      std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const double s =
      std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return {{0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
           1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
           0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s}};
}

struct Group {
  std::vector<int> ids;
  int representative = -1;
  int alpha          = 0;
  double weight      = 0;
  Lab lab;
};

struct Box {
  std::vector<int> groups;
  Lab center;
  int axis     = 0;
  double error = 0;
};

inline void measure(Box &box, const std::vector<Group> &groups) {
  box.center    = {{0, 0, 0}};
  double weight = 0;
  for (int i : box.groups) {
    weight += groups[i].weight;
    for (int d = 0; d < 3; ++d)
      box.center[d] += groups[i].weight * groups[i].lab[d];
  }
  for (double &v : box.center) v /= weight;
  Lab variance = {{0, 0, 0}};
  for (int i : box.groups)
    for (int d = 0; d < 3; ++d) {
      const double delta = groups[i].lab[d] - box.center[d];
      variance[d] += groups[i].weight * delta * delta;
    }
  box.axis  = int(std::max_element(variance.begin(), variance.end()) -
                 variance.begin());
  box.error = variance[0] + variance[1] + variance[2];
}

}  // namespace detail

// target == 0 only merges identical RGBA colors. Otherwise use deterministic
// coverage-weighted median cut in Oklab, separately for each exact opacity.
// The target is a maximum for colors referenced within the chosen scope;
// unused palette chips and styles outside that scope do not consume it.
inline Plan makePlan(const std::vector<Color> &colors, const Usage &usage,
                     const Used &used, int target,
                     const std::function<bool()> &cancel = {}) {
  using namespace detail;
  Plan plan;
  std::map<unsigned int, Group> duplicates;
  for (const Color &color : colors) {
    const int id = color.id;
    if (id <= 0 || id >= int(plan.styles.size())) continue;
    const TPixel32 &c      = color.rgba;
    const unsigned int key = (unsigned(c.r) << 24) | (unsigned(c.g) << 16) |
                             (unsigned(c.b) << 8) | unsigned(c.m);
    Group &group = duplicates[key];
    group.ids.push_back(id);
    if (!used[id]) continue;
    ++plan.before;
    if (group.representative < 0 || usage[id] > usage[group.representative] ||
        (usage[id] == usage[group.representative] &&
         id < group.representative)) {
      group.representative = id;
      group.alpha          = c.m;
      group.lab            = toLab(c);
    }
    group.weight += usage[id];
  }

  std::vector<Group> groups;
  std::map<int, Box> opacityBoxes;
  for (auto &entry : duplicates) {
    Group &group = entry.second;
    if (group.representative < 0) continue;
    // Hidden-only references still need remapping, but should not dominate
    // colors which actually contribute to a drawing.
    group.weight = std::max(group.weight, 1.0);
    for (int id : group.ids) plan.styles[id] = group.representative;
    opacityBoxes[group.alpha].groups.push_back(int(groups.size()));
    groups.push_back(std::move(group));
  }
  plan.after   = int(groups.size());
  plan.minimum = int(opacityBoxes.size());
  if (target <= 0 || target >= plan.after || target < plan.minimum) return plan;

  std::vector<Box> boxes;
  for (auto &entry : opacityBoxes) {
    measure(entry.second, groups);
    boxes.push_back(std::move(entry.second));
  }
  while (int(boxes.size()) < target) {
    if (cancel && cancel()) {
      plan.canceled = true;
      return plan;
    }
    int best = -1;
    for (int i = 0; i < int(boxes.size()); ++i)
      if (boxes[i].groups.size() > 1 &&
          (best < 0 || boxes[i].error > boxes[best].error))
        best = i;
    if (best < 0) break;
    Box &box       = boxes[best];
    const int axis = box.axis;
    std::sort(box.groups.begin(), box.groups.end(), [&](int a, int b) {
      if (groups[a].lab[axis] != groups[b].lab[axis])
        return groups[a].lab[axis] < groups[b].lab[axis];
      return groups[a].representative < groups[b].representative;
    });
    double total = 0, cumulative = 0;
    for (int i : box.groups) total += groups[i].weight;
    size_t split = 0;
    do {
      cumulative += groups[box.groups[split++]].weight;
    } while (split < box.groups.size() - 1 && cumulative < total * 0.5);
    Box other;
    other.groups.assign(box.groups.begin() + split, box.groups.end());
    box.groups.resize(split);
    measure(box, groups);
    measure(other, groups);
    boxes.push_back(std::move(other));
  }
  for (const Box &box : boxes) {
    int best            = -1;
    double bestDistance = 0;
    for (int i : box.groups) {
      double distance = 0;
      for (int d = 0; d < 3; ++d) {
        double delta = groups[i].lab[d] - box.center[d];
        distance += delta * delta;
      }
      if (best < 0 || distance < bestDistance ||
          (distance == bestDistance &&
           groups[i].representative < groups[best].representative)) {
        best         = i;
        bestDistance = distance;
      }
    }
    for (int i : box.groups)
      for (int id : groups[i].ids)
        plan.styles[id] = groups[best].representative;
  }
  plan.after = int(boxes.size());
  return plan;
}

// A Pareto-inspired heuristic, not a guarantee of visual importance: keep
// ceil(distinct used colors / 5) representatives, or more to preserve opacity.
// Pixel coverage and color separation estimate which colors matter most.
// tolerance < 0 calculates the smallest distance to this protected set that
// reaches that goal. A manual tolerance can retain additional colors.
inline Plan makeSimilarityPlan(const std::vector<Color> &colors,
                               const Usage &usage, const Used &used,
                               double tolerance                    = -1,
                               const std::function<bool()> &cancel = {}) {
  using namespace detail;
  Plan plan = makePlan(colors, usage, used, 0);
  if (cancel && cancel()) {
    plan.canceled = true;
    return plan;
  }
  // Reuse exact-color deduplication so duplicates and unused chips do not
  // inflate the 20% budget. Accumulate in ID order for deterministic weights.
  std::map<int, Color> ordered;
  for (const Color &color : colors)
    if (color.id > 0 && color.id < int(used.size()) && used[color.id])
      ordered.emplace(color.id, color);
  std::map<int, Group> distinct;
  for (const auto &entry : ordered) {
    const Color &color   = entry.second;
    const int id         = plan.styles[color.id];
    Group &group         = distinct[id];
    group.representative = id;
    group.alpha          = color.rgba.m;
    group.lab            = toLab(color.rgba);
    group.weight += usage[color.id];
  }
  if (distinct.empty()) return plan;
  std::vector<Group> groups;
  double maxWeight = 1;
  for (auto &entry : distinct) {
    entry.second.weight = std::max(1.0, entry.second.weight);
    maxWeight           = std::max(maxWeight, entry.second.weight);
    groups.push_back(std::move(entry.second));
  }

  const int goal = std::max(plan.minimum, (int(groups.size()) + 4) / 5);
  std::vector<bool> protectedColor(groups.size(), false);
  std::vector<int> nearest(groups.size(), -1);
  std::vector<double> distance(groups.size(),
                               std::numeric_limits<double>::infinity());
  const auto protect = [&](int anchor) {
    protectedColor[anchor] = true;
    nearest[anchor]        = anchor;
    distance[anchor]       = 0;
    plan.protectedStyles.push_back(groups[anchor].representative);
    for (int i = 0; i < int(groups.size()); ++i) {
      if (protectedColor[i] || groups[i].alpha != groups[anchor].alpha)
        continue;
      double squared = 0;
      for (int d = 0; d < 3; ++d) {
        const double delta = groups[i].lab[d] - groups[anchor].lab[d];
        squared += delta * delta;
      }
      if (squared < distance[i] ||
          (squared == distance[i] &&
           groups[anchor].representative < groups[nearest[i]].representative)) {
        distance[i] = squared;
        nearest[i]  = anchor;
      }
    }
  };
  // Every opacity group needs a survivor. Start with its greatest coverage.
  std::map<int, int> seeds;
  for (int i = 0; i < int(groups.size()); ++i) {
    auto seed = seeds.find(groups[i].alpha);
    if (seed == seeds.end() || groups[i].weight > groups[seed->second].weight)
      seeds[groups[i].alpha] = i;
  }
  for (const auto &seed : seeds) {
    if (cancel && cancel()) {
      plan.canceled = true;
      return plan;
    }
    protect(seed.second);
  }
  while (int(plan.protectedStyles.size()) < goal) {
    if (cancel && cancel()) {
      plan.canceled = true;
      return plan;
    }
    int best         = -1;
    double bestScore = -1;
    for (int i = 0; i < int(groups.size()); ++i) {
      if (protectedColor[i]) continue;
      // Temper coverage weighting to leave room for small, distinct accents.
      const double score =
          distance[i] * std::sqrt(groups[i].weight / maxWeight);
      if (score > bestScore) {
        best      = i;
        bestScore = score;
      }
    }
    protect(best);
  }
  // Map directly to a protected color. Chaining through another merged color
  // could exceed the user's tolerance even if each individual hop is close.
  const double threshold =
      tolerance < 0 ? *std::max_element(distance.begin(), distance.end())
                    : tolerance * tolerance;
  plan.tolerance = tolerance < 0 ? std::sqrt(threshold) : tolerance;
  StyleMap survivors;
  std::iota(survivors.begin(), survivors.end(), 0);
  plan.after = goal;
  for (int i = 0; i < int(groups.size()); ++i) {
    if (protectedColor[i]) continue;
    if (distance[i] <= threshold)
      survivors[groups[i].representative] = groups[nearest[i]].representative;
    else
      ++plan.after;
  }
  for (int &id : plan.styles) id = survivors[id];
  return plan;
}

}  // namespace PaletteColorReduction
