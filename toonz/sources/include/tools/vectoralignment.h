#pragma once

#ifndef VECTOR_ALIGNMENT_INCLUDED
#define VECTOR_ALIGNMENT_INCLUDED

// Adapted from manongjohn's Tahoma2D PR #1275.
// Keep the geometry independent of tools, selection containers and panel UI.
#include "tgeometry.h"
#include <algorithm>
#include <numeric>
#include <vector>

namespace VectorAlignment {

enum Type {
  ALIGN_LEFT,
  ALIGN_RIGHT,
  ALIGN_TOP,
  ALIGN_BOTTOM,
  ALIGN_CENTER_H,
  ALIGN_CENTER_V,
  DISTRIBUTE_H,
  DISTRIBUTE_V
};

enum Method {
  SELECT_AREA,
  FIRST_SELECTED,
  LAST_SELECTED,
  SMALLEST_OBJECT,
  LARGEST_OBJECT,
  CAMERA_AREA
};

inline bool isDistribution(Type type) {
  return type == DISTRIBUTE_H || type == DISTRIBUTE_V;
}

// boxes are in selection order; a group occupies one entry.
inline std::vector<TPointD> offsets(const std::vector<TRectD> &boxes, Type type,
                                    Method method,
                                    const TRectD &camera = TRectD()) {
  std::vector<TPointD> result(boxes.size());
  if (boxes.empty()) return result;
  if (isDistribution(type)) {
    if (method != SELECT_AREA && method != CAMERA_AREA) return result;
    if (boxes.size() < (method == CAMERA_AREA ? 2u : 3u)) return result;
    bool horizontal = type == DISTRIBUTE_H;
    auto center     = [horizontal](const TRectD &box) {
      return horizontal ? (box.x0 + box.x1) * 0.5 : (box.y0 + box.y1) * 0.5;
    };
    std::vector<size_t> sorted(boxes.size());
    std::iota(sorted.begin(), sorted.end(), 0);
    std::stable_sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
      return center(boxes[a]) < center(boxes[b]);
    });
    double first = center(boxes[sorted.front()]);
    double last  = center(boxes[sorted.back()]);
    if (method == CAMERA_AREA) {
      const TRectD &a = boxes[sorted.front()], &b = boxes[sorted.back()];
      first = horizontal ? camera.x0 + a.getLx() * 0.5
                         : camera.y0 + a.getLy() * 0.5;
      last  = horizontal ? camera.x1 - b.getLx() * 0.5
                         : camera.y1 - b.getLy() * 0.5;
      if (last < first) return result;
    }
    double spacing = (last - first) / (boxes.size() - 1);
    for (size_t i = 0; i < sorted.size(); ++i) {
      // Selection-area distribution keeps the two outer objects fixed.
      if (method == SELECT_AREA && (i == 0 || i + 1 == sorted.size())) continue;
      double delta      = first + spacing * i - center(boxes[sorted[i]]);
      result[sorted[i]] = horizontal ? TPointD(delta, 0) : TPointD(0, delta);
    }
    return result;
  }
  if (boxes.size() < 2 && method != CAMERA_AREA) return result;
  TRectD anchor = boxes.front();
  if (method == CAMERA_AREA)
    anchor = camera;
  else if (method == LAST_SELECTED)
    anchor = boxes.back();
  else if (method == SELECT_AREA) {
    for (const TRectD &box : boxes) {
      anchor.x0 = std::min(anchor.x0, box.x0);
      anchor.y0 = std::min(anchor.y0, box.y0);
      anchor.x1 = std::max(anchor.x1, box.x1);
      anchor.y1 = std::max(anchor.y1, box.y1);
    }
  } else if (method == SMALLEST_OBJECT || method == LARGEST_OBJECT) {
    for (const TRectD &box : boxes) {
      double area       = box.getLx() * box.getLy();
      double anchorArea = anchor.getLx() * anchor.getLy();
      if ((method == SMALLEST_OBJECT && area < anchorArea) ||
          (method == LARGEST_OBJECT && area > anchorArea))
        anchor = box;
    }
  }
  for (size_t i = 0; i < boxes.size(); ++i) {
    const TRectD &box = boxes[i];
    switch (type) {
    case ALIGN_LEFT:
      result[i].x = anchor.x0 - box.x0;
      break;
    case ALIGN_RIGHT:
      result[i].x = anchor.x1 - box.x1;
      break;
    case ALIGN_TOP:
      result[i].y = anchor.y1 - box.y1;
      break;
    case ALIGN_BOTTOM:
      result[i].y = anchor.y0 - box.y0;
      break;
    case ALIGN_CENTER_H:
      result[i].y = (anchor.y0 + anchor.y1 - box.y0 - box.y1) * 0.5;
      break;
    case ALIGN_CENTER_V:
      result[i].x = (anchor.x0 + anchor.x1 - box.x0 - box.x1) * 0.5;
      break;
    default:
      break;
    }
  }
  return result;
}

}  // namespace VectorAlignment

#endif
