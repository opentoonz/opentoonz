# Cel Directional Blur regression tests

This standalone target compiles the actual FX, directional filter, and shared
motion/noise helpers. A deterministic raster source supplies input pixels to
the FX's compute callback.

```sh
cmake -S toonz/sources/stdfx/celdirectional_tests -B build/celdirectional-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celdirectional-tests
ctest --test-dir build/celdirectional-tests --output-on-failure
```

Use matching OpenToonz libraries and Qt. Coverage includes 8/16/float pixels,
fractional streak lengths, a non-axis-aligned angle, procedural and layer noise,
split-tile equality, zero-strength bypass, HDR gain and alpha, cancellation, and
missing sources. Disabling both production streak lengths makes the tile test
fail.

These tests do not cover the queued renderer, desktop UI, transformed native
grids, infinite sources, or saved-scene compatibility.
