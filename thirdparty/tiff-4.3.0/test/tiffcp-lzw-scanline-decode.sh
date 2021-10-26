#!/bin/sh
#
# Basic sanity check for tiffcp with LZW decompression
#
. ${srcdir:-.}/common.sh
f_test_convert "${TIFFCP} -c none -r 1" "${IMG_LZW_SINGLE_STROP}" "o-tiffcp-lzw-scanline-decode.tiff"