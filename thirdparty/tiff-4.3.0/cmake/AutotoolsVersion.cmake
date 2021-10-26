# Read version information from configure.ac.
#
# Copyright © 2015 Open Microscopy Environment / University of Dundee
# Copyright © 2021 Roger Leigh <rleigh@codelibre.net>
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


# Get version from configure.ac
FILE(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/configure.ac" configure REGEX "^LIBTIFF_.*=")
foreach(line ${configure})
    foreach(var LIBTIFF_MAJOR_VERSION LIBTIFF_MINOR_VERSION LIBTIFF_MICRO_VERSION LIBTIFF_ALPHA_VERSION
            LIBTIFF_CURRENT LIBTIFF_REVISION LIBTIFF_AGE)
        if(NOT ${var} AND line MATCHES "^${var}=(.*)")
            set(${var} "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
endforeach()

# Package version
set(LIBTIFF_VERSION "${LIBTIFF_MAJOR_VERSION}.${LIBTIFF_MINOR_VERSION}.${LIBTIFF_MICRO_VERSION}")
set(LIBTIFF_VERSION_FULL "${LIBTIFF_VERSION}${LIBTIFF_ALPHA_VERSION}")

# Convert the libtool version variables to proper major and minor versions
math(EXPR SO_MAJOR "${LIBTIFF_CURRENT} - ${LIBTIFF_AGE}")
set(SO_MINOR "${LIBTIFF_AGE}")
set(SO_REVISION "${LIBTIFF_REVISION}")

# Library version (unlike libtool's baroque scheme, WYSIWYG here)
set(SO_COMPATVERSION "${SO_MAJOR}")
set(SO_VERSION "${SO_MAJOR}.${SO_MINOR}.${SO_REVISION}")
