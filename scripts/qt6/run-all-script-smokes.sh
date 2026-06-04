#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

tasks=(
  script-smoke-qt6
  script-smoke-run-errors-qt6
  script-smoke-filepath-qt6
  script-smoke-filepath-edges-qt6
  script-smoke-filepath-metadata-qt6
  script-smoke-filepath-mutation-metadata-qt6
  script-smoke-path-arguments-qt6
  script-smoke-scene-qt6
  script-smoke-scene-cells-qt6
  script-smoke-scene-columns-qt6
  script-smoke-scene-cell-fids-qt6
  script-smoke-scene-edges-qt6
  script-smoke-scene-argument-edges-qt6
  script-smoke-scene-lifecycle-edges-qt6
  script-smoke-scene-level-wrappers-qt6
  script-smoke-scene-loadlevel-qt6
  script-smoke-scene-loadlevel-sequence-qt6
  script-smoke-scene-save-reopen-qt6
  script-smoke-scene-reload-edges-qt6
  script-smoke-scene-load-failure-qt6
  script-smoke-scene-save-icon-qt6
  script-smoke-scene-save-icon-variants-qt6
  script-smoke-scene-frameids-qt6
  script-smoke-level-qt6
  script-smoke-level-edges-qt6
  script-smoke-level-io-qt6
  script-smoke-level-io-types-qt6
  script-smoke-level-path-qt6
  script-smoke-image-qt6
  script-smoke-image-edges-qt6
  script-smoke-image-level-first-frame-qt6
  script-smoke-image-builder-qt6
  script-smoke-transform-edges-qt6
  script-smoke-image-builder-edges-qt6
  script-smoke-toonz-raster-converter-qt6
  script-smoke-toonz-raster-converter-level-qt6
  script-smoke-toonz-raster-converter-edges-qt6
  script-smoke-outline-vectorizer-qt6
  script-smoke-centerline-vectorizer-qt6
  script-smoke-vectorizer-edges-qt6
  script-smoke-binding-lifecycle-edges-qt6
  script-smoke-rasterizer-qt6
  script-smoke-rasterizer-edges-qt6
  script-smoke-renderer-qt6
  script-smoke-renderer-frames-columns-qt6
  script-smoke-renderer-vector-qt6
  script-smoke-renderer-edges-qt6
  script-smoke-level-transformers-qt6
  script-smoke-wrapper-id-qt6
)

if (($# > 0)); then
  tasks=("$@")
fi

for task in "${tasks[@]}"; do
  echo "==> mise run ${task}"
  mise run "$task"
done
