#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

require_text() {
  local file="$1"
  local text="$2"
  if ! rg -Fq -- "$text" "$file"; then
    echo "Qt version policy drift: expected '$text' in $file" >&2
    exit 1
  fi
}

require_text toonz/sources/CMakeLists.txt \
  'set(OPENTOONZ_QT_MINIMUM_VERSION 6.9.0)'
require_text toonz/sources/CMakeLists.txt \
  'QT_DISABLE_DEPRECATED_UP_TO=0x060A00'
require_text .github/workflows/workflow_qt6_experimental_binaries.yml \
  'QT6_EXPERIMENTAL_QT_VERSION: 6.10.3'
require_text doc/qt6_migration_study.md \
  '| Compatibility floor | 6.9.0 API floor, tested with 6.9.3 |'
require_text doc/qt6_migration_study.md \
  '| Primary release | 6.10.3 |'
require_text doc/qt6_migration_study.md \
  '| Forward compatibility | Nixpkgs Qt 6.11.x (6.11.0 on July 14) |'

echo "qt6-version-policy-check ok"
