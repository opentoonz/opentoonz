#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

app_path="${OPENTOONZ_APP:-toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app}"
dmg_path="${OPENTOONZ_DMG:-${app_path%.app}.dmg}"
identity="${MACOS_DEVELOPER_ID_APPLICATION:-}"

if [[ -z "$identity" ]]; then
  echo "Skipping signing: MACOS_DEVELOPER_ID_APPLICATION is not set."
  exit 0
fi

if [[ ! -d "$app_path" ]]; then
  echo "error: app bundle not found: $app_path" >&2
  exit 1
fi

codesign --force --deep --options runtime --timestamp --sign "$identity" "$app_path"
codesign --verify --deep --strict --verbose=2 "$app_path"

if [[ ! -f "$dmg_path" ]]; then
  echo "Signed app. Skipping notarization until DMG exists: $dmg_path"
  exit 0
fi

codesign --force --timestamp --sign "$identity" "$dmg_path"
codesign --verify --verbose=2 "$dmg_path"
artifact="$dmg_path"

if [[ -n "${MACOS_NOTARY_KEYCHAIN_PROFILE:-}" ]]; then
  xcrun notarytool submit "$artifact" \
    --keychain-profile "$MACOS_NOTARY_KEYCHAIN_PROFILE" \
    --wait
elif [[ -n "${MACOS_NOTARY_APPLE_ID:-}" && \
        -n "${MACOS_NOTARY_TEAM_ID:-}" && \
        -n "${MACOS_NOTARY_PASSWORD:-}" ]]; then
  xcrun notarytool submit "$artifact" \
    --apple-id "$MACOS_NOTARY_APPLE_ID" \
    --team-id "$MACOS_NOTARY_TEAM_ID" \
    --password "$MACOS_NOTARY_PASSWORD" \
    --wait
else
  echo "Skipping notarization: no notarytool credentials are configured."
  exit 0
fi

xcrun stapler staple "$artifact"
xcrun stapler validate "$artifact"
