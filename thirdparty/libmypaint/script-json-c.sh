#!/bin/bash

set -e
[ ! -z "$SRC_DIR" ] || (echo "env was not set" && false)

NAME="json-c-0.12.1"
cp --remove-destination -rup "$SRC_DIR/$NAME" "$BUILD_DIR/"
cd "$BUILD_DIR/$NAME"
set -e

export CFLAGS="-Wno-error=unknown-pragmas $CFLAGS"
export LIBS="-ladvapi32 -lintl"

./configure --host="$HOST" --prefix="$PREFIX" --enable-shared --disable-static
make || true
echo "#undef malloc" >> config.h
echo "#undef realloc" >> config.h
make -j8
make install

echo ""
echo "success"
echo ""
