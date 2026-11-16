# Cel helper regression tests

These standalone C++ tests compile the actual distance, motion, and noise
helpers. They need no Qt or OpenToonz library build.

```sh
cmake -S toonz/sources/stdfx/cel_helpers_tests -B build/cel-helpers \
  '-DCMAKE_CXX_FLAGS=-Wall -Wextra -Werror -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/cel-helpers
ctest --test-dir build/cel-helpers --output-on-failure
```

Coverage includes every binary seed arrangement on a 4-by-3 image for all
three distance metrics, with and without exterior seeds, compared with an
independent nearest-seed search. Further cases exercise distance ties,
invalid dimensions, cancellation, region expansion/contraction, feather
coverage, connected components, HDR bilinear samples, and noise scale
invariance. Noise tests include positive subnormal and very large thickness.

The original noise implementation fails the scale-invariance assertion
because it clamps render-scaled thickness to the UI range. These tests do not
verify native-grid discovery, full scene rendering, or desktop UI behavior.
