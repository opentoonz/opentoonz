#!/bin/bash

set -e

[ ! -z "$HOST" ] || (echo "host was not set" && false)

mkdir -p "$INST_DIR"
mkdir -p "$DIST_DIR"
export PREFIX="$INST_DIR"

EXTRA_CPP_OPTIONS=

export PATH="/usr/$HOST/bin:/usr/$HOST/sys-root/mingw/bin:$PATH"
export LD_LIBRARY_PATH="/usr/$HOST/sys-root/mingw/lib:$LD_LIBRARY_PATH"

export ADDR2LINE=/usr/bin/$HOST-addr2line
export AS=/usr/bin/$HOST-as
export AR=/usr/bin/$HOST-ar
export CC=/usr/bin/$HOST-gcc
export CXXFILT=/usr/bin/$HOST-c++filt
export CXX=/usr/bin/$HOST-c++
export CPP=/usr/bin/$HOST-cpp
export DLLTOOL=/usr/bin/$HOST-dlltool
export DLLWRAP=/usr/bin/$HOST-dllwrap
export ELFEDIT=/usr/bin/$HOST-elfedit
export FORTRAN=/usr/bin/$HOST-gfortran
export GXX=/usr/bin/$HOST-g++
export GCC=/usr/bin/$HOST-gcc
export GCOV=/usr/bin/$HOST-gcov
export GCOV_TOOL=/usr/bin/$HOST-gcov-tool
export GFORTRAN=/usr/bin/$HOST-gfortran
export GPROF=/usr/bin/$HOST-gprof
export LD=/usr/bin/$HOST-ld
export LD_BFD=/usr/bin/$HOST-ld.bfd
export NM=/usr/bin/$HOST-nm
export OBJCOPY=/usr/bin/$HOST-objcopy
export OBJDUMP=/usr/bin/$HOST-objdump
export PKG_CONFIG=/usr/bin/$HOST-pkg-config
export RANLIB=/usr/bin/$HOST-ranlib
export READELF=/usr/bin/$HOST-readelf
export SIZE=/usr/bin/$HOST-size
export STRINGS=/usr/bin/$HOST-strings
export STRIP=/usr/bin/$HOST-strip
export WINDMC=/usr/bin/$HOST-windmc
export RC=/usr/bin/$HOST-windres
export WINDRES=/usr/bin/$HOST-windres

export LDFLAGS=" -L/usr/$HOST/sys-root/mingw/lib $LDFLAGS"
export CFLAGS=" $EXTRA_CPP_OPTIONS $CFLAGS"
export CPPFLAGS=" $EXTRA_CPP_OPTIONS $CPPFLAGS"
export CXXFLAGS=" $EXTRA_CPP_OPTIONS $CXXFLAGS"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/$HOST/sys-root/mingw/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="/usr/$HOST/sys-root/mingw/lib"
export XDG_DATA_DIRS="$XDG_DATA_DIRS"
export CMAKE_INCLUDE_PATH="$CMAKE_INCLUDE_PATH"
export CMAKE_LIBRARY_PATH="/usr/$HOST/sys-root/mingw/lib:$CMAKE_LIBRARY_PATH"

unset EXTRA_CPP_OPTIONS


export PATH="$PREFIX/bin:$PATH"
export LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH"
export LDFLAGS="-L$PREFIX/lib $LDFLAGS"
export CFLAGS="-I$PREFIX/include $CFLAGS"
export CPPFLAGS="-I$PREFIX/include $CPPFLAGS"
export CXXFLAGS="-I$PREFIX/include $CXXFLAGS"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig:$PKG_CONFIG_PATH"
export PKG_CONFIG_LIBDIR="$PREFIX/lib:$PKG_CONFIG_LIBDIR"
export PKG_CONFIG_SYSROOT_DIR="/"
export XDG_DATA_DIRS="$PREFIX/share:$XDG_DATA_DIRS"
#export ACLOCAL_PATH="$PREFIX/share/aclocal:$ACLOCAL_PATH"
export CMAKE_INCLUDE_PATH="$PREFIX/include:$CMAKE_INCLUDE_PATH"
export CMAKE_LIBRARY_PATH="$PREFIX/lib:$CMAKE_LIBRARY_PATH"
