#!/bin/bash
pushd thirdparty/tiff-4.0.3
CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-jbig && make
popd
cd toonz && mkdir build && cd build
source /opt/qt59/bin/qt59-env.sh
cmake ../sources -G Ninja\
    -DWITH_SYSTEM_SUPERLU:BOOL=OFF
ninja
cd ../../
ccache --show-stats > ccache_post
diff -U100 ccache_pre ccache_post # report ccache's efficiency
