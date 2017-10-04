#!/bin/bash

set -e
[ ! -z "$SRC_DIR" ] || (echo "env was not set" && false)

NAME="gettext-0.19.7"
cp --remove-destination -rup "$SRC_DIR/$NAME" "$BUILD_DIR/"
cd "$BUILD_DIR/$NAME"

export CFLAGS="-O2 $CFLAGS"
export CXXFLAGS="-O2 $CXXFLAGS"

./configure \
    --host="$HOST" \
    --prefix="$PREFIX" \
    --with-libiconv-prefix="$PREFIX" \
    --disable-java \
    --disable-native-java \
    --disable-csharp \
    --enable-static \
    --enable-threads=win32 \
    --without-emacs \
    --disable-openmp \
    --enable-shared \
    --disable-static
make -j8
make install

echo ""
echo "success"
echo ""
