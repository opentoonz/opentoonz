#
# Semantic versioning
#
# KFVER_MAJOR denotes the ABI version.
#
# - It must be bumped only if API public members are removed or
#   changed in the incompatible
#
# KFVER_MINOR denotes the minor version within a compatible ABI.
#
# - It should be bumped if new API public members are added
#   (but not removed!) so programs linked against the same library
#   version continue operating properly
#
# KFVER_PATCH denotes bugfix count since the last minor update.
#
# - It should be bumped whenever a bug fix is pushed.
#

export KFVER_MAJOR = 131
export KFVER_MINOR = 1
export KFVER_PATCH = 0

#
# Data type (float / int16_t / int32_t / simd)
#

export KISSFFT_DATATYPE ?= float

#
# Default options
#

export KISSFFT_OPENMP ?= 0
export KISSFFT_STATIC ?= 0
export KISSFFT_TOOLS ?= 1
export KISSFFT_USE_ALLOCA ?= 0

#
# Installation directories
#

PREFIX ?= /usr/local
export ABS_PREFIX = $(abspath $(PREFIX))

BINDIR ?= $(ABS_PREFIX)/bin
export ABS_BINDIR = $(abspath $(BINDIR))

INCLUDEDIR ?= $(ABS_PREFIX)/include
export ABS_INCLUDEDIR = $(abspath $(INCLUDEDIR))
export ABS_PKGINCLUDEDIR = $(ABS_INCLUDEDIR)/kissfft

#
# Override LIBDIR with lib64 following CMake's
# GNUInstallDirs logic:
#

CANDIDATE_LIBDIR_NAME = lib

ifneq ($(MAKECMDGOALS),clean)
  ifeq ($(shell uname -s),Linux)
    _UNAME_ARCH = $(shell uname -i)

    ifeq (,$(_UNAME_ARCH))
	_UNAME_ARCH = $(shell uname -m)

      ifeq (,$(_UNAME_ARCH))
	$(warning WARNING: Can not detect system architecture!)
      endif
    endif

    ifeq ($(_UNAME_ARCH),x86_64)
	CANDIDATE_LIBDIR_NAME = lib64
    endif
  endif
endif

CANDIDATE_LIBDIR = $(PREFIX)/$(CANDIDATE_LIBDIR_NAME)
LIBDIR ?= $(CANDIDATE_LIBDIR)

export ABS_LIBDIR = $(abspath $(LIBDIR))

export INSTALL ?= install

#
# Library name and version
#

ifeq ($(KISSFFT_OPENMP), 1)
	KISSFFTLIB_SHORTNAME = kissfft-$(KISSFFT_DATATYPE)-openmp
	KISSFFT_PKGCONFIG = kissfft-$(KISSFFT_DATATYPE)-openmp.pc
	KISSFFTLIB_FLAGS = -fopenmp
	TYPEFLAGS = -fopenmp
	PKGCONFIG_OPENMP = -fopenmp
else
	KISSFFTLIB_SHORTNAME = kissfft-$(KISSFFT_DATATYPE)
	KISSFFT_PKGCONFIG = kissfft-$(KISSFFT_DATATYPE).pc
	TYPEFLAGS =
	PKGCONFIG_OPENMP =
endif

ifeq ($(KISSFFT_STATIC), 1)
	KISSFFTLIB_NAME = lib$(KISSFFTLIB_SHORTNAME).a
	KISSFFTLIB_FLAGS += -static
else ifeq ($(shell uname -s),Darwin)
	KISSFFTLIB_NAME = lib$(KISSFFTLIB_SHORTNAME).dylib
	KISSFFTLIB_FLAGS += -shared -Wl,-install_name,$(KISSFFTLIB_NAME)
else
	KISSFFTLIB_SODEVELNAME = lib$(KISSFFTLIB_SHORTNAME).so
	KISSFFTLIB_SONAME = $(KISSFFTLIB_SODEVELNAME).$(KFVER_MAJOR)
	KISSFFTLIB_NAME = $(KISSFFTLIB_SONAME).$(KFVER_MINOR).$(KFVER_PATCH)
	KISSFFTLIB_FLAGS += -shared -Wl,-soname,$(KISSFFTLIB_SONAME)
endif

export KISSFFTLIB_SHORTNAME

#
# Compile-time definitions by datatype
#
#
# Note: -DKISS_FFT_BUILD and -DKISS_FFT_SHARED control
# C symbol visibility.
#

ifeq "$(KISSFFT_DATATYPE)" "int32_t"
	TYPEFLAGS += -DFIXED_POINT=32
else ifeq "$(KISSFFT_DATATYPE)" "int16_t"
	TYPEFLAGS += -DFIXED_POINT=16
else ifeq "$(KISSFFT_DATATYPE)" "simd"
	TYPEFLAGS += -DUSE_SIMD=1 -msse
else ifeq "$(KISSFFT_DATATYPE)" "float"
	TYPEFLAGS += -Dkiss_fft_scalar=$(KISSFFT_DATATYPE)
else ifeq "$(KISSFFT_DATATYPE)" "double"
	TYPEFLAGS += -Dkiss_fft_scalar=$(KISSFFT_DATATYPE)
else
	$(error ERROR: KISSFFT_DATATYPE must be one of: float double int16_t int32_t simd)
endif

ifneq ($(KISSFFT_STATIC), 1)
	TYPEFLAGS += -DKISS_FFT_SHARED
endif

ifeq ($(KISSFFT_USE_ALLOCA), 1)
	TYPEFLAGS += -DKISS_FFT_USE_ALLOCA=1
endif

#
# Compile-time definitions
#

#
# Save pkgconfig variables before appending
# -DKISS_FFT_BUILD to TYPEFLAGS
#

ifneq ($(shell uname -s),Darwin)
	PKGCONFIG_KISSFFT_VERSION = $(KFVER_MAJOR).$(KFVER_MINOR).$(KFVER_PATCH)
	PKGCONFIG_KISSFFT_OUTPUT_NAME = $(KISSFFTLIB_SHORTNAME)
	PKGCONFIG_PKG_KISSFFT_DEFS = $(TYPEFLAGS)
	PKGCONFIG_KISSFFT_PREFIX = $(ABS_PREFIX)
  ifeq ($(ABS_INCLUDEDIR),$(ABS_PREFIX)/include)
	PKGCONFIG_KISSFFT_INCLUDEDIR = $${prefix}/include
  else
	PKGCONFIG_KISSFFT_INCLUDEDIR = $(ABS_INCLUDEDIR)

  endif
  ifeq ($(ABS_LIBDIR),$(ABS_PREFIX)/$(CANDIDATE_LIBDIR_NAME))
	PKGCONFIG_KISSFFT_LIBDIR = $${prefix}/$(CANDIDATE_LIBDIR_NAME)
  else
	PKGCONFIG_KISSFFT_LIBDIR = $(ABS_LIBDIR)
  endif
	PKGCONFIG_KISSFFT_PKGINCLUDEDIR = $${includedir}/kissfft
endif

export TYPEFLAGS

# Compile .c into .o
#

#
# -DKISS_FFT_BUILD is used for library artifacts, so
# consumer executable in 'test' and 'tools' do _NOT_
# need it. pkg-config output does not need it either.
#

%.c.o: %.c
	$(CC) -Wall -fPIC \
		-o $@ \
		$(CFLAGS) $(TYPEFLAGS) -DKISS_FFT_BUILD \
		-c $<

#
# Target: "make all"
#

all: kfc.c.o kiss_fft.c.o kiss_fftnd.c.o kiss_fftndr.c.o kiss_fftr.c.o
ifneq ($(KISSFFT_STATIC), 1)
	$(CC) $(KISSFFTLIB_FLAGS) -o $(KISSFFTLIB_NAME) $^
  ifneq ($(shell uname -s),Darwin)
	ln -sf $(KISSFFTLIB_NAME) $(KISSFFTLIB_SONAME)
	ln -sf $(KISSFFTLIB_NAME) $(KISSFFTLIB_SODEVELNAME)
  endif
else
	$(AR) crus $(KISSFFTLIB_NAME) $^
endif
ifneq ($(KISSFFT_TOOLS), 0)
	$(MAKE) -C tools CFLAGADD="$(CFLAGADD)" all
endif

#
# Target: "make install"
#

install: all
	$(INSTALL) -Dt $(ABS_PKGINCLUDEDIR) -m 644 \
		kiss_fft.h \
		kissfft.hh \
		kiss_fftnd.h \
		kiss_fftndr.h \
		kiss_fftr.h
	$(INSTALL) -Dt $(ABS_LIBDIR) -m 644 $(KISSFFTLIB_NAME)
ifneq ($(KISSFFT_STATIC), 1)
  ifneq ($(shell uname -s),Darwin)
	cd $(LIBDIR) && \
	ln -sf $(KISSFFTLIB_NAME) $(KISSFFTLIB_SONAME) && \
	ln -sf $(KISSFFTLIB_NAME) $(KISSFFTLIB_SODEVELNAME)
  endif
endif
ifneq ($(shell uname -s),Darwin)
	mkdir -p "$(ABS_LIBDIR)/pkgconfig"
	sed \
		-e 's+@PKGCONFIG_KISSFFT_VERSION@+$(PKGCONFIG_KISSFFT_VERSION)+' \
		-e 's+@KISSFFT_OUTPUT_NAME@+$(PKGCONFIG_KISSFFT_OUTPUT_NAME)+' \
		-e 's+@PKG_KISSFFT_DEFS@+$(PKGCONFIG_PKG_KISSFFT_DEFS)+' \
		-e 's+@PKG_OPENMP@+$(PKGCONFIG_OPENMP)+' \
		-e 's+@PKGCONFIG_KISSFFT_PREFIX@+$(PKGCONFIG_KISSFFT_PREFIX)+' \
		-e 's+@PKGCONFIG_KISSFFT_INCLUDEDIR@+$(PKGCONFIG_KISSFFT_INCLUDEDIR)+' \
		-e 's+@PKGCONFIG_KISSFFT_LIBDIR@+$(PKGCONFIG_KISSFFT_LIBDIR)+' \
		-e 's+@PKGCONFIG_KISSFFT_PKGINCLUDEDIR@+$(PKGCONFIG_KISSFFT_PKGINCLUDEDIR)+' \
		kissfft.pc.in 1>"$(ABS_LIBDIR)/pkgconfig/$(KISSFFT_PKGCONFIG)"
endif
ifneq ($(KISSFFT_TOOLS), 0)
	$(MAKE) -C tools install
endif

#
# Target: "make doc"
#

doc:
	$(warning Start by reading the README file.  If you want to build and test lots of stuff, do a 'make testall')
	$(warning but be aware that 'make testall' has dependencies that the basic kissfft software does not.)
	$(warning It is generally unneeded to run these tests yourself, unless you plan on changing the inner workings)
	$(warning of kissfft and would like to make use of its regression tests.)

#
# Target: "make testsingle"
#

testsingle:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) -C test CFLAGADD="$(CFLAGADD)" test testcpp

#
# Target: "make testall"
#

testall:
	# Shared libraries
	$(MAKE) KISSFFT_DATATYPE=double testsingle
	$(MAKE) KISSFFT_DATATYPE=float testsingle
	$(MAKE) KISSFFT_DATATYPE=int16_t testsingle
	# The simd and int32_t types may or may not work on your machine
	$(MAKE) KISSFFT_DATATYPE=int32_t testsingle
	$(MAKE) KISSFFT_DATATYPE=simd testsingle
	# Static libraries
	$(MAKE) KISSFFT_DATATYPE=double KISSFFT_STATIC=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=float KISSFFT_STATIC=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=int16_t KISSFFT_STATIC=1 testsingle
	# The simd and int32_t types may or may not work on your machine
	$(MAKE) KISSFFT_DATATYPE=int32_t KISSFFT_STATIC=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=simd KISSFFT_STATIC=1 testsingle
	# OpenMP libraries
	$(MAKE) KISSFFT_DATATYPE=double KISSFFT_OPENMP=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=float KISSFFT_OPENMP=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=int16_t KISSFFT_OPENMP=1 testsingle
	# The simd and int32_t types may or may not work on your machine
	$(MAKE) KISSFFT_DATATYPE=int32_t KISSFFT_OPENMP=1 testsingle
	$(MAKE) KISSFFT_DATATYPE=simd KISSFFT_OPENMP=1 testsingle
	$(warning All tests passed!)

#
# Target: "make tarball"
#

tarball: clean
	git archive --prefix=kissfft/ -o kissfft$(KFVER).tar.gz v$(KFVER)
	git archive --prefix=kissfft/ -o kissfft$(KFVER).zip v$(KFVER)

#
# Target: "make clean"
#

clean:
	rm -f *.o *.a *.so *.so.*
	cd test && $(MAKE) clean
	cd tools && $(MAKE) clean
	rm -f kiss_fft*.tar.gz *~ *.pyc kiss_fft*.zip

#
# Target: "make asm"
#

asm: kiss_fft.s

# TODO: Sort out if we should add kfc / other C headers

kiss_fft.s: kiss_fft.c kiss_fft.h _kiss_fft_guts.h
	[ -e kiss_fft.s ] && mv kiss_fft.s kiss_fft.s~ || true
	$(CC) -S kiss_fft.c -O3 -mtune=native -ffast-math -fomit-frame-pointer -unroll-loops -dA -fverbose-asm
	$(CC) -o kiss_fft_short.s -S kiss_fft.c -O3 -mtune=native -ffast-math -fomit-frame-pointer -dA -fverbose-asm -DFIXED_POINT
	[ -e kiss_fft.s~ ] && diff kiss_fft.s~ kiss_fft.s || true
