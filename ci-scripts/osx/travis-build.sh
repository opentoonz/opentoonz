#!/bin/bash
pushd thirdparty/tiff-4.0.3
./configure --disable-lzma && make
popd
cd toonz && mkdir build && cd build
QTVERSION=`ls /usr/local/Cellar/qt`
echo "QT Version detected: $QTVERSION"
cmake ../sources \
      -DQT_PATH=/usr/local/Cellar/qt/$QTVERSION/lib/ \
      -DTIFF_INCLUDE_DIR=../../thirdparty/tiff-4.0.3/libtiff/ \
      -DSUPERLU_INCLUDE_DIR=../../thirdparty/superlu/SuperLU_4.1/include/
make -j 2
