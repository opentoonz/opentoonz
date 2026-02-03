#!/bin/sh
pushd ../../
git diff master --name-only | egrep \\.\(c\|cpp\|h\|hpp\)$ | xargs clang-format -style=file -i
popd
