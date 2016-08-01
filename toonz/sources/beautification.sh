#!/bin/sh
git ls-files | egrep \\.\(c\|cpp\|h\|hpp\)$ | xargs clang-format -i
