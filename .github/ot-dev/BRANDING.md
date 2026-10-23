# OT-Dev Windows build branding

OT-Dev Windows application packages include the splash, live PR/build information
in the title bar, and the session recorder. The overlay applies only in Windows
CI; Linux, macOS, clean upstream source, and standalone CLI/test builds keep
their existing behavior.

## Replace the artwork without editing the SVG

| Package path | Purpose |
| --- | --- |
| `OpenToonz.exe` | Application with the title-bar label and recorder |
| `splash.svg` | Stable layout with populated PR/build text |
| `portablestuff/splash.png` | Replaceable artwork at the root of `portablestuff` |
| `otdev-recorder/` | Encoder, matching sources, licenses, and README |
| `otdev-build.json` | Verified source, infrastructure, and package identity |

Replace **`portablestuff/splash.png`** with a valid PNG and restart OpenToonz.
No SVG edit or rebuild is needed. The fixed image reference in `splash.svg` is
`portablestuff/splash.png`, resolved relative to the SVG beside the executable.
The launch working directory does not affect it. CI copies the PNG after the
ordinary `stuff` files so those files cannot overwrite the selected artwork.

For future builds, replace `.github/ot-dev/ot-dev-splash-artwork.png`; CI installs
it as `portablestuff/splash.png`. Edit `splash-template-area10.svg` only when
changing the layout or templating. The original artwork was supplied by Rodney
Baker. It appears above the metadata footer without cropping, within 610 by 480
logical pixels.

CI populates the PR number/title, description, UTC date, and actual compiled
checkout commit. Branch builds identify their ref. All seven original fields
and the two wrapped heading fields remain available. XML escaping happens once.
The compiled splash embeds the same PNG and metadata as a self-contained
fallback when either external file is missing. Replacing the external PNG
retains the SVG's build information. Older downloads need a fresh build to
acquire this layout.

Preserved designs remain in `splash-template-assembly-line.svg` and
`splash-template-legacy.svg`, renderable explicitly with `--template` for
reference. Complete Windows packages require the active template and fixed PNG
location. Keep these OT-Dev assets out of upstream feature proposals.

## Automatic Windows coverage

`workflow_windows.yml` applies the full overlay directly when its branch
contains this infrastructure. Existing PR-specific tests still run. After
copying the application and `stuff`, `package_build.py` verifies the compiled
title, build ID, recorder, SVG/PNG, and encoder distribution before publication.

**OT-Dev Windows Package** (`otdev_branding.yml`) also follows successful
completion of the existing application workflows:

| Source workflow | Coverage |
| --- | --- |
| `Windows Build` (`workflow_windows.yml`) | Clean upstream-based PRs, older main/Edition PRs, and branch builds |
| `OT-Dev Branded PR Build` (`otdev_branding.yml`, older revisions) | Existing dedicated PR build refs |
| `Channel Stack Preview (Windows)` (`channel_stack_preview.yml`) | The existing combined channel preview |

The source workflow performs its own validation first. If it publishes both a
new-format complete application package and
`OT-Dev-package-manifest-SOURCE_RUN_ID`, the follow-up skips the duplicate build.
Otherwise it builds a complete OT-Dev package from that successful run's exact
application source, with build infrastructure checked out separately. Old
splash-only artifact names do not count as completion.

For PRs, the resolver checks the artifact's source SHA against the current test
merge and its exact base/head parents. Closed, conflicted, stale, or ambiguous
PR runs are skipped. A fresh source run or manual request builds the current PR.
Branch and channel-stack builds retain their source ref and commit instead of
being presented as a single PR. The **OT-Dev Windows package** commit status
links to the follow-up result.

Download the complete application artifact:

- `OT-Dev-PR-N-Windows-SOURCE_SHA-RUN_ID`
- `OT-Dev-Branch-REF-Windows-SOURCE_SHA-RUN_ID`

Ordinary branch builds use `Branch-build` in the name and record the actual ref
in `otdev-build.json`. Encoder, branding, and manifest artifacts are supporting
files. Original source-validation runs and artifacts remain available;
historical downloads are not rewritten. Existing PR heads, bases, and
specialized tests are preserved.

### Default-branch requirement

Maintain this infrastructure on `edition` and make `edition` the repository
default **after merging and validating the workflow**. GitHub starts a
[`workflow_run` listener](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#workflow_run)
only when its file exists on the default branch. Merging it into a non-default
branch alone does not activate automatic coverage.

Keep `upstream/master` an exact upstream mirror. New feature proposals still
choose `upstream/master` explicitly as their base, as described in
`CONTRIBUTING.md`; changing the default does not retarget existing PRs. Updating
`main` with this infrastructure avoids a follow-up rebuild, but the listener
also covers its older workflows.

Compilation and recorder-test jobs use read-only repository permissions,
persist no checkout credentials, and receive no repository secrets. They run
separately from jobs that publish status. The new worker name differs from the
observed source names, preventing a loop.

## Manual requests and infrastructure validation

Use **Actions → OT-Dev Windows Package → Run workflow**, select the infrastructure
branch, and supply exactly one input: `pr_number` for an open OT-Dev PR, or `ref`
for a branch/commit.

The existing build-ref route also works. Create a branch from the desired
infrastructure revision named `otdev-build/pr-NUMBER`, optionally with a suffix
such as `otdev-build/pr-279-branding-test`. Creation and pushes resolve the PR's
current merge separately from the build-ref commit. This tests unmerged
infrastructure without changing PR source. The unused reusable-workflow entry
point is removed; use these explicit request routes.

## Verification

Run the tests with Qt 5.15.2 and CMake available:

```sh
python -m pip install PyQt5==5.15.2
QT_QPA_PLATFORM=offscreen python -m unittest discover -s .github/ot-dev -p 'test_*.py' -v
python -m unittest discover -s .github/ot-dev/recorder -p 'test_*.py' -v
actionlint .github/workflows/workflow_windows.yml .github/workflows/otdev_branding.yml
```

The Qt regression renders from an unrelated working directory, replaces only
the PNG, and verifies that the artwork changes while the SVG bytes and rendered
metadata stay unchanged. It also renders the embedded fallback without the
external PNG. Resolver tests cover source identity, stale merges, pagination,
and duplicate prevention. Package tests reject incomplete branding/recorder
distributions. Recorder tests check external paths with the real CMake parser.

Windows CI additionally compiles and tests recorder consent, capture, encoding,
and native playback, builds OpenToonz, checks its compiled markers, and launches
the packaged encoder. Recording retains its existing consent behavior.

Artwork and requirements: Rodney Baker. Existing SVG/build work: OpenAI Codex
assistance. This Windows coverage and replaceable-PNG update: GPT-6 Astra Pro
assistance.
