#!/bin/sh
#
# Basic sanity check for tiffps with PostScript Level 1 output
#
PSFILE="o-tiff2ps-PS1.ps"
. ${srcdir:-.}/common.sh
f_test_stdout "${TIFF2PS} -a -p -1" "${IMG_MINISWHITE_1C_1B}" "${PSFILE}"
#diff -I '%%(CreationDate|Title):.*' -u "${REFS}/${PSFILE}" "${PSFILE}" || exit 1
diff -I '%%CreationDate:.*' -I '%%Title:.*' -u "${REFS}/${PSFILE}" "${PSFILE}" || exit 1
