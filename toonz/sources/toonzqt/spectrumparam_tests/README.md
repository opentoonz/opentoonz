# Precise spectrum regression tests

These standalone tests compile the checkout's actual spectrum and parameter
widgets, parameter classes, pixel conversion, and Qt metadata. Integration tests
also compile `TFxUtil::setKeyframe()`, FX Settings, and the actual
`TFxCommand::resetFxFrame()` implementation. Matching OpenToonz shared libraries
supply the remaining model, undo manager, and renderer.

Build OpenToonz libraries from the same checkout first, then configure:

```sh
cmake -S toonz/sources/toonzqt/spectrumparam_tests -B build/spectrum-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/spectrum-tests
ctest --test-dir build/spectrum-tests --output-on-failure
```

Use the same architecture, Qt installation, and build type as those libraries.
The library directory must include runtime dependencies. CTest selects Qt's
offscreen backend and, on macOS, the library search path required by OpenToonz's
install names. Sanitizers instrument sources compiled by these targets; other
dependencies retain their original instrumentation.

The twelve cases cover:

- RGBA byte and precise numeric edits at defaults, keys, between keys, and with
  asymmetric channel curves. Untouched stops and complete child curves retain
  their values, interpolation, handles, steps, and defaults. No-op and invalid
  entries create no history. HDR, negative RGB, and finite `1e100` remain exact;
  only the byte display is clamped.
- Mouse selection without movement, one-pixel dragging, release outside the
  bar, exact-position undo, and double interpolation when inserting an HDR
  midpoint. Insertion and removal also run with saved and preview parameters
  referring to the same object.
- Pending focused text across frame, stop, and FX rebinding; ordinary refresh
  preserves pending between-key edits and their displayed ramp.
- Pending colors and positions survive stop insertion/removal and subsequent
  refreshes without being committed to surviving saved stops. Coverage includes
  shifted indices, removal of the only animated stop, preview notifications,
  and committing the retained values with the key toggle and its undo/redo.
- Undo/redo after replacement of the preview object, including key deletion.
  External edits, stop insertion/removal, whole-ramp copying, matte replacement,
  rebinding, and control destruction exercise observer lifetime.
- Shift+Reset followed by precise numeric editing, stop insertion/removal, or
  key deletion, then repeated undo/redo of both commands. Tests check original
  stop/channel identity, values, defaults, and interpolation with both separate
  and aliased previews.
- Ordinary spectrum controls retain byte editing without precise numeric fields.
- The actual FX Settings preview-seeding path preserves double positions and
  colors, including extreme finite RGB, while ordinary FX retain byte seeding.
  Mixed ordinary/precise macros and nested macros exercise member-specific
  precision without changing ordinary member quantization.
- A test-only FX and layout exercise the real FX Settings panel and rendered
  swatch. Captured pixels are checked after numeric editing, frame changes,
  undo/redo with a replaced preview, and key deletion/history. PNG evidence is
  saved in the build directory. The layout is copied to a temporary profile.

The original spectrum-only commit `ae78c7c`, replayed onto merged prerequisites,
does not compile because it calls a missing `setDefaultPixelD()` helper. For
baseline comparisons only, a four-channel default setter was supplied so the
original behavior could execute. The original eleven cases fail that baseline.
The additional Shift+Reset history test and pending-preview assertions fail
against `a4693076` with its production sources unchanged. All twelve cases pass
with the fixes on macOS arm64, Apple Clang 21, and Qt 5.15.19 using the sanitizer
flags above. Separate test processes keep sanitizer aborts from hiding other
failures.

These tests do not establish full saved-scene rendering compatibility, manual
desktop behavior, theme/scaling coverage, or Windows/Linux runtime behavior.
Active expression and file-based channel interpolation are not exercised.
The swatch uses an unconnected input to select OpenToonz's direct FX preview
path. Three exact pre-existing layout/offscreen warnings are ignored with an
explanatory comment; other Qt warnings fail the tests.
