# Print Process regression tests

These tests exercise the production print code across all dot shapes, disabled
plates, fractional misregistration, paper and transparent output, tiled output,
malformed dimensions, and cancellation. Native cases compile the actual FX and
run the queued renderer with precomputation at 32, 64, and 128 bits.

```sh
cmake -S toonz/sources/stdfx/printprocess_tests -B build/printprocess-tests \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/printprocess-tests
ctest --test-dir build/printprocess-tests --output-on-failure
```

Match the libraries' source ABI, architecture, Qt installation, and build type
(RelWithDebInfo by default). Linked libraries retain their original sanitizer
instrumentation. Manual desktop use, platform coverage beyond the local test
system, and old-scene rendering compatibility remain unverified.
