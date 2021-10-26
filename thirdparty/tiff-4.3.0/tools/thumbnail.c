/*
 * Copyright (c) 1994-1997 Sam Leffler
 * Copyright (c) 1994-1997 Silicon Graphics, Inc.
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
#include <math.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "tiffio.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define	streq(a,b)	(strcmp(a,b) == 0)

#ifndef TIFFhowmany8
# define TIFFhowmany8(x) (((x)&0x07)?((uint32_t)(x)>>3)+1:(uint32_t)(x)>>3)
#endif

typedef enum {
    EXP50,
    EXP60,
    EXP70,
    EXP80,
    EXP90,
    EXP,
    LINEAR
} Contrast;

static	uint32_t tnw = 216;		/* thumbnail width */
static	uint32_t tnh = 274;		/* thumbnail height */
static	Contrast contrast = LINEAR;	/* current contrast */
static	uint8_t* thumbnail;

static	int cpIFD(TIFF*, TIFF*);
static	int generateThumbnail(TIFF*, TIFF*);
static	void initScale();
static	void usage(int code);

#if !HAVE_DECL_OPTARG
extern	char* optarg;
extern	int optind;
#endif

int
main(int argc, char* argv[])
{
    TIFF* in;
    TIFF* out;
    int c;

    while ((c = getopt(argc, argv, "w:h:c:")) != -1) {
	switch (c) {
	case 'w':	tnw = strtoul(optarg, NULL, 0); break;
	case 'h':	tnh = strtoul(optarg, NULL, 0); break;
	case 'c':	contrast = streq(optarg, "exp50") ? EXP50 :
				   streq(optarg, "exp60") ? EXP60 :
				   streq(optarg, "exp70") ? EXP70 :
				   streq(optarg, "exp80") ? EXP80 :
				   streq(optarg, "exp90") ? EXP90 :
				   streq(optarg, "exp")   ? EXP :
				   streq(optarg, "linear")? LINEAR :
							    EXP;
			break;
	default:	usage(EXIT_FAILURE);
	}
    }
    if (argc-optind != 2)
	usage(EXIT_FAILURE);

    out = TIFFOpen(argv[optind+1], "w");
    if (out == NULL)
	return 2;
    in = TIFFOpen(argv[optind], "r");
    if( in == NULL )
        return 2;

    thumbnail = (uint8_t*) _TIFFmalloc(tnw * tnh);
    if (!thumbnail) {
	    TIFFError(TIFFFileName(in),
		      "Can't allocate space for thumbnail buffer.");
	    return EXIT_FAILURE;
    }

    if (in != NULL) {
	initScale();
	do {
	    if (!generateThumbnail(in, out))
		goto bad;
	    if (!cpIFD(in, out) || !TIFFWriteDirectory(out))
		goto bad;
	} while (TIFFReadDirectory(in));
	(void) TIFFClose(in);
    }
    (void) TIFFClose(out);
    return EXIT_SUCCESS;
bad:
    (void) TIFFClose(out);
    return EXIT_FAILURE;
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
	case TIFF_LONG8:
		{ uint64_t longv8;
		  CopyField(tag, longv8);
		}
		break;
	case TIFF_SLONG8:
		{ int64_t longv8;
		  CopyField(tag, longv8);
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
	case TIFF_IFD8:
		{ toff_t ifd8;
		  CopyField(tag, ifd8);
		}
		break;          default:
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
    { TIFFTAG_IMAGEWIDTH,		1, TIFF_LONG },
    { TIFFTAG_IMAGELENGTH,		1, TIFF_LONG },
    { TIFFTAG_BITSPERSAMPLE,		1, TIFF_SHORT },
    { TIFFTAG_COMPRESSION,		1, TIFF_SHORT },
    { TIFFTAG_FILLORDER,		1, TIFF_SHORT },
    { TIFFTAG_SAMPLESPERPIXEL,		1, TIFF_SHORT },
    { TIFFTAG_ROWSPERSTRIP,		1, TIFF_LONG },
    { TIFFTAG_PLANARCONFIG,		1, TIFF_SHORT },
    { TIFFTAG_GROUP3OPTIONS,		1, TIFF_LONG },
    { TIFFTAG_SUBFILETYPE,		1, TIFF_LONG },
    { TIFFTAG_PHOTOMETRIC,		1, TIFF_SHORT },
    { TIFFTAG_THRESHHOLDING,		1, TIFF_SHORT },
    { TIFFTAG_DOCUMENTNAME,		1, TIFF_ASCII },
    { TIFFTAG_IMAGEDESCRIPTION,		1, TIFF_ASCII },
    { TIFFTAG_MAKE,			1, TIFF_ASCII },
    { TIFFTAG_MODEL,			1, TIFF_ASCII },
    { TIFFTAG_ORIENTATION,		1, TIFF_SHORT },
    { TIFFTAG_MINSAMPLEVALUE,		1, TIFF_SHORT },
    { TIFFTAG_MAXSAMPLEVALUE,		1, TIFF_SHORT },
    { TIFFTAG_XRESOLUTION,		1, TIFF_RATIONAL },
    { TIFFTAG_YRESOLUTION,		1, TIFF_RATIONAL },
    { TIFFTAG_PAGENAME,			1, TIFF_ASCII },
    { TIFFTAG_XPOSITION,		1,                     TIFF_RATIONAL },
    { TIFFTAG_YPOSITION,		1,                     TIFF_RATIONAL },
    { TIFFTAG_GROUP4OPTIONS,		1,                 TIFF_LONG },
    { TIFFTAG_RESOLUTIONUNIT,		1,                TIFF_SHORT },
    { TIFFTAG_PAGENUMBER,		2,                    TIFF_SHORT },
    { TIFFTAG_SOFTWARE,			1,                  TIFF_ASCII },
    { TIFFTAG_DATETIME,			1,                  TIFF_ASCII },
    { TIFFTAG_ARTIST,			1,                    TIFF_ASCII },
    { TIFFTAG_HOSTCOMPUTER,		1,                  TIFF_ASCII },
    { TIFFTAG_WHITEPOINT,		2,                    TIFF_RATIONAL },
    { TIFFTAG_PRIMARYCHROMATICITIES,	(uint16_t) -1, TIFF_RATIONAL },
    { TIFFTAG_HALFTONEHINTS,		2,                 TIFF_SHORT },
    { TIFFTAG_BADFAXLINES,		1,                   TIFF_LONG },
    { TIFFTAG_CLEANFAXDATA,		1,                  TIFF_SHORT },
    { TIFFTAG_CONSECUTIVEBADFAXLINES,	1,            TIFF_LONG },
    { TIFFTAG_INKSET,			1,                    TIFF_SHORT },
    /*{ TIFFTAG_INKNAMES,			1, TIFF_ASCII },*/ /* Needs much more complicated logic. See tiffcp */
    { TIFFTAG_DOTRANGE,			2,                  TIFF_SHORT },
    { TIFFTAG_TARGETPRINTER,		1,                 TIFF_ASCII },
    { TIFFTAG_SAMPLEFORMAT,		1,                  TIFF_SHORT },
    { TIFFTAG_YCBCRCOEFFICIENTS,	(uint16_t) -1,     TIFF_RATIONAL },
    { TIFFTAG_YCBCRSUBSAMPLING,		2,              TIFF_SHORT },
    { TIFFTAG_YCBCRPOSITIONING,		1,              TIFF_SHORT },
    { TIFFTAG_REFERENCEBLACKWHITE,	(uint16_t) -1,   TIFF_RATIONAL },
    { TIFFTAG_EXTRASAMPLES,		(uint16_t) -1,      TIFF_SHORT },
};
#define	NTAGS	(sizeof (tags) / sizeof (tags[0]))

static void
cpTags(TIFF* in, TIFF* out)
{
    const struct cpTag *p;
    for (p = tags; p < &tags[NTAGS]; p++)
	{
		/* Horrible: but TIFFGetField() expects 2 arguments to be passed */
		/* if we request a tag that is defined in a codec, but that codec */
		/* isn't used */
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

static int
cpStrips(TIFF* in, TIFF* out)
{
    tsize_t bufsize  = TIFFStripSize(in);
    unsigned char *buf = (unsigned char *)_TIFFmalloc(bufsize);

    if (buf) {
	tstrip_t s, ns = TIFFNumberOfStrips(in);
	uint64_t *bytecounts;

	TIFFGetField(in, TIFFTAG_STRIPBYTECOUNTS, &bytecounts);
	for (s = 0; s < ns; s++) {
	  if (bytecounts[s] > (uint64_t) bufsize) {
		buf = (unsigned char *)_TIFFrealloc(buf, (tmsize_t)bytecounts[s]);
		if (!buf)
		    goto bad;
		bufsize = (tmsize_t)bytecounts[s];
	    }
	    if (TIFFReadRawStrip(in, s, buf, (tmsize_t)bytecounts[s]) < 0 ||
		TIFFWriteRawStrip(out, s, buf, (tmsize_t)bytecounts[s]) < 0) {
		_TIFFfree(buf);
		return 0;
	    }
	}
	_TIFFfree(buf);
	return 1;
    }

bad:
	TIFFError(TIFFFileName(in),
		  "Can't allocate space for strip buffer.");
	return 0;
}

static int
cpTiles(TIFF* in, TIFF* out)
{
    tsize_t bufsize = TIFFTileSize(in);
    unsigned char *buf = (unsigned char *)_TIFFmalloc(bufsize);

    if (buf) {
	ttile_t t, nt = TIFFNumberOfTiles(in);
	uint64_t *bytecounts;

	TIFFGetField(in, TIFFTAG_TILEBYTECOUNTS, &bytecounts);
	for (t = 0; t < nt; t++) {
	    if (bytecounts[t] > (uint64_t) bufsize) {
		buf = (unsigned char *)_TIFFrealloc(buf, (tmsize_t)bytecounts[t]);
		if (!buf)
		    goto bad;
		bufsize = (tmsize_t)bytecounts[t];
	    }
	    if (TIFFReadRawTile(in, t, buf, (tmsize_t)bytecounts[t]) < 0 ||
		TIFFWriteRawTile(out, t, buf, (tmsize_t)bytecounts[t]) < 0) {
		_TIFFfree(buf);
		return 0;
	    }
	}
	_TIFFfree(buf);
	return 1;
    }

bad:
    TIFFError(TIFFFileName(in),
		  "Can't allocate space for tile buffer.");
	return (0);
}

static int
cpIFD(TIFF* in, TIFF* out)
{
    cpTags(in, out);
    if (TIFFIsTiled(in)) {
	if (!cpTiles(in, out))
	    return (0);
    } else {
	if (!cpStrips(in, out))
	    return (0);
    }
    return (1);
}

static	uint16_t	photometric;		/* current photometric of raster */
static	uint16_t	filterWidth;		/* filter width in pixels */
static	uint32_t	stepSrcWidth;		/* src image stepping width */
static	uint32_t	stepDstWidth;		/* dest stepping width */
static	uint8_t* src0;			/* horizontal bit stepping (start) */
static	uint8_t* src1;			/* horizontal bit stepping (middle) */
static	uint8_t* src2;			/* horizontal bit stepping (end) */
static	uint32_t* rowoff;			/* row offset for stepping */
static	uint8_t cmap[256];		/* colormap indexes */
static	uint8_t bits[256];		/* count of bits set */

static void
setupBitsTables()
{
    int i;
    for (i = 0; i < 256; i++) {
	int n = 0;
	if (i&0x01) n++;
	if (i&0x02) n++;
	if (i&0x04) n++;
	if (i&0x08) n++;
	if (i&0x10) n++;
	if (i&0x20) n++;
	if (i&0x40) n++;
	if (i&0x80) n++;
	bits[i] = n;
    }
}

static int clamp(float v, int low, int high)
    { return (v < low ? low : v > high ? high : (int)v); }

#ifndef M_E
#define M_E		2.7182818284590452354
#endif

static void
expFill(float pct[], uint32_t p, uint32_t n)
{
    uint32_t i;
    uint32_t c = (p * n) / 100;
    for (i = 1; i < c; i++)
	pct[i] = (float) (1-exp(i/((double)(n-1)))/ M_E);
    for (; i < n; i++)
	pct[i] = 0.;
}

static void
setupCmap()
{
    float pct[256];			/* known to be large enough */
    uint32_t i;
    pct[0] = 1;				/* force white */
    switch (contrast) {
    case EXP50: expFill(pct, 50, 256); break;
    case EXP60:	expFill(pct, 60, 256); break;
    case EXP70:	expFill(pct, 70, 256); break;
    case EXP80:	expFill(pct, 80, 256); break;
    case EXP90:	expFill(pct, 90, 256); break;
    case EXP:	expFill(pct, 100, 256); break;
    case LINEAR:
	for (i = 1; i < 256; i++)
	    pct[i] = 1-((float)i)/(256-1);
	break;
    }
    switch (photometric) {
    case PHOTOMETRIC_MINISWHITE:
	for (i = 0; i < 256; i++)
	    cmap[i] = clamp(255*pct[(256-1)-i], 0, 255);
	break;
    case PHOTOMETRIC_MINISBLACK:
	for (i = 0; i < 256; i++)
	    cmap[i] = clamp(255*pct[i], 0, 255);
	break;
    }
}

static void
initScale()
{
    src0 = (uint8_t*) _TIFFmalloc(sizeof (uint8_t) * tnw);
    src1 = (uint8_t*) _TIFFmalloc(sizeof (uint8_t) * tnw);
    src2 = (uint8_t*) _TIFFmalloc(sizeof (uint8_t) * tnw);
    rowoff = (uint32_t*) _TIFFmalloc(sizeof (uint32_t) * tnw);
    filterWidth = 0;
    stepDstWidth = stepSrcWidth = 0;
    setupBitsTables();
}

/*
 * Calculate the horizontal accumulation parameteres
 * according to the widths of the src and dst images.
 */
static void
setupStepTables(uint32_t sw)
{
    if (stepSrcWidth != sw || stepDstWidth != tnw) {
	int step = sw;
	int limit = tnw;
	int err = 0;
	uint32_t sx = 0;
	uint32_t x;
	int fw;
	uint8_t b;
	for (x = 0; x < tnw; x++) {
	    uint32_t sx0 = sx;
	    err += step;
	    while (err >= limit) {
		err -= limit;
		sx++;
	    }
	    rowoff[x] = sx0 >> 3;
	    fw = sx - sx0;		/* width */
	    b = (fw < 8) ? 0xff<<(8-fw) : 0xff;
	    src0[x] = b >> (sx0&7);
	    fw -= 8 - (sx0&7);
	    if (fw < 0)
		fw = 0;
	    src1[x] = fw >> 3;
	    fw -= (fw>>3)<<3;
	    src2[x] = 0xff << (8-fw);
	}
	stepSrcWidth = sw;
	stepDstWidth = tnw;
    }
}

static void
setrow(uint8_t* row, uint32_t nrows, const uint8_t* rows[])
{
    uint32_t x;
    uint32_t area = nrows * filterWidth;
    for (x = 0; x < tnw; x++) {
	uint32_t mask0 = src0[x];
	uint32_t fw = src1[x];
	uint32_t mask1 = src1[x];
	uint32_t off = rowoff[x];
	uint32_t acc = 0;
	uint32_t y, i;
	for (y = 0; y < nrows; y++) {
	    const uint8_t* src = rows[y] + off;
	    acc += bits[*src++ & mask0];
	    switch (fw) {
	    default:
		for (i = fw; i > 8; i--)
		    acc += bits[*src++];
		/* fall through... */
	    case 8: acc += bits[*src++]; /* fall through */
	    case 7: acc += bits[*src++]; /* fall through */
	    case 6: acc += bits[*src++]; /* fall through */
	    case 5: acc += bits[*src++]; /* fall through */
	    case 4: acc += bits[*src++]; /* fall through */
	    case 3: acc += bits[*src++]; /* fall through */
	    case 2: acc += bits[*src++]; /* fall through */
	    case 1: acc += bits[*src++]; /* fall through */
	    case 0: break;
	    }
	    acc += bits[*src & mask1];
	}
	*row++ = cmap[(255*acc)/area];
    }
}

/*
 * Install the specified image.  The
 * image is resized to fit the display page using
 * a box filter.  The resultant pixels are mapped
 * with a user-selectable contrast curve.
 */
static void
setImage1(const uint8_t* br, uint32_t rw, uint32_t rh)
{
    int step = rh;
    int limit = tnh;
    int err = 0;
    int bpr = TIFFhowmany8(rw);
    int sy = 0;
    uint8_t* row = thumbnail;
    uint32_t dy;
    for (dy = 0; dy < tnh; dy++) {
	const uint8_t* rows[256];
	uint32_t nrows = 1;
	fprintf(stderr, "bpr=%d, sy=%d, bpr*sy=%d\n", bpr, sy, bpr*sy);
	rows[0] = br + bpr*sy;
	err += step;
	while (err >= limit) {
	    err -= limit;
	    sy++;
	    if (err >= limit)
		{
			/* We should perhaps error loudly, but I can't make sense of that */
			/* code... */
			if( nrows == 256 )
				break;
			rows[nrows++] = br + bpr*sy;
		}
	}
	setrow(row, nrows, rows);
	row += tnw;
    }
}

static void
setImage(const uint8_t* br, uint32_t rw, uint32_t rh)
{
    filterWidth = (uint16_t) ceil((double) rw / (double) tnw);
    setupStepTables(rw);
    setImage1(br, rw, rh);
}

static int
generateThumbnail(TIFF* in, TIFF* out)
{
    unsigned char* raster;
    unsigned char* rp;
    uint32_t sw, sh, rps;
    uint16_t bps, spp;
    tsize_t rowsize, rastersize;
    tstrip_t s, ns = TIFFNumberOfStrips(in);
    toff_t diroff[1];

    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &sw);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &sh);
    TIFFGetFieldDefaulted(in, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetFieldDefaulted(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(in, TIFFTAG_ROWSPERSTRIP, &rps);
    if (spp != 1 || bps != 1)
	return 0;
    rowsize = TIFFScanlineSize(in);
    rastersize = sh * rowsize;
    fprintf(stderr, "rastersize=%u\n", (unsigned int)rastersize);
	/* +3 : add a few guard bytes since setrow() can read a bit */
	/* outside buffer */
    raster = (unsigned char*)_TIFFmalloc(rastersize+3);
    if (!raster) {
	    TIFFError(TIFFFileName(in),
		      "Can't allocate space for raster buffer.");
	    return 0;
    }
    raster[rastersize] = 0;
    raster[rastersize+1] = 0;
    raster[rastersize+2] = 0;
    rp = raster;
    for (s = 0; s < ns; s++) {
	(void) TIFFReadEncodedStrip(in, s, rp, -1);
	rp += rps * rowsize;
    }
    TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &photometric);
    setupCmap();
    setImage(raster, sw, sh);
    _TIFFfree(raster);

    TIFFSetField(out, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (uint32_t) tnw);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, (uint32_t) tnh);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, (uint16_t) 8);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, (uint16_t) 1);
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_PACKBITS);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    cpTag(in, out, TIFFTAG_SOFTWARE, (uint16_t) -1, TIFF_ASCII);
    cpTag(in, out, TIFFTAG_IMAGEDESCRIPTION, (uint16_t) -1, TIFF_ASCII);
    cpTag(in, out, TIFFTAG_DATETIME, (uint16_t) -1, TIFF_ASCII);
    cpTag(in, out, TIFFTAG_HOSTCOMPUTER, (uint16_t) -1, TIFF_ASCII);
    diroff[0] = 0UL;
    TIFFSetField(out, TIFFTAG_SUBIFD, 1, diroff);
    return (TIFFWriteEncodedStrip(out, 0, thumbnail, tnw*tnh) != -1 &&
            TIFFWriteDirectory(out) != -1);
}

const char* usage_info[] = {
"Create a TIFF file with thumbnail images\n\n"
"usage: thumbnail [options] input.tif output.tif",
"where options are:",
" -h #		specify thumbnail image height (default is 274)",
" -w #		specify thumbnail image width (default is 216)",
"",
" -c linear	use linear contrast curve",
" -c exp50	use 50% exponential contrast curve",
" -c exp60	use 60% exponential contrast curve",
" -c exp70	use 70% exponential contrast curve",
" -c exp80	use 80% exponential contrast curve",
" -c exp90	use 90% exponential contrast curve",
" -c exp		use pure exponential contrast curve",
NULL
};

static void
usage(int code)
{
	int i;
	FILE * out = (code == EXIT_SUCCESS) ? stdout : stderr;

        fprintf(out, "%s\n\n", TIFFGetVersion());
	for (i = 0; usage_info[i] != NULL; i++)
		fprintf(out, "%s\n", usage_info[i]);
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
