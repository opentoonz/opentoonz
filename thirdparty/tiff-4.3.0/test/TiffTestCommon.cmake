# CMake tests for libtiff (common functionality)
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

#
# Test a simple convert-like command.
#
# test_convert command infile outfile
macro(test_convert command infile outfile)
  file(TO_NATIVE_PATH "${infile}" native_infile)
  file(TO_NATIVE_PATH "${outfile}" native_outfile)
  file(REMOVE "${outfile}")
  message(STATUS Running "${MEMCHECK} ${command} ${native_infile} ${native_outfile}")
  execute_process(COMMAND ${MEMCHECK} ${command} "${native_infile}" "${native_outfile}"
                  RESULT_VARIABLE TEST_STATUS)
  if(TEST_STATUS)
    message(FATAL_ERROR "Returned failed status ${TEST_STATUS}!  Output (if any) is in \"${native_outfile}\"")
  endif()
endmacro()

#
# Test a simple convert-like command.
#
# test_convert command infile outfile
macro(test_convert_multi command infile outfile)
  foreach(file ${infile})
    file(TO_NATIVE_PATH "${file}" native_file)
    list(APPEND native_infile "${native_file}")
  endforeach()
  file(TO_NATIVE_PATH "${outfile}" native_outfile)
  file(REMOVE "${outfile}")
  message(STATUS Running "${MEMCHECK} ${command} ${native_infile} ${native_outfile}")
  execute_process(COMMAND ${MEMCHECK} ${command} ${native_infile} "${native_outfile}"
                  RESULT_VARIABLE TEST_STATUS)
  if(TEST_STATUS)
    message(FATAL_ERROR "Returned failed status ${TEST_STATUS}!  Output (if any) is in \"${native_outfile}\"")
  endif()
endmacro()
#
# Test a simple command which sends output to stdout
#
# test_stdout command infile outfile
macro(test_stdout command infile outfile)
  file(TO_NATIVE_PATH "${infile}" native_infile)
  file(TO_NATIVE_PATH "${outfile}" native_outfile)
  file(REMOVE "${outfile}")
  message(STATUS "Running ${MEMCHECK} ${command} ${native_infile} > ${native_outfile}")
  execute_process(COMMAND ${MEMCHECK} ${command} "${native_infile}"
                  OUTPUT_FILE "${outfile}"
                  RESULT_VARIABLE TEST_STATUS)
  if(TEST_STATUS)
    message(FATAL_ERROR "Returned failed status ${TEST_STATUS}!  Output (if any) is in \"${native_outfile}")
  endif()
endmacro()

#
# Execute a simple command (e.g. tiffinfo) with one input file
#
# test_exec command infile
macro(test_reader command infile)
  file(TO_NATIVE_PATH "${infile}" native_infile)
  message(STATUS "Running ${MEMCHECK} ${command} ${native_infile}")
  execute_process(COMMAND ${MEMCHECK} ${command} "${native_infile}"
                  RESULT_VARIABLE TEST_STATUS)
  if(TEST_STATUS)
    message(FATAL_ERROR "Returned failed status ${TEST_STATUS}!  Output (if any) is in \"${native_outfile}")
  endif()
endmacro()

#
# Execute tiffinfo on a specified file to validate it
#
# tiffinfo_validate infile
macro(tiffinfo_validate file)
  test_reader("${TIFFINFO};-D" "${file}")
endmacro()

# Add the directory containing libtiff to the PATH (Windows only)
if(WIN32)
  get_filename_component(LIBTIFF_DIR "${LIBTIFF}" DIRECTORY)
  file(TO_NATIVE_PATH "${LIBTIFF_DIR}" LIBTIFF_DIR)
  set(ENV{PATH} "${LIBTIFF_DIR};$ENV{PATH}")
endif()
if(CYGWIN)
  get_filename_component(LIBTIFF_DIR "${LIBTIFF}" DIRECTORY)
  file(TO_NATIVE_PATH "${LIBTIFF_DIR}" LIBTIFF_DIR)
  set(ENV{PATH} "${LIBTIFF_DIR}:$ENV{PATH}")
endif()
