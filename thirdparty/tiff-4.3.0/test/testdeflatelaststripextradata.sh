#!/bin/sh
#
# check decoding of a deflate compressed file whose last strip which should
# contain data for only 4 lines has more in it.
. ${srcdir:-.}/common.sh
infile="${IMAGES}/deflate-last-strip-extra-data.tiff"
outfile="o-deflate-last-strip-extra-data.tiff"
rm -f $outfile
echo "$MEMCHECK ${TIFFCP} -c zip $infile $outfile"
eval "$MEMCHECK ${TIFFCP} -c zip $infile $outfile"
status=$?
if [ $status != 0 ] ; then
  echo "Returned failed status $status!"
  echo "Output (if any) is in \"${outfile}\"."
  exit $status
fi
echo "$MEMCHECK ${TIFFCMP} $outfile ${REFS}/$outfile"
eval "$MEMCHECK ${TIFFCMP} $outfile ${REFS}/$outfile"
status=$?
if [ $status != 0 ] ; then
  echo "Returned failed status $status!"
  echo "\"${outfile}\" differs from reference file."
  exit $status
fi

outfile="o-deflate-last-strip-extra-data-tiled.tiff"
rm -f $outfile
echo "$MEMCHECK ${TIFFCP} -c zip -t -w 256 -l 256 $infile $outfile"
eval "$MEMCHECK ${TIFFCP} -c zip -t -w 256 -l 256 $infile $outfile"
status=$?
if [ $status != 0 ] ; then
  echo "Returned failed status $status!"
  echo "Output (if any) is in \"${outfile}\"."
  exit $status
fi
echo "$MEMCHECK ${TIFFCMP} $outfile ${REFS}/o-deflate-last-strip-extra-data.tiff"
eval "$MEMCHECK ${TIFFCMP} $outfile ${REFS}/o-deflate-last-strip-extra-data.tiff"
status=$?
if [ $status != 0 ] ; then
  echo "Returned failed status $status!"
  echo "\"${outfile}\" differs from reference file."
  exit $status
fi
