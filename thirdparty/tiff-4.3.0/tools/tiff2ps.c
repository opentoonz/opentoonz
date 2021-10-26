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
#include <stdlib.h>			/* for atof */
#include <math.h>
#include <time.h>
#include <string.h>

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

/*
 * Revision history
 * 2013-Jan-21
 *    Richard Nolde: Fix bug in auto rotate option code. Once a
 *    rotation angle was set by the auto rotate check, it was
 *    retained for all pages that followed instead of being
 *    retested for each page.
 *
 * 2010-Sep-17
 *    Richard Nolde: Reinstate code from Feb 2009 that never got
 *    accepted into CVS with major modifications to handle -H and -W
 *    options. Replaced original PlaceImage function with several
 *    new functions that make support for multiple output pages
 *    from a single image easier to understand. Added additional
 *    warning messages for incompatible command line options.
 *    Add new command line options to specify PageOrientation
 *    Document Structuring Comment for landscape or portrait
 *    and code to determine the values from output width and height
 *    if not specified on the command line.
 *    Add new command line option to specify document creator
 *    as an alterntive to the string "tiff2ps" following model
 *    of patch submitted by Thomas Jarosch for specifying a
 *    document title which is also supported now.
 *
 * 2009-Feb-11
 *    Richard Nolde: Added support for rotations of 90, 180, 270
 *    and auto using -r <90|180|270|auto>.  Auto picks the best
 *    fit for the image on the specified paper size (eg portrait
 *    or landscape) if -h or -w is specified. Rotation is in
 *    degrees counterclockwise since that is how Postscript does
 *    it. The auto opption rotates the image 90 degrees ccw to
 *    produce landscape if that is a better fit than portrait.
 *
 *    Cleaned up code in TIFF2PS and broke into smaller functions
 *    to simplify rotations.
 *
 *    Identified incompatible options and returned errors, eg
 *    -i for imagemask operator is only available for Level2 or
 *    Level3 Postscript in the current implementation since there
 *    is a difference in the way the operands are called for Level1
 *    and there is no function to provide the Level1 version.
 *    -H was not handled properly if -h and/or -w were specified.
 *    It should only clip the masked images if the scaled image
 *    exceeds the maxPageHeight specified with -H.
 *  
 *    New design allows for all of the following combinations:
 *    Conversion of TIFF to Postscript with optional rotations
 *    of 90, 180, 270, or auto degrees counterclockwise
 *    Conversion of TIFF to Postscript with entire image scaled
 *    to maximum of values specified with -h or -w while
 *    maintaining aspect ratio. Same rotations apply.
 *    Conversion of TIFF to Postscript with clipping of output
 *    viewport to height specified with -H, producing multiple
 *    pages at this height and original width as needed.
 *    Same rotations apply.
 *    Conversion of TIFF to Postscript with image scaled to 
 *    maximum specified by -h and -w and the resulting scaled
 *    image is presented in an output viewport clipped by -H height.
 *    The same rotations apply.
 *
 *    Added maxPageWidth option using -W flag. MaxPageHeight and
 *    MaxPageWidth are mutually exclusive since the aspect ratio
 *    cannot be maintained if you set both.
 *    Rewrote PlaceImage to allow maxPageHeight and maxPageWidth
 *    options to work with values smaller or larger than the
 *    physical paper size and still preserve the aspect ratio.
 *    This is accomplished by creating multiple pages across
 *    as well as down if need be.
 *
 * 2001-Mar-21
 *    I (Bruce A. Mallett) added this revision history comment ;)
 *
 *    Fixed PS_Lvl2page() code which outputs non-ASCII85 raw
 *    data.  Moved test for when to output a line break to
 *    *after* the output of a character.  This just serves
 *    to fix an eye-nuisance where the first line of raw
 *    data was one character shorter than subsequent lines.
 *
 *    Added an experimental ASCII85 encoder which can be used
 *    only when there is a single buffer of bytes to be encoded.
 *    This version is much faster at encoding a straight-line
 *    buffer of data because it can avoid a lot of the loop
 *    overhead of the byte-by-byte version.  To use this version
 *    you need to define EXP_ASCII85ENCODER (experimental ...).
 *
 *    Added bug fix given by Michael Schmidt to PS_Lvl2page()
 *    in which an end-of-data marker ('>') was not being output
 *    when producing non-ASCII85 encoded PostScript Level 2
 *    data.
 *
 *    Fixed PS_Lvl2colorspace() so that it no longer assumes that
 *    a TIFF having more than 2 planes is a CMYK.  This routine
 *    no longer looks at the samples per pixel but instead looks
 *    at the "photometric" value.  This change allows support of
 *    CMYK TIFFs.
 *
 *    Modified the PostScript L2 imaging loop so as to test if
 *    the input stream is still open before attempting to do a
 *    flushfile on it.  This was done because some RIPs close
 *    the stream after doing the image operation.
 *
 *    Got rid of the realloc() being done inside a loop in the
 *    PSRawDataBW() routine.  The code now walks through the
 *    byte-size array outside the loop to determine the largest
 *    size memory block that will be needed.
 *
 *    Added "-m" switch to ask tiff2ps to, where possible, use the
 *    "imagemask" operator instead of the "image" operator.
 *
 *    Added the "-i #" switch to allow interpolation to be disabled.
 *
 *    Unrolled a loop or two to improve performance.
 */

/*
 * Define EXP_ASCII85ENCODER if you want to use an experimental
 * version of the ASCII85 encoding routine.  The advantage of
 * using this routine is that tiff2ps will convert to ASCII85
 * encoding at between 3 and 4 times the speed as compared to
 * using the old (non-experimental) encoder.  The disadvantage
 * is that you will be using a new (and unproven) encoding
 * routine.  So user beware, you have been warned!
 */

#define	EXP_ASCII85ENCODER

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

#define DEFAULT_MAX_MALLOC (256 * 1024 * 1024)

/* malloc size limit (in bytes)
 * disabled when set to 0 */
static tmsize_t maxMalloc = DEFAULT_MAX_MALLOC;

int	ascii85 = FALSE;		/* use ASCII85 encoding */
int	interpolate = TRUE;		/* interpolate level2 image */
int	level2 = FALSE;			/* generate PostScript level 2 */
int	level3 = FALSE;			/* generate PostScript level 3 */
int	printAll = FALSE;		/* print all images in file */
int	generateEPSF = TRUE;		/* generate Encapsulated PostScript */
int	PSduplex = FALSE;		/* enable duplex printing */
int	PStumble = FALSE;		/* enable top edge binding */
int	PSavoiddeadzone = TRUE;		/* enable avoiding printer deadzone */
double	maxPageHeight = 0;		/* maximum height to select from image and print per page */
double	maxPageWidth  = 0;		/* maximum width  to select from image and print per page */
double	splitOverlap = 0;		/* amount for split pages to overlag */
int	rotation = 0;                   /* optional value for rotation angle */
int     auto_rotate = 0;                /* rotate image for best fit on the page */
char	*filename = NULL;		/* input filename */
char    *title = NULL;                  /* optional document title string */
char    *creator = NULL;                /* optional document creator string */
char    pageOrientation[12];            /* set optional PageOrientation DSC to Landscape or Portrait */
int	useImagemask = FALSE;		/* Use imagemask instead of image operator */
uint16_t  res_unit = 0;			/* Resolution units: 2 - inches, 3 - cm */

/*
 * ASCII85 Encoding Support.
 */
unsigned char ascii85buf[10];
int	ascii85count;
int	ascii85breaklen;

int	TIFF2PS(FILE*, TIFF*, double, double, double, double, int);
void	PSpage(FILE*, TIFF*, uint32_t, uint32_t);
void	PSColorContigPreamble(FILE*, uint32_t, uint32_t, int);
void	PSColorSeparatePreamble(FILE*, uint32_t, uint32_t, int);
void	PSDataColorContig(FILE*, TIFF*, uint32_t, uint32_t, int);
void	PSDataColorSeparate(FILE*, TIFF*, uint32_t, uint32_t, int);
void	PSDataPalette(FILE*, TIFF*, uint32_t, uint32_t);
void	PSDataBW(FILE*, TIFF*, uint32_t, uint32_t);
void	PSRawDataBW(FILE*, TIFF*, uint32_t, uint32_t);
void	Ascii85Init(void);
void	Ascii85Put(unsigned char code, FILE* fd);
void	Ascii85Flush(FILE* fd);
void    PSHead(FILE*, double, double, double, double);
void	PSTail(FILE*, int);
int     psStart(FILE *, int, int, int *, double *, double, double, double,
                double, double, double, double, double, double, double);
int     psPageSize(FILE *, int, double, double, double, double, double, double);
int     psRotateImage(FILE *, int, double, double, double, double);
int     psMaskImage(FILE *, TIFF *, int, int, int *, double, double,
		    double, double, double, double, double, double, double);
int     psScaleImage(FILE *, double, int, int, double, double, double, double,
                     double, double);
int     get_viewport (double, double, double, double, double *, double *, int);
int     exportMaskedImage(FILE *, double, double, double, double, int, int,
			  double, double, double, int, int);

#if	defined( EXP_ASCII85ENCODER)
tsize_t Ascii85EncodeBlock(uint8_t * ascii85_p, unsigned f_eod, const uint8_t * raw_p, tsize_t raw_l );
#endif

static	void usage(int);

/**
 * This custom malloc function enforce a maximum allocation size
 */
static void* limitMalloc(tmsize_t s)
{
	if (maxMalloc && (s > maxMalloc)) {
		fprintf(stderr, "MemoryLimitError: allocation of %" TIFF_SSIZE_FORMAT " bytes is forbidden. Limit is %" TIFF_SSIZE_FORMAT ".\n",
                s, maxMalloc);
		fprintf(stderr, "                  use -M option to change limit.\n");
		return NULL;
	}
	return _TIFFmalloc(s);
}

int
main(int argc, char* argv[])
{
	int dirnum = -1, c, np = 0;
	int centered = 0;
	double bottommargin = 0;
	double leftmargin = 0;
	double pageWidth = 0;
	double pageHeight = 0;
	uint32_t diroff = 0;
#if !HAVE_DECL_OPTARG
	extern char *optarg;
	extern int optind;
#endif
	FILE* output = stdout;

        pageOrientation[0] = '\0';

	while ((c = getopt(argc, argv, "b:d:h:H:W:L:M:i:w:l:o:O:P:C:r:t:acemxyzps1238DT")) != -1)
		switch (c) {
		case 'M':
			maxMalloc = (tmsize_t)strtoul(optarg, NULL, 0) << 20;
			break;
		case 'b':
			bottommargin = atof(optarg);
			break;
		case 'c':
			centered = 1;
			break;
		case 'C':
			creator = optarg;
			break;
		case 'd': /* without -a, this only processes one image at this IFD */
			dirnum = atoi(optarg);
			break;
		case 'D':
			PSduplex = TRUE;
			break;
		case 'i':
			interpolate = atoi(optarg) ? TRUE:FALSE;
			break;
		case 'T':
			PStumble = TRUE;
			break;
		case 'e':
                        PSavoiddeadzone = FALSE;
			generateEPSF = TRUE;
			break;
		case 'h':
			pageHeight = atof(optarg);
			break;
		case 'H':
			maxPageHeight = atof(optarg);
			break;
		case 'W':
			maxPageWidth = atof(optarg);
			break;
		case 'L':
			splitOverlap = atof(optarg);
			break;
		case 'm':
			useImagemask = TRUE;
			break;
		case 'o':
		        switch (optarg[0])
                          {
                          case '0':
                          case '1':
                          case '2':
                          case '3':
                          case '4':
                          case '5':
                          case '6':
                          case '7':
                          case '8':
                          case '9': diroff = (uint32_t) strtoul(optarg, NULL, 0);
			          break;
                          default: TIFFError ("-o", "Offset must be a numeric value.");
			    exit (EXIT_FAILURE);
			  }
			break;
		case 'O':		/* XXX too bad -o is already taken */
			output = fopen(optarg, "w");
			if (output == NULL) {
				fprintf(stderr,
				    "%s: %s: Cannot open output file.\n",
				    argv[0], optarg);
			    exit (EXIT_FAILURE);
			}
			break;
		case 'P':
			switch (optarg[0])
			{
			  case 'l':
			  case 'L': strcpy (pageOrientation, "Landscape");
				break;
			  case 'p':
			  case 'P': strcpy (pageOrientation, "Portrait");
				break;
			  default: TIFFError ("-P", "Page orientation must be Landscape or Portrait");
			    exit (EXIT_FAILURE);
			}
			break;
		case 'l':
			leftmargin = atof(optarg);
			break;
		case 'a': /* removed fall through to generate warning below, R Nolde 09-01-2010 */
			printAll = TRUE;
			break;
		case 'p':
			generateEPSF = FALSE;
			break;
		case 'r':
                        if (strcmp (optarg, "auto") == 0)
			  {
                          rotation = 0;
                          auto_rotate = TRUE;
                          }
                        else
			  {
 			  rotation = atoi(optarg);
                          auto_rotate = FALSE;
			  }
                        switch (rotation)
                          {
			  case   0:
                          case  90:
                          case 180:
                          case 270:
			    break;
			  default:
                            fprintf (stderr, "Rotation angle must be 90, 180, 270 (degrees ccw) or auto\n");
			    exit (EXIT_FAILURE);
			  }
			break;
		case 's':
			printAll = FALSE;
			break;
                case 't':
                        title = optarg;
                        break;
		case 'w':
			pageWidth = atof(optarg);
			break;
		case 'z':
			PSavoiddeadzone = FALSE;
			break;
		case '1':
			level2 = FALSE;
			level3 = FALSE;
			ascii85 = FALSE;
			break;
		case '2':
			level2 = TRUE;
			ascii85 = TRUE;			/* default to yes */
			break;
		case '3':
			level3 = TRUE;
			ascii85 = TRUE;			/* default to yes */
			break;
		case '8':
			ascii85 = FALSE;
			break;
		case 'x':
			res_unit = RESUNIT_CENTIMETER;
			break;
		case 'y':
			res_unit = RESUNIT_INCH;
			break;
		case '?':
			usage(EXIT_FAILURE);
		}

        if (useImagemask == TRUE)
          {
	  if ((level2 == FALSE) && (level3 == FALSE))
            {
	    TIFFError ("-m "," imagemask operator requires Postscript Level2 or Level3");
	    exit (EXIT_FAILURE);
            }
          }

        if (pageWidth && (maxPageWidth > pageWidth))
	  {
	  TIFFError ("-W", "Max viewport width cannot exceed page width");
	  exit (EXIT_FAILURE);
          }

        /* auto rotate requires a specified page width and height */
        if (auto_rotate == TRUE)
          {
	    /*
	  if ((pageWidth == 0) || (pageHeight == 0))
	    TIFFWarning ("-r auto", " requires page height and width specified with -h and -w");
	    */
          if ((maxPageWidth > 0) || (maxPageHeight > 0))
            {
	    TIFFError ("-r auto", " is incompatible with maximum page width/height specified by -H or -W");
            exit (EXIT_FAILURE);
            }
          }
        if ((maxPageWidth > 0) && (maxPageHeight > 0))
            {
	    TIFFError ("-H and -W", " Use only one of -H or -W to define a viewport");
            exit (EXIT_FAILURE);
            }

        if ((generateEPSF == TRUE) && (printAll == TRUE))
          {
	  TIFFError(" -e and -a", "Warning: Cannot generate Encapsulated Postscript for multiple images");
	  generateEPSF = FALSE;
          }

        if ((generateEPSF == TRUE) && (PSduplex == TRUE))
          {
	  TIFFError(" -e and -D", "Warning: Encapsulated Postscript does not support Duplex option");
	  PSduplex = FALSE;
          }

        if ((generateEPSF == TRUE) && (PStumble == TRUE))
          {
	  TIFFError(" -e and -T", "Warning: Encapsulated Postscript does not support Top Edge Binding option");
	  PStumble = FALSE;
          }

        if ((generateEPSF == TRUE) && (PSavoiddeadzone == TRUE))
	  PSavoiddeadzone = FALSE;

	for (; argc - optind > 0; optind++) {
		TIFF* tif = TIFFOpen(filename = argv[optind], "r");
		if (tif != NULL) {
			if (dirnum != -1
                            && !TIFFSetDirectory(tif, (tdir_t)dirnum))
                        {
                                TIFFClose(tif);
				return (EXIT_FAILURE);
                        }
			else if (diroff != 0 &&
			    !TIFFSetSubDirectory(tif, diroff))
                        {
                                TIFFClose(tif);
				return (EXIT_FAILURE);
                        }
			np = TIFF2PS(output, tif, pageWidth, pageHeight,
				     leftmargin, bottommargin, centered);
                        if (np < 0)
                          {
			  TIFFError("Error", "Unable to process %s", filename);
                          }
			TIFFClose(tif);
		}
	}
	if (np)
		PSTail(output, np);
	else
		usage(EXIT_FAILURE);
	if (output != stdout)
		fclose(output);
	return (EXIT_SUCCESS);
}

static	uint16_t samplesperpixel;
static	uint16_t bitspersample;
static	uint16_t planarconfiguration;
static	uint16_t photometric;
static	uint16_t compression;
static	uint16_t extrasamples;
static	int alpha;

static int
checkImage(TIFF* tif)
{
	switch (photometric) {
	case PHOTOMETRIC_YCBCR:
		if ((compression == COMPRESSION_JPEG || compression == COMPRESSION_OJPEG)
			&& planarconfiguration == PLANARCONFIG_CONTIG) {
			/* can rely on libjpeg to convert to RGB */
			TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE,
				     JPEGCOLORMODE_RGB);
			photometric = PHOTOMETRIC_RGB;
		} else {
			if (level2 || level3)
				break;
			TIFFError(filename, "Can not handle image with %s",
			    "PhotometricInterpretation=YCbCr");
			return (0);
		}
		/* fall through... */
	case PHOTOMETRIC_RGB:
		if (alpha && bitspersample != 8) {
			TIFFError(filename,
			    "Can not handle %"PRIu16"-bit/sample RGB image with alpha",
			    bitspersample);
			return (0);
		}
		/* fall through... */
	case PHOTOMETRIC_SEPARATED:
	case PHOTOMETRIC_PALETTE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		break;
	case PHOTOMETRIC_LOGL:
	case PHOTOMETRIC_LOGLUV:
		if (compression != COMPRESSION_SGILOG &&
		    compression != COMPRESSION_SGILOG24) {
			TIFFError(filename,
		    "Can not handle %s data with compression other than SGILog",
			    (photometric == PHOTOMETRIC_LOGL) ?
				"LogL" : "LogLuv"
			);
			return (0);
		}
		/* rely on library to convert to RGB/greyscale */
		TIFFSetField(tif, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_8BIT);
		photometric = (photometric == PHOTOMETRIC_LOGL) ?
		    PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB;
		bitspersample = 8;
		break;
	case PHOTOMETRIC_CIELAB:
		/* fall through... */
	default:
		TIFFError(filename,
		    "Can not handle image with PhotometricInterpretation=%"PRIu16,
		    photometric);
		return (0);
	}
	switch (bitspersample) {
	case 1: case 2:
	case 4: case 8:
	case 16:
		break;
	default:
		TIFFError(filename, "Can not handle %"PRIu16"-bit/sample image",
		    bitspersample);
		return (0);
	}
	if (planarconfiguration == PLANARCONFIG_SEPARATE && extrasamples > 0)
		TIFFWarning(filename, "Ignoring extra samples");
	return (1);
}

#define PS_UNIT_SIZE	72.0F
#define	PSUNITS(npix,res)	((npix) * (PS_UNIT_SIZE / (res)))

static const char RGBcolorimage[] = "\
/bwproc {\n\
    rgbproc\n\
    dup length 3 idiv string 0 3 0\n\
    5 -1 roll {\n\
	add 2 1 roll 1 sub dup 0 eq {\n\
	    pop 3 idiv\n\
	    3 -1 roll\n\
	    dup 4 -1 roll\n\
	    dup 3 1 roll\n\
	    5 -1 roll put\n\
	    1 add 3 0\n\
	} { 2 1 roll } ifelse\n\
    } forall\n\
    pop pop pop\n\
} def\n\
/colorimage where {pop} {\n\
    /colorimage {pop pop /rgbproc exch def {bwproc} image} bind def\n\
} ifelse\n\
";

/*
 * Adobe Photoshop requires a comment line of the form:
 *
 * %ImageData: <cols> <rows> <depth>  <main channels> <pad channels>
 *	<block size> <1 for binary|2 for hex> "data start"
 *
 * It is claimed to be part of some future revision of the EPS spec.
 */
static void
PhotoshopBanner(FILE* fd, uint32_t w, uint32_t h, int bs, int nc, const char* startline)
{
	fprintf(fd, "%%ImageData: %"PRIu32" %"PRIu32" %"PRIu16" %d 0 %d 2 \"",
	    w, h, bitspersample, nc, bs);
	fprintf(fd, startline, nc);
	fprintf(fd, "\"\n");
}

/*   Convert pixel width and height pw, ph, to points pprw, pprh 
 *   using image resolution and resolution units from TIFF tags.
 *   pw : image width in pixels
 *   ph : image height in pixels
 * pprw : image width in PS units (72 dpi)
 * pprh : image height in PS units (72 dpi)
 */
static void
setupPageState(TIFF* tif, uint32_t* pw, uint32_t* ph, double* pprw, double* pprh)
{
	float xres = 0.0F, yres = 0.0F;

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, pw);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, ph);
	if (res_unit == 0)	/* Not specified as command line option */
		if (!TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &res_unit))
			res_unit = RESUNIT_INCH;
	/*
	 * Calculate printable area.
	 */
	if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres)
            || fabs(xres) < 0.0000001)
		xres = PS_UNIT_SIZE;
	if (!TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)
            || fabs(yres) < 0.0000001)
		yres = PS_UNIT_SIZE;
	switch (res_unit) {
	case RESUNIT_CENTIMETER:
		xres *= 2.54F, yres *= 2.54F;
		break;
	case RESUNIT_INCH:
		break;
	case RESUNIT_NONE:	/* Subsequent code assumes we have converted to inches! */
		res_unit = RESUNIT_INCH;
		break;
	default:	/* Last ditch guess for unspecified RESUNIT case
			 * check that the resolution is not inches before scaling it.
			 * Moved to end of function with additional check, RJN, 08-31-2010
			 * if (xres != PS_UNIT_SIZE || yres != PS_UNIT_SIZE)
			 * xres *= PS_UNIT_SIZE, yres *= PS_UNIT_SIZE;
			 */
		break;
	}
	/* This is a hack to deal with images that have no meaningful Resolution Size
	 * but may have x and/or y resolutions of 1 pixel per undefined unit.
	 */
	if ((xres > 1.0) && (xres != PS_UNIT_SIZE))
		*pprw = PSUNITS(*pw, xres);
	else
		*pprw = PSUNITS(*pw, PS_UNIT_SIZE);
	if ((yres > 1.0) && (yres != PS_UNIT_SIZE))
		*pprh = PSUNITS(*ph, yres);
	else
		*pprh = PSUNITS(*ph, PS_UNIT_SIZE);
}

static int
isCCITTCompression(TIFF* tif)
{
    uint16_t compress;
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compress);
    return (compress == COMPRESSION_CCITTFAX3 ||
	    compress == COMPRESSION_CCITTFAX4 ||
	    compress == COMPRESSION_CCITTRLE ||
	    compress == COMPRESSION_CCITTRLEW);
}

static	tsize_t tf_bytesperrow;
static	tsize_t ps_bytesperrow;
static	uint32_t	tf_rowsperstrip;
static	uint32_t	tf_numberstrips;
static	char *hex = "0123456789abcdef";

/*
 * Pagewidth and pageheight are the output size in points,
 * may refer to values specified with -h and -w, or to
 * values read from the image if neither -h nor -w are used.
 * Imagewidth and imageheight are image size in points.
 * Ximages and Yimages are number of pages across and down.
 * Only one of maxPageHeight or maxPageWidth can be used.
 * These are global variables unfortunately.
 */
int get_subimage_count(double pagewidth,  double pageheight,
		       double imagewidth, double imageheight,
		       int *ximages, int *yimages,
		       int rotation, double scale)
{
	int pages = 1;
	double splitheight = 0;  /* Requested Max Height in points */
	double splitwidth  = 0;  /* Requested Max Width in points */
	double overlap     = 0;  /* Repeated edge width in points */

	splitheight = maxPageHeight * PS_UNIT_SIZE;
	splitwidth  = maxPageWidth  * PS_UNIT_SIZE;
	overlap     = splitOverlap  * PS_UNIT_SIZE;
	pagewidth  *= PS_UNIT_SIZE;
	pageheight *= PS_UNIT_SIZE;

	if ((imagewidth < 1.0) || (imageheight < 1.0))
	{
		TIFFError("get_subimage_count", "Invalid image width or height");
		return (0);
	}

  switch (rotation)
    {
    case 0:
    case 180: if (splitheight > 0) /* -H maxPageHeight */
                {
               if (imageheight > splitheight) /* More than one vertical image segment */
                 {
                 if (pagewidth)
                   *ximages = (int)ceil((scale * imagewidth)  / (pagewidth - overlap));
                  else
                   *ximages = 1;
                 *yimages = (int)ceil((scale * imageheight) / (splitheight - overlap)); /* Max vert pages needed */
                 }
                else
                 {
                 if (pagewidth)
                   *ximages = (int)ceil((scale * imagewidth) / (pagewidth - overlap));    /* Max horz pages needed */
                  else
                   *ximages = 1;
                 *yimages = 1;                                                     /* Max vert pages needed */
                 }
               }
              else
               {
                if (splitwidth > 0) /* -W maxPageWidth */
                 {
                 if (imagewidth >splitwidth)
                   {
                   *ximages = (int)ceil((scale * imagewidth)  / (splitwidth - overlap));   /* Max horz pages needed */
                    if (pageheight)
                     *yimages = (int)ceil((scale * imageheight) / (pageheight - overlap)); /* Max vert pages needed */
                    else
                     *yimages = 1;
                   }
                  else
                   {
                   *ximages = 1;                                                     /* Max vert pages needed */
                    if (pageheight)
                     *yimages = (int)ceil((scale * imageheight) / (pageheight - overlap)); /* Max vert pages needed */
                    else
                     *yimages = 1;
                   }
                 }
                else
                 {
                 *ximages = 1;
                 *yimages = 1;
                 }
               }
             break;
    case 90:
    case 270: if (splitheight > 0) /* -H maxPageHeight */
                {
               if (imagewidth > splitheight) /* More than one vertical image segment */
                 {
                 *yimages = (int)ceil((scale * imagewidth) / (splitheight - overlap)); /* Max vert pages needed */
                  if (pagewidth)
                   *ximages = (int)ceil((scale * imageheight) / (pagewidth - overlap));   /* Max horz pages needed */
                  else
                   *ximages = 1;
                 }
                else
                 {
                 *yimages = 1;                                                     /* Max vert pages needed */
                  if (pagewidth)
                   *ximages = (int)ceil((scale * imageheight) / (pagewidth - overlap));    /* Max horz pages needed */
                  else
                   *ximages = 1;
                 }
               }
              else
               {
                if (splitwidth > 0) /* -W maxPageWidth */
                 {
                 if (imageheight > splitwidth)
                   {
                   if (pageheight)
                     *yimages = (int)ceil((scale * imagewidth) / (pageheight - overlap)); /* Max vert pages needed */
                    else
                     *yimages = 1;
                   *ximages = (int)ceil((scale * imageheight)  / (splitwidth - overlap));   /* Max horz pages needed */
                   }
                  else
                   {
                   if (pageheight)
                     *yimages = (int)ceil((scale * imagewidth) / (pageheight - overlap));  /* Max horz pages needed */
                    else
                     *yimages = 1;
                   *ximages = 1;                                                     /* Max vert pages needed */
                   }
                 }
                else
                 {
                 *ximages = 1;
                 *yimages = 1;
                 }
               }
             break;
    default:  *ximages = 1;
             *yimages = 1;
  }
  pages = (*ximages) * (*yimages);
  return (pages);
  }

/* New version of PlaceImage that handles only the translation and rotation
 * for a single output page.
 */
int exportMaskedImage(FILE *fp, double pagewidth, double pageheight,
                     double imagewidth, double imageheight,
                      int row, int column,
                      double left_offset, double bott_offset,
                     double scale, int center, int rotation)
  {
  double xtran = 0.0;
  double ytran = 0.0;

  double xscale = 1.0;
  double yscale = 1.0;

  double splitheight    = 0;  /* Requested Max Height in points */
  double splitwidth     = 0;  /* Requested Max Width in points */
  double overlap        = 0;  /* Repeated edge width in points */
  double subimage_height = 0.0;

  splitheight = maxPageHeight * PS_UNIT_SIZE;
  splitwidth  = maxPageWidth  * PS_UNIT_SIZE;
  overlap     = splitOverlap  * PS_UNIT_SIZE;
  xscale = scale * imagewidth;
  yscale = scale * imageheight;

  if ((xscale < 0.0) || (yscale < 0.0))
    {
    TIFFError("exportMaskedImage", "Invalid parameters.");
    return (-1);
    }

  /* If images are cropped to a vewport with -H or -W, the output pages are shifted to
   * the top of each output page rather than the Postscript default lower edge.
   */
  switch (rotation)
    {
    case 0:
    case 180: if (splitheight > 0) /* -H maxPageHeight */
                {
               if (splitheight < imageheight) /* More than one vertical image segments */
                 {
                 /* Intra2net: Keep correct apspect ratio */
                 xscale = (imagewidth + overlap) * (pageheight / splitheight) * scale;

                 xtran = -1.0 * column * (pagewidth - overlap);
                  subimage_height = imageheight - ((splitheight - overlap) * row);
                 ytran  = pageheight - subimage_height * (pageheight / splitheight);
                  }
                else  /* Only one page in vertical direction */
                 {
                 xtran = -1.0 * column * (pagewidth - overlap);
                  ytran = splitheight - imageheight;
                 }
               }
              else
               {
                if (splitwidth > 0) /* maxPageWidth */
                 {
                 if (splitwidth < imagewidth)
                   {
                   xtran = -1.0  * column * splitwidth;
                   ytran = -1.0 * row * (pageheight - overlap);
                    }
                  else /* Only one page in horizontal direction */
                   {
                    ytran = -1.0 * row * (pageheight - overlap);
                    xtran = 0;
                   }
                 }
                else    /* Simple case, no splitting */
                 {
                 ytran = pageheight - imageheight;
                 xtran = 0;
                  }
                }

            if (imagewidth <= pagewidth) {
                /* Intra2net: Crop page at the bottom instead of the top (-> output starts at the top).
                     Only do this in non-page-split mode */
                if (imageheight <= splitheight) {
                    ytran = pageheight - imageheight; /* Note: Will be negative for images longer than page size */
                }
            }
              bott_offset += ytran / (center ? 2 : 1);
              left_offset += xtran / (center ? 2 : 1);
              break;
    case  90:
    case 270:  if (splitheight > 0) /* -H maxPageHeight */
                {
               if (splitheight < imagewidth) /* More than one vertical image segments */
                 {
                 xtran = -1.0 * column * (pageheight - overlap);
                 /* Commented code places image at bottom of page instead of top.
                     ytran = -1.0 * row * splitheight;
                   */
                  if (row == 0)
                    ytran = -1.0 * (imagewidth - splitheight);
                  else
                    ytran = -1.0 * (imagewidth - (splitheight - overlap) * (row + 1));
                  }
                else  /* Only one page in vertical direction */
                 {
                  xtran = -1.0 * column * (pageheight - overlap);
                  ytran = splitheight - imagewidth;
                 }
		}
              else
               {
                if (splitwidth > 0) /* maxPageWidth */
                 {
                 if (splitwidth < imageheight)
                   {
                    xtran = -1.0  * column * splitwidth;
                    ytran = -1.0 * row * (pagewidth - overlap);
                    }
                  else /* Only one page in horizontal direction */
                   {
                    ytran = -1.0 * row * (pagewidth - overlap);
                    xtran = 0;
                   }
                 }
                else    /* Simple case, no splitting */
                 {
                 ytran = pageheight - imageheight;
                 xtran = 0; /* pagewidth  - imagewidth; */
                  }
                }
              bott_offset += ytran / (center ? 2 : 1);
              left_offset += xtran / (center ? 2 : 1);
              break;
    default:  xtran = 0;
             ytran = 0;
    }

  switch (rotation)
    {
    case   0: fprintf(fp, "%f %f translate\n", left_offset, bott_offset);
              fprintf(fp, "%f %f scale\n", xscale, yscale);
             break;
    case 180: fprintf(fp, "%f %f translate\n", left_offset, bott_offset);
              fprintf(fp, "%f %f scale\n1 1 translate 180 rotate\n",  xscale, yscale);
              break;
    case  90: fprintf(fp, "%f %f translate\n", left_offset, bott_offset);
              fprintf(fp, "%f %f scale\n1 0 translate 90 rotate\n", yscale, xscale);
              break;
    case 270: fprintf(fp, "%f %f translate\n", left_offset, bott_offset);
              fprintf(fp, "%f %f scale\n0 1 translate 270 rotate\n", yscale, xscale);
              break;
    default:  TIFFError ("exportMaskedImage", "Unsupported rotation angle %d. No rotation", rotation);
             fprintf( fp, "%f %f scale\n", xscale, yscale);
              break;
    }

  return (0);
  }

/* Rotate an image without scaling or clipping */
int  psRotateImage (FILE * fd, int rotation, double pswidth, double psheight,
                    double left_offset, double bottom_offset)
  {
  if ((left_offset != 0.0) || (bottom_offset != 0))
    fprintf (fd, "%f %f translate\n", left_offset, bottom_offset);

  /* Exchange width and height for 90/270 rotations */
  switch (rotation)
    {
    case   0: fprintf (fd, "%f %f scale\n", pswidth, psheight);
              break;
    case  90: fprintf (fd, "%f %f scale\n1 0 translate 90 rotate\n", psheight, pswidth);
              break;
    case 180: fprintf (fd, "%f %f scale\n1 1 translate 180 rotate\n", pswidth, psheight);
              break;
    case 270: fprintf (fd, "%f %f scale\n0 1 translate 270 rotate\n", psheight, pswidth);
              break;
    default:  TIFFError ("psRotateImage", "Unsupported rotation %d.", rotation);
             fprintf( fd, "%f %f scale\n", pswidth, psheight);
              return (1);
    }
  return (0);
  }

/* Scale and rotate an image to a single output page. */
int psScaleImage(FILE * fd, double scale, int rotation, int center,
                 double reqwidth, double reqheight, double pswidth, double psheight,
                 double left_offset, double bottom_offset)
  {
  double hcenter = 0.0, vcenter = 0.0;

  /* Adjust offsets for centering */
  if (center)
    {
    switch (rotation)
      {
      case   90: vcenter = (reqheight - pswidth * scale) / 2;
                hcenter = (reqwidth - psheight * scale) / 2;
                 fprintf (fd, "%f %f translate\n", hcenter, vcenter);
                 fprintf (fd, "%f %f scale\n1 0 translate 90 rotate\n", psheight * scale, pswidth * scale);
                 break;
      case  180: hcenter = (reqwidth - pswidth * scale) / 2;
                vcenter = (reqheight - psheight * scale) / 2;
                 fprintf (fd, "%f %f translate\n", hcenter, vcenter);
                 fprintf (fd, "%f %f scale\n1 1 translate 180 rotate\n", pswidth * scale, psheight * scale);
                 break;
      case  270: vcenter = (reqheight - pswidth * scale) / 2;
                hcenter = (reqwidth - psheight * scale) / 2;
                 fprintf (fd, "%f %f translate\n", hcenter, vcenter);
                 fprintf (fd, "%f %f scale\n0 1 translate 270 rotate\n", psheight * scale, pswidth * scale);
                 break;
      case    0:
      default:   hcenter = (reqwidth - pswidth * scale) / 2;
                vcenter = (reqheight - psheight * scale) / 2;
                 fprintf (fd, "%f %f translate\n", hcenter, vcenter);
                 fprintf (fd, "%f %f scale\n", pswidth * scale, psheight * scale);
                 break;
      }
    }
  else  /* Not centered */
    {
    switch (rotation)
      {
      case 0:   fprintf (fd, "%f %f translate\n", left_offset ? left_offset : 0.0,
                         bottom_offset ? bottom_offset : reqheight - (psheight * scale));
                fprintf (fd, "%f %f scale\n", pswidth * scale, psheight * scale);
                break;
      case 90:  fprintf (fd, "%f %f translate\n", left_offset ? left_offset : 0.0,
                         bottom_offset ? bottom_offset : reqheight - (pswidth * scale));
                fprintf (fd, "%f %f scale\n1 0 translate 90 rotate\n", psheight * scale, pswidth * scale);
                break;
      case 180: fprintf (fd, "%f %f translate\n", left_offset ? left_offset : 0.0,
                         bottom_offset ? bottom_offset : reqheight - (psheight * scale));
                fprintf (fd, "%f %f scale\n1 1 translate 180 rotate\n", pswidth * scale, psheight * scale);
                break;
      case 270: fprintf (fd, "%f %f translate\n", left_offset ? left_offset : 0.0,
                         bottom_offset ? bottom_offset : reqheight - (pswidth * scale));
                fprintf (fd, "%f %f scale\n0 1 translate 270 rotate\n", psheight * scale, pswidth * scale);
                break;
      default:  TIFFError ("psScaleImage", "Unsupported rotation  %d", rotation);
               fprintf (fd, "%f %f scale\n", pswidth * scale, psheight * scale);
                return (1);
      }
    }

  return (0);
  }

/* This controls the visible portion of the page which is displayed.
 * N.B. Setting maxPageHeight no longer sets pageheight if not set explicitly
 */
int psPageSize (FILE * fd, int rotation, double pgwidth, double pgheight,
                double reqwidth, double reqheight, double pswidth, double psheight)
  {
  double xscale = 1.0, yscale = 1.0, scale = 1.0;
  double splitheight;
  double splitwidth;
  double new_width;
  double new_height;

  splitheight = maxPageHeight * PS_UNIT_SIZE;
  splitwidth  = maxPageWidth  * PS_UNIT_SIZE;

  switch (rotation)
    {
    case   0:
    case 180: if ((splitheight > 0) || (splitwidth > 0))
                {
               if (pgwidth != 0 || pgheight != 0)
                  {
                 xscale = reqwidth / (splitwidth ? splitwidth : pswidth);
                 yscale = reqheight / (splitheight ? splitheight : psheight);
                  scale = (xscale < yscale) ? xscale : yscale;
                  }
                new_width = splitwidth ? splitwidth : scale * pswidth;
                new_height = splitheight ? splitheight : scale * psheight;
                if (strlen(pageOrientation))
                  fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                else
                  fprintf (fd, "%%%%PageOrientation: %s\n", (new_width > new_height) ? "Landscape" : "Portrait");
                fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)new_width, (int32_t)new_height);
                fprintf (fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                       new_width, new_height);
                }
             else /* No viewport defined with -H or -W */
                {
                if ((pgwidth == 0) && (pgheight == 0)) /* Image not scaled */
                  {
                  if (strlen(pageOrientation))
                    fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                  else
                    fprintf (fd, "%%%%PageOrientation: %s\n", (pswidth > psheight) ? "Landscape" : "Portrait");
                 fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)pswidth, (int32_t)psheight);
                  fprintf(fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                          pswidth, psheight);
                  }
               else /* Image scaled */
                  {
                  if (strlen(pageOrientation))
                    fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                  else
                    fprintf (fd, "%%%%PageOrientation: %s\n", (reqwidth > reqheight) ? "Landscape" : "Portrait");
                 fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)reqwidth, (int32_t)reqheight);
                  fprintf(fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                           reqwidth, reqheight);
                  }
                }
             break;
    case  90:
    case 270: if ((splitheight > 0) || (splitwidth > 0))
               {
               if (pgwidth != 0 || pgheight != 0)
                  {
                 xscale = reqwidth / (splitwidth ? splitwidth : pswidth);
                 yscale = reqheight / (splitheight ? splitheight : psheight);
                  scale = (xscale < yscale) ? xscale : yscale;
                  }
                new_width = splitwidth ? splitwidth : scale * psheight;
                new_height = splitheight ? splitheight : scale * pswidth;

                if (strlen(pageOrientation))
                  fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                else
                  fprintf (fd, "%%%%PageOrientation: %s\n", (new_width > new_height) ? "Landscape" : "Portrait");
                fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)new_width, (int32_t)new_height);
                fprintf (fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                       new_width, new_height);
                }
              else
                {
                if ((pgwidth == 0) && (pgheight == 0)) /* Image not scaled */
                  {
                  if (strlen(pageOrientation))
                    fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                  else
                    fprintf (fd, "%%%%PageOrientation: %s\n", (psheight > pswidth) ? "Landscape" : "Portrait");
                 fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)psheight, (int32_t)pswidth);
                  fprintf(fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                         psheight, pswidth);
                  }
               else /* Image scaled */
                  {
                  if (strlen(pageOrientation))
                    fprintf (fd, "%%%%PageOrientation: %s\n", pageOrientation);
                  else
                    fprintf (fd, "%%%%PageOrientation: %s\n", (reqwidth > reqheight) ? "Landscape" : "Portrait");
                 fprintf (fd, "%%%%PageBoundingBox: 0 0 %"PRId32" %"PRId32"\n", (int32_t)reqwidth, (int32_t)reqheight);
                  fprintf(fd, "1 dict begin /PageSize [ %f %f ] def currentdict end setpagedevice\n",
                          reqwidth, reqheight);
                  }
               }
             break;
    default:  TIFFError ("psPageSize", "Invalid rotation %d", rotation);
      return (1);
    }
  fputs("<<\n  /Policies <<\n    /PageSize 3\n  >>\n>> setpagedevice\n", fd);

  return (0);
  } /* end psPageSize */

/* Mask an image as a series of pages, each only showing a section defined
 * by the maxPageHeight or maxPageWidth options.
 */
int psMaskImage(FILE *fd, TIFF *tif, int rotation, int center,
                int *npages, double pixwidth, double pixheight,
               double left_margin, double bottom_margin,
                double pgwidth, double pgheight,
               double pswidth, double psheight, double scale)
  {
  int i, j;
  int ximages = 1, yimages = 1;
  int pages = *npages;
  double view_width = 0;
  double view_height = 0;

  if (get_viewport (pgwidth, pgheight, pswidth, psheight, &view_width, &view_height, rotation))
    {
    TIFFError ("get_viewport", "Unable to set image viewport");
    return (-1);
    }

  if (get_subimage_count(pgwidth, pgheight, pswidth, psheight,
                        &ximages, &yimages, rotation, scale) < 1)
    {
    TIFFError("get_subimage_count", "Invalid image count: %d columns, %d rows", ximages, yimages);
    return (-1);
    }

  for (i = 0; i < yimages; i++)
    {
    for (j = 0; j < ximages; j++)
       {
       pages++;
       *npages = pages;
       fprintf(fd, "%%%%Page: %d %d\n", pages, pages);

       /* Write out the PageSize info for non EPS files */
       if (!generateEPSF && ( level2 || level3 ))
         {
         if (psPageSize(fd, rotation, pgwidth, pgheight,
                        view_width, view_height, pswidth, psheight))
           return (-1);
        }
       fprintf(fd, "gsave\n");
       fprintf(fd, "100 dict begin\n");
       if (exportMaskedImage(fd, view_width, view_height, pswidth, psheight,
                            i, j, left_margin, bottom_margin,
                            scale, center, rotation))
        {
        TIFFError("exportMaskedImage", "Invalid image parameters.");
        return (-1);
        }
       PSpage(fd, tif, pixwidth, pixheight);
       fprintf(fd, "end\n");
       fprintf(fd, "grestore\n");
       fprintf(fd, "showpage\n");
       }
    }

  return (pages);
  }

/* Compute scale factor and write out file header */
int psStart(FILE *fd, int npages, int auto_rotate, int *rotation, double *scale,
            double ox, double oy, double pgwidth, double pgheight,
           double reqwidth, double reqheight, double pswidth, double psheight,
           double left_offset, double bottom_offset)
  {
  double maxsource = 0.0;    /* Used for auto rotations */
  double maxtarget = 0.0;
  double xscale = 1.0, yscale = 1.0;
  double splitheight;
  double splitwidth;
  double view_width = 0.0, view_height = 0.0;
  double page_width = 0.0, page_height = 0.0;

  /* Splitheight and splitwidth are in inches */
  splitheight = maxPageHeight * PS_UNIT_SIZE;
  splitwidth  = maxPageWidth * PS_UNIT_SIZE;

  page_width = pgwidth * PS_UNIT_SIZE;
  page_height = pgheight * PS_UNIT_SIZE;

  /* If user has specified a page width and height and requested the
   * image to be auto-rotated to fit on that media, we match the
   * longest dimension of the image to the longest dimension of the
   * target media but we have to ignore auto rotate if user specified
   * maxPageHeight since this makes life way too complicated. */
  if (auto_rotate)
    {
    if ((splitheight != 0) || (splitwidth != 0))
      {
      TIFFError ("psStart", "Auto-rotate is incompatible with page splitting ");
      return (1);
      }

    /* Find longest edges in image and output media */
    maxsource = (pswidth >= psheight) ? pswidth : psheight;
    maxtarget = (reqwidth >= reqheight) ? reqwidth : reqheight;

    if (((maxsource == pswidth) && (maxtarget != reqwidth)) ||
        ((maxsource == psheight) && (maxtarget != reqheight)))
      {  /* optimal orientation does not match input orientation */
      *rotation = 90;
      xscale = (reqwidth - left_offset)/psheight;
      yscale = (reqheight - bottom_offset)/pswidth;
      }
    else /* optimal orientation matches input orientation */
      {
      xscale = (reqwidth - left_offset)/pswidth;
      yscale = (reqheight - bottom_offset)/psheight;
      }
    *scale = (xscale < yscale) ? xscale : yscale;

    /* Do not scale image beyond original size */
    if (*scale > 1.0)
      *scale = 1.0;

    /* Set the size of the displayed image to requested page size
     * and optimal orientation.
     */
    if (!npages)
      PSHead(fd, reqwidth, reqheight, ox, oy);

    return (0);
    }

  /* N.B. If pgwidth or pgheight are set from maxPageHeight/Width,
   * we have a problem with the tests below under splitheight.
   */

  switch (*rotation)  /* Auto rotate has NOT been specified */
    {
    case   0:
    case 180: if ((splitheight != 0)  || (splitwidth != 0))
                {  /* Viewport clipped to maxPageHeight or maxPageWidth */
                if ((page_width != 0) || (page_height != 0)) /* Image scaled */
                  {
                 xscale = (reqwidth  - left_offset) / (page_width ? page_width : pswidth);
                 yscale = (reqheight - bottom_offset) / (page_height ? page_height : psheight);
                  *scale = (xscale < yscale) ? xscale : yscale;
                  /*
                  if (*scale > 1.0)
                    *scale = 1.0;
                   */
                 }
                else       /* Image clipped but not scaled */
                 *scale = 1.0;

                view_width = splitwidth ? splitwidth : *scale * pswidth;
                view_height = splitheight ? splitheight: *scale * psheight;
               }
              else   /* Viewport not clipped to maxPageHeight or maxPageWidth */
                {
                if ((page_width != 0) || (page_height != 0))
                  {   /* Image scaled  */
                  xscale = (reqwidth - left_offset) / pswidth;
                  yscale = (reqheight - bottom_offset) / psheight;

                  view_width = reqwidth;
                  view_height = reqheight;
                 }
                else
                  {  /* Image not scaled  */
                  xscale = (pswidth - left_offset)/pswidth;
                  yscale = (psheight - bottom_offset)/psheight;

                  view_width = pswidth;
                  view_height = psheight;
                 }
               }
             break;
    case  90:
    case 270: if ((splitheight != 0) || (splitwidth != 0))
                {  /* Viewport clipped to maxPageHeight or maxPageWidth */
                if ((page_width != 0) || (page_height != 0)) /* Image scaled */
                  {
                 xscale = (reqwidth - left_offset)/ psheight;
                 yscale = (reqheight - bottom_offset)/ pswidth;
                  *scale = (xscale < yscale) ? xscale : yscale;
                  /*
                  if (*scale > 1.0)
                    *scale = 1.0;
                 */
                 }
                else  /* Image clipped but not scaled */
                 *scale = 1.0;
                view_width = splitwidth ? splitwidth : *scale * psheight;
                view_height = splitheight ? splitheight : *scale * pswidth;
               }
              else /* Viewport not clipped to maxPageHeight or maxPageWidth */
                {
                if ((page_width != 0) || (page_height != 0)) /* Image scaled */
                  {
                  xscale = (reqwidth - left_offset) / psheight;
                  yscale = (reqheight - bottom_offset) / pswidth;

                 view_width = reqwidth;
                 view_height = reqheight;
                 }
                else
                  {
                  xscale = (pswidth  - left_offset)/ psheight;
                 yscale = (psheight  - bottom_offset)/ pswidth;

                 view_width = psheight;
                 view_height = pswidth;
                  }
                }
              break;
    default:  TIFFError ("psPageSize", "Invalid rotation %d", *rotation);
              return (1);
    }

  if (!npages)
    PSHead(fd, (page_width ? page_width : view_width), (page_height ? page_height : view_height), ox, oy);

  *scale = (xscale < yscale) ? xscale : yscale;
  if (*scale > 1.0)
    *scale = 1.0;

  return (0);
  }

int get_viewport (double pgwidth, double pgheight, double pswidth, double psheight,
                  double *view_width, double *view_height, int rotation)
  {
  /* Only one of maxPageHeight or maxPageWidth can be specified */
  if (maxPageHeight != 0)   /* Clip the viewport to maxPageHeight on each page */
    {
    if (pgheight != 0 && pgheight < maxPageHeight)
      *view_height = pgheight * PS_UNIT_SIZE;
    else
      *view_height = maxPageHeight * PS_UNIT_SIZE;
    /*
     * if (res_unit == RESUNIT_CENTIMETER)
     * *view_height /= 2.54F;
     */
    }
  else
    {
    if (pgheight != 0) /* User has set PageHeight with -h flag */
      {
      *view_height = pgheight * PS_UNIT_SIZE; /* Postscript size for Page Height in inches */
      /* if (res_unit == RESUNIT_CENTIMETER)
       *  *view_height /= 2.54F;
       */
      }
    else /* If no width or height are specified, use the original size from image */
      switch (rotation)
        {
        default:
        case   0:
        case 180: *view_height = psheight;
                 break;
        case  90:
        case 270: *view_height = pswidth;
                 break;
       }
    }

  if (maxPageWidth != 0)   /* Clip the viewport to maxPageWidth on each page */
    {
    if (pgwidth != 0 && pgwidth < maxPageWidth)
      *view_width = pgwidth * PS_UNIT_SIZE;
    else
      *view_width = maxPageWidth * PS_UNIT_SIZE;
    /* if (res_unit == RESUNIT_CENTIMETER)
     *  *view_width /= 2.54F;
     */
    }
  else
    {
    if (pgwidth != 0)  /* User has set PageWidth with -w flag */
      {
      *view_width = pgwidth * PS_UNIT_SIZE; /* Postscript size for Page Width in inches */
      /* if (res_unit == RESUNIT_CENTIMETER)
       * *view_width /= 2.54F;
       */
      }
    else  /* If no width or height are specified, use the original size from image */
      switch (rotation)
        {
        default:
        case   0:
        case 180: *view_width = pswidth;
                 break;
        case  90:
        case 270: *view_width = psheight; /* (*view_height / psheight) * psheight; */
                 break;
       }
    }

  return (0);
  }

/* pgwidth and pgheight specify page width and height in inches from -h and -w flags
 * lm and bm are the LeftMargin and BottomMargin in inches
 * center causes the image to be centered on the page if the paper size is
 * larger than the image size
 * returns the sequence number of the page processed or -1 on error
 */

int TIFF2PS(FILE* fd, TIFF* tif, double pgwidth, double pgheight, double lm, double bm, int center)
  {
  uint32_t pixwidth = 0, pixheight = 0;  /* Image width and height in pixels */
  double ox = 0.0, oy = 0.0;  /* Offset from current Postscript origin */
  double pswidth, psheight;   /* Original raw image width and height in points */
  double view_width, view_height; /* Viewport width and height in points */
  double scale = 1.0;
  double left_offset = lm * PS_UNIT_SIZE;
  double bottom_offset = bm * PS_UNIT_SIZE;
  uint32_t subfiletype;
  uint16_t* sampleinfo;
  static int npages = 0;

  if (!TIFFGetField(tif, TIFFTAG_XPOSITION, &ox))
     ox = 0;
  if (!TIFFGetField(tif, TIFFTAG_YPOSITION, &oy))
     oy = 0;

  /* Consolidated all the tag information into one code segment, Richard Nolde */
  do {
     tf_numberstrips = TIFFNumberOfStrips(tif);
     TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &tf_rowsperstrip);
     TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample);
     TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
     TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarconfiguration);
     TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
     TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES, &extrasamples, &sampleinfo);
     alpha = (extrasamples == 1 && sampleinfo[0] == EXTRASAMPLE_ASSOCALPHA);
     if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric))
       {
       switch (samplesperpixel - extrasamples)
             {
            case 1: if (isCCITTCompression(tif))
                      photometric = PHOTOMETRIC_MINISWHITE;
                    else
                       photometric = PHOTOMETRIC_MINISBLACK;
                    break;
            case 3: photometric = PHOTOMETRIC_RGB;
                    break;
            case 4: photometric = PHOTOMETRIC_SEPARATED;
                    break;
            }
       }

     /* Read image tags for width and height in pixels pixwidth, pixheight,
      * and convert to points pswidth, psheight
      */
     setupPageState(tif, &pixwidth, &pixheight, &pswidth, &psheight);
     view_width = pswidth;
     view_height = psheight;

     if (get_viewport (pgwidth, pgheight, pswidth, psheight, &view_width, &view_height, rotation))
       {
       TIFFError("get_viewport", "Unable to set image viewport");
       return (1);
       }

     /* Write the Postscript file header with Bounding Box and Page Size definitions */
     if (psStart(fd, npages, auto_rotate, &rotation, &scale, ox, oy,
                pgwidth, pgheight, view_width, view_height, pswidth, psheight,
                 left_offset, bottom_offset))
       return (-1);

     if (checkImage(tif))  /* Aborts if unsupported image parameters */
       {
       tf_bytesperrow = TIFFScanlineSize(tif);

       /* Set viewport clipping and scaling options */
       if ((maxPageHeight) || (maxPageWidth)  || (pgwidth != 0) || (pgheight != 0))
         {
        if ((maxPageHeight) || (maxPageWidth)) /* used -H or -W  option */
           {
          if (psMaskImage(fd, tif, rotation, center, &npages, pixwidth, pixheight,
                          left_offset, bottom_offset, pgwidth, pgheight,
                           pswidth, psheight, scale) < 0)
            return (-1);
          }
         else  /* N.B. Setting maxPageHeight no longer sets pgheight */
           {
           if (pgwidth != 0 || pgheight != 0)
             {
             /* User did not specify a maximum page height or width using -H or -W flag
              * but did use -h or -w flag to scale to a specific size page.
              */
             npages++;
             fprintf(fd, "%%%%Page: %d %d\n", npages, npages);

             if (!generateEPSF && ( level2 || level3 ))
               {
              /* Write out the PageSize info for non EPS files */
              if (psPageSize(fd, rotation, pgwidth, pgheight,
                              view_width, view_height, pswidth, psheight))
                return (-1);
               }
             fprintf(fd, "gsave\n");
             fprintf(fd, "100 dict begin\n");
             if (psScaleImage(fd, scale, rotation, center, view_width, view_height,
                              pswidth, psheight, left_offset, bottom_offset))
              return (-1);

             PSpage(fd, tif, pixwidth, pixheight);
             fprintf(fd, "end\n");
             fprintf(fd, "grestore\n");
             fprintf(fd, "showpage\n");
            }
          }
        }
       else  /* Simple rotation: user did not use -H, -W, -h or -w */
         {
         npages++;
         fprintf(fd, "%%%%Page: %d %d\n", npages, npages);

         if (!generateEPSF && ( level2 || level3 ))
           {
          /* Write out the PageSize info for non EPS files */
          if (psPageSize(fd, rotation, pgwidth, pgheight,
                          view_width, view_height, pswidth, psheight))
           return (-1);
         }
         fprintf(fd, "gsave\n");
         fprintf(fd, "100 dict begin\n");
        if (psRotateImage(fd, rotation, pswidth, psheight, left_offset, bottom_offset))
           return (-1);

         PSpage(fd, tif, pixwidth, pixheight);
         fprintf(fd, "end\n");
         fprintf(fd, "grestore\n");
         fprintf(fd, "showpage\n");
         }
       }
  if (generateEPSF)
    break;
  if (auto_rotate)
    rotation = 0.0;
  TIFFGetFieldDefaulted(tif, TIFFTAG_SUBFILETYPE, &subfiletype);
  } while (((subfiletype & FILETYPE_PAGE) || printAll) && TIFFReadDirectory(tif));

return(npages);
}

static const char DuplexPreamble[] = "\
%%BeginFeature: *Duplex True\n\
systemdict begin\n\
  /languagelevel where { pop languagelevel } { 1 } ifelse\n\
  2 ge { 1 dict dup /Duplex true put setpagedevice }\n\
  { statusdict /setduplex known { statusdict begin setduplex true end } if\n\
  } ifelse\n\
end\n\
%%EndFeature\n\
";

static const char TumblePreamble[] = "\
%%BeginFeature: *Tumble True\n\
systemdict begin\n\
  /languagelevel where { pop languagelevel } { 1 } ifelse\n\
  2 ge { 1 dict dup /Tumble true put setpagedevice }\n\
  { statusdict /settumble known { statusdict begin true settumble end } if\n\
  } ifelse\n\
end\n\
%%EndFeature\n\
";

static const char AvoidDeadZonePreamble[] = "\
gsave newpath clippath pathbbox grestore\n\
  4 2 roll 2 copy translate\n\
  exch 3 1 roll sub 3 1 roll sub exch\n\
  currentpagedevice /PageSize get aload pop\n\
  exch 3 1 roll div 3 1 roll div abs exch abs\n\
  2 copy gt { exch } if pop\n\
  dup 1 lt { dup scale } { pop } ifelse\n\
";

void
PSHead(FILE *fd, double pagewidth, double pageheight, double xoff, double yoff)
{
	time_t t;

	t = time(0);
	fprintf(fd, "%%!PS-Adobe-3.0%s\n", generateEPSF ? " EPSF-3.0" : "");
	fprintf(fd, "%%%%Creator: %s\n", creator ? creator : "tiff2ps");
        fprintf(fd, "%%%%Title: %s\n", title ? title : filename);
	fprintf(fd, "%%%%CreationDate: %s", ctime(&t));
	fprintf(fd, "%%%%DocumentData: Clean7Bit\n");
	/* NB: should use PageBoundingBox for each page instead of BoundingBox *
         * PageBoundingBox DSC added in PSPageSize function, R Nolde 09-01-2010
         */
	fprintf(fd, "%%%%Origin: %"PRId32" %"PRId32"\n", (int32_t) xoff, (int32_t) yoff);
        fprintf(fd, "%%%%BoundingBox: 0 0 %"PRId32" %"PRId32"\n",
	       (int32_t) ceil(pagewidth), (int32_t) ceil(pageheight));

	fprintf(fd, "%%%%LanguageLevel: %d\n", (level3 ? 3 : (level2 ? 2 : 1)));
        if (generateEPSF == TRUE)
	  fprintf(fd, "%%%%Pages: 1 1\n");
        else
	  fprintf(fd, "%%%%Pages: (atend)\n");
	fprintf(fd, "%%%%EndComments\n");
        if (generateEPSF == FALSE)
          {
  	  fprintf(fd, "%%%%BeginSetup\n");
	  if (PSduplex)
		fprintf(fd, "%s", DuplexPreamble);
	  if (PStumble)
		fprintf(fd, "%s", TumblePreamble);
	  if (PSavoiddeadzone && (level2 || level3))
		fprintf(fd, "%s", AvoidDeadZonePreamble);
	  fprintf(fd, "%%%%EndSetup\n");
	  }
}

void
PSTail(FILE *fd, int npages)
{
	fprintf(fd, "%%%%Trailer\n");
        if (generateEPSF == FALSE)
	  fprintf(fd, "%%%%Pages: %d\n", npages);
	fprintf(fd, "%%%%EOF\n");
}

static int
checkcmap(TIFF* tif, int n, uint16_t* r, uint16_t* g, uint16_t* b)
{
	(void) tif;
	while (n-- > 0)
		if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
			return (16);
	TIFFWarning(filename, "Assuming 8-bit colormap");
	return (8);
}

static void
PS_Lvl2colorspace(FILE* fd, TIFF* tif)
{
	uint16_t *rmap, *gmap, *bmap;
	int i, num_colors;
	const char * colorspace_p;

	switch ( photometric )
	{
	case PHOTOMETRIC_SEPARATED:
		colorspace_p = "CMYK";
		break;

	case PHOTOMETRIC_RGB:
		colorspace_p = "RGB";
		break;

	default:
		colorspace_p = "Gray";
	}

	/*
	 * Set up PostScript Level 2 colorspace according to
	 * section 4.8 in the PostScript reference manual.
	 */
	fputs("% PostScript Level 2 only.\n", fd);
	if (photometric != PHOTOMETRIC_PALETTE) {
		if (photometric == PHOTOMETRIC_YCBCR) {
		    /* MORE CODE HERE */
		}
		fprintf(fd, "/Device%s setcolorspace\n", colorspace_p );
		return;
	}

	/*
	 * Set up an indexed/palette colorspace
	 */
	num_colors = (1 << bitspersample);
	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		TIFFError(filename,
			"Palette image w/o \"Colormap\" tag");
		return;
	}
	if (checkcmap(tif, num_colors, rmap, gmap, bmap) == 16) {
		/*
		 * Convert colormap to 8-bits values.
		 */
#define	CVT(x)		(((x) * 255) / ((1L<<16)-1))
		for (i = 0; i < num_colors; i++) {
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
#undef CVT
	}
	fprintf(fd, "[ /Indexed /DeviceRGB %d", num_colors - 1);
	if (ascii85) {
		Ascii85Init();
		fputs("\n<~", fd);
		ascii85breaklen -= 2;
	} else
		fputs(" <", fd);
	for (i = 0; i < num_colors; i++) {
		if (ascii85) {
			Ascii85Put((unsigned char)rmap[i], fd);
			Ascii85Put((unsigned char)gmap[i], fd);
			Ascii85Put((unsigned char)bmap[i], fd);
		} else {
			fputs((i % 8) ? " " : "\n  ", fd);
			fprintf(fd, "%02"PRIx16"%02"PRIx16"%02"PRIx16"",
			    rmap[i], gmap[i], bmap[i]);
		}
	}
	if (ascii85)
		Ascii85Flush(fd);
	else
		fputs(">\n", fd);
	fputs("] setcolorspace\n", fd);
}

static int
PS_Lvl2ImageDict(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	int use_rawdata;
	uint32_t tile_width, tile_height;
	uint16_t predictor, minsamplevalue, maxsamplevalue;
	uint32_t repeat_count;
	char im_h[64], im_x[64], im_y[64];
	const char * imageOp = "image";

	if ( useImagemask && (bitspersample == 1) )
		imageOp = "imagemask";

	(void)strcpy(im_x, "0");
	(void)snprintf(im_y, sizeof(im_y), "%"PRIu32, h);
	(void)snprintf(im_h, sizeof(im_h), "%"PRIu32, h);
	tile_width = w;
	tile_height = h;
	if (TIFFIsTiled(tif)) {
		repeat_count = TIFFNumberOfTiles(tif);
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
		if (tile_width > w || tile_height > h ||
		    (w % tile_width) != 0 || (h % tile_height != 0)) {
			/*
			 * The tiles does not fit image width and height.
			 * Set up a clip rectangle for the image unit square.
			 */
			fputs("0 0 1 1 rectclip\n", fd);
		}
		if (tile_width < w) {
			fputs("/im_x 0 def\n", fd);
			(void)strcpy(im_x, "im_x neg");
		}
		if (tile_height < h) {
			fputs("/im_y 0 def\n", fd);
			(void)snprintf(im_y, sizeof(im_y), "%"PRIu32" im_y sub", h);
		}
	} else {
		repeat_count = tf_numberstrips;
		tile_height = tf_rowsperstrip;
		if (tile_height > h)
			tile_height = h;
		if (repeat_count > 1) {
			fputs("/im_y 0 def\n", fd);
			fprintf(fd, "/im_h %"PRIu32" def\n", tile_height);
			(void)strcpy(im_h, "im_h");
			(void)snprintf(im_y, sizeof(im_y), "%"PRIu32" im_y sub", h);
		}
	}

	/*
	 * Output start of exec block
	 */
	fputs("{ % exec\n", fd);

	if (repeat_count > 1)
		fprintf(fd, "%"PRIu32" { %% repeat\n", repeat_count);

	/*
	 * Output filter options and image dictionary.
	 */
	if (ascii85)
		fputs(" /im_stream currentfile /ASCII85Decode filter def\n",
		    fd);
	fputs(" <<\n", fd);
	fputs("  /ImageType 1\n", fd);
	fprintf(fd, "  /Width %"PRIu32"\n", tile_width);
	/*
	 * Workaround for some software that may crash when last strip
	 * of image contains fewer number of scanlines than specified
	 * by the `/Height' variable. So for stripped images with multiple
	 * strips we will set `/Height' as `im_h', because one is 
	 * recalculated for each strip - including the (smaller) final strip.
	 * For tiled images and images with only one strip `/Height' will
	 * contain number of scanlines in tile (or image height in case of
	 * one-stripped image).
	 */
	if (TIFFIsTiled(tif) || tf_numberstrips == 1)
		fprintf(fd, "  /Height %"PRIu32"\n", tile_height);
	else
		fprintf(fd, "  /Height im_h\n");
	
	if (planarconfiguration == PLANARCONFIG_SEPARATE && samplesperpixel > 1)
		fputs("  /MultipleDataSources true\n", fd);
	fprintf(fd, "  /ImageMatrix [ %"PRIu32" 0 0 %"PRId32" %s %s ]\n",
	    w, - (int32_t)h, im_x, im_y);
	fprintf(fd, "  /BitsPerComponent %"PRIu16"\n", bitspersample);
	fprintf(fd, "  /Interpolate %s\n", interpolate ? "true" : "false");

	switch (samplesperpixel - extrasamples) {
	case 1:
		switch (photometric) {
		case PHOTOMETRIC_MINISBLACK:
			fputs("  /Decode [0 1]\n", fd);
			break;
		case PHOTOMETRIC_MINISWHITE:
			switch (compression) {
			case COMPRESSION_CCITTRLE:
			case COMPRESSION_CCITTRLEW:
			case COMPRESSION_CCITTFAX3:
			case COMPRESSION_CCITTFAX4:
				/*
				 * Manage inverting with /Blackis1 flag
				 * since there might be uncompressed parts
				 */
				fputs("  /Decode [0 1]\n", fd);
				break;
			default:
				/*
				 * ERROR...
				 */
				fputs("  /Decode [1 0]\n", fd);
				break;
			}
			break;
		case PHOTOMETRIC_PALETTE:
			TIFFGetFieldDefaulted(tif, TIFFTAG_MINSAMPLEVALUE,
			    &minsamplevalue);
			TIFFGetFieldDefaulted(tif, TIFFTAG_MAXSAMPLEVALUE,
			    &maxsamplevalue);
			fprintf(fd, "  /Decode [%"PRIu16" %"PRIu16"]\n",
				    minsamplevalue, maxsamplevalue);
			break;
		default:
			/*
			 * ERROR ?
			 */
			fputs("  /Decode [0 1]\n", fd);
			break;
		}
		break;
	case 3:
		switch (photometric) {
		case PHOTOMETRIC_RGB:
			fputs("  /Decode [0 1 0 1 0 1]\n", fd);
			break;
		case PHOTOMETRIC_MINISWHITE:
		case PHOTOMETRIC_MINISBLACK:
		default:
			/*
			 * ERROR??
			 */
			fputs("  /Decode [0 1 0 1 0 1]\n", fd);
			break;
		}
		break;
	case 4:
		/*
		 * ERROR??
		 */
		fputs("  /Decode [0 1 0 1 0 1 0 1]\n", fd);
		break;
	}
	fputs("  /DataSource", fd);
	if (planarconfiguration == PLANARCONFIG_SEPARATE &&
	    samplesperpixel > 1)
		fputs(" [", fd);
	if (ascii85)
		fputs(" im_stream", fd);
	else
		fputs(" currentfile /ASCIIHexDecode filter", fd);

	use_rawdata = TRUE;
	switch (compression) {
	case COMPRESSION_NONE:		/* 1: uncompressed */
		break;
	case COMPRESSION_CCITTRLE:	/* 2: CCITT modified Huffman RLE */
	case COMPRESSION_CCITTRLEW:	/* 32771: #1 w/ word alignment */
	case COMPRESSION_CCITTFAX3:	/* 3: CCITT Group 3 fax encoding */
	case COMPRESSION_CCITTFAX4:	/* 4: CCITT Group 4 fax encoding */
		fputs("\n\t<<\n", fd);
		if (compression == COMPRESSION_CCITTFAX3) {
			uint32_t g3_options;

			fputs("\t /EndOfLine true\n", fd);
			fputs("\t /EndOfBlock false\n", fd);
			if (!TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS,
					    &g3_options))
				g3_options = 0;
			if (g3_options & GROUP3OPT_2DENCODING)
				fprintf(fd, "\t /K %s\n", im_h);
			if (g3_options & GROUP3OPT_UNCOMPRESSED)
				fputs("\t /Uncompressed true\n", fd);
			if (g3_options & GROUP3OPT_FILLBITS)
				fputs("\t /EncodedByteAlign true\n", fd);
		}
		if (compression == COMPRESSION_CCITTFAX4) {
			uint32_t g4_options;

			fputs("\t /K -1\n", fd);
			TIFFGetFieldDefaulted(tif, TIFFTAG_GROUP4OPTIONS,
					       &g4_options);
			if (g4_options & GROUP4OPT_UNCOMPRESSED)
				fputs("\t /Uncompressed true\n", fd);
		}
		if (!(tile_width == w && w == 1728U))
			fprintf(fd, "\t /Columns %"PRIu32"\n",
			    tile_width);
		fprintf(fd, "\t /Rows %s\n", im_h);
		if (compression == COMPRESSION_CCITTRLE ||
		    compression == COMPRESSION_CCITTRLEW) {
			fputs("\t /EncodedByteAlign true\n", fd);
			fputs("\t /EndOfBlock false\n", fd);
		}
		if (photometric == PHOTOMETRIC_MINISBLACK)
			fputs("\t /BlackIs1 true\n", fd);
		fprintf(fd, "\t>> /CCITTFaxDecode filter");
		break;
	case COMPRESSION_LZW:	/* 5: Lempel-Ziv & Welch */
		TIFFGetFieldDefaulted(tif, TIFFTAG_PREDICTOR, &predictor);
		if (predictor == 2) {
			fputs("\n\t<<\n", fd);
			fprintf(fd, "\t /Predictor %"PRIu16"\n", predictor);
			fprintf(fd, "\t /Columns %"PRIu32"\n",
			    tile_width);
			fprintf(fd, "\t /Colors %"PRIu16"\n", samplesperpixel);
			fprintf(fd, "\t /BitsPerComponent %"PRIu16"\n",
			    bitspersample);
			fputs("\t>>", fd);
		}
		fputs(" /LZWDecode filter", fd);
		break;
	case COMPRESSION_DEFLATE:	/* 5: ZIP */
	case COMPRESSION_ADOBE_DEFLATE:
		if ( level3 ) {
			 TIFFGetFieldDefaulted(tif, TIFFTAG_PREDICTOR, &predictor);
			 if (predictor > 1) {
				fprintf(fd, "\t %% PostScript Level 3 only.");
				fputs("\n\t<<\n", fd);
				fprintf(fd, "\t /Predictor %"PRIu16"\n", predictor);
				fprintf(fd, "\t /Columns %"PRIu32"\n",
					tile_width);
				fprintf(fd, "\t /Colors %"PRIu16"\n", samplesperpixel);
					fprintf(fd, "\t /BitsPerComponent %"PRIu16"\n",
					bitspersample);
				fputs("\t>>", fd);
			 }
			 fputs(" /FlateDecode filter", fd);
		} else {
			use_rawdata = FALSE ;
		}
		break;
	case COMPRESSION_PACKBITS:	/* 32773: Macintosh RLE */
		fputs(" /RunLengthDecode filter", fd);
		use_rawdata = TRUE;
	    break;
	case COMPRESSION_OJPEG:		/* 6: !6.0 JPEG */
	case COMPRESSION_JPEG:		/* 7: %JPEG DCT compression */
#ifdef notdef
		/*
		 * Code not tested yet
		 */
		fputs(" /DCTDecode filter", fd);
		use_rawdata = TRUE;
#else
		use_rawdata = FALSE;
#endif
		break;
	case COMPRESSION_NEXT:		/* 32766: NeXT 2-bit RLE */
	case COMPRESSION_THUNDERSCAN:	/* 32809: ThunderScan RLE */
	case COMPRESSION_PIXARFILM:	/* 32908: Pixar companded 10bit LZW */
	case COMPRESSION_JBIG:		/* 34661: ISO JBIG */
		use_rawdata = FALSE;
		break;
	case COMPRESSION_SGILOG:	/* 34676: SGI LogL or LogLuv */
	case COMPRESSION_SGILOG24:	/* 34677: SGI 24-bit LogLuv */
		use_rawdata = FALSE;
		break;
	default:
		/*
		 * ERROR...
		 */
		use_rawdata = FALSE;
		break;
	}
	if (planarconfiguration == PLANARCONFIG_SEPARATE &&
	    samplesperpixel > 1) {
		uint16_t i;

		/*
		 * NOTE: This code does not work yet...
		 */
		for (i = 1; i < samplesperpixel; i++)
			fputs(" dup", fd);
		fputs(" ]", fd);
	}

	fprintf( fd, "\n >> %s\n", imageOp );
	if (ascii85)
		fputs(" im_stream status { im_stream flushfile } if\n", fd);
	if (repeat_count > 1) {
		if (tile_width < w) {
			fprintf(fd, " /im_x im_x %"PRIu32" add def\n",
			    tile_width);
			if (tile_height < h) {
				fprintf(fd, " im_x %"PRIu32" ge {\n",
				    w);
				fputs("  /im_x 0 def\n", fd);
				fprintf(fd, " /im_y im_y %"PRIu32" add def\n",
				    tile_height);
				fputs(" } if\n", fd);
			}
		}
		if (tile_height < h) {
			if (tile_width >= w) {
				fprintf(fd, " /im_y im_y %"PRIu32" add def\n",
				    tile_height);
				if (!TIFFIsTiled(tif)) {
					fprintf(fd, " /im_h %"PRIu32" im_y sub",
					    h);
					fprintf(fd, " dup %"PRIu32" gt { pop",
					    tile_height);
					fprintf(fd, " %"PRIu32" } if def\n",
					    tile_height);
				}
			}
		}
		fputs("} repeat\n", fd);
	}
	/*
	 * End of exec function
	 */
	fputs("}\n", fd);

	return(use_rawdata);
}

/* Flip the byte order of buffers with 16 bit samples */
static void
PS_FlipBytes(unsigned char* buf, tsize_t count)
{
	int i;
	unsigned char temp;

	if (count <= 0 || bitspersample <= 8) {
		return;
	}

	count--;

	for (i = 0; i < count; i += 2) {
		temp = buf[i];
		buf[i] = buf[i + 1];
		buf[i + 1] = temp;
	}
}

#define MAXLINE		36

int
PS_Lvl2page(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	uint16_t fillorder;
	int use_rawdata, tiled_image, breaklen = MAXLINE;
	uint32_t chunk_no, num_chunks;
        uint64_t *bc;
	unsigned char *buf_data, *cp;
	tsize_t chunk_size, byte_count;

#if defined( EXP_ASCII85ENCODER )
	tsize_t			ascii85_l;	/* Length, in bytes, of ascii85_p[] data */
	uint8_t		*	ascii85_p = 0;	/* Holds ASCII85 encoded data */
#endif

	PS_Lvl2colorspace(fd, tif);
	use_rawdata = PS_Lvl2ImageDict(fd, tif, w, h);

/* See http://bugzilla.remotesensing.org/show_bug.cgi?id=80 */
#ifdef ENABLE_BROKEN_BEGINENDDATA
	fputs("%%BeginData:\n", fd);
#endif
	fputs("exec\n", fd);

	tiled_image = TIFFIsTiled(tif);
	if (tiled_image) {
		num_chunks = TIFFNumberOfTiles(tif);
		TIFFGetField(tif, TIFFTAG_TILEBYTECOUNTS, &bc);
	} else {
		num_chunks = TIFFNumberOfStrips(tif);
		TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &bc);
	}

	if (use_rawdata) {
		chunk_size = (tsize_t) bc[0];
		for (chunk_no = 1; chunk_no < num_chunks; chunk_no++)
			if ((tsize_t) bc[chunk_no] > chunk_size)
				chunk_size = (tsize_t) bc[chunk_no];
	} else {
		if (tiled_image)
			chunk_size = TIFFTileSize(tif);
		else
			chunk_size = TIFFStripSize(tif);
	}
	buf_data = (unsigned char *)limitMalloc(chunk_size);
	if (!buf_data) {
		TIFFError(filename, "Can't alloc %"TIFF_SSIZE_FORMAT" bytes for %s.",
			chunk_size, tiled_image ? "tiles" : "strips");
		return(FALSE);
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85 ) {
	    /*
	     * Allocate a buffer to hold the ASCII85 encoded data.  Note
	     * that it is allocated with sufficient room to hold the
	     * encoded data (5*chunk_size/4) plus the EOD marker (+8)
	     * and formatting line breaks.  The line breaks are more
	     * than taken care of by using 6*chunk_size/4 rather than
	     * 5*chunk_size/4.
	     */

	    ascii85_p = limitMalloc( (chunk_size+(chunk_size/2)) + 8 );

	    if ( !ascii85_p ) {
		_TIFFfree( buf_data );

		TIFFError( filename, "Cannot allocate ASCII85 encoding buffer." );
		return ( FALSE );
	    }
	}
#endif

	TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);
	for (chunk_no = 0; chunk_no < num_chunks; chunk_no++) {
		if (ascii85)
			Ascii85Init();
		else
			breaklen = MAXLINE;
		if (use_rawdata) {
			if (tiled_image)
				byte_count = TIFFReadRawTile(tif, chunk_no,
						  buf_data, chunk_size);
			else
				byte_count = TIFFReadRawStrip(tif, chunk_no,
						  buf_data, chunk_size);
			if (fillorder == FILLORDER_LSB2MSB)
			    TIFFReverseBits(buf_data, byte_count);
		} else {
			if (tiled_image)
				byte_count = TIFFReadEncodedTile(tif,
						chunk_no, buf_data,
						chunk_size);
			else
				byte_count = TIFFReadEncodedStrip(tif,
						chunk_no, buf_data,
						chunk_size);
		}
		if (byte_count < 0) {
			TIFFError(filename, "Can't read %s %"PRIu32".",
				tiled_image ? "tile" : "strip", chunk_no);
			if (ascii85)
				Ascii85Put('\0', fd);
		}
		/*
		 * for 16 bits, the two bytes must be most significant
		 * byte first
		 */
		if (bitspersample == 16 && !TIFFIsBigEndian(tif)) {
			PS_FlipBytes(buf_data, byte_count);
		}
		/*
		 * For images with alpha, matte against a white background;
		 * i.e. Cback * (1 - Aimage) where Cback = 1. We will fill the
		 * lower part of the buffer with the modified values.
		 *
		 * XXX: needs better solution
		 */
		if (alpha) {
			int adjust, i, j = 0;
			int ncomps = samplesperpixel - extrasamples;
			for (i = 0; (i + ncomps) < byte_count; i+=samplesperpixel) {
				adjust = 255 - buf_data[i + ncomps];
				switch (ncomps) {
					case 1:
						buf_data[j++] = buf_data[i] + adjust;
						break;
					case 2:
						buf_data[j++] = buf_data[i] + adjust;
						buf_data[j++] = buf_data[i+1] + adjust;
						break;
					case 3:
						buf_data[j++] = buf_data[i] + adjust;
						buf_data[j++] = buf_data[i+1] + adjust;
						buf_data[j++] = buf_data[i+2] + adjust;
						break;
				}
			}
			byte_count -= j;
		}

		if (ascii85) {
#if defined( EXP_ASCII85ENCODER )
			ascii85_l = Ascii85EncodeBlock(ascii85_p, 1, buf_data, byte_count );

			if ( ascii85_l > 0 )
				fwrite( ascii85_p, ascii85_l, 1, fd );
#else
			for (cp = buf_data; byte_count > 0; byte_count--)
				Ascii85Put(*cp++, fd);
#endif
		}
		else
		{
			for (cp = buf_data; byte_count > 0; byte_count--) {
				putc(hex[((*cp)>>4)&0xf], fd);
				putc(hex[(*cp)&0xf], fd);
				cp++;

				if (--breaklen <= 0) {
					putc('\n', fd);
					breaklen = MAXLINE;
				}
			}
		}

		if ( !ascii85 ) {
			if ( level2 || level3 )
				putc( '>', fd );
			putc('\n', fd);
		}
#if !defined( EXP_ASCII85ENCODER )
		else
			Ascii85Flush(fd);
#endif
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85_p )
	    _TIFFfree( ascii85_p );
#endif
       
	_TIFFfree(buf_data);
#ifdef ENABLE_BROKEN_BEGINENDDATA
	fputs("%%EndData\n", fd);
#endif
	return(TRUE);
}

void
PSpage(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	char	*	imageOp = "image";

	if ( useImagemask && (bitspersample == 1) )
		imageOp = "imagemask";

	if ((level2 || level3) && PS_Lvl2page(fd, tif, w, h))
		return;
	ps_bytesperrow = tf_bytesperrow - (extrasamples * bitspersample / 8)*w;
	switch (photometric) {
	case PHOTOMETRIC_RGB:
		if (planarconfiguration == PLANARCONFIG_CONTIG) {
			fprintf(fd, "%s", RGBcolorimage);
			PSColorContigPreamble(fd, w, h, 3);
			PSDataColorContig(fd, tif, w, h, 3);
		} else {
			PSColorSeparatePreamble(fd, w, h, 3);
			PSDataColorSeparate(fd, tif, w, h, 3);
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		/* XXX should emit CMYKcolorimage */
		if (planarconfiguration == PLANARCONFIG_CONTIG) {
			PSColorContigPreamble(fd, w, h, 4);
			PSDataColorContig(fd, tif, w, h, 4);
		} else {
			PSColorSeparatePreamble(fd, w, h, 4);
			PSDataColorSeparate(fd, tif, w, h, 4);
		}
		break;
	case PHOTOMETRIC_PALETTE:
		fprintf(fd, "%s", RGBcolorimage);
		PhotoshopBanner(fd, w, h, 1, 3, "false 3 colorimage");
		fprintf(fd, "/scanLine %"TIFF_SSIZE_FORMAT" string def\n",
		    ps_bytesperrow * 3);
		fprintf(fd, "%"PRIu32" %"PRIu32" 8\n",
		    w, h);
		fprintf(fd, "[%"PRIu32" 0 0 -%"PRIu32" 0 %"PRIu32"]\n",
		    w, h, h);
		fprintf(fd, "{currentfile scanLine readhexstring pop} bind\n");
		fprintf(fd, "false 3 colorimage\n");
		PSDataPalette(fd, tif, w, h);
		break;
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		PhotoshopBanner(fd, w, h, 1, 1, imageOp);
		fprintf(fd, "/scanLine %"TIFF_SSIZE_FORMAT" string def\n",
		    ps_bytesperrow);
		fprintf(fd, "%"PRIu32" %"PRIu32" %"PRIu16"\n",
		    w, h, bitspersample);
		fprintf(fd, "[%"PRIu32" 0 0 -%"PRIu32" 0 %"PRIu32"]\n",
		    w, h, h);
		fprintf(fd,
		    "{currentfile scanLine readhexstring pop} bind\n");
		fprintf(fd, "%s\n", imageOp);
		PSDataBW(fd, tif, w, h);
		break;
	}
	putc('\n', fd);
}

void
PSColorContigPreamble(FILE* fd, uint32_t w, uint32_t h, int nc)
{
	ps_bytesperrow = nc * (tf_bytesperrow / samplesperpixel);
	PhotoshopBanner(fd, w, h, 1, nc, "false %d colorimage");
	fprintf(fd, "/line %"TIFF_SSIZE_FORMAT" string def\n", ps_bytesperrow);
	fprintf(fd, "%"PRIu32" %"PRIu32" %"PRIu16"\n",
	    w, h, bitspersample);
	fprintf(fd, "[%"PRIu32" 0 0 -%"PRIu32" 0 %"PRIu32"]\n",
	    w, h, h);
	fprintf(fd, "{currentfile line readhexstring pop} bind\n");
	fprintf(fd, "false %d colorimage\n", nc);
}

void
PSColorSeparatePreamble(FILE* fd, uint32_t w, uint32_t h, int nc)
{
	int i;

	PhotoshopBanner(fd, w, h, ps_bytesperrow, nc, "true %d colorimage");
	for (i = 0; i < nc; i++)
		fprintf(fd, "/line%d %"TIFF_SSIZE_FORMAT" string def\n",
		    i, ps_bytesperrow);
	fprintf(fd, "%"PRIu32" %"PRIu32" %"PRIu16"\n",
	    w, h, bitspersample);
	fprintf(fd, "[%"PRIu32" 0 0 -%"PRIu32" 0 %"PRIu32"] \n",
	    w, h, h);
	for (i = 0; i < nc; i++)
		fprintf(fd, "{currentfile line%d readhexstring pop}bind\n", i);
	fprintf(fd, "true %d colorimage\n", nc);
}

#define	DOBREAK(len, howmany, fd) \
	if (((len) -= (howmany)) <= 0) {	\
		putc('\n', fd);			\
		(len) = MAXLINE-(howmany);	\
	}

static inline void
puthex(unsigned int c, FILE *fd)
{
  putc(hex[((c)>>4)&0xf],fd);
  putc(hex[(c)&0xf],fd);
}

void
PSDataColorContig(FILE* fd, TIFF* tif, uint32_t w, uint32_t h, int nc)
{
	uint32_t row;
	int breaklen = MAXLINE, es = samplesperpixel - nc;
	tsize_t cc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
        if( es < 0 )
        {
            TIFFError(filename, "Inconsistent value of es: %d (samplesperpixel=%"PRIu16", nc=%d)", es, samplesperpixel, nc);
            return;
        }
	tf_buf = (unsigned char *) limitMalloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		cp = tf_buf;
		/*
		 * for 16 bits, the two bytes must be most significant
		 * byte first
		 */
		if (bitspersample == 16 && !HOST_BIGENDIAN) {
			PS_FlipBytes(cp, tf_bytesperrow);
		}
		if (alpha) {
			int adjust;
			/*
			 * the code inside this loop reads nc bytes + 1 extra byte (for adjust)
			 */
			for (cc = 0; (cc + nc) < tf_bytesperrow; cc += samplesperpixel) {
				DOBREAK(breaklen, nc, fd);
				/*
				 * For images with alpha, matte against
				 * a white background; i.e.
				 *    Cback * (1 - Aimage)
				 * where Cback = 1.
				 */
				adjust = 255 - cp[nc];
				for (int i = 0; i < nc; ++i)
                {
                    c = *cp++ + adjust;
                    puthex(c,fd);
                }
				cp += es;
			}
		} else {
			/*
			 * the code inside this loop reads nc bytes per iteration
			 */
			for (cc = 0; (cc + nc) <= tf_bytesperrow; cc += samplesperpixel) {
				DOBREAK(breaklen, nc, fd);
				for (int i = 0; i < nc; ++i)
                {
                    c = *cp++;
                    puthex(c,fd);
                }
				cp += es;
			}
		}
	}
	_TIFFfree((char *) tf_buf);
}

void
PSDataColorSeparate(FILE* fd, TIFF* tif, uint32_t w, uint32_t h, int nc)
{
	uint32_t row;
	int breaklen = MAXLINE;
	tsize_t cc;
	tsample_t s, maxs;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
	tf_buf = (unsigned char *) limitMalloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	maxs = (samplesperpixel > nc ? nc : samplesperpixel);
	for (row = 0; row < h; row++) {
		for (s = 0; s < maxs; s++) {
			if (TIFFReadScanline(tif, tf_buf, row, s) < 0)
				goto end_loop;
			for (cp = tf_buf, cc = 0; cc < tf_bytesperrow; cc++) {
				DOBREAK(breaklen, 1, fd);
				c = *cp++;
				puthex(c,fd);
			}
		}
	}
end_loop:
	_TIFFfree((char *) tf_buf);
}

#define	PUTRGBHEX(c,fd) \
	puthex(rmap[c],fd); puthex(gmap[c],fd); puthex(bmap[c],fd)

void
PSDataPalette(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	uint16_t *rmap, *gmap, *bmap;
	uint32_t row;
	int breaklen = MAXLINE, nc;
	tsize_t cc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		TIFFError(filename, "Palette image w/o \"Colormap\" tag");
		return;
	}
	switch (bitspersample) {
	case 8:	case 4: case 2: case 1:
		break;
	default:
		TIFFError(filename, "Depth %"PRIu16" not supported", bitspersample);
		return;
	}
	nc = 3 * (8 / bitspersample);
	tf_buf = (unsigned char *) limitMalloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	if (checkcmap(tif, 1<<bitspersample, rmap, gmap, bmap) == 16) {
		int i;
#define	CVT(x)		((unsigned short) (((x) * 255) / ((1U<<16)-1)))
		for (i = (1<<bitspersample)-1; i >= 0; i--) {
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
#undef CVT
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			goto end_loop;
		for (cp = tf_buf, cc = 0; cc < tf_bytesperrow; cc++) {
			DOBREAK(breaklen, nc, fd);
			switch (bitspersample) {
			case 8:
				c = *cp++; PUTRGBHEX(c, fd);
				break;
			case 4:
				c = *cp++; PUTRGBHEX(c&0xf, fd);
				c >>= 4;   PUTRGBHEX(c, fd);
				break;
			case 2:
				c = *cp++; PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c, fd);
				break;
			case 1:
				c = *cp++; PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c, fd);
				break;
			}
		}
	}
end_loop:
	_TIFFfree((char *) tf_buf);
}

void
PSDataBW(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	int breaklen = MAXLINE;
	unsigned char* tf_buf;
	unsigned char* cp;
	tsize_t stripsize = TIFFStripSize(tif);
	tstrip_t s;

#if defined( EXP_ASCII85ENCODER )
	tsize_t	ascii85_l;		/* Length, in bytes, of ascii85_p[] data */
	uint8_t	*ascii85_p = 0;		/* Holds ASCII85 encoded data */
#endif

	(void) w; (void) h;
	tf_buf = (unsigned char *) limitMalloc(stripsize);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}

	// FIXME
	memset(tf_buf, 0, stripsize);

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85 ) {
	    /*
	     * Allocate a buffer to hold the ASCII85 encoded data.  Note
	     * that it is allocated with sufficient room to hold the
	     * encoded data (5*stripsize/4) plus the EOD marker (+8)
	     * and formatting line breaks.  The line breaks are more
	     * than taken care of by using 6*stripsize/4 rather than
	     * 5*stripsize/4.
	     */

	    ascii85_p = limitMalloc( (stripsize+(stripsize/2)) + 8 );

	    if ( !ascii85_p ) {
		_TIFFfree( tf_buf );

		TIFFError( filename, "Cannot allocate ASCII85 encoding buffer." );
		return;
	    }
	}
#endif

	if (ascii85)
		Ascii85Init();

	for (s = 0; s < TIFFNumberOfStrips(tif); s++) {
		tmsize_t cc = TIFFReadEncodedStrip(tif, s, tf_buf, stripsize);
		if (cc < 0) {
			TIFFError(filename, "Can't read strip");
			break;
		}
		cp = tf_buf;
		if (photometric == PHOTOMETRIC_MINISWHITE) {
			for (cp += cc; --cp >= tf_buf;)
				*cp = ~*cp;
			cp++;
		}
		/*
		 * for 16 bits, the two bytes must be most significant
		 * byte first
		 */
		if (bitspersample == 16 && !HOST_BIGENDIAN) {
			PS_FlipBytes(cp, cc);
		}
		if (ascii85) {
#if defined( EXP_ASCII85ENCODER )
			if (alpha) {
				int adjust, i;
				for (i = 0; i < (cc - 1); i+=2) {
					adjust = 255 - cp[i + 1];
				    cp[i / 2] = cp[i] + adjust;
				}
				cc /= 2;
			}

			ascii85_l = Ascii85EncodeBlock( ascii85_p, 1, cp, cc );

			if ( ascii85_l > 0 )
			    fwrite( ascii85_p, ascii85_l, 1, fd );
#else
			while (cc-- > 0)
				Ascii85Put(*cp++, fd);
#endif /* EXP_ASCII85_ENCODER */
		} else {
			unsigned char c;

			if (alpha) {
				int adjust;
				while (cc-- > 1) {
					DOBREAK(breaklen, 1, fd);
					/*
					 * For images with alpha, matte against
					 * a white background; i.e.
					 *    Cback * (1 - Aimage)
					 * where Cback = 1.
					 */
					adjust = 255 - cp[1];
					c = *cp++ + adjust; puthex(c,fd);
					cp++, cc--;
				}
			} else {
				while (cc-- > 0) {
					c = *cp++;
					DOBREAK(breaklen, 1, fd);
					puthex(c, fd);
				}
			}
		}
	}

	if ( !ascii85 )
	{
	    if ( level2 || level3)
		fputs(">\n", fd);
	}
#if !defined( EXP_ASCII85ENCODER )
	else
	    Ascii85Flush(fd);
#else
	if ( ascii85_p )
	    _TIFFfree( ascii85_p );
#endif

	_TIFFfree(tf_buf);
}

void
PSRawDataBW(FILE* fd, TIFF* tif, uint32_t w, uint32_t h)
{
	uint64_t *bc;
	uint32_t bufsize;
	int breaklen = MAXLINE;
	tmsize_t cc;
	uint16_t fillorder;
	unsigned char *tf_buf;
	unsigned char *cp, c;
	tstrip_t s;

#if defined( EXP_ASCII85ENCODER )
	tsize_t 		ascii85_l;		/* Length, in bytes, of ascii85_p[] data */
	uint8_t		*	ascii85_p = 0;		/* Holds ASCII85 encoded data */
#endif

	(void) w; (void) h;
	TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);
	TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &bc);

	/*
	 * Find largest strip:
	 */

	bufsize = (uint32_t) bc[0];

	for ( s = 0; ++s < tf_numberstrips; ) {
		if ( bc[s] > bufsize )
			bufsize = (uint32_t) bc[s];
	}

	tf_buf = (unsigned char*) limitMalloc(bufsize);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for strip buffer");
		return;
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85 ) {
	    /*
	     * Allocate a buffer to hold the ASCII85 encoded data.  Note
	     * that it is allocated with sufficient room to hold the
	     * encoded data (5*bufsize/4) plus the EOD marker (+8)
	     * and formatting line breaks.  The line breaks are more
	     * than taken care of by using 6*bufsize/4 rather than
	     * 5*bufsize/4.
	     */

	    ascii85_p = limitMalloc( (bufsize+(bufsize/2)) + 8 );

	    if ( !ascii85_p ) {
		_TIFFfree( tf_buf );

		TIFFError( filename, "Cannot allocate ASCII85 encoding buffer." );
		return;
	    }
	}
#endif

	for (s = 0; s < tf_numberstrips; s++) {
		cc = TIFFReadRawStrip(tif, s, tf_buf, (tmsize_t) bc[s]);
		if (cc < 0) {
			TIFFError(filename, "Can't read strip");
			break;
		}
		if (fillorder == FILLORDER_LSB2MSB)
			TIFFReverseBits(tf_buf, cc);
		if (!ascii85) {
			for (cp = tf_buf; cc > 0; cc--) {
				DOBREAK(breaklen, 1, fd);
				c = *cp++;
				puthex(c, fd);
			}
			fputs(">\n", fd);
			breaklen = MAXLINE;
		} else {
			Ascii85Init();
#if defined( EXP_ASCII85ENCODER )
			ascii85_l = Ascii85EncodeBlock( ascii85_p, 1, tf_buf, cc );

			if ( ascii85_l > 0 )
				fwrite( ascii85_p, ascii85_l, 1, fd );
#else
			for (cp = tf_buf; cc > 0; cc--)
				Ascii85Put(*cp++, fd);
			Ascii85Flush(fd);
#endif	/* EXP_ASCII85ENCODER */
		}
	}
	_TIFFfree((char *) tf_buf);

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85_p )
		_TIFFfree( ascii85_p );
#endif
}

void
Ascii85Init(void)
{
	ascii85breaklen = 2*MAXLINE;
	ascii85count = 0;
}

static char*
Ascii85Encode(unsigned char* raw)
{
	static char encoded[6];
	uint32_t word;

	word = (((raw[0]<<8)+raw[1])<<16) + (raw[2]<<8) + raw[3];
	if (word != 0L) {
		uint32_t q;
		uint16_t w1;

		q = word / (85L*85*85*85);	/* actually only a byte */
		encoded[0] = (char) (q + '!');

		word -= q * (85L*85*85*85); q = word / (85L*85*85);
		encoded[1] = (char) (q + '!');

		word -= q * (85L*85*85); q = word / (85*85);
		encoded[2] = (char) (q + '!');

		w1 = (uint16_t) (word - q * (85L * 85));
		encoded[3] = (char) ((w1 / 85) + '!');
		encoded[4] = (char) ((w1 % 85) + '!');
		encoded[5] = '\0';
	} else
		encoded[0] = 'z', encoded[1] = '\0';
	return (encoded);
}

void
Ascii85Put(unsigned char code, FILE* fd)
{
	ascii85buf[ascii85count++] = code;
	if (ascii85count >= 4) {
		unsigned char* p;
		int n;

		for (n = ascii85count, p = ascii85buf; n >= 4; n -= 4, p += 4) {
			char* cp;
			for (cp = Ascii85Encode(p); *cp; cp++) {
				putc(*cp, fd);
				if (--ascii85breaklen == 0) {
					putc('\n', fd);
					ascii85breaklen = 2*MAXLINE;
				}
			}
		}
		_TIFFmemcpy(ascii85buf, p, n);
		ascii85count = n;
	}
}

void
Ascii85Flush(FILE* fd)
{
	if (ascii85count > 0) {
		char* res;
		_TIFFmemset(&ascii85buf[ascii85count], 0, 3);
		res = Ascii85Encode(ascii85buf);
		fwrite(res[0] == 'z' ? "!!!!" : res, ascii85count + 1, 1, fd);
	}
	fputs("~>\n", fd);
}
#if	defined( EXP_ASCII85ENCODER)

#define A85BREAKCNTR    ascii85breaklen
#define A85BREAKLEN     (2*MAXLINE)

/*****************************************************************************
*
* Name:         Ascii85EncodeBlock( ascii85_p, f_eod, raw_p, raw_l )
*
* Description:  This routine will encode the raw data in the buffer described
*               by raw_p and raw_l into ASCII85 format and store the encoding
*               in the buffer given by ascii85_p.
*
* Parameters:   ascii85_p   -   A buffer supplied by the caller which will
*                               contain the encoded ASCII85 data.
*               f_eod       -   Flag: Nz means to end the encoded buffer with
*                               an End-Of-Data marker.
*               raw_p       -   Pointer to the buffer of data to be encoded
*               raw_l       -   Number of bytes in raw_p[] to be encoded
*
* Returns:      (int)   <   0   Error, see errno
*                       >=  0   Number of bytes written to ascii85_p[].
*
* Notes:        An external variable given by A85BREAKCNTR is used to
*               determine when to insert newline characters into the
*               encoded data.  As each byte is placed into ascii85_p this
*               external is decremented.  If the variable is decrement to
*               or past zero then a newline is inserted into ascii85_p
*               and the A85BREAKCNTR is then reset to A85BREAKLEN.
*                   Note:  for efficiency reasons the A85BREAKCNTR variable
*                          is not actually checked on *every* character
*                          placed into ascii85_p but often only for every
*                          5 characters.
*
*               THE CALLER IS RESPONSIBLE FOR ENSURING THAT ASCII85_P[] IS
*               SUFFICIENTLY LARGE TO THE ENCODED DATA!
*                   You will need at least 5 * (raw_l/4) bytes plus space for
*                   newline characters and space for an EOD marker (if
*                   requested).  A safe calculation is to use 6*(raw_l/4) + 8
*                   to size ascii85_p.
*
*****************************************************************************/

tsize_t Ascii85EncodeBlock(uint8_t * ascii85_p, unsigned f_eod, const uint8_t * raw_p, tsize_t raw_l )

{
    char                        ascii85[5];     /* Encoded 5 tuple */
    tsize_t                     ascii85_l;      /* Number of bytes written to ascii85_p[] */
    int                         rc;             /* Return code */
    uint32_t                      val32;          /* Unencoded 4 tuple */

    ascii85_l = 0;                              /* Nothing written yet */

    if ( raw_p )
    {
        --raw_p;                                /* Prepare for pre-increment fetches */

        for ( ; raw_l > 3; raw_l -= 4 )
        {
            val32  = (uint32_t)*(++raw_p) << 24;
            val32 += (uint32_t)*(++raw_p) << 16;
            val32 += (uint32_t)*(++raw_p) << 8;
            val32 += (uint32_t)*(++raw_p);
    
            if ( val32 == 0 )                   /* Special case */
            {
                ascii85_p[ascii85_l] = 'z';
                rc = 1;
            }
    
            else
            {
                ascii85[4] = (char) ((val32 % 85) + 33);
                val32 /= 85;
    
                ascii85[3] = (char) ((val32 % 85) + 33);
                val32 /= 85;
    
                ascii85[2] = (char) ((val32 % 85) + 33);
                val32 /= 85;
    
                ascii85[1] = (char) ((val32 % 85) + 33);
                ascii85[0] = (char) ((val32 / 85) + 33);

                _TIFFmemcpy( &ascii85_p[ascii85_l], ascii85, sizeof(ascii85) );
                rc = sizeof(ascii85);
            }
    
            ascii85_l += rc;
    
            if ( (A85BREAKCNTR -= rc) <= 0 )
            {
                ascii85_p[ascii85_l] = '\n';
                ++ascii85_l;
                A85BREAKCNTR = A85BREAKLEN;
            }
        }
    
        /*
         * Output any straggler bytes:
         */
    
        if ( raw_l > 0 )
        {
            tsize_t         len;                /* Output this many bytes */
    
            len = raw_l + 1;
            val32 = (uint32_t)*++raw_p << 24;             /* Prime the pump */
    
            if ( --raw_l > 0 )  val32 += *(++raw_p) << 16;
            if ( --raw_l > 0 )  val32 += *(++raw_p) <<  8;
    
            val32 /= 85;
    
            ascii85[3] = (char) ((val32 % 85) + 33);
            val32 /= 85;
    
            ascii85[2] = (char) ((val32 % 85) + 33);
            val32 /= 85;
    
            ascii85[1] = (char) ((val32 % 85) + 33);
            ascii85[0] = (char) ((val32 / 85) + 33);
    
            _TIFFmemcpy( &ascii85_p[ascii85_l], ascii85, len );
            ascii85_l += len;
        }
    }

    /*
     * If requested add an ASCII85 End Of Data marker:
     */

    if ( f_eod )
    {
        ascii85_p[ascii85_l++] = '~';
        ascii85_p[ascii85_l++] = '>';
        ascii85_p[ascii85_l++] = '\n';
    }

    return ( ascii85_l );

}   /* Ascii85EncodeBlock() */

#endif	/* EXP_ASCII85ENCODER */


static const char usage_info[] =
"Convert a TIFF image to PostScript\n\n"
"usage: tiff2ps [options] input.tif ...\n"
"where options are:\n"
" -1            generate PostScript Level 1 (default)\n"
" -2            generate PostScript Level 2\n"
" -3            generate PostScript Level 3\n"
" -8            disable use of ASCII85 encoding with PostScript Level 2/3\n"
" -a            convert all directories in file (default is first), Not EPS\n"
" -b #          set the bottom margin to # inches\n"
" -c            center image (-b and -l still add to this)\n"
" -C name       set postscript document creator name\n"
" -d #          set initial directory to # counting from zero\n"
" -D            enable duplex printing (two pages per sheet of paper)\n"
" -e            generate Encapsulated PostScript (EPS) (implies -z)\n"
" -h #          set printed page height to # inches (no default)\n"
" -w #          set printed page width to # inches (no default)\n"
" -H #          split image if height is more than # inches\n"
" -W #          split image if width is more than # inches\n"
" -L #          overLap split images by # inches\n"
" -i #          enable/disable (Nz/0) pixel interpolation (default: enable)\n"
" -l #          set the left margin to # inches\n"
" -m            use \"imagemask\" operator instead of \"image\"\n"
" -M size       set the memory allocation limit in MiB. 0 to disable limit\n"
" -o #          convert directory at file offset # bytes\n"
" -O file       write PostScript to file instead of standard output\n"
" -p            generate regular (non-encapsulated) PostScript\n"
" -P L or P     set optional PageOrientation DSC comment to Landscape or Portrait\n"
" -r # or auto  rotate by 90, 180, 270 degrees or auto\n"
" -s            generate PostScript for a single image\n"
" -t name       set postscript document title. Otherwise the filename is used\n"
" -T            print pages for top edge binding\n"
" -x            override resolution units as centimeters\n"
" -y            override resolution units as inches\n"
" -z            enable printing in the deadzone (only for PostScript Level 2/3)\n"
;

static void
usage(int code)
{
	FILE * out = (code == EXIT_SUCCESS) ? stdout : stderr;

	fprintf(out, "%s\n\n", TIFFGetVersion());
        fprintf(out, "%s", usage_info);
	exit(code);
}

