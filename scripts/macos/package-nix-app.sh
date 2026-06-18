#!/usr/bin/env bash
set -euo pipefail

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

app_path="${1:-${OPENTOONZ_APP:-toonz/build/nix-relwithdebinfo/toonz/OpenToonz.app}}"
repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

if [[ ! -d "$app_path" ]]; then
  echo "error: app bundle not found: $app_path" >&2
  exit 1
fi

app_path="$(cd "$(dirname "$app_path")" && pwd)/$(basename "$app_path")"

if ! command -v macdeployqt >/dev/null 2>&1; then
  echo "error: macdeployqt is not available in PATH" >&2
  exit 1
fi

app_dir="$(cd "$(dirname "$app_path")" && pwd)"
app_name="$(basename "$app_path")"
macos_dir="$app_path/Contents/MacOS"
frameworks_dir="$app_path/Contents/Frameworks"
plugins_dir="$app_path/Contents/PlugIns"
qt_conf="$app_path/Contents/Resources/qt.conf"
portable_stuff="$app_path/Contents/Resources/portablestuff"
legacy_portable_stuff="$app_path/portablestuff"

rewrite_bundle_dylib_references() {
  [[ -d "$frameworks_dir" ]] || return 0

  while IFS= read -r -d '' target_file; do
    if ! file "$target_file" | grep -q 'Mach-O'; then
      continue
    fi
    while read -r from_path; do
      [[ -n "$from_path" ]] || continue
      lib_name="$(basename "$from_path")"
      if [[ -e "$frameworks_dir/$lib_name" ]]; then
        install_name_tool -change "$from_path" \
          "@executable_path/../Frameworks/$lib_name" \
          "$target_file"
      fi
    done < <(
      otool -L "$target_file" |
        awk '/\.dylib/ { print $1 }' |
        grep -v '^@executable_path/../Frameworks/' || true
    )
  done < <(find "$app_path/Contents" -type f -print0)
}

copy_missing_store_dylibs() {
  [[ -d "$frameworks_dir" ]] || return 0

  local copied=1
  while [[ "$copied" == "1" ]]; do
    copied=0
    while IFS= read -r -d '' target_file; do
      if ! file "$target_file" | grep -q 'Mach-O'; then
        continue
      fi
      while read -r from_path; do
        [[ -n "$from_path" ]] || continue
        [[ "$from_path" == /nix/store/* ]] || continue
        lib_name="$(basename "$from_path")"
        if [[ ! -e "$frameworks_dir/$lib_name" ]]; then
          cp -f "$from_path" "$frameworks_dir/$lib_name"
          chmod u+w "$frameworks_dir/$lib_name"
          install_name_tool -id "@executable_path/../Frameworks/$lib_name" \
            "$frameworks_dir/$lib_name"
          copied=1
        fi
      done < <(otool -L "$target_file" | awk '/\.dylib/ { print $1 }')
    done < <(find "$app_path/Contents" -type f -print0)

    rewrite_bundle_dylib_references
  done
}

copy_qt_plugins() {
  [[ -n "${OPENTOONZ_QT_PLUGIN_DIRS:-}" ]] || return 0

  mkdir -p "$plugins_dir"
  shopt -s nullglob
  IFS=':' read -r -a plugin_roots <<< "$OPENTOONZ_QT_PLUGIN_DIRS"
  for plugin_root in "${plugin_roots[@]}"; do
    [[ -n "$plugin_root" ]] || continue
    if [[ ! -d "$plugin_root" ]]; then
      echo "error: Qt plugin directory does not exist: $plugin_root" >&2
      exit 1
    fi
    while IFS= read -r -d '' plugin_group; do
      group_name="$(basename "$plugin_group")"
      case "$group_name" in
        audio|imageformats|mediaservice|platforms|playlistformats|printsupport|styles) ;;
        *) continue ;;
      esac
      if [[ -e "$plugins_dir/$group_name" ]]; then
        chmod -R u+w "$plugins_dir/$group_name" || true
      fi
      rm -rf "$plugins_dir/$group_name"
      mkdir -p "$plugins_dir/$group_name"

      case "$group_name" in
        audio)
          plugin_patterns=(libqtaudio_coreaudio.dylib)
          ;;
        mediaservice)
          plugin_patterns=(libqavfcamera.dylib libqavfmediaplayer.dylib libqtmedia_audioengine.dylib)
          ;;
        platforms)
          plugin_patterns=(libqcocoa.dylib libqminimal.dylib libqoffscreen.dylib)
          ;;
        *)
          plugin_patterns=(*.dylib)
          ;;
      esac

      for plugin_pattern in "${plugin_patterns[@]}"; do
        for plugin_file in "$plugin_group"/$plugin_pattern; do
          cp -p "$plugin_file" "$plugins_dir/$group_name"/
        done
      done
      chmod -R u+w "$plugins_dir/$group_name"
    done < <(find "$plugin_root" -mindepth 1 -maxdepth 1 -type d -print0)
  done
  shopt -u nullglob
}

rm -rf "$portable_stuff" "$legacy_portable_stuff"
mkdir -p "$(dirname "$portable_stuff")"
cp -pR "$repo_root/stuff" "$portable_stuff"

if otool -L "$macos_dir/OpenToonz" | grep -Eq '@(loader|executable)_path/\.\./Frameworks'; then
  if [[ ! -d "$frameworks_dir" ]]; then
    echo "error: app already references Contents/Frameworks, but that directory is missing" >&2
    echo "       rebuild OpenToonz before rerunning package-macos" >&2
    exit 1
  fi
  echo "Skipping macdeployqt: app already uses bundled Frameworks."
else
  for stale_dir in "$frameworks_dir" "$plugins_dir"; do
    if [[ -e "$stale_dir" ]]; then
      chmod -R u+w "$stale_dir" || true
    fi
  done
  rm -rf "$frameworks_dir" "$plugins_dir" "$qt_conf"

  deploy_args=("$app_name" -verbose=0 -always-overwrite)
  for helper in \
    lzocompress \
    lzodecompress \
    tcleanup \
    tcomposer \
    tconverter \
    tfarmcontroller \
    tfarmserver; do
    if [[ -x "$macos_dir/$helper" ]]; then
      deploy_args+=("-executable=$macos_dir/$helper")
    fi
  done

  cd "$app_dir"
  macdeployqt "${deploy_args[@]}"

  rewrite_bundle_dylib_references
fi

copy_qt_plugins
copy_missing_store_dylibs
rewrite_bundle_dylib_references

if [[ -n "${OPENTOONZ_GNU_LIBICONV_LIBRARY:-}" ]]; then
  if [[ ! -f "$OPENTOONZ_GNU_LIBICONV_LIBRARY" ]]; then
    echo "error: OPENTOONZ_GNU_LIBICONV_LIBRARY does not exist: $OPENTOONZ_GNU_LIBICONV_LIBRARY" >&2
    exit 1
  fi
  if [[ -d "$frameworks_dir" ]]; then
    if [[ -n "${OPENTOONZ_DARWIN_LIBICONV_LIBRARY:-}" ]]; then
      if [[ ! -f "$OPENTOONZ_DARWIN_LIBICONV_LIBRARY" ]]; then
        echo "error: OPENTOONZ_DARWIN_LIBICONV_LIBRARY does not exist: $OPENTOONZ_DARWIN_LIBICONV_LIBRARY" >&2
        exit 1
      fi
      bundled_iconv="$frameworks_dir/libiconv.2.dylib"
      cp -f "$OPENTOONZ_DARWIN_LIBICONV_LIBRARY" "$bundled_iconv"
      chmod u+w "$bundled_iconv"
      install_name_tool -id "@executable_path/../Frameworks/libiconv.2.dylib" "$bundled_iconv"
    fi

    bundled_gnu_iconv="$frameworks_dir/libgnuiconv.2.dylib"
    cp -f "$OPENTOONZ_GNU_LIBICONV_LIBRARY" "$bundled_gnu_iconv"
    chmod u+w "$bundled_gnu_iconv"
    install_name_tool -id "@executable_path/../Frameworks/libgnuiconv.2.dylib" "$bundled_gnu_iconv"

    while IFS= read -r -d '' target_file; do
      if ! file "$target_file" | grep -q 'Mach-O'; then
        continue
      fi
      while read -r from_path; do
        [[ -n "$from_path" ]] || continue
        install_name_tool -change "$from_path" \
          "@executable_path/../Frameworks/libiconv.2.dylib" \
          "$target_file"
      done < <(
        otool -L "$target_file" |
          awk '/libiconv\.2\.dylib/ { print $1 }' |
          grep -v '^@executable_path/../Frameworks/libiconv\.2\.dylib$' || true
      )

      if ! nm -u "$target_file" 2>/dev/null | grep -Eq '^_libiconv($|_)'; then
        continue
      fi
      while read -r from_path; do
        [[ -n "$from_path" ]] || continue
        install_name_tool -change "$from_path" \
          "@executable_path/../Frameworks/libgnuiconv.2.dylib" \
          "$target_file"
      done < <(
        otool -L "$target_file" |
          awk '/libiconv\.2\.dylib/ { print $1 }' |
          grep -v '^@executable_path/../Frameworks/libgnuiconv\.2\.dylib$' || true
      )
    done < <(find "$app_path/Contents" -type f -print0)

    copy_missing_store_dylibs
    rewrite_bundle_dylib_references
  fi
fi

if [[ "${OPENTOONZ_ADHOC_SIGN:-1}" == "1" ]]; then
  codesign --force --deep --sign - "$app_path"
  codesign --verify --deep --strict --verbose=2 "$app_path"
fi

echo "Packaged $app_path"
