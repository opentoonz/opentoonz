#pragma once

#include "trailcycle.h"

#include <string>
#include <tuple>

namespace TrailCycle {

struct StyleKey {
  // The tool owns the palette lifetime.
  const void *palette = nullptr;
  int styleId         = -1;
  std::string resource;

  bool operator==(const StyleKey &other) const {
    return std::tie(palette, styleId, resource) ==
           std::tie(other.palette, other.styleId, other.resource);
  }
};

struct Selection {
  StyleKey style;
  int frameCount = 0;
  int offset     = 0;
  int step       = 1;
  bool active    = false;
  // Off and single-frame selections leave the shared cursor unchanged.
  bool updatesCursor = false;
  int startOffset    = -1;
};

class State {
public:
  // startOffset is a source index; -1 preserves the current cycle.
  Selection begin(Mode mode, const StyleKey &style, int frameCount,
                  int startOffset = -1) const {
    Selection selection;
    const bool explicitStart = startOffset >= 0 && startOffset < frameCount;
    if (frameCount < 1 ||
        ((mode == Mode::Off || frameCount < 2) && !explicitStart))
      return selection;
    selection.style         = style;
    selection.frameCount    = frameCount;
    selection.active        = true;
    selection.updatesCursor = mode != Mode::Off && frameCount > 1;
    if (mode == Mode::Backward)
      selection.step = -1;
    else if (mode == Mode::Repeat)
      selection.step = 0;
    const bool sameCycle = m_last.active && m_last.style == style &&
                           m_last.frameCount == frameCount;
    selection.startOffset =
        explicitStart ? startOffset : (sameCycle ? m_last.startOffset : -1);
    if (!sameCycle || mode == Mode::Off ||
        (explicitStart &&
         (mode == Mode::Repeat || startOffset != m_last.startOffset))) {
      selection.offset = explicitStart
                             ? startOffset
                             : (selection.step < 0 ? frameCount - 1 : 0);
    } else {
      selection.offset = (m_last.offset + selection.step) % frameCount;
      if (selection.offset < 0) selection.offset += frameCount;
    }
    return selection;
  }

  void commit(const Selection &selection, bool strokeCommitted) {
    if (selection.active && selection.updatesCursor && strokeCommitted)
      m_last = selection;
  }

private:
  Selection m_last;
};

}  // namespace TrailCycle
