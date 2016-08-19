#!/bin/sh
git diff master --name-only | egrep \\.\(c\|cpp\|h\|hpp\)$ | xargs clang-format -i
