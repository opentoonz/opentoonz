#!/bin/bash

set -e
[ ! -z "$SRC_DIR" ] || (echo "env was not set" && false)

NAME="libmypaint"
cp --remove-destination -rup "$SRC_DIR/$NAME" "$BUILD_DIR/"
cd "$BUILD_DIR/$NAME"

./autogen.sh
./configure --host="$HOST" --prefix="$PREFIX" --enable-shared --enable-static
make clean
make -j8
make install

cp "$INST_DIR/bin/"libiconv*.dll "$DIST_DIR/"
cp "$INST_DIR/bin/"libintl*.dll "$DIST_DIR/"
cp "$INST_DIR/bin/"libjson-c*.dll "$DIST_DIR/"

cp "$INST_DIR/bin/"libmypaint*.dll "$DIST_DIR/"
cp "$INST_DIR/lib/"libmypaint*.dll.a "$DIST_DIR/libmypaint.lib"
mkdir -p "$DIST_DIR/include"
cp -r "$INST_DIR/include/libmypaint" "$DIST_DIR/include/"

echo ""
echo "success"
echo ""
