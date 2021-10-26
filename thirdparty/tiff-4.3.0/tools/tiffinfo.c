/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

#include "tif_config.h"
#include "libport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "tiffiop.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

static TIFFErrorHandler old_error_handler = 0;
static int status = EXIT_SUCCESS;       /* exit status */
static int showdata = 0;		/* show data */
static int rawdata = 0;			/* show raw/decoded data */
static int showwords = 0;		/* show data as bytes/words */
static int readdata = 0;		/* read data in file */
static int stoponerr = 1;		/* stop on first read error */

static	void usage(int);
static	void tiffinfo(TIFF*, uint16_t, long, int);

static void
PrivateErrorHandler(const char* module, const char* fmt, va_list ap)
{
        if (old_error_handler)
                (*old_error_handler)(module,fmt,ap);
	status = EXIT_FAILURE;
}

int
main(int argc, char* argv[])
{
	int dirnum = -1, multiplefiles, c;
	uint16_t order = 0;
	TIFF* tif;
#if !HAVE_DECL_OPTARG
	extern int optind;
	extern char* optarg;
#endif
	long flags = 0;
	uint64_t diroff = 0;
	int chopstrips = 0;		/* disable strip chopping */

	while ((c = getopt(argc, argv, "f:o:cdDSjilmrsvwz0123456789h")) != -1)
		switch (c) {
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
		case '8': case '9':
			dirnum = atoi(&argv[optind-1][1]);
			break;
		case 'd':
			showdata++;
			/* fall through... */
		case 'D':
			readdata++;
			break;
		case 'c':
			flags |= TIFFPRINT_COLORMAP | TIFFPRINT_CURVES;
			break;
		case 'f':		/* fill order */
			if (streq(optarg, "lsb2msb"))
				order = FILLORDER_LSB2MSB;
			else if (streq(optarg, "msb2lsb"))
				order = FILLORDER_MSB2LSB;
			else
				usage(EXIT_FAILURE);
			break;
		case 'i':
			stoponerr = 0;
			break;
		case 'o':
			diroff = strtoul(optarg, NULL, 0);
			break;
		case 'j':
			flags |= TIFFPRINT_JPEGQTABLES |
				 TIFFPRINT_JPEGACTABLES |
				 TIFFPRINT_JPEGDCTABLES;
			break;
		case 'r':
			rawdata = 1;
			break;
		case 's':
			flags |= TIFFPRINT_STRIPS;
			break;
		case 'w':
			showwords = 1;
			break;
		case 'z':
			chopstrips = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
                        /*NOTREACHED*/
                        break;
		case '?':
			usage(EXIT_FAILURE);
			/*NOTREACHED*/
                        break;
		}
	if (optind >= argc)
		usage(EXIT_FAILURE);

	old_error_handler = TIFFSetErrorHandler(PrivateErrorHandler);

	multiplefiles = (argc - optind > 1);
	for (; optind < argc; optind++) {
		if (multiplefiles)
			printf("%s:\n", argv[optind]);
		tif = TIFFOpen(argv[optind], chopstrips ? "rC" : "rc");
		if (tif != NULL) {
			if (dirnum != -1) {
				if (TIFFSetDirectory(tif, (tdir_t) dirnum))
					tiffinfo(tif, order, flags, 1);
			} else if (diroff != 0) {
				if (TIFFSetSubDirectory(tif, diroff))
					tiffinfo(tif, order, flags, 1);
			} else {
				do {
					toff_t offset=0;

					tiffinfo(tif, order, flags, 1);
					if (TIFFGetField(tif, TIFFTAG_EXIFIFD,
							 &offset)) {
						if (TIFFReadEXIFDirectory(tif, offset)) {
							tiffinfo(tif, order, flags, 0);
						}
					}
				} while (TIFFReadDirectory(tif));
			}
			TIFFClose(tif);
		}
	}
	return (status);
}

static const char usage_info[] =
"Display information about TIFF files\n\n"
"usage: tiffinfo [options] input...\n"
"where options are:\n"
" -D		read data\n"
" -i		ignore read errors\n"
" -c		display data for grey/color response curve or colormap\n"
" -d		display raw/decoded image data\n"
" -f lsb2msb	force lsb-to-msb FillOrder for input\n"
" -f msb2lsb	force msb-to-lsb FillOrder for input\n"
" -j		show JPEG tables\n"
" -o offset	set initial directory offset\n"
" -r		read/display raw image data instead of decoded data\n"
" -s		display strip offsets and byte counts\n"
" -w		display raw data in words rather than bytes\n"
" -z		enable strip chopping\n"
" -#		set initial directory (first directory is # 0)\n"
;

static void
usage(int code)
{
	FILE * out = (code == EXIT_SUCCESS) ? stdout : stderr;

        fprintf(out, "%s\n\n", TIFFGetVersion());
        fprintf(out, "%s", usage_info);
	exit(code);
}

static void
ShowStrip(tstrip_t strip, unsigned char* pp, uint32_t nrow, tsize_t scanline)
{
	register tsize_t cc;

	printf("Strip %"PRIu32":\n", strip);
	while (nrow-- > 0) {
		for (cc = 0; cc < scanline; cc++) {
			printf(" %02x", *pp++);
			if (((cc+1) % 24) == 0)
				putchar('\n');
		}
		putchar('\n');
	}
}

void
TIFFReadContigStripData(TIFF* tif)
{
	unsigned char *buf;
	tsize_t scanline = TIFFScanlineSize(tif);

	buf = (unsigned char *)_TIFFmalloc(TIFFStripSize(tif));
	if (buf) {
		uint32_t row, h=0;
		uint32_t rowsperstrip = (uint32_t)-1;

		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
		for (row = 0; row < h; row += rowsperstrip) {
			uint32_t nrow = (row + rowsperstrip > h ?
			    h-row : rowsperstrip);
			tstrip_t strip = TIFFComputeStrip(tif, row, 0);
			if (TIFFReadEncodedStrip(tif, strip, buf, nrow*scanline) < 0) {
				if (stoponerr)
					break;
			} else if (showdata)
				ShowStrip(strip, buf, nrow, scanline);
		}
		_TIFFfree(buf);
	}
}

void
TIFFReadSeparateStripData(TIFF* tif)
{
	unsigned char *buf;
	tsize_t scanline = TIFFScanlineSize(tif);

	buf = (unsigned char *)_TIFFmalloc(TIFFStripSize(tif));
	if (buf) {
		uint32_t row, h=0;
		uint32_t rowsperstrip = (uint32_t)-1;
		tsample_t s, samplesperpixel=0;

		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
		for (row = 0; row < h; row += rowsperstrip) {
			for (s = 0; s < samplesperpixel; s++) {
				uint32_t nrow = (row + rowsperstrip > h ?
				    h-row : rowsperstrip);
				tstrip_t strip = TIFFComputeStrip(tif, row, s);
				if (TIFFReadEncodedStrip(tif, strip, buf, nrow*scanline) < 0) {
					if (stoponerr)
						break;
				} else if (showdata)
					ShowStrip(strip, buf, nrow, scanline);
			}
		}
		_TIFFfree(buf);
	}
}

static void
ShowTile(uint32_t row, uint32_t col, tsample_t sample,
         unsigned char* pp, uint32_t nrow, tsize_t rowsize)
{
	uint32_t cc;

	printf("Tile (%" PRIu32 ",%" PRIu32 "", row, col);
	if (sample != (tsample_t) -1)
		printf(",%" PRIu16, sample);
	printf("):\n");
	while (nrow-- > 0) {
	  for (cc = 0; cc < (uint32_t) rowsize; cc++) {
			printf(" %02x", *pp++);
			if (((cc+1) % 24) == 0)
				putchar('\n');
		}
		putchar('\n');
	}
}

void
TIFFReadContigTileData(TIFF* tif)
{
	unsigned char *buf;
	tmsize_t rowsize = TIFFTileRowSize(tif);
        tmsize_t tilesize = TIFFTileSize(tif);

	buf = (unsigned char *)_TIFFmalloc(tilesize);
	if (buf) {
		uint32_t tw=0, th=0, w=0, h=0;
		uint32_t row, col;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
                if ( rowsize == 0 || th > (size_t) (tilesize / rowsize) )
        {
            fprintf(stderr, "Cannot display data: th * rowsize > tilesize\n");
            _TIFFfree(buf);
            return;
        }
		for (row = 0; row < h; row += th) {
			for (col = 0; col < w; col += tw) {
				if (TIFFReadTile(tif, buf, col, row, 0, 0) < 0) {
					if (stoponerr)
						break;
				} else if (showdata)
					ShowTile(row, col, (tsample_t) -1, buf, th, rowsize);
			}
		}
		_TIFFfree(buf);
	}
}

void
TIFFReadSeparateTileData(TIFF* tif)
{
	unsigned char *buf;
        tmsize_t rowsize = TIFFTileRowSize(tif);
        tmsize_t tilesize = TIFFTileSize(tif);

	buf = (unsigned char *)_TIFFmalloc(tilesize);
	if (buf) {
		uint32_t tw=0, th=0, w=0, h=0;
		uint32_t row, col;
		tsample_t s, samplesperpixel=0;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
                if ( rowsize == 0 || th > (size_t) (tilesize / rowsize) )
        {
            fprintf(stderr, "Cannot display data: th * rowsize > tilesize\n");
            _TIFFfree(buf);
            return;
        }
		for (row = 0; row < h; row += th) {
			for (col = 0; col < w; col += tw) {
				for (s = 0; s < samplesperpixel; s++) {
					if (TIFFReadTile(tif, buf, col, row, 0, s) < 0) {
						if (stoponerr)
							break;
					} else if (showdata)
						ShowTile(row, col, s, buf, th, rowsize);
				}
			}
		}
		_TIFFfree(buf);
	}
}

void
TIFFReadData(TIFF* tif)
{
	uint16_t config = PLANARCONFIG_CONTIG;

	TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);
	if (TIFFIsTiled(tif)) {
		if (config == PLANARCONFIG_CONTIG)
			TIFFReadContigTileData(tif);
		else
			TIFFReadSeparateTileData(tif);
	} else {
		if (config == PLANARCONFIG_CONTIG)
			TIFFReadContigStripData(tif);
		else
			TIFFReadSeparateStripData(tif);
	}
}

static void
ShowRawBytes(unsigned char* pp, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		printf(" %02x", *pp++);
		if (((i+1) % 24) == 0)
			printf("\n ");
	}
	putchar('\n');
}

static void
ShowRawWords(uint16_t* pp, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++) {
		printf(" %04" PRIx16, *pp++);
		if (((i+1) % 15) == 0)
			printf("\n ");
	}
	putchar('\n');
}

static void
TIFFReadRawDataStriped(TIFF* tif, int bitrev)
{
	tstrip_t nstrips = TIFFNumberOfStrips(tif);
	const char* what = "Strip";
	uint64_t* stripbc=NULL;

	TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &stripbc);
	if (stripbc != NULL && nstrips > 0) {
		uint32_t bufsize = (uint32_t) stripbc[0];
		tdata_t buf = _TIFFmalloc(bufsize);
		tstrip_t s;

		for (s = 0; s < nstrips; s++) {
			if (stripbc[s] > bufsize) {
				buf = _TIFFrealloc(buf, (tmsize_t)stripbc[s]);
				bufsize = (uint32_t) stripbc[s];
			}
			if (buf == NULL) {
				fprintf(stderr,
				   "Cannot allocate buffer to read strip %"PRIu32"\n",
				    s);
				break;
			}
			if (TIFFReadRawStrip(tif, s, buf, (tmsize_t) stripbc[s]) < 0) {
				fprintf(stderr, "Error reading strip %"PRIu32"\n",
				    s);
				if (stoponerr)
					break;
			} else if (showdata) {
				if (bitrev) {
					TIFFReverseBits(buf, (tmsize_t)stripbc[s]);
					printf("%s %"PRIu32": (bit reversed)\n ",
					    what, s);
				} else
					printf("%s %"PRIu32":\n ", what,
					    s);
				if (showwords)
					ShowRawWords((uint16_t*) buf, (uint32_t) stripbc[s] >> 1);
				else
					ShowRawBytes((unsigned char*) buf, (uint32_t) stripbc[s]);
			}
		}
		if (buf != NULL)
			_TIFFfree(buf);
	}
}

static void
TIFFReadRawDataTiled(TIFF* tif, int bitrev)
{
	const char* what = "Tile";
	uint32_t ntiles = TIFFNumberOfTiles(tif);
	uint64_t *tilebc;

	TIFFGetField(tif, TIFFTAG_TILEBYTECOUNTS, &tilebc);
	if (tilebc != NULL && ntiles > 0) {
		uint64_t bufsize = 0;
		tdata_t buf = NULL;
		uint32_t t;

		for (t = 0; t < ntiles; t++) {
			if (buf == NULL || tilebc[t] > bufsize) {
				buf = _TIFFrealloc(buf, (tmsize_t)tilebc[t]);
				bufsize = tilebc[t];
			}
			if (buf == NULL) {
				fprintf(stderr,
				   "Cannot allocate buffer to read tile %"PRIu32"\n",
				    t);
				break;
			}
			if (TIFFReadRawTile(tif, t, buf, (tmsize_t)tilebc[t]) < 0) {
				fprintf(stderr, "Error reading tile %"PRIu32"\n",
				    t);
				if (stoponerr)
					break;
			} else if (showdata) {
				if (bitrev) {
					TIFFReverseBits(buf, (tmsize_t)tilebc[t]);
					printf("%s %"PRIu32": (bit reversed)\n ",
					    what, t);
				} else {
					printf("%s %"PRIu32":\n ", what,
					    t);
				}
				if (showwords) {
					ShowRawWords((uint16_t*) buf, (uint32_t)(tilebc[t] >> 1));
				} else {
					ShowRawBytes((unsigned char*) buf, (uint32_t) tilebc[t]);
				}
			}
		}
		if (buf != NULL)
			_TIFFfree(buf);
	}
}

void
TIFFReadRawData(TIFF* tif, int bitrev)
{
	if (TIFFIsTiled(tif)) {
		TIFFReadRawDataTiled(tif, bitrev);
	} else {
		TIFFReadRawDataStriped(tif, bitrev);
	}
}

static void
tiffinfo(TIFF* tif, uint16_t order, long flags, int is_image)
{
	TIFFPrintDirectory(tif, stdout, flags);
	if (!readdata || !is_image)
		return;
	if (rawdata) {
		if (order) {
			uint16_t o;
			TIFFGetFieldDefaulted(tif,
			    TIFFTAG_FILLORDER, &o);
			TIFFReadRawData(tif, o != order);
		} else
			TIFFReadRawData(tif, 0);
	} else {
		if (order)
			TIFFSetField(tif, TIFFTAG_FILLORDER, order);
		TIFFReadData(tif);
	}
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
