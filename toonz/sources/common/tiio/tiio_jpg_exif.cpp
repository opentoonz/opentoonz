/*-------------------------------------------------------------
tiio_jpg_exif.cpp
Based on source code of a public domain software "Exif Jpeg header manipulation
tool (jhead)" by Matthias Wandel.
For now it is used only for obtaining resolution values.
http://www.sentex.net/~mwandel/jhead/
-------------------------------------------------------------*/
#include "tiio_jpg_exif.h"
#include <iostream>
#include <string.h>

// for debug
#define ShowTags 0
#define DumpExifMap 0

namespace {

typedef unsigned char uchar;
const int BytesPerFormat[] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};
//--------------------------------------------------------------------------
// Describes tag values
const int TAG_X_RESOLUTION    = 0x011A;
const int TAG_Y_RESOLUTION    = 0x011B;
const int TAG_RESOLUTION_UNIT = 0x0128;
#ifdef ReadAllTags
const int TAG_INTEROP_INDEX          = 0x0001;
const int TAG_INTEROP_VERSION        = 0x0002;
const int TAG_IMAGE_WIDTH            = 0x0100;
const int TAG_IMAGE_LENGTH           = 0x0101;
const int TAG_BITS_PER_SAMPLE        = 0x0102;
const int TAG_COMPRESSION            = 0x0103;
const int TAG_PHOTOMETRIC_INTERP     = 0x0106;
const int TAG_FILL_ORDER             = 0x010A;
const int TAG_DOCUMENT_NAME          = 0x010D;
const int TAG_IMAGE_DESCRIPTION      = 0x010E;
const int TAG_MAKE                   = 0x010F;
const int TAG_MODEL                  = 0x0110;
const int TAG_SRIP_OFFSET            = 0x0111;
const int TAG_ORIENTATION            = 0x0112;
const int TAG_SAMPLES_PER_PIXEL      = 0x0115;
const int TAG_ROWS_PER_STRIP         = 0x0116;
const int TAG_STRIP_BYTE_COUNTS      = 0x0117;
const int TAG_PLANAR_CONFIGURATION   = 0x011C;
const int TAG_TRANSFER_FUNCTION      = 0x012D;
const int TAG_SOFTWARE               = 0x0131;
const int TAG_DATETIME               = 0x0132;
const int TAG_ARTIST                 = 0x013B;
const int TAG_WHITE_POINT            = 0x013E;
const int TAG_PRIMARY_CHROMATICITIES = 0x013F;
const int TAG_TRANSFER_RANGE         = 0x0156;
const int TAG_JPEG_PROC              = 0x0200;
const int TAG_THUMBNAIL_OFFSET       = 0x0201;
const int TAG_THUMBNAIL_LENGTH       = 0x0202;
const int TAG_Y_CB_CR_COEFFICIENTS   = 0x0211;
const int TAG_Y_CB_CR_SUB_SAMPLING   = 0x0212;
const int TAG_Y_CB_CR_POSITIONING    = 0x0213;
const int TAG_REFERENCE_BLACK_WHITE  = 0x0214;
const int TAG_RELATED_IMAGE_WIDTH    = 0x1001;
const int TAG_RELATED_IMAGE_LENGTH   = 0x1002;
const int TAG_CFA_REPEAT_PATTERN_DIM = 0x828D;
const int TAG_CFA_PATTERN1           = 0x828E;
const int TAG_BATTERY_LEVEL          = 0x828F;
const int TAG_COPYRIGHT              = 0x8298;
const int TAG_EXPOSURETIME           = 0x829A;
const int TAG_FNUMBER                = 0x829D;
const int TAG_IPTC_NAA               = 0x83BB;
const int TAG_EXIF_OFFSET            = 0x8769;
const int TAG_INTER_COLOR_PROFILE    = 0x8773;
const int TAG_EXPOSURE_PROGRAM       = 0x8822;
const int TAG_SPECTRAL_SENSITIVITY   = 0x8824;
const int TAG_GPSINFO                = 0x8825;
const int TAG_ISO_EQUIVALENT         = 0x8827;
const int TAG_OECF                   = 0x8828;
const int TAG_EXIF_VERSION           = 0x9000;
const int TAG_DATETIME_ORIGINAL      = 0x9003;
const int TAG_DATETIME_DIGITIZED     = 0x9004;
const int TAG_COMPONENTS_CONFIG      = 0x9101;
const int TAG_CPRS_BITS_PER_PIXEL    = 0x9102;
const int TAG_SHUTTERSPEED           = 0x9201;
const int TAG_APERTURE               = 0x9202;
const int TAG_BRIGHTNESS_VALUE       = 0x9203;
const int TAG_EXPOSURE_BIAS          = 0x9204;
const int TAG_MAXAPERTURE            = 0x9205;
const int TAG_SUBJECT_DISTANCE       = 0x9206;
const int TAG_METERING_MODE          = 0x9207;
const int TAG_LIGHT_SOURCE           = 0x9208;
const int TAG_FLASH                  = 0x9209;
const int TAG_FOCALLENGTH            = 0x920A;
const int TAG_SUBJECTAREA            = 0x9214;
const int TAG_MAKER_NOTE             = 0x927C;
const int TAG_USERCOMMENT            = 0x9286;
const int TAG_SUBSEC_TIME            = 0x9290;
const int TAG_SUBSEC_TIME_ORIG       = 0x9291;
const int TAG_SUBSEC_TIME_DIG        = 0x9292;

const int TAG_WINXP_TITLE = 0x9c9b;  // Windows XP - not part of exif standard.
const int TAG_WINXP_COMMENT =
    0x9c9c;                           // Windows XP - not part of exif standard.
const int TAG_WINXP_AUTHOR = 0x9c9d;  // Windows XP - not part of exif standard.
const int TAG_WINXP_KEYWORDS =
    0x9c9e;  // Windows XP - not part of exif standard.
const int TAG_WINXP_SUBJECT =
    0x9c9f;  // Windows XP - not part of exif standard.

const int TAG_FLASH_PIX_VERSION  = 0xA000;
const int TAG_COLOR_SPACE        = 0xA001;
const int TAG_PIXEL_X_DIMENSION  = 0xA002;
const int TAG_PIXEL_Y_DIMENSION  = 0xA003;
const int TAG_RELATED_AUDIO_FILE = 0xA004;
const int TAG_INTEROP_OFFSET     = 0xA005;
const int TAG_FLASH_ENERGY       = 0xA20B;
const int TAG_SPATIAL_FREQ_RESP  = 0xA20C;
const int TAG_FOCAL_PLANE_XRES   = 0xA20E;
const int TAG_FOCAL_PLANE_YRES   = 0xA20F;
const int TAG_FOCAL_PLANE_UNITS  = 0xA210;
const int TAG_SUBJECT_LOCATION   = 0xA214;
const int TAG_EXPOSURE_INDEX     = 0xA215;
const int TAG_SENSING_METHOD     = 0xA217;
const int TAG_FILE_SOURCE        = 0xA300;
const int TAG_SCENE_TYPE         = 0xA301;
const int TAG_CFA_PATTERN        = 0xA302;
const int TAG_CUSTOM_RENDERED    = 0xA401;
const int TAG_EXPOSURE_MODE      = 0xA402;
const int TAG_WHITEBALANCE       = 0xA403;
const int TAG_DIGITALZOOMRATIO   = 0xA404;
const int TAG_FOCALLENGTH_35MM   = 0xA405;
const int TAG_SCENE_CAPTURE_TYPE = 0xA406;
const int TAG_GAIN_CONTROL       = 0xA407;
const int TAG_CONTRAST           = 0xA408;
const int TAG_SATURATION         = 0xA409;
const int TAG_SHARPNESS          = 0xA40A;
const int TAG_DISTANCE_RANGE     = 0xA40C;
const int TAG_IMAGE_UNIQUE_ID    = 0xA420;
#endif

typedef struct {
  unsigned short Tag;
  char *Desc;
} TagTable_t;

const TagTable_t TagTable[] = {
    {TAG_X_RESOLUTION, "XResolution"},
    {TAG_Y_RESOLUTION, "YResolution"},
    {TAG_RESOLUTION_UNIT, "ResolutionUnit"},
#ifdef ReadAllTags
    {TAG_INTEROP_INDEX, "InteropIndex"},
    {TAG_INTEROP_VERSION, "InteropVersion"},
    {TAG_IMAGE_WIDTH, "ImageWidth"},
    {TAG_IMAGE_LENGTH, "ImageLength"},
    {TAG_BITS_PER_SAMPLE, "BitsPerSample"},
    {TAG_COMPRESSION, "Compression"},
    {TAG_PHOTOMETRIC_INTERP, "PhotometricInterpretation"},
    {TAG_FILL_ORDER, "FillOrder"},
    {TAG_DOCUMENT_NAME, "DocumentName"},
    {TAG_IMAGE_DESCRIPTION, "ImageDescription"},
    {TAG_MAKE, "Make"},
    {TAG_MODEL, "Model"},
    {TAG_SRIP_OFFSET, "StripOffsets"},
    {TAG_ORIENTATION, "Orientation"},
    {TAG_SAMPLES_PER_PIXEL, "SamplesPerPixel"},
    {TAG_ROWS_PER_STRIP, "RowsPerStrip"},
    {TAG_STRIP_BYTE_COUNTS, "StripByteCounts"},
    {TAG_PLANAR_CONFIGURATION, "PlanarConfiguration"},
    {TAG_TRANSFER_FUNCTION, "TransferFunction"},
    {TAG_SOFTWARE, "Software"},
    {TAG_DATETIME, "DateTime"},
    {TAG_ARTIST, "Artist"},
    {TAG_WHITE_POINT, "WhitePoint"},
    {TAG_PRIMARY_CHROMATICITIES, "PrimaryChromaticities"},
    {TAG_TRANSFER_RANGE, "TransferRange"},
    {TAG_JPEG_PROC, "JPEGProc"},
    {TAG_THUMBNAIL_OFFSET, "ThumbnailOffset"},
    {TAG_THUMBNAIL_LENGTH, "ThumbnailLength"},
    {TAG_Y_CB_CR_COEFFICIENTS, "YCbCrCoefficients"},
    {TAG_Y_CB_CR_SUB_SAMPLING, "YCbCrSubSampling"},
    {TAG_Y_CB_CR_POSITIONING, "YCbCrPositioning"},
    {TAG_REFERENCE_BLACK_WHITE, "ReferenceBlackWhite"},
    {TAG_RELATED_IMAGE_WIDTH, "RelatedImageWidth"},
    {TAG_RELATED_IMAGE_LENGTH, "RelatedImageLength"},
    {TAG_CFA_REPEAT_PATTERN_DIM, "CFARepeatPatternDim"},
    {TAG_CFA_PATTERN1, "CFAPattern"},
    {TAG_BATTERY_LEVEL, "BatteryLevel"},
    {TAG_COPYRIGHT, "Copyright"},
    {TAG_EXPOSURETIME, "ExposureTime"},
    {TAG_FNUMBER, "FNumber"},
    {TAG_IPTC_NAA, "IPTC/NAA"},
    {TAG_EXIF_OFFSET, "ExifOffset"},
    {TAG_INTER_COLOR_PROFILE, "InterColorProfile"},
    {TAG_EXPOSURE_PROGRAM, "ExposureProgram"},
    {TAG_SPECTRAL_SENSITIVITY, "SpectralSensitivity"},
    {TAG_GPSINFO, "GPS Dir offset"},
    {TAG_ISO_EQUIVALENT, "ISOSpeedRatings"},
    {TAG_OECF, "OECF"},
    {TAG_EXIF_VERSION, "ExifVersion"},
    {TAG_DATETIME_ORIGINAL, "DateTimeOriginal"},
    {TAG_DATETIME_DIGITIZED, "DateTimeDigitized"},
    {TAG_COMPONENTS_CONFIG, "ComponentsConfiguration"},
    {TAG_CPRS_BITS_PER_PIXEL, "CompressedBitsPerPixel"},
    {TAG_SHUTTERSPEED, "ShutterSpeedValue"},
    {TAG_APERTURE, "ApertureValue"},
    {TAG_BRIGHTNESS_VALUE, "BrightnessValue"},
    {TAG_EXPOSURE_BIAS, "ExposureBiasValue"},
    {TAG_MAXAPERTURE, "MaxApertureValue"},
    {TAG_SUBJECT_DISTANCE, "SubjectDistance"},
    {TAG_METERING_MODE, "MeteringMode"},
    {TAG_LIGHT_SOURCE, "LightSource"},
    {TAG_FLASH, "Flash"},
    {TAG_FOCALLENGTH, "FocalLength"},
    {TAG_MAKER_NOTE, "MakerNote"},
    {TAG_USERCOMMENT, "UserComment"},
    {TAG_SUBSEC_TIME, "SubSecTime"},
    {TAG_SUBSEC_TIME_ORIG, "SubSecTimeOriginal"},
    {TAG_SUBSEC_TIME_DIG, "SubSecTimeDigitized"},
    {TAG_WINXP_TITLE, "Windows-XP Title"},
    {TAG_WINXP_COMMENT, "Windows-XP comment"},
    {TAG_WINXP_AUTHOR, "Windows-XP author"},
    {TAG_WINXP_KEYWORDS, "Windows-XP keywords"},
    {TAG_WINXP_SUBJECT, "Windows-XP subject"},
    {TAG_FLASH_PIX_VERSION, "FlashPixVersion"},
    {TAG_COLOR_SPACE, "ColorSpace"},
    {TAG_PIXEL_X_DIMENSION, "ExifImageWidth"},
    {TAG_PIXEL_Y_DIMENSION, "ExifImageLength"},
    {TAG_RELATED_AUDIO_FILE, "RelatedAudioFile"},
    {TAG_INTEROP_OFFSET, "InteroperabilityOffset"},
    {TAG_FLASH_ENERGY, "FlashEnergy"},
    {TAG_SPATIAL_FREQ_RESP, "SpatialFrequencyResponse"},
    {TAG_FOCAL_PLANE_XRES, "FocalPlaneXResolution"},
    {TAG_FOCAL_PLANE_YRES, "FocalPlaneYResolution"},
    {TAG_FOCAL_PLANE_UNITS, "FocalPlaneResolutionUnit"},
    {TAG_SUBJECT_LOCATION, "SubjectLocation"},
    {TAG_EXPOSURE_INDEX, "ExposureIndex"},
    {TAG_SENSING_METHOD, "SensingMethod"},
    {TAG_FILE_SOURCE, "FileSource"},
    {TAG_SCENE_TYPE, "SceneType"},
    {TAG_CFA_PATTERN, "CFA Pattern"},
    {TAG_CUSTOM_RENDERED, "CustomRendered"},
    {TAG_EXPOSURE_MODE, "ExposureMode"},
    {TAG_WHITEBALANCE, "WhiteBalance"},
    {TAG_DIGITALZOOMRATIO, "DigitalZoomRatio"},
    {TAG_FOCALLENGTH_35MM, "FocalLengthIn35mmFilm"},
    {TAG_SUBJECTAREA, "SubjectArea"},
    {TAG_SCENE_CAPTURE_TYPE, "SceneCaptureType"},
    {TAG_GAIN_CONTROL, "GainControl"},
    {TAG_CONTRAST, "Contrast"},
    {TAG_SATURATION, "Saturation"},
    {TAG_SHARPNESS, "Sharpness"},
    {TAG_DISTANCE_RANGE, "SubjectDistanceRange"},
    {TAG_IMAGE_UNIQUE_ID, "ImageUniqueId"},
#endif
};

const int TAG_TABLE_SIZE = (sizeof(TagTable) / sizeof(TagTable_t));

const int TRUE  = 1;
const int FALSE = 0;
}

//--------------------------------------------------------------------------
// Convert a 16 bit unsigned value from file's native byte order
//--------------------------------------------------------------------------
int JpgExifReader::Get16u(void *Short) {
  if (MotorolaOrder) {
    return (((uchar *)Short)[0] << 8) | ((uchar *)Short)[1];
  } else {
    return (((uchar *)Short)[1] << 8) | ((uchar *)Short)[0];
  }
}

//--------------------------------------------------------------------------
// Convert a 32 bit signed value from file's native byte order
//--------------------------------------------------------------------------
int JpgExifReader::Get32s(void *Long) {
  if (MotorolaOrder) {
    return (((char *)Long)[0] << 24) | (((uchar *)Long)[1] << 16) |
           (((uchar *)Long)[2] << 8) | (((uchar *)Long)[3] << 0);
  } else {
    return (((char *)Long)[3] << 24) | (((uchar *)Long)[2] << 16) |
           (((uchar *)Long)[1] << 8) | (((uchar *)Long)[0] << 0);
  }
}

//--------------------------------------------------------------------------
// Convert a 32 bit unsigned value from file's native byte order
//--------------------------------------------------------------------------
unsigned JpgExifReader::Get32u(void *Long) {
  return (unsigned)Get32s(Long) & 0xffffffff;
}

//--------------------------------------------------------------------------
// Display a number as one of its many formats
//--------------------------------------------------------------------------
void JpgExifReader::PrintFormatNumber(void *ValuePtr, int Format,
                                      int ByteCount) {
  int s, n;

  for (n = 0; n < 16; n++) {
    switch (Format) {
    case FMT_SBYTE:
    case FMT_BYTE:
      printf("%02x", *(uchar *)ValuePtr);
      s = 1;
      break;
    case FMT_USHORT:
      printf("%d", Get16u(ValuePtr));
      s = 2;
      break;
    case FMT_ULONG:
    case FMT_SLONG:
      printf("%d", Get32s(ValuePtr));
      s = 4;
      break;
    case FMT_SSHORT:
      printf("%hd", (signed short)Get16u(ValuePtr));
      s = 2;
      break;
    case FMT_URATIONAL:
      printf("%u/%u", Get32s(ValuePtr), Get32s(4 + (char *)ValuePtr));
      s = 8;
      break;

    case FMT_SRATIONAL:
      printf("%d/%d", Get32s(ValuePtr), Get32s(4 + (char *)ValuePtr));
      s = 8;
      break;

    case FMT_SINGLE:
      printf("%f", (double)*(float *)ValuePtr);
      s = 8;
      break;
    case FMT_DOUBLE:
      printf("%f", *(double *)ValuePtr);
      s = 8;
      break;
    default:
      printf("Unknown format %d:", Format);
      return;
    }
    ByteCount -= s;
    if (ByteCount <= 0) break;
    printf(", ");
    ValuePtr = (void *)((char *)ValuePtr + s);
  }
  if (n >= 16) printf("...");
}

//--------------------------------------------------------------------------
// Evaluate number, be it int, rational, or float from directory.
//--------------------------------------------------------------------------
double JpgExifReader::ConvertAnyFormat(void *ValuePtr, int Format) {
  double Value;
  Value = 0;

  switch (Format) {
  case FMT_SBYTE:
    Value = *(signed char *)ValuePtr;
    break;
  case FMT_BYTE:
    Value = *(uchar *)ValuePtr;
    break;

  case FMT_USHORT:
    Value = Get16u(ValuePtr);
    break;
  case FMT_ULONG:
    Value = Get32u(ValuePtr);
    break;

  case FMT_URATIONAL:
  case FMT_SRATIONAL: {
    int Num, Den;
    Num = Get32s(ValuePtr);
    Den = Get32s(4 + (char *)ValuePtr);
    if (Den == 0) {
      Value = 0;
    } else {
      if (Format == FMT_SRATIONAL) {
        Value = (double)Num / Den;
      } else {
        Value = (double)(unsigned)Num / (double)(unsigned)Den;
      }
    }
    break;
  }

  case FMT_SSHORT:
    Value = (signed short)Get16u(ValuePtr);
    break;
  case FMT_SLONG:
    Value = Get32s(ValuePtr);
    break;

  // Not sure if this is correct (never seen float used in Exif format)
  case FMT_SINGLE:
    Value = (double)*(float *)ValuePtr;
    break;
  case FMT_DOUBLE:
    Value = *(double *)ValuePtr;
    break;

  default:
    std::cout << "Illegal format code " << Format << " in Exif header"
              << std::endl;
  }
  return Value;
}

//--------------------------------------------------------------------------

void JpgExifReader::process_EXIF(unsigned char *ExifSection,
                                 unsigned int length) {
  unsigned int FirstOffset;

#ifdef ReadAllTags
  FocalplaneXRes       = 0;
  FocalplaneUnits      = 0;
  ExifImageWidth       = 0;
  NumOrientations      = 0;
  DirWithThumbnailPtrs = NULL;
#endif

  if (ShowTags) printf("Exif header %u bytes long\n", length);

  {  // Check the EXIF header component
    static uchar ExifHeader[] = "Exif\0\0";
    if (memcmp(ExifSection + 2, ExifHeader, 6)) {
      std::cout << "Incorrect Exif header" << std::endl;
      return;
    }
  }

  if (memcmp(ExifSection + 8, "II", 2) == 0) {
    if (ShowTags) printf("Exif section in Intel order\n");
    MotorolaOrder = 0;
  } else {
    if (memcmp(ExifSection + 8, "MM", 2) == 0) {
      if (ShowTags) printf("Exif section in Motorola order\n");
      MotorolaOrder = 1;
    } else {
      std::cout << "Invalid Exif alignment marker." << std::endl;
      return;
    }
  }

  // Check the next value for correctness.
  if (Get16u(ExifSection + 10) != 0x2a) {
    std::cout << "Invalid Exif start (1)" << std::endl;
    return;
  }

  FirstOffset = Get32u(ExifSection + 12);
  if (FirstOffset < 8 || FirstOffset > 16) {
    if (FirstOffset < 16 || FirstOffset > length - 16) {
      std::cout << "invalid offset for first Exif IFD value" << std::endl;
      return;
    }
    // Usually set to 8, but other values valid too.
    std::cout << "Suspicious offset of first Exif IFD value" << std::endl;
  }

  // First directory starts 16 bytes in.  All offset are relative to 8 bytes in.
  ProcessExifDir(ExifSection + 8 + FirstOffset, ExifSection + 8, length - 8, 0);

#ifdef ReadAllTags
  ImageInfo.ThumbnailAtEnd =
      ImageInfo.ThumbnailOffset >= ImageInfo.LargestExifOffset ? TRUE : FALSE;

  if (DumpExifMap) {
    unsigned a, b;
    printf("Map: %05d- End of exif\n", length - 8);
    for (a = 0; a < length - 8; a += 10) {
      printf("Map: %05d ", a);
      for (b = 0; b < 10; b++) printf(" %02x", *(ExifSection + 8 + a + b));
      printf("\n");
    }
  }

  // Compute the CCD width, in millimeters.
  if (FocalplaneXRes != 0 && ExifImageWidth != 0) {
    // Note: With some cameras, its not possible to compute this correctly
    // because
    // they don't adjust the indicated focal plane resolution units when using
    // less
    // than maximum resolution, so the CCDWidth value comes out too small.
    // Nothing
    // that Jhad can do about it - its a camera problem.
    ImageInfo.CCDWidth =
        (float)(ExifImageWidth * FocalplaneUnits / FocalplaneXRes);

    if (ImageInfo.FocalLength && ImageInfo.FocalLength35mmEquiv == 0) {
      // Compute 35 mm equivalent focal length based on sensor geometry if we
      // haven't
      // already got it explicitly from a tag.
      ImageInfo.FocalLength35mmEquiv =
          (int)(ImageInfo.FocalLength / ImageInfo.CCDWidth * 36 + 0.5);
    }
  }
#endif
}

//--------------------------------------------------------------------------
// Process one of the nested EXIF directories.
//--------------------------------------------------------------------------
void JpgExifReader::ProcessExifDir(unsigned char *DirStart,
                                   unsigned char *OffsetBase,
                                   unsigned ExifLength, int NestingLevel) {
  int de;
  int a;
  int NumDirEntries;
  unsigned ThumbnailOffset = 0;
  unsigned ThumbnailSize   = 0;
  char IndentString[25];

  if (NestingLevel > 4) {
    std::cout << "Maximum Exif directory nesting exceeded (corrupt Exif header)"
              << std::endl;
    return;
  }

  memset(IndentString, ' ', 25);
  IndentString[NestingLevel * 4] = '\0';

  NumDirEntries = Get16u(DirStart);
#define DIR_ENTRY_ADDR(Start, Entry) (Start + 2 + 12 * (Entry))

  {
    unsigned char *DirEnd;
    DirEnd = DIR_ENTRY_ADDR(DirStart, NumDirEntries);
    if (DirEnd + 4 > (OffsetBase + ExifLength)) {
      if (DirEnd + 2 == OffsetBase + ExifLength ||
          DirEnd == OffsetBase + ExifLength) {
        // Version 1.3 of jhead would truncate a bit too much.
        // This also caught later on as well.
      } else {
        std::cout << "Illegally sized Exif subdirectory (" << NumDirEntries
                  << "entries)" << std::endl;
        return;
      }
    }
    if (DumpExifMap) {
      printf("Map: %05u-%05u: Directory\n", (int)(DirStart - OffsetBase),
             (int)(DirEnd + 4 - OffsetBase));
    }
  }

  if (ShowTags) {
    printf("(dir has %d entries)\n", NumDirEntries);
  }

  for (de = 0; de < NumDirEntries; de++) {
    int Tag, Format, Components;
    unsigned char *ValuePtr;
    int ByteCount;
    unsigned char *DirEntry;
    DirEntry = DIR_ENTRY_ADDR(DirStart, de);

    Tag        = Get16u(DirEntry);
    Format     = Get16u(DirEntry + 2);
    Components = Get32u(DirEntry + 4);

    if ((Format - 1) >= NUM_FORMATS) {
      // (-1) catches illegal zero case as unsigned underflows to positive
      // large.
      std::cout << "Illegal number format " << Format << " for tag " << Tag
                << " in Exif" << std::endl;
      continue;
    }

    if ((unsigned)Components > 0x10000) {
      std::cout << "Too many components " << Components << " for tag " << Tag
                << " in Exif";
      continue;
    }

    ByteCount = Components * BytesPerFormat[Format];

    if (ByteCount > 4) {
      unsigned OffsetVal;
      OffsetVal = Get32u(DirEntry + 8);
      // If its bigger than 4 bytes, the dir entry contains an offset.
      if (OffsetVal + ByteCount > ExifLength) {
        // Bogus pointer offset and / or bytecount value
        std::cout << "Illegal value pointer for tag " << Tag << " in Exif";
        continue;
      }
      ValuePtr = OffsetBase + OffsetVal;

#ifdef ReadAllTags
      if (OffsetVal > ImageInfo.LargestExifOffset) {
        ImageInfo.LargestExifOffset = OffsetVal;
      }
#endif

      if (DumpExifMap) {
        printf("Map: %05u-%05u:   Data for tag %04x\n", OffsetVal,
               OffsetVal + ByteCount, Tag);
      }
    } else {
      // 4 bytes or less and value is in the dir entry itself
      ValuePtr = DirEntry + 8;
    }

#ifdef ReadAllTags
    if (Tag == TAG_MAKER_NOTE) {
      if (ShowTags) {
        printf("%s    Maker note: ", IndentString);
      }
      // ProcessMakerNote(ValuePtr, ByteCount, OffsetBase, ExifLength);
      continue;
    }

    if (ShowTags) {
      // Show tag name
      for (a = 0;; a++) {
        if (a >= TAG_TABLE_SIZE) {
          printf("%s    Unknown Tag %04x Value = ", IndentString, Tag);
          break;
        }
        if (TagTable[a].Tag == Tag) {
          printf("%s    %s = ", IndentString, TagTable[a].Desc);
          break;
        }
      }

      // Show tag value.
      switch (Format) {
      case FMT_BYTE:
        if (ByteCount > 1) {
          printf("%.*ls\n", ByteCount / 2, (wchar_t *)ValuePtr);
        } else {
          PrintFormatNumber(ValuePtr, Format, ByteCount);
          printf("\n");
        }
        break;

      case FMT_UNDEFINED:
      // Undefined is typically an ascii string.

      case FMT_STRING:
        // String arrays printed without function call (different from int
        // arrays)
        {
          int NoPrint = 0;
          printf("\"");
          for (a = 0; a < ByteCount; a++) {
            if (ValuePtr[a] >= 32) {
              putchar(ValuePtr[a]);
              NoPrint = 0;
            } else {
              // Avoiding indicating too many unprintable characters of
              // proprietary
              // bits of binary information this program may not know how to
              // parse.
              if (!NoPrint && a != ByteCount - 1) {
                putchar('?');
                NoPrint = 1;
              }
            }
          }
          printf("\"\n");
        }
        break;

      default:
        // Handle arrays of numbers later (will there ever be?)
        PrintFormatNumber(ValuePtr, Format, ByteCount);
        printf("\n");
      }
    }
#endif
    // Extract useful components of tag
    switch (Tag) {
#ifdef ReadAllTags
    case TAG_MAKE:
      strncpy(ImageInfo.CameraMake, (char *)ValuePtr,
              ByteCount < 31 ? ByteCount : 31);
      break;

    case TAG_MODEL:
      strncpy(ImageInfo.CameraModel, (char *)ValuePtr,
              ByteCount < 39 ? ByteCount : 39);
      break;

    case TAG_DATETIME_ORIGINAL:
      // If we get a DATETIME_ORIGINAL, we use that one.
      strncpy(ImageInfo.DateTime, (char *)ValuePtr, 19);
    // Fallthru...

    case TAG_DATETIME_DIGITIZED:
    case TAG_DATETIME:
      if (!isdigit(static_cast<unsigned char>(ImageInfo.DateTime[0]))) {
        // If we don't already have a DATETIME_ORIGINAL, use whatever
        // time fields we may have.
        strncpy(ImageInfo.DateTime, (char *)ValuePtr, 19);
      }

      if (ImageInfo.numDateTimeTags >= MAX_DATE_COPIES) {
        std::cout << "More than " << MAX_DATE_COPIES
                  << " date fields in Exif.  This is nuts" << std::endl;
        break;
      }
      ImageInfo.DateTimeOffsets[ImageInfo.numDateTimeTags++] =
          (char *)ValuePtr - (char *)OffsetBase;
      break;

    case TAG_WINXP_COMMENT:
      if (ImageInfo.Comments[0]) {  // We already have a jpeg comment.
        // Already have a comment (probably windows comment), skip this one.
        if (ShowTags)
          printf("Windows XP commend and other comment in header\n");
        break;  // Already have a windows comment, skip this one.
      }

      if (ByteCount > 1) {
        if (ByteCount > MAX_COMMENT_SIZE) ByteCount = MAX_COMMENT_SIZE;
        memcpy(ImageInfo.Comments, ValuePtr, ByteCount);
        ImageInfo.CommentWidthchars = ByteCount / 2;
      }
      break;

    case TAG_USERCOMMENT:
      if (ImageInfo.Comments[0]) {  // We already have a jpeg comment.
        // Already have a comment (probably windows comment), skip this one.
        if (ShowTags) printf("Multiple comments in exif header\n");
        break;  // Already have a windows comment, skip this one.
      }

      // Comment is often padded with trailing spaces.  Remove these first.
      for (a = ByteCount;;) {
        a--;
        if ((ValuePtr)[a] == ' ') {
          (ValuePtr)[a] = '\0';
        } else {
          break;
        }
        if (a == 0) break;
      }

      // Copy the comment
      {
        int msiz                   = ExifLength - (ValuePtr - OffsetBase);
        if (msiz > ByteCount) msiz = ByteCount;
        if (msiz > MAX_COMMENT_SIZE - 1) msiz = MAX_COMMENT_SIZE - 1;
        if (msiz > 5 && memcmp(ValuePtr, "ASCII", 5) == 0) {
          for (a = 5; a < 10 && a < msiz; a++) {
            int c = (ValuePtr)[a];
            if (c != '\0' && c != ' ') {
              strncpy(ImageInfo.Comments, (char *)ValuePtr + a, msiz - a);
              break;
            }
          }
        } else {
          strncpy(ImageInfo.Comments, (char *)ValuePtr, msiz);
        }
      }
      break;

    case TAG_FNUMBER:
      // Simplest way of expressing aperture, so I trust it the most.
      // (overwrite previously computd value if there is one)
      ImageInfo.ApertureFNumber = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_APERTURE:
    case TAG_MAXAPERTURE:
      // More relevant info always comes earlier, so only use this field if we
      // don't
      // have appropriate aperture information yet.
      if (ImageInfo.ApertureFNumber == 0) {
        ImageInfo.ApertureFNumber =
            (float)exp(ConvertAnyFormat(ValuePtr, Format) * log(2) * 0.5);
      }
      break;

    case TAG_FOCALLENGTH:
      // Nice digital cameras actually save the focal length as a function
      // of how farthey are zoomed in.
      ImageInfo.FocalLength = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_SUBJECT_DISTANCE:
      // Inidcates the distacne the autofocus camera is focused to.
      // Tends to be less accurate as distance increases.
      ImageInfo.Distance = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_EXPOSURETIME:
      // Simplest way of expressing exposure time, so I trust it most.
      // (overwrite previously computd value if there is one)
      ImageInfo.ExposureTime = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_SHUTTERSPEED:
      // More complicated way of expressing exposure time, so only use
      // this value if we don't already have it from somewhere else.
      if (ImageInfo.ExposureTime == 0) {
        ImageInfo.ExposureTime =
            (float)(1 / exp(ConvertAnyFormat(ValuePtr, Format) * log(2)));
      }
      break;

    case TAG_FLASH:
      ImageInfo.FlashUsed = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_ORIENTATION:
      if (NumOrientations >= 2) {
        // Can have another orientation tag for the thumbnail, but if there's
        // a third one, things are strange.
        std::cout << "More than two orientation in Exif" << std::endl;
        break;
      }
      OrientationPtr[NumOrientations]       = ValuePtr;
      OrientationNumFormat[NumOrientations] = Format;
      if (NumOrientations == 0) {
        ImageInfo.Orientation = (int)ConvertAnyFormat(ValuePtr, Format);
      }
      if (ImageInfo.Orientation < 0 || ImageInfo.Orientation > 8) {
        std::cout << "Undefined rotation value " << ImageInfo.Orientation
                  << " in Exif" << std::endl;
      }
      NumOrientations += 1;
      break;

    case TAG_PIXEL_Y_DIMENSION:
    case TAG_PIXEL_X_DIMENSION:
      // Use largest of height and width to deal with images that have been
      // rotated to portrait format.
      a = (int)ConvertAnyFormat(ValuePtr, Format);
      if (ExifImageWidth < a) ExifImageWidth = a;
      break;

    case TAG_FOCAL_PLANE_XRES:
      FocalplaneXRes = ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_FOCAL_PLANE_UNITS:
      switch ((int)ConvertAnyFormat(ValuePtr, Format)) {
      case 1:
        FocalplaneUnits = 25.4;
        break;  // inch
      case 2:
        // According to the information I was using, 2 means meters.
        // But looking at the Cannon powershot's files, inches is the only
        // sensible value.
        FocalplaneUnits = 25.4;
        break;

      case 3:
        FocalplaneUnits = 10;
        break;  // centimeter
      case 4:
        FocalplaneUnits = 1;
        break;  // millimeter
      case 5:
        FocalplaneUnits = .001;
        break;  // micrometer
      }
      break;

    case TAG_EXPOSURE_BIAS:
      ImageInfo.ExposureBias = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_WHITEBALANCE:
      ImageInfo.Whitebalance = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_LIGHT_SOURCE:
      ImageInfo.LightSource = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_METERING_MODE:
      ImageInfo.MeteringMode = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_EXPOSURE_PROGRAM:
      ImageInfo.ExposureProgram = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_EXPOSURE_INDEX:
      if (ImageInfo.ISOequivalent == 0) {
        // Exposure index and ISO equivalent are often used interchangeably,
        // so we will do the same in jhead.
        // http://photography.about.com/library/glossary/bldef_ei.htm
        ImageInfo.ISOequivalent = (int)ConvertAnyFormat(ValuePtr, Format);
      }
      break;

    case TAG_EXPOSURE_MODE:
      ImageInfo.ExposureMode = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_ISO_EQUIVALENT:
      ImageInfo.ISOequivalent = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_DIGITALZOOMRATIO:
      ImageInfo.DigitalZoomRatio = (float)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_THUMBNAIL_OFFSET:
      ThumbnailOffset      = (unsigned)ConvertAnyFormat(ValuePtr, Format);
      DirWithThumbnailPtrs = DirStart;
      break;

    case TAG_THUMBNAIL_LENGTH:
      ThumbnailSize = (unsigned)ConvertAnyFormat(ValuePtr, Format);
      ImageInfo.ThumbnailSizeOffset = ValuePtr - OffsetBase;
      break;

    case TAG_EXIF_OFFSET:
      if (ShowTags) printf("%s    Exif Dir:", IndentString);

    case TAG_INTEROP_OFFSET:
      if (Tag == TAG_INTEROP_OFFSET && ShowTags)
        printf("%s    Interop Dir:", IndentString);
      {
        unsigned char *SubdirStart;
        SubdirStart = OffsetBase + Get32u(ValuePtr);
        if (SubdirStart < OffsetBase || SubdirStart > OffsetBase + ExifLength) {
          std::cout << "Illegal Exif or interop ofset directory link"
                    << std::endl;
        } else {
          ProcessExifDir(SubdirStart, OffsetBase, ExifLength, NestingLevel + 1);
        }
        continue;
      }
      break;

    case TAG_GPSINFO:
      if (ShowTags) printf("%s    GPS info dir:", IndentString);
      {
        unsigned char *SubdirStart;
        SubdirStart = OffsetBase + Get32u(ValuePtr);
        if (SubdirStart < OffsetBase || SubdirStart > OffsetBase + ExifLength) {
          std::cout << "Illegal GPS directory link in Exif" << std::endl;
        } else {
          // ProcessGpsInfo(SubdirStart, OffsetBase, ExifLength);
        }
        continue;
      }
      break;

    case TAG_FOCALLENGTH_35MM:
      // The focal length equivalent 35 mm is a 2.2 tag (defined as of April
      // 2002)
      // if its present, use it to compute equivalent focal length instead of
      // computing it from sensor geometry and actual focal length.
      ImageInfo.FocalLength35mmEquiv =
          (unsigned)ConvertAnyFormat(ValuePtr, Format);
      break;

    case TAG_DISTANCE_RANGE:
      // Three possible standard values:
      //   1 = macro, 2 = close, 3 = distant
      ImageInfo.DistanceRange = (int)ConvertAnyFormat(ValuePtr, Format);
      break;

#endif

    case TAG_X_RESOLUTION:
      if (NestingLevel ==
          0) {  // Only use the values from the top level directory
        ImageInfo.xResolution = (float)ConvertAnyFormat(ValuePtr, Format);
        // if yResolution has not been set, use the value of xResolution
        if (ImageInfo.yResolution == 0.0)
          ImageInfo.yResolution = ImageInfo.xResolution;
      }
      break;

    case TAG_Y_RESOLUTION:
      if (NestingLevel ==
          0) {  // Only use the values from the top level directory
        ImageInfo.yResolution = (float)ConvertAnyFormat(ValuePtr, Format);
        // if xResolution has not been set, use the value of yResolution
        if (ImageInfo.xResolution == 0.0)
          ImageInfo.xResolution = ImageInfo.yResolution;
      }
      break;

    case TAG_RESOLUTION_UNIT:
      if (NestingLevel ==
          0) {  // Only use the values from the top level directory
        ImageInfo.ResolutionUnit = (int)ConvertAnyFormat(ValuePtr, Format);
      }
      break;
    }
  }

#ifdef ReadAllTags
  {
    // In addition to linking to subdirectories via exif tags,
    // there's also a potential link to another directory at the end of each
    // directory.  this has got to be the result of a committee!
    unsigned char *SubdirStart;
    unsigned Offset;

    if (DIR_ENTRY_ADDR(DirStart, NumDirEntries) + 4 <=
        OffsetBase + ExifLength) {
      Offset = Get32u(DirStart + 2 + 12 * NumDirEntries);
      if (Offset) {
        SubdirStart = OffsetBase + Offset;
        if (SubdirStart > OffsetBase + ExifLength || SubdirStart < OffsetBase) {
          if (SubdirStart > OffsetBase &&
              SubdirStart < OffsetBase + ExifLength + 20) {
            // Jhead 1.3 or earlier would crop the whole directory!
            // As Jhead produces this form of format incorrectness,
            // I'll just let it pass silently
            if (ShowTags)
              printf("Thumbnail removed with Jhead 1.3 or earlier\n");
          } else {
            std::cout << "Illegal subdirectory link in Exif header"
                      << std::endl;
          }
        } else {
          if (SubdirStart <= OffsetBase + ExifLength) {
            if (ShowTags) printf("%s    Continued directory ", IndentString);
            ProcessExifDir(SubdirStart, OffsetBase, ExifLength,
                           NestingLevel + 1);
          }
        }
        if (Offset > ImageInfo.LargestExifOffset) {
          ImageInfo.LargestExifOffset = Offset;
        }
      }
    } else {
      // The exif header ends before the last next directory pointer.
    }
  }

  if (ThumbnailOffset) {
    ImageInfo.ThumbnailAtEnd = FALSE;

    if (DumpExifMap) {
      printf("Map: %05d-%05d: Thumbnail\n", ThumbnailOffset,
             ThumbnailOffset + ThumbnailSize);
    }

    if (ThumbnailOffset <= ExifLength) {
      if (ThumbnailSize > ExifLength - ThumbnailOffset) {
        // If thumbnail extends past exif header, only save the part that
        // actually exists.  Canon's EOS viewer utility will do this - the
        // thumbnail extracts ok with this hack.
        ThumbnailSize = ExifLength - ThumbnailOffset;
        if (ShowTags) printf("Thumbnail incorrectly placed in header\n");
      }
      // The thumbnail pointer appears to be valid.  Store it.
      ImageInfo.ThumbnailOffset = ThumbnailOffset;
      ImageInfo.ThumbnailSize   = ThumbnailSize;

      if (ShowTags) {
        printf("Thumbnail size: %u bytes\n", ThumbnailSize);
      }
    }
  }
#endif
}
