# CMake build for libtiff
#
# Copyright © 2015 Open Microscopy Environment / University of Dundee
# Written by Roger Leigh <rleigh@codelibre.net>
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

# Test scripts (not used by CMake)
set(TESTSCRIPTS
    bmp2tiff_palette.sh
    bmp2tiff_rgb.sh
    gif2tiff.sh
    ppm2tiff_pbm.sh
    ppm2tiff_pgm.sh
    ppm2tiff_ppm.sh
    fax2tiff.sh
    tiffcp-g3.sh
    tiffcp-g3-1d.sh
    tiffcp-g3-1d-fill.sh
    tiffcp-g3-2d.sh
    tiffcp-g3-2d-fill.sh
    tiffcp-g4.sh
    tiffcp-logluv.sh
    tiffcp-thumbnail.sh
    tiffcp-lzw-compat.sh
    tiffcp-lzw-scanline-decode.sh
    tiffdump.sh
    tiffinfo.sh
    tiffcp-split.sh
    tiffcp-split-join.sh
    tiff2ps-PS1.sh
    tiff2ps-PS2.sh
    tiff2ps-PS3.sh
    tiff2ps-EPS1.sh
    tiff2pdf.sh
    tiffcrop-doubleflip-logluv-3c-16b.sh
    tiffcrop-doubleflip-minisblack-1c-16b.sh
    tiffcrop-doubleflip-minisblack-1c-8b.sh
    tiffcrop-doubleflip-minisblack-2c-8b-alpha.sh
    tiffcrop-doubleflip-miniswhite-1c-1b.sh
    tiffcrop-doubleflip-palette-1c-1b.sh
    tiffcrop-doubleflip-palette-1c-4b.sh
    tiffcrop-doubleflip-palette-1c-8b.sh
    tiffcrop-doubleflip-rgb-3c-16b.sh
    tiffcrop-doubleflip-rgb-3c-8b.sh
    tiffcrop-extract-logluv-3c-16b.sh
    tiffcrop-extract-minisblack-1c-16b.sh
    tiffcrop-extract-minisblack-1c-8b.sh
    tiffcrop-extract-minisblack-2c-8b-alpha.sh
    tiffcrop-extract-miniswhite-1c-1b.sh
    tiffcrop-extract-palette-1c-1b.sh
    tiffcrop-extract-palette-1c-4b.sh
    tiffcrop-extract-palette-1c-8b.sh
    tiffcrop-extract-rgb-3c-16b.sh
    tiffcrop-extract-rgb-3c-8b.sh
    tiffcrop-extractz14-logluv-3c-16b.sh
    tiffcrop-extractz14-minisblack-1c-16b.sh
    tiffcrop-extractz14-minisblack-1c-8b.sh
    tiffcrop-extractz14-minisblack-2c-8b-alpha.sh
    tiffcrop-extractz14-miniswhite-1c-1b.sh
    tiffcrop-extractz14-palette-1c-1b.sh
    tiffcrop-extractz14-palette-1c-4b.sh
    tiffcrop-extractz14-palette-1c-8b.sh
    tiffcrop-extractz14-rgb-3c-16b.sh
    tiffcrop-extractz14-rgb-3c-8b.sh
    tiffcrop-R90-logluv-3c-16b.sh
    tiffcrop-R90-minisblack-1c-16b.sh
    tiffcrop-R90-minisblack-1c-8b.sh
    tiffcrop-R90-minisblack-2c-8b-alpha.sh
    tiffcrop-R90-miniswhite-1c-1b.sh
    tiffcrop-R90-palette-1c-1b.sh
    tiffcrop-R90-palette-1c-4b.sh
    tiffcrop-R90-palette-1c-8b.sh
    tiffcrop-R90-rgb-3c-16b.sh
    tiffcrop-R90-rgb-3c-8b.sh
    tiff2rgba-logluv-3c-16b.sh
    tiff2rgba-minisblack-1c-16b.sh
    tiff2rgba-minisblack-1c-8b.sh
    tiff2rgba-minisblack-2c-8b-alpha.sh
    tiff2rgba-miniswhite-1c-1b.sh
    tiff2rgba-palette-1c-1b.sh
    tiff2rgba-palette-1c-4b.sh
    tiff2rgba-palette-1c-8b.sh
    tiff2rgba-rgb-3c-16b.sh
    tiff2rgba-rgb-3c-8b.sh
    tiff2rgba-quad-tile.jpg.sh
    tiff2rgba-ojpeg_zackthecat_subsamp22_single_strip.sh
    tiff2rgba-ojpeg_chewey_subsamp21_multi_strip.sh
    tiff2rgba-ojpeg_single_strip_no_rowsperstrip.sh
    testfax4.sh
    testdeflatelaststripextradata.sh)

# This list should contain all of the TIFF files in the 'images'
# subdirectory which are intended to be used as input images for
# tests.  All of these files should use the extension ".tiff".
set(TIFFIMAGES
    images/logluv-3c-16b.tiff
    images/minisblack-1c-16b.tiff
    images/minisblack-1c-8b.tiff
    images/minisblack-2c-8b-alpha.tiff
    images/miniswhite-1c-1b.tiff
    images/palette-1c-1b.tiff
    images/palette-1c-4b.tiff
    images/palette-1c-8b.tiff
    images/rgb-3c-16b.tiff
    images/rgb-3c-8b.tiff
    images/quad-tile.jpg.tiff
    images/quad-lzw-compat.tiff
    images/lzw-single-strip.tiff
    images/ojpeg_zackthecat_subsamp22_single_strip.tiff
    images/ojpeg_chewey_subsamp21_multi_strip.tiff
    images/ojpeg_single_strip_no_rowsperstrip.tiff
    images/testfax4.tiff)

set(BMPIMAGES
    images/palette-1c-8b.bmp
    images/rgb-3c-8b.bmp)

set(GIFIMAGES
    images/palette-1c-8b.gif)

set(PNMIMAGES
    images/minisblack-1c-8b.pgm
    images/miniswhite-1c-1b.pbm
    images/rgb-3c-16b.ppm
    images/rgb-3c-8b.ppm)

# All uncompressed image files
set(UNCOMPRESSEDIMAGES
    images/minisblack-1c-16b.tiff
    images/minisblack-1c-8b.tiff
    images/miniswhite-1c-1b.tiff
    images/palette-1c-1b.tiff
    images/palette-1c-4b.tiff
    images/palette-1c-8b.tiff
    images/rgb-3c-8b.tiff)

# This list should include all of the files in the 'images'
# subdirectory which are intended to be distributed.  This may include
# files which are not currently used by the tests.
set(IMAGES_EXTRA_DIST
    images/README.txt
    images/miniswhite-1c-1b.g3
    ${BMPIMAGES}
    ${GIFIMAGES}
    ${PNMIMAGES}
    ${TIFFIMAGES})

set(noinst_HEADERS tifftest.h)

add_executable(ascii_tag)
target_sources(ascii_tag PRIVATE ascii_tag.c)
target_link_libraries(ascii_tag PRIVATE tiff port)

add_executable(long_tag)
target_sources(long_tag PRIVATE long_tag.c check_tag.c)
target_link_libraries(long_tag PRIVATE tiff port)

add_executable(short_tag)
target_sources(short_tag PRIVATE short_tag.c check_tag.c)
target_link_libraries(short_tag PRIVATE tiff port)

add_executable(strip_rw)
target_sources(strip_rw PRIVATE strip_rw.c strip.c test_arrays.c test_arrays.h)
target_link_libraries(strip_rw PRIVATE tiff port)

add_executable(rewrite)
target_sources(rewrite PRIVATE rewrite_tag.c)
target_link_libraries(rewrite PRIVATE tiff port)

if(JPEG_SUPPORT)
  add_executable(raw_decode)
  target_sources(raw_decode PRIVATE raw_decode.c)
  target_link_libraries(raw_decode PRIVATE tiff port JPEG::JPEG)
endif()

add_executable(custom_dir)
target_sources(custom_dir PRIVATE custom_dir.c)
target_link_libraries(custom_dir PRIVATE tiff port)

if(NOT BUILD_SHARED_LIBS)
add_executable(rational_precision2double)
target_sources(rational_precision2double PRIVATE rational_precision2double.c)
target_link_libraries(rational_precision2double PRIVATE tiff port)
add_test(NAME "rational_precision2double"
         COMMAND "rational_precision2double")
endif()

if(NOT BUILD_SHARED_LIBS)
add_executable(custom_dir_EXIF_231)
target_sources(custom_dir_EXIF_231 PRIVATE custom_dir_EXIF_231.c)
target_link_libraries(custom_dir_EXIF_231 PRIVATE tiff port)
add_test(NAME "custom_dir_EXIF_231"
         COMMAND "custom_dir_EXIF_231")
endif()

add_executable(defer_strile_loading)
target_sources(defer_strile_loading PRIVATE defer_strile_loading.c)
target_link_libraries(defer_strile_loading PRIVATE tiff port)

add_executable(defer_strile_writing)
target_sources(defer_strile_writing PRIVATE defer_strile_writing.c)
target_link_libraries(defer_strile_writing PRIVATE tiff port)

add_executable(testtypes)
target_sources(testtypes PRIVATE testtypes.c)
target_link_libraries(testtypes PRIVATE tiff port)

if(WEBP_SUPPORT AND EMSCRIPTEN)
  # Emscripten is pretty finnicky about linker flags.
  # It needs --shared-memory if and only if atomics or bulk-memory is used.
  foreach(target ascii_tag
                 custom_dir
                 defer_strile_loading
                 defer_strile_writing
                 long_tag
                 rewrite
                 short_tag
                 strip_rw)
    target_link_options(${target} PUBLIC "-Wl,--shared-memory")
  endforeach()
  if(JPEG_SUPPORT)
    target_link_options(raw_decode PUBLIC "-Wl,--shared-memory")
  endif()
endif()

set(TEST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/output")
file(MAKE_DIRECTORY "${TEST_OUTPUT}")

set(tiff_test_extra_args
    "-DTIFFCP=$<TARGET_FILE:tiffcp>"
    "-DTIFFINFO=$<TARGET_FILE:tiffinfo>"
    "-DTIFFSPLIT=$<TARGET_FILE:tiffsplit>"
    "-DLIBTIFF=$<TARGET_FILE:tiff>")
if(WIN32)
  list(APPEND tiff_test_extra_args "-DWIN32=${WIN32}")
endif()
if(CYGWIN)
  list(APPEND tiff_test_extra_args "-DCYGWIN=${CYGWIN}")
endif()

macro(tiff_test_convert name command1 command2 command3 infile outfile validate)
  add_test(NAME "${name}"
           COMMAND "${CMAKE_COMMAND}"
           "-DCONVERT_COMMAND1=${command1}"
           "-DCONVERT_COMMAND2=${command2}"
           "-DCONVERT_COMMAND3=${command3}"
           "-DINFILE=${infile}"
           "-DOUTFILE=${outfile}"
           "-DVALIDATE=${validate}"
           ${tiff_test_extra_args}
           -P "${CMAKE_CURRENT_SOURCE_DIR}/TiffTest.cmake")
endmacro()

macro(tiff_test_stdout name command infile outfile)
  add_test(NAME "${name}"
           COMMAND "${CMAKE_COMMAND}"
           "-DSTDOUT_COMMAND=${command}"
           "-DINFILE=${infile}"
           "-DOUTFILE=${outfile}"
           ${tiff_test_extra_args}
           -P "${CMAKE_CURRENT_SOURCE_DIR}/TiffTest.cmake")
endmacro()

macro(tiff_test_reader name command infile)
  add_test(NAME "${name}"
           COMMAND "${CMAKE_COMMAND}"
           "-DREADER_COMMAND=${command}"
           "-DINFILE=${infile}"
           ${tiff_test_extra_args}
           -P "${CMAKE_CURRENT_SOURCE_DIR}/TiffTest.cmake")
endmacro()

macro(add_convert_test_multi commandname1 commandname2 commandname3
      categoryname commandargs1 commandargs2 commandargs3
      image validate)
  string(REPLACE " " "^" escaped_commandargs1 "${commandargs1}")
  string(REPLACE " " "^" escaped_commandargs2 "${commandargs2}")
  string(REPLACE " " "^" escaped_commandargs3 "${commandargs3}")
  get_filename_component(name "${image}" NAME)
  get_filename_component(base "${image}" NAME_WE)
  set(testname "${commandname1}-${categoryname}-${base}")
  if(commandname1)
    set(command1
        "$<TARGET_FILE:${commandname1}>^${escaped_commandargs1}")
  else()
    set(command1)
  endif()
  if(commandname2)
    set(command2
        "$<TARGET_FILE:${commandname2}>^${escaped_commandargs2}")
  else()
    set(command2)
  endif()
  if(commandname3)
    set(command3
        "$<TARGET_FILE:${commandname3}>^${escaped_commandargs3}")
  else()
    set(command3)
  endif()
  set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${image}")
  set(outfile "${TEST_OUTPUT}/${commandname1}-${categoryname}-${name}")
  string(REGEX REPLACE "\\.tiff\$" "" name "${name}")
  tiff_test_convert("${testname}" "${command1}" "${command2}" "${command3}"
                    "${infile}" "${outfile}" "${validate}")
endmacro()

macro(add_convert_test commandname
      categoryname commandargs
      image validate)
    add_convert_test_multi("${commandname}" "" ""
                           "${categoryname}" "${commandargs}" "" ""
                           "${image}" "${validate}")
endmacro()

macro(add_convert_tests_multi commandname commandname2 commandname3
      categoryname
      commandargs1 commandargs2 commandargs3
      images validate)
  foreach(file ${${images}})
    add_convert_test_multi("${commandname1}" "${commandname2}"
                           "${commandname3}" "${categoryname}"
                           "${commandargs1}" "${commandargs2}"
                           "${commandargs3}" "${file}" "${validate}")
  endforeach()
endmacro()

macro(add_convert_tests commandname categoryname commandargs images validate)
  foreach(file ${${images}})
    add_convert_test("${commandname}" "${categoryname}"
                     "${commandargs}" "${file}" "${validate}")
  endforeach()
endmacro()

macro(add_stdout_test commandname commandargs image)
  string(REPLACE " " "^" escaped_commandargs "${commandargs}")
  get_filename_component(name "${image}" NAME)
  get_filename_component(base "${image}" NAME_WE)
  set(testname "${commandname}-${base}")
  set(command "$<TARGET_FILE:${commandname}>^${escaped_commandargs}")
  set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${image}")
  set(outfile "${TEST_OUTPUT}/${commandname}-${name}")
  string(REGEX REPLACE "\\.tiff\$" "" name "${name}")
  tiff_test_stdout("${testname}" "${command}" "${infile}" "${outfile}")
endmacro()

macro(add_reader_test commandname commandargs image)
  string(REPLACE " " "^" escaped_commandargs "${commandargs}")
  get_filename_component(name "${image}" NAME)
  get_filename_component(base "${image}" NAME_WE)
  set(testname "${commandname}-${base}")
  set(command "$<TARGET_FILE:${commandname}>^${escaped_commandargs}")
  set(infile "${CMAKE_CURRENT_SOURCE_DIR}/${image}")
  string(REGEX REPLACE "\\.tiff\$" "" name "${name}")
  tiff_test_reader("${testname}" "${command}" "${infile}")
endmacro()

# BMP
add_convert_test(bmp2tiff palette "" "images/palette-1c-8b.bmp" TRUE)
add_convert_test(bmp2tiff rgb     "" "images/rgb-3c-8b.bmp"     TRUE)

# GIF
add_convert_test(gif2tiff palette "" "images/palette-1c-8b.gif" TRUE)

# PPM
add_convert_test(ppm2tiff miniswhite "" "images/miniswhite-1c-1b.pbm" TRUE)
add_convert_test(ppm2tiff minisblack "" "images/minisblack-1c-8b.pgm" TRUE)
add_convert_test(ppm2tiff rgb        "" "images/rgb-3c-16b.ppm"       TRUE)
add_convert_test(ppm2tiff rgb        "" "images/rgb-3c-8b.ppm"        TRUE)

# tiffcp
add_convert_test(tiffcp   g3         "-c g3"         "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   g31d       "-c g3:1d"      "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   g31dfill   "-c g3:1d:fill" "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   g32d       "-c g3:2d"      "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   g32dfill   "-c g3:2d:fill" "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   g4         "-c g4"         "images/miniswhite-1c-1b.tiff" FALSE)
add_convert_test(tiffcp   none       "-c none"       "images/quad-lzw-compat.tiff" FALSE)
add_convert_test(tiffcp   noner1     "-c none -r 1"  "images/lzw-single-strip.tiff" FALSE)
add_convert_test_multi(tiffcp tiffcp "" logluv "-c none" "-c sgilog" ""
                       "images/logluv-3c-16b.tiff"    FALSE)
add_convert_test_multi(tiffcp thumbnail "" thumbnail "g3:1d" "" ""
                       "images/miniswhite-1c-1b.tiff"    FALSE)

# tiffdump
add_reader_test(tiffdump "" "images/miniswhite-1c-1b.tiff")

# tiffinfo
add_reader_test(tiffinfo "-c -D -d -j -s" "images/minisblack-1c-16b.tiff")

# tiffcp split/join
foreach(image ${UNCOMPRESSEDIMAGES})
  list(APPEND ESCAPED_UNCOMPRESSED "${CMAKE_CURRENT_SOURCE_DIR}/${image}")
endforeach()
string(REPLACE ";" "^" ESCAPED_UNCOMPRESSED "${ESCAPED_UNCOMPRESSED}")
add_test(NAME "tiffcp-split"
         COMMAND "${CMAKE_COMMAND}"
         "-DTESTFILES=${ESCAPED_UNCOMPRESSED}"
         "-DCONJOINED=${TEST_OUTPUT}/tiffcp-split-conjoined.tif"
         "-DSPLITFILE=${TEST_OUTPUT}/tiffcp-split-split-"
         ${tiff_test_extra_args}
         -P "${CMAKE_CURRENT_SOURCE_DIR}/TiffSplitTest.cmake")
add_test(NAME "tiffcp-split-join"
         COMMAND "${CMAKE_COMMAND}"
         "-DTESTFILES=${ESCAPED_UNCOMPRESSED}"
         "-DCONJOINED=${TEST_OUTPUT}/tiffcp-split-join-conjoined.tif"
         "-DSPLITFILE=${TEST_OUTPUT}/tiffcp-split-join-split-"
         "-DRECONJOINED=${TEST_OUTPUT}/tiffcp-split-join-reconjoined.tif"
         ${tiff_test_extra_args}
         -P "${CMAKE_CURRENT_SOURCE_DIR}/TiffSplitTest.cmake")

# PDF
add_stdout_test(tiff2pdf "" "images/miniswhite-1c-1b.tiff" TRUE)

# RGBA
add_convert_tests(tiff2rgba default    ""                         TIFFIMAGES TRUE)
# Test rotations
add_convert_tests(tiffcrop  R90        "-R90"                     TIFFIMAGES TRUE)
# Test flip (mirror)
add_convert_tests(tiffcrop  doubleflip "-F both"                  TIFFIMAGES TRUE)
# Test extracting a section 60 pixels wide and 60 pixels high
add_convert_tests(tiffcrop  extract    "-U px -E top -X 60 -Y 60" TIFFIMAGES TRUE)
# Test extracting the first and fourth quarters from the left side.
add_convert_tests(tiffcrop  extractz14 "-E left -Z1:4,2:4"        TIFFIMAGES TRUE)

# test types
add_test(NAME "testtypes"
         COMMAND "testtypes")
