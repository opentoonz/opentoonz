# Checks for OpenGL and GLUT support
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

# OpenGL and GLUT
set(OpenGL_GL_PREFERENCE LEGACY)

find_package(OpenGL COMPONENTS OpenGL)
find_package(GLUT)

set(HAVE_OPENGL FALSE)
if(OPENGL_FOUND AND OPENGL_GLU_FOUND AND GLUT_FOUND)
    set(HAVE_OPENGL TRUE)
endif()

# Purely to satisfy the generated headers:
check_include_file(GL/gl.h HAVE_GL_GL_H)
check_include_file(GL/glu.h HAVE_GL_GLU_H)
check_include_file(GL/glut.h HAVE_GL_GLUT_H)
check_include_file(GLUT/glut.h HAVE_GLUT_GLUT_H)
check_include_file(OpenGL/gl.h HAVE_OPENGL_GL_H)
check_include_file(OpenGL/glu.h HAVE_OPENGL_GLU_H)
