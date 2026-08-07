#pragma once

#ifndef THIDELINESEGMENT_H
#define THIDELINESEGMENT_H

#include "tcommon.h"
#include "tgeometry.h"

#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStroke;
class VIStroke;

enum class THideLineMode : UCHAR { Invisible = 0, Hidden = 1 };

struct DVAPI THideLineSegment {
  double m_w0;
  double m_w1;
  THideLineMode m_mode;

  THideLineSegment() : m_w0(0), m_w1(0), m_mode(THideLineMode::Invisible) {}
  THideLineSegment(double w0, double w1, THideLineMode mode)
      : m_w0(w0), m_w1(w1), m_mode(mode) {}
};

DVAPI void mergeHideLineSegments(std::vector<THideLineSegment> &segments);

DVAPI void addHideLineSegments(VIStroke *vs,
                               const std::vector<DoublePair> &ranges,
                               THideLineMode mode);

DVAPI void removeHideLineSegments(VIStroke *vs,
                                  const std::vector<DoublePair> &ranges);

DVAPI std::vector<DoublePair> getVisibleStrokeRanges(
    const std::vector<THideLineSegment> &hideSegments);

// Merged parameter ranges for Hidden-mode segments only (fill passes through).
DVAPI std::vector<DoublePair> getHiddenModeRanges(
    const std::vector<THideLineSegment> &hideSegments);

// Parameter ranges that still block fill (complement of Hidden-mode ranges).
DVAPI std::vector<DoublePair> getFillBarrierRanges(
    const std::vector<THideLineSegment> &hideSegments);

DVAPI bool strokeParticipatesInFill(
    const std::vector<THideLineSegment> &hideSegments);

DVAPI bool isIntervalFullyHiddenForFill(
    double w0, double w1,
    const std::vector<THideLineSegment> &hideSegments);

DVAPI std::vector<THideLineSegment> interpolateHideLineSegments(
    const std::vector<THideLineSegment> &seg1,
    const std::vector<THideLineSegment> &seg2, double t);

DVAPI TStroke extractStrokePortion(const TStroke &src, double w0, double w1);

#endif
