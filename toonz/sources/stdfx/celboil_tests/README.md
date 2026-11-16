# Cel Boil rendering tests

These tests run the production Cel Boil kernel and noise implementation. They
compare a whole frame with four separately rendered tiles for both image and
line modes, verify held frames and changing cycles, and check cancellation.

```sh
cmake -S toonz/sources/stdfx/celboil_tests -B build/celboil-tests \
  '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -Wall -Wextra -Werror'
cmake --build build/celboil-tests
ctest --test-dir build/celboil-tests --output-on-failure
```

These are pixel-kernel tests. They do not exercise the queued renderer, raster
conversion, FX Settings widgets, or saved-scene compatibility.
