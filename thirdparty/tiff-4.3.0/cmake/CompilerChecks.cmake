# Compiler feature checks
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


include(CheckCCompilerFlag)


# These are annoyingly verbose, produce false positives or don't work
# nicely with all supported compiler versions, so are disabled unless
# explicitly enabled.
option(extra-warnings "Enable extra compiler warnings" OFF)

# This will cause the compiler to fail when an error occurs.
option(fatal-warnings "Compiler warnings are errors" OFF)

# Check if the compiler supports each of the following additional
# flags, and enable them if supported.  This greatly improves the
# quality of the build by checking for a number of common problems,
# some of which are quite serious.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(test_flags
            -Wall
            -Winline
            -Wformat-security
            -Wpointer-arith
            -Wdisabled-optimization
            -Wno-unknown-pragmas
            -fstrict-aliasing)
    if(extra-warnings)
        list(APPEND test_flags
                -pedantic
                -Wextra
                -Wformat
                -Wformat-overflow
                -Wformat-nonliteral
                -Wformat-signedness
                -Wformat-truncation
                -Wdeclaration-after-statement
                -Wconversion
                -Wsign-conversion
                -Wnull-dereference
                -Wdouble-promotion
                -Wmisleading-indentation
                -Wmissing-include-dirs
                -Wswitch-default
                -Wswitch-enum
                -Wunused-local-typedefs
                -Wunused-parameter
                -Wuninitialized
                -Warith-conversion
                -Wbool-operation
                -Wduplicated-branches
                -Wduplicated-cond
                -Wshadow
                -Wunused-macros
                -Wc99-c11-compat
                -Wcast-qual
                -Wcast-align
                -Wwrite-strings
                -Wdangling-else
                -Wsizeof-array-div
                -Wsizeof-pointer-div
                -Wsizeof-pointer-memaccess
                -Wlogical-op
                -Wlogical-not-parentheses
                -Wstrict-prototypes
                -Wmissing-declarations
                -Wredundant-decls
                -Wno-int-to-pointer-cast
                -Wfloat-equal
                -Wfloat-conversion
                -Wmissing-prototypes
                -Wunreachable-code)
    endif()
    if(fatal-warnings)
        list(APPEND test_flags
                -Werror)
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(test_flags)
    if(extra-warnings)
        list(APPEND test_flags
                /W4)
    else()
        list(APPEND test_flags
                /W3)
    endif()
    if (fatal-warnings)
        list(APPEND test_flags
                /WX)
    endif()
endif()

foreach(flag ${test_flags})
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" flag_var "${flag}")
    set(test_c_flag "C_FLAG${flag_var}")
    CHECK_C_COMPILER_FLAG(${flag} "${test_c_flag}")
    if (${test_c_flag})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    endif (${test_c_flag})
endforeach(flag ${test_flags})
