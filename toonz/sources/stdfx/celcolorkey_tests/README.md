# Cel Color Key raster regression tests

The test compiles the actual FX translation unit and runs its raster filter.
It checks remove/keep selection, unchanged source alpha, HDR replacement,
finite values outside the float range, invalid replacements, and cancellation.

```sh
cmake -S toonz/sources/stdfx/celcolorkey_tests -B build/celcolorkey-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celcolorkey-tests
ctest --test-dir build/celcolorkey-tests --output-on-failure
```

Use libraries with the same architecture, Qt version, build type and source
base. CTest sets the macOS library search path. For two optional PNGs, run the
executable with an output directory, using the same library search path.
The fixture contains a red disk, green rectangle and blue triangle. The after
image removes red with RGB comparison, zero threshold, no thinning or blur,
and replacement disabled. Both images come from the production raster filter;
the source is a synthetic test pattern, not an artist's scene or a UI capture.

The original implementation fails the finite extreme replacement check. These
tests do not establish queued-renderer behavior, UI or saved-scene compatibility.
