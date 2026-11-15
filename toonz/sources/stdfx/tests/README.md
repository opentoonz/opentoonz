# Light Shading regression tests

These tests compile the actual `lightshadingfx.cpp` into a standalone executable
and call `TRasterFx::compute()` with synthetic source and matte images. They link
the OpenToonz core/base libraries from an existing build. `stdfx.cpp` and
`gradients.cpp` are compiled directly to provide the existing affine helper
without linking a second copy of Light Shading or registering unrelated GPU FX.

Build OpenToonz's `tnzcore`, `tnzbase`, `tnzext`, and `toonzlib` targets first.
The test configuration must match that build's Qt, architecture, and build type:
OpenToonz changes the public smart-object layout between debug and release.

```sh
cmake -S toonz/sources/stdfx/tests -B build/lightshading-tests \
  -DTOONZ_BUILD_DIR=/absolute/path/to/opentoonz-build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/qt5
cmake --build build/lightshading-tests
ctest --test-dir build/lightshading-tests --output-on-failure
```

The default tests enable undefined-behavior and float-to-integer overflow
sanitizers for the compiled FX and test translation units. The linked OpenToonz
libraries are not automatically rebuilt with sanitizers. No desktop window or
OpenGL context is required.

The nine cases cover:

- Extreme and nonfinite Softness values, before integer conversion or allocation.
- Existing ordinary radii and rejection when a minimum tile exceeds the render budget.
- Both input rasters and peak float workspace in 32/64/128-bpp memory estimates,
  empty rectangles, and the halo cost of a one-pixel output.
- Pixel-exact HDR pass-through with negative RGB, RGB above one, and fractional alpha.
- Disabled shading with an extreme Softness value and a connected, unused matte,
  including the unpadded source request during dry compute.
- Active float shading without clipping negative/HDR channels or changing alpha.
- Known 8-bit and 16-bit shading results.
- Sixteen integer rendering combinations: 32/64 bpp, radial/directional,
  source alone/source plus matte, and zero/nonzero Softness. The golden hashes
  were captured from PR #7248 at `a6cc21916adaadaadf196c5307bf4a8c06812538`.
  Hashes use canonical BGRM channels with low bytes first; optional binary
  exports retain native raster bytes for comparisons on the same platform.
- The exact padded-dimension integer boundary and rejection beyond it.

To compare the original PR with the corrected source, configure a second test
build with `LIGHT_SHADING_SOURCE_DIR` pointing to the original PR's `stdfx`
directory and `LIGHT_SHADING_TEST_PADDING=OFF` (the original lacks that helper).
The same source tests then reproduce six original failures; the two existing
integer cases pass. The corrected source passes all nine cases.

For a complete byte comparison, export both integer matrices and compare them:

```sh
build/lightshading-baseline/lightshadingfx_test integer-matrix build/baseline.bin
build/lightshading-tests/lightshadingfx_test integer-matrix build/fixed.bin
cmp build/baseline.bin build/fixed.bin
```

Verified on macOS arm64 with Apple Clang 21 and Qt 5.15.19, using baseline
OpenToonz libraries built from `35f17def`. The sixteen output buffers were
byte-identical (SHA-256
`904f4e6947bb755cff025399da7292a8a12f92ad28d675148860a4a9a672ab5a`).
These synthetic render tests do not establish full application UI behavior,
other-platform results, or compatibility across a collection of saved scenes.
