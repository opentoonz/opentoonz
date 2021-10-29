#!/bin/sh
#
# check decoding of a CCITT Group 4 encoded TIFF
# with 0 length runs
. ${srcdir:-.}/common.sh
infile="${IMAGES}/testfax4.tiff"
outfile="o-testfax4.tiff"
rm -f $outfile
echo "$MEMCHECK ${TIFFCP} -c lzw $infile $outfile"
eval "$MEMCHECK ${TIFFCP} -c lzw $infile $outfile"
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
