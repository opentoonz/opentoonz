#!/usr/bin/env bash
set -euo pipefail

package_root=${1:?package root is required}
executable=${2:?executable path is required}

test -d "$package_root"
test -x "$executable"
test -d "$package_root/stuff"
test -d "$package_root/stuff/config/qss"
test -d "$package_root/stuff/library/shaders"
qm_count=$(find "$package_root/stuff/config/loc" -mindepth 2 -maxdepth 2 -name '*.qm' | wc -l | tr -d ' ')
test "$qm_count" = 63

printf 'qt6-packaged-runtime-check ok: root=%s executable=%s translations=%s\n' "$package_root" "$executable" "$qm_count"
