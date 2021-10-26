#!/bin/sh
. ${srcdir:-.}/common.sh
infile="$IMG_RGB_3C_8B_PPM"
outfile="o-ppm2tiff_8b_ppm.tiff"
f_test_convert "$PPM2TIFF" $infile $outfile
f_tiffinfo_validate $outfile
infile="$IMG_RGB_3C_16B_PPM"
outfile="o-ppm2tiff_16b_ppm.tiff"
f_test_convert "$PPM2TIFF" $infile $outfile
f_tiffinfo_validate $outfile
