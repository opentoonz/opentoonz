#pragma once

#ifndef _QT_MOVIE_H_
#define _QT_MOVIE_H_

/**************************************************************
This APIs permit to handle MACINTOSH QUICKTIME movies.
They are intended only for creation; viewing and updating movies
is not currently supported.
Moreover, image insertion is not permitted; only appending.
To view .mov file, use quicktime tools  present on both NT(movieplayer)
and IRIX(movieplayer, moviemaker) platform.
The .mov created are cross-platform
**************************************************************/

#include "toonz4.6/toonz.h"
#include "tsound.h"

#undef TNZAPI
#ifdef TNZ_IS_IMAGELIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

#undef TNZAPI2
#ifdef TNZ_IS_COMMONLIB
#define TNZAPI2 TNZ_EXPORT_API
#else
#define TNZAPI2 TNZ_IMPORT_API
#endif

/*opaque definition for the movie object*/

typedef struct TNZMOVIE_DATA *TNZMOVIE;

typedef struct _RASTER *MY_RASTER; /*to avoid raster.h inclusion */

/* Warning: TNZMOVIE_TYPE makes sense only on NT; on IRIX is always RGBX*/

typedef enum TNZMOVIE_TYPE {
  TM_RGBX_TYPE = 0,
  TM_RGB16_TYPE,
  TM_RGB8_TYPE,
  TM_BW_TYPE,
  TM_HOW_MANY_TYPE
} TNZMOVIE_TYPE;

typedef enum TNZMOVIE_QUALITY {
  TM_MIN_QUALITY = 0,
  TM_LOW_QUALITY,
  TM_NORMAL_QUALITY,
  TM_HIGH_QUALITY,
  TM_MAX_QUALITY,
  TM_LOSSLESS_QUALITY,
  TM_HOW_MANY_QUALITY
} TNZMOVIE_QUALITY;

#ifdef WIN32
typedef int TNZMOVIE_COMPRESSION;
#else
typedef enum TNZMOVIE_COMPRESSION {
  TM_JPG_COMPRESSION = 0,
  TM_VIDEO_COMPRESSION,
  TM_ANIM_COMPRESSION,
  TM_CINEPAK_COMPRESSION,
  TM_HOW_MANY_COMPRESSION
} TNZMOVIE_COMPRESSION;

#endif

typedef unsigned long CodecQ;
typedef unsigned long CodecType;
/*---------------------------------------------------------------------------*/

TNZAPI2 TBOOL tnz_movies_available(void);
TNZAPI2 TBOOL tnz_avi_available(void);

TNZAPI2 TBOOL get_movie_codec_info(char ***quality_string, int *numQ, int *defQ,
                                   char ***compression_string, int *numC,
                                   int *defC);

/*
#ifdef WIN32
TNZAPI2 TBOOL get_movie_codec_val(char *quality_string,     ULONG *quality_val,
                                  char *compression_string,  ULONG
*compression_val);
#else
*/
TNZAPI2 TBOOL get_movie_codec_val(char *quality_string,
                                  TNZMOVIE_QUALITY *quality_val,
                                  char *compression_string,
                                  TNZMOVIE_COMPRESSION *compression_val);

/*
#endif
*/

#ifndef WIN32
TNZAPI2 TBOOL get_movie_codec_info(char ***quality_string, int *numQ, int *defQ,
                                   char ***compression_string, int *numC,
                                   int *defC);

#endif

TNZAPI2 CodecQ tm_get_quality(int quality);
TNZAPI2 CodecType tm_get_compression(int compression);

/**************************************************************
This function creates and open a new movie, ready to append images;
  NIL is returned on error; Remember to use tm_close
  when finished!!!
**************************************************************/

/*---------------------------------------------------------------------------*/

TNZAPI TNZMOVIE tm_create(char *fullpathname, TBOOL do_overwrite_file,
                          TNZMOVIE_TYPE type, int rate, int lx, int ly,
                          TNZMOVIE_QUALITY quality,
                          TNZMOVIE_COMPRESSION compression);

/**************************************************************
Warning: raster lx and ly must be the same of  the movie.
Raster is converted to the suitable format(cloning it)
**************************************************************/

TNZAPI TBOOL tm_append_raster(TNZMOVIE movie, MY_RASTER r);

/**************************************************************
Append a .wav(NT) or .aiff(IRIX) audio file to the movie.
The length of the resulting movie is forced to be equal to the video track,
so any overflowing audio is truncated. For this reason, is adviced to add
the audio when the video part has been completely added.
The position can be a negative value too;
If 'position' is >0, the audio track is added beginning from video frame
'position'.
If 'position' is <0, the audio is added starting from audio frame 'position',
at the beginning of video track.
**************************************************************/

TNZAPI TBOOL tm_add_audio_track(TNZMOVIE movie, char *audiofullpathname,
                                int img_offs, int audio_offs, int frames,
                                TS_STRACK audioTrack);

TNZAPI TBOOL tm_close(TNZMOVIE movie);

#endif
