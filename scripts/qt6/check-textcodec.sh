#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
qt_major="${1:-${OPENTOONZ_QT_MAJOR:-6}}"

case "$qt_major" in
  5|qt5|Qt5) qt_major=5 ;;
  6|qt6|Qt6) qt_major=6 ;;
  *)
    echo "usage: $0 [qt5|qt6]" >&2
    exit 2
    ;;
esac

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/opentoonz-ttextcodec.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT

source="$tmpdir/ttextcodec_check.cpp"
binary="$tmpdir/ttextcodec_check"

cat >"$source" <<'CPP'
#if defined(__aarch64__) || defined(__ARM_ARCH)
#include <arm_acle.h>
#endif

#include "ttextcodec.h"

#include <QByteArray>
#include <QString>

#include <iostream>

namespace {

bool expect(bool ok, const char *message) {
  if (!ok) {
    std::cerr << "ttextcodec-check failed: " << message << '\n';
  }
  return ok;
}

}  // namespace

int main() {
  bool ok = true;

  const QString japanese =
      QString::fromUtf8("\346\227\245\346\234\254\350\252\236");
  const QByteArray shiftJis = TTextCodec::fromUnicode("Shift-JIS", japanese);
  ok &= expect(shiftJis == QByteArray::fromHex("93fa967b8cea"),
               "Shift-JIS encode");
  ok &= expect(TTextCodec::toUnicode("Shift-JIS", shiftJis) == japanese,
               "Shift-JIS decode");

  const QString chinese = QString::fromUtf8("\344\270\255\346\226\207");
  const QByteArray gbk = TTextCodec::fromUnicode("GBK", chinese);
  ok &= expect(gbk == QByteArray::fromHex("d6d0cec4"), "GBK encode");
  ok &= expect(TTextCodec::toUnicode("GBK", gbk) == chinese, "GBK decode");

  const QString accented = QString::fromUtf8("caf\303\251");
  const QByteArray fallback =
      TTextCodec::fromUnicode("not-a-real-codec", accented);
  ok &= expect(fallback == accented.toUtf8(), "unknown codec UTF-8 fallback");
  ok &= expect(TTextCodec::toUnicode("not-a-real-codec", fallback) ==
                   accented,
               "unknown codec UTF-8 fallback decode");

  if (!ok) return 1;

  std::cout << "ttextcodec-check ok\n";
  return 0;
}
CPP

modules=("Qt${qt_major}Core")
if [[ "$qt_major" == "6" ]]; then
  modules+=("Qt6Core5Compat")
fi

read -r -a qt_flags <<<"$(pkg-config --cflags --libs "${modules[@]}")"
cxx="${CXX:-c++}"
"$cxx" -std=c++17 -I"$repo_root/toonz/sources/include" "$source" \
  "${qt_flags[@]}" -o "$binary"
"$binary"
