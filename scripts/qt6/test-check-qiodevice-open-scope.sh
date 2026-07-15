#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$repo_root"

tmp_dir="${TMPDIR:-/tmp}/opentoonz-qiodevice-open-check-$$"
trap 'rm -rf "$tmp_dir"' EXIT

mkdir -p "$tmp_dir/good/toonz/sources" "$tmp_dir/bad/toonz/sources"

cat >"$tmp_dir/good/toonz/sources/good.cpp" <<'EOF'
#include <QFile>

bool readChecked() {
  QFile file("checked.txt");
  return file.open(QIODevice::ReadOnly);
}

bool readIfChecked() {
  QFile file("checked.txt");
  if (!file.open(QIODevice::ReadOnly)) return false;
  return true;
}

bool readCompoundChecked() {
  QFile file("checked.txt");
  return file.exists() && file.open(QIODevice::ReadOnly);
}
EOF

cat >"$tmp_dir/bad/toonz/sources/bad.cpp" <<'EOF'
#include <QFile>

void readUnchecked() {
  QFile file("unchecked.txt");
  file.open(QIODevice::ReadOnly);
}
EOF

for fixture in good bad; do
  git -C "$tmp_dir/$fixture" init -q
  git -C "$tmp_dir/$fixture" add toonz/sources
done

OPENTOONZ_REPO_ROOT="$tmp_dir/good" \
  bash scripts/qt6/check-qiodevice-open-scope.sh

bad_log="$tmp_dir/bad.log"
if OPENTOONZ_REPO_ROOT="$tmp_dir/bad" \
  bash scripts/qt6/check-qiodevice-open-scope.sh >"$bad_log" 2>&1; then
  cat "$bad_log" >&2
  echo "Expected Qt I/O open-result guard to fail for synthetic bad fixture." >&2
  exit 1
fi

echo "qt6-qiodevice-open-scope self-test passed."
