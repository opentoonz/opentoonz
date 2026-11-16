# Cel Edge Lines regression tests

The cases exercise production edge rendering: no-rule behavior, matching both
region orders, first-rule precedence, fractional weights, transparent outlines,
whole-frame versus tiled output, malformed dimensions, and cancellation.
Native cases compile the actual FX and run the queued renderer with
precomputation at 32, 64, and 128 bits.

```sh
cmake -S toonz/sources/stdfx/celedgelines_tests -B build/celedgelines-tests \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/celedgelines-tests
ctest --test-dir build/celedgelines-tests --output-on-failure
```

Match the libraries' source ABI, architecture, Qt installation, and build type
(RelWithDebInfo by default). Linked libraries retain their original sanitizer
instrumentation. Manual desktop use, platform coverage beyond the local test
system, and old-scene rendering compatibility remain unverified.
