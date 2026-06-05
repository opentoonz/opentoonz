# Qt 6 Validation Project Automation

This directory is the contract between repository automation, Codex progress
reports, and the GitHub Project used to track Qt 6 migration validation.

## Files

- `manual-goals.json` lists manual, user, studio, and Codex-GUI validation goals
  that should appear as draft items in the GitHub Project.
- `scripts/qt6/sync-github-project.mjs` combines this file with the latest
  GitHub Actions results for `codex/qt-6-port` and updates Project fields.
- `.github/workflows/qt6_project_sync.yml` runs the sync after relevant CI
  workflows complete, on manual dispatch, every six hours, and whenever this
  contract changes on `master`.

## Token

The workflow expects a repository secret named `QT6_PROJECT_TOKEN` with access to
the user-owned GitHub Project. The token needs permission to read repository
Actions results and update GitHub Projects. The workflow falls back to the
default `GITHUB_TOKEN`, but user-owned Projects often require the explicit
Project-capable token.

## Codex Progress Report Integration

The weekly OpenToonz Qt 6 progress-report automation should update
`manual-goals.json` whenever it creates or refreshes its dated progress docs.
Each goal should keep a stable `title`; the Project sync uses titles as the
upsert key to avoid duplicate Project items.

Keep build/package readiness separate from GUI and UX validation. A successful
build can make Codex or human validation possible, but it is not itself GUI
parity.
