#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

require_inside_qt5_block() {
  local file="$1"
  local needle="$2"
  local description="$3"

  awk -v needle="$needle" -v description="$description" '
    function fail(message) {
      printf "%s:%d: %s\n", FILENAME, FNR, message
      failed = 1
    }

    /^[[:space:]]*if[[:space:]]*\(/ {
      ++depth
      if ($0 ~ /^[[:space:]]*if[[:space:]]*\([[:space:]]*OPENTOONZ_QT_MAJOR[[:space:]]+EQUAL[[:space:]]+5[[:space:]]*\)/)
        qt5_depth = depth
    }

    /^[[:space:]]*else[[:space:]]*\(/ || /^[[:space:]]*else[[:space:]]*$/ {
      if (qt5_depth == depth)
        in_qt5_else = 1
    }

    index($0, needle) > 0 {
      found = 1
      if (qt5_depth == 0 || in_qt5_else)
        fail(description " must stay inside an OPENTOONZ_QT_MAJOR EQUAL 5 block")
    }

    /^[[:space:]]*endif[[:space:]]*\(/ || /^[[:space:]]*endif[[:space:]]*$/ {
      if (qt5_depth == depth) {
        qt5_depth = 0
        in_qt5_else = 0
      }
      if (depth > 0)
        --depth
    }

    END {
      if (!found) {
        printf "%s: missing expected Qt 5-only CMake entry for %s\n", FILENAME, description
        failed = 1
      }
      exit failed ? 1 : 0
    }
  ' "$file"
}

require_inside_qt5_block \
  toonz/sources/CMakeLists.txt \
  'list(APPEND OPENTOONZ_QT_COMPONENTS Script)' \
  'Qt Script component selection'

require_inside_qt5_block \
  toonz/sources/CMakeLists.txt \
  'set(QT_SCRIPT_TARGET Qt5::Script)' \
  'Qt Script target selection'

require_inside_qt5_block \
  toonz/sources/toonzlib/CMakeLists.txt \
  'list(APPEND MOC_HEADERS ${SCRIPT_BINDING_HEADERS})' \
  'legacy script binding MOC headers'

require_inside_qt5_block \
  toonz/sources/toonzlib/CMakeLists.txt \
  'list(APPEND SOURCES ${SCRIPT_BINDING_SOURCES})' \
  'legacy script binding sources'

if git grep -nE 'Qt6::Script|Qt::Script|QT_SCRIPT_TARGET.*Qt6' -- 'toonz/sources'; then
  echo "error: Qt 6 must use the QJSEngine facade, not Qt Script targets" >&2
  exit 1
fi

if git grep -nE '(^|[[:space:]])scriptengine[.]cpp([[:space:]]|$)' -- \
  'toonz/sources/toonz/CMakeLists.txt'; then
  echo "error: legacy toonz/scriptengine.cpp uses QScriptEngineDebugger and must not be added to the OpenToonz app target" >&2
  exit 1
fi

if git grep -nE 'QScriptEngineDebugger' -- 'toonz/sources' \
  ':!toonz/sources/toonz/scriptengine.cpp'; then
  echo "error: QScriptEngineDebugger has no Qt 6 replacement; keep debugger usage out of production script paths" >&2
  exit 1
fi

echo "qt6-script-scope-check ok"
