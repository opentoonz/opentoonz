# Qt 6 Migration Implementation Recommendations

Prepared: July 14, 2026

Derived from:
`doc/qt6_port_progress/2026-07-13-branch-audit.md`

Applicable source baseline: `82a9a8e58`

Documentation baseline: `a8fba9efa`

## Purpose

This document converts the July 13 branch audit into a recommended series of
bounded implementation work packages. Read it after the audit and before
resuming `doc/qt6_migration_goal_prompt.md`.

It is not a new status report and does not supersede the audit. The audit owns
the evidence snapshot; the goal prompt owns the overall execution contract;
the remaining-work guide owns requirement status and manual verification. This
document answers a narrower question: **in what order should the remaining work
be implemented, what can safely proceed in parallel, and what evidence should
unlock the next step?**

The documentation commit after the audited source baseline changed only
Markdown files. The audit therefore still describes the source tree at the time
this recommendation was prepared. Live `master`, CI, and external toolchains
may have advanced and must be refreshed before implementation begins.

## Executive Recommendation

Resume the migration as a gated stabilization program, not as another broad Qt
API cleanup pass.

The recommended order is:

1. refresh the dated evidence and integrate live `master`;
2. decide the supported Qt/platform/baseline contract;
3. restore same-commit Qt 5, Qt 6, strict-deprecation, and translation build
   evidence;
4. close shader correctness and Script Console cancellation as two parallel P0
   tracks;
5. repair current-candidate Qt 6 CI and cross-platform packaging;
6. close multimedia, real-input, high-DPI, translation, and manual parity gaps;
7. reduce test/compatibility debt only after the product gates are stable;
8. converge all evidence on one release-candidate commit and obtain signoff.

Do not start with `Core5Compat` removal, broad smoke expansion, test-harness
extraction, cosmetic deprecation cleanup, or Metal work. Those are lower-value
than the release-blocking proofs already identified.

## Recommended Work-Package Sequence

| Package | Requirements | Dependency | Can run in parallel? | Exit gate |
|---|---|---|---|---|
| R0. Refresh and freeze the starting state | All | None | No | Exact source, live-master, CI, and dirty-state snapshot recorded |
| R1. Integrate live `master` | `QT6-INT-01` | R0 | No | Integrated candidate, overlap review, clean preflight |
| R2. Adopt the support contract | `QT6-VER-01`, `QT6-QA-01`, `QT6-API-01` | R0; finalize before R3 | Limited policy work may overlap R1 | Minimum/release/latest Qt and supported platform/baseline matrix accepted |
| R3. Restore build evidence | `QT6-BLD-01`, `QT6-TRN-01` | R1 and R2 | Platform builds may fan out from one frozen commit | Required lanes build from the same commit with retained logs |
| R4A. Prove shader correctness | `QT6-REN-01` | R3 | Yes, with R4B | Expected nonzero/reference pixels plus controlled failure evidence |
| R4B. Restore responsive script cancellation | `QT6-SCR-01` | R3 | Yes, with R4A | Responsive UI, bounded interruption, binding matrix, script aggregates |
| R5. Repair Qt 6 CI and packages | `QT6-PKG-01`, `QT6-TRN-01` | R1-R3; absorb R4 when ready | Platform package fixes can fan out | Green same-commit packages and packaged-runtime smokes |
| R6. Complete parity and hardware evidence | `QT6-MED-01`, `QT6-INP-01`, `QT6-QA-01` | Stable R4/R5 candidate | Yes, by platform/hardware class | Named corpus and supported hardware matrix accepted |
| R7. Reduce structural debt | `QT6-API-01`, `QT6-DOC-01` | P0 gates stable | Selected tasks can run independently | Test boundary, bridge exit criteria, and projection consistency improved without invalidating product proof |
| R8. Release convergence | All P0/P1 | R1-R7 | No final convergence in parallel | One candidate commit, complete evidence ledger, approved waivers/signoff |

## R0 — Refresh And Freeze The Starting State

### Recommendation

Start the resumed goal with a read-only revalidation pass. Do not immediately
edit the shader, script engine, CMake, or workflows.

The July 13 audit remains the source baseline, but its live-master and public-CI
observations are timestamped. Refresh only facts that can drift:

- current branch tip and tracked dirty state;
- live `master` tip, merge base, ahead/behind counts, and overlapping paths;
- latest regular Qt 5 workflows;
- latest Qt 6 experimental workflow and retained artifacts;
- current Nix Qt 5 and Qt 6 versions;
- existence of any new pull request, release, tag, or manual evidence.

### Deliverable

Add a short dated delta report under `doc/qt6_port_progress/` only if material
facts changed. If nothing material changed, record the recheck in the first
implementation handoff rather than cloning the full audit.

### Exit gate

- exact starting commit and clean/dirty state recorded;
- live integration target recorded;
- external evidence timestamped;
- no stale CI result is being used as a current gate.

## R1 — Integrate Live `master` Before Expanding The Port

### Recommendation

Integrate live `master` first. Because `codex/qt-6-port` is a published branch
with a long history, prefer a normal merge over rewriting all branch history
unless maintainers explicitly require a rebased series.

Review the eight paths identified by the audit as explicit conflict/behavior
surfaces, even if Git resolves them automatically:

- `toonz/sources/tnztools/CMakeLists.txt`
- `toonz/sources/toonz/commandbarpopup.cpp`
- `toonz/sources/toonz/levelcreatepopup.cpp`
- `toonz/sources/toonz/mainwindow.cpp`
- `toonz/sources/toonz/menubar.cpp`
- `toonz/sources/toonz/shortcutpopup.cpp`
- `toonz/sources/toonz/tapp.cpp`
- `toonz/sources/toonzlib/preferences.cpp`

For each path, record whether the master-side behavior, Qt 6-side behavior, or
an intentional combination was retained. A conflict-free merge is not proof
that behavior was preserved.

Do not combine the integration commit with shader, scripting, multimedia, or
test-architecture changes. The integration boundary should remain reviewable.

### Minimum validation

```sh
bash scripts/qt6/check-migration-preflight.sh
mise run configure
mise run configure-qt6
```

If configuration fails, stop and repair the integration before starting R3 or
either R4 track.

### Exit gate

- live `master` integrated into the migration branch;
- all eight overlap decisions recorded;
- preflight and both configure lanes pass;
- resulting candidate commit recorded;
- draft PR or subsystem-oriented review map exists for the large branch delta.

## R2 — Adopt A Concrete Support Contract

### Recommendation

Resolve version and platform policy before more compatibility code is written.
Otherwise every fix risks targeting an untested or unwanted matrix.

### Recommended default decisions

1. **Qt minimum:** adopt Qt 6.9 as the default minimum unless a documented
   downstream/package requirement demonstrates that Qt 6.5-6.8 support is
   necessary. The source already uses a 6.9 API and experimental CI already
   uses 6.9.3. If maintainers choose to retain 6.5, add the required fallback
   and a real 6.5 build lane before claiming that floor.
2. **Release Qt:** pin one supported Qt minor/patch for release packages and
   use it consistently across platforms unless a platform exception is
   documented. Do not treat the historical 6.9.3 workflow pin as the release
   choice merely because it already exists.
3. **Forward lane:** keep the newest practical Nix Qt lane advisory or strict
   enough to expose newer deprecations. Record both the tested Qt version and
   `QT_DISABLE_DEPRECATED_UP_TO` value; they are different facts.
4. **Qt 5 parity baseline:** use a same-commit Qt 5 build as the primary parity
   baseline because it isolates Qt-major behavior. Use released OpenToonz 1.7.1
   only as a secondary regression reference, not as the sole baseline.
5. **macOS architecture:** explicitly retain or retire x64. Do not keep x64 in
   release criteria while producing only arm64 artifacts.
6. **Comparison corpus:** name representative raster, vector, Toonz Raster,
   shader-FX, script, audio, camera, input, and mixed-DPI fixtures before
   calling later manual checks representative.

### Where to record the decision

Update the authoritative matrix and stable research documents instead of
creating another independently maintained status file:

- version/platform policy in `doc/qt6_migration_study.md`;
- acceptance rows in
  `doc/qt6_remaining_work_and_manual_verification.md`;
- any resulting sequencing change in `doc/qt6_migration_goal_prompt.md`.

### Exit gate

- minimum, release, and forward Qt lanes named;
- operating-system, architecture, compiler, and SDK matrix named;
- Qt 5 comparison baseline and corpus named;
- macOS x64 disposition explicit;
- deprecation threshold aligned with the chosen policy;
- no active source requires an API newer than the declared minimum.

## R3 — Restore Same-Commit Build Evidence

### Recommendation

Build all required source lanes from the integrated, policy-aligned candidate
before diagnosing product behavior. This prevents shader or script work from
being performed on a branch that is already failing at a lower layer.

### Required local sequence

```sh
mise run doctor
mise run doctor-qt6
bash scripts/qt6/check-migration-preflight.sh
mise run configure
mise run build
mise run configure-qt6
mise run build-qt6
mise run build-qt6-deprecated-api
mise run build-qt6-translations
```

Run independent platform builds from the same frozen commit where available.
Retain exact Qt version, compiler/SDK, configuration options, timestamps, and
logs. A successful configure does not satisfy this package.

### Failure handling

- fix integration and ordinary compile failures before functional work;
- keep platform-specific fixes separate when possible;
- do not weaken the preflight or deprecation gate merely to obtain a build;
- if the selected minimum Qt fails, resolve the version contract rather than
  silently building only with a newer Qt.

### Exit gate

- Qt 5 app target builds;
- Qt 6 app target builds;
- strict Qt 6 deprecation target builds at the chosen threshold;
- translation-enabled lane builds or has an approved release-scope waiver;
- evidence comes from one recorded source commit.

## R4 — Run Two Parallel P0 Functional Tracks

### Parallelization rule

R4A and R4B may proceed in parallel after R3 because their primary source
surfaces and acceptance tests are distinct. They should use separate bounded
commits or short-lived worktrees and must rebase/merge from the same R3
candidate. Neither track should edit shared build policy, broad packaging, or
unrelated compatibility code.

Both tracks must close before release convergence. A shader pass does not
offset a scripting failure, and broad script-fixture coverage does not offset
black shader output.

### R4A — Prove Shader-FX Pixel Correctness

#### Recommendation

Rerun the bundled shader probe before writing more shader code. Commit
`82a9a8e58` was intended to correct packed-pixel uploads, but the audit could
not prove its runtime effect.

Use this decision tree:

1. If the probe produces expected nonzero/reference pixels, preserve the
   output artifact, compare the same fixture under Qt 5, and then add a bounded
   registered smoke.
2. If it remains black, capture context creation, `makeCurrent()`, shader
   compile/link, texture upload, transform-feedback, framebuffer, and readback
   results before changing more code.
3. If context creation or `makeCurrent()` fails, make that failure controlled
   and worker-safe before investigating pixel math.
4. Audit remaining raw raster transfers for channel order and packed pixel type
   assumptions instead of applying another isolated upload guess.

Required evidence:

- exact Qt version and source commit;
- GPU, driver, OpenGL vendor/renderer/version/profile;
- device-pixel ratio and surface/framebuffer formats;
- fixture name and expected pixels;
- Qt 5 and Qt 6 output artifacts;
- compile/link/context failure behavior;
- explicit nonzero/reference pixel assertion.

#### Exit gate

- bundled shader FX produces expected reference pixels;
- failure paths return actionable errors without worker-thread UI/assertions;
- Qt 5 comparison remains acceptable;
- the focused smoke is registered only after correctness is demonstrated.

### R4B — Restore Responsive Script Console Cancellation

#### Recommendation

Start with a failing bounded responsiveness test, then design ownership. Do not
simply route the existing Qt 6 engine through the old `Executor`: QJSEngine and
exposed QObject wrappers need an explicit thread-affinity model.

The preferred architecture should have one thread own the QJSEngine for its
entire lifetime. UI/application operations that must execute on the main thread
should cross a queued, bounded request boundary. Interruption must be callable
without requiring the blocked Script Console event handler to run on the engine
thread.

The design note should answer:

- which thread constructs, owns, evaluates, interrupts, and destroys the
  QJSEngine;
- how QObject wrappers are exposed safely;
- how synchronous-looking script APIs marshal main-thread work;
- how shutdown, exceptions, cancellation, and repeated evaluations behave;
- which legacy Qt Script behaviors are intentionally unsupported.

Required validation:

```sh
mise run script-smoke-user-workflow-qt6
mise run script-smokes-qt6
mise run script-smokes-natural-exit-qt6
```

Add a bounded long-running or infinite-loop fixture that proves the console UI
remains responsive and the accepted interrupt interaction completes. Preserve
the existing broad fixture suite and publish a binding-surface matrix rather
than relying on a generic “partial” warning.

#### Exit gate

- Script Console remains responsive during long evaluation;
- cancellation completes within a documented bound;
- engine/wrapper ownership is explicit and cleanly shut down;
- all aggregate and end-to-end script smokes pass;
- supported and unsupported binding behavior is documented.

## R5 — Repair Current-Candidate CI And Packaging

### Recommendation

Treat CI as a consumer of the established local contract, not as the place to
discover basic branch policy.

Implement the package work in three layers:

1. **Fast integration validation:** preflight, Qt 5 compatibility, Qt 6 build,
   and strict-deprecation build on integration candidates.
2. **Platform package construction:** Linux, Windows, and supported macOS
   architectures build from the same source commit.
3. **Packaged-runtime verification:** launch the produced artifact on a clean
   environment and verify platform plugins, multimedia plugins, runtime
   `stuff`, translations, relocation, and basic script/GUI smokes.

Address the known blockers in bounded changes:

- integrate the default-branch workflow definition needed to select the
  supported Windows runner/toolchain before relying on scheduled validation;
- fix the Linux `UCHAR_MAX` header dependency and retain a native Linux build
  result;
- decide whether macOS x64 is required before adding or removing that lane;
- run the strict-deprecation and aggregate script/GUI suites in CI at a
  deliberate cost-appropriate cadence;
- make release publication evidence explicit: tag, release, artifact names,
  checksums, and source commit must be visible after the job.

Do not report regular platform workflows as Qt 6 evidence while they still
build the Qt 5 lane.

### Exit gate

- required Linux, Windows, and macOS packages succeed from one commit;
- clean packaged launch succeeds;
- required plugins and runtime data load from the package;
- bounded packaged GUI/script smokes pass;
- signing/notarization policy is applied where required;
- rolling or candidate release publication actually exists and identifies its
  source commit.

## R6 — Complete Product-Parity And Hardware Evidence

### Recommendation

Once P0 runtime paths and packages are stable, fan out P1 verification by
platform and hardware class.

### Multimedia

Before broad manual testing, add a focused test for the identified playback
format risk: when a device rejects the requested format and selects a preferred
format, the audio bytes must be converted or rejected explicitly rather than
reinterpreted unchanged.

Then verify default/non-default devices, permissions, hotplug, pause/resume,
long sessions, recording, camera formats, still capture, stop-motion, and
packaged multimedia backends.

### Input and high DPI

Keep synthetic event smokes, but separately verify real mouse/keyboard/window
system delivery and tablet pressure/tilt. Cover 1.0, 1.25, 1.5, 1.75, and 2.0
scale factors, mixed-display movement, off-primary placement, and OpenGL
framebuffer/device-pixel behavior.

### Translations and runtime data

Decide whether the 63 generated `.qm` files are intentional release inputs,
reproducible build outputs, or CI artifacts. If the project intentionally
tracks them, document the regeneration command and prove package language
switching. Otherwise generate them in the release pipeline and avoid treating
tracked binaries as source evidence.

### Named parity corpus

Run the same named corpus against the same-commit Qt 5 and Qt 6 packages. Cover
at least:

- raster, vector, and Toonz Raster scene creation/open/save/reload;
- viewer, selection, onion skin, guides, overlays, schematic, and timeline;
- Preview, FX Preview, final render, and script render;
- bundled and user shader FX;
- representative user scripts;
- audio, camera, and stop-motion where hardware is supported;
- real input/tablet and mixed-DPI workflows.

Record expected result, observed result, reviewer, artifact, and waiver using
the template in
`doc/qt6_remaining_work_and_manual_verification.md`.

### Exit gate

- every supported platform/hardware row is accepted or explicitly waived;
- same-commit Qt 5/Qt 6 corpus results are retained;
- environment limitations are separated from product failures;
- all remaining P0/P1 requirement rows have owners and release dispositions.

## R7 — Reduce Structural Debt After Product Gates Stabilize

### Recommendation

Do not mix these changes into R1-R6 unless they directly block a release gate.
They create broad churn and can invalidate otherwise useful evidence.

Recommended order:

1. Extract the approximately 15,000 lines of app-side GUI smoke machinery from
   `toonz/sources/toonz/main.cpp` into a test-only compilation unit, target, or
   explicit build option while preserving registry coverage.
2. Add advisory Clazy Qt 6 port checks and promote only stable, low-noise checks
   into mechanical guardrails.
3. Give `Core5Compat`, the text-codec adapter, Qt 5 Script sources, and the
   legacy multimedia fallback explicit removal prerequisites. Do not remove
   them merely to make the source look more Qt 6-native.
4. Align `.github/qt6-validation/manual-goals.json` and its synchronizer with
   stable requirement IDs so the machine projection cannot drift from the
   authoritative matrix.
5. Consider splitting review by subsystem, but do not rewrite the published
   branch history solely to manufacture small commits after stabilization.

### Exit gate

- production and test code have a clear build boundary;
- new static checks are stable and documented;
- compatibility bridges have evidence-backed exit conditions;
- machine-readable project state projects the same requirement IDs and current
  dated report;
- no structural cleanup invalidates unrecorded release evidence.

## R8 — Converge On A Release Candidate

### Recommendation

Freeze one release-candidate commit and produce all final evidence from it. Do
not combine evidence from several nearly identical tips.

Required convergence record:

- exact source commit and clean state;
- platform/compiler/architecture and Qt matrix;
- Qt 5, Qt 6, strict-deprecation, and translation build results;
- package checksums and publication links;
- aggregate script and GUI results;
- named corpus comparison;
- real hardware/input/DPI results;
- open issue list with severity, owner, scope, and release disposition;
- approved waivers with rationale, approver, and expiry;
- release notes and reviewer/studio signoff.

### Exit gate

Every P0 and P1 row in the authoritative requirement matrix is accepted or has
an approved, owned, time-bounded waiver. Packages and manual evidence identify
the same release-candidate commit. Only then should Qt 5 retirement or the
separate Metal migration return to the active plan.

## Recommended Commit And Review Boundaries

Keep the implementation reviewable by using these boundaries where practical:

1. live-master integration only;
2. version/support policy and its direct CMake/compatibility enforcement;
3. build-frontier repairs, separated by platform when independent;
4. shader probe/fix/test;
5. Script Console architecture/fix/test;
6. CI trigger/build/package changes;
7. packaged-runtime verification;
8. multimedia conversion and hardware-specific fixes;
9. input/high-DPI fixes;
10. test-boundary and compatibility-debt cleanup;
11. release-evidence and documentation convergence.

Each boundary should name the requirement IDs it advances and the exact tests
that passed. Avoid a single “finish Qt 6” commit spanning unrelated gates.

## Stop/Go Rules

Stop the current package and repair the lower layer when:

- live-master integration is unresolved;
- the supported Qt/platform contract is still ambiguous;
- Qt 5 or Qt 6 no longer builds from the candidate;
- the strict-deprecation lane is weakened or bypassed;
- shader output is crash-free but still black or otherwise incorrect;
- Script Console cancellation depends on the blocked UI event loop;
- a package builds but cannot launch or find its plugins/runtime data;
- evidence comes from a different commit than the claimed candidate.

Proceed to the next package only when the current exit gate is met or an
explicit waiver is recorded. Lack of available hardware may defer a manual row;
it does not convert that row into a pass.

## Recommended First Turn When Resuming The Main Goal

The first resumed implementation turn should address only R0 and R1:

1. refresh live branch/CI evidence;
2. integrate live `master` using a reviewable merge boundary;
3. review the eight overlapping paths;
4. run the full migration preflight and both configure lanes;
5. record the post-integration candidate and any changed blockers;
6. stop before shader, scripting, or packaging work unless the integration gate
   is fully satisfied.

The second turn should finalize R2 and run R3. Only then should R4A and R4B
start in parallel.

This sequencing gives the main goal prompt a stable base, prevents functional
work from being invalidated by integration or version-policy changes, and
focuses the remaining effort on the small number of proofs that actually block
completion.
