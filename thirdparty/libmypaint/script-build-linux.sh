#!/bin/bash

mkdir -p src && cd src

[[ -d libmypaint ]] || git clone git@github.com:mypaint/libmypaint.git && cd libmypaint

./autogen.sh &&
./configure &&
make

cp .libs/libmypaint-2.0.so* ../../dist/64 && echo "All good! :D"
