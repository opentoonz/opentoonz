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
 * -- Module copied from custom_dir.c --
 *===========  Purpose ===================================================================================
 * Extended and amended version for testing of EXIF 2.32, GPS and handling of custom fields.
 * EXIF 2.32 and GPS are defined in amended files tif_dirinfo.c, tif_dirread.c, tiff.h, tiffio.h, tif_dir.h, tif_dir.c
 *
 *-- ATTENTION: After the upgrade with Rational2Double, the GPSTAG values are defined as double precision
 *              and need to be written and also read in double precision!
 *              In order to maintain this code for both cases, it is checked above if the TiffLibrary is
 *              compiled with the new interface with Rational2Double or still uses the old definitions,
 *              by setting blnIsRational2Double above.
 *
 */


/*------------
 * This version writes the GPS and EXIF tags correctly, without additional main-IFD and parameters!
 * In contrary, custom_dir.c does write additional main-IFD and parameters to file.
 -------------*/


#define FOR_AUTO_TESTING
#ifdef FOR_AUTO_TESTING
/*  Only for automake and CMake infrastructure the test should:
	a.) delete any written testfiles when test passed (otherwise autotest will fail)
	b.) goto failure, if any failure is detected, which is not necessary when test is initiated manually for debugging
*/
#define GOTOFAILURE	goto failure;
#define GOTOFAILURE_GPS	goto failure;
#define GOTOFAILURE_ALL_EXIF	goto failure;
#else
#define GOTOFAILURE
#define GOTOFAILURE_GPS
#define GOTOFAILURE_ALL_EXIF
#endif


#ifdef _MSC_VER
#pragma warning( disable : 4101)
#endif

#include "tif_config.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H 
# include <unistd.h> 
#endif 

#include "tiffio.h"
#include "tiffiop.h"
#include "tif_dir.h"
#include "tifftest.h"



int write_test_tiff(TIFF *tif, const char *filenameRead);

static const char filename[] = "custom_dir_EXIF_231.tif";
static const char filenameBigTiff[] = "custom_dir_EXIF_231_Big.tif";

#define	SPP	3		/* Samples per pixel */
const uint16_t	width = 1;
const uint16_t	length = 1;
const uint16_t	bps = 8;
const uint16_t	photometric = PHOTOMETRIC_RGB;
const uint16_t	rows_per_strip = 1;
const uint16_t	planarconfig = PLANARCONFIG_CONTIG;


int
main()
{
	TIFF			*tif;
	int				ret, ret1, ret2;

	fprintf(stderr, "==== Test automatically if all EXIF and GPS tags are written/read correctly. ====\n");
	/* --- Test with Classic-TIFF ---*/
	/* delete file, if exists */
	ret = unlink(filename);
	if (ret != 0 && errno != ENOENT) {
		fprintf(stderr, "Can't delete test TIFF file %s.\n", filename);
	}

	/* We write the main directory as a simple image. */
	tif = TIFFOpen(filename, "w+");
	if (!tif) {
		fprintf(stderr, "Can't create test TIFF file %s.\n", filename);
		return 1;
	}
	fprintf(stderr, "-------- Test with ClassicTIFF started  ----------\n");
	ret1 = write_test_tiff(tif, filename);

	if (ret1 > 0) return(ret1);

	/*--- Test with BIG-TIFF ---*/
	/* delete file, if exists */
	ret = unlink(filenameBigTiff);
	if (ret != 0 && errno != ENOENT) {
		fprintf(stderr, "Can't delete test TIFF file %s.\n", filenameBigTiff);
	}

	tif = TIFFOpen(filenameBigTiff, "w8");
	if (!tif) {
		fprintf(stderr, "Can't create test TIFF file %s.\n", filenameBigTiff);
		return 1;
	}
	fprintf(stderr, "\n\n-------- Test with BigTIFF started  ----------\n");
	ret2 = write_test_tiff(tif, filenameBigTiff);

	if (ret2 > 0) return(ret2 + 10); else return(ret2);

} /* main() */







int
write_test_tiff(TIFF *tif, const char *filenameRead)
{
	unsigned char	buf[SPP] = { 0, 127, 255 };
	uint64_t          dir_offset = 0;
	uint64_t			dir_offset_GPS = 0, dir_offset_EXIF = 0;
	uint64_t          read_dir_offset = 0;
    /*-- Additional variables --*/
	int				retCode, retCode2;
	unsigned char	exifVersion[4] = {'0','2','3','1'};		/* EXIF 2.31 version is 4 characters of a string! */
	unsigned char	gpsVersion[4] = {2,2,0,1};		/* GPS Version is 4 numbers! */
	unsigned char   *pGpsVersion;
	float			auxFloat = 0.0f;
	double			auxDouble = 0.0;
	char			auxChar = 0;
	uint32_t			auxUint32 = 0;
	short			auxShort=0;
	long			auxLong = 0;
	void			*pVoid;
	int				blnIsRational2Double;

	int		i, j;
	long	nTags;

	const TIFFFieldArray*	tFieldArray;
	unsigned long			tTag;
	TIFFDataType			tType;
	short					tWriteCount;
	TIFFSetGetFieldType		tSetFieldType;
	char					*tFieldName;
	const TIFFField			*fip;

	char	blnFillGPSManually = 1;

#define STRSIZE 1000
#define N_SIZE  120
#define VARIABLE_ARRAY_SIZE 6

	/* -- Test data for writing -- */
	char			auxCharArrayW[N_SIZE];
	short			auxShortArrayW[N_SIZE];
	long			auxLongArrayW[N_SIZE];
	float			auxFloatArrayW[N_SIZE];
	double			auxDoubleArrayW[N_SIZE];
	char			auxTextArrayW[N_SIZE][STRSIZE];
	double			auxDoubleArrayGPS1[3] = {1.0/7.0, 61.23456789012345, 62.0};
	double			auxDoubleArrayGPS2[3] = {1.0/19.0, 88.34434, 15.12345678901234567890};
	double			auxDoubleArrayGPSTime[3] = {22.0, 17.0, 15.3456789};
	double			auxDoubleGPSAltitude     =  3456.0;
	double			auxDoubleGPSDirection    =  63.7;
	float			auxFloatArrayN1[3] = { 1.0f / 7.0f, 61.23456789012345f, 62.3f };
	float			auxFloatArrayN2[3] = { -1.0f / 7.0f, -61.23456789012345f, -62.3f };

	/* -- Variables for reading -- */
	uint16_t      count16 = 0;
	union  {
		long		Long;
		short		Short1;
		short		Short2[2];
		char		Char[4];
	} unionLong;
	union {
		double	dbl;
		float	flt1;
		float	flt2;
	} auxDblUnion;
	void		*pVoidArray;
	char        *pAscii;
	char		auxCharArray[2*STRSIZE];
	short		auxShortArray[2*N_SIZE];
	long		auxLongArray[2*N_SIZE];
	float		auxFloatArray[2*N_SIZE];
	double		auxDoubleArray[2*N_SIZE];
	double		dblDiff, dblDiffLimit;
#define RATIONAL_EPS (1.0/30000.0) /* reduced difference of rational values, approx 3.3e-5 */

	/*-- Fill test data arrays for writing ----------- */
	for (i=0; i<N_SIZE; i++) {
		sprintf(auxTextArrayW[i],"N%d-String-%d_tttttttttttttttttttttttttttttx", i, i);
	}
	for (i=0; i<N_SIZE; i++) {
		auxCharArrayW[i] = (char)(i+1);
	}
	for (i=0; i<N_SIZE; i++) {
		auxShortArrayW[i] = (short)(i+1)*7;
	}
	for (i=0; i<N_SIZE; i++) {
		auxLongArrayW[i] = (i+1)*133;
	}
	for (i=0; i<N_SIZE; i++) {
		auxFloatArrayW[i] = (float)((i+1)*133)/3.3f;
	}
	for (i=0; i<N_SIZE; i++) {
		auxDoubleArrayW[i] = (double)((i+1)*3689)/4.5697;
	}

	/*-- Setup standard tags of a simple tiff file --*/
	if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width)) {
		fprintf (stderr, "Can't set ImageWidth tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_IMAGELENGTH, length)) {
		fprintf (stderr, "Can't set ImageLength tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bps)) {
		fprintf (stderr, "Can't set BitsPerSample tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, SPP)) {
		fprintf (stderr, "Can't set SamplesPerPixel tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip)) {
		fprintf (stderr, "Can't set SamplesPerPixel tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_PLANARCONFIG, planarconfig)) {
		fprintf (stderr, "Can't set PlanarConfiguration tag.\n");
		goto failure;
	}
	if (!TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric)) {
		fprintf (stderr, "Can't set PhotometricInterpretation tag.\n");
		goto failure;
	}

#define ADDITIONAL_TAGS
#ifdef ADDITIONAL_TAGS
	/*-- Additional tags to check Rational standard tags, which are also defined as FIELD_CUSTOM */

	/*- TIFFTAG_INKSET is a SHORT parameter (TIFF_SHORT, TIFF_SETGET_UINT16) with field_bit=FIELD_CUSTOM !! -*/
	if (!TIFFSetField(tif, TIFFTAG_INKSET, 34)) {
		fprintf(stderr, "Can't set TIFFTAG_INKSET tag.\n");
		goto failure;
	}

	/*- TIFFTAG_PIXAR_FOVCOT is a FLOAT parameter ( TIFF_FLOAT, TIFF_SETGET_FLOAT) with field_bit=FIELD_CUSTOM !! -*/
	/*  - can be written with Double but has to be read with float parameter                                       */
#define PIXAR_FOVCOT_VAL	5.123456789123456789
	auxFloat = (float)PIXAR_FOVCOT_VAL;
	auxDouble = (double)PIXAR_FOVCOT_VAL;
	if (!TIFFSetField(tif, TIFFTAG_PIXAR_FOVCOT, auxDouble)) {
		fprintf(stderr, "Can't set TIFFTAG_PIXAR_FOVCOT tag.\n");
		goto failure;
	}
	/*- TIFFTAG_STONITS is a DOUBLE parameter (TIFF_DOUBLE, TIFF_SETGET_DOUBLE) with field_bit=FIELD_CUSTOM!
	 *   Only TIFFTAG_STONITS is a TIFF_DOUBLE, which has to be read as DOUBLE!!
	 */
#define STONITS_VAL 6.123456789123456789
	auxDouble = STONITS_VAL;
	auxFloat = (float)auxDouble;
	if (!TIFFSetField(tif, TIFFTAG_STONITS, auxDouble)) {
		fprintf(stderr, "Can't set TIFFTAG_STONITS tag.\n");
		goto failure;
	}

	/*- TIFFTAG_YCBCRPOSITIONING is a SHORT parameter */
	auxLong = auxShort = 5;
	if (!TIFFSetField(tif, TIFFTAG_YCBCRPOSITIONING, auxLong )) {
		fprintf (stderr, "Can't set TIFFTAG_YCBCRPOSITIONING tag.\n");
		goto failure;
	}

	/* - TIFFTAG_BESTQUALITYSCALE is a Rational parameter, FIELD_CUSTOM and TIFF_SETGET_DOUBLE */
	/* With Rational2Double upgrade tag is redefined to TIFF_SETGET_FLOAT, but can be still written with double. */
#define BESTQUALITYSCALE_VAL   15.3
	auxDouble = BESTQUALITYSCALE_VAL;
	if (!TIFFSetField(tif, TIFFTAG_BESTQUALITYSCALE, auxDouble )) {
		fprintf (stderr, "Can't set TIFFTAG_BESTQUALITYSCALE tag.\n");
		goto failure;
	}

	/* - TIFFTAG_BASELINENOISE, 1, 1, TIFF_RATIONAL, 0, TIFF_SETGET_FLOAT */
	if (!TIFFSetField(tif, TIFFTAG_BASELINENOISE, auxDouble)) {
		fprintf(stderr, "Can't set TIFFTAG_BASELINENOISE tag.\n");
		goto failure;
	}

	
	/*--- For static or variable ARRAYs the case is different ---*/
/*- Variable Array: TIFFTAG_DECODE is a SRATIONAL parameter TIFF_SETGET_C16_FLOAT type FIELD_CUSTOM with passcount=1 and variable length of array. */
	if (!TIFFSetField(tif, TIFFTAG_DECODE, 3, auxFloatArrayN2)) {
		fprintf(stderr, "Can't set TIFFTAG_DECODE tag.\n");
		goto failure;
	}

	/*- Variable Array:  TIFF_RATIONAL, 0, TIFF_SETGET_C16_FLOAT */
	if (!TIFFSetField(tif, TIFFTAG_BLACKLEVEL, 3, auxFloatArrayN1)) {
		fprintf(stderr, "Can't set TIFFTAG_BLACKLEVEL tag.\n");
		goto failure;
	}

	/*- Fixed Array: TIFFTAG_DEFAULTCROPSIZE, 2, 2, TIFF_RATIONAL, 0, TIFF_SETGET_C0_FLOAT */
	if (!TIFFSetField(tif, TIFFTAG_DEFAULTCROPSIZE, &auxFloatArrayW[0])) {
		fprintf(stderr, "Can't set TIFFTAG_DEFAULTCROPSIZE tag.\n");
		goto failure;
	}
#endif	/* -- ADDITIONAL_TAGS -- */

/*================== Rational2Double Interface Check =====================*/
	/*-- Check, if the TiffLibrary is compiled with the new interface with Rational2Double or still uses the old definitions. 
	     For that, TIFF_RATIONAL tags with FIELD_CUSTOM are changed from TIFF_SETGET_DOUBLE to TIFF_SETGET_FLOAT for the 
	     new interface in order to prevent the old reading behaviour.
	     Tags to check: TIFFTAG_BESTQUALITYSCALE, TIFFTAG_BASELINENOISE, TIFFTAG_BASELINESHARPNESS
	 */
	fip = TIFFFindField(tif, TIFFTAG_BESTQUALITYSCALE, TIFF_ANY);
	tSetFieldType = fip->set_field_type;
	if (tSetFieldType == TIFF_SETGET_DOUBLE) {
		blnIsRational2Double = FALSE;
	} else {
		blnIsRational2Double = TRUE;
		fprintf(stderr, "-- Rational2Double from TIFF tag detected --\n");
	}

/*================== Write GPS and EXIF tags =====================*/

	/*-- Set dummy EXIF/GPS tag in original tiff-structure in order to reserve space for final dir_offset value, */
	/*   which is properly written at the end.  */
	dir_offset = 0;  /* Zero, in case no Custom-IFD is written */

#define WRITE_GPS_TAGS
#ifdef WRITE_GPS_TAGS
	if (!TIFFSetField(tif, TIFFTAG_GPSIFD, dir_offset )) {
		fprintf (stderr, "Can't write TIFFTAG_GPSIFD\n" );
	}
#endif

	/*------- And also do the same for the EXIF IFD tag here, because we have to save the main directory next ------*/
	/*-- Set dummy EXIF/GPS tag in original tiff-structure in order to reserve space for final dir_offset value,
	 *   which is properly written at the end.
     */
#define WRITE_EXIF_TAGS
#ifdef WRITE_EXIF_TAGS
	if (!TIFFSetField(tif, TIFFTAG_EXIFIFD, dir_offset )) {
		fprintf (stderr, "Can't write TIFFTAG_EXIFIFD\n" );
	}
#endif

#ifndef WRITEPIXELLAST
	/*-- Write dummy pixel data. --*/
	if (TIFFWriteScanline(tif, buf, 0, 0) < 0) {
		fprintf (stderr, "Can't write image data.\n");
		goto failure;
	}
#endif


#ifdef WRITE_GPS_TAGS
#define READ_GPS_TAGS
	/*================== Write GPS tags =====================*/

	/*-- Save current tiff-directory to file before directory is changed. Otherwise it will be lost!  */
	/*   The tif-structure is overwritten/ freshly initialized by any "CreateDirectory" */
	/*retCode = TIFFCheckpointDirectory(tif);*/	/* does not cleanup Tiff-Structure */
	retCode = TIFFWriteDirectory(tif);			/* cleanup Tiff-structure */

	/*-- Now create a GPS directory. */
	if (TIFFCreateGPSDirectory(tif) != 0) {
		fprintf (stderr, "TIFFCreateGPSDirectory() failed.\n" );
		goto failure;
	}

	if (!TIFFSetField( tif, GPSTAG_VERSIONID, gpsVersion)) {
		fprintf (stderr, "Can't write GPSTAG_VERSIONID\n" );
		goto failure;
	}		

	if (blnFillGPSManually) {
		/*================= Write manually valid data to the GPS fields ==============*/
		if (!TIFFSetField( tif, GPSTAG_LATITUDEREF, "N\0")) {
			fprintf (stderr, "Can't write GPSTAG_LATITUDEREF\n" );
			goto failure;
		}		
		/*-- Unfortunately, Rational values are defined as SETGET_DOUBLE but are internally always stored as float.
		 *   Single Rational values do not matter for writing, because TIFFSetField() uses va_arg() which performs "variables promotion" from type float to type double!
		 *   However, for reading of Rational values ONLY float-variables are allowed - in contrary to the SETGET_DOUBLE specification at tiffFields[] in tif_dirinfo.c.
		 */
		/*-- ATTENTION: After the upgrade with Rational2Double, the GPSTAG values are defined as double precision
		 *              and need to be written and also read in double precision!
		 *              In order to maintain this code for both cases, it is checked above if the TiffLibrary is
		 *              compiled with the new interface with Rational2Double or still uses the old definitions,
		 *              by setting blnIsRational2Double above.
		 */
		if (blnIsRational2Double) {
			fprintf(stderr, "-- GPS tags are written using Rational2Double --\n");
		} else {
			fprintf(stderr, "-- GPS tags are written using standard --\n");
		}
		if (!blnIsRational2Double) {
			for (j = 0; j < 3; j++) auxFloatArray[j] = (float)auxDoubleArrayGPS1[j];
			if (!TIFFSetField(tif, GPSTAG_LATITUDE, auxFloatArray)) {
				fprintf(stderr, "Can't write GPSTAG_LATITUDE\n");
				goto failure;
			}
		} else {
			/* Rational2Double interface for GPSTAG */
			if (!TIFFSetField(tif, GPSTAG_LATITUDE, auxDoubleArrayGPS1)) {
				fprintf(stderr, "Can't write GPSTAG_LATITUDE\n");
				goto failure;
			}
		}
		if (!TIFFSetField( tif, GPSTAG_LONGITUDEREF, "W\0")) {
			fprintf (stderr, "Can't write GPSTAG_LONGITUDEREF\n" );
			goto failure;
		}
		if (!blnIsRational2Double) {
			for (j=0; j<3; j++) auxFloatArray[j] = (float)auxDoubleArrayGPS2[j];
			if (!TIFFSetField( tif, GPSTAG_LONGITUDE, auxFloatArray)) {
				fprintf (stderr, "Can't write GPSTAG_LONGITUDE\n" );
				goto failure;
			}
		} else {
			/* Rational2Double interface for GPSTAG */
			if (!TIFFSetField(tif, GPSTAG_LONGITUDE, auxDoubleArrayGPS2)) {
				fprintf(stderr, "Can't write GPSTAG_LATITUDE\n");
				goto failure;
			}
		}
		/*-- AltitudeRef: default is above sea level!! */
		if (!TIFFSetField( tif, GPSTAG_ALTITUDEREF, 0)) {
			fprintf (stderr, "Can't write GPSTAG_ALTITUDEREF\n" );
			goto failure;
		}
		if (!TIFFSetField( tif, GPSTAG_ALTITUDE, auxDoubleGPSAltitude)) {
			fprintf (stderr, "Can't write GPSTAG_ALTITUDE\n" );
			goto failure;
		}
		/*-- TimeStamp is only hh:mm:ss. See also DateTime string */
		if (!TIFFSetField( tif, GPSTAG_TIMESTAMP, auxDoubleArrayGPSTime)) {
			fprintf (stderr, "Can't write GPSTAG_TIMESTAMP\n" );
			goto failure;
		}
		if (!TIFFSetField( tif, GPSTAG_DATESTAMP, "2012:11:04")) {
			fprintf (stderr, "Can't write GPSTAG_DATESTAMP\n" );
			goto failure;
		}

		if (!TIFFSetField( tif, GPSTAG_IMGDIRECTIONREF, "T\0")) {
			fprintf (stderr, "Can't write GPSTAG_IMGDIRECTIONREF\n" );
			goto failure;
		}
		if (!TIFFSetField( tif, GPSTAG_IMGDIRECTION, auxDoubleGPSDirection)) {
			fprintf (stderr, "Can't write GPSTAG_IMGDIRECTION\n" );
			goto failure;
		}

		/*-- Type TIFF_UNDEFINED */
		if (!TIFFSetField( tif, GPSTAG_PROCESSINGMETHOD, 3, &auxCharArrayW[10])) {
			fprintf (stderr, "Can't write GPSTAG_PROCESSINGMETHOD\n" );
			goto failure;
		}
		if (!TIFFSetField( tif, GPSTAG_AREAINFORMATION, 4, &auxCharArrayW[20])) {
			fprintf (stderr, "Can't write GPSTAG_AREAINFORMATION\n" );
			goto failure;
		}

		/*-- PSTAG_DIFFERENTIAL	, 1, 1,	TIFF_SHORT	, 0, 	TIFF_SETGET_UINT16 */
		if (!TIFFSetField( tif, GPSTAG_DIFFERENTIAL, auxShortArrayW[5])) {
			fprintf (stderr, "Can't write GPSTAG_DIFFERENTIAL\n" );
			goto failure;
		}

		/* GPSTAG_GPSHPOSITIONINGERROR	, 1, 1,	TIFF_RATIONAL	, 0, 	TIFF_SETGET_DOUBLE  but here written in float-precision */
#define GPSHPOSITIONINGERROR_VAL 0.369
		auxFloat = (float)GPSHPOSITIONINGERROR_VAL;
		if (!TIFFSetField( tif, GPSTAG_GPSHPOSITIONINGERROR, auxFloat)) {
			fprintf (stderr, "Can't write GPSTAG_GPSHPOSITIONINGERROR\n" );
			goto failure;
		}

	} else {
		/*================= Write arbitrary data to the GPS fields ==============*/

		/*-- Get array, where GPS tag fields are defined --*/
		tFieldArray = _TIFFGetGpsFields();
		nTags = tFieldArray->count;

		/*-- TODO: fill in the for / switch part of EXIF writing, when finished and tested!! */

	} /*-- if (blnFillGPSManually) --*/




	/*-- GPS - write custom directory GPS into file...---*/
	/*   (Get back the offset of GPS directory)          */
	if (!TIFFWriteCustomDirectory( tif, &dir_offset_GPS )) {
		fprintf (stderr, "TIFFWriteCustomDirectory() with GPS failed.\n");
		goto failure;
	}

	/*--- CheckpointDirectory at this place generates a second Main-IFD!!! */
	/* retCode = TIFFCheckpointDirectory(tif); */

	/*-- Set / reload previously saved main directory from file ---*/
	if (!TIFFSetDirectory(tif, 0)) {
		fprintf (stderr, "TIFFSetDirectory() within GPS failed.\n");
		goto failure;
	}

	/*-- Write GPS tag reference / offset into GPSIFD tag in main directory --*/
	if (!TIFFSetField(tif, TIFFTAG_GPSIFD, dir_offset_GPS )) {
		fprintf (stderr, "Can't write TIFFTAG_GPSIFD\n");
		goto failure;
	}

 /*===============  END writing GPS tags ==========================*/
#endif	/*-- WRITE_GPS_TAGS --*/


/*================== Write EXIF 2.31 tags =====================*/

	/*-- Set dummy EXIF/GPS tag in original tiff-structure in order to reserve space for final dir_offset value, */
	/*   which is properly written at the end.*/
	/*- We did this already above together with the GPS IFD-tag. Otherwise we would do this here !! --------*/
	/* if (!TIFFSetField(tif, TIFFTAG_EXIFIFD, dir_offset )) {
		fprintf (stderr, "Can't write TIFFTAG_EXIFIFD\n" );
	} 
	*/

#ifdef WRITE_EXIF_TAGS
#define READ_EXIF_TAGS
	/*-- Save current tiff-directory to file before directory is changed. Otherwise it will be lost! 
	 *   The tif-structure is overwritten/ freshly initialized by any "CreateDirectory"
	 */

	/*-----  What is needed here ??? ----
	 * In custom_dir.c only 	TIFFFreeDirectory( tif );   is used to set fields of another Sub-Directory
	 * TIFFFreeDirectory(tif);  *-- Release storage associated with a directory, especially custom-fields.
	 *-- Using only  TIFFFreeDirectory() here leads to an error!!
	 *-- Using here TIFFCheckpointDirectory() leads to an additional Main-IFD ??
	*/
	/*retCode = TIFFCheckpointDirectory(tif);*/	/* does not cleanup Tiff-Structure */
	retCode = TIFFWriteDirectory(tif);			/* cleanup Tiff-structure */

	/*-- Now create an EXIF directory. */
	if (TIFFCreateEXIFDirectory(tif) != 0) {
		fprintf (stderr, "TIFFCreateEXIFDirectory() failed.\n" );
		goto failure;
	}

#define WRITE_ALL_EXIF_TAGS
#ifdef WRITE_ALL_EXIF_TAGS
#define READ_ALL_EXIF_TAGS
	/*================= EXIF: Write arbitrary data to the EXIF fields ==============*/
	/*-- Get array, where EXIF tag fields are defined 
	*    EXIF tags are written automatically with the defined precision according to its tSetFieldType using the code below  --*/
	tFieldArray = _TIFFGetExifFields();
	nTags = tFieldArray->count;

	for (i=0; i<nTags; i++) {
		bool deferredSetField = false;
		tTag = tFieldArray->fields[i].field_tag;
		tType = tFieldArray->fields[i].field_type;				/* e.g. TIFF_RATIONAL */
		tWriteCount = tFieldArray->fields[i].field_writecount;
		tSetFieldType = tFieldArray->fields[i].set_field_type;	/* e.g. TIFF_SETGET_C0_FLOAT */
		tFieldName = tFieldArray->fields[i].field_name;		
		pVoid = NULL;

		/*-- dependent on set_field_type write value --*/
		switch (tSetFieldType)
		{
			case TIFF_SETGET_ASCII:
				/* Either the stringlength is defined as a fixed length in .field_writecount or a NULL-terminated string is used. */
				/* Shorter strings than in auxTextArraxW need a NULL-termination. Therefore copy the string. */
				if (tWriteCount > 0) auxLong = tWriteCount-1; else auxLong = (long)strlen(auxTextArrayW[i])-1;
				strncpy(auxCharArray, auxTextArrayW[i], auxLong);
				auxCharArray[auxLong] = 0;
				if (!TIFFSetField( tif, tTag, auxCharArray)) {
					fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
					goto failure;
				}		
				break;
			case TIFF_SETGET_UINT8:
			case TIFF_SETGET_UINT16:
			case TIFF_SETGET_UINT32:
			case TIFF_SETGET_IFD8:
			case TIFF_SETGET_INT:
				/*-- All those can be written with char, short or long parameter. Only value range should be in line. */
				if (!TIFFSetField( tif, tTag, auxLongArrayW[i])) {
					fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
					goto failure;
				}		
				break;
			case TIFF_SETGET_SINT8:
			case TIFF_SETGET_SINT16:
			case TIFF_SETGET_SINT32:
				/*-- All those can be written with char, short or long parameter. Only value range should be in line. */
				if (!TIFFSetField( tif, tTag, -1.0*auxLongArrayW[i])) {
					fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
					goto failure;
				}		
				break;
			case TIFF_SETGET_FLOAT:
			case TIFF_SETGET_DOUBLE:
				if (tWriteCount == 1) {
					/*-- All single values can be written with float or double parameter. Only value range should be in line. */
					if (!TIFFSetField( tif, tTag, auxDoubleArrayW[i])) {
						fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
						goto failure;
					}	
				} else {
					fprintf (stderr, "WriteCount for .set_field_type %d should be 1!  %s\n", tSetFieldType, tFieldArray->fields[i].field_name);
				}
				break;
			case TIFF_SETGET_C0_FLOAT:
			case TIFF_SETGET_C0_DOUBLE:
			case TIFF_SETGET_C16_FLOAT:
			case TIFF_SETGET_C16_DOUBLE:
			case TIFF_SETGET_C32_FLOAT:
			case TIFF_SETGET_C32_DOUBLE:
				/* _Cxx_ just defines the size of the count parameter for the array as C0=char, C16=short or C32=long */
				/*-- Check, if it is a single parameter, a fixed array or a variable array */
				if (tWriteCount == 1) {
					fprintf (stderr, "WriteCount for .set_field_type %d should be -1 or greater than 1!  %s\n", tSetFieldType, tFieldArray->fields[i].field_name);
				} else {
					/*-- Either fix or variable array --*/
					/* For arrays, distinguishing between float or double is essential, even for writing */
					if (tSetFieldType == TIFF_SETGET_C0_FLOAT || tSetFieldType == TIFF_SETGET_C16_FLOAT || tSetFieldType == TIFF_SETGET_C32_FLOAT)
						 pVoid = &auxFloatArrayW[i]; else pVoid = &auxDoubleArrayW[i];
					/* Now decide between fixed or variable array */
					if (tWriteCount > 1) {
						/* fixed array with needed arraysize defined in .field_writecount */
						if (!TIFFSetField( tif, tTag, pVoid)) {
							fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
							goto failure;
						}	
					} else  {
						/* special treatment of variable array */
						/* for test, use always arraysize of VARIABLE_ARRAY_SIZE */
						if (!TIFFSetField( tif, tTag, VARIABLE_ARRAY_SIZE, pVoid)) {
							fprintf (stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
							goto failure;
						}	
					}
				}
				break;
			case TIFF_SETGET_C0_UINT8:
			case TIFF_SETGET_C0_SINT8:
			case TIFF_SETGET_C16_UINT8: 
			case TIFF_SETGET_C16_SINT8:
			case TIFF_SETGET_C32_UINT8:
			case TIFF_SETGET_C32_SINT8:
					/* For arrays, distinguishing between float or double is essential, even for writing */
					pVoid = &auxCharArrayW[i];
					deferredSetField = true;
					break;
			case TIFF_SETGET_C0_UINT16: 
			case TIFF_SETGET_C0_SINT16: 
			case TIFF_SETGET_C16_UINT16:
			case TIFF_SETGET_C16_SINT16:
			case TIFF_SETGET_C32_UINT16:
			case TIFF_SETGET_C32_SINT16:
					pVoid = &auxShortArrayW[i];
					deferredSetField = true;
					break;
			case TIFF_SETGET_C0_UINT32:
			case TIFF_SETGET_C0_SINT32:
			case TIFF_SETGET_C16_UINT32:
			case TIFF_SETGET_C16_SINT32:
			case TIFF_SETGET_C32_UINT32:
			case TIFF_SETGET_C32_SINT32:
					pVoid = &auxLongArrayW[i];
					deferredSetField = true;
					break;
            default:
                fprintf (stderr, "SetFieldType %d not defined within writing switch for %s.\n", tSetFieldType, tFieldName);
        };  /*-- switch() --*/

		if (deferredSetField) {
			/* _Cxx_ just defines the size of the count parameter for the array as C0=char, C16=short or C32=long */
			/*-- Check, if it is a single parameter, a fixed array or a variable array */
			if (tWriteCount == 1) {
				fprintf(stderr, "WriteCount for .set_field_type %d should be -1 or greater than 1!  %s\n",
						tSetFieldType, tFieldArray->fields[i].field_name);
			} else {
				/*-- Either fix or variable array --*/
				/* Now decide between fixed or variable array */
				if (tWriteCount > 1) {
					/* fixed array with needed arraysize defined in .field_writecount */
					if (!TIFFSetField(tif, tTag, pVoid)) {
						fprintf(stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
						goto failure;
					}
				} else {
					/* special treatment of variable array */
					/* for test, use always arraysize of VARIABLE_ARRAY_SIZE */
					if (!TIFFSetField(tif, tTag, VARIABLE_ARRAY_SIZE, pVoid)) {
						fprintf(stderr, "Can't write %s\n", tFieldArray->fields[i].field_name);
						goto failure;
					}
				}
			}
		}
	} /*-- for() --*/
	/*================= EXIF: END Writing arbitrary data to the EXIF fields END END END ==============*/
#endif  /*-- WRITE_ALL_EXIF_TAGS --*/

	/*--- Set valid EXIF version, which is a 4 byte string --*/
	if (!TIFFSetField( tif, EXIFTAG_EXIFVERSION, exifVersion)) {
		fprintf (stderr, "Can't write EXIFTAG_EXIFVERSION\n" );
		goto failure;
	}		


	/*-- EXIF - write custom directory EXIF into file...---*/
	/*   (Get back the offset of EXIF directory) */
	if (!TIFFWriteCustomDirectory( tif, &dir_offset_EXIF )) {
		fprintf (stderr, "TIFFWriteCustomDirectory() with EXIF failed.\n");
		goto failure;
	}

	/*-- Go back to the first (main) directory, and set correct value of the EXIFIFD pointer.  */
	/*   (directory is reloaded from file!) */
	TIFFSetDirectory(tif, 0);
	TIFFSetField(tif, TIFFTAG_EXIFIFD, dir_offset_EXIF );	
#endif /*-- WRITE_EXIF_TAGS --*/

#ifdef WRITEPIXELLAST
	/*-- Write dummy pixel data. --*/
	if (TIFFWriteScanline(tif, buf, 0, 0) < 0) {
		fprintf (stderr, "Can't write image data.\n");
		goto failure;
	}
#endif
	/*-- Write directory to file --*/
	/* Always WriteDirectory before using/creating another directory. */
	/* Not necessary before TIFFClose(), however, TIFFClose() uses TIFFReWriteDirectory(), which forces directory to be written at another location. */
	retCode = TIFFWriteDirectory(tif);

	/*-- Write File to disk and close file --*/
	/* TIFFClose() uses TIFFReWriteDirectory(), which forces directory to be written at another location. */
	/* Therefore, better use TIFFWriteDirectory() before. */
	TIFFClose(tif);

	fprintf (stderr, "-------- Continue Test  ---------- reading ...\n");

/*=========================  READING  =============  READING  ========================================*/
	/* Ok, now test whether we can read written values correctly. */
	tif = TIFFOpen(filenameRead, "r");
	

	/*-- Read some parameters out of the main directory --*/

	/*-- IMAGEWIDTH and -LENGTH are defined as TIFF_SETGET_UINT32 */
	retCode = TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &auxUint32 );
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_IMAGEWIDTH"); }
	if (auxUint32 != width) {
		fprintf (stderr, "Read value of IMAGEWIDTH %"PRIu32" differs from set value %"PRIu16"\n", auxUint32, width);
	}
	retCode = TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &auxUint32 );
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_IMAGELENGTH"); }
	if (auxUint32 != width) {
		fprintf (stderr, "Read value of TIFFTAG_IMAGELENGTH %"PRIu32" differs from set value %"PRIu16"\n", auxUint32, length);
	}

#ifdef ADDITIONAL_TAGS
	/*- TIFFTAG_PIXAR_FOVCOT is a FLOAT parameter of type  FIELD_CUSTOM !! */
	retCode = TIFFGetField(tif, TIFFTAG_PIXAR_FOVCOT, &auxFloat ); 
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_PIXAR_FOVCOT"); }
	if (auxFloat != (float)PIXAR_FOVCOT_VAL) {
		fprintf (stderr, "Read value of TIFFTAG_PIXAR_FOVCOT %f differs from set value %f\n", auxFloat, PIXAR_FOVCOT_VAL);
	}

	/* - TIFFTAG_BESTQUALITYSCALE is a Rational parameter, FIELD_CUSTOM and TIFF_SETGET_FLOAT  */
	retCode = TIFFGetField(tif, TIFFTAG_BESTQUALITYSCALE, &auxFloat );
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_BESTQUALITYSCALE"); }
	if (auxFloat != (float)BESTQUALITYSCALE_VAL) {
		fprintf (stderr, "Read value of TIFFTAG_BESTQUALITYSCALE %f differs from set value %f\n", auxFloat, BESTQUALITYSCALE_VAL);
	}

	/* - TIFFTAG_BASELINENOISE, 1, 1, TIFF_RATIONAL, 0, TIFF_SETGET_FLOAT */
	retCode = TIFFGetField(tif, TIFFTAG_BASELINENOISE, &auxDblUnion.dbl);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_BASELINENOISE"); }
	if (auxDblUnion.flt1 != (float)BESTQUALITYSCALE_VAL) {
		fprintf(stderr, "Read float value of TIFFTAG_BASELINENOISE %f differs from set value %f\n", auxDblUnion.flt1, BESTQUALITYSCALE_VAL);
	}


	/*- Variable Array: TIFFTAG_DECODE is a SRATIONAL parameter TIFF_SETGET_C16_FLOAT type FIELD_CUSTOM with passcount=1 and variable length of array. */
	retCode = TIFFGetField(tif, TIFFTAG_DECODE, &count16, &pVoidArray );
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_DECODE"); }
	/*- pVoidArray points to a Tiff-internal temporary memorypart. Thus, contents needs to be saved. */
	memcpy(&auxFloatArray, pVoidArray,(count16 * sizeof(auxFloatArray[0])));
	for (i=0; i<count16; i++) {
		dblDiffLimit = RATIONAL_EPS*auxFloatArrayN2[i];
		dblDiff = auxFloatArray[i] - auxFloatArrayN2[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf (stderr, "Read value %d of TIFFTAG_DECODE Array %f differs from set value %f\n", i, auxFloatArray[i], auxFloatArrayN2[i]);
		}
	}

	retCode = TIFFGetField(tif, TIFFTAG_BLACKLEVEL, &count16, &pVoidArray);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_BLACKLEVEL"); }
	/*- pVoidArray points to a Tiff-internal temporary memorypart. Thus, contents needs to be saved. */
	memcpy(&auxFloatArray, pVoidArray, (count16 * sizeof(auxFloatArray[0])));
	for (i = 0; i<count16; i++) {
		dblDiffLimit = RATIONAL_EPS*auxFloatArrayN1[i];
		dblDiff = auxFloatArray[i] - auxFloatArrayN1[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf(stderr, "Read value %d of TIFFTAG_BLACKLEVEL Array %f differs from set value %f\n", i, auxFloatArray[i], auxFloatArrayN1[i]);
		}
	}

	/*- Fixed Array: TIFFTAG_DEFAULTCROPSIZE, 2, 2, TIFF_RATIONAL, 0, TIFF_SETGET_C0_FLOAT */
	retCode = TIFFGetField(tif, TIFFTAG_DEFAULTCROPSIZE, &pVoidArray);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFTAG_DEFAULTCROPSIZE"); }
	/*- pVoidArray points to a Tiff-internal temporary memorypart. Thus, contents needs to be saved. */
	memcpy(&auxFloatArray, pVoidArray, (2 * sizeof(auxFloatArray[0])));
	for (i = 0; i < 2; i++) {
		dblDiffLimit = RATIONAL_EPS * auxFloatArrayW[i];
		dblDiff = auxFloatArray[i] - auxFloatArrayW[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf(stderr, "Read value %d of TIFFTAG_DEFAULTCROPSIZE Array %f differs from set value %f\n", i, auxFloatArray[i], auxFloatArrayW[i]);
		}
	}

#endif	/*-- ADDITIONAL_TAGS --*/


#ifdef READ_GPS_TAGS
/*================== Reading GPS tags =====================*/
	/*-- First get offset to GPS-directory and set it active (this will destroy previously main directory fields in memory!) */
	retCode = TIFFGetField(tif, TIFFTAG_GPSIFD, &read_dir_offset );
	if (!retCode) {fprintf(stderr, "Can't read %s\n", "TIFFTAG_GPSIFD"); }
	retCode = TIFFReadGPSDirectory(tif, read_dir_offset);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "TIFFReadGPSDirectory()"); }

	/*-- Now read some parameters from GPS-directory --*/

	/*-- Fixed Array: GPS-Version is a fixed array (of 4 characters) */
	retCode = TIFFGetField(tif, GPSTAG_VERSIONID, &pGpsVersion);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_VERSIONID"); }
	else {
		memcpy(auxCharArray, pGpsVersion, sizeof(gpsVersion));
		for (i = 0; i < 4; i++) {
			if (auxCharArray[i] != pGpsVersion[i]) {
				fprintf(stderr, "Read value %d of GPSTAG_VERSIONID %d differs from set value %d\n", i, auxCharArray[i], pGpsVersion[i]);
			}
		}
	}
	/*-- LATITUDEREF is a fixed String of one character plus ending zero. */
	retCode = TIFFGetField(tif, GPSTAG_LATITUDEREF, &pAscii);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_LATITUDEREF"); }
	retCode2 = strncmp("N", pAscii, 1);
	if (retCode2 != 0) {
		fprintf (stderr, "Read value %d of GPSTAG_LATITUDEREF %s differs from set value %s\n", i, "N", pAscii);
	}

	/*-- Fixed Array: Latitude is an array of 3 Rational-values. TIFFGetField() returns a pointer to a temporary float-/double-array. */
	/*-- ATTENTION: After the upgrade with Rational2Double, the GPSTAG values are defined as double precision 
	 *              and need to be written and also read in double precision!
	 *              In order to maintain this code for both cases, it is checked above if the TiffLibrary is
	 *              compiled with the new interface with Rational2Double or still uses the old definitions,
	 *              by setting blnIsRational2Double above.
	 */
	if (blnIsRational2Double) {
		fprintf(stderr, "-- GPS tags are read using Rational2Double --\n");
	} else {
		fprintf(stderr, "-- GPS tags are read using standard --\n");
	}
	retCode = TIFFGetField(tif, GPSTAG_LATITUDE, &pVoidArray);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_LATITUDE"); }
	if (!blnIsRational2Double) {
		/* Reset arrays for debugging purpose first */
		memset(auxFloatArray, 0, sizeof(auxFloatArray));
		memcpy(auxFloatArray, pVoidArray, 3*sizeof(float));
		/* for comparison copy to doubleArray */
		for (i=0; i<3; i++) auxDoubleArray[i] = (double)auxFloatArray[i];
	} else {
		/* Rational2Double interface for GPSTAG reads double array */
		memset(auxDoubleArray, 0, sizeof(auxDoubleArray));
		memcpy(auxDoubleArray, pVoidArray, 3 * sizeof(double));
	}
	for (i=0; i<3; i++) {
		dblDiffLimit = RATIONAL_EPS*auxDoubleArrayGPS1[i];
		dblDiff = auxDoubleArray[i] - auxDoubleArrayGPS1[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf (stderr, "Read value %d of GPSTAG_LATITUDE %f differs from set value %f\n", i, auxDoubleArray[i], auxDoubleArrayGPS1[i]);
		}
	}

	/*-- LONGITUDEREF is a fixed String of one character plus ending zero. */
	retCode = TIFFGetField(tif, GPSTAG_LONGITUDEREF, &pAscii);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_LONGITUDEREF"); }
	retCode2 = strncmp("W", pAscii, 1);
	if (retCode2 != 0) {
		fprintf(stderr, "Read value %d of GPSTAG_LONGITUDEREF %s differs from set value %s\n", i, "W", pAscii);
	}

	retCode = TIFFGetField(tif, GPSTAG_LONGITUDE, &pVoidArray);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_LONGITUDE"); }
	if (!blnIsRational2Double) {
		/* Reset arrays for debugging purpose first */
		memset(auxFloatArray, 0, sizeof(auxFloatArray));
		memcpy(auxFloatArray, pVoidArray, 3 * sizeof(float));
		/* for comparison copy to doubleArray */
		for (i = 0; i < 3; i++) auxDoubleArray[i] = (double)auxFloatArray[i];
	} else {
		/* Rational2Double interface for GPSTAG reads double array */
		memset(auxDoubleArray, 0, sizeof(auxDoubleArray));
		memcpy(auxDoubleArray, pVoidArray, 3 * sizeof(double));
	}
	for (i = 0; i < 3; i++) {
		dblDiffLimit = RATIONAL_EPS * auxDoubleArrayGPS2[i];
		dblDiff = auxDoubleArray[i] - auxDoubleArrayGPS2[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf(stderr, "Read value %d of GPSTAG_LONGITUDE %f differs from set value %f\n", i, auxDoubleArray[i], auxDoubleArrayGPS2[i]);
		}
	}

	/* TIFF_RATIONAL, TIFF_SETGET_DOUBLE */
	if (!TIFFGetField(tif, GPSTAG_ALTITUDE, &auxDblUnion.dbl)) {
		fprintf(stderr, "Can't read GPSTAG_ALTITUDE\n");
		GOTOFAILURE_GPS
	}
	if (blnIsRational2Double) {
		/* New interface allows also double precision for TIFF_RATIONAL */
		auxDouble = auxDblUnion.dbl;
	} else {
		/* Old interface reads TIFF_RATIONAL defined as TIFF_SETGET_DOUBLE always as FLOAT */
		auxDouble = (double)auxDblUnion.flt1;
	}
	/* compare read values with written ones */
	dblDiffLimit = RATIONAL_EPS * auxDoubleGPSAltitude;
	dblDiff = auxDouble - auxDoubleGPSAltitude;
	if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf(stderr, "Read value of GPSTAG_ALTITUDE %f differs from set value %f\n", auxDouble, auxDoubleGPSAltitude);
			GOTOFAILURE_GPS
	}

	/*-- TimeStamp is only hh:mm:ss. See also DateTime string  3, TIFF_RATIONAL, TIFF_SETGET_C0_DOUBLE */
	retCode = TIFFGetField(tif, GPSTAG_TIMESTAMP, &pVoidArray);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_TIMESTAMP"); }
	if (!blnIsRational2Double) {
		/* Reset arrays for debugging purpose first */
		memset(auxFloatArray, 0, sizeof(auxFloatArray));
		memcpy(auxFloatArray, pVoidArray, 3 * sizeof(float));
		/* for comparison copy to doubleArray */
		for (i = 0; i < 3; i++) auxDoubleArray[i] = (double)auxFloatArray[i];
	} else {
		/* Rational2Double interface for GPSTAG reads double array */
		memset(auxDoubleArray, 0, sizeof(auxDoubleArray));
		memcpy(auxDoubleArray, pVoidArray, 3 * sizeof(double));
	}
	for (i = 0; i < 3; i++) {
		dblDiffLimit = RATIONAL_EPS * auxDoubleArrayGPSTime[i];
		dblDiff = auxDoubleArray[i] - auxDoubleArrayGPSTime[i];
		if (fabs(dblDiff) > fabs(dblDiffLimit)) {
			fprintf(stderr, "Read value %d of GPSTAG_TIMESTAMP %f differs from set value %f\n", i, auxDoubleArray[i], auxDoubleArrayGPS2[i]);
			GOTOFAILURE_GPS
		}
	}

	/* GPSTAG_IMGDIRECTION --- TIFF_RATIONAL, TIFF_SETGET_DOUBLE */
	if (!TIFFGetField(tif, GPSTAG_IMGDIRECTION, &auxDblUnion.dbl)) {
		fprintf(stderr, "Can't read GPSTAG_IMGDIRECTION\n");
		GOTOFAILURE_GPS
	}
	if (blnIsRational2Double) {
		/* New interface allows also double precision for TIFF_RATIONAL */
		auxDouble = auxDblUnion.dbl;
	} else {
		/* Old interface reads TIFF_RATIONAL defined as TIFF_SETGET_DOUBLE always as FLOAT */
		auxDouble = (double)auxDblUnion.flt1;
	}
	/* compare read values with written ones */
	dblDiffLimit = RATIONAL_EPS * auxDoubleGPSDirection;
	dblDiff = auxDouble - auxDoubleGPSDirection;
	if (fabs(dblDiff) > fabs(dblDiffLimit)) {
		fprintf(stderr, "Read value of GPSTAG_IMGDIRECTION %f differs from set value %f\n", auxDouble, auxDoubleGPSDirection);
		GOTOFAILURE_GPS
	}

	/*-- GPSTAG_DIFFERENTIAL	, 1, 1,	TIFF_SHORT	, 0, 	TIFF_SETGET_UINT16 */
	retCode = TIFFGetField(tif, GPSTAG_DIFFERENTIAL, &auxShort);
	if (!retCode) { fprintf(stderr, "Can't read %s\n", "GPSTAG_DIFFERENTIAL"); }
	if (auxShort != auxShortArrayW[5]) {
		fprintf(stderr, "Read value of GPSTAG_DIFFERENTIAL %d differs from set value %d\n", auxShort, auxShortArrayW[5]);
		GOTOFAILURE_GPS
	}

	/*-- GPSHPOSITIONINGERROR - new tag for EXIF 2.31 --*/
	if (!TIFFGetField(tif, GPSTAG_GPSHPOSITIONINGERROR, &auxDblUnion.dbl)) {
		fprintf(stderr, "Can't read GPSTAG_GPSHPOSITIONINGERROR\n");
		GOTOFAILURE_GPS
	}
	if (blnIsRational2Double) {
		/* New interface allows also double precision for TIFF_RATIONAL */
		auxDouble = auxDblUnion.dbl;
	} else {
		/* Old interface reads TIFF_RATIONAL defined as TIFF_SETGET_DOUBLE always as FLOAT */
		auxDouble = (double)auxDblUnion.flt1;
	}
	/* compare read values with written ones */
	auxFloat = (float)GPSHPOSITIONINGERROR_VAL;
	dblDiffLimit = RATIONAL_EPS * auxFloat;
	dblDiff = auxDouble - auxFloat;
	if (fabs(dblDiff) > fabs(dblDiffLimit)) {
		fprintf(stderr, "Read value of GPSTAG_GPSHPOSITIONINGERROR %f differs from set value %f\n", auxDouble, auxFloat);
		GOTOFAILURE_GPS
	}

 /*===============  END reading GPS tags ==========================*/
#endif /*-- READ_GPS_TAGS --*/



/*================== Reading EXIF 2.31 tags =====================*/

	/*--- Firstly, get EXIF directory offset from main directory. */

	/*-- Go back to the first (main) directory, and get value of the EXIFIFD directory- offset.  */
	/*   (directory is reloaded from file!) */
	TIFFSetDirectory(tif, 0);
	retCode = TIFFGetField(tif, TIFFTAG_EXIFIFD, &read_dir_offset );

#ifdef READ_EXIF_TAGS
	/*-- Now read EXIF directory from file into memory --*/
	retCode = TIFFReadEXIFDirectory(tif, read_dir_offset);

	/*-- Now get some parameters from EXIF-directory (already read into memory) --*/
	retCode = TIFFGetField(tif, EXIFTAG_EXIFVERSION, &pAscii);


#ifdef READ_ALL_EXIF_TAGS
	/*-- Get array, where EXIF tag fields are defined --*/
	tFieldArray = _TIFFGetExifFields();
	nTags = tFieldArray->count;
	/*-- Check, if the TiffLibrary is compiled with the new interface with Rational2Double or still uses the old definitions. */
	/* tif points to EXIF tags, so TIFFFindField() can only access the EXIF tag fields */
	fip = TIFFFindField(tif, EXIFTAG_EXPOSURETIME, TIFF_ANY);
	tSetFieldType = fip->set_field_type;
	if (tSetFieldType == TIFF_SETGET_DOUBLE) {
		blnIsRational2Double = FALSE;
		fprintf(stderr, "-- EXIF tags read with standard --\n");
	} else {
		blnIsRational2Double = TRUE;
		fprintf(stderr, "-- Rational2Double for reading EXIF tags detected --\n");
	}

	for (i=0; i<nTags; i++) {
        bool deferredSetField = false;
		tTag = tFieldArray->fields[i].field_tag;
		tType = tFieldArray->fields[i].field_type;				/* e.g. TIFF_RATIONAL */
		tWriteCount = tFieldArray->fields[i].field_writecount;
		tSetFieldType = tFieldArray->fields[i].set_field_type;	/* e.g. TIFF_SETGET_C0_FLOAT */
		tFieldName = tFieldArray->fields[i].field_name;		
		pVoid = NULL;

		/*-- dependent on set_field_type read value --*/
		switch (tSetFieldType)
		{
			case TIFF_SETGET_ASCII:
				/* Either the stringlength is defined as a fixed length in .field_writecount or a NULL-terminated string is used. */
				if (!TIFFGetField( tif, tTag, &pAscii)) {
					fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				/* Save string from temporary buffer and compare with written string. */
				strncpy(auxCharArray, pAscii, sizeof(auxCharArray) - 1u);
                auxCharArray[sizeof(auxCharArray) - 1u] = '\0';
				if (tWriteCount > 0) auxLong = tWriteCount-1; else auxLong = (long)strlen(auxCharArray);
				retCode2 = strncmp(auxCharArray, auxTextArrayW[i], auxLong);
				if (retCode2 != 0) {
					fprintf (stderr, "%d:Read value of %s %s differs from set value %s\n", i, tFieldName, auxCharArray, auxTextArrayW[i]);
					GOTOFAILURE_ALL_EXIF
				}
				break;
				/*-- For reading, the parameter size is to be observed !! */
			case TIFF_SETGET_UINT8:
			case TIFF_SETGET_SINT8:
				if (!TIFFGetField( tif, tTag, &auxChar)) {
					fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				/* compare read values with written ones */
				auxLong = auxChar;
				if (auxLong != (char)auxLongArrayW[i]) {
					fprintf (stderr, "%d:Read value of %s %ld differs from set value %ld\n", i, tFieldName, auxLong, auxLongArrayW[i]);
				}
				break;
			case TIFF_SETGET_UINT16:
			case TIFF_SETGET_SINT16:
				if (!TIFFGetField( tif, tTag, &auxShort)) {
					fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				/* compare read values with written ones */
				auxLong = auxShort;	
				if (auxLong != (short)auxLongArrayW[i]) {
					fprintf (stderr, "%d:Read value of %s %ld differs from set value %ld\n", i, tFieldName, auxLong, auxLongArrayW[i]);
				}
				break;
			case TIFF_SETGET_UINT32:
			case TIFF_SETGET_SINT32:
			case TIFF_SETGET_IFD8:
			case TIFF_SETGET_INT:
				if (!TIFFGetField( tif, tTag, &auxUint32)) {
					fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				/* compare read values with written ones */
				auxLong = auxUint32;
				if (auxLong != auxLongArrayW[i]) {
					fprintf (stderr, "%d:Read value of %s %ld differs from set value %ld\n", i, tFieldName, auxLong, auxLongArrayW[i]);
				}
				break;
			case TIFF_SETGET_FLOAT:
				if (!TIFFGetField( tif, tTag, &auxFloat)) {
					fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				/* compare read values with written ones */
				if (tType == TIFF_RATIONAL || tType == TIFF_SRATIONAL) dblDiffLimit = RATIONAL_EPS*auxDoubleArrayW[i]; else dblDiffLimit = 1e-6;
				dblDiff = auxFloat - auxDoubleArrayW[i];
				if (fabs(dblDiff) > fabs(dblDiffLimit)) {
					/*--: EXIFTAG_SUBJECTDISTANCE: LibTiff returns value of "-1.0" if numerator equals 4294967295 (0xFFFFFFFF) to indicate infinite distance!
					 * However, there are two other EXIF tags where numerator indicates a special value and six other cases where the denominator indicates special values,
					 * which are not treated within LibTiff!!
					*/
					if (!(tTag == EXIFTAG_SUBJECTDISTANCE && auxFloat == -1.0)) {
						fprintf (stderr, "%d:Read value of %s %f differs from set value %f\n", i, tFieldName, auxFloat, auxDoubleArrayW[i]);
						GOTOFAILURE_ALL_EXIF
					}
				}
				break;
			case TIFF_SETGET_DOUBLE:
				/*-- Unfortunately, TIFF_SETGET_DOUBLE is used for TIFF_RATIONAL but those have to be read with FLOAT !!! */
				/*   Only TIFFTAG_STONITS is a TIFF_DOUBLE, which has to be read as DOUBLE!! */
				/*-- ATTENTION: ----
				 *   Only after update with Rational2Double feature, also TIFF_RATIONAL can be read in double precision!!! 
				 *   Therefore, use a union to avoid overflow in TIFFGetField() return value
				 *   and depending on version check for the right interface here:
				 *   - old interface:  correct value should be here a float
				 *   - new interface:  correct value should be here a double
				 *   Interface version (old/new) is determined above.
				 -------------------*/
				if (!TIFFGetField(tif, tTag, &auxDblUnion.dbl)) {
					fprintf(stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
					GOTOFAILURE_ALL_EXIF
					break;
				}
				if (tType == TIFF_RATIONAL || tType == TIFF_SRATIONAL) {
					if (blnIsRational2Double) {
						/* New interface allows also double precision for TIFF_RATIONAL */
						auxDouble = auxDblUnion.dbl;
					}
					else {
						/* Old interface reads TIFF_RATIONAL defined as TIFF_SETGET_DOUBLE always as FLOAT */
						auxDouble = (double)auxDblUnion.flt1;
					}
				}
				else {
					auxDouble = auxDblUnion.dbl;
				}
				/* compare read values with written ones */
				if (tType == TIFF_RATIONAL || tType == TIFF_SRATIONAL) dblDiffLimit = RATIONAL_EPS*auxDoubleArrayW[i]; else dblDiffLimit = 1e-6;
				dblDiff = auxDouble - auxDoubleArrayW[i];
				if (fabs(dblDiff) > fabs(dblDiffLimit)) {
					/*--: EXIFTAG_SUBJECTDISTANCE: LibTiff returns value of "-1.0" if numerator equals 4294967295 (0xFFFFFFFF) to indicate infinite distance! */
					if (!(tTag == EXIFTAG_SUBJECTDISTANCE && auxDouble == -1.0)) {
						fprintf (stderr, "%d:Read value of %s %f differs from set value %f\n", i, tFieldName, auxDouble, auxDoubleArrayW[i]);
						GOTOFAILURE_ALL_EXIF
					}
				}
				break;

			case TIFF_SETGET_C0_FLOAT:
			case TIFF_SETGET_C0_DOUBLE:
			case TIFF_SETGET_C16_FLOAT:
			case TIFF_SETGET_C16_DOUBLE:
			case TIFF_SETGET_C32_FLOAT:
			case TIFF_SETGET_C32_DOUBLE:
				/* _Cxx_ just defines the size of the count parameter for the array as C0=char, C16=short or C32=long */
				/*-- Check, if it is a single parameter, a fixed array or a variable array */
				if (tWriteCount == 1) {
					fprintf (stderr, "Reading: WriteCount for .set_field_type %d should be -1 or greater than 1!  %s\n", tSetFieldType, tFieldArray->fields[i].field_name);
				} else {
					/*-- Either fix or variable array --*/
					/* For arrays, distinguishing between float or double is essential. */
					/* Now decide between fixed or variable array */
					if (tWriteCount > 1) {
						/* fixed array with needed arraysize defined in .field_writecount */
						if (!TIFFGetField( tif, tTag, &pVoidArray)) {
							fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
							GOTOFAILURE_ALL_EXIF
							break;
						}	
						/* set tWriteCount to number of read samples for next steps */
						auxLong = tWriteCount;
					} else  {
						/* Special treatment of variable array. */
						/* Dependent on Cxx, the count parameter is char, short or long. Therefore use unionLong! */
						if (!TIFFGetField( tif, tTag, &unionLong, &pVoidArray)) {
							fprintf (stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
							GOTOFAILURE_ALL_EXIF
							break;
						}	
						/* set tWriteCount to number of read samples for next steps */
						auxLong = unionLong.Short1;
					}
					/* Save values from temporary array */
					if (tSetFieldType == TIFF_SETGET_C0_FLOAT || tSetFieldType == TIFF_SETGET_C16_FLOAT || tSetFieldType == TIFF_SETGET_C32_FLOAT) {
						memcpy(&auxFloatArray, pVoidArray,(auxLong * sizeof(auxFloatArray[0])));
						/* compare read values with written ones */
						if (tType == TIFF_RATIONAL || tType == TIFF_SRATIONAL) dblDiffLimit = RATIONAL_EPS*auxDoubleArrayW[i]; else dblDiffLimit = 1e-6;
						for (j=0; j<auxLong; j++) {
							dblDiff = auxFloatArray[j] - auxFloatArrayW[i+j];
							if (fabs(dblDiff) > fabs(dblDiffLimit)) {
							/*if (auxFloatArray[j] != (float)auxFloatArrayW[i+j]) { */
								fprintf (stderr, "Read value %d of %s #%d %f differs from set value %f\n", i, tFieldName, j, auxFloatArray[j], auxFloatArrayW[i+j]);
								GOTOFAILURE_ALL_EXIF
							}
						}
					} else {
						memcpy(&auxDoubleArray, pVoidArray,(auxLong * sizeof(auxDoubleArray[0])));
						/* compare read values with written ones */
						if (tType == TIFF_RATIONAL || tType == TIFF_SRATIONAL) dblDiffLimit = RATIONAL_EPS*auxDoubleArrayW[i]; else dblDiffLimit = 1e-6;
						for (j=0; j<auxLong; j++) {
							dblDiff = auxDoubleArray[j] - auxDoubleArrayW[i+j];
							if (fabs(dblDiff) > fabs(dblDiffLimit)) {
							/*if (auxDoubleArray[j] != auxDoubleArrayW[i+j]) { */
								fprintf (stderr, "Read value %d of %s #%d %f differs from set value %f\n", i, tFieldName, j, auxDoubleArray[j], auxDoubleArrayW[i+j]);
								GOTOFAILURE_ALL_EXIF
							}
						}
					}
				}
				break;
			case TIFF_SETGET_C0_UINT8:
			case TIFF_SETGET_C0_SINT8:
			case TIFF_SETGET_C16_UINT8: 
			case TIFF_SETGET_C16_SINT8:
			case TIFF_SETGET_C32_UINT8:
			case TIFF_SETGET_C32_SINT8:
					/* For arrays, distinguishing between float or double is essential, even for writing */
					pVoid = &auxCharArrayW[i];
					deferredSetField = true;
					break;
			case TIFF_SETGET_C0_UINT16:
			case TIFF_SETGET_C0_SINT16: 
			case TIFF_SETGET_C16_UINT16:
			case TIFF_SETGET_C16_SINT16:
			case TIFF_SETGET_C32_UINT16:
			case TIFF_SETGET_C32_SINT16:
					pVoid = &auxShortArrayW[i];
					deferredSetField = true;
break;
			case TIFF_SETGET_C0_UINT32:
			case TIFF_SETGET_C0_SINT32:
			case TIFF_SETGET_C16_UINT32:
			case TIFF_SETGET_C16_SINT32:
			case TIFF_SETGET_C32_UINT32:
			case TIFF_SETGET_C32_SINT32:
					pVoid = &auxLongArrayW[i];
					deferredSetField = true;
					break;
			default:
				fprintf (stderr, "SetFieldType %d not defined within writing switch for %s.\n", tSetFieldType, tFieldName);
				GOTOFAILURE
		};  /*-- switch() --*/

		if(deferredSetField) {
			/* _Cxx_ just defines the size of the count parameter for the array as C0=char, C16=short or C32=long */
			/*-- Check, if it is a single parameter, a fixed array or a variable array */
			if (tWriteCount == 1) {
				fprintf(stderr, "WriteCount for .set_field_type %d should be -1 or greater than 1!  %s\n",
						tSetFieldType, tFieldArray->fields[i].field_name);
			} else {
				/*-- Either fix or variable array --*/
				/* Now decide between fixed or variable array */
				if (tWriteCount > 1) {
					/* fixed array with needed arraysize defined in .field_writecount */
					if (!TIFFGetField(tif, tTag, &pVoidArray)) {
						fprintf(stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
						GOTOFAILURE_ALL_EXIF
						break;
					}
					/* set tWriteCount to number of read samples for next steps */
					auxLong = tWriteCount;
				} else {
					/* special treatment of variable array */
					/* for test, use always arraysize of VARIABLE_ARRAY_SIZE */
					if (!TIFFGetField(tif, tTag, &unionLong, &pVoidArray)) {
						fprintf(stderr, "Can't read %s\n", tFieldArray->fields[i].field_name);
						GOTOFAILURE_ALL_EXIF
						break;
					}
					/* set tWriteCount to number of read samples for next steps */
					auxLong = unionLong.Short1;
				}
				/* Save values from temporary array */
				if (tSetFieldType == TIFF_SETGET_C0_UINT8 || tSetFieldType == TIFF_SETGET_C0_SINT8 ||
					tSetFieldType == TIFF_SETGET_C16_UINT8 || tSetFieldType == TIFF_SETGET_C16_SINT8 ||
					tSetFieldType == TIFF_SETGET_C32_UINT8 || tSetFieldType == TIFF_SETGET_C32_SINT8) {
					memcpy(&auxCharArray, pVoidArray, (auxLong * sizeof(auxCharArray[0])));
					/* Compare and check values  */
					for (j = 0; j < auxLong; j++) {
						if (tTag == EXIFTAG_EXIFVERSION) {
							/*-- Use exifVersion[] instead of auxCharArrayW[] for differently set EXIFVERSION tag */
							if (auxCharArray[j] != exifVersion[j]) {
								fprintf(stderr, "Read value %d of %s #%d %d differs from set value %d\n", i, tFieldName,
										j, auxCharArray[j], auxCharArrayW[i + j]);
								GOTOFAILURE_ALL_EXIF
							}
						} else {
							if (auxCharArray[j] != auxCharArrayW[i + j]) {
								fprintf(stderr, "Read value %d of %s #%d %d differs from set value %d\n", i, tFieldName,
										j, auxCharArray[j], auxCharArrayW[i + j]);
								GOTOFAILURE_ALL_EXIF
							}
						}
					}
				} else if (tSetFieldType == TIFF_SETGET_C0_UINT16 || tSetFieldType == TIFF_SETGET_C0_SINT16 ||
						   tSetFieldType == TIFF_SETGET_C16_UINT16 || tSetFieldType == TIFF_SETGET_C16_SINT16 ||
						   tSetFieldType == TIFF_SETGET_C32_UINT16 || tSetFieldType == TIFF_SETGET_C32_SINT16) {
					memcpy(&auxShortArray, pVoidArray, (auxLong * sizeof(auxShortArray[0])));
					/* Compare and check values  */
					for (j = 0; j < auxLong; j++) {
						if (auxShortArray[j] != auxShortArrayW[i + j]) {
							fprintf(stderr, "Read value %d of %s #%d %d differs from set value %d\n", i, tFieldName, j,
									auxShortArray[j], auxShortArrayW[i + j]);
							GOTOFAILURE_ALL_EXIF
						}
					}
				} else if (tSetFieldType == TIFF_SETGET_C0_UINT32 || tSetFieldType == TIFF_SETGET_C0_SINT32 ||
						   tSetFieldType == TIFF_SETGET_C16_UINT32 || tSetFieldType == TIFF_SETGET_C16_SINT32 ||
						   tSetFieldType == TIFF_SETGET_C32_UINT32 || tSetFieldType == TIFF_SETGET_C32_SINT32) {
					memcpy(&auxLongArray, pVoidArray, (auxLong * sizeof(auxLongArray[0])));
					/* Compare and check values  */
					for (j = 0; j < auxLong; j++) {
						if (auxLongArray[j] != auxLongArrayW[i + j]) {
							fprintf(stderr, "Read value %d of %s #%d %ld differs from set value %ld\n", i, tFieldName,
									j, auxLongArray[j], auxLongArrayW[i + j]);
							GOTOFAILURE_ALL_EXIF
						}
					}
				} else {
					fprintf(stderr, "SetFieldType %d not defined within switch case reading for UINT for %s.\n",
							tSetFieldType, tFieldName);
					GOTOFAILURE
				}
			}
		}
	} /*-- for() --*/
	/*================= EXIF: END Reading arbitrary data to the EXIF fields END END END ==============*/
#endif /*-- READ_ALL_EXIF_TAGS --*/
#endif /*-- READ_EXIF_TAGS --*/




	TIFFClose(tif);
	
	/* All tests passed; delete file and exit with success status. */
#ifdef FOR_AUTO_TESTING
	unlink(filenameRead);
#endif
	fprintf(stderr, "-------- Test finished OK ----------\n");
	return 0;

failure:
	/* 
	 * Something goes wrong; close file and return unsuccessful status.
	 * Do not remove the file for further manual investigation.
	 */
	TIFFClose(tif);
	fprintf(stderr, "-------- Test finished with FAILURE --------\n");
	return 1;
}
