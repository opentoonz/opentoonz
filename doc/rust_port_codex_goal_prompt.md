# Codex Goal Prompt: OpenToonz Rust Port, Integrated Generative Workbench, And Comparison Harness

Use this prompt to start a long-running Codex implementation session in this
checkout.

## Goal Prompt

You are working in `/Users/briangyss/.codex/worktrees/d160/opentoonz`.

Your objective is to turn the Rust migration study into an executable,
verified project scaffold for an OpenToonz Rust port. The deliverable must
include the comparison harness required to verify Rust behavior against the
current C++ OpenToonz implementation. The generative animation workbench must be
deeply integrated with the Rust port through a host/module SDK inspired by the
current OpenToonz plugin SDK, not implemented as a detached sidecar app.

Read these first:

1. `AGENTS.md`
2. `doc/rust_migration_study.md`
3. `toonz/sources/toonzqt/toonz_plugin.h`
4. `toonz/sources/toonzqt/toonz_hostif.h`
5. `toonz/sources/toonzqt/pluginhost.cpp`
6. `plugins/blur/blur.cpp`
7. `plugins/geom/geom.cpp`
8. `plugins/multiplugin/multi.cpp`
9. `mise.toml`
10. `flake.nix`

## Non-Negotiable Direction

- Do not attempt a big-bang rewrite of OpenToonz.
- Do not replace Qt first.
- Do not make the generative workbench a separate Tauri-only product.
- Do not start broad source rewrites before the comparison harness exists.
- Keep the existing C++/Qt application buildable.
- Preserve the Nix + mise workflow.
- Treat automated comparison against C++ as the first-class correctness oracle.
- Keep changes scoped, documented, and easy to continue by another agent.

## Required Architecture

Create a Rust workspace under `rust/` with these crates unless the live repo
proves a better nearby naming scheme:

- `otz-core`: IDs, frame numbers, geometry, affine transforms, errors,
  diagnostics, path-safe types, and common serialization helpers.
- `otz-workbench-sdk`: versioned host/module ABI and Rust traits inspired by the
  existing OpenToonz SDK: module probe metadata, host initialization,
  capability query by UUID/version, parameter descriptors, typed ports,
  lifecycle callbacks, progress/cancellation, and safe FFI boundaries.
- `otz-generative`: canonical `ShotPackage`, `ModelPackage`, `CandidateTake`,
  `RetakeRequest`, provenance, source asset hashes, role-marked drawings, and
  model-route metadata.
- `otz-comfy`: ComfyUI endpoint validation, workflow-template metadata, and a
  `ModelRoute` trait that consumes typed `ModelPackage` values and returns
  typed job/take metadata. Network calls must be optional or mockable in tests.
- `otz-review`: review notes, frame/timecode annotations, approval states,
  retake failure labels, and selected-take export decisions.
- `otz-compare`: canonical comparison types, JSON normalization, image metric
  helpers, report generation, and fixture-manifest loading.
- `otz-cli`: command-line entry point for Rust snapshot, package, comparison,
  and mock route workflows.

The Rust workspace must build with Cargo and be reachable from the repo's
existing Nix/mise workflow. Add or update tasks only when needed and document
them.

## Required C++ Oracle

Add a minimal C++ oracle target to the existing CMake build. It does not need to
cover the entire OpenToonz feature surface in the first pass, but it must prove
the pattern for deterministic C++ behavior export.

The oracle should support commands shaped like:

```sh
opentoonz-cpp-oracle scene-snapshot <input> --json <output>
opentoonz-cpp-oracle render-frame <input> --frame <n> --png <output>
opentoonz-cpp-oracle roundtrip-level <input> --out <output>
```

If a fully functional renderer hook is too large for the first pass, implement a
truthful subset and mark unsupported operations explicitly in machine-readable
output. Do not fake parity. The harness must distinguish:

- passed
- failed
- unsupported
- skipped because fixture requires an unimplemented oracle capability

The C++ oracle should produce canonical JSON for at least repository metadata,
fixture metadata, path normalization checks, and any scene/level data that can
be safely read in the first implementation slice.

## Required Comparison Harness

Create a comparison tree such as:

```text
comparison/
  fixtures/
    scenes/
    levels/
    palettes/
    images/
    audio/
  manifests/
    fixture_index.toml
    expected_capabilities.toml
  reports/
```

Implement `otz-compare` and/or `otz-cli compare` so a developer can run one
command that:

1. reads the fixture manifest
2. invokes the C++ oracle where required
3. invokes the Rust implementation where required
4. compares canonical JSON outputs
5. compares image outputs when available
6. writes a human-readable Markdown report and a machine-readable JSON report
7. exits non-zero on unexpected failure

The comparison harness must include:

- semantic JSON canonicalization
- stable path normalization
- fixture capability flags
- unsupported/skipped handling
- deterministic report paths
- at least one tiny fixture that can run in CI without large binary assets
- tests for the report and comparison logic

Do not require full OpenToonz sample projects for the first harness pass. Build
the harness so richer scenes can be added later.

## Required Integrated Generative Workbench Scaffold

Implement the generative layer as a first-class Rust module family:

- The `otz-workbench-sdk` must model the existing plugin SDK's useful ideas:
  probe metadata, host interface, queryable capabilities, parameter pages,
  typed ports, lifecycle callbacks, and multi-capability modules.
- `otz-generative` must define typed packages that can be built from scene,
  xsheet, level, palette, camera, source tile, and render/cache data as those
  host capabilities become available.
- The first implementation may use mock host data, but the API must make the
  host relationship explicit. Avoid APIs that only accept loose file folders and
  prompts.
- The first module family should include data types or stubs for:
  - Shot Package Builder
  - Control Map Baker
  - Model Route Validator
  - Generative Render Job
  - Candidate Take Importer
  - Dailies Review Panel model
  - optional Generative FX Node descriptor
- Every model route must consume a typed `ModelPackage`.
- Every generated result must return a typed `CandidateTake`.
- Store provenance: source hashes, workflow hash, route ID, model version,
  seed, cost estimate, created time, and package version.

Tauri/React may be mentioned or scaffolded only as an optional surface. The
state, package model, review data, and route rules must live in Rust crates.

## Required Tests

Add focused tests for:

- Rust workspace build and crate-level unit tests
- host/module SDK descriptor validation
- UUID/version capability negotiation
- parameter descriptor validation
- typed port validation
- shot/model package serialization
- candidate take and provenance serialization
- ComfyUI route validation with a mock or local-only mode
- comparison manifest loading
- comparison JSON canonicalization
- report generation
- unsupported/skipped fixture behavior

If image metrics are implemented, include small generated test images and test
exact match plus thresholded mismatch reporting.

## Required Documentation

Update or add docs that explain:

- how to build the Rust workspace
- how the C++ oracle is built
- how to run the comparison harness
- how to add a fixture
- how unsupported fixture capabilities are represented
- how the workbench SDK maps to the current OpenToonz plugin SDK
- how the generative package model is integrated with the Rust port
- what is intentionally not implemented yet

Keep `doc/rust_migration_study.md` consistent with implementation reality if
the implementation changes a recommendation.

## Validation Commands

Run the smallest practical validation after each phase. At minimum, try:

```sh
mise run doctor
mise run configure
cargo test --manifest-path rust/Cargo.toml
cargo run --manifest-path rust/Cargo.toml -p otz-cli -- --help
cargo run --manifest-path rust/Cargo.toml -p otz-cli -- compare \
  --manifest comparison/manifests/fixture_index.toml \
  --out comparison/reports/latest
```

If Cargo is not available directly in the environment, add or use a Nix/mise
task and document the exact command that works. If a full OpenToonz build is too
large, run configure plus the specific C++ oracle target if available. Be clear
about any validation that could not be run.

## Completion Criteria

The goal is complete only when:

1. the repo contains a Rust workspace with the required crates or documented
   equivalent modules
2. the Rust workspace builds and its tests pass
3. the C++ oracle target exists and is wired into the build or a clearly
   documented partial build path
4. the comparison fixture tree exists
5. the comparison harness runs on at least one tiny fixture and produces JSON
   and Markdown reports
6. unsupported oracle/Rust capabilities are represented honestly, not hidden
7. the integrated generative workbench data model and SDK scaffold exist
8. the generative model route path is typed around `ModelPackage` and
   `CandidateTake`
9. docs explain how to continue adding real scene, render, image, and UI parity
   tests
10. final handoff lists files changed, commands run, results, and remaining
    gaps

## Working Style

Work in phases and keep the tree buildable. Start with the harness and Rust
workspace skeleton, then add the C++ oracle, then implement comparison reports,
then add generative SDK/package scaffolding, then wire validation tasks and docs.
Prefer narrow, durable contracts over broad stubs. If you hit a blocker, record
the blocker in docs and make the nearest useful automated test pass.
