

#include "thidelinesegment.h"

#include "tmathutil.h"
#include "tstroke.h"
#include "tvectorimageP.h"

#include <algorithm>

namespace {

bool doublePairCompare(DoublePair p1, DoublePair p2) {
  return p1.first < p2.first;
}

}  // namespace

//-----------------------------------------------------------------------------

void mergeHideLineSegments(std::vector<THideLineSegment> &segments) {
  if (segments.empty()) return;

  std::sort(segments.begin(), segments.end(),
            [](const THideLineSegment &a, const THideLineSegment &b) {
              return a.m_w0 < b.m_w0;
            });

  std::vector<THideLineSegment> merged;
  merged.push_back(segments[0]);

  for (size_t i = 1; i < segments.size(); ++i) {
    THideLineSegment &last = merged.back();
    const THideLineSegment &cur = segments[i];
    if (last.m_mode == cur.m_mode && cur.m_w0 <= last.m_w1 + 1e-3) {
      last.m_w1 = std::max(last.m_w1, cur.m_w1);
    } else {
      merged.push_back(cur);
    }
  }

  segments.swap(merged);
}

namespace {

void replaceHideLineSegmentsInRange(std::vector<THideLineSegment> &segments,
                                    double w0, double w1, THideLineMode mode) {
  if (w1 <= w0 + 1e-6) return;

  std::vector<THideLineSegment> result;
  result.reserve(segments.size() + 1);

  for (const THideLineSegment &seg : segments) {
    if (seg.m_w1 <= w0 + 1e-6 || seg.m_w0 >= w1 - 1e-6) {
      result.push_back(seg);
      continue;
    }
    if (seg.m_w0 < w0 - 1e-3)
      result.emplace_back(seg.m_w0, w0, seg.m_mode);
    if (seg.m_w1 > w1 + 1e-3)
      result.emplace_back(w1, seg.m_w1, seg.m_mode);
  }

  result.emplace_back(w0, w1, mode);
  segments.swap(result);
}

}  // namespace

//-----------------------------------------------------------------------------

void addHideLineSegments(VIStroke *vs, const std::vector<DoublePair> &ranges,
                         THideLineMode mode) {
  if (!vs || ranges.empty()) return;

  for (const DoublePair &range : ranges) {
    replaceHideLineSegmentsInRange(vs->m_hideLineSegments, range.first,
                                 range.second, mode);
  }
  mergeHideLineSegments(vs->m_hideLineSegments);
}

//-----------------------------------------------------------------------------

namespace {

void removeHideLineSegmentsInRange(std::vector<THideLineSegment> &segments,
                                    double w0, double w1) {
  if (w1 <= w0 + 1e-6) return;

  std::vector<THideLineSegment> result;
  result.reserve(segments.size() + 1);

  for (const THideLineSegment &seg : segments) {
    if (seg.m_w1 <= w0 + 1e-6 || seg.m_w0 >= w1 - 1e-6) {
      result.push_back(seg);
      continue;
    }
    if (seg.m_w0 < w0 - 1e-3)
      result.emplace_back(seg.m_w0, w0, seg.m_mode);
    if (seg.m_w1 > w1 + 1e-3)
      result.emplace_back(w1, seg.m_w1, seg.m_mode);
  }

  segments.swap(result);
}

}  // namespace

//-----------------------------------------------------------------------------

void removeHideLineSegments(VIStroke *vs,
                            const std::vector<DoublePair> &ranges) {
  if (!vs || ranges.empty()) return;

  for (const DoublePair &range : ranges)
    removeHideLineSegmentsInRange(vs->m_hideLineSegments, range.first,
                                range.second);
  mergeHideLineSegments(vs->m_hideLineSegments);
}

//-----------------------------------------------------------------------------

std::vector<DoublePair> getVisibleStrokeRanges(
    const std::vector<THideLineSegment> &hideSegments) {
  if (hideSegments.empty()) return {DoublePair(0.0, 1.0)};

  std::vector<DoublePair> hidden;
  hidden.reserve(hideSegments.size());
  for (const THideLineSegment &seg : hideSegments)
    hidden.push_back(DoublePair(seg.m_w0, seg.m_w1));

  std::sort(hidden.begin(), hidden.end(), doublePairCompare);

  std::vector<DoublePair> merged;
  merged.push_back(hidden[0]);
  for (size_t i = 1; i < hidden.size(); ++i) {
    if (merged.back().second < hidden[i].first &&
        !areAlmostEqual(merged.back().second, hidden[i].first, 1e-3)) {
      merged.push_back(hidden[i]);
    } else if (merged.back().second < hidden[i].second) {
      merged.back().second = hidden[i].second;
    }
  }

  std::vector<DoublePair> visible;
  double last = 0.0;
  for (const DoublePair &range : merged) {
    if (!areAlmostEqual(last, range.first, 1e-3))
      visible.push_back(DoublePair(last, range.first));
    last = range.second;
  }
  if (!areAlmostEqual(last, 1.0, 1e-3)) visible.push_back(DoublePair(last, 1.0));

  return visible;
}

//-----------------------------------------------------------------------------

std::vector<DoublePair> getHiddenModeRanges(
    const std::vector<THideLineSegment> &hideSegments) {
  if (hideSegments.empty()) return {};

  std::vector<DoublePair> hidden;
  hidden.reserve(hideSegments.size());
  for (const THideLineSegment &seg : hideSegments) {
    if (seg.m_mode != THideLineMode::Hidden) continue;
    hidden.push_back(DoublePair(seg.m_w0, seg.m_w1));
  }
  if (hidden.empty()) return {};

  std::sort(hidden.begin(), hidden.end(), doublePairCompare);

  std::vector<DoublePair> merged;
  merged.push_back(hidden[0]);
  for (size_t i = 1; i < hidden.size(); ++i) {
    if (merged.back().second < hidden[i].first &&
        !areAlmostEqual(merged.back().second, hidden[i].first, 1e-3)) {
      merged.push_back(hidden[i]);
    } else if (merged.back().second < hidden[i].second) {
      merged.back().second = hidden[i].second;
    }
  }
  return merged;
}

//-----------------------------------------------------------------------------

std::vector<DoublePair> getFillBarrierRanges(
    const std::vector<THideLineSegment> &hideSegments) {
  const std::vector<DoublePair> hidden = getHiddenModeRanges(hideSegments);
  if (hidden.empty()) return {DoublePair(0.0, 1.0)};

  std::vector<DoublePair> barriers;
  double last = 0.0;
  for (const DoublePair &range : hidden) {
    if (range.first > last + 1e-3)
      barriers.push_back(DoublePair(last, range.first));
    last = std::max(last, range.second);
  }
  if (last < 1.0 - 1e-3) barriers.push_back(DoublePair(last, 1.0));
  return barriers;
}

//-----------------------------------------------------------------------------

bool strokeParticipatesInFill(
    const std::vector<THideLineSegment> &hideSegments) {
  for (const DoublePair &range : getFillBarrierRanges(hideSegments)) {
    if (range.second - range.first > 1e-3) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool isIntervalFullyHiddenForFill(
    double w0, double w1,
    const std::vector<THideLineSegment> &hideSegments) {
  if (w1 < w0) std::swap(w0, w1);
  if (w1 <= w0 + 1e-6) return false;

  const std::vector<DoublePair> hidden = getHiddenModeRanges(hideSegments);
  if (hidden.empty()) return false;

  for (const DoublePair &range : hidden) {
    if (w0 >= range.first - 1e-3 && w1 <= range.second + 1e-3) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

std::vector<THideLineSegment> interpolateHideLineSegments(
    const std::vector<THideLineSegment> &seg1,
    const std::vector<THideLineSegment> &seg2, double t) {
  if (seg1.empty() && seg2.empty()) return {};
  if (t <= 0.0) return seg1;
  if (t >= 1.0) return seg2;

  const size_t n = std::max(seg1.size(), seg2.size());
  std::vector<THideLineSegment> out;
  out.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    const bool has1 = i < seg1.size();
    const bool has2 = i < seg2.size();
    if (!has1 && !has2) continue;

    if (has1 && !has2) {
      if (t < 0.5) out.push_back(seg1[i]);
      continue;
    }
    if (!has1 && has2) {
      if (t >= 0.5) out.push_back(seg2[i]);
      continue;
    }

    const THideLineSegment &a = seg1[i];
    const THideLineSegment &b = seg2[i];
    const double w0           = a.m_w0 * (1.0 - t) + b.m_w0 * t;
    const double w1           = a.m_w1 * (1.0 - t) + b.m_w1 * t;
    if (w1 <= w0 + 1e-6) continue;

    THideLineMode mode = (t < 0.5) ? a.m_mode : b.m_mode;
    out.emplace_back(w0, w1, mode);
  }

  mergeHideLineSegments(out);
  return out;
}

//-----------------------------------------------------------------------------

TStroke extractStrokePortion(const TStroke &src, double w0, double w1) {
  TStroke dummy, portion, result;
  if (areAlmostEqual(w0, 0.0, 1e-4))
    portion = src;
  else
    src.split(w0, dummy, portion);

  double lenAtW0 = src.getLength(w0);
  double lenAtW1 = src.getLength(w1);
  double newW1   = portion.getParameterAtLength(lenAtW1 - lenAtW0);

  if (areAlmostEqual(newW1, 1.0, 1e-4))
    result = portion;
  else
    portion.split(newW1, result, dummy);

  return result;
}
