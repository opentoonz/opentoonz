dnl                                               -*- Autoconf -*-
dnl Tag Image File Format (TIFF) Software
dnl
dnl Copyright (C) 2004, Andrey Kiselev <dron@ak4719.spb.edu>
dnl
dnl Permission to use, copy, modify, distribute, and sell this software and 
dnl its documentation for any purpose is hereby granted without fee, provided
dnl that (i) the above copyright notices and this permission notice appear in
dnl all copies of the software and related documentation, and (ii) the names of
dnl Sam Leffler and Silicon Graphics may not be used in any advertising or
dnl publicity relating to the software without the specific, prior written
dnl permission of Sam Leffler and Silicon Graphics.
dnl 
dnl THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
dnl EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
dnl WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
dnl 
dnl IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
dnl ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
dnl OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
dnl WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
dnl LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
dnl OF THIS SOFTWARE.

dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.64)
AC_INIT([LibTIFF Software],[4.3.0],[tiff@lists.maptools.org],[tiff])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_LANG(C)

dnl Compute the canonical host (run-time) system type variable
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(tar-ustar)
dnl Do not rebuild generated files every time
AM_MAINTAINER_MODE

dnl Versioning.
dnl Don't fill the ALPHA_VERSION field, if not applicable.
LIBTIFF_MAJOR_VERSION=4
LIBTIFF_MINOR_VERSION=3
LIBTIFF_MICRO_VERSION=0
LIBTIFF_ALPHA_VERSION=
LIBTIFF_VERSION=$LIBTIFF_MAJOR_VERSION.$LIBTIFF_MINOR_VERSION.$LIBTIFF_MICRO_VERSION$LIBTIFF_ALPHA_VERSION
dnl This will be used with the 'make release' target
LIBTIFF_RELEASE_DATE=`date +"%Y%m%d"`

dnl Libtool library revision control info
dnl See the libtool documentation under the heading "Libtool's versioning
dnl system" in order to understand the meaning of these fields
dnl
dnl current
dnl      The most recent interface number that this library implements.
dnl revision
dnl      The implementation number of the current interface.
dnl age
dnl      The difference between the newest and oldest interfaces that
dnl      this library implements. In other words, the library implements
dnl      all the interface numbers in the range from number current -
dnl      age to current.
dnl
dnl Here are a set of rules to help you update your library version
dnl information:
dnl
dnl  1. Start with version information of `0:0:0' for each libtool library.
dnl  2. Update the version information only immediately before a public
dnl     release of your software. More frequent updates are unnecessary, and
dnl     only guarantee that the current interface number gets larger faster.
dnl  3. If the library source code has changed at all since the last update,
dnl     then increment revision (`c:r:a' becomes `c:r+1:a').
dnl  4. If any interfaces have been added, removed, or changed since the last
dnl     update, increment current, and set revision to 0.
dnl  5. If any interfaces have been added since the last public release, then
dnl     increment age.
dnl  6. If any interfaces have been removed since the last public release,
dnl     then set age to 0.                                
LIBTIFF_CURRENT=12
LIBTIFF_REVISION=0
LIBTIFF_AGE=7
LIBTIFF_VERSION_INFO=$LIBTIFF_CURRENT:$LIBTIFF_REVISION:$LIBTIFF_AGE

# This is a special hack for OpenBSD and MirOS systems. The dynamic linker
# in OpenBSD uses some special semantics for shared libraries. Their soname
# contains only two numbers, major and minor.
# See http://bugzilla.remotesensing.org/show_bug.cgi?id=838 for details.
#case "$host_os" in
#    openbsd* | mirbsd*)
#	LIBTIFF_VERSION_INFO=$LIBTIFF_MAJOR_VERSION$LIBTIFF_MINOR_VERSION:$LIBTIFF_MICRO_VERSION:0
#	;;
#    *)
#	LIBTIFF_VERSION_INFO=$LIBTIFF_MAJOR_VERSION:$LIBTIFF_MINOR_VERSION:$LIBTIFF_MICRO_VERSION
#	;;
#esac

AC_SUBST(LIBTIFF_MAJOR_VERSION)
AC_SUBST(LIBTIFF_MINOR_VERSION)
AC_SUBST(LIBTIFF_MICRO_VERSION)
AC_SUBST(LIBTIFF_ALPHA_VERSION)
AC_SUBST(LIBTIFF_VERSION)
AC_SUBST(LIBTIFF_VERSION_INFO)
AC_SUBST(LIBTIFF_RELEASE_DATE)

dnl Checks for programs.
AC_PROG_CC_C99
AM_PROG_CC_C_O

dnl We want warnings. As many warnings as possible.
VL_PROG_CC_WARNINGS()

dnl Checks for programs
AC_PROG_INSTALL
AC_PROG_LN_S
# Used only for validating the source distribution during distcheck
AC_PATH_PROG(CMAKE, cmake)


# Check if LD supports linker scripts, and define automake conditional
# HAVE_LD_VERSION_SCRIPT if so.  This functionality is currently
# constrained to compilers using GNU ld on ELF systems or systems
# which provide an adequate emulation thereof.
AC_ARG_ENABLE([ld-version-script],
  AS_HELP_STRING([--enable-ld-version-script],
    [enable linker version script (default is disabled)]),
    [have_ld_version_script=$enableval], [have_ld_version_script=no])
if test "$have_ld_version_script" != no; then
  AC_MSG_CHECKING([if LD -Wl,--version-script works])
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -Wl,--version-script=conftest.map"
  cat > conftest.map <<EOF
VERS_1 {
        global: sym;
};

VERS_2 {
        global: sym;
} VERS_1;
EOF
  AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                 [have_ld_version_script=yes], [have_ld_version_script=no])
  rm -f conftest.map
  LDFLAGS="$save_LDFLAGS"
  AC_MSG_RESULT($have_ld_version_script)
fi
AM_CONDITIONAL(HAVE_LD_VERSION_SCRIPT, test "$have_ld_version_script" = "yes")


dnl Tests for Windows
AC_EXEEXT
AC_OBJEXT

dnl initialize libtool
LT_INIT([win32-dll])
LT_LANG([C++])

# Enable support for silent build rules
AM_SILENT_RULES

tiff_libs_private=
AC_SUBST(tiff_libs_private)

dnl We don't need to add math library to all targets
case "${host_os}" in
    cygwin* | mingw32* | beos* | darwin*)
        ;;
    *)
	AC_CHECK_LIB(m,sin,[libm_lib=yes], [libm_lib=no],)
	if test "x$libm_lib" = "xyes" ; then
	  LIBS="-lm $LIBS"
	  tiff_libs_private="-lm ${tiff_libs_private}"
	fi
        ;;
esac

dnl Checks for header files.
AC_CHECK_HEADERS([assert.h fcntl.h io.h search.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl If the `fseeko' function is available, define `HAVE_FSEEKO'. Define
dnl `_LARGEFILE_SOURCE' if necessary.
AC_FUNC_FSEEKO

dnl Check if optarg (and presumably related externs) already declared in headers
AC_CHECK_DECLS([optarg])

dnl ---------------------------------------------------------------------------
dnl Deprecated features and backward compatibilty
dnl ---------------------------------------------------------------------------

# Disable deprecated features to ensure clean build
CPPFLAGS="-DTIFF_DISABLE_DEPRECATED $CPPFLAGS"

dnl ---------------------------------------------------------------------------
dnl Compute sized types for current CPU and compiler options
dnl ---------------------------------------------------------------------------

# Obtain the size of 'size_t' and define as SIZEOF_SIZE_T.  Result is
# available in ac_cv_sizeof_size_t
AC_CHECK_SIZEOF([size_t])


# C99 fixed-size integer types
INT8_T='int8_t'
UINT8_T='uint8_t'

INT16_T='int16_t'
UINT16_T='uint16_t'

INT32_T='int32_t'
UINT32_T='uint32_t'

INT64_T='int64_t'
UINT64_T='uint64_t'

if test $ac_cv_sizeof_size_t -eq 4
then
  SSIZE_T='int32_t'
elif test $ac_cv_sizeof_size_t -eq 8
then
  SSIZE_T='int64_t'
else
  AC_MSG_ERROR([Unsupported size_t size ${ac_cv_sizeof_size_t}; please add support])
fi


# Determine TIFF equivalent of ssize_t
AC_MSG_CHECKING(for signed size type)
AC_MSG_RESULT($SSIZE_T)
AC_DEFINE_UNQUOTED(TIFF_SSIZE_T,$SSIZE_T,[Signed size type])

dnl Checks for library functions.
AC_CHECK_FUNCS([mmap setmode])

dnl Will use local replacements for unavailable functions
AC_REPLACE_FUNCS(getopt)

dnl ---------------------------------------------------------------------------
dnl Check the native cpu bit order.
dnl ---------------------------------------------------------------------------
AC_MSG_CHECKING([native cpu bit order])
case "$host_cpu" in
    i*86*|x86_64*)
        HOST_FILLORDER=FILLORDER_LSB2MSB
	AC_MSG_RESULT([lsb2msb])
	;;
    *)
	HOST_FILLORDER=FILLORDER_MSB2LSB
	AC_MSG_RESULT([msb2lsb])
        ;;
esac
AC_DEFINE_UNQUOTED(HOST_FILLORDER, $HOST_FILLORDER, [Set the native cpu bit order (FILLORDER_LSB2MSB or FILLORDER_MSB2LSB)])

dnl ---------------------------------------------------------------------------
dnl Configure legacy tifconf.h HOST_BIGENDIAN.
dnl ---------------------------------------------------------------------------
if test "$ac_cv_c_bigendian" = yes ; then
    HOST_BIGENDIAN=1
else
    HOST_BIGENDIAN=0
fi
AC_DEFINE_UNQUOTED(HOST_BIGENDIAN,$HOST_BIGENDIAN,[Native cpu byte order: 1 if big-endian (Motorola) or 0 if little-endian (Intel)])

dnl ---------------------------------------------------------------------------
dnl Make the POSIX.2 features available.
dnl ---------------------------------------------------------------------------
#_POSIX_C_SOURCE=2
#AC_DEFINE_UNQUOTED(_POSIX_C_SOURCE, $_POSIX_C_SOURCE, [Define this macro to a positive integer to control which POSIX functionality is made available.])

dnl ---------------------------------------------------------------------------
dnl Set the floating point format.
dnl FIXME: write appropriate test.
dnl ---------------------------------------------------------------------------
HAVE_IEEEFP=1
AC_DEFINE_UNQUOTED(HAVE_IEEEFP, $HAVE_IEEEFP, [Define as 0 or 1 according to the floating point format supported by the machine])

dnl ---------------------------------------------------------------------------
dnl Enable run-time paths to libraries usage.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(rpath,
	      AS_HELP_STRING([--enable-rpath],
			     [Enable runtime linker paths (-R libtool option)]),
	      [HAVE_RPATH=$enableval], [HAVE_RPATH=no])
AM_CONDITIONAL(HAVE_RPATH, test "$HAVE_RPATH" = "yes")

dnl ---------------------------------------------------------------------------
dnl Support large files.
dnl ---------------------------------------------------------------------------

AC_SYS_LARGEFILE

dnl ---------------------------------------------------------------------------
dnl Point to path where we should install documentation.
dnl ---------------------------------------------------------------------------

LIBTIFF_DOCDIR=\${prefix}/share/doc/${PACKAGE}-${LIBTIFF_VERSION}

AC_ARG_WITH(docdir,
	    AS_HELP_STRING([--with-docdir=DIR],
			   [directory where documentation should be installed]),,)
if test "x$with_docdir" != "x" ; then
  LIBTIFF_DOCDIR=$with_docdir
fi
 
AC_SUBST(LIBTIFF_DOCDIR)



dnl ---------------------------------------------------------------------------
dnl Switch on/off internal codecs.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(ccitt,
	      AS_HELP_STRING([--disable-ccitt],
			     [disable support for CCITT Group 3 & 4 algorithms]),
	      [HAVE_CCITT=$enableval], [HAVE_CCITT=yes])

if test "$HAVE_CCITT" = "yes" ; then
  AC_DEFINE(CCITT_SUPPORT,1,[Support CCITT Group 3 & 4 algorithms])
fi

AC_ARG_ENABLE(packbits,
	      AS_HELP_STRING([--disable-packbits],
			     [disable support for Macintosh PackBits algorithm]),
	      [HAVE_PACKBITS=$enableval], [HAVE_PACKBITS=yes])

if test "$HAVE_PACKBITS" = "yes" ; then
  AC_DEFINE(PACKBITS_SUPPORT,1,[Support Macintosh PackBits algorithm])
fi

AC_ARG_ENABLE(lzw,
	      AS_HELP_STRING([--disable-lzw],
			     [disable support for LZW algorithm]),
	      [HAVE_LZW=$enableval], [HAVE_LZW=yes])

if test "$HAVE_LZW" = "yes" ; then
  AC_DEFINE(LZW_SUPPORT,1,[Support LZW algorithm])
fi

AC_ARG_ENABLE(thunder,
	      AS_HELP_STRING([--disable-thunder],
			     [disable support for ThunderScan 4-bit RLE algorithm]),
	      [HAVE_THUNDER=$enableval], [HAVE_THUNDER=yes])

if test "$HAVE_THUNDER" = "yes" ; then
  AC_DEFINE(THUNDER_SUPPORT,1,[Support ThunderScan 4-bit RLE algorithm])
fi

HAVE_NEXT=yes

AC_ARG_ENABLE(next,
	      AS_HELP_STRING([--disable-next],
			     [disable support for NeXT 2-bit RLE algorithm]),
	      [HAVE_NEXT=$enableval], [HAVE_NEXT=yes])

if test "$HAVE_NEXT" = "yes" ; then
  AC_DEFINE(NEXT_SUPPORT,1,[Support NeXT 2-bit RLE algorithm])
fi

AC_ARG_ENABLE(logluv,
	      AS_HELP_STRING([--disable-logluv],
			     [disable support for LogLuv high dynamic range encoding]),
	      [HAVE_LOGLUV=$enableval], [HAVE_LOGLUV=yes])

if test "$HAVE_LOGLUV" = "yes" ; then
  AC_DEFINE(LOGLUV_SUPPORT,1,[Support LogLuv high dynamic range encoding])
fi

dnl ---------------------------------------------------------------------------
dnl Switch on/off support for Microsoft Document Imaging
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(mdi,
	      AS_HELP_STRING([--disable-mdi],
			     [disable support for Microsoft Document Imaging]),
	      [HAVE_MDI=$enableval], [HAVE_MDI=yes])

if test "$HAVE_MDI" = "yes" ; then
  AC_DEFINE(MDI_SUPPORT,1,[Support Microsoft Document Imaging format])
fi

dnl ---------------------------------------------------------------------------
dnl Check for ZLIB.
dnl ---------------------------------------------------------------------------

HAVE_ZLIB=no

AC_ARG_ENABLE(zlib,
	      AS_HELP_STRING([--disable-zlib],
			     [disable Zlib usage (required for Deflate compression, enabled by default)]),,)
AC_ARG_WITH(zlib-include-dir,
	    AS_HELP_STRING([--with-zlib-include-dir=DIR],
			   [location of Zlib headers]),,)
AC_ARG_WITH(zlib-lib-dir,
	    AS_HELP_STRING([--with-zlib-lib-dir=DIR],
			   [location of Zlib library binary]),,)

if test "x$enable_zlib" != "xno" ; then

  if test "x$with_zlib_lib_dir" != "x" ; then
    LDFLAGS="-L$with_zlib_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(z, inflateEnd, [zlib_lib=yes], [zlib_lib=no],)
  if test "$zlib_lib" = "no" -a "x$with_zlib_lib_dir" != "x"; then
    AC_MSG_ERROR([Zlib library not found at $with_zlib_lib_dir])
  fi
    
  if test "x$with_zlib_include_dir" != "x" ; then
    CPPFLAGS="-I$with_zlib_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(zlib.h, [zlib_h=yes], [zlib_h=no])
  if test "$zlib_h" = "no" -a "x$with_zlib_include_dir" != "x" ; then
    AC_MSG_ERROR([Zlib headers not found at $with_zlib_include_dir])
  fi

  if test "$zlib_lib" = "yes" -a "$zlib_h" = "yes" ; then
    HAVE_ZLIB=yes
  fi

fi

if test "$HAVE_ZLIB" = "yes" ; then
  AC_DEFINE(ZIP_SUPPORT,1,[Support Deflate compression])
  LIBS="-lz $LIBS"
  tiff_libs_private="-lz ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_zlib_lib_dir" != "x" ; then
    LIBDIR="-R $with_zlib_lib_dir $LIBDIR"
  fi

fi

dnl ---------------------------------------------------------------------------
dnl Check for libdeflate.
dnl ---------------------------------------------------------------------------

HAVE_LIBDEFLATE=no

AC_ARG_ENABLE(libdeflate,
	      AS_HELP_STRING([--disable-libdeflate],
			     [disable libdeflate usage (optional for faster Deflate support (still requires zlib), enabled by default)]),,)
AC_ARG_WITH(libdeflate-include-dir,
	    AS_HELP_STRING([--with-libdeflate-include-dir=DIR],
			   [location of libdeflate headers]),,)
AC_ARG_WITH(libdeflate-lib-dir,
	    AS_HELP_STRING([--with-libdeflate-lib-dir=DIR],
			   [location of libdeflate library binary]),,)

if test "x$enable_libdeflate" != "xno" ; then

  if test "x$with_libdeflate_lib_dir" != "x" ; then
    LDFLAGS="-L$with_libdeflate_lib_dir $LDFLAGS"
  fi

  AC_CHECK_LIB(deflate, libdeflate_zlib_decompress, [libdeflate_lib=yes], [libdeflate_lib=no],)
  if test "$libdeflate_lib" = "no" -a "x$with_libdeflate_lib_dir" != "x"; then
    AC_MSG_ERROR([libdeflate library not found at $with_libdeflate_lib_dir])
  fi

  if test "x$with_libdeflate_include_dir" != "x" ; then
    CPPFLAGS="-I$with_libdeflate_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(libdeflate.h, [libdeflate_h=yes], [libdeflate_h=no])
  if test "$libdeflate_h" = "no" -a "x$with_libdeflate_include_dir" != "x" ; then
    AC_MSG_ERROR([libdeflate headers not found at $with_libdeflate_include_dir])
  fi

  if test "$libdeflate_lib" = "yes" -a "$libdeflate_h" = "yes" ; then
    HAVE_LIBDEFLATE=yes
  fi

fi

if test "$HAVE_LIBDEFLATE" = "yes" -a "$HAVE_ZLIB" = "no" ; then
  AC_MSG_WARN([libdeflate available but zlib is not. libdeflate cannot be used])
  HAVE_LIBDEFLATE=no
fi  

if test "$HAVE_LIBDEFLATE" = "yes" ; then
  AC_DEFINE(LIBDEFLATE_SUPPORT,1,[Support libdeflate enhanced compression])
  LIBS="-ldeflate $LIBS"
  tiff_libs_private="-ldeflate ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_libdeflate_lib_dir" != "x" ; then
    LIBDIR="-R $with_libdeflate_lib_dir $LIBDIR"
  fi

fi

dnl ---------------------------------------------------------------------------
dnl Check for Pixar log-format algorithm.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(pixarlog,
	      AS_HELP_STRING([--disable-pixarlog],
			     [disable support for Pixar log-format algorithm (requires Zlib)]),
	      [HAVE_PIXARLOG=$enableval], [HAVE_PIXARLOG=yes])

if test "$HAVE_ZLIB" = "yes" -a "$HAVE_PIXARLOG" = "yes" ; then
  AC_DEFINE(PIXARLOG_SUPPORT, 1,
	    [Support Pixar log-format algorithm (requires Zlib)])
else
  HAVE_PIXARLOG=no
fi

dnl ---------------------------------------------------------------------------
dnl Check for JPEG.
dnl ---------------------------------------------------------------------------

HAVE_JPEG=no

AC_ARG_ENABLE(jpeg,
	      AS_HELP_STRING([--disable-jpeg],
			     [disable IJG JPEG library usage (required for JPEG compression, enabled by default)]),,)
AC_ARG_WITH(jpeg-include-dir,
	    AS_HELP_STRING([--with-jpeg-include-dir=DIR],
			   [location of IJG JPEG library headers]),,)
AC_ARG_WITH(jpeg-lib-dir,
	    AS_HELP_STRING([--with-jpeg-lib-dir=DIR],
			   [location of IJG JPEG library binary]),,)

if test "x$enable_jpeg" != "xno" ; then

  if test "x$with_jpeg_lib_dir" != "x" ; then
    LDFLAGS="-L$with_jpeg_lib_dir $LDFLAGS"
  
  fi
  
  AC_CHECK_LIB(jpeg, jpeg_read_scanlines, [jpeg_lib=yes], [jpeg_lib=no],)
  if test "$jpeg_lib" = "no" -a "x$with_jpeg_lib_dir" != "x" ; then
    AC_MSG_ERROR([IJG JPEG library not found at $with_jpeg_lib_dir])
  fi
    
  if test "x$with_jpeg_include_dir" != "x" ; then
    CPPFLAGS="-I$with_jpeg_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(jpeglib.h, [jpeg_h=yes], [jpeg_h=no])
  if test "$jpeg_h" = "no" -a "x$with_jpeg_include_dir" != "x" ; then
    AC_MSG_ERROR([IJG JPEG library headers not found at $with_jpeg_include_dir])
  fi

  if test "$jpeg_lib" = "yes" -a "$jpeg_h" = "yes" ; then
    HAVE_JPEG=yes
  fi

fi

if test "$HAVE_JPEG" = "yes" ; then
  AC_DEFINE(JPEG_SUPPORT,1,[Support JPEG compression (requires IJG JPEG library)])
  LIBS="-ljpeg $LIBS"
  tiff_libs_private="-ljpeg ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_jpeg_lib_dir" != "x" ; then
    LIBDIR="-R $with_jpeg_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_JPEG, test "$HAVE_JPEG" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for Old JPEG.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(old-jpeg,
	      AS_HELP_STRING([--disable-old-jpeg],
			     [disable support for Old JPEG compresson (read-only, enabled by default)]),
	      [HAVE_OJPEG=${enableval}], [HAVE_OJPEG=yes])

if test "$HAVE_JPEG" = "yes" -a "$HAVE_OJPEG" = "yes" ; then
  AC_DEFINE(OJPEG_SUPPORT, 1,
	    [Support Old JPEG compresson (read-only)])
else
  HAVE_OJPEG=no
fi

dnl ---------------------------------------------------------------------------
dnl Check for JBIG-KIT.
dnl ---------------------------------------------------------------------------

HAVE_JBIG=no

AC_ARG_ENABLE(jbig,
	      AS_HELP_STRING([--disable-jbig],
			     [disable JBIG-KIT usage (required for ISO JBIG compression, enabled by default)]),,)
AC_ARG_WITH(jbig-include-dir,
	    AS_HELP_STRING([--with-jbig-include-dir=DIR],
			   [location of JBIG-KIT headers]),,)
AC_ARG_WITH(jbig-lib-dir,
	    AS_HELP_STRING([--with-jbig-lib-dir=DIR],
			   [location of JBIG-KIT library binary]),,)

if test "x$enable_jbig" != "xno" ; then

  if test "x$with_jbig_lib_dir" != "x" ; then
    LDFLAGS="-L$with_jbig_lib_dir $LDFLAGS"
  
  fi
  
  AC_CHECK_LIB(jbig, jbg_dec_init, [jbig_lib=yes], [jbig_lib=no],)
  if test "$jbig_lib" = "no" -a "x$with_jbig_lib_dir" != "x" ; then
    AC_MSG_ERROR([JBIG-KIT library not found at $with_jbig_lib_dir])
  fi
    
  if test "x$with_jbig_include_dir" != "x" ; then
    CPPFLAGS="-I$with_jbig_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(jbig.h, [jbig_h=yes], [jbig_h=no])
  if test "$jbig_h" = "no" -a "x$with_jbig_include_dir" != "x" ; then
    AC_MSG_ERROR([JBIG-KIT library headers not found at $with_jbig_include_dir])
  fi

  if test "$jbig_lib" = "yes" -a "$jbig_h" = "yes" ; then
    HAVE_JBIG=yes
  fi

fi

if test "$HAVE_JBIG" = "yes" ; then
  AC_DEFINE(JBIG_SUPPORT,1,[Support ISO JBIG compression (requires JBIG-KIT library)])
  LIBS="-ljbig $LIBS"
  tiff_libs_private="-ljbig ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_jbig_lib_dir" != "x" ; then
    LIBDIR="-R $with_jbig_lib_dir $LIBDIR"
  fi

  # Older versions of jbigkit lack jbg_newlen
  AC_CHECK_FUNCS([jbg_newlen])

fi

AM_CONDITIONAL(HAVE_JBIG, test "$HAVE_JBIG" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for liblerc.
dnl ---------------------------------------------------------------------------

HAVE_LERC=no

AC_ARG_ENABLE(lerc,
      AS_HELP_STRING([--disable-lerc],
         [disable liblerc usage (required for lerc compression, enabled by default)]),,)
AC_ARG_WITH(lerc-include-dir,
      AS_HELP_STRING([--with-lerc-include-dir=DIR],
         [location of liblerc headers]),,)
AC_ARG_WITH(lerc-lib-dir,
	    AS_HELP_STRING([--with-lerc-lib-dir=DIR],
			   [location of liblerc library binary]),,)

if test "x$enable_lerc" != "xno" ; then

  if test "x$with_lerc_lib_dir" != "x" ; then
    LDFLAGS="-L$with_lerc_lib_dir $LDFLAGS"
  fi

  lerc_lib_name="LercLib"
  AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  if test "$lerc_lib" = "no" ; then
    lerc_lib_name="Lerc"
    AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  fi
  if test "$lerc_lib" = "no" -a "x$with_lerc_lib_dir" != "x"; then
    AC_MSG_ERROR([lerc library not found at $with_lerc_lib_dir])
  fi

  if test "x$with_lerc_include_dir" != "x" ; then
    CPPFLAGS="-I$with_lerc_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(Lerc_c_api.h, [lerc_c_api_h=yes], [lerc_c_api_h=no])
  if test "$lerc_c_api_h" = "no" -a "x$with_lerc_include_dir" != "x" ; then
    AC_MSG_ERROR([Liblerc headers not found at $with_lerc_include_dir])
  fi

  if test "$lerc_lib" = "yes" -a "$lerc_c_api_h" = "yes" ; then
    HAVE_LERC=yes
  fi

fi

if test "$HAVE_LERC" = "yes" ; then
  AC_DEFINE(LERC_SUPPORT,1,[Support lerc compression])
  LIBS="-l${lerc_lib_name} $LIBS"
  tiff_libs_private="-l${lerc_lib_name} ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_lerc_lib_dir" != "x" ; then
    LIBDIR="-R $with_lerc_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_LERC, test "$HAVE_LERC" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for liblzma2.
dnl ---------------------------------------------------------------------------

HAVE_LZMA=no

AC_ARG_ENABLE(lzma,
	      AS_HELP_STRING([--disable-lzma],
			     [disable liblzma usage (required for LZMA2 compression, enabled by default)]),,)
AC_ARG_WITH(lzma-include-dir,
	    AS_HELP_STRING([--with-lzma-include-dir=DIR],
			   [location of liblzma headers]),,)
AC_ARG_WITH(lzma-lib-dir,
	    AS_HELP_STRING([--with-lzma-lib-dir=DIR],
			   [location of liblzma library binary]),,)

if test "x$enable_lzma" != "xno" ; then

  if test "x$with_lzma_lib_dir" != "x" ; then
    LDFLAGS="-L$with_lzma_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(lzma, lzma_code, [lzma_lib=yes], [lzma_lib=no],)
  if test "$lzma_lib" = "no" -a "x$with_lzma_lib_dir" != "x"; then
    AC_MSG_ERROR([lzma library not found at $with_lzma_lib_dir])
  fi
    
  if test "x$with_lzma_include_dir" != "x" ; then
    CPPFLAGS="-I$with_lzma_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(lzma.h, [lzma_h=yes], [lzma_h=no])
  if test "$lzma_h" = "no" -a "x$with_lzma_include_dir" != "x" ; then
    AC_MSG_ERROR([Liblzma headers not found at $with_lzma_include_dir])
  fi

  if test "$lzma_lib" = "yes" -a "$lzma_h" = "yes" ; then
    HAVE_LZMA=yes
  fi

fi

if test "$HAVE_LZMA" = "yes" ; then
  AC_DEFINE(LZMA_SUPPORT,1,[Support LZMA2 compression])
  LIBS="-llzma $LIBS"
  tiff_libs_private="-llzma ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_lzma_lib_dir" != "x" ; then
    LIBDIR="-R $with_lzma_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_LZMA, test "$HAVE_LZMA" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for libzstd.
dnl ---------------------------------------------------------------------------

HAVE_ZSTD=no

AC_ARG_ENABLE(zstd,
	      AS_HELP_STRING([--disable-zstd],
			     [disable libzstd usage (required for zstd compression, enabled by default)]),,)
AC_ARG_WITH(zstd-include-dir,
	    AS_HELP_STRING([--with-zstd-include-dir=DIR],
			   [location of libzstd headers]),,)
AC_ARG_WITH(zstd-lib-dir,
	    AS_HELP_STRING([--with-zstd-lib-dir=DIR],
			   [location of libzstd library binary]),,)

if test "x$enable_zstd" != "xno" ; then

  if test "x$with_zstd_lib_dir" != "x" ; then
    LDFLAGS="-L$with_zstd_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(zstd, ZSTD_decompressStream, [zstd_lib=yes], [zstd_lib=no],)
  if test "$zstd_lib" = "no" -a "x$with_zstd_lib_dir" != "x"; then
    AC_MSG_ERROR([zstd library not found at $with_zstd_lib_dir])
  fi
    
  if test "x$with_zstd_include_dir" != "x" ; then
    CPPFLAGS="-I$with_zstd_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(zstd.h, [zstd_h=yes], [zstd_h=no])
  if test "$zstd_h" = "no" -a "x$with_zstd_include_dir" != "x" ; then
    AC_MSG_ERROR([Libzstd headers not found at $with_zstd_include_dir])
  fi

  if test "$zstd_lib" = "yes" -a "$zstd_h" = "yes" ; then
    HAVE_ZSTD=yes
  fi

fi

if test "$HAVE_ZSTD" = "yes" ; then
  AC_DEFINE(ZSTD_SUPPORT,1,[Support zstd compression])
  LIBS="-lzstd $LIBS"
  tiff_libs_private="-lzstd ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_zstd_lib_dir" != "x" ; then
    LIBDIR="-R $with_zstd_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_ZSTD, test "$HAVE_ZSTD" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Check for libwebp.
dnl ---------------------------------------------------------------------------

HAVE_WEBP=no

AC_ARG_ENABLE(webp,
	      AS_HELP_STRING([--disable-webp],
			     [disable libwebp usage (required for webp compression, enabled by default)]),,)
AC_ARG_WITH(webp-include-dir,
	    AS_HELP_STRING([--with-webp-include-dir=DIR],
			   [location of libwebp headers]),,)
AC_ARG_WITH(webp-lib-dir,
	    AS_HELP_STRING([--with-webp-lib-dir=DIR],
			   [location of libwebp library binary]),,)

if test "x$enable_webp" != "xno" ; then

  if test "x$with_webp_lib_dir" != "x" ; then
    LDFLAGS="-L$with_webp_lib_dir $LDFLAGS"
  fi
  
  AC_CHECK_LIB(webp, WebPDecode, [webp_lib=yes], [webp_lib=no],)
  if test "$webp_lib" = "no" -a "x$with_webp_lib_dir" != "x"; then
    AC_MSG_ERROR([webp library not found at $with_webp_lib_dir])
  fi
    
  if test "x$with_webp_include_dir" != "x" ; then
    CPPFLAGS="-I$with_webp_include_dir $CPPFLAGS"
  fi
  AC_CHECK_HEADER(webp/decode.h, [webp_h=yes], [webp_h=no])
  if test "$webp_h" = "no" -a "x$with_webp_include_dir" != "x" ; then
    AC_MSG_ERROR([Libwebp headers not found at $with_webp_include_dir])
  fi

  if test "$webp_lib" = "yes" -a "$webp_h" = "yes" ; then
    HAVE_WEBP=yes
  fi

fi

if test "$HAVE_WEBP" = "yes" ; then
  AC_DEFINE(WEBP_SUPPORT,1,[Support webp compression])
  LIBS="-lwebp $LIBS"
  tiff_libs_private="-lwebp ${tiff_libs_private}"

  if test "$HAVE_RPATH" = "yes" -a "x$with_webp_lib_dir" != "x" ; then
    LIBDIR="-R $with_webp_lib_dir $LIBDIR"
  fi

fi

AM_CONDITIONAL(HAVE_WEBP, test "$HAVE_WEBP" = 'yes')

dnl ---------------------------------------------------------------------------
dnl Should 8/12 bit jpeg mode be enabled?
dnl ---------------------------------------------------------------------------

HAVE_JPEG12=no

AC_ARG_ENABLE(jpeg12,
	      AS_HELP_STRING([--enable-jpeg12],
			     [enable libjpeg 8/12bit dual mode]),,)
AC_ARG_WITH(jpeg12-include-dir,
	    AS_HELP_STRING([--with-jpeg12-include-dir=DIR],
			   [location of libjpeg 12bit headers]),,)
AC_ARG_WITH(jpeg12-lib,
	    AS_HELP_STRING([--with-jpeg12-lib=LIBRARY],
			   [path to libjpeg 12bit library]),,)

if test "x$enable_jpeg12" = "xyes" ; then

  if test "x$with_jpeg12_lib" != "x" ; then
    LIBS="$with_jpeg12_lib $LIBS"
  fi
  
  HAVE_JPEG12=yes

  AC_DEFINE(JPEG_DUAL_MODE_8_12,1,[8/12 bit libjpeg dual mode enabled])
  if test "x$with_jpeg12_include_dir" != "x" ; then
    AC_DEFINE_UNQUOTED(LIBJPEG_12_PATH,"$with_jpeg12_include_dir/jpeglib.h",[12bit libjpeg primary include file with path])
  fi
fi


dnl ---------------------------------------------------------------------------
dnl Check for C++.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(cxx,
	      AS_HELP_STRING([--enable-cxx],
			     [enable C++ stream API building (requires C++ compiler)]),
	      [HAVE_CXX=$enableval], [HAVE_CXX=yes])

if test "$HAVE_CXX" = "yes" ; then
  AC_DEFINE(CXX_SUPPORT, 1, [Support C++ stream API (requires C++ compiler)])
else
  HAVE_CXX=no
fi

AM_CONDITIONAL(HAVE_CXX, test "$HAVE_CXX" = "yes")

dnl ---------------------------------------------------------------------------
dnl Check for OpenGL and GLUT.
dnl ---------------------------------------------------------------------------

HAVE_OPENGL=no


AC_PATH_XTRA

dnl AX_CHECK_GL sets GL_CFLAGS & GL_LIBS.  Also PTHREAD_LIBS,
dnl PTHREAD_CFLAGS, & PTHREAD_CC as a side-effect
AX_CHECK_GL

dnl AX_CHECK_GLU sets GLU_CFLAGS & GLU_LIBS
AX_CHECK_GLU

dnl AX_CHECK_GLUT sets GLUT_CFLAGS & GLUT_LIBS
AX_CHECK_GLUT

if test "$no_x" != "yes" -a "$no_gl" != "yes" \
	-a "$no_glu" != "yes" -a "$no_glut" != "yes" ; then
  HAVE_OPENGL=yes
fi

AM_CONDITIONAL(HAVE_OPENGL, test "$HAVE_OPENGL" = "yes")

dnl ---------------------------------------------------------------------------
dnl Check for Win32 IO: make sure we have windows.h but not cygwin
dnl this must be after the ogl test, since that looks for windows.h and we
dnl test it
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(win32-io,
	      AS_HELP_STRING([--disable-win32-io],
			     [disable Win32 I/O (Windows only, enabled by default except for Cygwin)]),,)

win32_io_ok=no
case "${host_os}" in
    cygwin*)
        if test x"$ac_cv_header_windows_h" = xyes -a "x$enable_win32_io" = xyes ; then 
            win32_io_ok=yes
	fi
        ;;
    *)
        if test x"$ac_cv_header_windows_h" = xyes -a ! "x$enable_win32_io" = xno ; then 
            win32_io_ok=yes
	fi
        ;;
esac

if test "$win32_io_ok" = "yes" ; then
    AC_DEFINE(USE_WIN32_FILEIO,1,[define to use win32 IO system])
fi

AM_CONDITIONAL([WIN32_IO], [test "$win32_io_ok" = yes])

dnl ---------------------------------------------------------------------------
dnl Check for X Athena Widgets
dnl ---------------------------------------------------------------------------

dnl HAVE_XAW=no

dnl ICE_FIND_ATHENA

dnl if test "$no_xaw" != "yes" ; then
dnl   HAVE_XAW=yes
dnl fi

dnl AM_CONDITIONAL(HAVE_XAW, test "$HAVE_XAW" = "yes")

dnl ===========================================================================
dnl ``Orthogonal Features''
dnl ===========================================================================

dnl ---------------------------------------------------------------------------
dnl Default handling of strip chopping support.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(strip-chopping,
	      AS_HELP_STRING([--disable-strip-chopping],
			     [disable support for strip chopping (whether or not to convert single-strip uncompressed images to multiple strips of specified size to reduce memory usage)]),
	      [HAVE_STRIPCHOP=$enableval], [HAVE_STRIPCHOP=yes])
AC_ARG_WITH(default-strip-size,
	    AS_HELP_STRING([--with-default-strip-size=SIZE],
			   [default size of the strip in bytes (when strip chopping enabled) [[default=8192]]]),,)

if test "$HAVE_STRIPCHOP" = "yes" \
	-a "x$with_default_strip_size" != "xno"; then
  AC_DEFINE(STRIPCHOP_DEFAULT,TIFF_STRIPCHOP,[Support strip chopping (whether or not to convert single-strip uncompressed images to multiple strips of specified size to reduce memory usage)])

  if test "x$with_default_strip_size" = "x" \
	  -o "x$with_default_strip_size" = "xyes"; then
    with_default_strip_size="8192"
  fi

  AC_DEFINE_UNQUOTED(STRIP_SIZE_DEFAULT,$with_default_strip_size,[Default size of the strip in bytes (when strip chopping enabled)])

fi

dnl ---------------------------------------------------------------------------
dnl Should we try to defer loading of strip/tile offsets and sizes to 
dnl optimize directory scanning?
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(defer-strile-load,
	      AS_HELP_STRING([--enable-defer-strile-load],
			     [enable deferred strip/tile offset/size loading (also available at runtime with the 'D' flag of TIFFOpen()]),
	      [HAVE_DEFER_STRILE_LOAD=$enableval], [HAVE_DEFER_STRILE_LOAD=no])

if test "$HAVE_DEFER_STRILE_LOAD" = "yes" ; then
  AC_DEFINE(DEFER_STRILE_LOAD,1,[enable deferred strip/tile offset/size loading])

fi

dnl ---------------------------------------------------------------------------
dnl Check for support of CHUNKY_STRIP_READ_SUPPORT, a mechanism to allowing
dnl reading large strips (usually one strip files) in chunks when using
dnl TIFFReadScanline().  This is an experimental feature in libtiff 4.0.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(chunky-strip-read,
	      AS_HELP_STRING([--enable-chunky-strip-read],
			     [enable reading large strips in chunks for TIFFReadScanline() (experimental)]),
	      [HAVE_CHUNKY_STRIP_READ=$enableval], [HAVE_CHUNKY_STRIP_READ=no])

if test "$HAVE_CHUNKY_STRIP_READ" = "yes" ; then
  AC_DEFINE(CHUNKY_STRIP_READ_SUPPORT,1,[enable partial strip reading for large strips (experimental)])

fi

dnl ---------------------------------------------------------------------------
dnl Default subifd support.
dnl ---------------------------------------------------------------------------
AC_DEFINE(SUBIFD_SUPPORT,1,[Enable SubIFD tag (330) support])

dnl ---------------------------------------------------------------------------
dnl Default handling of ASSOCALPHA support.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(extrasample-as-alpha,
	      AS_HELP_STRING([--disable-extrasample-as-alpha],
			     [the RGBA interface will treat a fourth sample with no EXTRASAMPLE_ value as being ASSOCALPHA. Many packages produce RGBA files but don't mark the alpha properly]),
	      [HAVE_EXTRASAMPLE_AS_ALPHA=$enableval],
	      [HAVE_EXTRASAMPLE_AS_ALPHA=yes])

if test "$HAVE_EXTRASAMPLE_AS_ALPHA" = "yes" ; then
  AC_DEFINE(DEFAULT_EXTRASAMPLE_AS_ALPHA, 1,
	    [Treat extra sample as alpha (default enabled). The RGBA interface will treat a fourth sample with no EXTRASAMPLE_ value as being ASSOCALPHA. Many packages produce RGBA files but don't mark the alpha properly.])
fi

dnl ---------------------------------------------------------------------------
dnl Default handling of YCbCr subsampling support.
dnl See Bug 168 in Bugzilla, and JPEGFixupTestSubsampling() for details.
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(check-ycbcr-subsampling,
	      AS_HELP_STRING([--disable-check-ycbcr-subsampling],
			     [disable picking up YCbCr subsampling info from the JPEG data stream to support files lacking the tag]),
	      [CHECK_JPEG_YCBCR_SUBSAMPLING=$enableval],
	      [CHECK_JPEG_YCBCR_SUBSAMPLING=yes])

if test "$CHECK_JPEG_YCBCR_SUBSAMPLING" = "yes" ; then
  AC_DEFINE(CHECK_JPEG_YCBCR_SUBSAMPLING, 1,
	    [Pick up YCbCr subsampling info from the JPEG data stream to support files lacking the tag (default enabled).])
fi

dnl ---------------------------------------------------------------------------

AC_SUBST(LIBDIR)

AC_CONFIG_HEADERS([config.h libtiff/tif_config.h libtiff/tiffconf.h port/libport_config.h])

AC_CONFIG_FILES([Makefile \
		 build/Makefile \
		 contrib/Makefile \
		 contrib/addtiffo/Makefile \
		 contrib/dbs/Makefile \
		 contrib/dbs/xtiff/Makefile \
		 contrib/iptcutil/Makefile \
		 contrib/mfs/Makefile \
		 contrib/pds/Makefile \
		 contrib/ras/Makefile \
		 contrib/stream/Makefile \
		 contrib/tags/Makefile \
		 contrib/win_dib/Makefile \
                 html/Makefile \
		 html/images/Makefile \
		 html/man/Makefile \
                 libtiff-4.pc \
                 libtiff/Makefile \
                 man/Makefile \
		 port/Makefile \
		 test/Makefile \
                 tools/Makefile])
AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl Display configuration status
dnl ---------------------------------------------------------------------------

LOC_MSG()
LOC_MSG([Libtiff is now configured for ${host}])
LOC_MSG()
LOC_MSG([  Installation directory:             ${prefix}])
LOC_MSG([  Documentation directory:            ${LIBTIFF_DOCDIR}])
LOC_MSG([  C compiler:                         ${CC} ${CFLAGS}])
LOC_MSG([  C++ compiler:                       ${CXX} ${CXXFLAGS}])
LOC_MSG([  Enable runtime linker paths:        ${HAVE_RPATH}])
LOC_MSG([  Enable linker symbol versioning:    ${have_ld_version_script}])
LOC_MSG([  Support Microsoft Document Imaging: ${HAVE_MDI}])
LOC_MSG([  Use Win32 IO:                       ${win32_io_ok}])
LOC_MSG()
LOC_MSG([ Support for internal codecs:])
LOC_MSG([  CCITT Group 3 & 4 algorithms:       ${HAVE_CCITT}])
LOC_MSG([  Macintosh PackBits algorithm:       ${HAVE_PACKBITS}])
LOC_MSG([  LZW algorithm:                      ${HAVE_LZW}])
LOC_MSG([  ThunderScan 4-bit RLE algorithm:    ${HAVE_THUNDER}])
LOC_MSG([  NeXT 2-bit RLE algorithm:           ${HAVE_NEXT}])
LOC_MSG([  LogLuv high dynamic range encoding: ${HAVE_LOGLUV}])
LOC_MSG()
LOC_MSG([ Support for external codecs:])
LOC_MSG([  ZLIB support:                       ${HAVE_ZLIB}])
LOC_MSG([  libdeflate support:                 ${HAVE_LIBDEFLATE}])
LOC_MSG([  Pixar log-format algorithm:         ${HAVE_PIXARLOG}])
LOC_MSG([  JPEG support:                       ${HAVE_JPEG}])
LOC_MSG([  Old JPEG support:                   ${HAVE_OJPEG}])
LOC_MSG([  JPEG 8/12 bit dual mode:            ${HAVE_JPEG12}])
LOC_MSG([  ISO JBIG support:                   ${HAVE_JBIG}])
LOC_MSG([  LERC support:                       ${HAVE_LERC}])
LOC_MSG([  LZMA2 support:                      ${HAVE_LZMA}])
LOC_MSG([  ZSTD support:                       ${HAVE_ZSTD}])
LOC_MSG([  WEBP support:                       ${HAVE_WEBP}])
LOC_MSG()
LOC_MSG([  C++ support:                        ${HAVE_CXX}])
LOC_MSG()
dnl LOC_MSG([  X Athena Widgets support:           ${HAVE_XAW}])
LOC_MSG([  OpenGL support:                     ${HAVE_OPENGL}])
LOC_MSG()

