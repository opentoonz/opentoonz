# Parameter field regression tests

This focused executable compiles the checkout's complete `doublefield.cpp`,
`paramfield.cpp`, and `tdoubleparam.cpp` translation units and Qt metadata. It
links the remaining dependencies from an existing OpenToonz build. It does not
replace the parameter model, undo manager, or widgets with test doubles.

Build OpenToonz's `toonzqt` target first, then run from the repository root:

```sh
cmake -S toonz/sources/toonzqt/tests -B build/paramfield-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_BUILD_DIR=/absolute/path/to/opentoonz-build
cmake --build build/paramfield-tests
ctest --test-dir build/paramfield-tests --output-on-failure
```

Use the same build type, source base, and Qt installation as the dependency
build so platform and debug definitions remain consistent. The test target
defaults to Release when no single-configuration build type is specified. The
changed parameter/widget sources are recompiled so that the same dependency
build can compare the original PR against its fix. On macOS, CTest sets the
library search paths required by OpenToonz's `@executable_path` install names.

Coverage:

- Typed upper and lower slider endpoints commit while the existing value lies
  outside the optional slider window.
- Checkbox and combo choice controls synchronize the complete preview curve,
  displayed choice, and key indicator after key deletion, undo, and redo.
- History remains correct after replacing the preview parameter.
- Ordinary refresh retains an uncommitted choice between keys; keying it then
  commits that choice without prematurely adding an undo item.
- An external committed curve edit updates the preview and signals consumers.
- Rebinding the actual parameter detaches the old observer and registers the new
  one; destroying the field detaches it before later parameter edits.

The `fxsettings_swatch` integration test opens the real FX Settings panel with a
test-only color FX and layout. It checks the actual painted swatch pixels after
the keyed state, deletion, undo, and redo. The renderer runs asynchronously
through `SwatchViewer`; no replacement renderer or simulated refresh is used.
PNG snapshots are saved in the test build directory.

The integration fixture uses an unconnected input so FX Settings follows its
existing direct FX preview path. It does not test the complete scene graph,
camera preview, manual desktop interactions, or UI themes. Known pre-existing
nested-layout warnings and the offscreen platform's unsupported size-hint
warning are narrowly ignored in the fixture; any other warning fails the test.

The eight `doublefield_bounds` scenarios cover measured parameter construction
with numeric bounds of positive and negative ten million, preserving the
initial value, precision changes, and rebinding. They drive actual slider
positions and releases into both the actual and preview parameters, checking
every position in small fractional windows against the exact hard and optional
bounds. Additional cases cover unrepresentable and sub-quantum windows,
representable decimal endpoints, numeric entry before and after slider setup,
ordinary/nonlinear mapping compatibility, ordinary range-change clamping and
notifications, and XML-style precision setup before parameter binding. Fine
optional windows recover when precision becomes sufficient, including after
temporarily selecting a coarser precision. Numeric and roller editing remain
available when a window has no representable slider positions.

To check conversions in the production sources, configure a separate build
with AddressSanitizer, UndefinedBehaviorSanitizer, and float-cast overflow
instrumentation explicitly enabled:

```sh
cmake -S toonz/sources/toonzqt/tests -B build/paramfield-sanitized \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_BUILD_DIR=/absolute/path/to/opentoonz-build \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/paramfield-sanitized
ctest --test-dir build/paramfield-sanitized --output-on-failure
```

Each bounds scenario runs separately with UBSan configured to stop on the first
error, so both signed constructor failures can be reproduced independently.
The linked OpenToonz and Qt dependency libraries retain their original build
instrumentation; these flags instrument the production sources compiled into
the test executables.
