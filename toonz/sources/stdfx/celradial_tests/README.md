# Cel Radial Blur raster tests

This target compiles the production FX, motion analysis, and noise functions,
then renders a synthetic source through the native OpenToonz raster path.
Both Zoom and Rotation are checked at 8-bit, 16-bit and floating-point depth:
active blur changes the source; four tiles match the whole frame exactly;
zero blur preserves the source. The fixture enables both fades, ellipse ratio
1.75, and procedural noise. Raster allocation boundary checks also run.

```sh
cmake -S toonz/sources/stdfx/celradial_tests -B build/celradial-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celradial-tests
ctest --test-dir build/celradial-tests --output-on-failure
```

Match the libraries' architecture, Qt version, build type and source base.
CTest sets the macOS library search path. Replacing global sampling positions
with tile-local positions makes the tile comparison fail.

The tests invoke the FX inside an installed render context. They do not run
the queued renderer, camera/infinite-source path, UI, or saved-scene renders.
