/*
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
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
 * Module to test TIFFDeferStrileArrayWriting and TIFFForceStrileArrayWriting
 */

#include "tif_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H 
# include <unistd.h> 
#endif 

#include "tiffio.h"

int test(const char* mode, int tiled, int height)
{
    const char* filename = "defer_strile_writing.tif";
    TIFF* tif;
    int i;
    int ret = 0;
    (void)ret;

    tif = TIFFOpen(filename, mode);
    if(!tif)
    {
        fprintf(stderr, "cannot create %s\n", filename);
        return 1;
    }
    ret = TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, 1);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    assert(ret);

    if( tiled )
    {
        ret = TIFFSetField(tif, TIFFTAG_TILEWIDTH, 16);
        assert( ret );
        ret = TIFFSetField(tif, TIFFTAG_TILELENGTH, 16);
        assert( ret );
    }
    else
    {
        ret = TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
        assert(ret);
    }

    ret = TIFFDeferStrileArrayWriting(tif);
    assert(ret);

    ret = TIFFWriteCheck( tif, tiled, "test" );
    assert(ret);

    ret = TIFFWriteDirectory( tif );
    assert(ret);

    /* Create other directory */
    TIFFFreeDirectory( tif );
    TIFFCreateDirectory( tif );

    ret = TIFFSetField( tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE );
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, 1);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_IMAGELENGTH, 1);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    assert(ret);
    ret = TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    assert(ret);

    ret = TIFFDeferStrileArrayWriting(tif);
    assert(ret);

    ret = TIFFWriteCheck( tif, 0, "test" );
    assert(ret);

    ret = TIFFWriteDirectory( tif );
    assert(ret);

    /* Force writing of strile arrays */
    ret = TIFFSetDirectory( tif, 0 );
    assert(ret);

    ret = TIFFForceStrileArrayWriting(tif);
    assert(ret);

    ret = TIFFSetDirectory( tif, 1 );
    assert(ret);

    ret = TIFFForceStrileArrayWriting(tif);
    assert(ret);

    /* Now write data on first directory */
    ret = TIFFSetDirectory( tif, 0 );
    assert(ret);

    if( tiled )
    {
        int j;
        for( j = 0; j < (height+15) / 16; j++ )
        {
            unsigned char tilebuffer[256];
            memset(tilebuffer, (unsigned char)j, 256);
            ret = TIFFWriteEncodedTile( tif, j, tilebuffer, 256 );
            assert(ret == 256);
        }
    }
    else
    {
        for( i = 0; i < height; i++ )
        {
            unsigned char c = (unsigned char)i;
            ret = TIFFWriteEncodedStrip( tif, i, &c, 1 );
            assert(ret == 1);

            if( i == 1 && height > 100000 )
                i = height -  2;
        }
    }

    TIFFClose(tif);

    tif = TIFFOpen(filename, "r");
    if(!tif)
    {
        fprintf(stderr, "cannot open %s\n", filename);
        return 1;
    }
    if( tiled )
    {
        int j;
        for( j = 0; j < (height+15) / 16; j++ )
        {
            int retry;
            for( retry = 0; retry < 2; retry++ )
            {
                unsigned char tilebuffer[256];
                unsigned char expected_c = (unsigned char)j;
                memset(tilebuffer,0, 256);
                ret = TIFFReadEncodedTile( tif, j, tilebuffer, 256 );
                assert(ret == 256);
                if( tilebuffer[0] != expected_c ||
                    tilebuffer[255] != expected_c )
                {
                    fprintf(stderr, "unexpected value at tile %d: %d %d\n",
                            j, tilebuffer[0], tilebuffer[255]);
                    TIFFClose(tif);
                    return 1;
                }
            }
        }
    }
    else
    {
        int j;
        for( j = 0; j < height; j++ )
        {
            int retry;
            for( retry = 0; retry < 2; retry++ )
            {
                unsigned char c = 0;
                unsigned char expected_c = (unsigned char)j;
                ret = TIFFReadEncodedStrip( tif, j, &c, 1 );
                assert(ret == 1);
                if( c != expected_c )
                {
                    fprintf(stderr, "unexpected value at line %d: %d\n",
                            j, c);
                    TIFFClose(tif);
                    return 1;
                }
            }
        }
    }

    TIFFClose(tif);

    unlink(filename);
    return 0;
}

int
main()
{
    int tiled;
    for( tiled = 0; tiled <= 1; tiled ++ )
    {
        if( test("w", tiled, 1) )
            return 1;
        if( test("w", tiled, 10) )
            return 1;
        if( test("w8", tiled, 1) )
            return 1;
        if( test("wD", tiled, 1) )
            return 1;
    }
    return 0;
}
