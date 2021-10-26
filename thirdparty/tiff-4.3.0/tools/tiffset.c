/******************************************************************************
 * Project:  libtiff tools
 * Purpose:  Mainline for setting metadata in existing TIFF files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 */

#include "tif_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tiffio.h"

#ifdef NEED_LIBPORT
# include "libport.h"
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

static const char usageMsg[] =
"Set the value of a TIFF header to a specified value\n\n"
"usage: tiffset [options] filename\n"
"where options are:\n"
" -s  <tagname> [count] <value>...   set the tag value\n"
" -u  <tagname> to unset the tag\n"
" -d  <dirno> set the directory\n"
" -sd <diroff> set the subdirectory\n"
" -sf <tagname> <filename>  read the tag value from file (for ASCII tags only)\n"
" -h  this help screen\n"
;

static void
usage(int code)
{
	FILE * out = (code == EXIT_SUCCESS) ? stdout : stderr;

	fprintf(out, "%s\n\n", TIFFGetVersion());
        fprintf(out, "%s", usageMsg);
	exit(code);
}

static const TIFFField *
GetField(TIFF *tiff, const char *tagname)
{
    const TIFFField *fip;

    if( atoi(tagname) > 0 )
        fip = TIFFFieldWithTag(tiff, (ttag_t)atoi(tagname));
    else
        fip = TIFFFieldWithName(tiff, tagname);

    if (!fip) {
        fprintf( stderr, "Field name \"%s\" is not recognised.\n", tagname );
        return (TIFFField *)NULL;
    }

    return fip;
}

int
main(int argc, char* argv[])
{
    TIFF *tiff;
    int  arg_index;

    if (argc < 2)
        usage(EXIT_FAILURE);

    tiff = TIFFOpen(argv[argc-1], "r+");
    if (tiff == NULL)
        return EXIT_FAILURE;

    for( arg_index = 1; arg_index < argc-1; arg_index++ ) {
	if (strcmp(argv[arg_index],"-d") == 0 && arg_index < argc-2) {
	    arg_index++;
	    if( TIFFSetDirectory(tiff, atoi(argv[arg_index]) ) != 1 )
            {
               fprintf( stderr, "Failed to set directory=%s\n", argv[arg_index] );
               return EXIT_FAILURE;
            }
	    arg_index++;
	}
	if (strcmp(argv[arg_index],"-sd") == 0 && arg_index < argc-2) {
	    arg_index++;
	    if( TIFFSetSubDirectory(tiff, atoi(argv[arg_index]) ) != 1 )
            {
               fprintf( stderr, "Failed to set sub directory=%s\n", argv[arg_index] );
               return EXIT_FAILURE;
            }
	    arg_index++;
	}
    /* Add unset option to tiffset -- Zach Baker (niquil@niquil.net) 11/14/2012 */ 
    if (strcmp(argv[arg_index],"-u") == 0 && arg_index < argc-2) {
            const TIFFField *fip;
            const char *tagname;
            arg_index++;
            tagname = argv[arg_index];
            fip = GetField(tiff, tagname);
            if (!fip)
               return EXIT_FAILURE;

            if (TIFFUnsetField(tiff, TIFFFieldTag(fip)) != 1)
            {
                    fprintf(stderr, "Failed to unset %s\n", TIFFFieldName(fip));
            }
            arg_index++;
    } else if (strcmp(argv[arg_index],"-s") == 0 && arg_index < argc-3) {
            const TIFFField *fip;
            const char *tagname;

            arg_index++;
            tagname = argv[arg_index];
            fip = GetField(tiff, tagname);

            if (!fip)
                return 3;

            arg_index++;
            if (TIFFFieldDataType(fip) == TIFF_ASCII) {
                if (TIFFSetField(tiff, TIFFFieldTag(fip), argv[arg_index]) != 1)
                    fprintf( stderr, "Failed to set %s=%s\n",
                             TIFFFieldName(fip), argv[arg_index] );
            } else if (TIFFFieldWriteCount(fip) > 0
		       || TIFFFieldWriteCount(fip) == TIFF_VARIABLE) {
                int     ret = 1;
                short   wc;

                if (TIFFFieldWriteCount(fip) == TIFF_VARIABLE)
                        wc = atoi(argv[arg_index++]);
                else
                        wc = TIFFFieldWriteCount(fip);

                if (argc - arg_index < wc) {
                    fprintf( stderr,
                             "Number of tag values is not enough. "
                             "Expected %d values for %s tag, got %d\n",
                             wc, TIFFFieldName(fip), argc - arg_index);
                    return EXIT_FAILURE;
                }
                    
                if (wc > 1 || TIFFFieldWriteCount(fip) == TIFF_VARIABLE) {
                        int     i, size;
                        void    *array;

                        switch (TIFFFieldDataType(fip)) {
                                /*
                                 * XXX: We can't use TIFFDataWidth()
                                 * to determine the space needed to store
                                 * the value. For TIFF_RATIONAL values
                                 * TIFFDataWidth() returns 8, but we use 4-byte
                                 * float to represent rationals.
                                 */
                                case TIFF_BYTE:
                                case TIFF_ASCII:
                                case TIFF_SBYTE:
                                case TIFF_UNDEFINED:
				default:
                                    size = 1;
                                    break;

                                case TIFF_SHORT:
                                case TIFF_SSHORT:
                                    size = 2;
                                    break;

                                case TIFF_LONG:
                                case TIFF_SLONG:
                                case TIFF_FLOAT:
                                case TIFF_IFD:
                                case TIFF_RATIONAL:
                                case TIFF_SRATIONAL:
                                    size = 4;
                                    break;

                                case TIFF_LONG8:
                                case TIFF_SLONG8:
                                case TIFF_IFD8:
                                case TIFF_DOUBLE:
                                    size = 8;
                                    break;
                        }

                        array = _TIFFmalloc(wc * size);
                        if (!array) {
                                fprintf(stderr, "No space for %s tag\n",
                                        tagname);
                                return EXIT_FAILURE;
                        }

                        switch (TIFFFieldDataType(fip)) {
                            case TIFF_BYTE:
                                for (i = 0; i < wc; i++)
                                    ((uint8_t *)array)[i] = atoi(argv[arg_index + i]);
                                break;
                            case TIFF_SHORT:
                                for (i = 0; i < wc; i++)
                                    ((uint16_t *)array)[i] = atoi(argv[arg_index + i]);
                                break;
                            case TIFF_SBYTE:
                                for (i = 0; i < wc; i++)
                                    ((int8_t *)array)[i] = atoi(argv[arg_index + i]);
                                break;
                            case TIFF_SSHORT:
                                for (i = 0; i < wc; i++)
                                    ((int16_t *)array)[i] = atoi(argv[arg_index + i]);
                                break;
                            case TIFF_LONG:
                                for (i = 0; i < wc; i++)
                                    ((uint32_t *)array)[i] = atol(argv[arg_index + i]);
                                break;
                            case TIFF_SLONG:
                            case TIFF_IFD:
                                for (i = 0; i < wc; i++)
                                    ((int32_t *)array)[i] = atol(argv[arg_index + i]);
                                break;
                            case TIFF_LONG8:
                                for (i = 0; i < wc; i++)
                                    ((uint64_t *)array)[i] = strtoll(argv[arg_index + i], (char **)NULL, 10);
                                break;
                            case TIFF_SLONG8:
                            case TIFF_IFD8:
                                for (i = 0; i < wc; i++)
                                    ((int64_t *)array)[i] = strtoll(argv[arg_index + i], (char **)NULL, 10);
                                break;
                            case TIFF_DOUBLE:
                                for (i = 0; i < wc; i++)
                                    ((double *)array)[i] = atof(argv[arg_index+i]);
                                break;
                            case TIFF_RATIONAL:
                            case TIFF_SRATIONAL:
                            case TIFF_FLOAT:
                                for (i = 0; i < wc; i++)
                                    ((float *)array)[i] = (float)atof(argv[arg_index+i]);
                                break;
                            default:
                                break;
                        }
                
                        if (TIFFFieldPassCount(fip)) {
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   wc, array);
                        } else if (TIFFFieldTag(fip) == TIFFTAG_PAGENUMBER
				   || TIFFFieldTag(fip) == TIFFTAG_HALFTONEHINTS
				   || TIFFFieldTag(fip) == TIFFTAG_YCBCRSUBSAMPLING
				   || TIFFFieldTag(fip) == TIFFTAG_DOTRANGE) {
       				if (TIFFFieldDataType(fip) == TIFF_BYTE) {
					ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                       ((uint8_t *)array)[0], ((uint8_t *)array)[1]);
				} else if (TIFFFieldDataType(fip) == TIFF_SHORT) {
					ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                       ((uint16_t *)array)[0], ((uint16_t *)array)[1]);
				}
			} else {
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   array);
                        }

                        _TIFFfree(array);
                } else {
                        switch (TIFFFieldDataType(fip)) {
                            case TIFF_BYTE:
                            case TIFF_SHORT:
                            case TIFF_SBYTE:
                            case TIFF_SSHORT:
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   atoi(argv[arg_index++]));
                                break;
                            case TIFF_LONG:
                            case TIFF_SLONG:
                            case TIFF_IFD:
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   atol(argv[arg_index++]));
                                break;
                            case TIFF_LONG8:
                            case TIFF_SLONG8:
                            case TIFF_IFD8:
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   strtoll(argv[arg_index++], (char **)NULL, 10));
                                break;
                            case TIFF_DOUBLE:
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   atof(argv[arg_index++]));
                                break;
                            case TIFF_RATIONAL:
                            case TIFF_SRATIONAL:
                            case TIFF_FLOAT:
                                ret = TIFFSetField(tiff, TIFFFieldTag(fip),
                                                   (float)atof(argv[arg_index++]));
                                break;
                            default:
                                break;
                        }
                }

                if (ret != 1)
                    fprintf(stderr, "Failed to set %s\n", TIFFFieldName(fip));
                arg_index += wc;
            }
        } else if (strcmp(argv[arg_index],"-sf") == 0 && arg_index < argc-3) {
            FILE    *fp;
            const TIFFField *fip;
            char    *text;
            size_t  len;
            int ret;

            arg_index++;
            fip = GetField(tiff, argv[arg_index]);

            if (!fip)
                return EXIT_FAILURE;

            if (TIFFFieldDataType(fip) != TIFF_ASCII) {
                fprintf( stderr,
                         "Only ASCII tags can be set from file. "
                         "%s is not ASCII tag.\n", TIFFFieldName(fip) );
                return EXIT_FAILURE;
            }

            arg_index++;
            fp = fopen( argv[arg_index], "rt" );
            if(fp == NULL) {
                perror( argv[arg_index] );
                continue;
            }

            text = (char *) malloc(1000000);
            if(text == NULL) {
                fprintf( stderr,
                         "Memory allocation error\n");
                fclose( fp );
                continue;
            }
            len = fread( text, 1, 999999, fp );
            text[len] = '\0';

            fclose( fp );

            if(TIFFFieldPassCount( fip )) {
                ret = TIFFSetField(tiff, TIFFFieldTag(fip), (uint16_t)len, text );
            } else {
                ret = TIFFSetField( tiff, TIFFFieldTag(fip), text );
            }
            if(!ret) {
                fprintf(stderr, "Failed to set %s from file %s\n", 
                        TIFFFieldName(fip), argv[arg_index]);
            }

            _TIFFfree( text );
            arg_index++;
        } else if (strcmp(argv[arg_index],"-h") == 0 || strcmp(argv[arg_index],"--help") == 0) {
            usage(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "Unrecognised option: %s\n",
                    argv[arg_index]);
            usage(EXIT_FAILURE);
        }
    }

    TIFFRewriteDirectory(tiff);
    TIFFClose(tiff);
    return EXIT_SUCCESS;
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
