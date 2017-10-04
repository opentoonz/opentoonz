#!/bin/bash
pushd thirdparty/tiff-4.0.3
./configure && make
popd
cd toonz && mkdir build && cd build
cmake ../sources \
      -DQT_PATH=/usr/local/Cellar/qt5/5.6.1-1/lib/ \
      -DTIFF_INCLUDE_DIR=../../thirdparty/tiff-4.0.3/libtiff/ \
      -DSUPERLU_INCLUDE_DIR=../../thirdparty/superlu/SuperLU_4.1/include/
make
