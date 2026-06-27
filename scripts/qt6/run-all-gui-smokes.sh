#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

tasks=(
  gui-smoke-qt6
  gui-smoke-scene-create-qt6
  gui-smoke-highdpi-qt6
  gui-smoke-script-console-view-qt6
  gui-smoke-media-devices-qt6
  gui-smoke-camera-formats-qt6
  gui-smoke-audio-input-qt6
  gui-smoke-audio-recording-wav-qt6
  gui-smoke-audio-playback-wav-qt6
  gui-smoke-audio-output-qt6
  gui-smoke-xsheet-scrub-qt6
  gui-smoke-viewer-render-qt6
  gui-smoke-viewer-vector-render-qt6
  gui-smoke-preview-render-output-qt6
  gui-smoke-fx-preview-render-output-qt6
  gui-smoke-fx-preview-subcamera-render-output-qt6
  gui-smoke-fx-preview-flipbook-controls-qt6
  gui-smoke-fx-preview-save-previewed-frames-qt6
  gui-smoke-fx-preview-subcamera-save-previewed-frames-qt6
  gui-smoke-final-render-output-qt6
  gui-smoke-final-render-background-output-qt6
  gui-smoke-final-render-sequence-output-qt6
  gui-smoke-final-render-composite-output-qt6
  gui-smoke-final-render-vector-output-qt6
  gui-smoke-final-render-toonz-raster-output-qt6
  gui-smoke-final-render-fx-output-qt6
  gui-smoke-viewer-zoom-pan-qt6
  gui-smoke-viewer-onion-skin-qt6
  gui-smoke-viewer-onion-skin-rowarea-qt6
  gui-smoke-viewer-onion-skin-rowarea-drag-qt6
  gui-smoke-viewer-onion-skin-fixed-marker-drag-qt6
  gui-smoke-viewer-onion-skin-shift-trace-qt6
  gui-smoke-viewer-onion-skin-context-menu-qt6
  gui-smoke-viewer-onion-skin-custom-colors-qt6
  gui-smoke-viewer-onion-skin-orientations-qt6
  gui-smoke-viewer-camera-overlay-qt6
  gui-smoke-viewer-safe-area-field-guide-qt6
  gui-smoke-viewer-safe-area-presets-qt6
  gui-smoke-viewer-safe-area-custom-file-qt6
  gui-smoke-viewer-field-guide-settings-qt6
  gui-smoke-viewer-ruler-guide-qt6
  gui-smoke-viewer-ruler-guide-events-qt6
  gui-smoke-viewer-ruler-guide-variants-qt6
  gui-smoke-viewer-ruler-guide-lines-qt6
  gui-smoke-viewer-ruler-guide-styles-qt6
  gui-smoke-viewer-ruler-ticks-qt6
  gui-smoke-viewer-animate-tool-overlay-qt6
  gui-smoke-viewer-animate-tool-drag-qt6
  gui-smoke-viewer-animate-tool-mouse-events-qt6
  gui-smoke-viewer-animate-tool-undo-redo-qt6
  gui-smoke-viewer-animate-tool-modifiers-qt6
  gui-smoke-viewer-animate-tool-handles-qt6
  gui-smoke-viewer-animate-tool-handle-variants-qt6
  gui-smoke-viewer-animate-tool-axis-drags-qt6
  gui-smoke-viewer-animate-tool-cursors-qt6
  gui-smoke-viewer-selection-tool-vector-handles-qt6
  gui-smoke-viewer-selection-tool-vector-handle-variants-qt6
  gui-smoke-viewer-selection-tool-vector-center-thickness-deform-qt6
  gui-smoke-viewer-selection-tool-vector-mode-variants-qt6
  gui-smoke-viewer-selection-tool-raster-handles-qt6
  gui-smoke-viewer-selection-tool-raster-mode-variants-qt6
  gui-smoke-viewer-vector-brush-qt6
  gui-smoke-viewer-raster-brush-qt6
  gui-smoke-viewer-raster-brush-mouse-events-qt6
  gui-smoke-viewer-raster-brush-tablet-events-qt6
)

if (($# > 0)); then
  tasks=("$@")
fi

for task in "${tasks[@]}"; do
  echo "==> mise run ${task}"
  mise run "$task"
done
