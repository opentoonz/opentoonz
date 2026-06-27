#!/usr/bin/env bash
set -euo pipefail

repo_root="${OPENTOONZ_REPO_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$repo_root"

qtime_timer_uses="$(
  git grep -l 'QTime' -- 'toonz/sources' |
    xargs perl -ne '
      if (/\bQTime\s+([^;]+);/) {
        my $decl = $1;
        while ($decl =~ /\b([A-Za-z_][A-Za-z0-9_]*)\b/g) {
          $vars{$ARGV}{$1} = 1;
        }
      }
      for my $var (keys %{ $vars{$ARGV} || {} }) {
        if (/(^|[^A-Za-z0-9_])\Q$var\E\s*\.\s*(start|elapsed)\s*\(/) {
          print "$ARGV:$.:$_";
          last;
        }
      }
      if (eof) {
        $. = 0;
        delete $vars{$ARGV};
      }
    ' || true
)"

if [[ -n "$qtime_timer_uses" ]]; then
  printf '%s\n' "$qtime_timer_uses"
  echo "error: use QElapsedTimer instead of deprecated QTime start()/elapsed() timing APIs" >&2
  exit 1
fi

echo "qt6-qtime-elapsed-scope-check ok"
