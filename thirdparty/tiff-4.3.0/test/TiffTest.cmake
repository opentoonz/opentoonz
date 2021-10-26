# CMake tests for libtiff
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

include(${CMAKE_CURRENT_LIST_DIR}/TiffTestCommon.cmake)

string(REPLACE "^" ";" CONVERT_COMMAND1 "${CONVERT_COMMAND1}")
string(REPLACE "^" ";" CONVERT_COMMAND2 "${CONVERT_COMMAND2}")
string(REPLACE "^" ";" CONVERT_COMMAND3 "${CONVERT_COMMAND3}")
string(REPLACE "^" ";" STDOUT_COMMAND "${STDOUT_COMMAND}")
string(REPLACE "^" ";" READER_COMMAND "${READER_COMMAND}")

if(CONVERT_COMMAND1)
  test_convert("${CONVERT_COMMAND1}" "${INFILE}" "${OUTFILE}")
  if(VALIDATE)
    tiffinfo_validate("${OUTFILE}")
  endif()
endif()

get_filename_component(base "${OUTFILE}" NAME)
get_filename_component(ext "${OUTFILE}" EXT)

if(CONVERT_COMMAND2)
  test_convert("${CONVERT_COMMAND2}" "${OUTFILE}" "${base}-2${ext}")
  if(VALIDATE)
    tiffinfo_validate("${base}-2${ext}")
  endif()
endif()

if(CONVERT_COMMAND3)
  test_convert("${CONVERT_COMMAND3}" "${base}-2${ext}" "${base}-3${ext}")
  if(VALIDATE)
    tiffinfo_validate("${base}-3${ext}")
  endif()
endif()

if(STDOUT_COMMAND)
  test_stdout("${STDOUT_COMMAND}" "${INFILE}" "${OUTFILE}")
endif()

if(READER_COMMAND)
  test_reader("${READER_COMMAND}" "${INFILE}")
endif()
