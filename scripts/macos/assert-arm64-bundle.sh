#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

app_path="${1:-${OPENTOONZ_APP:-toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app}}"
required_arch="${OPENTOONZ_REQUIRED_ARCH:-arm64}"
allow_universal="${OPENTOONZ_ALLOW_UNIVERSAL:-0}"

if [[ ! -d "$app_path" ]]; then
  echo "error: app bundle not found: $app_path" >&2
  exit 1
fi

main_binary="$app_path/Contents/MacOS/OpenToonz"
if [[ ! -x "$main_binary" ]]; then
  echo "error: OpenToonz binary not found: $main_binary" >&2
  exit 1
fi

echo "Main binary:"
file "$main_binary"
lipo -archs "$main_binary"

failed=0
checked=0

while IFS= read -r -d '' candidate; do
  if ! file_output="$(file -b "$candidate")"; then
    continue
  fi

  [[ "$file_output" == *Mach-O* ]] || continue

  checked=$((checked + 1))
  archs="$(lipo -archs "$candidate" 2>/dev/null || true)"

  if [[ " $archs " != *" $required_arch "* ]]; then
    echo "error: missing $required_arch slice: $candidate ($archs)" >&2
    failed=1
    continue
  fi

  if [[ "$allow_universal" != "1" && "$archs" != "$required_arch" ]]; then
    echo "error: expected $required_arch-only Mach-O: $candidate ($archs)" >&2
    failed=1
  fi
done < <(find "$app_path/Contents" -type f -print0)

echo "Checked $checked Mach-O files for $required_arch."
exit "$failed"
