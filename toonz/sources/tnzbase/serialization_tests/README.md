# FX serialization regression tests

These standalone tests link the real OpenToonz `tnzbase` and `tnzcore` libraries.
Build both libraries from the same source revision and with the same build type
as the tests: `TFx` has an added virtual method, and debug/release class layouts
also differ. Installed libraries from an older OpenToonz are incompatible.

```sh
cmake -S toonz/sources/tnzbase/serialization_tests -B build-serialization-tests \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPENTOONZ_LIBRARY_DIR=/absolute/path/to/matching/build \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/qt5
cmake --build build-serialization-tests
ctest --test-dir build-serialization-tests --output-on-failure
```

`OPENTOONZ_LIBRARY_DIR` accepts a normal OpenToonz build directory or a directory
containing both libraries. Use a fresh test build directory when changing the
library path. The CTest configuration supplies the macOS dynamic library search
path. On other platforms, make the matching shared libraries available through
the platform's normal library search mechanism.

`fxserialization_test` exercises `TFx::saveData`/`loadData` through registered
test FX objects with real double parameters and input ports. Its 26 cases cover:

- Ordinary and opted-in nodes alone, connected in both directions, nested as
  opted-in/ordinary/opted-in, and connected as siblings.
- Default values, keyframes, graph-position metadata, and a following sentinel.
- Both the default legacy stream precision and explicitly enabled full precision.
- Ordinary and opted-in presets through `TFx::savePreset`/`loadPreset`.
- Serialization through `paramsLinkedTo` with an ordinary parameter owner.
- Restoration of stream precision after normal writes and injected parameter
  write exceptions, for both scene serialization and presets.

The fixture classes select the precision contract and supply data. They do not
implement the serialization or precision-scoping algorithm. Fault injection is
limited to a parameter writer throwing during the real FX serialization path.

Pass a directory to `fxserialization_test` to retain its output files for byte
comparison. An ordinary node, an all-ordinary graph, and an ordinary preset
should keep identical bytes before and after the precision-scope repair. The
unrepaired PR revision should fail connected ordinary-node, linked-parameter,
and graph-metadata assertions.

`streamroundtrip_test` uses `TOStream` and `TIStream` for 24 cases covering exact
finite-double round trips (including signed zero, subnormals, and normal bounds),
continued parsing after tiny values, unchanged legacy bytes, malformed numbers,
overflow, underflow to zero, EOF, prior failures, and invalid locale grouping.
The original PR reader fails the subnormal cases on the tested macOS standard
library; these failures can depend on the platform's standard library.

These tests cover the scene FX serialization path, not saving an entire
`ToonzScene`, desktop preset workflows, rendered output, or UI undo/redo.
