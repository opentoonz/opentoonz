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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "tiffio.h"
#include "tiffiop.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define	streq(a,b)	(strcmp(a,b) == 0)
#define	strneq(a,b,n)	(strncmp(a,b,n) == 0)

#define	CopyField(tag, v) \
	if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)

uint32_t	imagewidth;
uint32_t	imagelength;
int	threshold = 128;

static	void usage(int code);

/* 
 * Floyd-Steinberg error propragation with threshold.
 * This code is stolen from tiffmedian.
 */
static int
fsdither(TIFF* in, TIFF* out)
{
	unsigned char *outline, *inputline, *inptr;
	short *thisline, *nextline, *tmpptr;
	register unsigned char	*outptr;
	register short *thisptr, *nextptr;
	register uint32_t i, j;
	uint32_t imax, jmax;
	int lastline, lastpixel;
	int bit;
	tsize_t outlinesize;
	int errcode = 0;

	imax = imagelength - 1;
	jmax = imagewidth - 1;
	inputline = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(in));
	thisline = (short *)_TIFFmalloc(TIFFSafeMultiply(tmsize_t, imagewidth, sizeof (short)));
	nextline = (short *)_TIFFmalloc(TIFFSafeMultiply(tmsize_t, imagewidth, sizeof (short)));
	outlinesize = TIFFScanlineSize(out);
	outline = (unsigned char *) _TIFFmalloc(outlinesize);
	if (! (inputline && thisline && nextline && outline)) {
	    fprintf(stderr, "Out of memory.\n");
	    goto skip_on_error;
	}

	/*
	 * Get first line
	 */
	if (TIFFReadScanline(in, inputline, 0, 0) <= 0)
            goto skip_on_error;

	inptr = inputline;
	nextptr = nextline;
	for (j = 0; j < imagewidth; ++j)
		*nextptr++ = *inptr++;
	for (i = 1; i < imagelength; ++i) {
		tmpptr = thisline;
		thisline = nextline;
		nextline = tmpptr;
		lastline = (i == imax);
		if (TIFFReadScanline(in, inputline, i, 0) <= 0)
			goto skip_on_error;
		inptr = inputline;
		nextptr = nextline;
		for (j = 0; j < imagewidth; ++j)
			*nextptr++ = *inptr++;
		thisptr = thisline;
		nextptr = nextline;
		_TIFFmemset(outptr = outline, 0, outlinesize);
		bit = 0x80;
		for (j = 0; j < imagewidth; ++j) {
			register int v;

			lastpixel = (j == jmax);
			v = *thisptr++;
			if (v < 0)
				v = 0;
			else if (v > 255)
				v = 255;
			if (v > threshold) {
				*outptr |= bit;
				v -= 255;
			}
			bit >>= 1;
			if (bit == 0) {
				outptr++;
				bit = 0x80;
			}
			if (!lastpixel)
				thisptr[0] += v * 7 / 16;
			if (!lastline) {
				if (j != 0)
					nextptr[-1] += v * 3 / 16;
				*nextptr++ += v * 5 / 16;
				if (!lastpixel)
					nextptr[0] += v / 16;
			}
		}
		if (TIFFWriteScanline(out, outline, i-1, 0) < 0)
			goto skip_on_error;
	}
	goto exit_label;

  skip_on_error:
	errcode = 1;
  exit_label:
	_TIFFfree(inputline);
	_TIFFfree(thisline);
	_TIFFfree(nextline);
	_TIFFfree(outline);
	return errcode;
}

static	uint16_t compression = COMPRESSION_PACKBITS;
static	uint16_t predictor = 0;
static	uint32_t group3options = 0;

static void
processG3Options(char* cp)
{
	if ((cp = strchr(cp, ':'))) {
		do {
			cp++;
			if (strneq(cp, "1d", 2))
				group3options &= ~GROUP3OPT_2DENCODING;
			else if (strneq(cp, "2d", 2))
				group3options |= GROUP3OPT_2DENCODING;
			else if (strneq(cp, "fill", 4))
				group3options |= GROUP3OPT_FILLBITS;
			else
				usage(EXIT_FAILURE);
		} while ((cp = strchr(cp, ':')));
	}
}

static int
processCompressOptions(char* opt)
{
	if (streq(opt, "none"))
		compression = COMPRESSION_NONE;
	else if (streq(opt, "packbits"))
		compression = COMPRESSION_PACKBITS;
	else if (strneq(opt, "g3", 2)) {
		processG3Options(opt);
		compression = COMPRESSION_CCITTFAX3;
	} else if (streq(opt, "g4"))
		compression = COMPRESSION_CCITTFAX4;
	else if (strneq(opt, "lzw", 3)) {
		char* cp = strchr(opt, ':');
		if (cp)
			predictor = atoi(cp+1);
		compression = COMPRESSION_LZW;
	} else if (strneq(opt, "zip", 3)) {
		char* cp = strchr(opt, ':');
		if (cp)
			predictor = atoi(cp+1);
		compression = COMPRESSION_DEFLATE;
	} else
		return (0);
	return (1);
}

int
main(int argc, char* argv[])
{
	TIFF *in, *out;
	uint16_t samplesperpixel, bitspersample = 1, shortv;
	float floatv;
	char thing[1024];
	uint32_t rowsperstrip = (uint32_t) -1;
	uint16_t fillorder = 0;
	int c;
#if !HAVE_DECL_OPTARG
	extern int optind;
	extern char *optarg;
#endif

	while ((c = getopt(argc, argv, "c:f:r:t:h")) != -1)
		switch (c) {
		case 'c':		/* compression scheme */
			if (!processCompressOptions(optarg))
				usage(EXIT_FAILURE);
			break;
		case 'f':		/* fill order */
			if (streq(optarg, "lsb2msb"))
				fillorder = FILLORDER_LSB2MSB;
			else if (streq(optarg, "msb2lsb"))
				fillorder = FILLORDER_MSB2LSB;
			else
				usage(EXIT_FAILURE);
			break;
		case 'r':		/* rows/strip */
			rowsperstrip = atoi(optarg);
			break;
		case 't':
			threshold = atoi(optarg);
			if (threshold < 0)
				threshold = 0;
			else if (threshold > 255)
				threshold = 255;
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
	if (argc - optind < 2)
		usage(EXIT_FAILURE);
	in = TIFFOpen(argv[optind], "r");
	if (in == NULL)
		return (EXIT_FAILURE);
	TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
	if (samplesperpixel != 1) {
		fprintf(stderr, "%s: Not a b&w image.\n", argv[0]);
		return (EXIT_FAILURE);
	}
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8) {
		fprintf(stderr,
		    " %s: Sorry, only handle 8-bit samples.\n", argv[0]);
		return (EXIT_FAILURE);
	}
	out = TIFFOpen(argv[optind+1], "w");
	if (out == NULL)
		return (EXIT_FAILURE);
	CopyField(TIFFTAG_IMAGEWIDTH, imagewidth);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &imagelength);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, imagelength-1);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 1);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 1);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	if (fillorder)
		TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
	else
		CopyField(TIFFTAG_FILLORDER, shortv);
	snprintf(thing, sizeof(thing), "Dithered B&W version of %s", argv[optind]);
	TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, thing);
	CopyField(TIFFTAG_PHOTOMETRIC, shortv);
	CopyField(TIFFTAG_ORIENTATION, shortv);
	CopyField(TIFFTAG_XRESOLUTION, floatv);
	CopyField(TIFFTAG_YRESOLUTION, floatv);
	CopyField(TIFFTAG_RESOLUTIONUNIT, shortv);
        rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip);
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
	switch (compression) {
	case COMPRESSION_CCITTFAX3:
		TIFFSetField(out, TIFFTAG_GROUP3OPTIONS, group3options);
		break;
	case COMPRESSION_LZW:
	case COMPRESSION_DEFLATE:
		if (predictor)
			TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
		break;
	}
	fsdither(in, out);
	TIFFClose(in);
	TIFFClose(out);
	return (EXIT_SUCCESS);
}

static const char usage_info[] =
"Convert a greyscale image to bilevel using dithering\n\n"
"usage: tiffdither [options] input.tif output.tif\n"
"where options are:\n"
" -r #      make each strip have no more than # rows\n"
" -t #      set the threshold value for dithering (default 128)\n"
" -f lsb2msb    force lsb-to-msb FillOrder for output\n"
" -f msb2lsb    force msb-to-lsb FillOrder for output\n"
"\n"
#ifdef LZW_SUPPORT
" -c lzw[:opts] compress output with Lempel-Ziv & Welch encoding\n"
"    #          set predictor value\n"
"    For example, -c lzw:2 for LZW-encoded data with horizontal differencing\n"
#endif
#ifdef ZIP_SUPPORT
" -c zip[:opts] compress output with deflate encoding\n"
"    #          set predictor value\n"
#endif
#ifdef PACKBITS_SUPPORT
" -c packbits   compress output with packbits encoding\n"
#endif
#ifdef CCITT_SUPPORT
" -c g3[:opts]  compress output with CCITT Group 3 encoding\n"
"    Group 3 options:\n"
"    1d         use default CCITT Group 3 1D-encoding\n"
"    2d         use optional CCITT Group 3 2D-encoding\n"
"    fill       byte-align EOL codes\n"
"    For example, -c g3:2d:fill for G3-2D-encoded data with byte-aligned EOLs\n"
" -c g4         compress output with CCITT Group 4 encoding\n"
#endif
#if defined(LZW_SUPPORT) || defined(ZIP_SUPPORT) || defined(PACKBITS_SUPPORT) || defined(CCITT_SUPPORT)
" -c none       use no compression algorithm on output\n"
#endif
"\n"
;

static void
usage(int code)
{
	FILE * out = (code == EXIT_SUCCESS) ? stdout : stderr;

        fprintf(out, "%s\n\n", TIFFGetVersion());
        fprintf(out, "%s", usage_info);
	exit(code);
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
