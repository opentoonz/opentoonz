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
 * Module to test 'D' and 'O' open flags
 */

#include "tif_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H 
# include <unistd.h> 
#endif 

#include "tiffio.h"
#include "tifftest.h"

int test(int classictif, int height, int tiled)
{
    const char* filename = "defer_strile_loading.tif";
    TIFF* tif;
    int i;
    int ret = 0;
    FILE* f;

    (void)ret;

    tif = TIFFOpen(filename, classictif ? "wDO" : "w8DO"); /* O should be ignored in write mode */
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
        int j;
        ret = TIFFSetField(tif, TIFFTAG_TILEWIDTH, 16);
        assert( ret );
        ret = TIFFSetField(tif, TIFFTAG_TILELENGTH, 16);
        assert( ret );
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
        ret = TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
        assert(ret);
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

    f = fopen(filename, "rb");
    if( !f )
        return 1;

    for( i = 0; i < 2; i++ )
    {
        tif = TIFFOpen(filename, i == 0 ? "rD" : "rO");
        if(!tif)
        {
            fprintf(stderr, "cannot open %s\n", filename);
            fclose(f);
            return 1;
        }
        if( tiled )
        {
            int j;
            for( j = 0; j < (height+15) / 16; j++ )
            {
                int retry;
                unsigned char expected_c = (unsigned char)j;

                for( retry = 0; retry < 2; retry++ )
                {
                    unsigned char tilebuffer[256];
                    memset(tilebuffer,0, 256);
                    ret = TIFFReadEncodedTile( tif, j, tilebuffer, 256 );
                    assert(ret == 256);
                    if( tilebuffer[0] != expected_c ||
                        tilebuffer[255] != expected_c )
                    {
                        fprintf(stderr, "unexpected value at tile %d: %d %d\n",
                                j, tilebuffer[0], tilebuffer[255]);
                        TIFFClose(tif);
                        fclose(f);
                        return 1;
                    }
                }

                {
                    int err = 0;
                    int offset, size;
                    unsigned char inputbuffer[256];
                    unsigned char tilebuffer[256];

                    offset = TIFFGetStrileOffsetWithErr(tif, j, &err);
                    assert(offset != 0);
                    assert(err == 0);

                    size = TIFFGetStrileByteCountWithErr(tif, j, &err);
                    (void)size;
                    assert(size == 256);
                    assert(err == 0);

                    fseek(f, offset, SEEK_SET);
                    {
                        size_t nread = fread(inputbuffer, 256, 1, f);
                        (void)nread;
                    }

                    memset(tilebuffer,0, 256);
                    ret = TIFFReadFromUserBuffer(tif, j,
                                                    inputbuffer, 256,
                                                    tilebuffer, 256 );
                    assert(ret == 1);
                    if( tilebuffer[0] != expected_c ||
                        tilebuffer[255] != expected_c )
                    {
                        fprintf(stderr, "unexpected value at tile %d: %d %d\n",
                                j, tilebuffer[0], tilebuffer[255]);
                        TIFFClose(tif);
                        fclose(f);
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
                unsigned char expected_c = (unsigned char)j;
                for( retry = 0; retry < 2; retry++ )
                {
                    unsigned char c = 0;
                    ret = TIFFReadEncodedStrip( tif, j, &c, 1 );
                    assert(ret == 1);
                    if( c != expected_c )
                    {
                        fprintf(stderr, "unexpected value at line %d: %d\n",
                                j, c);
                        TIFFClose(tif);
                        fclose(f);
                        return 1;
                    }
                }

                {
                    int err = 0;
                    int offset, size;
                    unsigned char inputbuffer[1];
                    unsigned char tilebuffer[1];

                    offset = TIFFGetStrileOffsetWithErr(tif, j, &err);
                    assert(offset != 0);
                    assert(err == 0);

                    size = TIFFGetStrileByteCountWithErr(tif, j, &err);
                    (void)size;
                    assert(size == 1);
                    assert(err == 0);

                    fseek(f, offset, SEEK_SET);
                    {
                        size_t nread = fread(inputbuffer, 1, 1, f);
                        (void)nread;
                    }
                    memset(tilebuffer,0, 1);
                    ret = TIFFReadFromUserBuffer(tif, j,
                                                    inputbuffer, 1,
                                                    tilebuffer, 1 );
                    assert(ret == 1);
                    if( tilebuffer[0] != expected_c )
                    {
                        fprintf(stderr, "unexpected value at line %d: %d\n",
                                j, tilebuffer[0]);
                        TIFFClose(tif);
                        fclose(f);
                        return 1;
                    }
                }

                if( j == 1 && height > 100000 )
                    j = height -  2;
            }

            if( height > 100000 )
            {
                /* Missing strip */
                int err = 0;
                ret = TIFFGetStrileOffsetWithErr(tif, 2, &err);
                assert(ret == 0);
                assert(err == 0);

                ret = TIFFGetStrileByteCountWithErr(tif, 2, &err);
                assert(ret == 0);
                assert(err == 0);

            }
        }

        {
            int err = 0;
            ret = TIFFGetStrileOffsetWithErr(tif, 0xFFFFFFFFU, &err);
            assert(ret == 0);
            assert(err == 1);

            ret = TIFFGetStrileByteCountWithErr(tif, 0xFFFFFFFFU, &err);
            assert(ret == 0);
            assert(err == 1);
        }

        {
            toff_t* offsets = NULL;
            toff_t* bytecounts = NULL;
            ret = TIFFGetField( tif,
                tiled ? TIFFTAG_TILEOFFSETS : TIFFTAG_STRIPOFFSETS, &offsets );
            assert(ret);
            assert(offsets);
            ret = TIFFGetField( tif,
                tiled ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS, &bytecounts );
            assert(ret);
            assert(bytecounts);
            if( tiled )
            {
                assert(bytecounts[0] == 256);
            }
            else
            {
                assert(bytecounts[0] == 1);
                if( height > 1 && height <= 100000)
                {
                    assert(offsets[1] == offsets[0] + 1);
                    assert(offsets[height - 1] == offsets[0] + height - 1);
                }
                assert(bytecounts[height - 1] == 1);
            }
        }

        TIFFClose(tif);
    }
    fclose(f);

    unlink(filename);
    return 0;
}

int
main()
{
    int is_classic;
    for( is_classic = 1; is_classic >= 0; is_classic-- )
    {
        int tiled;
        for( tiled = 0; tiled <= 1; tiled ++ )
        {
            if( test(is_classic, 1, tiled) )
                return 1;
            if( test(is_classic, 8192, tiled) )
                return 1;
        }
        if( test(is_classic, 2000000, 0) )
            return 1;
    }
    return 0;
}
