# Pixel parameter regression tests

These tests compile this checkout's actual `paramfield.cpp` and Qt metadata.
The swatch test also compiles `fxsettings.cpp`. Other production classes,
including the color widget, parameter curves, undo manager, and renderer, come
from matching OpenToonz libraries. No replacement parameter model or renderer
is used.

Build matching OpenToonz libraries first. In particular, a build predating
`TFx::requiresFullPrecisionSerialization()` has an incompatible virtual table
and cannot be reused. Point `OPENTOONZ_LIBRARY_DIR` at a directory containing
the matching shared libraries, including their runtime dependencies:

```sh
cmake -S toonz/sources/toonzqt/pixelparam_tests -B build/pixelparam-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=/path/to/qt5 \
  -DOPENTOONZ_LIBRARY_DIR=/path/to/matching-libraries \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer'
cmake --build build/pixelparam-tests
ctest --test-dir build/pixelparam-tests --output-on-failure
```

Use the dependency build's Qt installation, architecture, and build type.
CTest selects the offscreen platform and, on macOS, the library search path
required by `@executable_path` install names. The sanitizer flags instrument
the production sources compiled into these executables; linked dependencies
retain their original instrumentation.

The widget suite finishes edits through the real RGBA channel line edits. It
covers each channel with default, keyed, between-key, and asymmetric channel
curves, including fractional alpha, negative RGB, and RGB above one:

- Re-entering the displayed byte leaves exact curves, undo history, and preview
  notifications unchanged.
- Editing one channel leaves all untouched channel curves exactly unchanged,
  including their keys, interpolation, handles, steps, and defaults.
- Undo and redo restore complete curves; ordinary refresh preserves an
  uncommitted preview between keys.
- Deleting a key while another remains updates the preview and key indicator,
  including undo, redo, and replacement of the preview parameter.
- Preview refresh signals observe already synchronized curves.
- External child-curve edits refresh the preview. Rebinding and destruction
  detach observers, including subsequent history operations. Hidden alpha
  remains exact when RGB changes; replacing the matte curve also rebinds its
  observer.
- A control without the precision opt-in retains ordinary byte color editing.

The integration test uses a test-only precision-aware color FX and layout with
the real FX Settings panel. It checks painted swatch pixels after the initial
keyed state, deletion, undo, and redo, saving PNGs in the test build directory.
An unconnected input selects the existing direct FX preview path. Complete
scene/camera rendering, manual desktop interactions, and other platforms are
outside this fixture's coverage. Active expression/file interpolation is not
exercised. The swatch fixture copies its layout into a temporary profile so
OpenToonz preference writes stay outside the checkout. Three exact, pre-existing
nested-layout and offscreen size-hint warnings are ignored with a source comment; other Qt
warnings fail the swatch test.
