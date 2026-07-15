# Codex Goal Prompt: Complete The OpenToonz Qt 6 Migration

Use this prompt to start or continue an implementation session in the
OpenToonz repository root.

Last audited: July 14, 2026
Authoritative audit commit: `82a9a8e58f88aabdd3c6fad32d4adcf86cac48c0`

## Goal

Complete the Qt 5 to Qt 6 migration as an incremental, evidence-backed product
migration. Preserve the Qt 5 lane until maintainers intentionally retire it.
Do not treat compilation, static guards, synthetic smokes, or safe execution as
substitutes for supported-platform package and workflow parity.

The Metal migration remains out of scope until the Qt 6 port meets this
prompt's completion contract. Qt 6 viewer, rendering, drawing, shader, input,
and visual-parity work is in scope when it closes an identified requirement.

## Read These First

1. `AGENTS.md`
2. `doc/qt6_port_progress/2026-07-13-branch-audit.md`
3. `doc/qt6_implementation_recommendations.md`
4. `doc/qt6_remaining_work_and_manual_verification.md`
5. `doc/qt6_migration_study.md`
6. `doc/how_to_build_nix_mise.md`
7. `toonz/sources/CMakeLists.txt`
8. `toonz/sources/CMakePresets.json`
9. `nix/opentoonz-env.nix`
10. `mise.toml`
11. `scripts/qt6/check-migration-preflight.sh`
12. `.github/workflows/workflow_qt6_experimental_binaries.yml`
13. the regular Linux, macOS, and Windows workflows relevant to the change

Treat the dated audit as the current snapshot. Treat older dated progress
reports as historical evidence, not current truth. If the branch tip, live
`master`, toolchain, or CI state has changed, refresh the affected evidence
before implementing.

## Current Snapshot

As audited on July 13, 2026:

| Workstream | State | Next proof |
|---|---|---|
| `QT6-INT-01` Branch integration | Integrated locally | Merge commit `991804f3e`; eight overlap decisions and the subsystem review map are recorded in the July 14 delta; draft PR remains |
| `QT6-BLD-01` Dual Qt lane | Advanced | Same-commit Qt 5, Qt 6, and strict Qt 6 builds after integration |
| `QT6-VER-01` Qt version policy | Adopted, evidence pending | Qt 6.9 floor, Qt 6.10.3 release, Qt 6.11.x forward; prove all lanes |
| `QT6-API-01` Deprecated/removed APIs | Advanced | Test the 6.9 floor and 6.10 release Qt; strict gate disables through 6.10 |
| `QT6-SCR-01` QJSEngine migration | Advanced but blocked | Prove Script Console responsiveness and bounded interruption; publish a supported-binding matrix |
| `QT6-REN-01` Viewer/render/shader | Advanced but blocked | Rerun the current-tip bundled shader probe and require expected nonzero pixels |
| `QT6-MED-01` Multimedia/hardware | Partial | Real device, unsupported-format, permission, hotplug, long-session, and packaged-backend matrix |
| `QT6-INP-01` Input/high DPI | Partial | Real OS/tablet input and fractional mixed-display validation |
| `QT6-PKG-01` Packaging/CI | Blocked | Green Linux, Windows, and supported macOS artifacts from one commit; packaged launch/plugin/runtime-data proof |
| `QT6-TRN-01` Translations/runtime data | Partial | Decide tracked `.qm` policy and prove translation-enabled candidate packages |
| `QT6-QA-01` Product parity | Blocked | Named Qt 5 baseline/corpus, measurable acceptance criteria, manual/studio signoff, and waiver process |
| `QT6-DOC-01` Documentation/evidence | Advanced | Keep documents, dated reports, and the machine projection consistent and current |

Do not convert this table into a percentage. Update the dated audit or add a
new dated report when evidence materially changes.

## Immediate Ordered Outcomes

Work in this order unless new evidence shows a safer dependency order.

### Outcome 1: Integrate and freeze a reviewable candidate

Before expanding the port:

1. refresh live `master` and record the exact merge base;
2. merge or otherwise integrate it using the repository's chosen workflow;
3. explicitly review the overlapping paths listed in the July 13 audit;
4. create a draft PR or document an accepted subsystem-oriented merge plan for
   the 506-file branch delta;
5. record the exact post-integration candidate commit.

Exit criterion: integration decisions and conflicts are resolved, the work is
reviewable, and subsequent evidence refers to the new exact commit.

### Outcome 2: Decide and enforce the support contract

Record one authoritative matrix containing:

- minimum supported Qt 6 version;
- primary release Qt 6 version;
- forward-compatibility/latest Qt 6 lane;
- supported operating systems, architectures, compilers, and SDKs;
- whether macOS x64 remains required;
- exact Qt 5 parity baseline by commit, Qt version, platform, and package;
- named scene, script, media, and input corpus revisions.

The adopted contract is Qt 6.9.0 as the API floor (tested with 6.9.3), Qt
6.10.3 for release packages, and the Nixpkgs Qt 6.11.x lane for forward
compatibility. The strict build disables APIs deprecated through 6.10.
Supported release artifacts are Linux x86_64, Windows x86_64, and macOS arm64;
macOS x86_64 is best-effort source compatibility rather than a release gate.
Use the same-candidate Qt 5 build as the primary baseline and
`qt6-parity-corpus-v1` as the named comparison corpus.

Exit criterion: every declared floor/release/latest lane has a command or CI
job, and source does not require an API newer than the declared floor.

### Outcome 3: Restore same-commit build evidence

On the integrated candidate:

1. run the full preflight;
2. configure and build Qt 5;
3. configure and build Qt 6;
4. run the strict Qt 6 deprecation build aligned with the chosen policy;
5. run the translation-enabled build if translations remain a release
   requirement;
6. retain logs with commit, Qt version, compiler, OS, and result.

Exit criterion: the required lanes build from the same candidate commit. A
configure-only result is not a build result.

### Outcome 4: Close the shader and Script Console functional gates

Shader-FX gate:

- rerun the bundled shader-FX probe after `82a9a8e58` or its integrated
  descendant;
- record context version/profile, vendor, renderer, driver, Qt version, pixel
  format/type, fixture, and output artifact;
- compare against the same Qt 5 fixture;
- require expected nonzero/reference pixels;
- preserve worker-safe compile/link failure handling and add controlled
  context-creation/`makeCurrent()` failure handling;
- add the test to an aggregate only after it proves pixel correctness.

Script Console gate:

- design QJSEngine ownership and asynchronous evaluation explicitly;
- prove that a long-running script does not freeze the console UI;
- prove bounded Ctrl-Y interruption or the accepted replacement interaction;
- run the end-to-end user workflow and both aggregate script suites;
- document the supported Qt Script binding surface and all accepted
  incompatibilities.

Exit criterion: both gates have reproducible, current-commit user-observable
evidence. Safe execution and fixture quantity alone are insufficient.

### Outcome 5: Close CI, packaging, and product-parity gates

- fix Qt 6 Linux and Windows package jobs in the default-branch workflow
  execution model;
- run current-tip Qt 6 validation on integration candidates, not only a weekly
  schedule;
- run preflight and the strict deprecation lane in CI where appropriate;
- test packaged launch, platform plugins, multimedia plugins, runtime `stuff`,
  translations, relocation, signing/notarization, and clean-machine behavior;
- execute aggregate script and GUI smokes from the candidate packages;
- run production scenes, real input/tablet, fractional/mixed DPI, audio,
  camera, and stop-motion checks on the supported matrix;
- publish and verify the release/tag/artifacts rather than assuming a
  conditional publication step ran.

Exit criterion: all P0/P1 requirements in the verification guide have
same-commit evidence or an approved, owned, time-bounded waiver.

## Non-Negotiable Constraints

- Keep the default Qt 5 lane working until its retirement is an explicit
  maintainer decision.
- Keep Nix as the dependency owner and mise as the task runner for the
  reproducible local path.
- Do not perform a global one-shot Qt replacement.
- Do not remove Qt Script behavior without a compatibility matrix, fixtures,
  and an accepted migration decision.
- Do not disable a user-visible feature to obtain a green build unless the
  limitation is explicitly accepted, owned, and documented.
- Do not edit vendored third-party code or binary assets unless a specific,
  demonstrated blocker requires it.
- Do not call a path complete from static grep, configure, launch, or crash-free
  execution alone when correctness is visual, temporal, audio, input, or
  hardware-dependent.
- Keep compatibility bridges narrow and guarded, but do not remove them before
  their replacement behavior and retirement prerequisites are proved.
- Keep implementation changes bounded to an identified requirement and attach
  the smallest relevant proof.
- Never overwrite historical dated reports to make them appear current.

## Evidence Contract

Every material claim must record:

- stable requirement ID;
- exact source commit and dirty/clean state;
- date and reviewer or automation identity;
- OS version, architecture, compiler/SDK, exact Qt version;
- build preset/task and relevant options;
- fixture/corpus revision and expected result;
- observed result, including pixel/audio/input metrics where applicable;
- retained log, image, package, checksum, or CI link;
- what the evidence proves and what it does not prove;
- blocker, next action, owner, and waiver if applicable.

Freshness rules:

- changes to a path invalidate its affected runtime evidence;
- dependency/build changes invalidate build and package evidence;
- packaging changes invalidate packaged-runtime evidence;
- release artifacts must be produced from the reviewed candidate commit;
- Qt 5 CI is Qt 5 compatibility evidence, not Qt 6 evidence;
- synthetic Qt events do not prove OS or tablet-driver delivery;
- an asset-reference check does not prove shader compile, execution, or pixels;
- a macOS arm64 result does not prove macOS x64, Windows, or Linux.

## Validation By Change Type

Select validation based on the requirement touched. Do not run a giant suite
without explaining what it proves, and do not omit a high-risk proof because a
smaller check is green.

### Baseline environment and static preflight

```sh
mise run doctor
mise run doctor-qt6
bash scripts/qt6/check-migration-preflight.sh
```

### Build lanes

```sh
mise run configure
mise run build
mise run configure-qt6
mise run build-qt6
mise run build-qt6-deprecated-api
```

Run translation, package, signing, and clean-machine tasks required by the
chosen platform matrix. Retain the exact commands in the evidence record.

### Aggregate runtime suites

```sh
mise run gui-smokes-app-qt6
mise run script-smoke-user-workflow-qt6
mise run script-smokes-qt6
mise run script-smokes-natural-exit-qt6
```

The GUI aggregate intentionally does not prove real OS input or hardware. Add
focused tasks for the touched viewer, render, preview, FX, audio, camera,
input, or script surface and record their exact names in the dated report.

### Mechanical guardrail commands

The preflight is authoritative, but keep its task mapping documented so task
and guidance drift is visible:

```sh
mise run check-windows-msvc-abi
mise run check-qt6-version-policy
mise run check-no-qregexp
mise run check-core5compat-scope
mise run check-qt6-multimedia-scope
mise run check-qt6-script-scope
mise run check-qt6-script-smoke-registry
mise run check-qt6-gui-smoke-registry
mise run check-qt6-fontmetrics-scope
mise run check-qt6-qtextstream-scope
mise run check-qt6-qiodevice-open-scope
mise run check-qt6-qtime-elapsed-scope
mise run check-qt6-guardrail-docs
mise run check-qt6-fontdatabase-scope
mise run check-qt6-highdpi-attribute-scope
mise run check-qt6-touch-scope
mise run check-qt6-tabletevent-scope
mise run check-qt6-mouseevent-scope
mise run check-qt6-enterevent-scope
mise run check-qt6-helpevent-scope
mise run check-qt6-qvariant-scope
mise run check-qt6-qkeysequence-scope
mise run check-qt6-mediaplayer-scope
mise run check-qt6-desktopwidget-scope
mise run check-qt6-combobox-activated-scope
mise run check-qt6-checkbox-state-scope
mise run check-qt6-buttongroup-scope
mise run check-qt6-wheelevent-scope
mise run check-qt6-graphicssceneevent-scope
mise run check-qt6-graphicsscenecontextmenu-scope
mise run check-qt6-hoverevent-scope
mise run check-qt6-localfileurl-scope
mise run check-qt6-qaction-scope
mise run check-qt6-qcolor-scope
mise run check-qt6-qimage-mirrored-scope
mise run check-qt6-qglformat-scope
mise run check-qt6-qgllegacy-scope
mise run check-qt6-shader-assets
mise run check-qt6-appkit-event-scope
mise run check-qt6-modern-qt-macros
```

### Mandatory manual classes before release

- representative scene creation/open/save/reload and project relocation;
- raster, vector, Toonz Raster, selection, onion skin, guides, overlays,
  schematic, preview, FX Preview, final render, and script render;
- bundled and user shader FX with expected pixels and failure behavior;
- fractional 1.25/1.5/1.75 and 2.0 scaling, mixed displays, display movement,
  and off-primary placement;
- mouse, keyboard, touch where supported, and real tablet pressure/tilt;
- audio playback/recording, unsupported device formats, permissions, hotplug,
  pause/resume, and long sessions;
- camera preview/capture and stop-motion on supported devices;
- clean-machine launch from each candidate package, plugin discovery, runtime
  data, translations, file associations, relocation, signing, and
  notarization.

Use the record template in
`doc/qt6_remaining_work_and_manual_verification.md`.

## Durable Feedback Loop

Use repeated evidence to improve the correct repository layer:

- `AGENTS.md` for always-on repository conventions;
- this prompt for sequencing, constraints, validation classes, and reporting;
- `doc/qt6_remaining_work_and_manual_verification.md` for requirement rows,
  acceptance criteria, manual procedures, and release signoff;
- `doc/qt6_migration_study.md` for stable architecture and primary research;
- `mise.toml` and `scripts/qt6/` for deterministic mechanical checks;
- `.github/workflows/` after local behavior is stable and CI is the right
  durable layer;
- dated progress reports for evidence and chronology.

Do not turn a one-off observation, transient environment failure, or preference
into permanent guidance. When the same ambiguity or failure recurs, record the
evidence, choose the smallest durable layer, add an objective guard or clearer
contract, validate the update itself, and include a short retrospective.

Mechanical guards may enforce forbidden APIs, registry drift, path hygiene,
version-policy shape, and artifact structure. They must not encode subjective
visual or release judgment.

## Reporting Requirements

End each implementation turn with:

1. exact starting and ending commits and integration state;
2. requirement IDs addressed and files changed;
3. validation actually run, with platform and exact Qt version;
4. retained evidence links or paths;
5. what passed, what failed, and what was not attempted;
6. whether older evidence was invalidated;
7. remaining blockers in priority order;
8. hardware or environment limitations separated from product failures;
9. durable guidance considered or changed and the evidence justifying it;
10. the single most valuable next outcome.

Do not say “Qt 6 CI is green” when only regular Qt 5 workflows passed. Do not
say a package or rolling prerelease exists until the artifact/release is
visible. Do not say shader rendering passes from crash-free all-black output.

## Completion Contract

The migration is complete only when:

- the supported platform/compiler/architecture and Qt floor/release/latest
  matrix is explicit and green;
- live `master` is integrated and the final branch is reviewable;
- required Qt 5, Qt 6, strict-deprecation, translation, and package evidence is
  from the same candidate commit;
- every P0/P1 requirement is accepted with current evidence or an approved,
  owned, time-bounded waiver;
- Script Console responsiveness/interruption and supported bindings are proved;
- bundled shader FX produces expected reference pixels;
- the named Qt 5/Qt 6 workflow corpus meets its acceptance thresholds;
- real input/DPI/multimedia/camera/stop-motion tests are signed off on the
  supported matrix;
- clean-machine packages, plugins, runtime data, translations, signing, and
  publication are verified;
- remaining known issues have severity, owner, scope, and release disposition.

Only after that should the project consider retiring Qt 5 compatibility bridges
or resuming the separate Metal migration.
