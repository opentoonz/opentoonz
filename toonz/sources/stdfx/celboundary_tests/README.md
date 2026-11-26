# Cel Boundary Blur regression tests

This standalone target compiles the actual FX and filtering implementation.
A deterministic raster source supplies input pixels. The tests exercise the FX's
compute callback, including source requests, output conversion, and split tiles.

```sh
cmake -S toonz/sources/stdfx/celboundary_tests -B build/celboundary-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celboundary-tests
ctest --test-dir build/celboundary-tests --output-on-failure
```

Use matching OpenToonz libraries and Qt. Coverage includes both kernels,
fractional radii, zero and repeated passes, 8/16/float pixels, split-tile equality,
unchanged alpha, cancellation, missing sources, and finite HDR output. The HDR
case fails the original implementation because legacy mixing can exceed the
float range even when its source channels are finite.

These tests do not cover the queued renderer, desktop UI, transformed native
grids, or saved-scene compatibility.
