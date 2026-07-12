#
# Warnings
#

WARNINGS = -W -Wall -Wstrict-prototypes -Wmissing-prototypes \
    -Wcast-align -Wcast-qual -Wnested-externs -Wshadow -Wbad-function-cast \
    -Wwrite-strings

#
# Compile-time definitions
#

CFLAGS = -Wall -O3 $(WARNINGS)
#CFLAGS = -Wall -O3 -pedantic -march=pentiumpro -ffast-math -fomit-frame-pointer $(WARNINGS)
# If the above flags do not work, try the following
# tip: try -openmp or -fopenmp to use multiple cores

CFLAGS += $(CFLAGADD)

#
# Check missing external libraries
#

ifneq ($(MAKECMDGOALS),clean)
LIBPNG_MISSING = $(shell echo "int main(){return 0;}" > _test_library_dummy.c; \
	$(CC) -o _test_library_dummy _test_library_dummy.c -lpng; \
	echo $$?; \
	rm -f _test_library_dummy.c _test_library_dummy)
endif

#
# Tool names
#

ifneq ($(KISSFFT_OPENMP),1)
FFTUTIL = fft-$(KISSFFT_DATATYPE)
FASTFILT = fastconv-$(KISSFFT_DATATYPE)
FASTFILTREAL = fastconvr-$(KISSFFT_DATATYPE)
PSDPNG = psdpng-$(KISSFFT_DATATYPE)
DUMPHDR = dumphdr-$(KISSFFT_DATATYPE)
else
FFTUTIL = fft-$(KISSFFT_DATATYPE)-openmp
FASTFILT = fastconv-$(KISSFFT_DATATYPE)-openmp
FASTFILTREAL = fastconvr-$(KISSFFT_DATATYPE)-openmp
PSDPNG = psdpng-$(KISSFFT_DATATYPE)-openmp
DUMPHDR = dumphdr-$(KISSFFT_DATATYPE)-openmp
endif

#
# Target: "make all"
#

all: $(FFTUTIL) $(FASTFILT) $(FASTFILTREAL) $(PSDPNG)
#	$(DUMPHDR)

#
# Individual tool make rules
#

$(FASTFILTREAL): kiss_fastfir.c
	$(CC) -o $@ $(CFLAGS) -I.. $(TYPEFLAGS) -DREAL_FASTFIR $< -DFAST_FILT_UTIL -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(FASTFILT): kiss_fastfir.c
	$(CC) -o $@ $(CFLAGS) -I.. $(TYPEFLAGS) $<  -DFAST_FILT_UTIL -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(FFTUTIL): fftutil.c
	$(CC) -o $@ $(CFLAGS) -I.. $(TYPEFLAGS) $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

$(PSDPNG): psdpng.c
ifeq "$(KISSFFT_DATATYPE)" "simd"
	$(warning WARNING: psdpng can not utilize SIMD!)
else ifeq ($(LIBPNG_MISSING), 0)
	$(CC) -o $@ $(CFLAGS) -I.. $(TYPEFLAGS) $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lpng -lm
else
	$(error ERROR: no libpng development files found!)
endif

$(DUMPHDR): dumphdr.c
	$(CC) -o $@ $(CFLAGS) -I.. $(TYPEFLAGS) $< -L.. -l$(KISSFFTLIB_SHORTNAME) -lm

#
# Target: "make install"
#

install: all
	$(INSTALL) -Dt $(ABS_BINDIR) -m 755 \
		$(FFTUTIL) \
		$(FASTFILT) \
		$(FASTFILTREAL)

ifneq "$(KISSFFT_DATATYPE)" "simd"
	$(INSTALL) -Dt $(ABS_BINDIR) -m 755 \
		$(PSDPNG)
endif

#
# Target: "make clean"
#

clean:
	rm -f *~ fft fft-* fastconv fastconv-* fastconvr fastconvr-* psdpng psdpng-* _test_library_dummy _test_library_dummy.c
