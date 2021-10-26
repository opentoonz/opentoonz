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

add_executable(fax2ps)
target_sources(fax2ps PRIVATE fax2ps.c)
target_link_libraries(fax2ps PRIVATE tiff port)

add_executable(fax2tiff)
target_sources(fax2tiff PRIVATE fax2tiff.c)
target_link_libraries(fax2tiff PRIVATE tiff port)

add_executable(pal2rgb)
target_sources(pal2rgb PRIVATE pal2rgb.c)
target_link_libraries(pal2rgb PRIVATE tiff port)

add_executable(ppm2tiff)
target_sources(ppm2tiff PRIVATE ppm2tiff.c)
target_link_libraries(ppm2tiff PRIVATE tiff port)

add_executable(raw2tiff)
target_sources(raw2tiff PRIVATE raw2tiff.c)
target_link_libraries(raw2tiff PRIVATE tiff port CMath::CMath)

add_executable(rgb2ycbcr)
target_sources(rgb2ycbcr PRIVATE rgb2ycbcr.c)
target_link_libraries(rgb2ycbcr PRIVATE tiff port)

add_executable(thumbnail)
target_sources(thumbnail PRIVATE thumbnail.c)
target_link_libraries(thumbnail PRIVATE tiff port CMath::CMath)

add_executable(tiff2bw)
target_sources(tiff2bw PRIVATE tiff2bw.c)
target_link_libraries(tiff2bw PRIVATE tiff port)

add_executable(tiff2pdf)
target_sources(tiff2pdf PRIVATE tiff2pdf.c)
target_link_libraries(tiff2pdf PRIVATE tiff port)

add_executable(tiff2ps)
target_sources(tiff2ps PRIVATE tiff2ps.c)
target_link_libraries(tiff2ps PRIVATE tiff port CMath::CMath)

add_executable(tiff2rgba)
target_sources(tiff2rgba PRIVATE tiff2rgba.c)
target_link_libraries(tiff2rgba PRIVATE tiff port)

add_executable(tiffcmp)
target_sources(tiffcmp PRIVATE tiffcmp.c)
target_link_libraries(tiffcmp PRIVATE tiff port)

add_executable(tiffcp)
target_sources(tiffcp PRIVATE tiffcp.c)
target_link_libraries(tiffcp PRIVATE tiff port)

add_executable(tiffcrop)
target_sources(tiffcrop PRIVATE tiffcrop.c)
target_link_libraries(tiffcrop PRIVATE tiff port CMath::CMath)

add_executable(tiffdither)
target_sources(tiffdither PRIVATE tiffdither.c)
target_link_libraries(tiffdither PRIVATE tiff port)

add_executable(tiffdump)
target_sources(tiffdump PRIVATE tiffdump.c)
target_link_libraries(tiffdump PRIVATE tiff port)

add_executable(tiffinfo)
target_sources(tiffinfo PRIVATE tiffinfo.c)
target_link_libraries(tiffinfo PRIVATE tiff port)

add_executable(tiffmedian)
target_sources(tiffmedian PRIVATE tiffmedian.c)
target_link_libraries(tiffmedian PRIVATE tiff port)

add_executable(tiffset)
target_sources(tiffset PRIVATE tiffset.c)
target_link_libraries(tiffset PRIVATE tiff port)

add_executable(tiffsplit)
target_sources(tiffsplit PRIVATE tiffsplit.c)
target_link_libraries(tiffsplit PRIVATE tiff port)

# rgb2ycbcr and thumbnail are intended to *NOT* be installed. They are for
# testing purposes only.
install(TARGETS fax2ps
                fax2tiff
                pal2rgb
                ppm2tiff
                raw2tiff
                tiff2bw
                tiff2pdf
                tiff2ps
                tiff2rgba
                tiffcmp
                tiffcp
                tiffcrop
                tiffdither
                tiffdump
                tiffinfo
                tiffmedian
                tiffset
                tiffsplit
        RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

if(HAVE_OPENGL)
  add_executable(tiffgt)
  target_sources(tiffgt PRIVATE tiffgt.c)
  target_link_libraries(tiffgt PRIVATE tiff port)
  target_link_libraries(tiffgt PRIVATE OpenGL::GL GLUT::GLUT)

  install(TARGETS tiffgt
          RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif()

if(WEBP_SUPPORT AND EMSCRIPTEN)
  # Emscripten is pretty finnicky about linker flags.
  # It needs --shared-memory if and only if atomics or bulk-memory is used.
  foreach(target fax2ps
                 fax2tiff
                 pal2rgb
                 ppm2tiff
                 raw2tiff
                 rgb2ycbcr
                 thumbnail
                 tiff2bw
                 tiff2pdf
                 tiff2ps
                 tiff2rgba
                 tiffcmp
                 tiffcp
                 tiffcrop
                 tiffdither
                 tiffdump
                 tiffinfo
                 tiffmedian
                 tiffset
                 tiffsplit)
    target_link_options(${target} PUBLIC "-Wl,--shared-memory")
  endforeach()
endif()
