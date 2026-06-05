#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

direct_fontmetrics_width="$(
  {
    git grep -nE 'fontMetrics\(\)[[:space:]]*\.width[[:space:]]*\(' -- \
      'toonz/sources' \
      ':!toonz/sources/include/toonzqt/qtcompat.h' || true
    git grep -nE 'QFontMetrics\([^)]*\)[[:space:]]*\.width[[:space:]]*\(' -- \
      'toonz/sources' \
      ':!toonz/sources/include/toonzqt/qtcompat.h' || true
    while IFS= read -r source_file; do
      awk '
        /(^|[^[:alnum:]_])QFontMetrics[[:space:]]+[[:alpha:]_][[:alnum:]_]*/ {
          varName = $0
          sub(/^.*QFontMetrics[[:space:]]+/, "", varName)
          sub(/[^[:alnum:]_].*$/, "", varName)
          if (varName != "") {
            fontMetricsVars[varName] = 1
          }
        }
        {
          for (varName in fontMetricsVars) {
            pattern = "(^|[^[:alnum:]_])" varName "[[:space:]]*\\.width[[:space:]]*\\("
            if ($0 ~ pattern) {
              printf "%s:%d:%s\n", FILENAME, FNR, $0
            }
          }
        }
      ' "$source_file"
    done < <(git grep -Il 'QFontMetrics' -- \
      'toonz/sources' \
      ':!toonz/sources/include/toonzqt/qtcompat.h' || true)
  } | sort -u
)"

if [[ -n "$direct_fontmetrics_width" ]]; then
  printf '%s\n' "$direct_fontmetrics_width"
  echo "error: use QtCompat::fontMetricsHorizontalAdvance() instead of deprecated QFontMetrics::width()" >&2
  exit 1
fi

if ! git grep -q 'fontMetricsHorizontalAdvance' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::fontMetricsHorizontalAdvance() helper in qtcompat.h" >&2
  exit 1
fi

echo "qt6-fontmetrics-scope-check ok"
