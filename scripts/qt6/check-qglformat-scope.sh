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
  FNR == 1 {
    stackDepth = 0
    qt5Depth = 0
    delete qtGuard
    delete qt5Active
  }
  /#if QT_VERSION < QT_VERSION_CHECK\(6, 0, 0\)/ {
    stackDepth++
    qtGuard[stackDepth] = "lt6"
    qt5Active[stackDepth] = 1
    qt5Depth++
    next
  }
  /#if QT_VERSION >= QT_VERSION_CHECK\(6, 0, 0\)/ {
    stackDepth++
    qtGuard[stackDepth] = "ge6"
    qt5Active[stackDepth] = 0
    next
  }
  /#if/ {
    stackDepth++
    qtGuard[stackDepth] = ""
    qt5Active[stackDepth] = 0
    next
  }
  /#else/ && stackDepth > 0 {
    if (qtGuard[stackDepth] == "lt6") {
      qt5Depth--
      qt5Active[stackDepth] = 0
    } else if (qtGuard[stackDepth] == "ge6") {
      qt5Depth++
      qt5Active[stackDepth] = 1
    }
    next
  }
  /#endif/ && stackDepth > 0 {
    if (qt5Active[stackDepth]) qt5Depth--
    delete qtGuard[stackDepth]
    delete qt5Active[stackDepth]
    stackDepth--
    next
  }
  /^[[:space:]]*(\/\/|\/\*|\*)/ { next }
  /QGLFormat/ && qt5Depth == 0 {
    print FILENAME ":" FNR ":" $0
    found = 1
  }
  END { exit found ? 1 : 0 }
' toonz/sources/toonz/main.cpp toonz/sources/include/qtofflinegl.h \
  toonz/sources/common/tvrender/qtofflinegl.cpp; then
  echo "QGLFormat references must remain inside Qt 5-only preprocessor guards." >&2
  exit 1
fi

echo "qt6-qglformat-scope-check ok"
