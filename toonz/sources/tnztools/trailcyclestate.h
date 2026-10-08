#pragma once

#include <string>
#include <tuple>

namespace TrailCycle {

enum class Mode { Off, Forward, Backward, Repeat };

struct StyleKey {
  // The tool retains the palette while its cursor or a gesture refers to it.
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
};

class State {
public:
  Selection begin(Mode mode, const StyleKey &style, int frameCount) const {
    Selection selection;
    if (mode == Mode::Off || frameCount < 2) return selection;
    selection.style      = style;
    selection.frameCount = frameCount;
    selection.active     = true;
    switch (mode) {
    case Mode::Off:
      break;
    case Mode::Forward:
      selection.step = 1;
      break;
    case Mode::Backward:
      selection.step = -1;
      break;
    case Mode::Repeat:
      selection.step = 0;
      break;
    }
    if (m_last.active && m_last.style == style &&
        m_last.frameCount == frameCount) {
      selection.offset = (m_last.offset + selection.step) % frameCount;
      if (selection.offset < 0) selection.offset += frameCount;
    } else {
      selection.offset = selection.step < 0 ? frameCount - 1 : 0;
    }
    return selection;
  }

  void commit(const Selection &selection, bool strokeCommitted) {
    if (selection.active && strokeCommitted) m_last = selection;
  }

private:
  Selection m_last;
};

}  // namespace TrailCycle
