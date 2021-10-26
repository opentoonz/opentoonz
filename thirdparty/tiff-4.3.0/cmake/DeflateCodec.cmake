# Checks for deflate codec support
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


# ZLIB
set(ZLIB_SUPPORT FALSE)
find_package(ZLIB)
option(zlib "use zlib (required for Deflate compression)" ${ZLIB_FOUND})
if(zlib AND ZLIB_FOUND)
    set(ZLIB_SUPPORT TRUE)
endif()
set(ZIP_SUPPORT ${ZLIB_SUPPORT})

# libdeflate
set(LIBDEFLATE_SUPPORT FALSE)
find_package(Deflate)
option(libdeflate "use libdeflate (optional for faster Deflate support, still requires zlib)" ${Deflate_FOUND})
if (libdeflate AND Deflate_FOUND AND ZIP_SUPPORT)
    set(LIBDEFLATE_SUPPORT TRUE)
endif()
if(Deflate_FOUND AND NOT ZIP_SUPPORT)
    message(WARNING "libdeflate available but zlib is not. libdeflate cannot be used")
endif()
