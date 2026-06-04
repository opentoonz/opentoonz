#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$repo_root"

search_sources() {
  local pattern="$1"

  if command -v rg >/dev/null 2>&1; then
    rg -n \
      --glob '*.cpp' \
      --glob '*.cxx' \
      --glob '*.cc' \
      --glob '*.mm' \
      "$pattern" toonz/sources || true
  else
    git grep -n -E "$pattern" -- toonz/sources |
      awk -F: '$1 ~ /\.(cpp|cxx|cc|mm)$/ { print }' || true
  fi
}

is_allowed_qualified_export_definition() {
  case "$1" in
    toonz/sources/tnzbase/tfxutil.cpp:*"DVAPI TFxP TFxUtil::makeCheckboard("*)
      return 0
      ;;
    toonz/sources/common/tipc/tipc.cpp:*"DVAPI void tipc::terminateCurrentBackgroundProcess("*)
      return 0
      ;;
  esac

  return 1
}

qualified_export_definition_pattern='^[[:space:]]*(DVAPI|DV_EXPORT_API|DV_IMPORT_API|TFARMAPI)[[:space:]]+[^;{]*::[^;{]*\{'
explicit_import_definition_pattern='^[[:space:]]*(DV_IMPORT_API|__declspec[[:space:]]*\([[:space:]]*dllimport[[:space:]]*\))[[:space:]]+[^;{]*\{'

violations=()
while IFS= read -r match; do
  [[ -z "$match" ]] && continue
  if ! is_allowed_qualified_export_definition "$match"; then
    violations+=("$match")
  fi
done < <(search_sources "$qualified_export_definition_pattern")

while IFS= read -r match; do
  [[ -z "$match" ]] && continue
  violations+=("$match")
done < <(search_sources "$explicit_import_definition_pattern")

if ((${#violations[@]} > 0)); then
  cat >&2 <<'EOF'
Windows MSVC ABI guard failed.

Avoid putting DVAPI, DV_IMPORT_API, DV_EXPORT_API, TFARMAPI, or explicit
__declspec(dllimport) on out-of-class C++ function definitions in source files.
Keep export/import annotations on declarations in headers; source definitions
should start with the return type. MSVC rejects definitions that are seen as
dllimport, and native macOS/Linux builds may not catch that.

Matches:
EOF

  printf '  %s\n' "${violations[@]}" >&2
  exit 1
fi

echo "Windows MSVC ABI guard passed."
