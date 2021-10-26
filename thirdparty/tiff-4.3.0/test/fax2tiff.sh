#!/bin/sh
#
# Basic sanity check for fax2tiff
#
. ${srcdir:-.}/common.sh
infile="${IMAGES}/miniswhite-1c-1b.g3"
outfile="o-fax2tiff.tiff"
rm -f $outfile
echo "$MEMCHECK ${FAX2TIFF} -M -o $outfile $infile"
eval $MEMCHECK ${FAX2TIFF} -M -o $outfile $infile
status=$?
if [ $status != 0 ] ; then
  echo "Returned failed status $status!"
  echo "Output (if any) is in \"${outfile}\"."
  exit $status
fi
f_tiffinfo_validate $outfile
