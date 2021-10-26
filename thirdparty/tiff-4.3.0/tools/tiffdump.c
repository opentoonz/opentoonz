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

#include "tiffiop.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_IO_H
# include <io.h>
#endif

#include "tiffio.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef O_BINARY
# define O_BINARY	0
#endif

static union
{
	TIFFHeaderClassic classic;
	TIFFHeaderBig big;
	TIFFHeaderCommon common;
} hdr;
static char* appname;
static char* curfile;
static int swabflag;
static int bigendian;
static int bigtiff;
static uint32_t maxitems = 24;   /* maximum indirect data items to print */

static const char bytefmt[] = "%s%#02" PRIx8;	/* BYTE */
static const char sbytefmt[] = "%s%" PRId8;		/* SBYTE */
static const char shortfmtd[] = "%s%" PRIu16;		/* SHORT */
static const char shortfmth[] = "%s%#" PRIx16;
static const char sshortfmtd[] = "%s%" PRId16;	/* SSHORT */
static const char sshortfmth[] = "%s%#" PRIx16;
static const char longfmtd[] = "%s%" PRIu32;		/* LONG */
static const char longfmth[] = "%s%#" PRIx32;
static const char slongfmtd[] = "%s%" PRId32;	/* SLONG */
static const char slongfmth[] = "%s%#" PRIx32;
static const char ifdfmt[] = "%s%#04" PRIx32;	/* IFD offset */
static const char long8fmt[] = "%s%" PRIu64;	/* LONG8 */
static const char slong8fmt[] = "%s%" PRId64;	/* SLONG8 */
static const char ifd8fmt[] = "%s%#08" PRIx64;	/* IFD offset8 */
static const char rationalfmt[] = "%s%g";	/* RATIONAL */
static const char srationalfmt[] = "%s%g";	/* SRATIONAL */
static const char floatfmt[] = "%s%g";		/* FLOAT */
static const char doublefmt[] = "%s%g";		/* DOUBLE */

unsigned int hex_mode;

static void dump(int, uint64_t);

#if !HAVE_DECL_OPTARG
extern int optind;
extern char* optarg;
#endif

void
usage()
{
	fprintf(stderr, "\nDisplay directory information from TIFF files\n\n");
	fprintf(stderr, "usage: %s [-h] [-o offset] [-m maxitems] file.tif ...\n", appname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char* argv[])
{
	int one = 1, fd;
	int multiplefiles = (argc > 1);
	int c;
	uint64_t diroff = 0;
        hex_mode=0;
	bigendian = (*(char *)&one == 0);

	appname = argv[0];
	while ((c = getopt(argc, argv, "m:o:h")) != -1) {
		switch (c) {
		case 'h':  /* print values in hex */
                        hex_mode=1;
			break;
		case 'o':
			diroff = (uint64_t) strtoul(optarg, NULL, 0);
			break;
		case 'm':
			maxitems = strtoul(optarg, NULL, 0);
			break;
		default:
			usage();
		}
	}
	if (optind >= argc)
		usage();
	for (; optind < argc; optind++) {
		fd = open(argv[optind], O_RDONLY|O_BINARY, 0);
		if (fd < 0) {
			perror(argv[0]);
			return (EXIT_FAILURE);
		}
		if (multiplefiles)
			printf("%s:\n", argv[optind]);
		curfile = argv[optind];
		swabflag = 0;
		bigtiff = 0;
		dump(fd, diroff);
		close(fd);
	}
	return (EXIT_SUCCESS);
}

#define ord(e) ((int)e)

static uint64_t ReadDirectory(int, unsigned, uint64_t);
static void ReadError(char*);
static void Error(const char*, ...);
static void Fatal(const char*, ...);

static void
dump(int fd, uint64_t diroff)
{
	unsigned i, j;
	uint64_t* visited_diroff = NULL;
	unsigned int count_visited_dir = 0;

	_TIFF_lseek_f(fd, (_TIFF_off_t) 0, 0);
	if (read(fd, (char*) &hdr, sizeof (TIFFHeaderCommon)) != sizeof (TIFFHeaderCommon))
		ReadError("TIFF header");
	if (hdr.common.tiff_magic != TIFF_BIGENDIAN
	    && hdr.common.tiff_magic != TIFF_LITTLEENDIAN &&
#if HOST_BIGENDIAN
	    /* MDI is sensitive to the host byte order, unlike TIFF */
	    MDI_BIGENDIAN != hdr.common.tiff_magic
#else
	    MDI_LITTLEENDIAN != hdr.common.tiff_magic
#endif
	   ) {
		Fatal("Not a TIFF or MDI file, bad magic number %u (%#x)",
		    hdr.common.tiff_magic, hdr.common.tiff_magic);
	}
	if (hdr.common.tiff_magic == TIFF_BIGENDIAN
	    || hdr.common.tiff_magic == MDI_BIGENDIAN)
		swabflag = !bigendian;
	else
		swabflag = bigendian;
	if (swabflag)
		TIFFSwabShort(&hdr.common.tiff_version);
	if (hdr.common.tiff_version==42)
	{
		if (read(fd, (char*) &hdr.classic.tiff_diroff, 4) != 4)
			ReadError("TIFF header");
		if (swabflag)
			TIFFSwabLong(&hdr.classic.tiff_diroff);
		printf("Magic: %#x <%s-endian> Version: %#x <%s>\n",
		    hdr.classic.tiff_magic,
		    hdr.classic.tiff_magic == TIFF_BIGENDIAN ? "big" : "little",
		    42,"ClassicTIFF");
		if (diroff == 0)
			diroff = hdr.classic.tiff_diroff;
	}
	else if (hdr.common.tiff_version==43)
	{
		if (read(fd, (char*) &hdr.big.tiff_offsetsize, 12) != 12)
			ReadError("TIFF header");
		if (swabflag)
		{
			TIFFSwabShort(&hdr.big.tiff_offsetsize);
			TIFFSwabShort(&hdr.big.tiff_unused);
			TIFFSwabLong8(&hdr.big.tiff_diroff);
		}
		printf("Magic: %#x <%s-endian> Version: %#x <%s>\n",
		    hdr.big.tiff_magic,
		    hdr.big.tiff_magic == TIFF_BIGENDIAN ? "big" : "little",
		    43,"BigTIFF");
		printf("OffsetSize: %#x Unused: %#x\n",
		    hdr.big.tiff_offsetsize,hdr.big.tiff_unused);
		if (diroff == 0)
			diroff = hdr.big.tiff_diroff;
		bigtiff = 1;
	}
	else
		Fatal("Not a TIFF file, bad version number %u (%#x)",
		    hdr.common.tiff_version, hdr.common.tiff_version);
	for (i = 0; diroff != 0; i++) {
		for(j=0; j<count_visited_dir; j++)
		{
		    if( visited_diroff[j] == diroff )
		    {
			free(visited_diroff);
			Fatal("Cycle detected in chaining of TIFF directories!");
		    }
		}
                {
                    size_t alloc_size;
                    alloc_size=TIFFSafeMultiply(tmsize_t,(count_visited_dir + 1),
                                                sizeof(uint64_t));
                    if (alloc_size == 0)
                    {
                        if (visited_diroff)
                            free(visited_diroff);
                        visited_diroff = 0;
                    }
                    else
                    {
                        visited_diroff = (uint64_t*) realloc(visited_diroff, alloc_size);
                    }
                }
		if( !visited_diroff )
		    Fatal("Out of memory");
		visited_diroff[count_visited_dir] = diroff;
		count_visited_dir ++;

		if (i > 0)
			putchar('\n');
		diroff = ReadDirectory(fd, i, diroff);
	}
	if( visited_diroff )
	    free(visited_diroff);
}

static const int datawidth[] = {
	0, /* 00 = undefined */
	1, /* 01 = TIFF_BYTE */
	1, /* 02 = TIFF_ASCII */
	2, /* 03 = TIFF_SHORT */
	4, /* 04 = TIFF_LONG */
	8, /* 05 = TIFF_RATIONAL */
	1, /* 06 = TIFF_SBYTE */
	1, /* 07 = TIFF_UNDEFINED */
	2, /* 08 = TIFF_SSHORT */
	4, /* 09 = TIFF_SLONG */
	8, /* 10 = TIFF_SRATIONAL */
	4, /* 11 = TIFF_FLOAT */
	8, /* 12 = TIFF_DOUBLE */
	4, /* 13 = TIFF_IFD */
	0, /* 14 = undefined */
	0, /* 15 = undefined */
	8, /* 16 = TIFF_LONG8 */
	8, /* 17 = TIFF_SLONG8 */
	8, /* 18 = TIFF_IFD8 */
};
#define NWIDTHS (sizeof (datawidth) / sizeof (datawidth[0]))
static void PrintTag(FILE*, uint16_t);
static void PrintType(FILE*, uint16_t);
static void PrintData(FILE*, uint16_t, uint32_t, unsigned char*);

/*
 * Read the next TIFF directory from a file
 * and convert it to the internal format.
 * We read directories sequentially.
 */
static uint64_t
ReadDirectory(int fd, unsigned int ix, uint64_t off)
{
	uint16_t dircount;
	uint32_t direntrysize;
	void* dirmem = NULL;
	uint64_t nextdiroff = 0;
	uint32_t n;
	uint8_t* dp;

	if (off == 0)			/* no more directories */
		goto done;
	if (_TIFF_lseek_f(fd, (_TIFF_off_t)off, SEEK_SET) != (_TIFF_off_t)off) {
		Fatal("Seek error accessing TIFF directory");
		goto done;
	}
	if (!bigtiff) {
		if (read(fd, (char*) &dircount, sizeof (uint16_t)) != sizeof (uint16_t)) {
			ReadError("directory count");
			goto done;
		}
		if (swabflag)
			TIFFSwabShort(&dircount);
		direntrysize = 12;
	} else {
		uint64_t dircount64 = 0;
		if (read(fd, (char*) &dircount64, sizeof (uint64_t)) != sizeof (uint64_t)) {
			ReadError("directory count");
			goto done;
		}
		if (swabflag)
			TIFFSwabLong8(&dircount64);
		if (dircount64>0xFFFF) {
			Error("Sanity check on directory count failed");
			goto done;
		}
		dircount = (uint16_t)dircount64;
		direntrysize = 20;
	}
	dirmem = _TIFFmalloc(TIFFSafeMultiply(tmsize_t,dircount,direntrysize));
	if (dirmem == NULL) {
		Fatal("No space for TIFF directory");
		goto done;
	}
	n = read(fd, (char*) dirmem, dircount*direntrysize);
	if (n != dircount*direntrysize) {
		n /= direntrysize;
		Error(
	    "Could only read %" PRIu32 " of %" PRIu16 " entries in directory at offset %#" PRIu64,
		      n, dircount, off);
		dircount = n;
		nextdiroff = 0;
	} else {
		if (!bigtiff) {
			uint32_t nextdiroff32;
			if (read(fd, (char*) &nextdiroff32, sizeof (uint32_t)) != sizeof (uint32_t))
				nextdiroff32 = 0;
			if (swabflag)
				TIFFSwabLong(&nextdiroff32);
			nextdiroff = nextdiroff32;
		} else {
			if (read(fd, (char*) &nextdiroff, sizeof (uint64_t)) != sizeof (uint64_t))
				nextdiroff = 0;
			if (swabflag)
				TIFFSwabLong8(&nextdiroff);
		}
	}
	printf("Directory %u: offset %" PRIu64 " (%#" PRIx64 ") next %" PRIu64 " (%#" PRIx64 ")\n",
        ix, off, off, nextdiroff, nextdiroff);
	for (dp = (uint8_t*)dirmem, n = dircount; n > 0; n--) {
		uint16_t tag;
		uint16_t type;
		uint16_t typewidth;
		uint64_t count;
		uint64_t datasize;
		int datafits;
		void* datamem;
		uint64_t dataoffset;
		int datatruncated;
                int datasizeoverflow;

		tag = *(uint16_t*)dp;
		if (swabflag)
			TIFFSwabShort(&tag);
		dp += sizeof(uint16_t);
		type = *(uint16_t*)dp;
		dp += sizeof(uint16_t);
		if (swabflag)
			TIFFSwabShort(&type);
		PrintTag(stdout, tag);
		putchar(' ');
		PrintType(stdout, type);
		putchar(' ');
		if (!bigtiff)
		{
			uint32_t count32;
			count32 = *(uint32_t*)dp;
			if (swabflag)
				TIFFSwabLong(&count32);
			dp += sizeof(uint32_t);
			count = count32;
		}
		else
		{
			memcpy(&count, dp, sizeof(uint64_t));
			if (swabflag)
				TIFFSwabLong8(&count);
			dp += sizeof(uint64_t);
		}
		printf("%" PRIu64 "<", count);
		if (type >= NWIDTHS)
			typewidth = 0;
		else
			typewidth = datawidth[type];
		datasize = TIFFSafeMultiply(tmsize_t,count,typewidth);
                datasizeoverflow = (typewidth > 0 && datasize / typewidth != count);
		datafits = 1;
		datamem = dp;
		dataoffset = 0;
		datatruncated = 0;
		if (!bigtiff)
		{
			if (datasizeoverflow || datasize>4)
			{
				uint32_t dataoffset32;
				datafits = 0;
				datamem = NULL;
				dataoffset32 = *(uint32_t*)dp;
				if (swabflag)
					TIFFSwabLong(&dataoffset32);
				dataoffset = dataoffset32;
			}
			dp += sizeof(uint32_t);
		}
		else
		{
			if (datasizeoverflow || datasize>8)
			{
				datafits = 0;
				datamem = NULL;
				memcpy(&dataoffset, dp, sizeof(uint64_t));
				if (swabflag)
					TIFFSwabLong8(&dataoffset);
			}
			dp += sizeof(uint64_t);
		}
		if (datasizeoverflow || datasize>0x10000)
		{
			datatruncated = 1;
			count = 0x10000/typewidth;
			datasize = TIFFSafeMultiply(tmsize_t,count,typewidth);
		}
		if (count>maxitems)
		{
			datatruncated = 1;
			count = maxitems;
                        datasize = TIFFSafeMultiply(tmsize_t,count,typewidth);
		}
		if (!datafits)
		{
			datamem = _TIFFmalloc(datasize);
			if (datamem) {
				if (_TIFF_lseek_f(fd, (_TIFF_off_t)dataoffset, 0) !=
				    (_TIFF_off_t)dataoffset)
				{
					Error(
				"Seek error accessing tag %u value", tag);
					_TIFFfree(datamem);
					datamem = NULL;
				}
				else if (read(fd, datamem, (size_t)datasize) != (tmsize_t)datasize)
				{
					Error(
				"Read error accessing tag %u value", tag);
					_TIFFfree(datamem);
					datamem = NULL;
				}
			} else
				Error("No space for data for tag %u",tag);
		}
		if (datamem)
		{
			if (swabflag)
			{
				switch (type)
				{
					case TIFF_BYTE:
					case TIFF_ASCII:
					case TIFF_SBYTE:
					case TIFF_UNDEFINED:
						break;
					case TIFF_SHORT:
					case TIFF_SSHORT:
						TIFFSwabArrayOfShort((uint16_t*)datamem, (tmsize_t)count);
						break;
					case TIFF_LONG:
					case TIFF_SLONG:
					case TIFF_FLOAT:
					case TIFF_IFD:
						TIFFSwabArrayOfLong((uint32_t*)datamem, (tmsize_t)count);
						break;
					case TIFF_RATIONAL:
					case TIFF_SRATIONAL:
						TIFFSwabArrayOfLong((uint32_t*)datamem, (tmsize_t)count * 2);
						break;
					case TIFF_DOUBLE:
					case TIFF_LONG8:
					case TIFF_SLONG8:
					case TIFF_IFD8:
						TIFFSwabArrayOfLong8((uint64_t*)datamem, (tmsize_t)count);
						break;
				}
			}
			PrintData(stdout, type, (uint32_t)count, datamem);
			if (datatruncated)
				printf(" ...");
			if (!datafits)
                                {
                                        _TIFFfree(datamem);
                                        datamem = NULL;
                                }
		}
		printf(">\n");
	}
done:
	if (dirmem)
		_TIFFfree((char *)dirmem);
	return (nextdiroff);
}

static const struct tagname {
	uint16_t tag;
	const char* name;
} tagnames[] = {
    { TIFFTAG_SUBFILETYPE,	"SubFileType" },
    { TIFFTAG_OSUBFILETYPE,	"OldSubFileType" },
    { TIFFTAG_IMAGEWIDTH,	"ImageWidth" },
    { TIFFTAG_IMAGELENGTH,	"ImageLength" },
    { TIFFTAG_BITSPERSAMPLE,	"BitsPerSample" },
    { TIFFTAG_COMPRESSION,	"Compression" },
    { TIFFTAG_PHOTOMETRIC,	"Photometric" },
    { TIFFTAG_THRESHHOLDING,	"Threshholding" },
    { TIFFTAG_CELLWIDTH,	"CellWidth" },
    { TIFFTAG_CELLLENGTH,	"CellLength" },
    { TIFFTAG_FILLORDER,	"FillOrder" },
    { TIFFTAG_DOCUMENTNAME,	"DocumentName" },
    { TIFFTAG_IMAGEDESCRIPTION,	"ImageDescription" },
    { TIFFTAG_MAKE,		"Make" },
    { TIFFTAG_MODEL,		"Model" },
    { TIFFTAG_STRIPOFFSETS,	"StripOffsets" },
    { TIFFTAG_ORIENTATION,	"Orientation" },
    { TIFFTAG_SAMPLESPERPIXEL,	"SamplesPerPixel" },
    { TIFFTAG_ROWSPERSTRIP,	"RowsPerStrip" },
    { TIFFTAG_STRIPBYTECOUNTS,	"StripByteCounts" },
    { TIFFTAG_MINSAMPLEVALUE,	"MinSampleValue" },
    { TIFFTAG_MAXSAMPLEVALUE,	"MaxSampleValue" },
    { TIFFTAG_XRESOLUTION,	"XResolution" },
    { TIFFTAG_YRESOLUTION,	"YResolution" },
    { TIFFTAG_PLANARCONFIG,	"PlanarConfig" },
    { TIFFTAG_PAGENAME,		"PageName" },
    { TIFFTAG_XPOSITION,	"XPosition" },
    { TIFFTAG_YPOSITION,	"YPosition" },
    { TIFFTAG_FREEOFFSETS,	"FreeOffsets" },
    { TIFFTAG_FREEBYTECOUNTS,	"FreeByteCounts" },
    { TIFFTAG_GRAYRESPONSEUNIT,	"GrayResponseUnit" },
    { TIFFTAG_GRAYRESPONSECURVE,"GrayResponseCurve" },
    { TIFFTAG_GROUP3OPTIONS,	"Group3Options" },
    { TIFFTAG_GROUP4OPTIONS,	"Group4Options" },
    { TIFFTAG_RESOLUTIONUNIT,	"ResolutionUnit" },
    { TIFFTAG_PAGENUMBER,	"PageNumber" },
    { TIFFTAG_COLORRESPONSEUNIT,"ColorResponseUnit" },
    { TIFFTAG_TRANSFERFUNCTION,	"TransferFunction" },
    { TIFFTAG_SOFTWARE,		"Software" },
    { TIFFTAG_DATETIME,		"DateTime" },
    { TIFFTAG_ARTIST,		"Artist" },
    { TIFFTAG_HOSTCOMPUTER,	"HostComputer" },
    { TIFFTAG_PREDICTOR,	"Predictor" },
    { TIFFTAG_WHITEPOINT,	"Whitepoint" },
    { TIFFTAG_PRIMARYCHROMATICITIES,"PrimaryChromaticities" },
    { TIFFTAG_COLORMAP,		"Colormap" },
    { TIFFTAG_HALFTONEHINTS,	"HalftoneHints" },
    { TIFFTAG_TILEWIDTH,	"TileWidth" },
    { TIFFTAG_TILELENGTH,	"TileLength" },
    { TIFFTAG_TILEOFFSETS,	"TileOffsets" },
    { TIFFTAG_TILEBYTECOUNTS,	"TileByteCounts" },
    { TIFFTAG_BADFAXLINES,	"BadFaxLines" },
    { TIFFTAG_CLEANFAXDATA,	"CleanFaxData" },
    { TIFFTAG_CONSECUTIVEBADFAXLINES, "ConsecutiveBadFaxLines" },
    { TIFFTAG_SUBIFD,		"SubIFD" },
    { TIFFTAG_INKSET,		"InkSet" },
    { TIFFTAG_INKNAMES,		"InkNames" },
    { TIFFTAG_NUMBEROFINKS,	"NumberOfInks" },
    { TIFFTAG_DOTRANGE,		"DotRange" },
    { TIFFTAG_TARGETPRINTER,	"TargetPrinter" },
    { TIFFTAG_EXTRASAMPLES,	"ExtraSamples" },
    { TIFFTAG_SAMPLEFORMAT,	"SampleFormat" },
    { TIFFTAG_SMINSAMPLEVALUE,	"SMinSampleValue" },
    { TIFFTAG_SMAXSAMPLEVALUE,	"SMaxSampleValue" },
    { TIFFTAG_JPEGPROC,		"JPEGProcessingMode" },
    { TIFFTAG_JPEGIFOFFSET,	"JPEGInterchangeFormat" },
    { TIFFTAG_JPEGIFBYTECOUNT,	"JPEGInterchangeFormatLength" },
    { TIFFTAG_JPEGRESTARTINTERVAL,"JPEGRestartInterval" },
    { TIFFTAG_JPEGLOSSLESSPREDICTORS,"JPEGLosslessPredictors" },
    { TIFFTAG_JPEGPOINTTRANSFORM,"JPEGPointTransform" },
    { TIFFTAG_JPEGTABLES,       "JPEGTables" },
    { TIFFTAG_JPEGQTABLES,	"JPEGQTables" },
    { TIFFTAG_JPEGDCTABLES,	"JPEGDCTables" },
    { TIFFTAG_JPEGACTABLES,	"JPEGACTables" },
    { TIFFTAG_YCBCRCOEFFICIENTS,"YCbCrCoefficients" },
    { TIFFTAG_YCBCRSUBSAMPLING,	"YCbCrSubsampling" },
    { TIFFTAG_YCBCRPOSITIONING,	"YCbCrPositioning" },
    { TIFFTAG_REFERENCEBLACKWHITE, "ReferenceBlackWhite" },
    { TIFFTAG_REFPTS,		"IgReferencePoints (Island Graphics)" },
    { TIFFTAG_REGIONTACKPOINT,	"IgRegionTackPoint (Island Graphics)" },
    { TIFFTAG_REGIONWARPCORNERS,"IgRegionWarpCorners (Island Graphics)" },
    { TIFFTAG_REGIONAFFINE,	"IgRegionAffine (Island Graphics)" },
    { TIFFTAG_MATTEING,		"OBSOLETE Matteing (Silicon Graphics)" },
    { TIFFTAG_DATATYPE,		"OBSOLETE DataType (Silicon Graphics)" },
    { TIFFTAG_IMAGEDEPTH,	"ImageDepth (Silicon Graphics)" },
    { TIFFTAG_TILEDEPTH,	"TileDepth (Silicon Graphics)" },
    { 32768,			"OLD BOGUS Matteing tag" },
    { TIFFTAG_COPYRIGHT,	"Copyright" },
    { TIFFTAG_ICCPROFILE,	"ICC Profile" },
    { TIFFTAG_JBIGOPTIONS,	"JBIG Options" },
    { TIFFTAG_STONITS,		"StoNits" },
};
#define	NTAGS	(sizeof (tagnames) / sizeof (tagnames[0]))

static void
PrintTag(FILE* fd, uint16_t tag)
{
	const struct tagname *tp;

	for (tp = tagnames; tp < &tagnames[NTAGS]; tp++)
		if (tp->tag == tag) {
			fprintf(fd, "%s (%u)", tp->name, tag);
			return;
		}
	fprintf(fd, "%u (%#x)", tag, tag);
}

static void
PrintType(FILE* fd, uint16_t type)
{
	static const char *typenames[] = {
	    "0",
	    "BYTE",
	    "ASCII",
	    "SHORT",
	    "LONG",
	    "RATIONAL",
	    "SBYTE",
	    "UNDEFINED",
	    "SSHORT",
	    "SLONG",
	    "SRATIONAL",
	    "FLOAT",
	    "DOUBLE",
	    "IFD",
	    "14",
	    "15",
	    "LONG8",
	    "SLONG8",
	    "IFD8"
	};
#define	NTYPES	(sizeof (typenames) / sizeof (typenames[0]))

	if (type < NTYPES)
		fprintf(fd, "%s (%u)", typenames[type], type);
	else
		fprintf(fd, "%u (%#x)", type, type);
}
#undef	NTYPES

#include <ctype.h>

static void
PrintASCII(FILE* fd, uint32_t cc, const unsigned char* cp)
{
	for (; cc > 0; cc--, cp++) {
		const char* tp;

		if (isprint(*cp)) {
			fputc(*cp, fd);
			continue;
		}
		for (tp = "\tt\bb\rr\nn\vv"; *tp; tp++)
			if (*tp++ == *cp)
				break;
		if (*tp)
			fprintf(fd, "\\%c", *tp);
		else if (*cp)
			fprintf(fd, "\\%03o", *cp);
		else
			fprintf(fd, "\\0");
	}
}

static void
PrintData(FILE* fd, uint16_t type, uint32_t count, unsigned char* data)
{
	char* sep = "";

	switch (type) {
	case TIFF_BYTE:
		while (count-- > 0)
			fprintf(fd, bytefmt, sep, *data++), sep = " ";
		break;
	case TIFF_SBYTE:
		while (count-- > 0)
			fprintf(fd, sbytefmt, sep, *(char *)data++), sep = " ";
		break;
	case TIFF_UNDEFINED:
		while (count-- > 0)
			fprintf(fd, bytefmt, sep, *data++), sep = " ";
		break;
	case TIFF_ASCII:
		PrintASCII(fd, count, data);
		break;
	case TIFF_SHORT: {
		uint16_t *wp = (uint16_t*)data;
		while (count-- > 0)
			fprintf(fd, hex_mode ? shortfmth : shortfmtd, sep, *wp++), sep = " ";
		break;
	}
	case TIFF_SSHORT: {
		int16_t *wp = (int16_t*)data;
		while (count-- > 0)
			fprintf(fd, hex_mode ? sshortfmth : sshortfmtd, sep, *wp++), sep = " ";
		break;
	}
	case TIFF_LONG: {
		uint32_t *lp = (uint32_t*)data;
		while (count-- > 0) {
			fprintf(fd, hex_mode ? longfmth : longfmtd, sep, *lp++);
			sep = " ";
		}
		break;
	}
	case TIFF_SLONG: {
		int32_t *lp = (int32_t*)data;
		while (count-- > 0)
			fprintf(fd, hex_mode ? slongfmth : slongfmtd, sep, *lp++), sep = " ";
		break;
	}
	case TIFF_LONG8: {
		uint64_t *llp = (uint64_t*)data;
		while (count-- > 0) {
                        uint64_t val;
                        memcpy(&val, llp, sizeof(uint64_t));
                        llp ++;
			fprintf(fd, long8fmt, sep, val);
			sep = " ";
		}
		break;
	}
	case TIFF_SLONG8: {
		int64_t *llp = (int64_t*)data;
		while (count-- > 0) {
                        int64_t val;
                        memcpy(&val, llp, sizeof(int64_t));
                        llp ++;
                        fprintf(fd, slong8fmt, sep, val);
                        sep = " ";
                }
		break;
	}
	case TIFF_RATIONAL: {
		uint32_t *lp = (uint32_t*)data;
		while (count-- > 0) {
			if (lp[1] == 0)
				fprintf(fd, "%sNan (%"PRIu32"/%"PRIu32")", sep,
				    lp[0], lp[1]);
			else
				fprintf(fd, rationalfmt, sep,
				    (double)lp[0] / (double)lp[1]);
			sep = " ";
			lp += 2;
		}
		break;
	}
	case TIFF_SRATIONAL: {
		int32_t *lp = (int32_t*)data;
		while (count-- > 0) {
			if (lp[1] == 0)
				fprintf(fd, "%sNan (%"PRId32"/%"PRId32")", sep,
				    lp[0], lp[1]);
			else
				fprintf(fd, srationalfmt, sep,
				    (double)lp[0] / (double)lp[1]);
			sep = " ";
			lp += 2;
		}
		break;
	}
	case TIFF_FLOAT: {
		float *fp = (float *)data;
		while (count-- > 0)
			fprintf(fd, floatfmt, sep, *fp++), sep = " ";
		break;
	}
	case TIFF_DOUBLE: {
		double *dp = (double *)data;
		while (count-- > 0)
			fprintf(fd, doublefmt, sep, *dp++), sep = " ";
		break;
	}
	case TIFF_IFD: {
		uint32_t *lp = (uint32_t*)data;
		while (count-- > 0) {
			fprintf(fd, ifdfmt, sep, *lp++);
			sep = " ";
		}
		break;
	}
	case TIFF_IFD8: {
		uint64_t *llp = (uint64_t*)data;
		while (count-- > 0) {
			fprintf(fd, ifd8fmt, sep, *llp++);
			sep = " ";
			sep = " ";
		}
		break;
	}
	}
}

static void
ReadError(char* what)
{
	Fatal("Error while reading %s", what);
}

#include <stdarg.h>

static void
vError(FILE* fd, const char* fmt, va_list ap)
{
	fprintf(fd, "%s: ", curfile);
	vfprintf(fd, fmt, ap);
	fprintf(fd, ".\n");
}

static void
Error(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vError(stderr, fmt, ap);
	va_end(ap);
}

static void
Fatal(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vError(stderr, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

/* vim: set ts=8 sts=8 sw=8 noet: */
/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * fill-column: 78
 * End:
 */
