#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

allowed='^(toonz/sources/toonz/main\.cpp|toonz/sources/include/qtofflinegl\.h|toonz/sources/common/tvrender/qtofflinegl\.cpp)$'

if rg -n "\\bQGLFormat\\b|<QGLFormat>" toonz/sources --glob '!**/build/**' |
  awk -F: -v allowed="$allowed" '$1 !~ allowed { print; found = 1 } END { exit found ? 0 : 1 }'; then
  cat >&2 <<'MSG'
QGLFormat is allowed only in the documented Qt 5 startup/offline-GL compatibility
scope. Use QSurfaceFormat for active Qt 5/Qt 6 OpenGL format setup.
MSG
  exit 1
fi

if ! awk '
  /#if QT_VERSION < QT_VERSION_CHECK\(6, 0, 0\)/ { depth++ }
  /#if/ && !/#if QT_VERSION < QT_VERSION_CHECK\(6, 0, 0\)/ && depth > 0 { depth++ }
  /#endif/ && depth > 0 { depth-- }
  /QGLFormat/ && depth == 0 { print FILENAME ":" FNR ":" $0; found = 1 }
  END { exit found ? 1 : 0 }
' toonz/sources/toonz/main.cpp toonz/sources/include/qtofflinegl.h \
  toonz/sources/common/tvrender/qtofflinegl.cpp; then
  echo "QGLFormat references must remain inside Qt 5-only preprocessor guards." >&2
  exit 1
fi

echo "qt6-qglformat-scope-check ok"
