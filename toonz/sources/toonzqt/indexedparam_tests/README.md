# Indexed parameter regression tests

These tests load actual `indexedColors` and `indexedGroups` layouts into
`ParamsPage`, drive the numeric count widgets, select entries, and use the
real key toggles and undo manager. The current parameter/page sources and
pixel conversion are compiled into the test; matching OpenToonz libraries
provide the remaining dependencies.

```sh
cmake -S toonz/sources/toonzqt/indexedparam_tests -B build/indexed-parameters \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-DGL_SILENCE_DEPRECATION -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/indexed-parameters
ctest --test-dir build/indexed-parameters --output-on-failure
```

Ordinary refresh preserves pending counts between keys. Keying and undo/redo
synchronize the changed count without discarding another list's pending
edit. Other cases cover default count undo, frame changes, rebinding,
observer destruction, read-only list navigation, and clamped HDR navigation
swatches that leave precise colors unchanged.

The original implementation fails the pending-count refresh check. Restoring
its byte conversion in the navigation swatch triggers float-cast-overflow
with a finite `1e100` channel. One existing ParamViewer nested-layout warning
is ignored with an exact match; other Qt warnings fail the tests.

These offscreen tests do not establish manual desktop, theme/scaling,
macro-pin, or saved-scene rendering compatibility.
