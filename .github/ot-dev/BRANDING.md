# OT-Dev build branding

The assembly-line SVG is a template. `brand_build.py` fills it from the current
GitHub event: PR number/title, two description lines, UTC build date, and actual
checkout commit. Push and manual branch builds show the ref instead. The seven
original template fields remain supported; two wrapped heading fields keep long
PR titles inside the new artwork. XML escaping is applied once to metadata.

Ordinary Edition Windows builds generate the compiled splash and external
`splash.svg`, and add the PR/ref label to both window-title construction paths.
These changes are made only in the CI working tree.

## Branded builds of upstream-based PRs

The default `upstream/master` branch is an exact upstream mirror, so GitHub does
not expose the Edition-only manual dispatch inputs there. To build an open PR
with branding, create a branch **from edition** named **otdev-build/pr-NUMBER**
in GitHub's branch picker. For example, `otdev-build/pr-259` builds PR #259.
Branch creation triggers `otdev_branding.yml`; a push to that branch also runs it.
For a new build with refreshed metadata, create a new branch with a suffix such
as `otdev-build/pr-259-20261018`. No extra pull request is needed for build refs.

The job fetches the current PR description and test merge SHA, checks out that
exact commit, and obtains branding assets from its own workflow revision. It
uploads a branded Windows application and a separate populated SVG. It leaves
the PR branch and the clean mirror untouched. A conflicted/closed PR is rejected.

Existing PRs against `main` retain their established workflows until their
branding update is integrated there. This infrastructure belongs on `edition`;
do not include it or the build-only overlay in upstream feature proposals.

Validation: `python -m unittest discover -s .github/ot-dev -p test_brand_build.py -v`.
Artwork concept: Rodney Baker. SVG drawing and workflow implementation assisted
by OpenAI Codex. Static lettering is outlined; current build metadata is SVG text.
