#!/bin/sh
# Generated file, master is Makefile.am
. ${srcdir:-.}/common.sh
infile="$srcdir/images/ojpeg_single_strip_no_rowsperstrip.tiff"
outfile="o-tiff2rgba-ojpeg_single_strip_no_rowsperstrip.tiff"
f_test_convert "$TIFF2RGBA" $infile $outfile
f_tiffinfo_validate $outfile
