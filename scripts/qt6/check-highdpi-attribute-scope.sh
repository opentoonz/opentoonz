#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

require_inside_qt5_cpp_block() {
  local file="$1"
  local needle="$2"
  local description="$3"

  awk -v needle="$needle" -v description="$description" '
    function fail(message) {
      printf "%s:%d: %s\n", FILENAME, FNR, message
      failed = 1
    }

    /^[[:space:]]*#[[:space:]]*if/ {
      ++depth
      if ($0 ~ /^[[:space:]]*#[[:space:]]*if[[:space:]]+QT_VERSION[[:space:]]*<[[:space:]]*QT_VERSION_CHECK\(6,[[:space:]]*0,[[:space:]]*0\)/)
        qt5Depth = depth
    }

    index($0, needle) > 0 {
      found = 1
      if (qt5Depth == 0)
        fail(description " must stay inside a QT_VERSION < QT_VERSION_CHECK(6, 0, 0) block")
    }

    /^[[:space:]]*#[[:space:]]*endif/ {
      if (qt5Depth == depth)
        qt5Depth = 0
      if (depth > 0)
        --depth
    }

    END {
      if (!found) {
        printf "%s: missing expected Qt 5-only startup attribute for %s\n", FILENAME, description
        failed = 1
      }
      exit failed ? 1 : 0
    }
  ' "$file"
}

require_inside_qt5_cpp_block \
  toonz/sources/toonz/main.cpp \
  'AA_EnableHighDpiScaling' \
  'Qt 5 high-DPI scaling startup attribute'

require_inside_qt5_cpp_block \
  toonz/sources/toonz/main.cpp \
  'AA_UseHighDpiPixmaps' \
  'Qt 5 high-DPI pixmap startup attribute'

echo "qt6-highdpi-attribute-scope-check ok"
