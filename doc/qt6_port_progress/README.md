# Qt 6 Port Progress Reports

This directory contains dated, immutable evidence snapshots for the OpenToonz
Qt 6 migration.

## Current Snapshot

The current authoritative snapshot is:

- `2026-07-13-branch-audit.md` — full branch, implementation, CI/package,
  documentation-contract, and primary-research audit at `82a9a8e58`.

The implementation-sequencing companion is:

- `../qt6_implementation_recommendations.md` — ordered work packages,
  dependencies, parallelization rules, exit gates, and the recommended first
  turn before resuming the main goal prompt.

Reports dated July 6 or earlier are historical snapshots. They remain useful
for chronology and provenance, but they must not override a newer report or be
quoted as current without refreshing the relevant repository and CI evidence.

## Report Contract

Each new report should record:

- exact audited commit and dirty/clean state;
- live comparison base and whether remotes were refreshed;
- date/time of external CI observations;
- exact platform, compiler, and Qt version for build/runtime evidence;
- requirement IDs changed since the preceding report;
- retained commands, logs, images, packages, checksums, or CI links;
- what each result proves and does not prove;
- blockers added, closed, reopened, or invalidated;
- the preceding report it supersedes for current-state claims.

Do not republish the full migration checklist in each dated report. Record the
delta and link to:

- `doc/qt6_migration_goal_prompt.md` for the execution contract;
- `doc/qt6_remaining_work_and_manual_verification.md` for requirements and
  manual procedures;
- `doc/qt6_migration_study.md` for stable architecture and primary research.

Historical reports must not be rewritten to make their evidence appear newer.
