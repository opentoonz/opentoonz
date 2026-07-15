#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

grep_hits() {
  local output
  local status

  set +e
  output="$("$@" 2>&1)"
  status=$?
  set -e

  if [[ $status -gt 1 ]]; then
    printf '%s\n' "$output" >&2
    exit "$status"
  fi

  printf '%s' "$output"
}

font_database_hits="$(
  grep_hits git grep -nE \
    -e '(^|[^[:alnum:]_])QFontDatabase[[:space:]]*[*&]?[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*([;={]|$)' \
    -e 'new[[:space:]]+QFontDatabase[[:space:]]*([;(]|$)' \
    -- 'toonz/sources' ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/common/tvrender/tfont_qt.cpp' \
    ':!toonz/sources/translations'
)"

compat_font_database_hits="$(
  awk '
    FNR == 1 {
      stackDepth = 0
      qt5Depth = 0
      delete qtGuard
      delete qt5Active
    }
    /#if QT_VERSION < QT_VERSION_CHECK\(6, 0, 0\)/ {
      stackDepth++
      qtGuard[stackDepth] = "lt6"
      qt5Active[stackDepth] = 1
      qt5Depth++
      next
    }
    /#if QT_VERSION >= QT_VERSION_CHECK\(6, 0, 0\)/ {
      stackDepth++
      qtGuard[stackDepth] = "ge6"
      qt5Active[stackDepth] = 0
      next
    }
    /#if/ {
      stackDepth++
      qtGuard[stackDepth] = ""
      qt5Active[stackDepth] = 0
      next
    }
    /#else/ && stackDepth > 0 {
      if (qtGuard[stackDepth] == "lt6") {
        qt5Depth--
        qt5Active[stackDepth] = 0
      } else if (qtGuard[stackDepth] == "ge6") {
        qt5Depth++
        qt5Active[stackDepth] = 1
      }
      next
    }
    /#endif/ && stackDepth > 0 {
      if (qt5Active[stackDepth]) qt5Depth--
      delete qtGuard[stackDepth]
      delete qt5Active[stackDepth]
      stackDepth--
      next
    }
    /(^|[^[:alnum:]_])QFontDatabase[[:space:]]*[*&]?[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*([;={]|$)/ && qt5Depth == 0 {
      print FILENAME ":" FNR ":" $0
      found = 1
    }
    /new[[:space:]]+QFontDatabase[[:space:]]*([;(]|$)/ && qt5Depth == 0 {
      print FILENAME ":" FNR ":" $0
      found = 1
    }
    END { exit found ? 0 : 1 }
  ' toonz/sources/include/toonzqt/qtcompat.h \
    toonz/sources/common/tvrender/tfont_qt.cpp || true
)"

set_font_family_hits="$(
  grep_hits git grep -nE -e '[.]setFontFamily[[:space:]]*\(' -- \
    'toonz/sources' ':!toonz/sources/translations'
)"

if [[ -n "$font_database_hits" || -n "$compat_font_database_hits" ||
      -n "$set_font_family_hits" ]]; then
  [[ -z "$font_database_hits" ]] || printf '%s\n' "$font_database_hits"
  [[ -z "$compat_font_database_hits" ]] ||
    printf '%s\n' "$compat_font_database_hits"
  [[ -z "$set_font_family_hits" ]] || printf '%s\n' "$set_font_family_hits"
  echo "error: use QtCompat font database/text-format helpers instead of deprecated direct font APIs" >&2
  exit 1
fi

echo "qt6-fontdatabase-scope-check ok"
