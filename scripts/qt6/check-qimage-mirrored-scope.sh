#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_qimage_mirrored="$(
  git grep -nE -e '\.mirrored[[:space:]]*\(' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' || true
)"

if [[ -n "$direct_qimage_mirrored" ]]; then
  printf '%s\n' "$direct_qimage_mirrored"
  echo "error: use QtCompat::mirroredImage() instead of direct QImage::mirrored()" >&2
  exit 1
fi

if ! git grep -q 'mirroredImage' -- \
  'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::mirroredImage() helper in qtcompat.h" >&2
  exit 1
fi

echo "qt6-qimage-mirrored-scope-check ok"
