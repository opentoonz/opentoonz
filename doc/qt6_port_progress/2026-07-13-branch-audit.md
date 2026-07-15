# Qt 6 Migration Branch Audit — 2026-07-13

Audited branch: `codex/qt-6-port`

Audited commit: `82a9a8e58f88aabdd3c6fad32d4adcf86cac48c0`

Live comparison base: `bgyss/opentoonz` `master` at
`9b10b9aea0e2309a3597e4f2270da3ad88dc0b5b`

Audit type: repository, documentation, primary-source research, and public CI
inspection with local environment/configure checks. No tracked repository file
outside documentation was changed; configure checks generated only ignored
local build state.

## Executive Verdict

The Qt 6 migration is substantial and credible, but it is **not yet
integration-ready or release-ready**.

The branch has a real dual Qt 5/Qt 6 build abstraction, a Qt 6 Nix/preset/mise
lane, broad Qt API cleanup, a large QJSEngine compatibility layer, modernized
multimedia paths, focused script and GUI smoke infrastructure, experimental
cross-platform packaging, and recent work on the shader-FX path. The remaining
work is no longer mainly bulk API replacement. It is concentrated in a small
number of high-risk contracts and proofs:

1. the branch is 111 commits ahead of and 12 commits behind live `master`, with
   overlapping source paths and no pull request or merge plan;
2. the declared Qt 6.5 minimum is contradicted by unconditional use of an API
   introduced in Qt 6.9;
3. Qt 6 Script Console evaluation is synchronous on the caller/UI thread, so a
   long-running script can prevent the UI-delivered interrupt command from
   running;
4. the latest shader-FX probe reached safe execution but returned an all-black
   image, and the subsequent packed-pixel fix has not been rerun successfully;
5. the only cross-platform Qt 6 package workflow has failed overall on six
   consecutive scheduled runs, with Linux and Windows still failing in the
   latest inspected run;
6. no current-tip Qt 6 build, deprecation build, aggregate smoke, or package
   result exists in CI, and no complete current-tip local build was performed
   during this audit;
7. real-hardware, production-scene, packaged-runtime, mixed-DPI, input, and
   studio parity evidence remains incomplete.

A numerical completion percentage would create false precision. The branch is
best described as **implementation-advanced, proof-incomplete, and carrying
significant integration debt**.

## Evidence Rules Used In This Audit

The audit distinguishes four evidence levels:

- **Verified now**: commands run in this checkout on the audited commit during
  this audit.
- **Verified in public CI**: result and commit are visible in a linked GitHub
  Actions run.
- **Historical repository evidence**: a dated repository document records a
  result, but the artifact was not reproduced in this audit.
- **Static finding or inference**: source, configuration, or documentation
  demonstrates a risk; a focused runtime test may still be required to prove
  user-visible impact.

Evidence freshness is part of the requirement, not optional metadata:

- a source change invalidates runtime evidence for the changed path;
- a build-system or dependency change invalidates build and package evidence;
- a packaging change invalidates packaged-launch and plugin evidence;
- release evidence must be produced from the exact candidate commit;
- ordinary Qt 5 CI does not count as Qt 6 validation;
- safe execution does not count as correct pixels, audio, input, or workflow
  behavior.

## Evidence Collected During This Audit

### Verified now

| Check | Result | What it proves | What it does not prove |
|---|---|---|---|
| `git status --short --branch` | Clean tracked worktree at audit start; branch tip matched `origin/codex/qt-6-port` | The audit began from a known branch tip | Live `master` integration or build correctness |
| `mise run doctor` | Passed; Qt 5.15.18 detected | The local Qt 5 Nix environment resolves | The application builds or runs |
| `mise run doctor-qt6` | Passed; Qt 6.11.0 detected | The local Qt 6 Nix environment resolves | Compatibility with the declared Qt 6.5 floor |
| `bash scripts/qt6/check-migration-preflight.sh` | Passed, including the 9-interface/16-program-reference shader asset check | Current mechanical source and registry guards are clean | Compilation, shader execution, product parity, or package behavior |
| `mise run configure` | Passed with Qt 5.15.18 | The default Qt 5 CMake lane configures | Link or runtime success |
| `mise run configure-qt6` | Passed with Qt 6.11.0 | The Qt 6 CMake lane configures on the current local toolchain | Link, runtime, minimum-version, deprecation, or package success |

No Qt 5 or Qt 6 compile was run during this documentation-only audit. The
Qt 6 configure created `toonz/build/nix-qt6-relwithdebinfo`, but no app binary
or smoke result was produced there. Historical build claims below remain
historical unless backed by a linked CI run.

### Branch and integration state

The local `origin/master` reference was stale. Live GitHub state was inspected
instead:

- audited tip: [`82a9a8e58`](https://github.com/bgyss/opentoonz/commit/82a9a8e58f88aabdd3c6fad32d4adcf86cac48c0);
- live `master`: [`9b10b9aea`](https://github.com/bgyss/opentoonz/commit/9b10b9aea0e2309a3597e4f2270da3ad88dc0b5b);
- [live comparison](https://github.com/bgyss/opentoonz/compare/master...codex%2Fqt-6-port):
  111 commits ahead, 12 commits behind, merge base `6fb126817`;
- branch delta from that merge base: 506 files, approximately 41,794
  insertions and 2,841 deletions;
- no pull request exists in the fork or upstream repository.

The 12 live-master commits touch 52 files. Eight paths also changed on this
branch and therefore deserve explicit merge review:

- `toonz/sources/tnztools/CMakeLists.txt`
- `toonz/sources/toonz/commandbarpopup.cpp`
- `toonz/sources/toonz/levelcreatepopup.cpp`
- `toonz/sources/toonz/mainwindow.cpp`
- `toonz/sources/toonz/menubar.cpp`
- `toonz/sources/toonz/shortcutpopup.cpp`
- `toonz/sources/toonz/tapp.cpp`
- `toonz/sources/toonzlib/preferences.cpp`

The branch has outgrown the goal prompt's original “small, incremental slice”
model. Further implementation should pause until live `master` is integrated
and the work has a reviewable PR or subsystem-oriented merge plan.

## Progress By Workstream

| ID | Workstream | State | Demonstrable progress | Current gate |
|---|---|---|---|---|
| `QT6-INT-01` | Branch integration | Blocked | A coherent migration branch exists and matches its remote tip | Merge live `master`, resolve/review eight overlapping paths, and define a review plan for 506 changed files |
| `QT6-BLD-01` | Dual build lane | Advanced | Qt-major selection, versioned target aliases, Nix shells, presets, and mise tasks exist; both lanes configure now | Fresh Qt 5, Qt 6, and strict-deprecation builds from one commit on the declared matrix |
| `QT6-VER-01` | Qt version contract | Failing | CI uses Qt 6.9.3; local Nix resolves 6.11.0 | Reconcile the declared 6.5 floor, 6.9.3 CI, 6.9 deprecation gate, and 6.11 local lane |
| `QT6-API-01` | Removed/deprecated APIs | Advanced | `QRegExp` is absent; `QTextCodec`, QGL, Qt Script, and legacy multimedia residue is narrowly guarded or Qt 5-only | Test the declared minimum and align the deprecation gate with the chosen support policy |
| `QT6-SCR-01` | Script API compatibility | Advanced but blocked | QJSEngine facade, roughly 70 fixtures, and broad aggregate script smokes exist | Prove responsive console evaluation and cancellation; define unsupported bindings; rerun aggregate and end-to-end workflow tests |
| `QT6-REN-01` | Viewer/rendering/shader FX | Advanced but blocked | Viewer, preview, render, OpenGL, and shader safety work exists; recent commits fixed context/feedback/upload paths | Current-tip bundled shader output must contain expected nonzero pixels; broaden Qt 5/Qt 6 scene comparisons |
| `QT6-MED-01` | Audio/camera/stop-motion | Partial | Qt 6 multimedia types and focused backend probes exist | Validate real devices, permissions, hotplug, format conversion, long sessions, and packaged backends |
| `QT6-INP-01` | Mouse/tablet/high DPI | Partial | Many event API bridges and synthetic GUI smokes exist | Real OS input, tablets, fractional DPI, and mixed-display behavior on supported platforms |
| `QT6-PKG-01` | Packaging/deployment | Blocked | macOS arm64 Qt 6 DMG job succeeded on July 13; Linux/Windows jobs and workflow structure exist | Green same-commit packages, packaged launch/plugin/runtime-data smokes, signing policy, and actual release publication |
| `QT6-TRN-01` | Translations/resources | Partial | Translation lane and 63 generated `.qm` files exist | Decide whether the 3.9 MiB generated binaries belong in the branch and validate `WITH_TRANSLATION=ON` packages |
| `QT6-QA-01` | Product parity/release proof | Blocked | Large automated smoke surface and manual checklists exist | Named Qt 5 baseline/corpus, supported-platform matrix, exact evidence ledger, manual/studio signoff, and waiver policy |
| `QT6-DOC-01` | Migration contract | Revised by this audit | Detailed research and historical evidence exist | Keep status in dated reports, stable architecture in the study, and measurable requirements in the verification guide |

## Highest-Priority Findings

### P0 — Resolve before further expansion or release claims

#### `QT6-INT-01`: integrate live `master` and make the branch reviewable

The branch is both large and behind. Continuing to add source surface before
integrating the 12 master-side commits increases conflict and invalidation risk.
The next implementation cycle should start with the merge/rebase decision,
review the eight overlapping paths, and run the full local baseline before any
new migration feature work. A draft PR or explicit subsystem slicing plan is
needed so the 506-file delta can be reviewed.

Acceptance evidence:

- exact merge base and resulting candidate commit;
- review notes for each overlapping path;
- clean preflight;
- Qt 5, Qt 6, and strict Qt 6 deprecation builds from the integrated commit;
- retained logs or CI links.

#### `QT6-VER-01`: the declared Qt 6.5 minimum is not currently credible

`toonz/sources/CMakeLists.txt` accepts Qt 6.5.0. However,
`toonz/sources/include/toonzqt/qtcompat.h` calls `QImage::flipped()` for every
Qt 6 build, and Qt documents that function as introduced in Qt 6.9. The
experimental workflow pins 6.9.3 and the current Nix lane resolves 6.11.0, so
neither detects the floor violation.

This is a contract decision, not merely a compiler cleanup:

- either raise the supported Qt minimum to 6.9 and update every document,
  preset, package lane, and test expectation; or
- retain 6.5 as the floor, guard 6.9-only APIs, and add a real oldest-supported
  build lane.

The strict build currently sets `QT_DISABLE_DEPRECATED_UP_TO=0x060900` while
the local toolchain is Qt 6.11.0. A clean strict build therefore proves only
that APIs deprecated through 6.9 are absent. It does not prove clean use of the
actual 6.11 toolchain. The project needs three explicit concepts: minimum
supported Qt, primary release Qt, and forward-compatibility/latest Qt.

Primary sources:

- [QImage::flipped()](https://doc.qt.io/qt-6/qimage.html#flipped)
- [Qt release and support table](https://doc.qt.io/qt-6/qt-releases.html)
- [Qt deprecation macros](https://doc.qt.io/qt-6/qtdeprecationmarkers.html)
- [Qt 5/6 CMake compatibility](https://doc.qt.io/archives/qt-6.9/cmake-qt5-and-qt6-compatibility.html)

#### `QT6-SCR-01`: Script Console responsiveness and cancellation are unproved and likely regressed

The Qt 5 `ScriptEngine::evaluate()` starts an `Executor` thread. A parallel
Qt 6 `Executor` class exists but the Qt 6 `evaluate()` implementation bypasses
it and calls `QJSEngine::evaluate()` synchronously. `ScriptConsole` invokes
that method directly when the user submits a command. Its Ctrl-Y interrupt is
delivered through the same widget's event handler, which cannot run while the
UI thread is blocked by a long or infinite evaluation.

This is a static control-flow finding with a strong user-visible implication.
It still needs a bounded interactive acceptance test, but fixture count does
not cover it. Simply reinstating the old worker call is not an adequate design
answer because `QJSEngine` and the exposed QObject facade have thread-affinity
constraints.

Acceptance evidence:

- an explicitly designed asynchronous engine/worker ownership model;
- a bounded long-running/infinite-loop test that proves the console remains
  responsive and Ctrl-Y interruption completes;
- `mise run script-smoke-user-workflow-qt6`;
- `mise run script-smokes-qt6` and
  `mise run script-smokes-natural-exit-qt6`;
- a documented matrix of the supported Qt Script public binding surface and
  any intentionally unsupported behavior.

Primary sources:

- [QJSEngine](https://doc.qt.io/qt-6/qjsengine.html)
- [Making applications scriptable](https://doc.qt.io/qt-6/qtjavascript.html)
- [Qt 6 removed modules](https://doc.qt.io/qt-6/whatsnew60.html#removed-modules-in-qt-6-0)

#### `QT6-REN-01`: shader safe execution is not pixel correctness

The July 12 historical probe reached `ShaderFx::doCompute` without crashing
but produced a valid all-black 320×240 result for bundled
`SHADER_HSLBlendGPU`. Commit `82a9a8e58` changes packed texture upload types,
which is a plausible fix for macOS `MRGB` channel/type handling, but no
successful current-tip rerun exists. The branch must not promote this path into
the aggregate suite or call it complete until the expected nonzero pixels are
observed.

Additional static risks should be included in the next rendering audit:

- shader context `create()`/`makeCurrent()` failures are not consistently
  converted into actionable failures;
- transfer paths mix Qt OpenGL dispatch, legacy immediate mode, and GLEW;
- upload/readback channel and packed-type assumptions should be reviewed
  systematically, not only at the already-fixed call;
- context format/profile, vendor, renderer, driver, and device-pixel ratio
  should be attached to retained evidence.

Acceptance evidence:

- the exact audited commit and Qt/OpenGL/GPU environment;
- a bundled shader output with expected nonzero/reference pixels;
- explicit compile/link/context failure coverage;
- comparison against the same Qt 5 fixture;
- only then, a registered aggregate smoke.

Primary sources:

- [QOpenGLContext](https://doc.qt.io/qt-6/qopenglcontext.html)
- [QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html)

#### `QT6-PKG-01`: cross-platform Qt 6 package validation is red

The regular Linux, macOS, and Windows workflows are Qt 5 compatibility lanes,
not Qt 6 validation. The only cross-platform Qt 6 workflow is
`Qt 6 Experimental Binary Builds`, triggered by schedule, manual dispatch, or
tag rather than branch pushes and pull requests.

Six consecutive scheduled runs from June 8 through July 13 failed overall.
The [July 13 run](https://github.com/bgyss/opentoonz/actions/runs/29245050730)
checked out `ee69d8b72`, one commit behind the audited tip:

| Platform | Result | Evidence and blocker |
|---|---|---|
| macOS arm64 | Passed build/package/bundle/DMG | [job 86799823600](https://github.com/bgyss/opentoonz/actions/runs/29245050730/job/86799823600); [artifact 8277659738](https://github.com/bgyss/opentoonz/actions/runs/29245050730/artifacts/8277659738), expires 2026-10-11 |
| Windows | Failed during CMake configure, before generation/build | [job 86799823607](https://github.com/bgyss/opentoonz/actions/runs/29245050730/job/86799823607); scheduled workflow definition came from `master`, selected Windows 2025/VS 2026, while CMake requested the VS 2022 generator |
| Linux | Failed compile | [job 86799823618](https://github.com/bgyss/opentoonz/actions/runs/29245050730/job/86799823618); `iwa_bokehreffx.cpp` uses `UCHAR_MAX` without `<climits>` |

Because scheduled workflow definitions come from the default branch while jobs
checkout the migration branch, branch-only runner corrections do not fix the
scheduled job definition. No rolling `qt-6-experimental` release/tag currently
exists; documentation must describe publication as conditional on all jobs
succeeding.

No GitHub workflow currently runs the strict Qt 6 deprecation build, the Qt 6
script-smoke aggregate, or the Qt 6 GUI-smoke aggregate. Linux and Windows also
need packaged launch/plugin/runtime-data checks. The macOS checklist asks for
x64 while the workflow only builds arm64; the project must either add x64 or
remove it from the support contract.

Primary sources:

- [Qt deployment overview](https://doc.qt.io/qt-6/deployment.html)
- [Linux deployment](https://doc.qt.io/qt-6/linux-deployment.html)
- [Windows deployment](https://doc.qt.io/qt-6/windows-deployment.html)
- [macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html)
- [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html)

### Additional release and maintenance findings

#### P1 — `QT6-MED-01`: multimedia needs behavior and conversion proof

The Qt 6 playback path selects a device's preferred format when the requested
format is unsupported, then copies the original raw bytes unchanged. If sample
rate, channel count, or sample representation differs, the bytes can be
interpreted using the wrong format. This is a static risk requiring a focused
test and likely implementation work in a later code turn.

The release matrix must cover default and non-default devices, unsupported
formats, permissions, hotplug, pause/resume, long sessions, live camera
formats, stop-motion capture, and packaged multimedia plugins on every
supported platform.

Primary source: [Qt Multimedia changes in Qt 6](https://doc.qt.io/qt-6/qtmultimedia-changes-qt6.html).

#### P1 — `QT6-INP-01`: synthetic smokes do not establish real input or DPI parity

The branch has broad in-process GUI event coverage, but OS event delivery,
tablet drivers, pressure/tilt, multi-monitor placement, and fractional scaling
remain manual obligations. Required DPI cases should include 1.0, 1.25, 1.5,
1.75, 2.0, and movement between displays with different scale factors. OpenGL
framebuffer dimensions and low-level coordinates must account for device pixel
ratio.

Primary sources:

- [Qt high-DPI model](https://doc.qt.io/qt-6/highdpi.html)
- [Qt Test overview](https://doc.qt.io/qt-6/qtest-overview.html)

#### P0 — `QT6-QA-01`: the baseline, support matrix, and evidence corpus are undefined

`.github/qt6-validation/manual-goals.json` calls the baseline “OpenToonz 1.7.1
/ Qt 5,” while current source declares OpenToonz 1.8.0. Qt 5 toolchains also
differ by platform: local macOS Nix uses 5.15.18, Windows uses a custom
5.15.2/WinTab build, and Linux uses distribution packages. “Matches Qt 5” is
not reproducible without an exact commit, toolchain, package, and scene corpus.

Choose and record:

- supported operating systems, architectures, compilers, and Qt versions;
- same-commit Qt 5 parity versus released 1.7.1 behavior, or both;
- named representative scene/script/media fixtures with checksums or revision;
- automated thresholds and manual reviewer/signoff rules;
- a waiver format with owner, rationale, scope, and expiry.

### Maintenance and integration-debt findings

#### P2 — Test machinery should have a test-only boundary

`toonz/sources/toonz/main.cpp` has grown by roughly 15,000 lines on the branch,
largely for app-side GUI smoke actions. Runtime environment gating is valuable,
but test machinery compiled into a production source file is not a narrow
test-only boundary. A later implementation plan should extract this into a
test target, test-only compilation unit, or explicit build option while
preserving coverage and platform access.

#### P1 — `QT6-TRN-01`: generated translation binaries need an explicit policy

The branch adds 63 `.qm` files under `toonz/stuff/config/loc`, approximately
3.9 MiB. They may be intentional translation-lane evidence, but generated
binary assets should not enter the final integration by accident. Record
whether they are release inputs, reproducible outputs, or CI artifacts, then
test the chosen `WITH_TRANSLATION=ON` package path.

#### P2 — Compatibility bridges need exit criteria, not immediate removal

`QTextCodec` is confined to the adapter and `Core5Compat` is narrowly scoped.
Legacy Qt Script bindings and the old video-surface path are Qt 5-only. These
are controlled transitional boundaries, not urgent active Qt 6 blockers.
Remove them only after encoding round-trip fixtures, a Qt 5 retirement decision,
and equivalent Qt 6 workflow coverage exist.

## Recent Progress Since The Last Dated Report

The latest previous progress reports stop at July 6. Seven first-parent commits
landed from July 10 through July 13:

| Commit | Date | Change |
|---|---|---|
| `22edd9e5a` | 2026-07-10 | Strengthen the Qt 6 deprecated-API gate |
| `c98ec408a` | 2026-07-10 | Preserve shader-FX offscreen surface format |
| `c92a822c5` | 2026-07-12 | Preserve shader context format on recreation |
| `2db78f59e` | 2026-07-12 | Guard bundled shader assets in Qt 6 preflight |
| `e1db185ae` | 2026-07-12 | Document the Qt 6 shader runtime failure |
| `ee69d8b72` | 2026-07-12 | Use Qt OpenGL functions for shader feedback |
| `82a9a8e58` | 2026-07-13 | Fix packed-pixel uploads in shader textures |

This is meaningful progress. It moved the shader path from a crash/unsafe
state toward a plausible pixel fix and added a mechanical asset guard. It did
not yet close pixel correctness because the final fix has no successful
runtime result.

## CI Claims That Must Stay Separate

As observed at 2026-07-13 22:55 UTC:

- [current-tip Linux regular CI](https://github.com/bgyss/opentoonz/actions/runs/29290354304)
  passed, but it is a Qt 5 build;
- current-tip [macOS](https://github.com/bgyss/opentoonz/actions/runs/29290354277)
  and [Windows](https://github.com/bgyss/opentoonz/actions/runs/29290354262)
  regular workflows were still running, and are also Qt 5 lanes;
- the last all-green regular three-platform set at `ee69d8b72` is Qt 5
  compatibility evidence only;
- the latest inspected Qt 6 experimental run was one commit behind and failed
  overall;
- [Project Sync run 29290572468](https://github.com/bgyss/opentoonz/actions/runs/29290572468)
  appeared green in the UI but logged an empty `QT6_PROJECT_TOKEN` and exited
  without syncing. It is not project-state evidence.

## Ordered Critical Path

The next implementation work should be ordered as follows:

1. **Integrate and freeze a candidate.** Merge live `master`, review the eight
   overlapping paths, create a draft PR or slicing plan, and record the exact
   candidate commit.
2. **Decide the support contract.** Choose minimum/release/latest Qt versions,
   supported OS/architecture/compiler lanes, macOS x64 disposition, and exact
   Qt 5 parity baseline.
3. **Restore compile evidence.** From the same commit run preflight, Qt 5,
   Qt 6, strict Qt 6 deprecation, and translation builds. Retain logs.
4. **Close the two highest-risk functional gates.** Prove nonblack expected
   shader pixels and responsive/cancellable Script Console evaluation.
5. **Close CI/package blockers.** Fix Linux and Windows in the default-branch
   workflow context, add current-tip Qt 6 validation, and test clean-machine
   packaged launch, plugins, runtime data, translations, signing, and
   publication.
6. **Run aggregate and manual parity.** Execute script/GUI aggregates, named
   production scene corpus, real input, mixed-DPI, audio/camera/stop-motion,
   and package tests on the supported matrix.
7. **Release signoff.** Require all P0/P1 requirements accepted or explicitly
   waived, same-commit artifacts, checksums, reviewers, and release notes.

Do not start new compatibility cleanup or expand smoke surface merely because
it is easy. The next work should close these ordered proofs.

## Revised Completion Contract

The Qt 6 port is complete only when all of the following are true:

- the supported platform/compiler/architecture and minimum/release/latest Qt
  matrix is written and every required lane is green;
- live `master` is integrated and the final delta has a reviewable PR or
  accepted merge plan;
- Qt 5, Qt 6, strict-deprecation, translation, and package results come from
  the same release-candidate commit;
- every P0 and P1 requirement has stable evidence or an approved, owned,
  time-bounded waiver;
- the Script Console remains responsive and a bounded interruption test passes;
- bundled shader FX produces expected nonzero/reference pixels on supported
  GPU/platform lanes;
- named Qt 5 and Qt 6 scene/script/media corpora meet their automated and
  manual parity criteria;
- real input, fractional/mixed DPI, audio, camera, and stop-motion tests are
  signed off on the supported hardware matrix;
- clean-machine packages launch with required plugins, runtime data,
  translations, signing/notarization, and checksums verified;
- release publication has actually succeeded; a conditional workflow step is
  not evidence of a published release;
- remaining known issues are documented with severity, owner, scope, and
  release disposition.

## Documentation And Evidence Architecture

To prevent further drift:

- `doc/qt6_migration_study.md` owns stable architecture, risk analysis,
  version policy, and cited primary research;
- `doc/qt6_migration_goal_prompt.md` owns the concise execution contract,
  ordered gates, validation classes, and reporting rules;
- `doc/qt6_remaining_work_and_manual_verification.md` owns requirement IDs,
  acceptance criteria, manual procedures, and the evidence record format;
- `doc/qt6_port_progress/README.md` identifies the latest dated snapshot;
- dated reports are immutable evidence deltas, not copies of the entire goal;
- `.github/qt6-validation/manual-goals.json` should become a generated or
  checked projection of the same stable requirement IDs, not an independent
  source of truth.

The existing documentation guard proves that preflight task names appear in
the canonical documents. It does not detect stale dates, contradictory claims,
missing evidence, or commands placed only in prose. Future guard improvement
should validate requirement/evidence shape and freshness mechanically without
encoding subjective release judgment.

## Research Additions That Accelerate Completion

The most useful next research is not another broad Qt 6 survey. It is focused
decision support for the remaining gates:

1. **Version policy:** use the official [release table](https://doc.qt.io/qt-6/qt-releases.html),
   [porting guide](https://doc.qt.io/qt-6/portingguide.html), and
   [deprecation markers](https://doc.qt.io/qt-6/qtdeprecationmarkers.html) to
   choose and test floor/release/latest lanes.
2. **Static port checks:** add an advisory Clazy evidence pass using Qt's
   [Qt 6 porting checks](https://doc.qt.io/qt-6/porting-to-qt6-using-clazy.html),
   especially deprecated API, missing headers, QHash behavior, forward
   declarations, and missing `Q_OBJECT` checks.
3. **Script ownership/cancellation:** derive an explicit public-binding matrix
   and worker model from [QJSEngine](https://doc.qt.io/qt-6/qjsengine.html),
   then test user-observable responsiveness rather than only fixture output.
4. **Rendering evidence:** capture context/profile/vendor/renderer/driver,
   default framebuffer behavior, device-pixel ratio, upload/readback format,
   and expected pixels using the official OpenGL and high-DPI guidance linked
   above.
5. **Deployment matrix:** turn Qt's platform deployment guidance into explicit
   executable/plugin/runtime-data/signing/clean-machine acceptance rows for
   Linux, Windows, and macOS.
6. **Test boundary:** use Qt Test for deterministic widget/input units where
   appropriate, while retaining separate OS/hardware tests for behavior that
   synthetic events cannot prove.

## Audit Limitations

- This audit did not compile or launch OpenToonz.
- It did not rerun the shader-FX probe or any GUI/script aggregate.
- It did not test cameras, audio devices, tablets, signing, notarization, or
  clean virtual machines.
- Public CI state is a timestamped observation and may change after this report.
- Static risks are not presented as reproduced crashes unless explicitly
  supported by runtime evidence.
- No generated `.qm`, JSON, CMake, workflow, script, fixture, or source file was
  changed as part of this audit.
