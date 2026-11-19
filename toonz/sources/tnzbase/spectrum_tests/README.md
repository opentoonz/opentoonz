# Precise spectrum cache aliases

This standalone test compiles the actual `TRasterFx` implementation and links
matching OpenToonz libraries. It checks that opt-in spectrum parameters retain
distinct cache aliases for adjacent double channel values and stop positions,
HDR and finite extremes, and animated colors. It also checks locale stability,
ordinary FX aliases, other parameter types, and an ordinary connected input.
Macro and nested macro cases exercise the member's own precision setting and
compare ordinary macro aliases to frozen upstream results.

Build the OpenToonz libraries from the same revision first. On macOS, a flat
directory containing those shared libraries can be used as follows:

```sh
cmake -S toonz/sources/tnzbase/spectrum_tests -B build/spectrum-tests \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching/libraries \
  -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined,float-cast-overflow
cmake --build build/spectrum-tests
ctest --test-dir build/spectrum-tests --output-on-failure
```

The fixture, `trasterfx.cpp`, `tmacrofx.cpp`, spectrum/pixel parameter
implementations, and pixel conversion implementation are instrumented by these
flags. Other linked code requires its own sanitizer build for instrumentation.
The tests call the cache
alias API and do not establish full saved-scene rendering compatibility.

For the regression comparison, configure a separate build with
`OPENTOONZ_RASTERFX_SOURCE` and `OPENTOONZ_MACROFX_SOURCE` pointing to the
`trasterfx.cpp` and `tmacrofx.cpp` from upstream
`ecdc40f12f2aa0ab3d0a7ebd041621c66159f247`. Nine of thirteen cases fail there; the
ordinary-alias cases pass. All thirteen pass with the correction
on macOS arm64, Apple Clang 21, and Qt 5.15.19. The huge-value case demonstrates
the original out-of-range integer conversion under UndefinedBehaviorSanitizer.
