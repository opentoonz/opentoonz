# Native render-grid regression test

Build `tnzbase` and `tnzcore` from the same checkout before configuring this
standalone test. The PR adds a virtual method; libraries from an older build
must not be mixed with its headers.
Use the same `CMAKE_BUILD_TYPE` as the library build (the default is
`Release`); `NDEBUG` changes the layout of `TSmartObject`.

```sh
cmake --build build --target tnzbase
cmake -S toonz/sources/tnzbase/tests -B build/rendergrid-tests \
  -DOPENTOONZ_BUILD_DIR="$PWD/build" -DCMAKE_PREFIX_PATH=/path/to/qt5
cmake --build build/rendergrid-tests
ctest --test-dir build/rendergrid-tests --output-on-failure
```

The test uses the real `TRenderer`, including precomputation, with a test-only
FX. It checks compute and dry-compute routing for enabled and disabled custom
grids, scales 0.5/1.5/2, phases 0/0.5, requested scales 1/2/0.75, and translations
0/0.25/3.75/-0.25/3/-3. The half-pixel phase represents the grid used by odd-sized
source images. A successful render must also return a nonempty center pixel.

These 216 cases verify the core routing contract, including the identity fast
path for effects that do not opt in. They do not load raster/TLV levels or test
an actual cel effect, source DPI, camera resolution, or interactive viewer
parity. Those remain separate integration checks.

Rodney Baker's review of PR #7242 motivated this matrix. His attachment also
documents a separate change in existing TLV output caused by global source
cropping; the routing test does not establish compatibility for that change.
