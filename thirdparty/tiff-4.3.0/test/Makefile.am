# Tag Image File Format (TIFF) Software
#
# Copyright (C) 2004, Andrey Kiselev <dron@ak4719.spb.edu>
#
# Permission to use, copy, modify, distribute, and sell this software and 
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Sam Leffler and Silicon Graphics.
# 
# THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
# EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
# WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
# 
# IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
# ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
# LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
# OF THIS SOFTWARE.

# Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.12 color-tests parallel-tests foreign

LIBTIFF = $(top_builddir)/libtiff/libtiff.la

# Environment parameters to be used during tests
TESTS_ENVIRONMENT = \
	MAKE="$(MAKE)" \
	MAKEFLAGS="$(MAKEFLAGS)" \
	MEMCHECK="$(MEMCHECK)"

EXTRA_DIST = \
	$(REFFILES) \
	$(TESTSCRIPTS) \
	$(IMAGES_EXTRA_DIST) \
	CMakeLists.txt \
	common.sh \
	TiffSplitTest.cmake \
	TiffTestCommon.cmake \
	TiffTest.cmake

# All of the tests to execute via 'make check'
TESTS = $(check_PROGRAMS) $(TESTSCRIPTS)

# Tests which are expected to fail
XFAIL_TESTS =

# Extra files which should be cleaned by 'make clean'
CLEANFILES = test_packbits.tif o-*

if HAVE_JPEG
JPEG_DEPENDENT_CHECK_PROG=raw_decode
JPEG_DEPENDENT_TESTSCRIPTS=\
	tiff2rgba-quad-tile.jpg.sh \
	tiff2rgba-ojpeg_zackthecat_subsamp22_single_strip.sh \
	tiff2rgba-ojpeg_chewey_subsamp21_multi_strip.sh \
	tiff2rgba-ojpeg_single_strip_no_rowsperstrip.sh

else
JPEG_DEPENDENT_CHECK_PROG=
JPEG_DEPENDENT_TESTSCRIPTS=
endif

# Executable programs which need to be built in order to support tests
check_PROGRAMS = \
	ascii_tag long_tag short_tag strip_rw rewrite custom_dir custom_dir_EXIF_231 \
	rational_precision2double defer_strile_loading defer_strile_writing testtypes \
	$(JPEG_DEPENDENT_CHECK_PROG)

# Test scripts to execute
TESTSCRIPTS = \
	ppm2tiff_pbm.sh \
	ppm2tiff_pgm.sh \
	ppm2tiff_ppm.sh \
	fax2tiff.sh \
	tiffcp-g3.sh \
	tiffcp-g3-1d.sh \
	tiffcp-g3-1d-fill.sh \
	tiffcp-g3-2d.sh \
	tiffcp-g3-2d-fill.sh \
	tiffcp-g4.sh \
	tiffcp-logluv.sh \
	tiffcp-thumbnail.sh \
	tiffcp-lzw-compat.sh \
	tiffcp-lzw-scanline-decode.sh \
	tiffdump.sh \
	tiffinfo.sh \
	tiffcp-split.sh \
	tiffcp-split-join.sh \
	tiff2ps-PS1.sh \
	tiff2ps-PS2.sh \
	tiff2ps-PS3.sh \
	tiff2ps-EPS1.sh \
	tiff2pdf.sh \
	tiffcrop-doubleflip-logluv-3c-16b.sh \
	tiffcrop-doubleflip-minisblack-1c-16b.sh \
	tiffcrop-doubleflip-minisblack-1c-8b.sh \
	tiffcrop-doubleflip-minisblack-2c-8b-alpha.sh \
	tiffcrop-doubleflip-miniswhite-1c-1b.sh \
	tiffcrop-doubleflip-palette-1c-1b.sh \
	tiffcrop-doubleflip-palette-1c-4b.sh \
	tiffcrop-doubleflip-palette-1c-8b.sh \
	tiffcrop-doubleflip-rgb-3c-16b.sh \
	tiffcrop-doubleflip-rgb-3c-8b.sh \
	tiffcrop-extract-logluv-3c-16b.sh \
	tiffcrop-extract-minisblack-1c-16b.sh \
	tiffcrop-extract-minisblack-1c-8b.sh \
	tiffcrop-extract-minisblack-2c-8b-alpha.sh \
	tiffcrop-extract-miniswhite-1c-1b.sh \
	tiffcrop-extract-palette-1c-1b.sh \
	tiffcrop-extract-palette-1c-4b.sh \
	tiffcrop-extract-palette-1c-8b.sh \
	tiffcrop-extract-rgb-3c-16b.sh \
	tiffcrop-extract-rgb-3c-8b.sh \
	tiffcrop-extractz14-logluv-3c-16b.sh \
	tiffcrop-extractz14-minisblack-1c-16b.sh \
	tiffcrop-extractz14-minisblack-1c-8b.sh \
	tiffcrop-extractz14-minisblack-2c-8b-alpha.sh \
	tiffcrop-extractz14-miniswhite-1c-1b.sh \
	tiffcrop-extractz14-palette-1c-1b.sh \
	tiffcrop-extractz14-palette-1c-4b.sh \
	tiffcrop-extractz14-palette-1c-8b.sh \
	tiffcrop-extractz14-rgb-3c-16b.sh \
	tiffcrop-extractz14-rgb-3c-8b.sh \
	tiffcrop-R90-logluv-3c-16b.sh \
	tiffcrop-R90-minisblack-1c-16b.sh \
	tiffcrop-R90-minisblack-1c-8b.sh \
	tiffcrop-R90-minisblack-2c-8b-alpha.sh \
	tiffcrop-R90-miniswhite-1c-1b.sh \
	tiffcrop-R90-palette-1c-1b.sh \
	tiffcrop-R90-palette-1c-4b.sh \
	tiffcrop-R90-palette-1c-8b.sh \
	tiffcrop-R90-rgb-3c-16b.sh \
	tiffcrop-R90-rgb-3c-8b.sh \
	tiff2bw-palette-1c-8b.sh \
	tiff2bw-quad-lzw-compat.sh \
	tiff2bw-rgb-3c-8b.sh \
	tiff2rgba-logluv-3c-16b.sh \
	tiff2rgba-minisblack-1c-16b.sh \
	tiff2rgba-minisblack-1c-8b.sh \
	tiff2rgba-minisblack-2c-8b-alpha.sh \
	tiff2rgba-miniswhite-1c-1b.sh \
	tiff2rgba-palette-1c-1b.sh \
	tiff2rgba-palette-1c-4b.sh \
	tiff2rgba-palette-1c-8b.sh \
	tiff2rgba-rgb-3c-16b.sh \
	tiff2rgba-rgb-3c-8b.sh \
	testfax4.sh \
	testdeflatelaststripextradata.sh \
	$(JPEG_DEPENDENT_TESTSCRIPTS)

# This list should contain the references files
# from the 'refs' subdirectory
REFFILES = \
	refs/o-tiff2ps-EPS1.ps \
	refs/o-tiff2ps-PS1.ps \
	refs/o-tiff2ps-PS2.ps \
	refs/o-tiff2ps-PS3.ps \
	refs/o-testfax4.tiff \
	refs/o-deflate-last-strip-extra-data.tiff

# This list should contain all of the TIFF files in the 'images'
# subdirectory which are intended to be used as input images for
# tests.  All of these files should use the extension ".tiff".
TIFFIMAGES = \
	images/logluv-3c-16b.tiff \
	images/minisblack-1c-16b.tiff \
	images/minisblack-1c-8b.tiff \
	images/minisblack-2c-8b-alpha.tiff \
	images/miniswhite-1c-1b.tiff \
	images/palette-1c-1b.tiff \
	images/palette-1c-4b.tiff \
	images/palette-1c-8b.tiff \
	images/rgb-3c-16b.tiff \
	images/rgb-3c-8b.tiff \
	images/quad-tile.jpg.tiff \
	images/quad-lzw-compat.tiff \
	images/lzw-single-strip.tiff \
	images/ojpeg_zackthecat_subsamp22_single_strip.tiff \
	images/ojpeg_chewey_subsamp21_multi_strip.tiff \
	images/ojpeg_single_strip_no_rowsperstrip.tiff \
	images/testfax4.tiff \
	images/deflate-last-strip-extra-data.tiff

PNMIMAGES = \
	images/minisblack-1c-8b.pgm \
	images/miniswhite-1c-1b.pbm \
	images/rgb-3c-16b.ppm \
	images/rgb-3c-8b.ppm

# This list should include all of the files in the 'images'
# subdirectory which are intended to be distributed.  This may include
# files which are not currently used by the tests.
IMAGES_EXTRA_DIST = \
	images/README.txt \
	images/miniswhite-1c-1b.g3 \
	$(PNMIMAGES) \
	$(TIFFIMAGES)

noinst_HEADERS = tifftest.h

ascii_tag_SOURCES = ascii_tag.c
ascii_tag_LDADD = $(LIBTIFF)
long_tag_SOURCES = long_tag.c check_tag.c
long_tag_LDADD = $(LIBTIFF)
short_tag_SOURCES = short_tag.c check_tag.c
short_tag_LDADD = $(LIBTIFF)
strip_rw_SOURCES = strip_rw.c strip.c test_arrays.c test_arrays.h
strip_rw_LDADD = $(LIBTIFF)
rewrite_SOURCES = rewrite_tag.c
rewrite_LDADD = $(LIBTIFF)
raw_decode_SOURCES = raw_decode.c
raw_decode_LDADD = $(LIBTIFF)
custom_dir_SOURCES = custom_dir.c
custom_dir_LDADD = $(LIBTIFF)
rational_precision2double_SOURCES = rational_precision2double.c
rational_precision2double_LDADD = $(LIBTIFF)
custom_dir_EXIF_231_SOURCES = custom_dir_EXIF_231.c
custom_dir_EXIF_231_LDADD = $(LIBTIFF)
defer_strile_loading_SOURCES = defer_strile_loading.c
defer_strile_loading_LDADD = $(LIBTIFF)
defer_strile_writing_SOURCES = defer_strile_writing.c
defer_strile_writing_LDADD = $(LIBTIFF)

AM_CPPFLAGS = -I$(top_srcdir)/libtiff

# memcheck: valgrind's memory access checker.
#
# The suppressions which come with valgrind are sometimes insufficient
# to handle certain system library aspects which may be reported and
# which are unrelated to libtiff.  When first starting with a new
# system (or after a major system update), it is good to execute the
# test suite (known to already be passing!) like 'make memcheck
# VALGRIND_EXTRA_OPTS=--gen-suppressions=all' to create valgrind
# suppression entries in the test log.  Make sure that none of the
# suppressions are due to libtiff itself.  Tell valgrind about the
# suppressions by creating a .valgrindrc file with content like:
# --memcheck:suppressions=mysupp.supp
memcheck:
	$(MAKE) MEMCHECK='valgrind --tool=memcheck --leak-check=full --read-var-info=yes \
	--error-exitcode=2 --track-origins=yes --num-callers=12 \
	--quiet $(VALGRIND_EXTRA_OPTS)' check

# ptrcheck: valgrind's experimental pointer checking tool.
ptrcheck:
	$(MAKE) MEMCHECK='valgrind --tool=exp-ptrcheck --quiet $(VALGRIND_EXTRA_OPTS)' check

# tiff2bw is pretty lame so currently only the generated scripts
# tiff2bw-palette-1c-8b.sh, tiff2bw-quad-lzw-compat.sh, and
# tiff2bw-rgb-3c-8b.sh pass tests.
generate-tiff2bw-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiff2bw-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiff2bw-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFF2BW\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

generate-tiff2rgba-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiff2rgba-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiff2rgba-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFF2RGBA\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

# Test rotations
generate-tiffcrop-R90-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiffcrop-R90-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiffcrop-R90-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFFCROP -R90\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

# Test flip (mirror)
generate-tiffcrop-doubleflip-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiffcrop-doubleflip-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiffcrop-doubleflip-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFFCROP -F both\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

# Test extracting a section 100 pixels wide and 100 pixels high
generate-tiffcrop-extract-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiffcrop-extract-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiffcrop-extract-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFFCROP -U px -E top -X 60 -Y 60\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

# Test extracting the first and fourth quarters from the left side.
generate-tiffcrop-extractz14-tests:
	for file in $(TIFFIMAGES) ; \
	do \
	base=`basename $$file .tiff` ; \
	testscript=$(srcdir)/tiffcrop-extractz14-$$base.sh ; \
	( \
	  echo "#!/bin/sh" ; \
	  echo "# Generated file, master is Makefile.am" ; \
	  echo ". \$${srcdir:-.}/common.sh" ; \
	  echo "infile=\"\$$srcdir/$$file\"" ; \
	  echo "outfile=\"o-tiffcrop-extractz14-$$base.tiff\"" ; \
	  echo "f_test_convert \"\$$TIFFCROP -E left -Z1:4,2:4\" \$$infile \$$outfile" ; \
	  echo "f_tiffinfo_validate \$$outfile" ; \
	) > $$testscript ; \
	chmod +x $$testscript ; \
	done

generate-tiffcrop-tests: \
	generate-tiffcrop-R90-tests \
	generate-tiffcrop-doubleflip-tests \
	generate-tiffcrop-extract-tests \
	generate-tiffcrop-extractz14-tests

generate-tests: \
	generate-tiff2bw-tests \
	generate-tiff2rgba-tests \
	generate-tiffcrop-tests
