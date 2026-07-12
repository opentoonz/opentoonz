#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
shader_dir="$repo_root/stuff/library/shaders"

if [[ ! -d "$shader_dir" ]]; then
  echo "error: shader library directory not found: $shader_dir" >&2
  exit 1
fi

xml_count=0
reference_count=0
while IFS= read -r xml_file; do
  ((xml_count += 1))
  while IFS= read -r program; do
    [[ -z "$program" ]] && continue
    ((reference_count += 1))
    if [[ ! -f "$shader_dir/$program" ]]; then
      echo "error: missing shader program referenced by $xml_file: $program" >&2
      exit 1
    fi
  done < <(sed -nE 's/.*"(programs\/[^\"]+)".*/\1/p' "$xml_file")
done < <(find "$shader_dir" -maxdepth 1 -type f -name '*.xml' -print | sort)

if (( xml_count == 0 || reference_count == 0 )); then
  echo "error: shader library contains no usable XML program references" >&2
  exit 1
fi

printf 'qt6-shader-assets-check ok: %d interfaces, %d program references\n' \
  "$xml_count" "$reference_count"
