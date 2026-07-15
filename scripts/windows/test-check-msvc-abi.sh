#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$repo_root"

tmp_dir="${TMPDIR:-/tmp}/opentoonz-msvc-abi-check-$$"
trap 'rm -rf "$tmp_dir"' EXIT

mkdir -p "$tmp_dir/good" "$tmp_dir/bad"

cat >"$tmp_dir/good/good.cpp" <<'EOF'
#define DVAPI

class Exported {
public:
  DVAPI void declared();
};

void Exported::declared() {}
EOF

cat >"$tmp_dir/bad/bad.cpp" <<'EOF'
#define DVAPI __declspec(dllimport)
#define DV_IMPORT_API __declspec(dllimport)

class Imported {
public:
  DVAPI void declared();
};

DVAPI void Imported::declared() {}
DV_IMPORT_API void freeFunction() {}
EOF

bash scripts/windows/check-msvc-abi.sh "$tmp_dir/good"

bad_log="$tmp_dir/bad.log"
if bash scripts/windows/check-msvc-abi.sh "$tmp_dir/bad" >"$bad_log" 2>&1; then
  cat "$bad_log" >&2
  echo "Expected MSVC ABI guard to fail for synthetic bad fixture." >&2
  exit 1
fi

echo "Windows MSVC ABI guard self-test passed."
