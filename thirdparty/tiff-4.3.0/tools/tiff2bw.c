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
#include <ctype.h>

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

/* x% weighting -> fraction of full color */
#define	PCT(x)	(((x)*256+50)/100)
int	RED = PCT(30);		/* 30% */
int	GREEN = PCT(59);	/* 59% */
int	BLUE = PCT(11);		/* 11% */

static	void usage(int code);
static	int processCompressOptions(char*);

static void
compresscontig(unsigned char* out, unsigned char* rgb, uint32_t n)
{
	register int v, red = RED, green = GREEN, blue = BLUE;

	while (n-- > 0) {
		v = red*(*rgb++);
		v += green*(*rgb++);
		v += blue*(*rgb++);
		*out++ = v>>8;
	}
}

static void
compresssep(unsigned char* out,
            unsigned char* r, unsigned char* g, unsigned char* b, uint32_t n)
{
	register uint32_t red = RED, green = GREEN, blue = BLUE;

	while (n-- > 0)
		*out++ = (unsigned char)
			((red*(*r++) + green*(*g++) + blue*(*b++)) >> 8);
}

static int
checkcmap(TIFF* tif, int n, uint16_t* r, uint16_t* g, uint16_t* b)
{
	while (n-- > 0)
		if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
			return (16);
	TIFFWarning(TIFFFileName(tif), "Assuming 8-bit colormap");
	return (8);
}

static void
compresspalette(unsigned char* out, unsigned char* data, uint32_t n, uint16_t* rmap, uint16_t* gmap, uint16_t* bmap)
{
	register int v, red = RED, green = GREEN, blue = BLUE;

	while (n-- > 0) {
		unsigned int ix = *data++;
		v = red*rmap[ix];
		v += green*gmap[ix];
		v += blue*bmap[ix];
		*out++ = v>>8;
	}
}

static	uint16_t compression = (uint16_t) -1;
static	uint16_t predictor = 0;
static	int jpegcolormode = JPEGCOLORMODE_RGB;
static	int quality = 75;		/* JPEG quality */

static	void cpTags(TIFF* in, TIFF* out);

int
main(int argc, char* argv[])
{
	uint32_t rowsperstrip = (uint32_t) -1;
	TIFF *in, *out;
	uint32_t w, h;
	uint16_t samplesperpixel;
	uint16_t bitspersample;
	uint16_t config;
	uint16_t photometric;
	uint16_t* red;
	uint16_t* green;
	uint16_t* blue;
	tsize_t rowsize;
	register uint32_t row;
	register tsample_t s;
	unsigned char *inbuf, *outbuf;
	char thing[1024];
	int c;
#if !HAVE_DECL_OPTARG
	extern int optind;
	extern char *optarg;
#endif
        
        in = (TIFF *) NULL;
        out = (TIFF *) NULL;
        inbuf = (unsigned char *) NULL;
        outbuf = (unsigned char *) NULL;

	while ((c = getopt(argc, argv, "c:r:R:G:B:h")) != -1)
		switch (c) {
		case 'c':		/* compression scheme */
			if (!processCompressOptions(optarg))
				usage(EXIT_FAILURE);
			break;
		case 'r':		/* rows/strip */
			rowsperstrip = atoi(optarg);
			break;
		case 'R':
			RED = PCT(atoi(optarg));
			break;
		case 'G':
			GREEN = PCT(atoi(optarg));
			break;
		case 'B':
			BLUE = PCT(atoi(optarg));
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
	photometric = 0;
	TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &photometric);
	if (photometric != PHOTOMETRIC_RGB && photometric != PHOTOMETRIC_PALETTE ) {
		fprintf(stderr,
	    "%s: Bad photometric; can only handle RGB and Palette images.\n",
		    argv[optind]);
                goto tiff2bw_error;
	}
	TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
	if (samplesperpixel != 1 && samplesperpixel != 3) {
		fprintf(stderr, "%s: Bad samples/pixel %u.\n",
		    argv[optind], samplesperpixel);
                goto tiff2bw_error;
	}
	if( photometric == PHOTOMETRIC_RGB && samplesperpixel != 3) {
		fprintf(stderr, "%s: Bad samples/pixel %u for PHOTOMETRIC_RGB.\n",
		    argv[optind], samplesperpixel);
                goto tiff2bw_error;
	}
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8) {
		fprintf(stderr,
		    " %s: Sorry, only handle 8-bit samples.\n", argv[optind]);
                goto tiff2bw_error;
	}
	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &h);
	TIFFGetField(in, TIFFTAG_PLANARCONFIG, &config);

	out = TIFFOpen(argv[optind+1], "w");
	if (out == NULL)
	{
                goto tiff2bw_error;
	}
	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, w);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, h);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 1);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	cpTags(in, out);
	if (compression != (uint16_t) -1) {
		TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
		switch (compression) {
		case COMPRESSION_JPEG:
			TIFFSetField(out, TIFFTAG_JPEGQUALITY, quality);
			TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, jpegcolormode);
			break;
		case COMPRESSION_LZW:
		case COMPRESSION_DEFLATE:
			if (predictor != 0)
				TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
			break;
		}
	}
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
	snprintf(thing, sizeof(thing), "B&W version of %s", argv[optind]);
	TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, thing);
	TIFFSetField(out, TIFFTAG_SOFTWARE, "tiff2bw");
	outbuf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(out));
        if( !outbuf )
        {
            fprintf(stderr, "Out of memory\n");
            goto tiff2bw_error;
        }
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP,
	    TIFFDefaultStripSize(out, rowsperstrip));

#define	pack(a,b)	((a)<<8 | (b))
	switch (pack(photometric, config)) {
	case pack(PHOTOMETRIC_PALETTE, PLANARCONFIG_CONTIG):
	case pack(PHOTOMETRIC_PALETTE, PLANARCONFIG_SEPARATE):
		TIFFGetField(in, TIFFTAG_COLORMAP, &red, &green, &blue);
		/*
		 * Convert 16-bit colormap to 8-bit (unless it looks
		 * like an old-style 8-bit colormap).
		 */
		if (checkcmap(in, 1<<bitspersample, red, green, blue) == 16) {
			int i;
#define	CVT(x)		(((x) * 255L) / ((1L<<16)-1))
			for (i = (1<<bitspersample)-1; i >= 0; i--) {
				red[i] = CVT(red[i]);
				green[i] = CVT(green[i]);
				blue[i] = CVT(blue[i]);
			}
#undef CVT
		}
		inbuf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(in));
                if( !inbuf )
                {
                    fprintf(stderr, "Out of memory\n");
                    goto tiff2bw_error;
                }
		for (row = 0; row < h; row++) {
			if (TIFFReadScanline(in, inbuf, row, 0) < 0)
				break;
			compresspalette(outbuf, inbuf, w, red, green, blue);
			if (TIFFWriteScanline(out, outbuf, row, 0) < 0)
				break;
		}
		break;
	case pack(PHOTOMETRIC_RGB, PLANARCONFIG_CONTIG):
		inbuf = (unsigned char *)_TIFFmalloc(TIFFScanlineSize(in));
                if( !inbuf )
                {
                    fprintf(stderr, "Out of memory\n");
                    goto tiff2bw_error;
                }
		for (row = 0; row < h; row++) {
			if (TIFFReadScanline(in, inbuf, row, 0) < 0)
				break;
			compresscontig(outbuf, inbuf, w);
			if (TIFFWriteScanline(out, outbuf, row, 0) < 0)
				break;
		}
		break;
	case pack(PHOTOMETRIC_RGB, PLANARCONFIG_SEPARATE):
        {
                tmsize_t inbufsize;
		rowsize = TIFFScanlineSize(in);
                inbufsize = TIFFSafeMultiply(tmsize_t, 3, rowsize);
		inbuf = (unsigned char *)_TIFFmalloc(inbufsize);
                if( !inbuf )
                {
                    fprintf(stderr, "Out of memory\n");
                    goto tiff2bw_error;
                }
		for (row = 0; row < h; row++) {
			for (s = 0; s < 3; s++)
				if (TIFFReadScanline(in,
				    inbuf+s*rowsize, row, s) < 0)
                                        goto tiff2bw_error;
			compresssep(outbuf,
			    inbuf, inbuf+rowsize, inbuf+2*rowsize, w);
			if (TIFFWriteScanline(out, outbuf, row, 0) < 0)
				break;
		}
		break;
        }
	}
#undef pack
        if (inbuf)
                _TIFFfree(inbuf);
        if (outbuf)
                _TIFFfree(outbuf);
        TIFFClose(in);
	TIFFClose(out);
	return (EXIT_SUCCESS);

 tiff2bw_error:
        if (inbuf)
                _TIFFfree(inbuf);
        if (outbuf)
                _TIFFfree(outbuf);
        if (out)
                TIFFClose(out);
        if (in)
                TIFFClose(in);
        return (EXIT_FAILURE);
}

static int
processCompressOptions(char* opt)
{
	if (streq(opt, "none"))
		compression = COMPRESSION_NONE;
	else if (streq(opt, "packbits"))
		compression = COMPRESSION_PACKBITS;
	else if (strneq(opt, "jpeg", 4)) {
		char* cp = strchr(opt, ':');

                compression = COMPRESSION_JPEG;
                while( cp )
                {
                    if (isdigit((int)cp[1]))
			quality = atoi(cp+1);
                    else if (cp[1] == 'r' )
			jpegcolormode = JPEGCOLORMODE_RAW;
                    else
                        usage(EXIT_FAILURE);

                    cp = strchr(cp+1,':');
                }
	} else if (strneq(opt, "lzw", 3)) {
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

#define	CopyField(tag, v) \
    if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)
#define	CopyField2(tag, v1, v2) \
    if (TIFFGetField(in, tag, &v1, &v2)) TIFFSetField(out, tag, v1, v2)
#define	CopyField3(tag, v1, v2, v3) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(out, tag, v1, v2, v3)
#define	CopyField4(tag, v1, v2, v3, v4) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3, &v4)) TIFFSetField(out, tag, v1, v2, v3, v4)

static void
cpTag(TIFF* in, TIFF* out, uint16_t tag, uint16_t count, TIFFDataType type)
{
	switch (type) {
	case TIFF_SHORT:
		if (count == 1) {
			uint16_t shortv;
			CopyField(tag, shortv);
		} else if (count == 2) {
			uint16_t shortv1, shortv2;
			CopyField2(tag, shortv1, shortv2);
		} else if (count == 4) {
			uint16_t *tr, *tg, *tb, *ta;
			CopyField4(tag, tr, tg, tb, ta);
		} else if (count == (uint16_t) -1) {
			uint16_t shortv1;
			uint16_t* shortav;
			CopyField2(tag, shortv1, shortav);
		}
		break;
	case TIFF_LONG:
		{ uint32_t longv;
		  CopyField(tag, longv);
		}
		break;
	case TIFF_RATIONAL:
		if (count == 1) {
			float floatv;
			CopyField(tag, floatv);
		} else if (count == (uint16_t) -1) {
			float* floatav;
			CopyField(tag, floatav);
		}
		break;
	case TIFF_ASCII:
		{ char* stringv;
		  CopyField(tag, stringv);
		}
		break;
	case TIFF_DOUBLE:
		if (count == 1) {
			double doublev;
			CopyField(tag, doublev);
		} else if (count == (uint16_t) -1) {
			double* doubleav;
			CopyField(tag, doubleav);
		}
		break;
          default:
                TIFFError(TIFFFileName(in),
                          "Data type %d is not supported, tag %d skipped.",
                          tag, type);
	}
}

#undef CopyField4
#undef CopyField3
#undef CopyField2
#undef CopyField

static const struct cpTag {
	uint16_t	tag;
	uint16_t	count;
	TIFFDataType type;
} tags[] = {
	{ TIFFTAG_SUBFILETYPE,		1, TIFF_LONG },
	{ TIFFTAG_THRESHHOLDING,	1, TIFF_SHORT },
	{ TIFFTAG_DOCUMENTNAME,		1, TIFF_ASCII },
	{ TIFFTAG_IMAGEDESCRIPTION,	1, TIFF_ASCII },
	{ TIFFTAG_MAKE,			1, TIFF_ASCII },
	{ TIFFTAG_MODEL,		1, TIFF_ASCII },
	{ TIFFTAG_MINSAMPLEVALUE,	1, TIFF_SHORT },
	{ TIFFTAG_MAXSAMPLEVALUE,	1, TIFF_SHORT },
	{ TIFFTAG_XRESOLUTION,		1, TIFF_RATIONAL },
	{ TIFFTAG_YRESOLUTION,		1,                 TIFF_RATIONAL },
	{ TIFFTAG_PAGENAME,		1,                    TIFF_ASCII },
	{ TIFFTAG_XPOSITION,		1,                   TIFF_RATIONAL },
	{ TIFFTAG_YPOSITION,		1,                   TIFF_RATIONAL },
	{ TIFFTAG_RESOLUTIONUNIT,	1,                  TIFF_SHORT },
	{ TIFFTAG_SOFTWARE,		1,                    TIFF_ASCII },
	{ TIFFTAG_DATETIME,		1,                    TIFF_ASCII },
	{ TIFFTAG_ARTIST,		1,                      TIFF_ASCII },
	{ TIFFTAG_HOSTCOMPUTER,		1,                TIFF_ASCII },
	{ TIFFTAG_WHITEPOINT,		2,                  TIFF_RATIONAL },
	{ TIFFTAG_PRIMARYCHROMATICITIES,(uint16_t) -1,   TIFF_RATIONAL },
	{ TIFFTAG_HALFTONEHINTS,	2,                   TIFF_SHORT },
	{ TIFFTAG_INKSET,		1,                      TIFF_SHORT },
	{ TIFFTAG_DOTRANGE,		2,                    TIFF_SHORT },
	{ TIFFTAG_TARGETPRINTER,	1,                   TIFF_ASCII },
	{ TIFFTAG_SAMPLEFORMAT,		1,                TIFF_SHORT },
	{ TIFFTAG_YCBCRCOEFFICIENTS,	(uint16_t) -1,   TIFF_RATIONAL },
	{ TIFFTAG_YCBCRSUBSAMPLING,	2,                TIFF_SHORT },
	{ TIFFTAG_YCBCRPOSITIONING,	1,                TIFF_SHORT },
	{ TIFFTAG_REFERENCEBLACKWHITE,	(uint16_t) -1, TIFF_RATIONAL },
	{ TIFFTAG_EXTRASAMPLES,		(uint16_t) -1,    TIFF_SHORT },
	{ TIFFTAG_SMINSAMPLEVALUE,	1,                 TIFF_DOUBLE },
	{ TIFFTAG_SMAXSAMPLEVALUE,	1,                 TIFF_DOUBLE },
	{ TIFFTAG_STONITS,		1,                     TIFF_DOUBLE },
};
#define	NTAGS	(sizeof (tags) / sizeof (tags[0]))

static void
cpTags(TIFF* in, TIFF* out)
{
    const struct cpTag *p;
    for (p = tags; p < &tags[NTAGS]; p++)
    {
        if( p->tag == TIFFTAG_GROUP3OPTIONS )
        {
            uint16_t compression;
            if( !TIFFGetField(in, TIFFTAG_COMPRESSION, &compression) ||
                    compression != COMPRESSION_CCITTFAX3 )
                continue;
        }
        if( p->tag == TIFFTAG_GROUP4OPTIONS )
        {
            uint16_t compression;
            if( !TIFFGetField(in, TIFFTAG_COMPRESSION, &compression) ||
                    compression != COMPRESSION_CCITTFAX4 )
                continue;
        }
        cpTag(in, out, p->tag, p->count, p->type);
    }
}
#undef NTAGS

static const char usage_info[] =
"Convert a color TIFF image to greyscale\n\n"
"usage: tiff2bw [options] input.tif output.tif\n"
"where options are:\n"
" -R %		use #% from red channel\n"
" -G %		use #% from green channel\n"
" -B %		use #% from blue channel\n"
"\n"
" -r #		make each strip have no more than # rows\n"
"\n"
#ifdef LZW_SUPPORT
" -c lzw[:opts]	compress output with Lempel-Ziv & Welch encoding\n"
/* "    LZW options:\n" */
"    #  set predictor value\n"
"    For example, -c lzw:2 for LZW-encoded data with horizontal differencing\n"
#endif
#ifdef ZIP_SUPPORT
" -c zip[:opts]	compress output with deflate encoding\n"
/* "    Deflate (ZIP) options:\n" */
"    #  set predictor value\n"
#endif
#ifdef PACKBITS_SUPPORT
" -c packbits	compress output with packbits encoding\n"
#endif
#ifdef CCITT_SUPPORT
" -c g3[:opts]	compress output with CCITT Group 3 encoding\n"
" -c g4		compress output with CCITT Group 4 encoding\n"
#endif
#if defined(LZW_SUPPORT) || defined(ZIP_SUPPORT) || defined(PACKBITS_SUPPORT) || defined(CCITT_SUPPORT)
" -c none	use no compression algorithm on output\n"
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
