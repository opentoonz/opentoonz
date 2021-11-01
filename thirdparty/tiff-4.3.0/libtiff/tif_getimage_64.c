/*
  Modified tif_getimage.c that takes uint64_t as output pixel type
*/


/* $Id: tif_getimage.c,v 1.82 2012-06-06 00:17:49 fwarmerdam Exp $ */

/*
 * Copyright (c) 1991-1997 Sam Leffler
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

/*
 * TIFF Library
 *
 * Read and return a packed RGBA image.
 */
#include "tiffiop.h"
#include <stdio.h>
#include <limits.h>

typedef int (*gtFunc_32)(TIFFRGBAImage*, uint32_t*, uint32_t, uint32_t);
typedef int (*gtFunc_64)(TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t);
typedef void (*tileContigRoutine_64)
    (TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,
	unsigned char*);
typedef void (*tileSeparateRoutine_64)
    (TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t, uint32_t, uint32_t, int32_t, int32_t,
	unsigned char*, unsigned char*, unsigned char*, unsigned char*);


static int gtTileContig(TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t);
static int gtTileSeparate(TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t);
static int gtStripContig(TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t);
static int gtStripSeparate(TIFFRGBAImage*, uint64_t*, uint32_t, uint32_t);
static int PickContigCase(TIFFRGBAImage*);
static int PickSeparateCase(TIFFRGBAImage*);

static int BuildMapUaToAa(TIFFRGBAImage* img);
static int BuildMapBitdepth16To8(TIFFRGBAImage* img);

static const char photoTag[] = "PhotometricInterpretation";

/* 
 * Helper constants used in Orientation tag handling
 */
#define FLIP_VERTICALLY 0x01
#define FLIP_HORIZONTALLY 0x02

/*
 * Color conversion constants. We will define display types here.
 */

static const TIFFDisplay display_sRGB = {
	{			/* XYZ -> luminance matrix */
		{  3.2410F, -1.5374F, -0.4986F },
		{  -0.9692F, 1.8760F, 0.0416F },
		{  0.0556F, -0.2040F, 1.0570F }
	},	
	100.0F, 100.0F, 100.0F,	/* Light o/p for reference white */
	255, 255, 255,		/* Pixel values for ref. white */
	1.0F, 1.0F, 1.0F,	/* Residual light o/p for black pixel */
	2.4F, 2.4F, 2.4F,	/* Gamma values for the three guns */
};


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

int
TIFFRGBAImageBegin_64(TIFFRGBAImage* img, TIFF* tif, int stop, char emsg[1024])
{
	uint16_t* sampleinfo;
	uint16_t extrasamples;
	uint16_t planarconfig;
	uint16_t compress;
	int colorchannels;
	uint16_t *red_orig, *green_orig, *blue_orig;
	int n_color;

	/* Initialize to normal values */
	img->row_offset = 0;
	img->col_offset = 0;
	img->redcmap = NULL;
	img->greencmap = NULL;
	img->bluecmap = NULL;
	img->req_orientation = ORIENTATION_BOTLEFT;     /* It is the default */

	img->tif = tif;
	img->stoponerr = stop;
	TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &img->bitspersample);
	switch (img->bitspersample) {
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
			break;
		default:
			sprintf(emsg, "Sorry, can not handle images with %d-bit samples",
			    img->bitspersample);
			goto fail_return;
	}
	img->alpha = 0;
	TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &img->samplesperpixel);
	TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
	    &extrasamples, &sampleinfo);
	if (extrasamples >= 1)
	{
		switch (sampleinfo[0]) {
			case EXTRASAMPLE_UNSPECIFIED:          /* Workaround for some images without */
				if (img->samplesperpixel > 3)  /* correct info about alpha channel */
					img->alpha = EXTRASAMPLE_ASSOCALPHA;
				break;
			case EXTRASAMPLE_ASSOCALPHA:           /* data is pre-multiplied */
			case EXTRASAMPLE_UNASSALPHA:           /* data is not pre-multiplied */
				img->alpha = sampleinfo[0];
				break;
		}
	}

#ifdef DEFAULT_EXTRASAMPLE_AS_ALPHA
	if( !TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &img->photometric))
		img->photometric = PHOTOMETRIC_MINISWHITE;

	if( extrasamples == 0
	    && img->samplesperpixel == 4
	    && img->photometric == PHOTOMETRIC_RGB )
	{
		img->alpha = EXTRASAMPLE_ASSOCALPHA;
		extrasamples = 1;
	}
#endif

	colorchannels = img->samplesperpixel - extrasamples;
	TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compress);
	TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarconfig);
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &img->photometric)) {
		switch (colorchannels) {
			case 1:
				if (isCCITTCompression(tif))
					img->photometric = PHOTOMETRIC_MINISWHITE;
				else
					img->photometric = PHOTOMETRIC_MINISBLACK;
				break;
			case 3:
				img->photometric = PHOTOMETRIC_RGB;
				break;
			default:
				sprintf(emsg, "Missing needed %s tag", photoTag);
                                goto fail_return;
		}
	}
	switch (img->photometric) {
		case PHOTOMETRIC_PALETTE:
			if (!TIFFGetField(tif, TIFFTAG_COLORMAP,
			    &red_orig, &green_orig, &blue_orig)) {
				sprintf(emsg, "Missing required \"Colormap\" tag");
                                goto fail_return;
			}

			/* copy the colormaps so we can modify them */
			n_color = (1L << img->bitspersample);
			img->redcmap = (uint16_t *) _TIFFmalloc(sizeof(uint16_t)*n_color);
			img->greencmap = (uint16_t *) _TIFFmalloc(sizeof(uint16_t)*n_color);
			img->bluecmap = (uint16_t *) _TIFFmalloc(sizeof(uint16_t)*n_color);
			if( !img->redcmap || !img->greencmap || !img->bluecmap ) {
				sprintf(emsg, "Out of memory for colormap copy");
                                goto fail_return;
			}

			_TIFFmemcpy( img->redcmap, red_orig, n_color * 2 );
			_TIFFmemcpy( img->greencmap, green_orig, n_color * 2 );
			_TIFFmemcpy( img->bluecmap, blue_orig, n_color * 2 );

			/* fall thru... */
		case PHOTOMETRIC_MINISWHITE:
		case PHOTOMETRIC_MINISBLACK:
			if (planarconfig == PLANARCONFIG_CONTIG
			    && img->samplesperpixel != 1
			    && img->bitspersample < 8 ) {
				sprintf(emsg,
				    "Sorry, can not handle contiguous data with %s=%d, "
				    "and %s=%d and Bits/Sample=%d",
				    photoTag, img->photometric,
				    "Samples/pixel", img->samplesperpixel,
				    img->bitspersample);
                                goto fail_return;
			}
			break;
		case PHOTOMETRIC_YCBCR:
			/* It would probably be nice to have a reality check here. */
			if (planarconfig == PLANARCONFIG_CONTIG)
				/* can rely on libjpeg to convert to RGB */
				/* XXX should restore current state on exit */
				switch (compress) {
					case COMPRESSION_JPEG:
						/*
						 * TODO: when complete tests verify complete desubsampling
						 * and YCbCr handling, remove use of TIFFTAG_JPEGCOLORMODE in
						 * favor of tif_getimage.c native handling
						 */
						TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
						img->photometric = PHOTOMETRIC_RGB;
						break;
					default:
						/* do nothing */;
						break;
				}
			/*
			 * TODO: if at all meaningful and useful, make more complete
			 * support check here, or better still, refactor to let supporting
			 * code decide whether there is support and what meaningfull
			 * error to return
			 */
			break;
		case PHOTOMETRIC_RGB:
			if (colorchannels < 3) {
				sprintf(emsg, "Sorry, can not handle RGB image with %s=%d",
				    "Color channels", colorchannels);
                                goto fail_return;
			}
			break;
		case PHOTOMETRIC_SEPARATED:
			{
				uint16_t inkset;
				TIFFGetFieldDefaulted(tif, TIFFTAG_INKSET, &inkset);
				if (inkset != INKSET_CMYK) {
					sprintf(emsg, "Sorry, can not handle separated image with %s=%d",
					    "InkSet", inkset);
                                        goto fail_return;
				}
				if (img->samplesperpixel < 4) {
					sprintf(emsg, "Sorry, can not handle separated image with %s=%d",
					    "Samples/pixel", img->samplesperpixel);
                                        goto fail_return;
				}
			}
			break;
		case PHOTOMETRIC_LOGL:
			if (compress != COMPRESSION_SGILOG) {
				sprintf(emsg, "Sorry, LogL data must have %s=%d",
				    "Compression", COMPRESSION_SGILOG);
                                goto fail_return;
			}
			TIFFSetField(tif, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_8BIT);
			img->photometric = PHOTOMETRIC_MINISBLACK;	/* little white lie */
			img->bitspersample = 8;
			break;
		case PHOTOMETRIC_LOGLUV:
			if (compress != COMPRESSION_SGILOG && compress != COMPRESSION_SGILOG24) {
				sprintf(emsg, "Sorry, LogLuv data must have %s=%d or %d",
				    "Compression", COMPRESSION_SGILOG, COMPRESSION_SGILOG24);
                                goto fail_return;
			}
			if (planarconfig != PLANARCONFIG_CONTIG) {
				sprintf(emsg, "Sorry, can not handle LogLuv images with %s=%d",
				    "Planarconfiguration", planarconfig);
				return (0);
			}
			TIFFSetField(tif, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_8BIT);
			img->photometric = PHOTOMETRIC_RGB;		/* little white lie */
			img->bitspersample = 8;
			break;
		case PHOTOMETRIC_CIELAB:
			break;
		default:
			sprintf(emsg, "Sorry, can not handle image with %s=%d",
			    photoTag, img->photometric);
                        goto fail_return;
	}
	img->Map = NULL;
	img->BWmap = NULL;
	img->PALmap = NULL;
	img->ycbcr = NULL;
	img->cielab = NULL;
	img->UaToAa = NULL;
	img->Bitdepth16To8 = NULL;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &img->width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img->height);
	TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &img->orientation);
	img->isContig =
	    !(planarconfig == PLANARCONFIG_SEPARATE && img->samplesperpixel > 1);
	if (img->isContig) {
		if (!PickContigCase(img)) {
			sprintf(emsg, "Sorry, can not handle image");
			goto fail_return;
		}
	} else {
		if (!PickSeparateCase(img)) {
			sprintf(emsg, "Sorry, can not handle image");
			goto fail_return;
		}
	}
	return 1;

  fail_return:
        _TIFFfree( img->redcmap );
        _TIFFfree( img->greencmap );
        _TIFFfree( img->bluecmap );
        img->redcmap = img->greencmap = img->bluecmap = NULL;
        return 0;
}


int
TIFFRGBAImageGet_64(TIFFRGBAImage* img, uint64_t* raster, uint32_t w, uint32_t h)
{
  gtFunc_64 get = (gtFunc_64) img->get;

    if (img->get == NULL) {
		TIFFErrorExt(img->tif->tif_clientdata, TIFFFileName(img->tif), "No \"get\" routine setup");
		return (0);
	}
	if (img->put.any == NULL) {
		TIFFErrorExt(img->tif->tif_clientdata, TIFFFileName(img->tif),
		"No \"put\" routine setupl; probably can not handle image format");
		return (0);
    }

   // Casting between 2 function pointers is allowed in and C++ and works as expected
   // if re-casted back.
   // See C 6.3.2.3 (8) and 5.2.10 (6)

   return (*get)(img, raster, w, h);
}

static int 
setorientation(TIFFRGBAImage* img)
{
	switch (img->orientation) {
		case ORIENTATION_TOPLEFT:
		case ORIENTATION_LEFTTOP:
			if (img->req_orientation == ORIENTATION_TOPRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTTOP)
				return FLIP_HORIZONTALLY;
			else if (img->req_orientation == ORIENTATION_BOTRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTBOT)
				return FLIP_HORIZONTALLY | FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_BOTLEFT ||
			    img->req_orientation == ORIENTATION_LEFTBOT)
				return FLIP_VERTICALLY;
			else
				return 0;
		case ORIENTATION_TOPRIGHT:
		case ORIENTATION_RIGHTTOP:
			if (img->req_orientation == ORIENTATION_TOPLEFT ||
			    img->req_orientation == ORIENTATION_LEFTTOP)
				return FLIP_HORIZONTALLY;
			else if (img->req_orientation == ORIENTATION_BOTRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTBOT)
				return FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_BOTLEFT ||
			    img->req_orientation == ORIENTATION_LEFTBOT)
				return FLIP_HORIZONTALLY | FLIP_VERTICALLY;
			else
				return 0;
		case ORIENTATION_BOTRIGHT:
		case ORIENTATION_RIGHTBOT:
			if (img->req_orientation == ORIENTATION_TOPLEFT ||
			    img->req_orientation == ORIENTATION_LEFTTOP)
				return FLIP_HORIZONTALLY | FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_TOPRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTTOP)
				return FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_BOTLEFT ||
			    img->req_orientation == ORIENTATION_LEFTBOT)
				return FLIP_HORIZONTALLY;
			else
				return 0;
		case ORIENTATION_BOTLEFT:
		case ORIENTATION_LEFTBOT:
			if (img->req_orientation == ORIENTATION_TOPLEFT ||
			    img->req_orientation == ORIENTATION_LEFTTOP)
				return FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_TOPRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTTOP)
				return FLIP_HORIZONTALLY | FLIP_VERTICALLY;
			else if (img->req_orientation == ORIENTATION_BOTRIGHT ||
			    img->req_orientation == ORIENTATION_RIGHTBOT)
				return FLIP_HORIZONTALLY;
			else
				return 0;
		default:	/* NOTREACHED */
			return 0;
	}
}

/*
 * Get an tile-organized image that has
 *	PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *	SamplesPerPixel == 1
 */	
static int
gtTileContig(TIFFRGBAImage* img, uint64_t* raster, uint32_t w, uint32_t h)
{
    TIFF* tif = img->tif;
    tileContigRoutine_64 put = (tileContigRoutine_64) img->put.contig;
    uint32_t col, row, y, rowstoread;
    tmsize_t pos;
    uint32_t tw, th;
    unsigned char* buf;
    int32_t fromskew, toskew;
    uint32_t nrow;
    int ret = 1, flip;

    buf = (unsigned char*) _TIFFmalloc(TIFFTileSize(tif));
    if (buf == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "%s", "No space for tile buffer");
		return (0);
    }
    _TIFFmemset(buf, 0, TIFFTileSize(tif));
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);

    flip = setorientation(img);
    if (flip & FLIP_VERTICALLY) {
	    y = h - 1;
	    toskew = -(int32_t)(tw + w);
    }
    else {
	    y = 0;
	    toskew = -(int32_t)(tw - w);
    }
     
    for (row = 0; row < h; row += nrow)
    {
        rowstoread = th - (row + img->row_offset) % th;
    	nrow = (row + rowstoread > h ? h - row : rowstoread);
	for (col = 0; col < w; col += tw) 
        {
	    if (TIFFReadTile(tif, buf, col+img->col_offset,  
			     row+img->row_offset, 0, 0)==(tmsize_t)(-1) && img->stoponerr)
            {
                ret = 0;
                break;
            }
	    
	    pos = ((row+img->row_offset) % th) * TIFFTileRowSize(tif);  

    	    if (col + tw > w) 
            {
                /*
                 * Tile is clipped horizontally.  Calculate
                 * visible portion and skewing factors.
                 */
                uint32_t npix = w - col;
                fromskew = tw - npix;
                (*put)(img, raster+y*w+col, col, y,
                       npix, nrow, fromskew, toskew + fromskew, buf + pos);
            }
            else 
            {
                (*put)(img, raster+y*w+col, col, y, tw, nrow, 0, toskew, buf + pos);
            }
        }

        y += (flip & FLIP_VERTICALLY ? -(int32_t) nrow : (int32_t) nrow);
    }
    _TIFFfree(buf);

    if (flip & FLIP_HORIZONTALLY) {
	    uint32_t line;

	    for (line = 0; line < h; line++) {
		    uint64_t *left = raster + (line * w);
		    uint64_t *right = left + w - 1;
		    
		    while ( left < right ) {
			    uint64_t temp = *left;
			    *left = *right;
			    *right = temp;
			    left++, right--;
		    }
	    }
    }

    return (ret);
}

/*
 * Get an tile-organized image that has
 *	 SamplesPerPixel > 1
 *	 PlanarConfiguration separated
 * We assume that all such images are RGB.
 */	
static int
gtTileSeparate(TIFFRGBAImage* img, uint64_t* raster, uint32_t w, uint32_t h)
{
	TIFF* tif = img->tif;
	tileSeparateRoutine_64 put = (tileSeparateRoutine_64) img->put.separate;
	uint32_t col, row, y, rowstoread;
	tmsize_t pos;
	uint32_t tw, th;
	unsigned char* buf;
	unsigned char* p0;
	unsigned char* p1;
	unsigned char* p2;
	unsigned char* pa;
	tmsize_t tilesize;
	tmsize_t bufsize;
	int32_t fromskew, toskew;
	int alpha = img->alpha;
	uint32_t nrow;
	int ret = 1, flip;
        int colorchannels;

	tilesize = TIFFTileSize(tif);  
	bufsize = TIFFSafeMultiply(tmsize_t,alpha?4:3,tilesize);
	if (bufsize == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "Integer overflow in %s", "gtTileSeparate");
		return (0);
	}
	buf = (unsigned char*) _TIFFmalloc(bufsize);
	if (buf == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "%s", "No space for tile buffer");
		return (0);
	}
	_TIFFmemset(buf, 0, bufsize);
	p0 = buf;
	p1 = p0 + tilesize;
	p2 = p1 + tilesize;
	pa = (alpha?(p2+tilesize):NULL);
	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
	TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);

	flip = setorientation(img);
	if (flip & FLIP_VERTICALLY) {
		y = h - 1;
		toskew = -(int32_t)(tw + w);
	}
	else {
		y = 0;
		toskew = -(int32_t)(tw - w);
	}

        switch( img->photometric )
        {
          case PHOTOMETRIC_MINISWHITE:
          case PHOTOMETRIC_MINISBLACK:
          case PHOTOMETRIC_PALETTE:
            colorchannels = 1;
            p2 = p1 = p0;
            break;

          default:
            colorchannels = 3;
            break;
        }

	for (row = 0; row < h; row += nrow)
	{
		rowstoread = th - (row + img->row_offset) % th;
		nrow = (row + rowstoread > h ? h - row : rowstoread);
		for (col = 0; col < w; col += tw)
		{
			if (TIFFReadTile(tif, p0, col+img->col_offset,  
			    row+img->row_offset,0,0)==(tmsize_t)(-1) && img->stoponerr)
			{
				ret = 0;
				break;
			}
			if (colorchannels > 1 
                            && TIFFReadTile(tif, p1, col+img->col_offset,  
                                            row+img->row_offset,0,1) == (tmsize_t)(-1) 
                            && img->stoponerr)
			{
				ret = 0;
				break;
			}
			if (colorchannels > 1 
                            && TIFFReadTile(tif, p2, col+img->col_offset,  
                                            row+img->row_offset,0,2) == (tmsize_t)(-1) 
                            && img->stoponerr)
			{
				ret = 0;
				break;
			}
			if (alpha
                            && TIFFReadTile(tif,pa,col+img->col_offset,  
                                            row+img->row_offset,0,colorchannels) == (tmsize_t)(-1) 
                            && img->stoponerr)
                        {
                            ret = 0;
                            break;
			}

			pos = ((row+img->row_offset) % th) * TIFFTileRowSize(tif);  

			if (col + tw > w)
			{
				/*
				 * Tile is clipped horizontally.  Calculate
				 * visible portion and skewing factors.
				 */
				uint32_t npix = w - col;
				fromskew = tw - npix;
				(*put)(img, raster+y*w+col, col, y,
				    npix, nrow, fromskew, toskew + fromskew,
				    p0 + pos, p1 + pos, p2 + pos, (alpha?(pa+pos):NULL));
			} else {
				(*put)(img, raster+y*w+col, col, y,
				    tw, nrow, 0, toskew, p0 + pos, p1 + pos, p2 + pos, (alpha?(pa+pos):NULL));
			}
		}

		y += (flip & FLIP_VERTICALLY ?-(int32_t) nrow : (int32_t) nrow);
	}

	if (flip & FLIP_HORIZONTALLY) {
		uint32_t line;

		for (line = 0; line < h; line++) {
			uint64_t *left = raster + (line * w);
			uint64_t *right = left + w - 1;

			while ( left < right ) {
				uint64_t temp = *left;
				*left = *right;
				*right = temp;
				left++, right--;
			}
		}
	}

	_TIFFfree(buf);
	return (ret);
}

/*
 * Get a strip-organized image that has
 *	PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *	SamplesPerPixel == 1
 */	
static int
gtStripContig(TIFFRGBAImage* img, uint64_t* raster, uint32_t w, uint32_t h)
{
	TIFF* tif = img->tif;
	tileContigRoutine_64 put = (tileContigRoutine_64) img->put.contig;
	uint32_t row, y, nrow, nrowsub, rowstoread;
	tmsize_t pos;
	unsigned char* buf;
	uint32_t rowsperstrip;
	uint16_t subsamplinghor,subsamplingver;
	uint32_t imagewidth = img->width;
	tmsize_t scanline;
	int32_t fromskew, toskew;
	int ret = 1, flip;

	buf = (unsigned char*) _TIFFmalloc(TIFFStripSize(tif));
	if (buf == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "No space for strip buffer");
		return (0);
	}
	_TIFFmemset(buf, 0, TIFFStripSize(tif));

	flip = setorientation(img);
	if (flip & FLIP_VERTICALLY) {
		y = h - 1;
		toskew = -(int32_t)(w + w);
	} else {
		y = 0;
		toskew = -(int32_t)(w - w);
	}

	TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
	TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRSUBSAMPLING, &subsamplinghor, &subsamplingver);
	scanline = TIFFScanlineSize(tif);
	fromskew = (w < imagewidth ? imagewidth - w : 0);
	for (row = 0; row < h; row += nrow)
	{
		rowstoread = rowsperstrip - (row + img->row_offset) % rowsperstrip;
		nrow = (row + rowstoread > h ? h - row : rowstoread);
		nrowsub = nrow;
		if ((nrowsub%subsamplingver)!=0)
			nrowsub+=subsamplingver-nrowsub%subsamplingver;
		if (TIFFReadEncodedStrip(tif,
		    TIFFComputeStrip(tif,row+img->row_offset, 0),
		    buf,
		    ((row + img->row_offset)%rowsperstrip + nrowsub) * scanline)==(tmsize_t)(-1)
		    && img->stoponerr)
		{
			ret = 0;
			break;
		}

		pos = ((row + img->row_offset) % rowsperstrip) * scanline;
		(*put)(img, raster+y*w, 0, y, w, nrow, fromskew, toskew, buf + pos);
		y += (flip & FLIP_VERTICALLY ? -(int32_t) nrow : (int32_t) nrow);
	}

	if (flip & FLIP_HORIZONTALLY) {
		uint32_t line;

		for (line = 0; line < h; line++) {
			uint64_t *left = raster + (line * w);
			uint64_t *right = left + w - 1;

			while ( left < right ) {
				uint64_t temp = *left;
				*left = *right;
				*right = temp;
				left++, right--;
			}
		}
	}

	_TIFFfree(buf);
	return (ret);
}

/*
 * Get a strip-organized image with
 *	 SamplesPerPixel > 1
 *	 PlanarConfiguration separated
 * We assume that all such images are RGB.
 */
static int
gtStripSeparate(TIFFRGBAImage* img, uint64_t* raster, uint32_t w, uint32_t h)
{
	TIFF* tif = img->tif;
	tileSeparateRoutine_64 put = (tileSeparateRoutine_64) img->put.separate;
	unsigned char *buf;
	unsigned char *p0, *p1, *p2, *pa;
	uint32_t row, y, nrow, rowstoread;
	tmsize_t pos;
	tmsize_t scanline;
	uint32_t rowsperstrip, offset_row;
	uint32_t imagewidth = img->width;
	tmsize_t stripsize;
	tmsize_t bufsize;
	int32_t fromskew, toskew;
	int alpha = img->alpha;
	int ret = 1, flip, colorchannels;

	stripsize = TIFFStripSize(tif);  
	bufsize = TIFFSafeMultiply(tmsize_t,alpha?4:3,stripsize);
	if (bufsize == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "Integer overflow in %s", "gtStripSeparate");
		return (0);
	}
	p0 = buf = (unsigned char *)_TIFFmalloc(bufsize);
	if (buf == 0) {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "No space for tile buffer");
		return (0);
	}
	_TIFFmemset(buf, 0, bufsize);
	p1 = p0 + stripsize;
	p2 = p1 + stripsize;
	pa = (alpha?(p2+stripsize):NULL);

	flip = setorientation(img);
	if (flip & FLIP_VERTICALLY) {
		y = h - 1;
		toskew = -(int32_t)(w + w);
	}
	else {
		y = 0;
		toskew = -(int32_t)(w - w);
	}

        switch( img->photometric )
        {
          case PHOTOMETRIC_MINISWHITE:
          case PHOTOMETRIC_MINISBLACK:
          case PHOTOMETRIC_PALETTE:
            colorchannels = 1;
            p2 = p1 = p0;
            break;

          default:
            colorchannels = 3;
            break;
        }

	TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
	scanline = TIFFScanlineSize(tif);  
	fromskew = (w < imagewidth ? imagewidth - w : 0);
	for (row = 0; row < h; row += nrow)
	{
		rowstoread = rowsperstrip - (row + img->row_offset) % rowsperstrip;
		nrow = (row + rowstoread > h ? h - row : rowstoread);
		offset_row = row + img->row_offset;
		if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, offset_row, 0),
		    p0, ((row + img->row_offset)%rowsperstrip + nrow) * scanline)==(tmsize_t)(-1)
		    && img->stoponerr)
		{
			ret = 0;
			break;
		}
		if (colorchannels > 1 
                    && TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, offset_row, 1),
                                            p1, ((row + img->row_offset)%rowsperstrip + nrow) * scanline) == (tmsize_t)(-1)
		    && img->stoponerr)
		{
			ret = 0;
			break;
		}
		if (colorchannels > 1 
                    && TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, offset_row, 2),
                                            p2, ((row + img->row_offset)%rowsperstrip + nrow) * scanline) == (tmsize_t)(-1)
		    && img->stoponerr)
		{
			ret = 0;
			break;
		}
		if (alpha)
		{
			if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, offset_row, colorchannels),
			    pa, ((row + img->row_offset)%rowsperstrip + nrow) * scanline)==(tmsize_t)(-1)
			    && img->stoponerr)
			{
				ret = 0;
				break;
			}
		}

		pos = ((row + img->row_offset) % rowsperstrip) * scanline;
		(*put)(img, raster+y*w, 0, y, w, nrow, fromskew, toskew, p0 + pos, p1 + pos,
		    p2 + pos, (alpha?(pa+pos):NULL));
		y += (flip & FLIP_VERTICALLY ? -(int32_t) nrow : (int32_t) nrow);
	}

	if (flip & FLIP_HORIZONTALLY) {
		uint32_t line;

		for (line = 0; line < h; line++) {
			uint64_t *left = raster + (line * w);
			uint64_t *right = left + w - 1;

			while ( left < right ) {
				uint64_t temp = *left;
				*left = *right;
				*right = temp;
				left++, right--;
			}
		}
	}

	_TIFFfree(buf);
	return (ret);
}

/*
 * The following routines move decoded data returned
 * from the TIFF library into rasters filled with packed
 * ABGR pixels (i.e. suitable for passing to lrecwrite.)
 *
 * The routines have been created according to the most
 * important cases and optimized.  PickContigCase and
 * PickSeparateCase analyze the parameters and select
 * the appropriate "get" and "put" routine to use.
 */
#define	REPEAT8(op)	REPEAT4(op); REPEAT4(op)
#define	REPEAT4(op)	REPEAT2(op); REPEAT2(op)
#define	REPEAT2(op)	op; op
#define	CASE8(x,op)			\
    switch (x) {			\
    case 7: op; case 6: op; case 5: op;	\
    case 4: op; case 3: op; case 2: op;	\
    case 1: op;				\
    }
#define	CASE4(x,op)	switch (x) { case 3: op; case 2: op; case 1: op; }
#define	NOP

#define	UNROLL8(w, op1, op2) {		\
    uint32_t _x;				\
    for (_x = w; _x >= 8; _x -= 8) {	\
	op1;				\
	REPEAT8(op2);			\
    }					\
    if (_x > 0) {			\
	op1;				\
	CASE8(_x,op2);			\
    }					\
}
#define	UNROLL4(w, op1, op2) {		\
    uint64_t _x;				\
    for (_x = w; _x >= 4; _x -= 4) {	\
	op1;				\
	REPEAT4(op2);			\
    }					\
    if (_x > 0) {			\
	op1;				\
	CASE4(_x,op2);			\
    }					\
}
#define	UNROLL2(w, op1, op2) {		\
    uint64_t _x;				\
    for (_x = w; _x >= 2; _x -= 2) {	\
	op1;				\
	REPEAT2(op2);			\
    }					\
    if (_x) {				\
	op1;				\
	op2;				\
    }					\
}
    
#define	SKEW(r,g,b,skew)	{ r += skew; g += skew; b += skew; }
#define	SKEW4(r,g,b,a,skew)	{ r += skew; g += skew; b += skew; a+= skew; }

#define A1 (((uint64_t)0xffffL)<<48)
#define	PACK_32(r,g,b)	\
	((uint32_t)(r)|((uint32_t)(g)<<8)|((uint32_t)(b)<<16)|(0xff<<24))
#define	PACK(r,g,b)	\
	((uint64_t)(r)|((uint64_t)(g)<<16)|((uint64_t)(b)<<32)|A1)
#define	PACK4(r,g,b,a)	\
	((uint64_t)(r)|((uint64_t)(g)<<16)|((uint64_t)(b)<<32)|((uint64_t)(a)<<48))
#define W2B(v) (((v)>>16)&0xffff)
/* TODO: PACKW should have be made redundant in favor of Bitdepth16To8 LUT */
#define	PACKW(r,g,b)	\
	((uint64_t)W2B(r)|((uint64_t)W2B(g)<<16)|((uint64_t)W2B(b)<<32)|A1)
#define	PACKW4(r,g,b,a)	\
	((uint64_t)W2B(r)|((uint64_t)W2B(g)<<16)|((uint64_t)W2B(b)<<32)|((uint64_t)W2B(a)<<48))

#define	DECLAREContigPutFunc(name) \
static void name(\
    TIFFRGBAImage* img, \
    uint64_t* cp, \
    uint32_t x, uint32_t y, \
    uint32_t w, uint32_t h, \
    int32_t fromskew, int32_t toskew, \
    unsigned char* pp \
)

/*
 * 8-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(putgreytile)
{
    int samplesperpixel = img->samplesperpixel;
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) y;
    while (h-- > 0) {
	for (x = w; x-- > 0;)
        {
	    *cp++ = BWmap[*pp][0];
            pp += samplesperpixel;
        }
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 8-bit greyscale with associated alpha => colormap/RGBA
 */
DECLAREContigPutFunc(putagreytile)
{
    int samplesperpixel = img->samplesperpixel;
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) y;
    while (h-- > 0) {
	for (x = w; x-- > 0;)
        {
            *cp++ = BWmap[*pp][0] & ((uint64_t)*(pp+1) << 48 | ~A1);
            pp += samplesperpixel;
        }
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 16-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(put16bitbwtile)
{
    int samplesperpixel = img->samplesperpixel;
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) y;
    while (h-- > 0) {
        uint16_t *wp = (uint16_t *) pp;

	for (x = w; x-- > 0;)
        {
            /* use high order byte of 16bit value */

	    *cp++ = BWmap[*wp >> 8][0];
            pp += 2 * samplesperpixel;
            wp += samplesperpixel;
        }
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 1-bit bilevel => colormap/RGB
 */
DECLAREContigPutFunc(put1bitbwtile)
{
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) x; (void) y;
    fromskew /= 8;
    while (h-- > 0) {
	uint64_t* bw;
	UNROLL8(w, bw = BWmap[*pp++], *cp++ = *bw++);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 2-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(put2bitbwtile)
{
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) x; (void) y;
    fromskew /= 4;
    while (h-- > 0) {
	uint64_t* bw;
	UNROLL4(w, bw = BWmap[*pp++], *cp++ = *bw++);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 4-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(put4bitbwtile)
{
    uint64_t** BWmap = (uint64_t**) img->BWmap;

    (void) x; (void) y;
    fromskew /= 2;
    while (h-- > 0) {
	uint64_t* bw;
	UNROLL2(w, bw = BWmap[*pp++], *cp++ = *bw++);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 8-bit packed samples, no Map => RGB
 */
DECLAREContigPutFunc(putRGBcontig8bittile)
{
    int samplesperpixel = img->samplesperpixel;

    (void) x; (void) y;
    fromskew *= samplesperpixel;
    while (h-- > 0) {
	UNROLL8(w, NOP,
	    *cp++ = PACK(pp[0] << 8, pp[1] << 8, pp[2] << 8);
	    pp += samplesperpixel);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 8-bit packed samples => RGBA
 * (known to have Map == NULL)
 */
DECLAREContigPutFunc(putRGBAAcontig8bittile)
{
    int samplesperpixel = img->samplesperpixel;

    (void) x; (void) y;
    fromskew *= samplesperpixel;
    while (h-- > 0) {
	UNROLL8(w, NOP,
	    *cp++ = PACK4(pp[0] << 8, pp[1] << 8, pp[2] << 8, pp[3] << 8);
	    pp += samplesperpixel);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 16-bit packed samples => RGB
 */
DECLAREContigPutFunc(putRGBcontig16bittile)
{
	int samplesperpixel = img->samplesperpixel;
	uint16_t *wp = (uint16_t *)pp;
	(void) y;
	fromskew *= samplesperpixel;
	while (h-- > 0) {
		for (x = w; x-- > 0;) {
			*cp++ = PACK(wp[0], wp[1], wp[2]);
			wp += samplesperpixel;
		}
		cp += toskew;
		wp += fromskew;
	}
}

/*
 * 16-bit packed samples => RGBA w/ associated alpha
 * (known to have Map == NULL)
 */
DECLAREContigPutFunc(putRGBAAcontig16bittile)
{
	int samplesperpixel = img->samplesperpixel;
	uint16_t *wp = (uint16_t *)pp;
	(void) y;
	fromskew *= samplesperpixel;
	while (h-- > 0) {
		for (x = w; x-- > 0;) {
			*cp++ = PACK4(wp[0], wp[1], wp[2], wp[3]);
			wp += samplesperpixel;
		}
		cp += toskew;
		wp += fromskew;
	}
}

/*
 * 8-bit packed CMYK samples w/o Map => RGB
 *
 * NB: The conversion of CMYK->RGB is *very* crude.
 */
DECLAREContigPutFunc(putRGBcontig8bitCMYKtile)
{
    int samplesperpixel = img->samplesperpixel;
    uint16_t r, g, b, k;

    (void) x; (void) y;
    fromskew *= samplesperpixel;
    while (h-- > 0) {
	UNROLL8(w, NOP,
	    k = 255 - pp[3];
	    r = (k*(255-pp[0]));
	    g = (k*(255-pp[1]));
	    b = (k*(255-pp[2]));
	    *cp++ = PACK(r, g, b);
	    pp += samplesperpixel);
	cp += toskew;
	pp += fromskew;
    }
}

/*
 * 8-bit packed CMYK samples w/Map => RGB
 *
 * NB: The conversion of CMYK->RGB is *very* crude.
 */
DECLAREContigPutFunc(putRGBcontig8bitCMYKMaptile)
{
    int samplesperpixel = img->samplesperpixel;
    TIFFRGBValue* Map = img->Map;
    uint16_t r, g, b, k;

    (void) y;
    fromskew *= samplesperpixel;
    while (h-- > 0) {
	for (x = w; x-- > 0;) {
	    k = 255 - pp[3];
	    r = (k*(255-pp[0]))/255;
	    g = (k*(255-pp[1]))/255;
	    b = (k*(255-pp[2]))/255;
	    *cp++ = PACK(Map[r] << 8, Map[g] << 8, Map[b] << 8);
	    pp += samplesperpixel;
	}
	pp += fromskew;
	cp += toskew;
    }
}

#define	DECLARESepPutFunc(name) \
static void name(\
    TIFFRGBAImage* img,\
    uint64_t* cp,\
    uint32_t x, uint32_t y, \
    uint32_t w, uint32_t h,\
    int32_t fromskew, int32_t toskew,\
    unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a\
)

/*
 * 8-bit unpacked samples => RGB
 */
DECLARESepPutFunc(putRGBseparate8bittile)
{
    (void) img; (void) x; (void) y; (void) a;
    while (h-- > 0) {
	UNROLL8(w, NOP, *cp++ = PACK(*r++ << 8, *g++ << 8, *b++ << 8));
	SKEW(r, g, b, fromskew);
	cp += toskew;
    }
}

/*
 * 8-bit unpacked samples => RGBA w/ associated alpha
 */
DECLARESepPutFunc(putRGBAAseparate8bittile)
{
	(void) img; (void) x; (void) y; 
	while (h-- > 0) {
		UNROLL8(w, NOP, *cp++ = PACK4(*r++ << 8, *g++ << 8, *b++ << 8, *a++ << 8));
		SKEW4(r, g, b, a, fromskew);
		cp += toskew;
	}
}

/*
 * 8-bit unpacked CMYK samples => RGBA
 */
DECLARESepPutFunc(putCMYKseparate8bittile)
{
	(void) img; (void) y;
	while (h-- > 0) {
		uint32_t rv, gv, bv, kv;
		for (x = w; x-- > 0;) {
			kv = 255 - *a++;
			rv = (kv*(255-*r++));
			gv = (kv*(255-*g++));
			bv = (kv*(255-*b++));
			*cp++ = PACK4(rv,gv,bv,65535);
		}
		SKEW4(r, g, b, a, fromskew);
		cp += toskew;
	}
}

/*
 * 16-bit unpacked samples => RGB
 */
DECLARESepPutFunc(putRGBseparate16bittile)
{
	uint16_t *wr = (uint16_t*) r;
	uint16_t *wg = (uint16_t*) g;
	uint16_t *wb = (uint16_t*) b;
	(void) img; (void) y; (void) a;
	while (h-- > 0) {
		for (x = 0; x < w; x++)
			*cp++ = PACK(*wr++, *wg++, *wb++);
		SKEW(wr, wg, wb, fromskew);
		cp += toskew;
	}
}

/*
 * 16-bit unpacked samples => RGBA w/ associated alpha
 */
DECLARESepPutFunc(putRGBAAseparate16bittile)
{
	uint16_t *wr = (uint16_t*) r;
	uint16_t *wg = (uint16_t*) g;
	uint16_t *wb = (uint16_t*) b;
	uint16_t *wa = (uint16_t*) a;
	(void) img; (void) y;
	while (h-- > 0) {
		for (x = 0; x < w; x++)
			*cp++ = PACK4(*wr++, *wg++, *wb++, *wa++);
		SKEW4(wr, wg, wb, wa, fromskew);
		cp += toskew;
	}
}

/*
 * 8-bit packed CIE L*a*b 1976 samples => RGB
 */
DECLAREContigPutFunc(putcontig8bitCIELab)
{
	float X, Y, Z;
	uint32_t r, g, b;
	(void) y;
	fromskew *= 3;
	while (h-- > 0) {
		for (x = w; x-- > 0;) {
			TIFFCIELabToXYZ(img->cielab,
					(unsigned char)pp[0],
					(signed char)pp[1],
					(signed char)pp[2],
					&X, &Y, &Z);
			TIFFXYZToRGB(img->cielab, X, Y, Z, &r, &g, &b);
			*cp++ = PACK(r << 8, g << 8, b << 8);
			pp += 3;
		}
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * YCbCr -> RGB conversion and packing routines.
 */

#define	YCbCrtoRGB(dst, Y) {						\
	uint32_t r, g, b;							\
	TIFFYCbCrtoRGB(img->ycbcr, (Y), Cb, Cr, &r, &g, &b);		\
	dst = PACK(((uint64_t)r) << 8, ((uint64_t)g) << 8, ((uint64_t)b) << 8);						\
}

/*
 * 8-bit packed YCbCr samples w/ 4,4 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr44tile)
{
    uint64_t* cp1 = cp+w+toskew;
    uint64_t* cp2 = cp1+w+toskew;
    uint64_t* cp3 = cp2+w+toskew;
    int32_t incr = 3*w+4*toskew;

    (void) y;
    /* adjust fromskew */
    fromskew = (fromskew * 18) / 4;
    if ((h & 3) == 0 && (w & 3) == 0) {				        
        for (; h >= 4; h -= 4) {
            x = w>>2;
            do {
                int32_t Cb = pp[16];
                int32_t Cr = pp[17];

                YCbCrtoRGB(cp [0], pp[ 0]);
                YCbCrtoRGB(cp [1], pp[ 1]);
                YCbCrtoRGB(cp [2], pp[ 2]);
                YCbCrtoRGB(cp [3], pp[ 3]);
                YCbCrtoRGB(cp1[0], pp[ 4]);
                YCbCrtoRGB(cp1[1], pp[ 5]);
                YCbCrtoRGB(cp1[2], pp[ 6]);
                YCbCrtoRGB(cp1[3], pp[ 7]);
                YCbCrtoRGB(cp2[0], pp[ 8]);
                YCbCrtoRGB(cp2[1], pp[ 9]);
                YCbCrtoRGB(cp2[2], pp[10]);
                YCbCrtoRGB(cp2[3], pp[11]);
                YCbCrtoRGB(cp3[0], pp[12]);
                YCbCrtoRGB(cp3[1], pp[13]);
                YCbCrtoRGB(cp3[2], pp[14]);
                YCbCrtoRGB(cp3[3], pp[15]);

                cp += 4, cp1 += 4, cp2 += 4, cp3 += 4;
                pp += 18;
            } while (--x);
            cp += incr, cp1 += incr, cp2 += incr, cp3 += incr;
            pp += fromskew;
        }
    } else {
        while (h > 0) {
            for (x = w; x > 0;) {
                int32_t Cb = pp[16];
                int32_t Cr = pp[17];
                switch (x) {
                default:
                    switch (h) {
                    default: YCbCrtoRGB(cp3[3], pp[15]); /* FALLTHROUGH */
                    case 3:  YCbCrtoRGB(cp2[3], pp[11]); /* FALLTHROUGH */
                    case 2:  YCbCrtoRGB(cp1[3], pp[ 7]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [3], pp[ 3]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 3:
                    switch (h) {
                    default: YCbCrtoRGB(cp3[2], pp[14]); /* FALLTHROUGH */
                    case 3:  YCbCrtoRGB(cp2[2], pp[10]); /* FALLTHROUGH */
                    case 2:  YCbCrtoRGB(cp1[2], pp[ 6]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [2], pp[ 2]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 2:
                    switch (h) {
                    default: YCbCrtoRGB(cp3[1], pp[13]); /* FALLTHROUGH */
                    case 3:  YCbCrtoRGB(cp2[1], pp[ 9]); /* FALLTHROUGH */
                    case 2:  YCbCrtoRGB(cp1[1], pp[ 5]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [1], pp[ 1]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 1:
                    switch (h) {
                    default: YCbCrtoRGB(cp3[0], pp[12]); /* FALLTHROUGH */
                    case 3:  YCbCrtoRGB(cp2[0], pp[ 8]); /* FALLTHROUGH */
                    case 2:  YCbCrtoRGB(cp1[0], pp[ 4]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [0], pp[ 0]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                }
                if (x < 4) {
                    cp += x; cp1 += x; cp2 += x; cp3 += x;
                    x = 0;
                }
                else {
                    cp += 4; cp1 += 4; cp2 += 4; cp3 += 4;
                    x -= 4;
                }
                pp += 18;
            }
            if (h <= 4)
                break;
            h -= 4;
            cp += incr, cp1 += incr, cp2 += incr, cp3 += incr;
            pp += fromskew;
        }
    }
}

/*
 * 8-bit packed YCbCr samples w/ 4,2 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr42tile)
{
    uint64_t* cp1 = cp+w+toskew;
    int32_t incr = 2*toskew+w;

    (void) y;
    fromskew = (fromskew * 10) / 4;
    if ((h & 3) == 0 && (w & 1) == 0) {
        for (; h >= 2; h -= 2) {
            x = w>>2;
            do {
                int32_t Cb = pp[8];
                int32_t Cr = pp[9];
                
                YCbCrtoRGB(cp [0], pp[0]);
                YCbCrtoRGB(cp [1], pp[1]);
                YCbCrtoRGB(cp [2], pp[2]);
                YCbCrtoRGB(cp [3], pp[3]);
                YCbCrtoRGB(cp1[0], pp[4]);
                YCbCrtoRGB(cp1[1], pp[5]);
                YCbCrtoRGB(cp1[2], pp[6]);
                YCbCrtoRGB(cp1[3], pp[7]);
                
                cp += 4, cp1 += 4;
                pp += 10;
            } while (--x);
            cp += incr, cp1 += incr;
            pp += fromskew;
        }
    } else {
        while (h > 0) {
            for (x = w; x > 0;) {
                int32_t Cb = pp[8];
                int32_t Cr = pp[9];
                switch (x) {
                default:
                    switch (h) {
                    default: YCbCrtoRGB(cp1[3], pp[ 7]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [3], pp[ 3]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 3:
                    switch (h) {
                    default: YCbCrtoRGB(cp1[2], pp[ 6]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [2], pp[ 2]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 2:
                    switch (h) {
                    default: YCbCrtoRGB(cp1[1], pp[ 5]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [1], pp[ 1]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                case 1:
                    switch (h) {
                    default: YCbCrtoRGB(cp1[0], pp[ 4]); /* FALLTHROUGH */
                    case 1:  YCbCrtoRGB(cp [0], pp[ 0]); /* FALLTHROUGH */
                    }                                    /* FALLTHROUGH */
                }
                if (x < 4) {
                    cp += x; cp1 += x;
                    x = 0;
                }
                else {
                    cp += 4; cp1 += 4;
                    x -= 4;
                }
                pp += 10;
            }
            if (h <= 2)
                break;
            h -= 2;
            cp += incr, cp1 += incr;
            pp += fromskew;
        }
    }
}

/*
 * 8-bit packed YCbCr samples w/ 4,1 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr41tile)
{
    (void) y;
    /* XXX adjust fromskew */
    do {
	x = w>>2;
	do {
	    int32_t Cb = pp[4];
	    int32_t Cr = pp[5];

	    YCbCrtoRGB(cp [0], pp[0]);
	    YCbCrtoRGB(cp [1], pp[1]);
	    YCbCrtoRGB(cp [2], pp[2]);
	    YCbCrtoRGB(cp [3], pp[3]);

	    cp += 4;
	    pp += 6;
	} while (--x);

        if( (w&3) != 0 )
        {
	    int32_t Cb = pp[4];
	    int32_t Cr = pp[5];

            switch( (w&3) ) {
              case 3: YCbCrtoRGB(cp [2], pp[2]);
              case 2: YCbCrtoRGB(cp [1], pp[1]);
              case 1: YCbCrtoRGB(cp [0], pp[0]);
              case 0: break;
            }

            cp += (w&3);
            pp += 6;
        }

	cp += toskew;
	pp += fromskew;
    } while (--h);

}

/*
 * 8-bit packed YCbCr samples w/ 2,2 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr22tile)
{
	uint64_t* cp2;
	int32_t incr = 2*toskew+w;
	(void) y;
	fromskew = (fromskew / 2) * 6;
	cp2 = cp+w+toskew;
	while (h>=2) {
		x = w;
		while (x>=2) {
			uint32_t Cb = pp[4];
			uint32_t Cr = pp[5];
			YCbCrtoRGB(cp[0], pp[0]);
			YCbCrtoRGB(cp[1], pp[1]);
			YCbCrtoRGB(cp2[0], pp[2]);
			YCbCrtoRGB(cp2[1], pp[3]);
			cp += 2;
			cp2 += 2;
			pp += 6;
			x -= 2;
		}
		if (x==1) {
			uint32_t Cb = pp[4];
			uint32_t Cr = pp[5];
			YCbCrtoRGB(cp[0], pp[0]);
			YCbCrtoRGB(cp2[0], pp[2]);
			cp ++ ;
			cp2 ++ ;
			pp += 6;
		}
		cp += incr;
		cp2 += incr;
		pp += fromskew;
		h-=2;
	}
	if (h==1) {
		x = w;
		while (x>=2) {
			uint32_t Cb = pp[4];
			uint32_t Cr = pp[5];
			YCbCrtoRGB(cp[0], pp[0]);
			YCbCrtoRGB(cp[1], pp[1]);
			cp += 2;
			cp2 += 2;
			pp += 6;
			x -= 2;
		}
		if (x==1) {
			uint32_t Cb = pp[4];
			uint32_t Cr = pp[5];
			YCbCrtoRGB(cp[0], pp[0]);
		}
	}
}

/*
 * 8-bit packed YCbCr samples w/ 2,1 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr21tile)
{
	(void) y;
	fromskew = (fromskew * 4) / 2;
	do {
		x = w>>1;
		do {
			int32_t Cb = pp[2];
			int32_t Cr = pp[3];

			YCbCrtoRGB(cp[0], pp[0]);
			YCbCrtoRGB(cp[1], pp[1]);

			cp += 2;
			pp += 4;
		} while (--x);

		if( (w&1) != 0 )
		{
			int32_t Cb = pp[2];
			int32_t Cr = pp[3];

			YCbCrtoRGB(cp[0], pp[0]);

			cp += 1;
			pp += 4;
		}

		cp += toskew;
		pp += fromskew;
	} while (--h);
}

/*
 * 8-bit packed YCbCr samples w/ 1,2 subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr12tile)
{
	uint64_t* cp2;
	int32_t incr = 2*toskew+w;
	(void) y;
	fromskew = (fromskew / 2) * 4;
	cp2 = cp+w+toskew;
	while (h>=2) {
		x = w;
		do {
			uint32_t Cb = pp[2];
			uint32_t Cr = pp[3];
			YCbCrtoRGB(cp[0], pp[0]);
			YCbCrtoRGB(cp2[0], pp[1]);
			cp ++;
			cp2 ++;
			pp += 4;
		} while (--x);
		cp += incr;
		cp2 += incr;
		pp += fromskew;
		h-=2;
	}
	if (h==1) {
		x = w;
		do {
			uint32_t Cb = pp[2];
			uint32_t Cr = pp[3];
			YCbCrtoRGB(cp[0], pp[0]);
			cp ++;
			pp += 4;
		} while (--x);
	}
}

/*
 * 8-bit packed YCbCr samples w/ no subsampling => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCr11tile)
{
	(void) y;
	fromskew *= 3;
	do {
		x = w; /* was x = w>>1; patched 2000/09/25 warmerda@home.com */
		do {
			int32_t Cb = pp[1];
			int32_t Cr = pp[2];

			YCbCrtoRGB(*cp++, pp[0]);

			pp += 3;
		} while (--x);
		cp += toskew;
		pp += fromskew;
	} while (--h);
}

/*
 * 8-bit packed YCbCr samples w/ no subsampling => RGB
 */
DECLARESepPutFunc(putseparate8bitYCbCr11tile)
{
	(void) y;
	(void) a;
	/* TODO: naming of input vars is still off, change obfuscating declaration inside define, or resolve obfuscation */
	while (h-- > 0) {
		x = w;
		do {
			uint32_t dr, dg, db;
			TIFFYCbCrtoRGB(img->ycbcr,*r++,*g++,*b++,&dr,&dg,&db);
			*cp++ = PACK(dr,dg,db);
		} while (--x);
		SKEW(r, g, b, fromskew);
		cp += toskew;
	}
}
#undef YCbCrtoRGB

static int
initYCbCrConversion(TIFFRGBAImage* img)
{
	static const char module[] = "initYCbCrConversion";

	float *luma, *refBlackWhite;

	if (img->ycbcr == NULL) {
		img->ycbcr = (TIFFYCbCrToRGB*) _TIFFmalloc(
		    TIFFroundup_32(sizeof (TIFFYCbCrToRGB), sizeof (long))  
		    + 4*256*sizeof (TIFFRGBValue)
		    + 2*256*sizeof (int)
		    + 3*256*sizeof (int32_t)
		    );
		if (img->ycbcr == NULL) {
			TIFFErrorExt(img->tif->tif_clientdata, module,
			    "No space for YCbCr->RGB conversion state");
			return (0);
		}
	}

	TIFFGetFieldDefaulted(img->tif, TIFFTAG_YCBCRCOEFFICIENTS, &luma);
	TIFFGetFieldDefaulted(img->tif, TIFFTAG_REFERENCEBLACKWHITE,
	    &refBlackWhite);
	if (TIFFYCbCrToRGBInit(img->ycbcr, luma, refBlackWhite) < 0)
		return(0);
	return (1);
}

static tileContigRoutine_64
initCIELabConversion(TIFFRGBAImage* img)
{
	static const char module[] = "initCIELabConversion";

	float   *whitePoint;
	float   refWhite[3];

	if (!img->cielab) {
		img->cielab = (TIFFCIELabToRGB *)
			_TIFFmalloc(sizeof(TIFFCIELabToRGB));
		if (!img->cielab) {
			TIFFErrorExt(img->tif->tif_clientdata, module,
			    "No space for CIE L*a*b*->RGB conversion state.");
			return NULL;
		}
	}

	TIFFGetFieldDefaulted(img->tif, TIFFTAG_WHITEPOINT, &whitePoint);
	refWhite[1] = 100.0F;
	refWhite[0] = whitePoint[0] / whitePoint[1] * refWhite[1];
	refWhite[2] = (1.0F - whitePoint[0] - whitePoint[1])
		      / whitePoint[1] * refWhite[1];
	if (TIFFCIELabToRGBInit(img->cielab, &display_sRGB, refWhite) < 0) {
		TIFFErrorExt(img->tif->tif_clientdata, module,
		    "Failed to initialize CIE L*a*b*->RGB conversion state.");
		_TIFFfree(img->cielab);
		return NULL;
	}

	return putcontig8bitCIELab;
}

/*
 * Greyscale images with less than 8 bits/sample are handled
 * with a table to avoid lots of shifts and masks.  The table
 * is setup so that put*bwtile (below) can retrieve 8/bitspersample
 * pixel values simply by indexing into the table with one
 * number.
 */
static int
makebwmap(TIFFRGBAImage* img)
{
    TIFFRGBValue* Map = img->Map;
    int bitspersample = img->bitspersample;
    int nsamples = 8 / bitspersample;
    int i;
    uint64_t* p;

    if( nsamples == 0 )
        nsamples = 1;

    img->BWmap = (uint32_t**) _TIFFmalloc(
	256*sizeof (uint32_t *)+(256*nsamples*sizeof(uint64_t)));
    if (img->BWmap == NULL) {
		TIFFErrorExt(img->tif->tif_clientdata, TIFFFileName(img->tif), "No space for B&W mapping table");
		return (0);
    }
    p = (uint64_t*)(img->BWmap + 256);
    for (i = 0; i < 256; i++) {
	TIFFRGBValue c;
	img->BWmap[i] = (uint32_t*) p;
	switch (bitspersample) {
#define	GREY(x)	c = Map[x]; *p++ = PACK(c << 8,c << 8,c << 8);
	case 1:
	    GREY(i>>7);
	    GREY((i>>6)&1);
	    GREY((i>>5)&1);
	    GREY((i>>4)&1);
	    GREY((i>>3)&1);
	    GREY((i>>2)&1);
	    GREY((i>>1)&1);
	    GREY(i&1);
	    break;
	case 2:
	    GREY(i>>6);
	    GREY((i>>4)&3);
	    GREY((i>>2)&3);
	    GREY(i&3);
	    break;
	case 4:
	    GREY(i>>4);
	    GREY(i&0xf);
	    break;
	case 8:
        case 16:
	    GREY(i);
	    break;
	}
#undef	GREY
    }
    return (1);
}

/*
 * Construct a mapping table to convert from the range
 * of the data samples to [0,255] --for display.  This
 * process also handles inverting B&W images when needed.
 */ 
static int
setupMap(TIFFRGBAImage* img)
{
    int32_t x, range;

    range = (int32_t)((1L<<img->bitspersample)-1);
    
    /* treat 16 bit the same as eight bit */
    if( img->bitspersample == 16 )
        range = (int32_t) 255;

    img->Map = (TIFFRGBValue*) _TIFFmalloc((range+1) * sizeof (TIFFRGBValue));
    if (img->Map == NULL) {
		TIFFErrorExt(img->tif->tif_clientdata, TIFFFileName(img->tif),
			"No space for photometric conversion table");
		return (0);
    }
    if (img->photometric == PHOTOMETRIC_MINISWHITE) {
	for (x = 0; x <= range; x++)
	    img->Map[x] = (TIFFRGBValue) (((range - x) * 255) / range);
    } else {
	for (x = 0; x <= range; x++)
	    img->Map[x] = (TIFFRGBValue) ((x * 255) / range);
    }
    if (img->bitspersample <= 16 &&
	(img->photometric == PHOTOMETRIC_MINISBLACK ||
	 img->photometric == PHOTOMETRIC_MINISWHITE)) {
	/*
	 * Use photometric mapping table to construct
	 * unpacking tables for samples <= 8 bits.
	 */
	if (!makebwmap(img))
	    return (0);
	/* no longer need Map, free it */
	_TIFFfree(img->Map), img->Map = NULL;
    }
    return (1);
}

static int
checkcmap(TIFFRGBAImage* img)
{
    uint16_t* r = img->redcmap;
    uint16_t* g = img->greencmap;
    uint16_t* b = img->bluecmap;
    long n = 1L<<img->bitspersample;

    while (n-- > 0)
	if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
	    return (16);
    return (8);
}

static void
cvtcmap(TIFFRGBAImage* img)
{
    uint16_t* r = img->redcmap;
    uint16_t* g = img->greencmap;
    uint16_t* b = img->bluecmap;
    long i;

    for (i = (1L<<img->bitspersample)-1; i >= 0; i--) {
#define	CVT(x)		((uint16_t)((x)>>8))
	r[i] = CVT(r[i]);
	g[i] = CVT(g[i]);
	b[i] = CVT(b[i]);
#undef	CVT
    }
}

/*
 * Palette images with <= 8 bits/sample are handled
 * with a table to avoid lots of shifts and masks.  The table
 * is setup so that put*cmaptile (below) can retrieve 8/bitspersample
 * pixel values simply by indexing into the table with one
 * number.
 */
static int
makecmap(TIFFRGBAImage* img)
{
    int bitspersample = img->bitspersample;
    int nsamples = 8 / bitspersample;
    uint16_t* r = img->redcmap;
    uint16_t* g = img->greencmap;
    uint16_t* b = img->bluecmap;
    uint32_t *p;
    int i;

    img->PALmap = (uint32_t**) _TIFFmalloc(
	256*sizeof (uint32_t *)+(256*nsamples*sizeof(uint32_t)));
    if (img->PALmap == NULL) {
		TIFFErrorExt(img->tif->tif_clientdata, TIFFFileName(img->tif), "No space for Palette mapping table");
		return (0);
	}
    p = (uint32_t*)(img->PALmap + 256);
    for (i = 0; i < 256; i++) {
	TIFFRGBValue c;
	img->PALmap[i] = p;
#define	CMAP(x)	c = (TIFFRGBValue) x; *p++ = PACK_32(r[c]&0xff, g[c]&0xff, b[c]&0xff);
	switch (bitspersample) {
	case 1:
	    CMAP(i>>7);
	    CMAP((i>>6)&1);
	    CMAP((i>>5)&1);
	    CMAP((i>>4)&1);
	    CMAP((i>>3)&1);
	    CMAP((i>>2)&1);
	    CMAP((i>>1)&1);
	    CMAP(i&1);
	    break;
	case 2:
	    CMAP(i>>6);
	    CMAP((i>>4)&3);
	    CMAP((i>>2)&3);
	    CMAP(i&3);
	    break;
	case 4:
	    CMAP(i>>4);
	    CMAP(i&0xf);
	    break;
	case 8:
	    CMAP(i);
	    break;
	}
#undef CMAP
    }
    return (1);
}

/* 
 * Construct any mapping table used
 * by the associated put routine.
 */
static int
buildMap(TIFFRGBAImage* img)
{
    switch (img->photometric) {
    case PHOTOMETRIC_RGB:
    case PHOTOMETRIC_YCBCR:
    case PHOTOMETRIC_SEPARATED:
	if (img->bitspersample == 8)
	    break;
	/* fall thru... */
    case PHOTOMETRIC_MINISBLACK:
    case PHOTOMETRIC_MINISWHITE:
	if (!setupMap(img))
	    return (0);
	break;
    case PHOTOMETRIC_PALETTE:
	/*
	 * Convert 16-bit colormap to 8-bit (unless it looks
	 * like an old-style 8-bit colormap).
	 */
	if (checkcmap(img) == 16)
	    cvtcmap(img);
	else
	    TIFFWarningExt(img->tif->tif_clientdata, TIFFFileName(img->tif), "Assuming 8-bit colormap");
	/*
	 * Use mapping table and colormap to construct
	 * unpacking tables for samples < 8 bits.
	 */
	if (img->bitspersample <= 8 && !makecmap(img))
	    return (0);
	break;
    }
    return (1);
}

/*
 * Select the appropriate conversion routine for packed data.
 */
static int
PickContigCase(TIFFRGBAImage* img)
{
   img->get = (gtFunc_32) (TIFFIsTiled(img->tif) ? gtTileContig : gtStripContig);
	img->put.contig = NULL;
	switch (img->photometric) {
		case PHOTOMETRIC_RGB:
			switch (img->bitspersample) {
				case 8:
               if (img->alpha != EXTRASAMPLE_UNSPECIFIED)
						img->put.contig = (tileContigRoutine) putRGBAAcontig8bittile;
					else
						img->put.contig = (tileContigRoutine) putRGBcontig8bittile;
					break;
				case 16:
					if (img->alpha != EXTRASAMPLE_UNSPECIFIED)
						img->put.contig = (tileContigRoutine) putRGBAAcontig16bittile;
					else
					{
						if (BuildMapBitdepth16To8(img))
							img->put.contig = (tileContigRoutine) putRGBcontig16bittile;
					}
					break;
			}
			break;
		case PHOTOMETRIC_SEPARATED:
			if (buildMap(img)) {
				if (img->bitspersample == 8) {
					if (!img->Map)
						img->put.contig = (tileContigRoutine) putRGBcontig8bitCMYKtile;
					else
						img->put.contig = (tileContigRoutine) putRGBcontig8bitCMYKMaptile;
				}
			}
			break;
		case PHOTOMETRIC_PALETTE:
		case PHOTOMETRIC_MINISWHITE:
		case PHOTOMETRIC_MINISBLACK:
			if (buildMap(img)) {
				switch (img->bitspersample) {
					case 16:
						img->put.contig = (tileContigRoutine) put16bitbwtile;
						break;
					case 8:
						if (img->alpha && img->samplesperpixel == 2)
							img->put.contig = (tileContigRoutine) putagreytile;
						else
							img->put.contig = (tileContigRoutine) putgreytile;
						break;
					case 4:
						img->put.contig = (tileContigRoutine) put4bitbwtile;
						break;
					case 2:
						img->put.contig = (tileContigRoutine) put2bitbwtile;
						break;
					case 1:
						img->put.contig = (tileContigRoutine) put1bitbwtile;
						break;
				}
			}
			break;
		case PHOTOMETRIC_YCBCR:
			if ((img->bitspersample==8) && (img->samplesperpixel==3))
			{
				if (initYCbCrConversion(img)!=0)
				{
					/*
					 * The 6.0 spec says that subsampling must be
					 * one of 1, 2, or 4, and that vertical subsampling
					 * must always be <= horizontal subsampling; so
					 * there are only a few possibilities and we just
					 * enumerate the cases.
					 * Joris: added support for the [1,2] case, nonetheless, to accomodate
					 * some OJPEG files
					 */
					uint16_t SubsamplingHor;
					uint16_t SubsamplingVer;
					TIFFGetFieldDefaulted(img->tif, TIFFTAG_YCBCRSUBSAMPLING, &SubsamplingHor, &SubsamplingVer);
					switch ((SubsamplingHor<<4)|SubsamplingVer) {
						case 0x44:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr44tile;
							break;
						case 0x42:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr42tile;
							break;
						case 0x41:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr41tile;
							break;
						case 0x22:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr22tile;
							break;
						case 0x21:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr21tile;
							break;
						case 0x12:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr12tile;
							break;
						case 0x11:
							img->put.contig = (tileContigRoutine) putcontig8bitYCbCr11tile;
							break;
					}
				}
			}
			break;
		case PHOTOMETRIC_CIELAB:
			if (buildMap(img)) {
				if (img->bitspersample == 8)
					img->put.contig = (tileContigRoutine) initCIELabConversion(img);
				break;
			}
	}
	return ((img->get!=NULL) && (img->put.contig!=NULL));
}

/*
 * Select the appropriate conversion routine for unpacked data.
 *
 * NB: we assume that unpacked single channel data is directed
 *	 to the "packed routines.
 */
static int
PickSeparateCase(TIFFRGBAImage* img)
{
	img->get = (gtFunc_32) (TIFFIsTiled(img->tif) ? gtTileSeparate : gtStripSeparate);
	img->put.separate = NULL;
	switch (img->photometric) {
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
		/* greyscale images processed pretty much as RGB by gtTileSeparate */
	case PHOTOMETRIC_RGB:
		switch (img->bitspersample) {
		case 8:
			if (img->alpha != EXTRASAMPLE_UNSPECIFIED)
				img->put.separate = (tileSeparateRoutine) putRGBAAseparate8bittile;
			else
				img->put.separate = (tileSeparateRoutine) putRGBseparate8bittile;
			break;
		case 16:
			if (img->alpha != EXTRASAMPLE_UNSPECIFIED)
			{
				if (BuildMapBitdepth16To8(img))
					img->put.separate = (tileSeparateRoutine) putRGBAAseparate16bittile;
			}
			else
			{
				if (BuildMapBitdepth16To8(img))
					img->put.separate = (tileSeparateRoutine) putRGBseparate16bittile;
			}
			break;
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		if (img->bitspersample == 8 && img->samplesperpixel == 4)
		{
			img->alpha = 1; // Not alpha, but seems like the only way to get 4th band
			img->put.separate = (tileSeparateRoutine) putCMYKseparate8bittile;
		}
		break;
	case PHOTOMETRIC_YCBCR:
		if ((img->bitspersample==8) && (img->samplesperpixel==3))
		{
			if (initYCbCrConversion(img)!=0)
			{
				uint16_t hs, vs;
				TIFFGetFieldDefaulted(img->tif, TIFFTAG_YCBCRSUBSAMPLING, &hs, &vs);
				switch ((hs<<4)|vs) {
				case 0x11:
					img->put.separate = (tileSeparateRoutine) putseparate8bitYCbCr11tile;
					break;
					/* TODO: add other cases here */
				}
			}
		}
		break;
	}
	return ((img->get!=NULL) && (img->put.separate!=NULL));
}

static int
BuildMapBitdepth16To8(TIFFRGBAImage* img)
{
	static const char module[]="BuildMapBitdepth16To8";
	uint8_t* m;
	uint32_t n;
	assert(img->Bitdepth16To8==NULL);
	img->Bitdepth16To8=_TIFFmalloc(65536);
	if (img->Bitdepth16To8==NULL)
	{
		TIFFErrorExt(img->tif->tif_clientdata,module,"Out of memory");
		return(0);
	}
	m=img->Bitdepth16To8;
	for (n=0; n<65536; n++)
		*m++=(n+128)/257;
	return(1);
}


/*
 * Read a whole strip off data from the file, and convert to RGBA form.
 * If this is the last strip, then it will only contain the portion of
 * the strip that is actually within the image space.  The result is
 * organized in bottom to top form.
 */


int
TIFFReadRGBAStrip_64(TIFF* tif, uint32_t row, uint64_t * raster )

{
    char 	emsg[1024] = "";
    TIFFRGBAImage img;
    int 	ok;
    uint32_t	rowsperstrip, rows_to_read;

    if( TIFFIsTiled( tif ) )
    {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif),
                  "Can't use TIFFReadRGBAStrip() with tiled file.");
	return (0);
    }
    
    TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
    if( (row % rowsperstrip) != 0 )
    {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif),
				"Row passed to TIFFReadRGBAStrip() must be first in a strip.");
		return (0);
    }

    if (TIFFRGBAImageOK(tif, emsg) && TIFFRGBAImageBegin_64(&img, tif, 0, emsg)) {

        img.row_offset = row;
        img.col_offset = 0;

        if( row + rowsperstrip > img.height )
            rows_to_read = img.height - row;
        else
            rows_to_read = rowsperstrip;
        
	ok = TIFFRGBAImageGet_64(&img, raster, img.width, rows_to_read );
        
	TIFFRGBAImageEnd(&img);
    } else {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "%s", emsg);
		ok = 0;
    }
    
    return (ok);
}

/*
 * Read a whole tile off data from the file, and convert to RGBA form.
 * The returned RGBA data is organized from bottom to top of tile,
 * and may include zeroed areas if the tile extends off the image.
 */

int
TIFFReadRGBATile_64(TIFF* tif, uint32_t col, uint32_t row, uint64_t * raster)

{
    char 	emsg[1024] = "";
    TIFFRGBAImage img;
    int 	ok;
    uint32_t	tile_xsize, tile_ysize;
    uint32_t	read_xsize, read_ysize;
    uint32_t	i_row;

    /*
     * Verify that our request is legal - on a tile file, and on a
     * tile boundary.
     */
    
    if( !TIFFIsTiled( tif ) )
    {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif),
				  "Can't use TIFFReadRGBATile() with stripped file.");
		return (0);
    }
    
    TIFFGetFieldDefaulted(tif, TIFFTAG_TILEWIDTH, &tile_xsize);
    TIFFGetFieldDefaulted(tif, TIFFTAG_TILELENGTH, &tile_ysize);
    if( (col % tile_xsize) != 0 || (row % tile_ysize) != 0 )
    {
		TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif),
                  "Row/col passed to TIFFReadRGBATile() must be top"
                  "left corner of a tile.");
	return (0);
    }

    /*
     * Setup the RGBA reader.
     */
    
    if (!TIFFRGBAImageOK(tif, emsg) 
	|| !TIFFRGBAImageBegin_64(&img, tif, 0, emsg)) {
	    TIFFErrorExt(tif->tif_clientdata, TIFFFileName(tif), "%s", emsg);
	    return( 0 );
    }

    /*
     * The TIFFRGBAImageGet() function doesn't allow us to get off the
     * edge of the image, even to fill an otherwise valid tile.  So we
     * figure out how much we can read, and fix up the tile buffer to
     * a full tile configuration afterwards.
     */

    if( row + tile_ysize > img.height )
        read_ysize = img.height - row;
    else
        read_ysize = tile_ysize;
    
    if( col + tile_xsize > img.width )
        read_xsize = img.width - col;
    else
        read_xsize = tile_xsize;

    /*
     * Read the chunk of imagery.
     */
    
    img.row_offset = row;
    img.col_offset = col;

    ok = TIFFRGBAImageGet_64(&img, raster, read_xsize, read_ysize );
        
    TIFFRGBAImageEnd(&img);

    /*
     * If our read was incomplete we will need to fix up the tile by
     * shifting the data around as if a full tile of data is being returned.
     *
     * This is all the more complicated because the image is organized in
     * bottom to top format. 
     */

    if( read_xsize == tile_xsize && read_ysize == tile_ysize )
        return( ok );

    for( i_row = 0; i_row < read_ysize; i_row++ ) {
        memmove( raster + (tile_ysize - i_row - 1) * tile_xsize,
                 raster + (read_ysize - i_row - 1) * read_xsize,
                 read_xsize * sizeof(uint64_t) );
        _TIFFmemset( raster + (tile_ysize - i_row - 1) * tile_xsize+read_xsize,
                     0, sizeof(uint64_t) * (tile_xsize - read_xsize) );
    }

    for( i_row = read_ysize; i_row < tile_ysize; i_row++ ) {
        _TIFFmemset( raster + (tile_ysize - i_row - 1) * tile_xsize,
                     0, sizeof(uint64_t) * tile_xsize );
    }

    return (ok);
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
