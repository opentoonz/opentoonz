#
# Warnings
#

WARNINGS = -W -Wall -Wstrict-prototypes -Wmissing-prototypes  \
    -Wcast-align -Wcast-qual -Wnested-externs -Wshadow -Wbad-function-cast \
    -Wwrite-strings

#
# Compile-time definitions
#

CFLAGS  =  -O3 -I.. -I../tools $(WARNINGS)
CFLAGS += -ffast-math -fomit-frame-pointer
#CFLAGS += -funroll-loops
#CFLAGS += -march=prescott
#CFLAGS += -mtune=native
# TIP: try adding -openmp or -fopenmp  to enable OPENMP directives and use of multiple cores
#CFLAGS += -fopenmp
CFLAGS += $(CFLAGADD)

CXXFLAGS = -O3 -ffast-math -fomit-frame-pointer  -I.. -W -Wall -march=native -mtune=native

#
# Count of FFT runs tested
#

ifeq "$(NFFT)" ""
 NFFT = 1800
endif
ifeq "$(NUMFFTS)" ""
 NUMFFTS = 10000
endif

#
# Test binary executable names
#

SELFTESTSRC = twotonetest.c

ifneq ($(KISSFFT_OPENMP),1)
BENCHKISS = bm-kiss-$(KISSFFT_DATATYPE)
BENCHFFTW = bm-fftw-$(KISSFFT_DATATYPE)
SELFTEST = st-$(KISSFFT_DATATYPE)
TESTREAL = tr-$(KISSFFT_DATATYPE)
TESTKFC = tkfc-$(KISSFFT_DATATYPE)
TESTFASTFILT = fastfilt-$(KISSFFT_DATATYPE)
TESTCPP = testcpp-$(KISSFFT_DATATYPE)
TESTSIMD = testsimd
else
BENCHKISS = bm-kiss-$(KISSFFT_DATATYPE)-openmp
BENCHFFTW = bm-fftw-$(KISSFFT_DATATYPE)-openmp
SELFTEST = st-$(KISSFFT_DATATYPE)-openmp
TESTREAL = tr-$(KISSFFT_DATATYPE)-openmp
TESTKFC = tkfc-$(KISSFFT_DATATYPE)-openmp
TESTFASTFILT = fastfilt-$(KISSFFT_DATATYPE)-openmp
TESTCPP = testcpp-$(KISSFFT_DATATYPE)-openmp
TESTSIMD = testsimd-openmp
CFLAGS += -fopenmp
CXXFLAGS += -fopenmp
endif

ifeq "$(KISSFFT_DATATYPE)" "float"
 # fftw needs to be built with --enable-float to build this lib
 FFTWLIB = -lfftw3f
else
 FFTWLIB = -lfftw3
endif

FFTWLIBDIR ?= $(ABS_LIBDIR)
ABS_FFTWLIBDIR = $(abspath $(FFTWLIBDIR))

#
# Check missing external libraries
#

ifneq ($(MAKECMDGOALS),clean)
LIBFFTW_MISSING = $(shell echo "int main(){return 0;}" > _test_library_dummy.c; \
	$(CC) -o _test_library_dummy _test_library_dummy.c $(FFTWLIB) -L$(ABS_FFTWLIBDIR); \
	echo $$?; \
	rm -f _test_library_dummy.c _test_library_dummy)
endif

#
# Find Python interpreter
#

ifneq ($(MAKECMDGOALS),clean)
  PYTHON_INTERPRETER = $(shell python --version)
  ifeq ($(PYTHON_INTERPRETER), )
	PYTHON_INTERPRETER = $(shell python2 --version)
    ifeq ($(PYTHON_INTERPRETER), )
	PYTHON_INTERPRETER = $(shell python3 --version)
      ifeq ($(PYTHON_INTERPRETER), )
	$(error ERROR: Can not find Python interpreter!)
      else
	PYTHON_INTERPRETER = "python3"
      endif
    else
	PYTHON_INTERPRETER = "python2"
    endif
  else
	PYTHON_INTERPRETER = "python"
  endif
endif

#
# Target: "make all"
#

all: $(BENCHKISS) $(SELFTEST) $(BENCHFFTW) $(TESTREAL) $(TESTKFC) $(TESTFASTFILT)

#
# Individual test make rules
#

$(SELFTEST): $(SELFTESTSRC)
	$(CC) -o $@ $(CFLAGS) $(TYPEFLAGS) $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(TESTKFC): ../kfc.c
	$(CC) -o $@ $(CFLAGS)  -DKFC_TEST $(TYPEFLAGS) $^ -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(TESTREAL): test_real.c
	$(CC) -o $@ $(CFLAGS) $(TYPEFLAGS) $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(BENCHKISS): benchkiss.c pstats.c
	$(CC) -o $@ $(CFLAGS) $(TYPEFLAGS)  $^ -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(TESTFASTFILT): ../tools/fftutil.c
	$(CC) -o $@ $(CFLAGS)  -DKFC_TEST $(TYPEFLAGS) $^ -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(BENCHFFTW): benchfftw.c pstats.c
	$(warning ======attempting to build FFTW benchmark)
ifeq ($(LIBFFTW_MISSING), 0)
	$(CC) -o $@ $(CFLAGS) -DDATATYPE$(KISSFFT_DATATYPE) $^ $(FFTWLIB) -L$(ABS_FFTWLIBDIR) -L.. -l$(KISSFFTLIB_SHORTNAME) -lm
else
	$(warning WARNING: No FFTW development files found! FFTW not available for comparison!)
endif

#
# Test SSE
#

$(TESTSIMD): test_simd.c
ifeq "$(KISSFFT_DATATYPE)" "simd"
	$(CC) -o $@ -g $(CFLAGS) -DUSE_SIMD=1 -msse $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lm
else
	$(error ERROR: This test makes sense only with KISSFFT_DATATYPE=simd)
endif

testsse: $(TESTSIMD)
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(TESTSIMD)

#
# Test C++
#

$(TESTCPP): testcpp.cc ../kissfft.hh
	$(CXX) -o $@ $(CXXFLAGS) testcpp.cc -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

testcpp: $(TESTCPP)
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(TESTCPP)

#
# Target: "make test"
#

test: all
ifeq "$(KISSFFT_DATATYPE)" "simd"
	$(MAKE) testsse
endif
	@LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(TESTKFC)
	$(warning ======1d & 2-d complex fft self test (type= $(KISSFFT_DATATYPE) ))
	@LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(SELFTEST)
	$(warning ======real FFT (type= $(KISSFFT_DATATYPE) ))
	@LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(TESTREAL)
	$(warning ======timing test (type=$(KISSFFT_DATATYPE)))
	@LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(BENCHKISS) -x $(NUMFFTS) -n $(NFFT)
	@[ -x ./$(BENCHFFTW) ] && LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." ./$(BENCHFFTW) -x $(NUMFFTS) -n $(NFFT) || true
	$(warning ======higher dimensions (type=$(KISSFFT_DATATYPE)))
	@LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):.." $(PYTHON_INTERPRETER) ./testkiss.py

#
# Target: "make clean"
#

clean:
	rm -f *~ bm-* st-* tr-* kf-* tkfc-* ff-* fastfilt-* *.pyc *.pyo *.dat testcpp-* testsimd testsimd-* _test_library_dummy _test_library_dummy.c
