#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

app_path="${1:-${OPENTOONZ_APP:-toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app}}"

if [[ ! -d "$app_path" ]]; then
  echo "error: app bundle not found: $app_path" >&2
  exit 1
fi

if ! command -v macdeployqt >/dev/null 2>&1; then
  echo "error: macdeployqt is not available in PATH" >&2
  exit 1
fi

app_dir="$(cd "$(dirname "$app_path")" && pwd)"
app_name="$(basename "$app_path")"
dmg_path="$app_dir/${app_name%.app}.dmg"

cd "$app_dir"
rm -f "$dmg_path"

attempt=1
until macdeployqt "$app_name" -dmg -verbose=0 && [[ -f "$dmg_path" ]]; do
  echo "DMG creation attempt $attempt failed." >&2
  if [[ "$attempt" -eq 10 ]]; then
    echo "error: DMG creation failed after 10 attempts" >&2
    exit 1
  fi
  attempt=$((attempt + 1))
  sleep 5
done

echo "Created $dmg_path"
