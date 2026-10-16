#include "tnztools/trailcyclestate.h"

#include <stdexcept>

namespace {

using TrailCycle::Mode;
using TrailCycle::State;
using TrailCycle::StyleKey;

const StyleKey first{nullptr, 1, "raster:first"};
const StyleKey replacement{nullptr, 1, "raster:replacement"};

void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}

int stamp(State &state, Mode mode, const StyleKey &style = first,
          int count = 3) {
  const auto selection = state.begin(mode, style, count);
  state.commit(selection, true);
  return selection.offset;
}

void checkModes() {
  State forward, backward, repeat;
  for (int expected : {0, 1, 2, 0})
    require(stamp(forward, Mode::Forward) == expected,
            "Forward sequence failed");
  for (int expected : {2, 1, 0, 2})
    require(stamp(backward, Mode::Backward) == expected,
            "Backward sequence failed");
  for (int i = 0; i < 4; ++i)
    require(stamp(repeat, Mode::Repeat) == 0, "Initial Repeat sequence failed");
  State shared;
  stamp(shared, Mode::Forward);
  stamp(shared, Mode::Forward);
  stamp(shared, Mode::Forward);
  require(stamp(shared, Mode::Backward) == 1, "Backward did not backtrack");
  require(stamp(shared, Mode::Repeat) == 1, "Repeat did not hold");
  require(stamp(shared, Mode::Repeat) == 1, "Repeat moved the cursor");
  require(stamp(shared, Mode::Forward) == 2, "Forward did not resume");
}

void checkCancellationAndReplacement() {
  State state;
  stamp(state, Mode::Forward);
  const auto canceled = state.begin(Mode::Forward, first, 3);
  state.commit(canceled, false);
  require(stamp(state, Mode::Forward) == 1, "Canceled gesture advanced");
  state.commit(state.begin(Mode::Forward, replacement, 3), false);
  require(stamp(state, Mode::Repeat) == 1,
          "Canceled replacement erased cursor");
  require(stamp(state, Mode::Forward, replacement) == 0,
          "Same-slot replacement did not reset");
  require(stamp(state, Mode::Backward, first) == 2,
          "Returning from another Trail did not reset");
  require(stamp(state, Mode::Forward, first, 2) == 0,
          "Frame-count change did not reset");
}

void checkInactiveAndReplicas() {
  State state;
  stamp(state, Mode::Forward);
  state.commit(state.begin(Mode::Off, replacement, 3), true);
  state.commit(state.begin(Mode::Forward, replacement, 1), true);
  state.commit(state.begin(Mode::Forward, replacement, 0), true);
  require(stamp(state, Mode::Repeat) == 0, "Inactive style erased cursor");
  const auto gesture = state.begin(Mode::Forward, first, 3);
  for (int replica = 0; replica < 4; ++replica) {
    require(gesture.offset == 1 && gesture.step == 1,
            "Replicas are out of phase");
    state.commit(gesture, true);
  }
  require(stamp(state, Mode::Forward) == 2, "Replicas advanced more than once");
}

}  // namespace

int main() {
  checkModes();
  checkCancellationAndReplacement();
  checkInactiveAndReplicas();
}
