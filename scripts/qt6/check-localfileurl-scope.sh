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

if ! git grep -q 'localFileUrl' -- 'toonz/sources/include/toonzqt/qtcompat.h'; then
  echo "error: expected QtCompat::localFileUrl() helper in qtcompat.h" >&2
  exit 1
fi

direct_file_url_hits="$(
  grep_hits git grep -nE \
    -e 'QUrl[[:space:]]*\([[:space:]]*"file:///' \
    -e 'QUrl[[:space:]]*\([[:space:]]*[^)]*(getQString|toQString|fromStdWString)' \
    -e 'QUrl::fromLocalFile[[:space:]]*\([[:space:]]*[^)]*(getQString|toQString|fromStdWString)' -- \
    'toonz/sources' \
    ':!toonz/sources/include/toonzqt/qtcompat.h' \
    ':!toonz/sources/toonzqt/gutil.cpp' \
    ':!toonz/sources/common/tsystem/tsystem.cpp' \
    ':!toonz/sources/translations'
)"

direct_desktop_services_from_local_file_hits="$(
  grep_hits git grep -nE \
    -e 'QDesktopServices::openUrl[[:space:]]*\([[:space:]]*QUrl::fromLocalFile' -- \
    'toonz/sources' \
    ':!toonz/sources/common/tsystem/tsystem.cpp' \
    ':!toonz/sources/translations'
)"

if [[ -n "$direct_file_url_hits" || -n "$direct_desktop_services_from_local_file_hits" ]]; then
  printf '%s\n' "$direct_file_url_hits"
  printf '%s\n' "$direct_desktop_services_from_local_file_hits"
  echo "error: use QtCompat::localFileUrl() for local file/folder URLs" >&2
  exit 1
fi

echo "qt6-localfileurl-scope-check ok"
