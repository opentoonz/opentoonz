/*
 * Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
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
 * The objective of this test suite is to test the JPEGRawDecode() 
 * interface via TIFReadEncodedTile().  This function with YCbCr subsampling
 * is a frequent source of bugs. 
 */

#include "tif_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H 
# include <unistd.h> 
#endif 

#include "tiffio.h"

/*
  Libjpeg's jmorecfg.h defines INT16 and INT32, but only if XMD_H is
  not defined.  Unfortunately, the MinGW and Borland compilers include
  a typedef for INT32, which causes a conflict.  MSVC does not include
  a conficting typedef given the headers which are included.
*/
#if defined(__BORLANDC__) || defined(__MINGW32__)
# define XMD_H 1
#endif

/*
   The windows RPCNDR.H file defines boolean, but defines it with the
   unsigned char size.  You should compile JPEG library using appropriate
   definitions in jconfig.h header, but many users compile library in wrong
   way. That causes errors of the following type:

   "JPEGLib: JPEG parameter struct mismatch: library thinks size is 432,
   caller expects 464"

   For such users we will fix the problem here. See install.doc file from
   the JPEG library distribution for details.
*/

/* Define "boolean" as unsigned char, not int, per Windows custom. */
#if defined(__WIN32__) && !defined(__MINGW32__)
# ifndef __RPCNDR_H__            /* don't conflict if rpcndr.h already read */
   typedef unsigned char boolean;
# endif
# define HAVE_BOOLEAN            /* prevent jmorecfg.h from redefining it */
#endif
#include "jpeglib.h" /* Needed for JPEG_LIB_VERSION */

static unsigned char cluster_0[] = { 0, 0, 2, 0, 138, 139 };
static unsigned char cluster_64[] = { 0, 0, 9, 6, 134, 119 };
static unsigned char cluster_128[] = { 44, 40, 63, 59, 230, 95 };

static int check_cluster( int cluster, unsigned char *buffer, unsigned char *expected_cluster ) {
	unsigned char *target = buffer + cluster*6;

	if (memcmp(target, expected_cluster, 6) == 0) {
		return 0;
	}

	fprintf( stderr, "Cluster %d did not match expected results.\n", cluster );
	fprintf( stderr, 
		 "Expect: %3d %3d   %3d   %3d\n"
		 "        %3d %3d\n", 
		 expected_cluster[0], expected_cluster[1],
		 expected_cluster[4], expected_cluster[5],
		 expected_cluster[2], expected_cluster[3] );
	fprintf( stderr, 
		 "   Got: %3d %3d   %3d   %3d\n"
		 "        %3d %3d\n", 
		 target[0], target[1], 
		 target[4], target[5],
		 target[2], target[3] );
	return 1;
}

static int check_rgb_pixel( int pixel,
			    int min_red, int max_red,
			    int min_green, int max_green,
			    int min_blue, int max_blue,
			    unsigned char *buffer ) {
	unsigned char *rgb = buffer + 3 * pixel;
	
	if( rgb[0] >= min_red && rgb[0] <= max_red &&
	    rgb[1] >= min_green && rgb[1] <= max_green &&
	    rgb[2] >= min_blue && rgb[2] <= max_blue ) {
		return 0;
	}

	fprintf( stderr, "Pixel %d did not match expected results.\n", pixel );
	fprintf( stderr, "Got R=%d (expected %d..%d), G=%d (expected %d..%d), B=%d (expected %d..%d)\n",
		 rgb[0], min_red, max_red,
		 rgb[1], min_green, max_green,
		 rgb[2], min_blue, max_blue );
	return 1;
}

static int check_rgba_pixel(int pixel,
                            int min_red, int max_red,
                            int min_green, int max_green,
                            int min_blue, int max_blue,
                            int min_alpha, int max_alpha,
                            uint32_t *buffer ) {
	/* RGBA images are upside down - adjust for normal ordering */
	int adjusted_pixel = pixel % 128 + (127 - (pixel/128)) * 128;
	uint32_t rgba = buffer[adjusted_pixel];

	if( TIFFGetR(rgba) >= (uint32_t) min_red &&
	    TIFFGetR(rgba) <= (uint32_t) max_red &&
	    TIFFGetG(rgba) >= (uint32_t) min_green &&
	    TIFFGetG(rgba) <= (uint32_t) max_green &&
	    TIFFGetB(rgba) >= (uint32_t) min_blue &&
	    TIFFGetB(rgba) <= (uint32_t) max_blue &&
	    TIFFGetA(rgba) >= (uint32_t) min_alpha &&
	    TIFFGetA(rgba) <= (uint32_t) max_alpha ) {
		return 0;
	}

	fprintf( stderr, "Pixel %d did not match expected results.\n", pixel );
	fprintf( stderr, "Got R=%d (expected %d..%d), G=%d (expected %d..%d), B=%d (expected %d..%d), A=%d (expected %d..%d)\n",
		 TIFFGetR(rgba), min_red, max_red,
		 TIFFGetG(rgba), min_green, max_green,
		 TIFFGetB(rgba), min_blue, max_blue,
		 TIFFGetA(rgba), min_alpha, max_alpha );
	return 1;
}

int
main(int argc, char **argv)
{
	TIFF		*tif;
	static const char *srcfilerel = "images/quad-tile.jpg.tiff";
	char *srcdir = NULL;
	char srcfile[1024];
	unsigned short h, v;
	int status;
	unsigned char *buffer;
	uint32_t *rgba_buffer;
	tsize_t sz, szout;
	unsigned int pixel_status = 0;

        (void) argc;
        (void) argv;

	if ((srcdir = getenv("srcdir")) == NULL) {
		srcdir = ".";
	}
	if ((strlen(srcdir) + 1 + strlen(srcfilerel)) >= sizeof(srcfile)) {
		fprintf( stderr, "srcdir too long %s\n", srcdir);
		exit( 1 );
	}
	strcpy(srcfile,srcdir);
	strcat(srcfile,"/");
	strcat(srcfile,srcfilerel);

	tif = TIFFOpen(srcfile,"r");
	if ( tif == NULL ) {
		fprintf( stderr, "Could not open %s\n", srcfile);
		exit( 1 );
	}

	status = TIFFGetField(tif,TIFFTAG_YCBCRSUBSAMPLING, &h, &v);
	if ( status == 0 || h != 2 || v != 2) {
		fprintf( stderr, "Could not retrieve subsampling tag.\n" );
		exit(1);
	}

	/*
	 * What is the appropriate size of a YCbCr encoded tile?
	 */
	sz = TIFFTileSize(tif);
	if( sz != 24576) {
		fprintf(stderr, "tiles are %d bytes\n", (int)sz);
		exit(1);
	}

	buffer = (unsigned char *) malloc(sz);

	/*
	 * Read a tile in decompressed form, but still YCbCr subsampled.
	 */
	szout = TIFFReadEncodedTile(tif,9,buffer,sz);
	if (szout != sz) {
		fprintf( stderr, 
			 "Did not get expected result code from TIFFReadEncodedTile()(%d instead of %d)\n", 
			 (int) szout, (int) sz );
		return 1;
	}

	if( check_cluster( 0, buffer, cluster_0 )
	    || check_cluster( 64, buffer, cluster_64 )
	    || check_cluster( 128, buffer, cluster_128 ) ) {
		exit(1);
	}
	free(buffer);

	/*
	 * Read a tile using the built-in conversion to RGB format provided by the JPEG library.
	 */
	TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);

	sz = TIFFTileSize(tif);
	if( sz != 128*128*3) {
		fprintf(stderr, "tiles are %d bytes\n", (int)sz);
		exit(1);
	}

	buffer = (unsigned char *) malloc(sz);

	szout = TIFFReadEncodedTile(tif,9,buffer,sz);
	if (szout != sz) {
		fprintf( stderr, 
			 "Did not get expected result code from TIFFReadEncodedTile()(%d instead of %d)\n", 
			 (int) szout, (int) sz );
		return 1;
	}

	/*
	 * JPEG decoding is inherently inexact, so we can't test for exact
	 * pixel values.  (Well, if we knew exactly which libjpeg version
	 * we were using, and with what settings, we could expect specific
	 * values ... but it's not worth the trouble to keep track of.)
	 * Hence, use ranges of expected values.  The ranges may need to be
	 * widened over time as more versions of libjpeg appear.
	 */
	pixel_status |= check_rgb_pixel( 0, 15, 18, 0, 0, 18, 41, buffer );
	pixel_status |= check_rgb_pixel( 64, 0, 0, 0, 0, 0, 2, buffer );
	pixel_status |= check_rgb_pixel( 512, 5, 6, 34, 36, 182, 196, buffer );

	free( buffer );

	TIFFClose(tif);

	/*
	 * Reopen and test reading using the RGBA interface.
	 */
	tif = TIFFOpen(srcfile,"r");
	
	sz = 128 * 128 * sizeof(uint32_t);
	rgba_buffer = (uint32_t *) malloc(sz);
	
	if (!TIFFReadRGBATile( tif, 1*128, 2*128, rgba_buffer )) {
		fprintf( stderr, "TIFFReadRGBATile() returned failure code.\n" );
		return 1;
	}

	/*
	 * Currently TIFFReadRGBATile() just uses JPEGCOLORMODE_RGB so this
	 * trivially matches the last results.  Eventually we should actually
	 * accomplish it from the YCbCr subsampled buffer ourselves in which
	 * case the results may be subtly different but similar.
	 */
	pixel_status |= check_rgba_pixel( 0, 15, 18, 0, 0, 18, 41, 255, 255,
					  rgba_buffer );
	pixel_status |= check_rgba_pixel( 64, 0, 0, 0, 0, 0, 2, 255, 255,
					  rgba_buffer );
	pixel_status |= check_rgba_pixel( 512, 5, 6, 34, 36, 182, 196, 255, 255,
					  rgba_buffer );

	free( rgba_buffer );
	TIFFClose(tif);

	if (pixel_status) {
		exit(1);
	}
	
	exit( 0 );
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
