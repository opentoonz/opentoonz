#pragma once

namespace TrailCycle {

// Persisted numeric values. This is a style's drawing default, not a cursor.
enum class Mode { Off = 0, Forward = 1, Backward = 2, Repeat = 3 };

inline Mode modeFromValue(int value) {
  return value >= int(Mode::Off) && value <= int(Mode::Repeat)
             ? static_cast<Mode>(value)
             : Mode::Off;
}

}  // namespace TrailCycle
