# Flow Displace regression tests

The mathematical cases exercise the production displacement code, including
HDR and negative fields, whole-frame versus tiled output, noise scaling,
unconnected inputs, malformed dimensions, and cancellation. The native cases
compile the actual FX and render it through `TRenderer` with precomputation at
32, 64, and 128 bits. The floating-point field contains the largest finite
`float`, exercising the sampling bound before any integer conversion.

```sh
cmake -S toonz/sources/stdfx/flowdisplace_tests -B build/flowdisplace-tests \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/flowdisplace-tests
ctest --test-dir build/flowdisplace-tests --output-on-failure
```

The libraries must match the checkout's ABI, architecture, Qt installation,
and build type (RelWithDebInfo by default). Sanitizers instrument the sources
compiled here; linked libraries retain their original instrumentation.
These tests do not establish manual desktop behavior or old-scene render
compatibility.
