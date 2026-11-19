# Cel Smoother v2 regression tests

This standalone target compiles the actual FX and contour filter. A deterministic
raster source supplies input pixels to its compute callback.

```sh
cmake -S toonz/sources/stdfx/celsmoother_tests -B build/celsmoother-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celsmoother-tests
ctest --test-dir build/celsmoother-tests --output-on-failure
```

Use matching OpenToonz libraries and Qt. Tests cover 8/16/float pixels, both
smoothing versions, staircase color mixing, split-tile equality, zero-strength
bypass, partial-alpha color keying, inverted keying, HDR preservation, gamma
selection, cancellation, and missing sources. Disabling production smoothing
makes the tile test fail.

These tests do not cover the queued renderer, desktop UI, transformed native
grids, infinite sources, or saved-scene compatibility.
