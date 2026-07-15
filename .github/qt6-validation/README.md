# Qt 6 Validation Project Automation

This directory is the contract between repository automation, Codex progress
reports, and the GitHub Project used to track Qt 6 migration validation.

## Files

- `manual-goals.json` lists manual, user, studio, and Codex-GUI validation goals
  that should appear as draft items in the GitHub Project.
- `scripts/qt6/sync-github-project.mjs` combines this file with the latest
  GitHub Actions results for `codex/qt-6-port` and updates Project fields.
- `.github/workflows/qt6_project_sync.yml` runs the sync when relevant CI
  workflows are requested, enter progress, or complete; on manual dispatch;
  every six hours; and whenever this contract changes on `master`.

## Token

The workflow expects a repository secret named `QT6_PROJECT_TOKEN` with access to
the user-owned GitHub Project. The token needs permission to read repository
Actions results and update GitHub Projects. The default `GITHUB_TOKEN` cannot
update this user-owned Project, so the workflow skips cleanly until the secret is
present.

Recommended token shape:

- Owner: `bgyss`
- Repository access: only `bgyss/opentoonz`
- Account permissions: Projects read/write
- Repository permissions: Actions read, Contents read, Metadata read

A classic personal token with broad `repo` and `project` scopes also works, but
it is broader than this workflow needs.

## Codex Progress Report Integration

The scheduled OpenToonz Qt 6 progress-report automation should update
`manual-goals.json` whenever it creates a new dated progress report. Its cadence
is configured outside this repository and must not be inferred from this file.
The schema is version 2. Each projected goal receives a stable slug
`projection_id`; the synchronizer stores that marker in the draft body and
uses it for upserts, with a title-derived fallback for older draft items.

`manual-goals.json` is a machine-readable projection, not the source of truth
for current branch state. Its `source_report` and `source_commit` identify
the dated report and exact commit it projects. If it is older than the latest report under `doc/qt6_port_progress`,
treat it as stale and do not use its baseline, CI state, or blockers as current
evidence. The July 13, 2026 audit intentionally did not edit the JSON because
this was a documentation-only audit; its OpenToonz 1.7.1 baseline and July 6
branch/CI snapshot require a separate synchronized update.

Keep build/package readiness separate from GUI and UX validation. A successful
build can make Codex or human validation possible, but it is not itself GUI
parity.
