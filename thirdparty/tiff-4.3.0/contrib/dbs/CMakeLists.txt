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

add_executable(tiff-bi tiff-bi.c)
target_link_libraries(tiff-bi tiff port)

add_executable(tiff-grayscale tiff-grayscale.c)
target_link_libraries(tiff-grayscale tiff port CMath::CMath)

add_executable(tiff-palette tiff-palette.c)
target_link_libraries(tiff-palette tiff port)

add_executable(tiff-rgb tiff-rgb.c)
target_link_libraries(tiff-rgb tiff port CMath::CMath)

if(WEBP_SUPPORT AND EMSCRIPTEN)
  # Emscripten is pretty finnicky about linker flags.
  # It needs --shared-memory if and only if atomics or bulk-memory is used.
  foreach(target tiff-bi
                 tiff-grayscale
                 tiff-palette
                 tiff-rgb)
    target_link_options(${target} PUBLIC "-Wl,--shared-memory")
  endforeach()
endif()
