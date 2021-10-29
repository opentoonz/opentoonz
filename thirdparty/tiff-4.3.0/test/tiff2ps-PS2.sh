#!/bin/sh
#
# Basic sanity check for tiffps with PostScript Level 2 output
#
PSFILE=o-tiff2ps-PS2.ps
. ${srcdir:-.}/common.sh
f_test_stdout "${TIFF2PS} -a -p -2" "${IMG_MINISWHITE_1C_1B}" "${PSFILE}"
diff -I '%%CreationDate:.*' -I '%%Title:.*' -u "${REFS}/${PSFILE}" "${PSFILE}" || exit 1
