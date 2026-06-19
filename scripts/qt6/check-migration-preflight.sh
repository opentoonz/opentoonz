#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

checks=(
  scripts/windows/check-msvc-abi.sh
  scripts/qt6/check-no-qregexp.sh
  scripts/qt6/check-core5compat-scope.sh
  scripts/qt6/check-multimedia-scope.sh
  scripts/qt6/check-script-scope.sh
  scripts/qt6/check-script-smoke-registry.sh
  scripts/qt6/check-gui-smoke-registry.sh
  scripts/qt6/check-fontmetrics-scope.sh
  scripts/qt6/check-fontdatabase-scope.sh
  scripts/qt6/check-highdpi-attribute-scope.sh
  scripts/qt6/check-touch-scope.sh
  scripts/qt6/check-tabletevent-scope.sh
  scripts/qt6/check-mouseevent-scope.sh
  scripts/qt6/check-enterevent-scope.sh
  scripts/qt6/check-helpevent-scope.sh
  scripts/qt6/check-qvariant-scope.sh
  scripts/qt6/check-qkeysequence-scope.sh
  scripts/qt6/check-mediaplayer-scope.sh
  scripts/qt6/check-desktopwidget-scope.sh
  scripts/qt6/check-combobox-activated-scope.sh
  scripts/qt6/check-checkbox-state-scope.sh
  scripts/qt6/check-buttongroup-scope.sh
  scripts/qt6/check-wheelevent-scope.sh
  scripts/qt6/check-graphicssceneevent-scope.sh
  scripts/qt6/check-graphicsscenecontextmenu-scope.sh
  scripts/qt6/check-hoverevent-scope.sh
  scripts/qt6/check-localfileurl-scope.sh
  scripts/qt6/check-qaction-scope.sh
  scripts/qt6/check-qcolor-scope.sh
  scripts/qt6/check-qimage-mirrored-scope.sh
  scripts/qt6/check-qglformat-scope.sh
  scripts/qt6/check-qgllegacy-scope.sh
)

for check in "${checks[@]}"; do
  bash "$check"
done
