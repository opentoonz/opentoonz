# Checks for JPEG codec support
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


# JPEG
set(JPEG_SUPPORT FALSE)
find_package(JPEG)
option(jpeg "use libjpeg (required for JPEG compression)" ${JPEG_FOUND})
if (jpeg AND JPEG_FOUND)
    set(JPEG_SUPPORT TRUE)
endif()

# Old-jpeg
set(OJPEG_SUPPORT FALSE)
option(old-jpeg "support for Old JPEG compression (read-only)" ${JPEG_SUPPORT})
if (old-jpeg AND JPEG_SUPPORT)
    set(OJPEG_SUPPORT TRUE)
endif()

# 8/12-bit jpeg mode
set(JPEG12_INCLUDE_DIR JPEG12_INCLUDE_DIR-NOTFOUND CACHE PATH "Include directory for 12-bit libjpeg")
set(JPEG12_LIBRARY JPEG12_LIBRARY-NOTFOUND CACHE FILEPATH "12-bit libjpeg library")
set(JPEG_DUAL_MODE_8_12 FALSE)
if (JPEG12_INCLUDE_DIR AND JPEG12_LIBRARY)
    set(JPEG12_LIBRARIES ${JPEG12_LIBRARY})
    set(JPEG12_FOUND TRUE)
else()
    set(JPEG12_FOUND FALSE)
endif()
option(jpeg12 "enable libjpeg 8/12-bit dual mode (requires separate 12-bit libjpeg build)" ${JPEG12_FOUND})
if (jpeg12 AND JPEG12_FOUND)
    set(JPEG_DUAL_MODE_8_12 TRUE)
    set(LIBJPEG_12_PATH "${JPEG12_INCLUDE_DIR}/jpeglib.h")
endif()
