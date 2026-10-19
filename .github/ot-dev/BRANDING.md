# OT-Dev build branding

## Active splash

`splash-template-area10.svg` contains the OT-Dev title, live PR/build fields,
and a relative image link to `ot-dev-splash-artwork.png`. The PNG is the original
artwork supplied by Rodney Baker. The complete image appears above the metadata
footer, without cropping. The splash fits within 610 by 480 logical pixels.

The build populates the PR number/title, description, UTC date, and actual
checkout commit. Branch builds show the ref instead. All seven original fields
and the two wrapped heading fields remain available. XML escaping happens once.

The package contains **both** `ot-dev-splash.svg` and
`ot-dev-splash-artwork.png` beside the executable. To change the artwork in a
local build, replace that PNG with a valid PNG under the same name and restart.
To change future CI builds, replace `.github/ot-dev/ot-dev-splash-artwork.png`.
The SVG layout and its PR data remain separate from the image.

The compiled splash embeds the same PNG as base64, together with the same live
metadata. The build-only startup overlay uses this self-contained resource if
the external SVG or PNG is missing. Both files are uploaded as a review artifact.
The older `splash.svg` cannot mask the OT-Dev splash. Source overlays are applied
only in the CI working tree and do not change the clean upstream source.

Preserved designs: `splash-template-assembly-line.svg` and
`splash-template-legacy.svg`. Select one explicitly with `--template` to render
an archived design. Keep these OT-Dev assets out of upstream feature proposals.

Both `main` and `edition` need this infrastructure update for their ordinary
PR builds. Merging one branch's PR does not update the other branch's builds.
Existing downloads require a fresh build for the new template and startup path.

## Branded builds of upstream-based PRs

`otdev_branding.yml` is maintained on `edition`. Create a branch **from edition**
named **otdev-build/pr-NUMBER**, such as `otdev-build/pr-259`. A suffix such as
`otdev-build/pr-259-20261019` requests a fresh build with current metadata.
Branch creation and pushes trigger it. No extra PR is needed for build refs.

The job fetches current PR metadata and its test merge SHA, checks out that
exact source, and loads branding assets from its workflow revision. It uploads
the application and the SVG/PNG review pair. A conflicted or closed PR is
rejected. The PR source and clean upstream mirror stay intact.

Validation: `python -m unittest discover -s .github/ot-dev -p test_brand_build.py -v`.
Artwork supplied by Rodney Baker. SVG layout and build implementation assisted
by OpenAI Codex. OT-Dev title and live metadata remain SVG text.
