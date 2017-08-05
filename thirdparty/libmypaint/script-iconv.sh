#!/bin/bash

set -e
[ ! -z "$SRC_DIR" ] || (echo "env was not set" && false)

NAME="libiconv-1.15"
cp --remove-destination -rup "$SRC_DIR/$NAME" "$BUILD_DIR/"
cd "$BUILD_DIR/$NAME"

./configure --host="$HOST" --prefix="$PREFIX" --enable-shared --disable-static
make -j8
make install

echo ""
echo "success"
echo ""
