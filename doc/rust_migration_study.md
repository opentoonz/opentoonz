# OpenToonz Rust Migration And Generative Workbench Study

Prepared: May 16, 2026

## Executive Recommendation

Moving the entire OpenToonz project to Rust is technically possible, but a
direct rewrite is the highest-risk path. The current application is not just a
Qt shell around simple drawing code. It is a mature C++/Qt desktop animation
suite with a custom scene model, xsheet, level formats, raster and vector image
types, FX graph, renderer, scanner and stop-motion paths, farm tools, plugin
interfaces, translations, application data, and a large amount of platform
packaging behavior.

The recommended plan is:

1. Build a Rust successor architecture next to the C++ application, not inside a
   big-bang rewrite branch.
2. Keep the existing Qt GUI for the compatibility-preserving phase. Add Rust
   services through C-compatible APIs or CXX-Qt where Qt object integration is
   useful.
3. Build the generative workbench first as Rust port functionality with a
   plugin-style host contract, not as a detached companion app. It should consume
   the same scene, xsheet, level, palette, tile, FX, render, cache, and project
   abstractions that the Rust port is introducing.
4. Use automated comparison testing against the current C++ application as the
   first deliverable of the port. Do not begin large rewrites until the C++
   version can produce machine-readable golden outputs for scene structure,
   file round-trips, rendered frames, UI screenshots, and performance traces.
5. Target Rust for the core engine, IO, render graph, job orchestration,
   generative package model, and GPU rendering work. Defer a full GUI replacement
   until the Rust engine proves parity on real productions.

The best near-term product experiment is therefore not "OpenToonz rewritten in
Rust." It is "OpenToonz-compatible Rust engine where generative production is a
first-class module." That module should match the anim-workbench direction:
local-first project data, shot manifests, ComfyUI/model-route integration,
dailies, retakes, cost/provenance tracking, and an xsheet-aware way to make
generative models respect animation timing rather than inventing it. The key
change is ownership: the Rust port owns the canonical scene/package/render
contracts, and the generative UI is a client of those contracts.

## Baseline From This Checkout

This checkout is a CMake C++17 project rooted at `toonz/sources`. The current
Nix/mise workflow builds the app with:

```sh
mise run doctor
mise run configure
mise run build
```

The Nix path configures `toonz/sources/CMakePresets.json` and builds into
`toonz/build/nix-relwithdebinfo`. It uses Qt 5, CMake, Ninja, pkg-config, native
libraries, and a vendored modified TIFF preparation step.

The main CMake modules are:

| Module | Role in the current system | Port implication |
|---|---|---|
| `tnzcore` | Common platform, raster, geometry, cache, path, sound, GL, Qt-core integration | Must be split into Rust foundational crates before broad replacement |
| `tnzbase` | Base application services, scanner abstractions, platform hooks | Needs platform-specific Rust wrappers and likely FFI for scanner APIs |
| `tnzext` | Extension math, mesh/deformation and numerical paths | Candidate for Rust reimplementation after test corpus exists |
| `toonzlib` | Scene, xsheet, levels, palettes, cleanup, vectorizer, renderer, scripting bindings | Highest-value and highest-risk core port |
| `toonzqt` | Shared Qt widgets, docking, palettes, schematics, style editors, spreadsheet widgets | GUI parity blocker if replacing Qt too early |
| `tnztools` | Drawing tools and MyPaint brush integration | Needs input replay tests and brush-stroke parity tests |
| `image` | Image IO for raster/video/PSD/SVG/TIFF/PNG/JPEG/etc. | Needs strict compatibility harness before replacing decoders |
| `sound` | Sound IO | Can be ported after audio decode/playback contract is specified |
| `stdfx`, `colorfx` | FX plugins and color processing | Good Rust target after frame comparison harness exists |
| `toonz` | Main OpenToonz GUI application | Last thing to fully replace |
| `toonzfarm` | Farm/controller/server tools | Good candidate for Rust service rewrite |
| `tcleanup`, `tcomposer`, `tconverter` | Command-line tools | Good early headless parity targets |
| `stuff` | Runtime product defaults, rooms, qss, palettes, brushes, textures, FX data | Treat as product behavior and compatibility data |
| `translations` | Qt Linguist translation sources | Needs an i18n migration plan if leaving Qt |

This repository has roughly 2,240 C/C++/ObjC source/header files under
`toonz/sources`, plus 70 Qt translation source files. A simple search across the
main GUI/core areas finds thousands of Qt references. The main app and shared UI
libraries are deeply tied to Qt Widgets, Qt signals/slots, Qt resources,
Qt translations, QOpenGLWidget/QOpenGLFramebufferObject, Qt Script, and
platform-specific Qt packaging.

## What Complete Functionality Means

"Complete functionality" should be defined as compatibility with the existing
artist workflow, not as a line-for-line Rust translation. The Rust project would
need to preserve at least these behaviors:

| Area | Required parity |
|---|---|
| Project and scene model | Projects, rooms, scenes, xsheets, levels, palettes, camera, stage objects, keyframes, paths, hooks, skeletons, mesh columns, preferences, and project defaults |
| File formats | OpenToonz scene files, level formats, palettes, TLV/TZP/TZU/PLI and other native formats, raster formats, SVG, PSD import behavior, audio/video import/export, and legacy path conventions |
| Drawing workflows | Raster, Toonz raster, vector drawing, MyPaint brushes, eraser, fill, onion skin, vector guided drawing, cleanup, vectorization, palette/style workflows, tablet input |
| Xsheet and timeline | Exposure timing, holds, twos, sound columns, notes, keyframe editing, column/row operations, drag/drop behavior |
| Rendering | Preview, flipbook, final render, camera, DPI behavior, vector/raster compositing, color processing, alpha handling, antialiasing, subsampling, caching |
| FX | Standard FX, color FX, schematic graph, plugin host, parameter animation, preview cache, render determinism |
| IO and pipeline | Import/export panels, batch conversion, movie generation, soundtrack export, version control hooks, render farm, packaging data |
| Hardware and platform | Scanning, TWAIN paths, stop-motion/camera capture, Canon SDK when enabled, serial devices, tablets, platform windows, file dialogs |
| UI | Docking, rooms, sheets, viewers, panels, popups, stylesheets, translations, shortcut editing, accessibility expectations |
| Scripting and automation | Qt Script replacement or compatibility layer, command-line tools, batch modes |
| Deployment | macOS app bundle, Windows installer/runtime DLL behavior, Linux install layout, `stuff` copying, third-party binary handling |

The central risk is not Rust itself. The central risk is recreating decades of
implicit behavior without an oracle. That is why the C++ application must become
the oracle before the Rust application tries to replace it.

## GUI Framework Recommendation

### Recommendation: keep Qt during the parity phase

For the compatibility-preserving port, keep the current Qt GUI and move core
logic underneath it to Rust incrementally. This is the lowest-risk path because
OpenToonz is already built around Qt Widgets, MOC-generated classes, Qt resource
files, Qt translations, dock widgets, custom item views, and OpenGL widgets.

Use one of these interop patterns:

| Pattern | Use it for | Avoid it for |
|---|---|---|
| C ABI with `cbindgen` or handwritten headers | Stable service boundaries: scene snapshot export, render jobs, generative package builder, file parsers | Fine-grained Qt object calls |
| `cxx` | Shared Rust/C++ data models, safe typed bridge, core services | QObject integration |
| CXX-Qt | Rust QObjects, signals/properties, threading back to Qt, QML-compatible service models | Directly binding all QWidget APIs |
| Sidecar process | ComfyUI/model orchestration, GPU render workers, crash isolation | Low-latency drawing input loops |

CXX-Qt is a credible bridge for adding Rust objects to Qt/C++ applications. Its
own documentation emphasizes bridging normal Qt code with normal Rust code
rather than one-to-one wrapping every Qt API. It supports Qt 5.15 LTS and Qt 6,
and its QWidget support is limited to C++ bindings for Rust-defined QObject
subclasses, not a complete Rust QWidget replacement. That matches this project:
use Rust for domain services and background workers, not for rewriting every
existing widget in place.

### Recommendation for the generative workbench UI: integrated first, shell second

The generative workbench should be architected as Rust port functionality with
multiple host surfaces, not as a separate Tauri project that happens to import
OpenToonz files. The first-class implementation should live in the Rust
workspace and expose a stable host/plugin contract that can be called by:

- the existing C++/Qt OpenToonz application during migration
- the future Rust scene, FX, render, and cache crates
- a docked Qt panel or CXX-Qt model inside the current app
- optional Tauri/React review windows for rich dailies and production boards
- headless CLI and farm/render workers

This mirrors the current OpenToonz SDK shape. The sample plugin projects under
`plugins/blur`, `plugins/geom`, and `plugins/multiplugin` are standalone modules,
but they are not separate applications. They export `toonz_plugin_init`, advertise
probe metadata, receive a host interface, query host capabilities by UUID, define
parameter pages and input ports, and participate in `TRasterFx` rendering through
callbacks such as `do_compute`, `do_get_bbox`, `can_handle`,
`on_new_frame`, and `on_end_frame`.

The generative workbench should use the same idea at a higher level:

- a versioned Rust ABI for "workbench modules"
- probe metadata for commands, panels, generators, render jobs, and FX nodes
- host capability queries for scene/xsheet/level/palette/render/cache access
- declared parameter pages for model routes, seeds, references, LoRA policy,
  cost limits, and retake modes
- typed input/output ports for drawings, masks, control maps, candidate takes,
  reviewed takes, and cleanup exports
- lifecycle callbacks for package creation, render submission, progress,
  cancellation, completion, review, and retake

Tauri plus React remains useful, but it should be an optional surface over the
same Rust contracts. It is a good fit for dailies boards, A/B/C comparison,
cost/provenance views, and production dashboards. It should not own the scene
schema, shot package format, render queue, or model-route rules. Those belong in
`otz-scene`, `otz-render`, `otz-fx`, `otz-generative`, and `otz-comfy`.

### Recommendation for a future pure-Rust native GUI

If the long-term goal is a native Rust OpenToonz successor, the practical options
are:

| Framework | Fit | Recommendation |
|---|---|---|
| Slint | Native Rust/C++ UI toolkit with declarative UI, cross-platform support, GPU/native rendering options, testing features, and commercial/permissive licensing options | Best candidate for a polished native Rust desktop app if licensing and custom-widget needs are acceptable |
| Tauri/React | Excellent for production-management UI and rapid review/dashboard iteration | Best as an optional workbench surface over Rust port contracts, not as the owner of generative state |
| egui/eframe | Immediate-mode Rust UI, portable, easy to integrate with custom renderers, good for tools/debug/editor panels | Use for internal tools and prototypes, not the primary professional UI |
| Iced | Cross-platform Rust GUI inspired by Elm architecture, type-safe and clean | Worth prototyping, but no clear advantage over Slint/Tauri for this domain |
| Qt via CXX-Qt | Best migration bridge from current OpenToonz | Keep for parity phase; not a pure-Rust GUI endpoint |

The concrete recommendation is:

1. Use existing Qt Widgets for OpenToonz parity.
2. Use Rust host/plugin contracts for the generative workbench core, with Qt,
   Tauri/React, CLI, and farm surfaces all calling the same crates.
3. Use `wgpu` as the shared rendering/GPU abstraction for new Rust viewport,
   preview, control-map, and compositing work.
4. Re-evaluate Slint only after the Rust engine and comparison harness are
   mature. Slint is promising, but replacing the whole Qt surface before the
   engine is stable would multiply risk.
5. Use egui for debug inspectors, render graph test tools, cache viewers, and
   developer-only panes.

## Rust Target Architecture

The Rust workspace should be designed as a clean animation engine plus product
layers, not as a folder-for-folder mirror of `toonz/sources`.

Proposed crates:

| Crate | Responsibility |
|---|---|
| `otz-core` | IDs, geometry, time, frame numbers, affine transforms, errors, diagnostics, task traits |
| `otz-color` | Color models, palettes, style data, LUTs, color-management adapters |
| `otz-raster` | Pixel buffers, tiles, lock-free/borrow-safe image views, SIMD operations, alpha/compositing primitives |
| `otz-vector` | Stroke/path model, region topology, fill solver, vector serialization, tessellation adapters |
| `otz-scene` | Projects, scenes, levels, xsheets, columns, stage objects, cameras, keyframes |
| `otz-fx` | FX node graph, parameters, animation curves, plugin ABI, render DAG |
| `otz-render` | Headless renderer, preview renderer, tile scheduler, cache, frame comparison outputs |
| `otz-gpu` | `wgpu` device/surface management, shaders, GPU compositing, control-map baking |
| `otz-io` | Import/export registry, scene and level parsers, raster/audio/video adapters |
| `otz-audio` | Audio decode, waveform caches, playback abstractions |
| `otz-tools` | Brush engine, fill, cleanup, vectorizer, selection, stroke generators |
| `otz-automation` | Scripting, command runner, batch operations, CLI |
| `otz-generative` | Shot package manifests, role-marked drawings, model routes, prompt policy, provenance |
| `otz-comfy` | ComfyUI API client, workflow template registry, custom-node package metadata |
| `otz-compare` | C++ oracle readers, render metrics, scene snapshots, report generation |
| `otz-ffi` | C ABI or CXX bridge exposed to the existing app |
| `otz-desktop` | Tauri/React or future native shell integration |

The important boundary is that `otz-scene`, `otz-render`, `otz-io`, and
`otz-generative` must be usable headlessly. The GUI should be a client of those
crates, not the owner of core behavior.

## Substitute Library Recommendations

Rust can replace a meaningful part of the dependency stack, but several areas
should remain FFI-backed until parity tests prove otherwise.

| Current dependency or subsystem | Rust replacement or strategy | Recommendation |
|---|---|---|
| Qt Widgets / Qt Core / Qt GUI | Keep Qt during migration; CXX-Qt or C ABI for Rust services; later Tauri or Slint | Do not replace first |
| Qt Script | Rhai, Lua/mlua, Boa/QuickJS, or a compatibility shim | Inventory existing scripts before choosing |
| QOpenGLWidget, OpenGL, GLEW, GLUT | `wgpu` for cross-platform GPU rendering; `glow` only for narrow GL interop | Use `wgpu` for new renderer |
| QPainter-style 2D drawing | `tiny-skia`, `kurbo`, `lyon`, `vello` experiments, custom `wgpu` renderer | Use `tiny-skia`/`lyon` for deterministic CPU/vector tests; evaluate Vello but do not bet production on it yet |
| Vector tessellation | `lyon`, `kurbo`, custom region/fill topology | Use `lyon` for GPU tessellation, but preserve OpenToonz vector semantics separately |
| SVG import/render | `usvg`/`resvg` | Good candidate, with behavior comparison against current SVG importer |
| PNG/JPEG/BMP/TGA/etc. | `image`, `png`, `jpeg-decoder`, `jpeg-encoder`, `turbojpeg` bindings | Use pure Rust for common formats where parity is easy; keep TurboJPEG where performance matters |
| Modified TIFF | `tiff` crate plus custom extensions, or libtiff FFI | Keep current modified libtiff through FFI initially |
| OpenEXR | `exr` crate or ASWF OpenEXR bindings | Use `exr` for tests/prototypes; validate color/deep metadata requirements |
| PSD | Rust PSD crates are not likely enough for full parity | Keep current implementation until a fixture corpus says otherwise |
| FFmpeg/movie IO | ffmpeg CLI sidecar, `ffmpeg-next`, `rsmpeg`, or GStreamer Rust bindings | Use CLI/sidecar for deterministic pipelines first; bind later only where frame-level access is needed |
| QuickTime Windows legacy path | Replace with FFmpeg/GStreamer, keep old C++ helper only for compatibility if required | Deprioritize unless old projects require it |
| Audio decoding | `symphonia` for decode/demux, `cpal`/`rodio` for playback | Good Rust candidate after waveform and sync tests exist |
| OpenCV | `opencv` crate initially; `kornia-rs` or custom Rust algorithms later | Bind first, port algorithms selectively |
| MyPaint brushes | libmypaint FFI | Keep FFI until brush-stroke parity can be measured |
| SuperLU/OpenBLAS | `faer`, `nalgebra`, `ndarray`, or BLAS/LAPACK bindings | Replace only after numerical fixtures and tolerance metrics exist |
| Boost | Rust std, `itertools`, `serde`, `thiserror`, `anyhow`, `camino`, `indexmap` | Replace organically |
| LZ4/LZO/zlib/lzma | `lz4_flex`, `miniz_oxide`, `flate2`, `xz2`, FFI for exact legacy streams | Use pure Rust where exact stream compatibility is verified |
| libusb / serial | `rusb`, `serialport` | Good Rust candidates |
| TWAIN / scanner / Canon SDK | Platform FFI or sidecar process | Keep isolated; do not block engine port |
| Qt Linguist `.ts` | Keep existing translations while Qt remains; later Fluent or Project Fluent-style catalogs | Defer migration |
| Build system | Cargo workspace plus Nix/mise; CMake only for C++ bridge during transition | Preserve reproducible Nix/mise workflow |

## Rendering And Performance Enhancements

Rust is not automatically faster than the current C++ code. The performance win
comes from redesigning the data flow while maintaining compatibility.

Concrete opportunities:

1. Tile-based raster storage with cache-aware layouts.
   - Store frame buffers as tiles with explicit pixel formats.
   - Use copy-on-write or immutable tile snapshots for undo and preview caches.
   - Make dirty regions explicit and hashable.

2. Render DAG scheduling.
   - Represent scene/FX renders as a deterministic task graph.
   - Cache node outputs by content hash, frame, camera, DPI, palette version,
     and FX parameter version.
   - Re-render only invalidated tiles.

3. `rayon` and work-stealing CPU parallelism.
   - Apply parallelism to tile filters, vector rasterization batches, image
     conversion, thumbnail generation, control-map baking, and comparison tests.
   - Keep GUI/event loops non-blocking.

4. GPU compositing and preview with `wgpu`.
   - Move viewport compositing, onion-skin overlays, color transforms, and
     common FX previews onto the GPU.
   - Use compute paths for control maps, masks, depth/edge preprocessors, and
     selected filters.

5. Vector renderer modernization.
   - Keep OpenToonz vector semantics in a custom Rust model.
   - Tessellate through `lyon` or a purpose-built path pipeline.
   - Evaluate Vello for large vector scenes and interactive previews, but treat
     its current alpha status as a risk for production parity.

6. Safer memory ownership.
   - Replace ad hoc pointer ownership with arenas, generational IDs, `Arc`
     snapshots, and explicit borrow scopes.
   - Avoid sharing mutable scene state directly with background render workers.

7. SIMD and pixel specialization.
   - Specialize hot pixel kernels by format.
   - Use stable SIMD where practical and keep scalar reference kernels for tests.

8. IO and proxy caches.
   - Generate thumbnails, waveforms, proxies, and control maps asynchronously.
   - Use content hashes to avoid recomputing imported assets.

9. Generative preprocessing on the same graph.
   - Bake line, mask, depth, pose, exposure timing, and sparse track maps from
     the OpenToonz scene graph.
   - Cache those artifacts as first-class production assets.

## Automated Comparison Testing Against C++

The comparison harness is the most important part of the migration. It should be
implemented before substantial Rust feature replacement.

### Test architecture

Create a `comparison/` or `tests/comparison/` tree with:

```text
comparison/
  fixtures/
    scenes/
    levels/
    palettes/
    images/
    audio/
    scripts/
  manifests/
    fixture_index.toml
    expected_capabilities.toml
  cpp_oracle/
    export_scene_snapshot.cpp
    render_frame_driver.cpp
    import_export_driver.cpp
  rust_oracle/
    scene_snapshot.rs
    render_frame.rs
    import_export.rs
  reports/
```

The C++ oracle should expose command-line, deterministic operations:

```sh
opentoonz-cpp-oracle scene-snapshot input.tnz --json out/cpp_scene.json
opentoonz-cpp-oracle render-frame input.tnz --frame 42 --png out/cpp_0042.png
opentoonz-cpp-oracle roundtrip-level input.tlv --out out/cpp_roundtrip.tlv
```

The Rust candidate should expose the same contract:

```sh
otz-rust scene-snapshot input.tnz --json out/rust_scene.json
otz-rust render-frame input.tnz --frame 42 --png out/rust_0042.png
otz-rust roundtrip-level input.tlv --out out/rust_roundtrip.tlv
```

Then `otz-compare` should produce a report:

```sh
otz-compare run comparison/manifests/fixture_index.toml \
  --cpp-bin toonz/build/nix-relwithdebinfo/bin/opentoonz-cpp-oracle \
  --rust-bin target/release/otz-rust \
  --out comparison/reports/latest
```

### Fixture corpus

The fixture corpus must include:

- empty scenes
- simple raster levels
- Toonz raster levels
- vector levels with strokes, fills, styles, closed and open regions
- palette edge cases
- xsheets with holds, twos, blanks, sound, notes, and nested levels
- camera and DPI edge cases
- cleanup and vectorizer examples
- FX graphs with animated parameters
- mesh/plastic deformation examples
- imported PNG/JPEG/TIFF/TGA/BMP/SVG/PSD examples
- video/audio import examples
- old project files from multiple OpenToonz versions
- platform path edge cases
- non-English file names and translated UI strings
- large scenes for performance baselines

Fixtures should be small enough for CI but realistic enough to catch hidden
behavior. Large production scenes should run in a nightly or local benchmark
profile.

### Comparison metrics

Use different metrics by layer:

| Output | Metric |
|---|---|
| Scene snapshot JSON | Exact semantic equality after canonical sorting and path normalization |
| Native scene/level round-trip | Byte equality where possible; otherwise semantic equality plus stable rewrite warnings |
| Rendered raster frames | Exact pixel match for deterministic CPU paths; thresholded RGBA delta, SSIM/PSNR, perceptual hash, and alpha-edge checks for GPU/AA paths |
| Vector data | Topology, stroke count, control points, region adjacency, fill styles, bounding boxes, rasterized preview |
| Palette/style data | Exact style IDs, colors, names, references, and behavior under reassignment |
| FX graph | Node graph isomorphism, parameter values, animated curves, rendered result |
| UI behavior | Event replay logs, accessibility tree snapshots, screenshots, focus/shortcut state |
| Performance | Frame time percentiles, memory high-water mark, cache hit rate, thread utilization |

### UI comparison

GUI parity should not rely only on manual testing. Use a layered approach:

1. Instrument current Qt UI actions so important workflows can be replayed:
   open scene, expose drawings, draw stroke, fill region, edit palette, apply FX,
   render preview, export package.
2. Capture structured state after each action.
3. Capture screenshots at fixed resolution, theme, DPI, and locale.
4. Compare screenshots with tolerant visual metrics and manually review diffs.
5. For optional Tauri/React workbench surfaces, use Playwright for browser-level
   flows.
6. For Qt/Slint/native surfaces, use a native automation harness plus screenshot
   capture.

### Fuzzing and property tests

Add fuzz/property tests for:

- file parsers
- palette/style references
- xsheet operations
- vector region topology
- undo/redo invariants
- FX graph serialization
- path normalization
- image decoders for untrusted inputs

The core invariant is: Rust must not silently accept a file and change its
meaning. Every lossy conversion must be explicit in the report.

## Generative Workbench Direction

The generative layer should be built as a shot-aware production system inside
the Rust port, not as a prompt box and not as a sidecar project. It should be a
first-class extension family that exercises the same host/module boundaries the
Rust rewrite needs for FX, tools, render jobs, scene IO, farm workers, and future
third-party integrations.

### SDK inspiration from current OpenToonz plugins

The current sample SDK projects are small, but their shape is exactly the right
inspiration:

| SDK pattern | Where it appears | Generative/Rust-port lesson |
|---|---|---|
| Shared module with `.plugin` suffix | `plugins/blur/CMakeLists.txt`, `plugins/geom/CMakeLists.txt`, `plugins/multiplugin/CMakeLists.txt` | A workbench capability should be loadable and versioned, not hardwired into one UI process |
| Probe metadata | `TOONZ_PLUGIN_PROBE_DEFINE` in plugin examples | Generators, panels, model routes, and render nodes should advertise stable IDs, names, vendor, version, help URL, class, and capabilities |
| Host initialization | `toonz_plugin_init(toonz::host_interface_t *hostif)` | Rust workbench modules should receive a host handle rather than reaching into globals |
| Capability query | `hostif->query_interface(TOONZ_UUID_..., &interface)` | The Rust port should expose scene, xsheet, tile, FX, cache, render, package, and model-route interfaces by UUID/version |
| Parameter pages | `set_parameter_pages_with_error(...)` | Model settings should be declared as host-visible parameters, not hidden in a separate web app state model |
| Input ports | `add_input_port(node, "IPort", TOONZ_PORT_TYPE_RASTER)` | Generative nodes need typed ports for drawings, masks, palettes, timing, control maps, prompts, candidate takes, and cleanup outputs |
| Render callbacks | `do_compute`, `do_get_bbox`, `can_handle`, frame lifecycle callbacks | Local preprocessing, control-map baking, preview generation, and selected generative FX should participate in the render graph |
| Tile access | `toonz_tile_interface_t` raw address, stride, type, copy, rectangle | Rust must own safe wrappers around tiles so control maps and comparison tests can run inside the same render/cache model |
| Multi-plugin module | `plugins/multiplugin` | A single Rust dynamic library can advertise several related capabilities: panels, FX nodes, package builders, route validators, and review exporters |

The existing SDK only exposes enough surface for raster FX plugins. The Rust port
should generalize that model into an `otz-workbench-sdk`: same versioned,
capability-query style, but with richer interfaces for scene selection, xsheet
timing, level/frame access, palette/style data, camera data, render jobs,
background task status, and persistent production metadata.

### Integrated Rust architecture

The generative workbench should be split into host-facing modules rather than
one external app:

| Rust crate/module | Port integration role |
|---|---|
| `otz-workbench-sdk` | C ABI and Rust traits for module probe, lifecycle, host query, parameter descriptors, typed ports, callbacks, and capability negotiation |
| `otz-generative` | Canonical shot package, model package, candidate take, retake, provenance, cost, and review data types |
| `otz-scene` | Owns projects, scenes, xsheets, levels, frame IDs, cameras, stage objects, and selection snapshots consumed by generative modules |
| `otz-fx` | Hosts generative preprocessing nodes as graph nodes with declared ports and parameters |
| `otz-render` | Provides deterministic frame/tile rendering, preview exports, control-map baking, and selected-take import/export |
| `otz-cache` | Stores thumbnails, proxies, control maps, model outputs, hashes, and invalidation metadata |
| `otz-comfy` | Calls ComfyUI or local model routes from a render-job interface, not from UI-only code |
| `otz-review` | Stores candidate ranking, notes, frame annotations, retake requests, approval state, and cleanup export decisions |
| `otz-ui-model` | Exposes dockable panel models to Qt/CXX-Qt now and to future Slint/Tauri/native shells later |

The host contract should support both directions:

1. OpenToonz/Rust host calls a workbench module to build packages, validate
   routes, create control maps, submit jobs, and ingest takes.
2. Workbench modules query the host for current scene state, selected xsheet
   range, source tiles, palette styles, camera box, render settings, cache paths,
   cancellation flags, and project storage.

That is the difference from a sidecar. The generative workbench does not parse a
rendered export after the fact. It sees the same logical scene and render graph
the Rust port sees.

### Canonical data ownership

The anim-workbench local spec still points in the right direction: Rust domain
crates, local-first persistence, exposure cells, role-marked drawings, shot
package manifests, ComfyUI/model-route helpers, and review metadata. In the
OpenToonz Rust port, those concepts should be promoted into the canonical engine
model:

| Generative object | Rust/OpenToonz owner |
|---|---|
| Shot | `otz-scene` selection over scene, frame range, camera, xsheet section, and frame rate |
| Keyframe package | `otz-generative` view over role-marked level drawings and selected cells |
| Timing package | `otz-scene` exposure cells, holds, twos, blanks, sound/timecode, and retime policy |
| Control maps | `otz-render`/`otz-fx` generated line, mask, depth proxy, pose/sparse track, and onion delta outputs |
| Style package | `otz-color` and `otz-generative` references to palettes, model sheets, prompt fragments, and LoRA policy |
| Background package | `otz-scene` columns plus `otz-render` camera crop, plate, depth hints, and masks |
| Model route | `otz-comfy` or cloud/local route descriptor registered through `otz-workbench-sdk` |
| Candidate take | `otz-generative` asset with seed, workflow hash, model versions, source asset hashes, cost, and review status |
| Retake | `otz-review` operation tied to frame/timecode, failure label, notes, and source package version |
| Cleanup export | `otz-io` write-back into scene-friendly folders, image sequences, levels, or future native level records |

Persistence should be local-first, but not separate. Use a project-level
workbench database or manifest directory under the OpenToonz project, keyed by
stable scene/shot IDs and content hashes. The scene file should reference the
generative package state enough to reopen the workbench; large generated assets
should live in the project asset/cache store.

### Host-visible generative nodes

The first module family should register several capabilities, similar to how
`plugins/multiplugin` exposes multiple plugin descriptors from one library:

| Capability | Host integration |
|---|---|
| Shot Package Builder | Command/panel that consumes the current xsheet selection and writes a canonical package |
| Control Map Baker | FX/render node that produces line, mask, depth proxy, motion guide, and palette masks from scene tiles |
| Model Route Validator | Background task that checks ComfyUI/cloud route availability and required workflow inputs |
| Generative Render Job | Render/farm task that submits a package to a route, tracks progress, supports cancellation, and records provenance |
| Candidate Take Importer | IO/action module that registers generated outputs as scene assets or cleanup references |
| Dailies Review Panel | Dockable UI model for comparing takes, writing frame notes, marking retakes, and approving selected takes |
| Generative FX Node | Optional node in the FX schematic for local image/video-to-image operations where deterministic preview is possible |

The UI should be dockable inside the current application during migration. A
Tauri/React surface can still be used for a richer dailies board, but it should
connect to the same `otz-ui-model` and `otz-review` stores as the Qt panel. It
should be a view, not the product boundary.

### Model-route integration

ComfyUI should be treated like an external render backend invoked from the
Rust render/job system. Its docs describe API-mode workflows and custom nodes;
that maps naturally onto an OpenToonz host that prepares typed inputs and records
typed outputs.

Recommended model routes for experimentation:

| Route | Integrated use |
|---|---|
| LTX-2 through ComfyUI | Local/open route for image-to-video, keyframe-conditioned motion, LoRA, and control-map experiments |
| Wan 2.2 through ComfyUI | Comparison route for image/video generation with the same shot package inputs |
| Cloud model adapters | Quality benchmark and fallback route behind the same `ModelRoute` trait |
| Interpolation/cleanup route | Lower-risk in-between, limited-animation, line stabilization, and cleanup assistant route |
| Non-generative render route | Baseline OpenToonz/Rust render route proving timing, camera, and source assets are unchanged |

The key integration rule: every route receives a typed `ModelPackage`, never a
loose prompt plus a folder path. Every output returns a typed `CandidateTake`.

### First integrated workflow

The first useful workflow should run inside the OpenToonz/Rust host:

1. User selects a frame range or shot in the xsheet.
2. User marks drawings as start, breakdown, end, control, cleanup, or retake.
3. The Shot Package Builder queries the host for scene, xsheet, level, palette,
   camera, and source tile data.
4. The Control Map Baker creates line/mask/depth/sparse-track outputs through
   the render/cache interfaces.
5. The Model Route Validator checks required inputs and estimates cost.
6. The Generative Render Job submits a typed model package to ComfyUI/local/cloud
   routes through the Rust job system.
7. Candidate takes return with seed, route, workflow hash, source asset hashes,
   cost, and frame/timecode metadata.
8. The Dailies Review Panel records approvals or retake requests in the project
   workbench store.
9. The Candidate Take Importer writes selected outputs back as scene assets,
   cleanup references, or renderable columns.

This can start while the main app is still C++/Qt, but it should be implemented
as the first production-grade consumer of the Rust port's scene, render, cache,
and host-interface crates. That makes generative work a forcing function for the
Rust architecture instead of a parallel experiment.

## Phased Migration Plan

### Phase 0: Compatibility harness and architecture freeze

Duration: 2 to 4 months with 2 to 3 engineers.

Deliverables:

- C++ oracle command-line tools
- fixture corpus
- scene snapshot schema
- frame render comparison report
- dependency inventory
- license inventory
- Rust workspace skeleton
- FFI proof of concept
- CI job that runs a small comparison set

Exit criteria:

- At least 50 representative fixtures compare successfully through the C++
  oracle.
- The team can explain which parts require exact parity and which may be
  intentionally redesigned.

### Phase 1: Integrated Rust generative workbench module

Duration: 3 to 6 months with 2 to 4 engineers.

Deliverables:

- `otz-workbench-sdk` host/module ABI inspired by the current plugin SDK
- `otz-generative` model package, candidate take, retake, provenance, and cost
  types
- C++/Qt bridge that lets the current OpenToonz host call the Rust workbench
  module for selected xsheet ranges
- dockable Qt or CXX-Qt workbench panel backed by Rust `otz-ui-model` state
- optional Tauri/React dailies surface that talks to the same Rust state
- ComfyUI API client behind a `ModelRoute` trait
- Control Map Baker integrated with the render/cache interfaces
- candidate take import/export path that writes back into scene assets or
  cleanup references

Exit criteria:

- An artist can select an xsheet range in OpenToonz, build a typed model package
  through the Rust workbench module, bake control maps from the same scene/render
  data used by the port, generate candidates through a local ComfyUI route,
  review takes in a host-integrated panel, and write a selected take back into
  the scene or cleanup workflow.

### Phase 2: Rust scene and IO read path

Duration: 6 to 12 months with 3 to 5 engineers.

Deliverables:

- Rust readers for core project/scene/xsheet/level/palette formats
- semantic JSON snapshot parity
- read-only Rust scene browser
- round-trip tests for low-risk formats
- fuzz tests for parsers

Exit criteria:

- Rust can load and semantically inspect a meaningful fixture corpus without
  mutating files.

### Phase 3: Headless Rust renderer parity

Duration: 9 to 18 months with 4 to 6 engineers.

Deliverables:

- raster/vector core
- palette/style application
- camera/DPI behavior
- initial FX graph
- tile render scheduler
- CPU reference renderer
- image comparison report

Exit criteria:

- Rust renders representative scenes close enough to C++ that diffs are
  understood, categorized, and tracked.

### Phase 4: Tools, vector semantics, cleanup, and brush parity

Duration: 12 to 24 months with 4 to 8 engineers.

Deliverables:

- brush-stroke replay tests
- MyPaint FFI or Rust brush equivalent
- vector topology and fill solver
- cleanup/vectorizer parity
- undo/redo invariants
- tablet/input replay

Exit criteria:

- The Rust engine can create and modify drawings with measured parity for core
  production workflows.

### Phase 5: New desktop shell

Duration: 9 to 18 months with 4 to 8 engineers, overlapping earlier phases only
after the engine API stabilizes.

Deliverables:

- Tauri or Slint shell prototype
- docking/timeline/xsheet/canvas surface
- Rust renderer integration
- packaging story
- translation/i18n plan
- UI event comparison tests

Exit criteria:

- The new shell can run a real workflow end to end without falling back to the
  old app for basic drawing, timing, preview, render, and export.

### Phase 6: Plugin, scripting, farm, and platform parity

Duration: 6 to 18 months depending on supported scope.

Deliverables:

- scripting replacement
- plugin ABI or compatibility bridge
- farm rewrite
- scanner/camera platform integration
- installer/package equivalents

Exit criteria:

- Production teams can migrate without losing critical platform workflows.

## Effort Estimate

These estimates assume serious parity with the current application.

| Goal | Team | Time | Outcome |
|---|---:|---:|---|
| Integrated generative workbench module | 2 to 4 engineers plus artist/pipeline tester | 3 to 6 months | Rust host/module SDK, Qt-integrated panel, ComfyUI route, package/review/write-back loop |
| Rust read-only compatibility engine | 3 to 5 engineers | 6 to 12 months | Load/inspect scenes, export packages, compare metadata |
| Rust headless renderer parity | 4 to 6 engineers | 12 to 24 months | Render meaningful scenes, run automated frame comparison |
| Full Rust production app with partial OpenToonz parity | 6 to 10 engineers | 24 to 36 months | New app useful for selected workflows |
| Full OpenToonz replacement | 8 to 12+ engineers | 3 to 5+ years | Only plausible with disciplined comparison testing and scoped compatibility decisions |

A solo or small-team experiment should not aim for full parity first. It should
aim for the integrated generative module and compatibility harness.

## Risks And Mitigations

| Risk | Why it matters | Mitigation |
|---|---|---|
| GUI rewrite consumes the project | The current Qt surface is huge and behavior-rich | Keep Qt until the Rust engine has parity |
| File format drift | Artists need old projects to load correctly | Build read-only parsers and semantic snapshots before writers |
| Renderer differences are invisible until late | Tiny differences affect production output | Add frame comparison and fixture reports first |
| Rust GUI ecosystem churn | Framework choices may change faster than the port | Keep GUI choice isolated from core crates |
| Generative model churn | LTX/Wan/cloud routes will change quickly | Use model-route registry, pinned workflow hashes, and provenance |
| GPU nondeterminism | GPU output may vary by backend | Keep CPU reference renderer and tolerant GPU metrics |
| Licensing | OpenToonz, third-party code, brushes, Qt, Slint, model weights may have incompatible terms | Run license inventory before copying code or shipping model integrations |
| Plugin ABI | Existing FX/plugin contracts may not map cleanly to Rust | Use a versioned plugin bridge and keep C ABI where needed |
| Hardware integrations | Scanner, Canon, serial, tablet paths are platform-specific | Isolate behind sidecars or FFI modules |
| Scope collapse | "Rewrite OpenToonz" can absorb all available time | Make the host-integrated generative module the first product milestone |

## Concrete Technical Decisions

Recommended now:

- Create a Rust workspace beside the C++ tree, not inside `toonz/sources`.
- Preserve Nix/mise as the reproducible developer entry point.
- Add C++ oracle tools before replacing behavior.
- Define `otz-workbench-sdk` as a versioned host/module ABI inspired by the
  current OpenToonz plugin SDK.
- Use Rust for canonical generative package/export/review/job logic.
- Use a dockable Qt or CXX-Qt surface as the first workbench UI inside the
  current application.
- Use Tauri/React only as an optional rich review/dashboard surface over the
  same Rust state.
- Use C ABI or CXX/CXX-Qt to call Rust from the current Qt application and to
  let Rust modules query host capabilities.
- Use ComfyUI as the first render backend integration.
- Use `wgpu` for new GPU renderer/control-map work.
- Keep modified libtiff, MyPaint, OpenCV, FFmpeg/video, scanner/camera, and
  platform hardware paths behind FFI or sidecars initially.
- Use `image-rs`, `resvg/usvg`, `symphonia`, `rusb`, `serialport`, `serde`,
  `rayon`, `tracing`, `criterion`, and `proptest` where they fit cleanly.

Recommended later:

- Evaluate Slint for a native Rust UI only after engine APIs and comparison
  tests are mature.
- Evaluate Vello for interactive vector scenes, but keep a CPU/reference path
  and custom vector semantics.
- Replace C++ FX one family at a time, starting with deterministic image/color
  effects.
- Replace command-line tools before the main GUI.
- Replace farm/controller services before mature drawing widgets.

Not recommended:

- Big-bang rewrite.
- Replacing Qt first.
- Treating Tauri as a complete substitute for the professional drawing/viewer
  surface without a native/GPU viewport plan.
- Replacing image/file decoders without a fixture corpus.
- Baking model-specific behavior directly into scene files.
- Letting prompt text become the only source of continuity.

## Suggested First Milestones For This Checkout

1. `doc/rust_migration_study.md` as the decision baseline.
2. `rust/` workspace with `otz-core`, `otz-workbench-sdk`, `otz-generative`,
   `otz-comfy`, `otz-review`, `otz-compare`, and `otz-cli`.
3. C++ command-line oracle target that can export a scene/xsheet snapshot to
   canonical JSON.
4. Rust host/module ABI modeled after the current SDK:
   - module probe metadata
   - host initialization
   - capability query by UUID/version
   - parameter page descriptors
   - typed ports
   - lifecycle callbacks
   - cancellation/progress reporting
5. Shot/model package manifest schema:
   - project
   - sequence
   - shot
   - frame range
   - frame rate
   - aspect/camera
   - exposure cells
   - role-marked drawings
   - palette/style references
   - masks/control maps
   - prompt fragments
   - model route policy
   - provenance
6. Current OpenToonz bridge command/panel that calls Rust to build a typed
   model package from the selected xsheet range.
7. Control Map Baker module that reads host scene/tile/render data and writes
   cached line/mask/depth/sparse-track artifacts.
8. Host-integrated review panel that validates a ComfyUI route, submits a job,
   records candidate takes, and writes a selected take back into the scene or
   cleanup package.
9. CI comparison job for a tiny fixture set.

This sequence provides learning quickly and does not require betting the whole
application on a Rust rewrite.

## Source Notes

External sources checked for current framework and backend facts:

- CXX-Qt: https://github.com/KDAB/cxx-qt and https://kdab.github.io/cxx-qt/book/
- Tauri: https://v2.tauri.app/start/ and https://v2.tauri.app/concept/inter-process-communication/
- Slint: https://slint.rs/
- egui: https://github.com/emilk/egui
- Iced: https://github.com/iced-rs/iced
- wgpu: https://github.com/gfx-rs/wgpu
- Vello: https://github.com/linebender/vello
- resvg/usvg: https://github.com/linebender/resvg
- image-rs: https://github.com/image-rs/image
- ComfyUI custom nodes/API mode: https://docs.comfy.org/custom-nodes/overview
- LTX-2 ComfyUI integration: https://docs.ltx.video/open-source-model/integration-tools/comfy-ui
- Wan 2.2 ComfyUI examples: https://comfyanonymous.github.io/ComfyUI_examples/wan22/

Local references used:

- `toonz/sources/CMakeLists.txt`
- `toonz/sources/CMakePresets.json`
- `mise.toml`
- `flake.nix`
- `doc/how_to_build_nix_mise.md`
- `doc/how_to_build_linux.md`
- `doc/how_to_build_macosx.md`
- `doc/how_to_build_win.md`
- `toonz/sources/toonzqt/toonz_plugin.h`
- `toonz/sources/toonzqt/toonz_hostif.h`
- `toonz/sources/toonzqt/pluginhost.cpp`
- `toonz/sources/include/toonzqt/pluginloader.h`
- `plugins/blur/blur.cpp`
- `plugins/geom/geom.cpp`
- `plugins/multiplugin/multi.cpp`
- `/Users/briangyss/src/anim-workbench/README.md`
- `/Users/briangyss/src/anim-workbench/docs/opentoonz_inspired_mapping.md`
- `/Users/briangyss/src/anim-workbench/docs/book/src/drawing-workbench.md`
- `/Users/briangyss/src/anim-workbench/generative_toonboom_six_month_software_plan.md`
