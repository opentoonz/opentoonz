/*-------------------------------------------------------------
tiio_jpg_exif.h
Based on source code of a public domain software "Exif Jpeg header manipulation
tool (jhead)" by Matthias Wandel.
For now it is used only for obtaining resolution values.
http://www.sentex.net/~mwandel/jhead/
-------------------------------------------------------------*/
#pragma once
#ifndef TIIO_JPG_EXIF_H
#define TIIO_JPG_EXIF_H

#include <stdlib.h>

// undefining ReadAllTags will load only resolution infomation
#define ReadAllTags
#undef ReadAllTags

#define MAX_COMMENT_SIZE 16000
#define MAX_DATE_COPIES 10

#ifdef _WIN32
#define PATH_MAX _MAX_PATH
#define SLASH '\\'
#else
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define SLASH '/'
#endif

class JpgExifReader {
  enum FORMATS {
    FMT_BYTE = 1,
    FMT_STRING,
    FMT_USHORT,
    FMT_ULONG,
    FMT_URATIONAL,
    FMT_SBYTE,
    FMT_UNDEFINED,
    FMT_SSHORT,
    FMT_SLONG,
    FMT_SRATIONAL,
    FMT_SINGLE,
    FMT_DOUBLE
  };

  int NUM_FORMATS = FMT_DOUBLE;

  struct ExifImageInfo {
    float xResolution = 0.0;
    float yResolution = 0.0;
    int ResolutionUnit;
#ifdef ReadAllTags
    char FileName[PATH_MAX + 1];
    time_t FileDateTime;

    unsigned FileSize;
    char CameraMake[32];
    char CameraModel[40];
    char DateTime[20];
    unsigned Height, Width;
    int Orientation;
    int IsColor;
    int Process;
    int FlashUsed;
    float FocalLength;
    float ExposureTime;
    float ApertureFNumber;
    float Distance;
    float CCDWidth;
    float ExposureBias;
    float DigitalZoomRatio;
    int FocalLength35mmEquiv;  // Exif 2.2 tag - usually not present.
    int Whitebalance;
    int MeteringMode;
    int ExposureProgram;
    int ExposureMode;
    int ISOequivalent;
    int LightSource;
    int DistanceRange;

    char Comments[MAX_COMMENT_SIZE];
    int CommentWidthchars;  // If nonzero, widechar comment, indicates number of
                            // chars.

    unsigned ThumbnailOffset;    // Exif offset to thumbnail
    unsigned ThumbnailSize;      // Size of thumbnail.
    unsigned LargestExifOffset;  // Last exif data referenced (to check if
                                 // thumbnail is at end)

    char ThumbnailAtEnd;  // Exif header ends with the thumbnail
    // (we can only modify the thumbnail if its at the end)
    int ThumbnailSizeOffset;

    int DateTimeOffsets[MAX_DATE_COPIES];
    int numDateTimeTags = 0;

    int GpsInfoPresent;
    char GpsLat[31];
    char GpsLong[31];
    char GpsAlt[20];

    int QualityGuess;
#endif
  } ImageInfo;

#ifdef ReadAllTags
  unsigned char* DirWithThumbnailPtrs;
  double FocalplaneXRes;
  double FocalplaneUnits;
  int ExifImageWidth;

  // for fixing the rotation.
  void* OrientationPtr[2];
  int OrientationNumFormat[2];
  int NumOrientations = 0;
#endif

  int MotorolaOrder = 0;

  int Get16u(void* Short);
  unsigned Get32u(void* Long);
  int Get32s(void* Long);
  void PrintFormatNumber(void* ValuePtr, int Format, int ByteCount);
  double ConvertAnyFormat(void* ValuePtr, int Format);

public:
  void process_EXIF(unsigned char* ExifSection, unsigned int length);
  void ProcessExifDir(unsigned char* DirStart, unsigned char* OffsetBase,
                      unsigned ExifLength, int NestingLevel);

  // obtaining resolution info
  bool containsResolution() { return ImageInfo.xResolution != 0.0; }
  float getXResolution() { return ImageInfo.xResolution; }
  float getYResolution() { return ImageInfo.yResolution; }
  int getResolutionUnit() { return ImageInfo.ResolutionUnit; }
};

#endif