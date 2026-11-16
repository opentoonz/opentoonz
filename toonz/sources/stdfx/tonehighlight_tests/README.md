# Tone / Highlight regression tests

These tests compile the actual `tonehighlightfx.cpp` and call
`TRasterFx::compute()` with synthetic Source and Matte images. They link the
OpenToonz libraries from an existing build. `stdfx.cpp` and `gradients.cpp` are
compiled directly for the existing affine helper without registering unrelated
effects or requiring an OpenGL context.

Build the `tnzcore`, `tnzbase`, `tnzext`, and `toonzlib` targets first. Use matching
source headers, architecture, Qt, and build type: OpenToonz's public smart-object
layout changes between debug and release, and FX virtual interfaces have changed
between branches.

```sh
cmake -S toonz/sources/stdfx/tonehighlight_tests -B build/tonehighlight-tests \
  -DTOONZ_BUILD_DIR=/absolute/path/to/opentoonz-build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/qt5
cmake --build build/tonehighlight-tests
ctest --test-dir build/tonehighlight-tests --output-on-failure
```

Undefined-behavior and float-to-integer overflow sanitizers are enabled by
default for the compiled FX and test. The linked libraries are not automatically
rebuilt with sanitizers. The tests require no desktop window or video recording.

The fourteen cases cover:

- Extreme, nonfinite, and scaled Softness; the reported radius of 8,192;
  and a minimum halo that exceeds the render tile budget. Memory, dry-compute,
  and render paths must reject the request before computing a padded Matte.
- Memory for the live Matte images, blur plane, and oversized/empty requests,
  at 32, 64, and 128 bpp.
- Pixel-exact HDR RGB and fractional alpha with zero intensity or no Matte.
  Inactive shading must also bypass Softness, Matte precomputation, and the
  blur's affine restriction.
- Active float Tone and Highlight, including negative RGB and RGB above one.
- A spatially varying Matte at tiny Softness, at the float-epsilon boundary,
  and immediately below, at, and above one pixel. The center-only triangular
  kernel must not lose Matte alpha through cancellation or reciprocal overflow.
- Thirty-two integer combinations of bit depth, mode, line protection, inverted
  Matte, and Softness zero or 2.125. Canonical BGRM hashes were captured from
  original PR #7247 at `c571a682b48bf3a339a0772508d3009bed6f71e3`.
- Forty-eight more integer combinations at Softness 0.01, 0.75, and 1, checked
  against the mathematically identical unblurred result.
- Missing Source, and checked padded dimensions at the allocator boundary.
  The dimension test does not allocate the large raster it describes.

For a baseline comparison, configure another build with
`TONE_HIGHLIGHT_SOURCE_DIR` pointing to the original PR's `stdfx` directory and
`TONE_HIGHLIGHT_TEST_PADDING=OFF`. The original does not have the checked-size
helper. Ten of thirteen applicable cases fail on that baseline; the two integer
matrices and missing-Source case pass. All fourteen pass with the correction.
The radius test first checks the memory-query rejection, so the unsafe original
never attempts the multi-gigabyte allocation.

Both integer matrices can also export their complete output for comparison on
the same platform:

```sh
build/tonehighlight-baseline/tonehighlightfx_test integer-matrix build/baseline.bin
build/tonehighlight-tests/tonehighlightfx_test integer-matrix build/fixed.bin
cmp build/baseline.bin build/fixed.bin
build/tonehighlight-baseline/tonehighlightfx_test subpixel-matrix build/baseline-subpixel.bin
build/tonehighlight-tests/tonehighlightfx_test subpixel-matrix build/fixed-subpixel.bin
cmp build/baseline-subpixel.bin build/fixed-subpixel.bin
```

Verified on macOS arm64, Apple Clang 21 and Qt 5.15.19, with native OpenToonz
libraries built from `35f17def`. All eighty integer buffers are byte-identical
between the original PR and the correction. The ordinary matrix SHA-256 is
`230227e960f02583a2a1bac56398434a99edfb463b18b3b4e1230e231a3f4389`;
the subpixel matrix SHA-256 is
`af69031879b2df1a79a32f137720a1796b5a041629dd4b0ee0cb0f557c3f2fa1`.

Removing the center-only-kernel bypass from the corrected source makes the
spatial tiny-Softness test fail, independently of the original HDR fallback.
Synthetic fixtures do not establish saved-scene compatibility, full desktop
behavior, or results on other platforms.
