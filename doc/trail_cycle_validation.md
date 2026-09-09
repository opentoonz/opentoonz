# Trail Cycle behavior and validation

Raster Trail sizing uses the artwork bounds, including when opening an existing
project. Transparent source padding no longer determines stamp size or spacing.
This is an intentional appearance change; there is no legacy-sizing switch.
Review padded Trail assets before adopting this version for an existing production.

## Cycle behavior

- Off uses the normal per-stroke source sequence and does not move the brush cursor.
- Forward starts at the first source frame and advances between committed gestures.
- Backward starts at the last source frame and moves backward.
- Repeat holds the shared cursor and writes a zero frame step, holding that source
  frame throughout the stroke.
- Changing between Forward, Backward and Repeat keeps the shared cursor. Drawing
  with an ordinary style, a single-frame Trail, or Frame Range leaves it unchanged.
- A committed gesture using a different Trail resource, palette, style slot or
  frame count starts a new cursor. Cancellation does not replace the old cursor.
- Symmetry replicas share one selection and advance the cursor once per gesture.
- Trail Cycle is available only for multi-frame Trail styles with Frame Range off.

## Raster preparation and caching

The loader measures nonzero alpha in the original pixels before reducing them.
Every frame uses the union of the animation's artwork bounds, preserving relative
positions and sizes, including empty frames. A transparent pixel surrounds the
prepared artwork. Sources are limited to 2048 pixels per side and a conservative
128 MiB total pixel budget per loaded style. Loading makes two passes and retains
one original decoded frame at a time; the budget does not bound decoder workspace,
the original frame, all style instances combined, or temporary rendering buffers.

Prepared texture caching uses retained source identity, output dimensions, concrete
color-function type and parameter values. Source replacement cannot reuse an old
entry through a recycled address. Unsupported color signatures bypass caching.
Texture preparation runs outside the cache mutex; insertion checks again under the
mutex. Least-recently-used entries are evicted at 32 entries or 64 MiB, counting
retained source and prepared pixel storage conservatively.

Texture dimensions remain powers of two for compatibility OpenGL contexts.
Texture uploads still happen during drawing. Persistent GPU caching and mipmaps
are not implemented by this change. Stamp bounds cover the artwork without the
historical expansion proportional to the length of the entire stroke.

## PLI compatibility

Nondefault cycle offset/step values extend the existing outline-options tag.
Readers accept the original payload; incomplete optional extensions use offset 0
and step 1, and additional trailing bytes are ignored. Truncated original outline
fields are rejected. PLI's signed-magnitude integers cannot represent `INT_MIN`;
the writer rejects it and non-finite or out-of-range miter values explicitly.

The older reader's tag-length handling can skip the extension. That is source-level
compatibility evidence, not a completed released-application round trip. An older
application does not implement the new cycle fields; saving through it loses them.
Do not describe the whole PR as making no persistence change.

`TStroke::OutlineOptions` has additional members. Rebuild dependent modules/plugins
against the matching headers and libraries; do not mix the old binary layout with
the new one.

## Running regression tests

The standalone tests require a C++17 compiler and CMake. The two dependency-light
suites can run without building OpenToonz:

```sh
cmake -S toonz/tests -B /tmp/trail-tests
cmake --build /tmp/trail-tests
ctest --test-dir /tmp/trail-tests --output-on-failure
```

For all suites, build `tnzcore` from the same checkout, supply `TNZCORE_LIBRARY`,
and make Qt 5 and PNG discoverable through the normal CMake search path:

```sh
cmake -S toonz/tests -B /tmp/trail-tests \
  -DTNZCORE_LIBRARY=/absolute/path/to/build/tnzcore/libtnzcore.dylib \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/qt5
cmake --build /tmp/trail-tests
ctest --test-dir /tmp/trail-tests --output-on-failure
```

Use the `.so` library on Linux. The `opengl`-labelled test requires a working display
and OpenGL context. `ctest -LE opengl` explicitly excludes that test for headless
runners. The Linux and macOS build workflows run the four headless suites; they do
not claim OpenGL coverage.

## Local evidence and remaining checks

Validated on macOS arm64 with Qt 5.15.19 and the bundled TIFF/SuperLU dependencies
rebuilt for arm64:

- Full Debug OpenToonz application build.
- Opacity/filter pixel behavior and color-signature separation.
- Original low-alpha one-pixel artwork on a 16K-wide canvas, shared animation
  coordinates, empty frames, source budgets, LRU and byte-budget eviction,
  unsupported-color cache bypass, invalid cache inputs and concurrent requests.
  Raising the texture-cache budget from 64 to 128 MiB makes the byte-limit test
  fail; restoring 64 MiB makes it pass.
- Forward/Backward/Repeat transitions, cancellation, replacement, inactive styles
  and shared replica selection.
- Actual PLI reader/writer combinations, partial/trailing tags, malformed base
  payloads, format limits, and stroke copy/split/transform metadata retention.
- Actual offscreen OpenGL output for PNG, vector and rasterized-vector sources,
  Forward/Backward/Repeat sequences, wrapped offsets, tiled/full pixel equality,
  opacity changes, artwork sizing and visibility at thumbnail scale.
- The five suites with UndefinedBehaviorSanitizer on test code, helper code and
  directly compiled PLI/color sources. The linked core library is not instrumented.

AddressSanitizer could not run: the installed Apple runtime deadlocks during its
initialization before `main`, including in the dependency-light state test.
The full application build emits existing legacy/OpenGL deprecation warnings;
the regression-test translation units compile with warnings treated as errors.

Interactive toolbar enablement, real viewer/preview/column-icon parity, undo/redo
across scene switches, and a released older application's open/save cycle still
need a manual pass. Desktop automation was unavailable because its client and
server versions did not match. The offscreen thumbnail-scale test does not stand
in for a real column-icon UI check. Windows/Linux rendering and the newly added CI
steps have not been run locally.
