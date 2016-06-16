#pragma once

#ifndef __TSOUND_H__
#define __TSOUND_H__

#include "tmacro.h"

#undef TNZAPI
#ifdef TNZ_IS_AUDIOLIB
#define TNZAPI TNZ_EXPORT_API
#else
#define TNZAPI TNZ_IMPORT_API
#endif

/*----------------------------------------------------------------------- */

#ifndef TSAPI
#define TSAPI extern
#endif

typedef int TS_RESULT;

enum {
  TS_NOERROR = 0,
  TS_BADFILE,
  TS_BADFORMAT,
  TS_ENV,
  TS_NOT_ALLOCATED,
  TS_BADDEVICE,
  TS_DEVICEBUSY,
  TS_NODRIVER,
  TS_PLAYERROR,
  TS_RECERROR,
  TS_NODATA,
  TS_OK_PLAYSTOPPED,
  TS_OK_RECSTOPPED

};

typedef unsigned long DWORD;
/*
        typedef unsigned char	BYTE;
        typedef int		BOOL;
*/
typedef unsigned char *PBYTE;

typedef const char *TS_STRING;

/*-----------------------------------------------------------------------------
 */
/* Sound data type */
/* MEGACAMBIO!!!! AUDIOFRAME intero */

typedef int AUDIOFRAME; /* -inf , +inf */
typedef float TIME;     /* -inf , +inf */
typedef DWORD SAMPLE;   /* 0    , +inf */
typedef DWORD TS_SIZE;
typedef float HERTZ;    /* 0    , +inf */
typedef float PRESSURE; /* -1.0 , +1.0 */
typedef float AMPLIFICATION;
typedef unsigned int VOLUME; /* 0    , 255  */

/* conversione: tempo -> frame */
#define TS_CINEMA_FRAMEDURATION ((double)1 / 24)
#define TS_PAL_FRAMEDURATION ((double)1 / 25)
#define TS_NTSC_FRAMEDURATION ((double)1 / 30)

/*------------------- DIGITAL AUDIO --------------------*/

/* channels number */
typedef enum { TS_MONO = 1, TS_STEREO = 2 } TS_DIGITAL_CHANNELS;

/*
 audio quality (sampling rate)
 mu: HERTZ
 */

typedef enum {
  TS_RATE_8000  = 8000,
  TS_RATE_11025 = 11025,
  TS_RATE_16000 = 16000,
  TS_RATE_22050 = 22050,
  TS_RATE_32000 = 32000,
  TS_RATE_44100 = 44100,
  TS_RATE_48000 = 48000
} TS_DIGITAL_QUALITY;

#define TS_VOICE_QUALITY TS_RATE_8000
#define TS_PHONE_QUALITY TS_RATE_11025
#define TS_RADIO_QUALITY TS_RATE_22050
#define TS_CD_QUALITY TS_RATE_44100

/*
 sample dimension
 mu: BIT
 */

typedef enum {
  TS_8BIT_SAMPLES  = 8,
  TS_16BIT_SAMPLES = 16,
#ifndef WIN32
  TS_24BIT_SAMPLES = 24
#else
  TS_24BIT_SAMPLES = 24
#endif
} TS_DIGITAL_SAMPLES;

/*
 bit masks for dwMask
*/
enum {
  TS_MASK_DEFAULT  = 0, /* per usare param. di default */
  TS_MASK_ORIGIN   = (1 << 0),
  TS_MASK_DURATION = (1 << 1),
  TS_MASK_QUALITY  = (1 << 2),
  TS_MASK_CHANNEL  = (1 << 3),
  TS_MASK_SAMPLE   = (1 << 4),
  TS_MASK_FILENAME = (1 << 5),
  TS_MASK_ALL      = TS_MASK_ORIGIN | TS_MASK_DURATION | TS_MASK_QUALITY |
                TS_MASK_CHANNEL | TS_MASK_SAMPLE | TS_MASK_FILENAME,
  TS_MASK_CREATE = TS_MASK_QUALITY | TS_MASK_CHANNEL | TS_MASK_SAMPLE
};

/*
 internal time unit = SAMPLE
 */
typedef struct _TS_STRACK_INFO {
  TIME tmOrigin;
  TIME tmDuration;
  char *tmFilename;
  /* DIGITAL audio dependent part - low level*/
  TS_DIGITAL_CHANNELS dChannels;
  TS_DIGITAL_QUALITY dSamplingRate;
  TS_DIGITAL_SAMPLES dBitsPerSample;
  /* --- */
  DWORD dwMask;
} * TS_STRACK_INFO;

typedef struct _TS_STRACK *TS_STRACK;

/*
 I/O devices
 */

typedef enum {
  TS_INPUT_LINE,
  TS_INPUT_MIC,
  TS_INPUT_DIGITAL,
  TS_INPUT_DUMMY /* per aprire in output */
} TS_DEVTYPE;

typedef enum { TS_UNIQUE = 0, TS_RIGHT, TS_LEFT } TS_CHAN;

/*
typedef struct _TS_DEVICE_INFO {
        TS_DEVTYPE 	input_source;
        VOLUME 		input_volume;
        VOLUME 		output_volume;
}*TS_DEVICE_INFO;
*/

typedef struct _TS_DEVICE *TS_DEVICE;

/* ------------------------ FUNCTIONS ------------------------------*/
/* N.B. Tutte le funzioni con from, to (src_from, src_to)
 *	 prendono TUTTA la track se from==TS_BEGIN && to==TS_END
 */

#define ALL -1 /* PROVVISORIO!!!!!*/
#define TS_BEGIN MININT
#define TS_END MAXINT

/*---------------------- Creation/Destruction ----------------------------*/

/* usa i campi di info dichiarati in dwFlagsi, per gli altri usa default */
TNZAPI TSAPI TS_STRACK tsCreateStrack(TS_STRACK_INFO info);
TNZAPI TSAPI TS_RESULT tsDestroyStrack(TS_STRACK strack);

/*---------------------- Conversion Functions -----------------------------*/
/*
        secs * tsFrameRate() => num.frames
        N.B.:Nessun controllo sui parametri passati (conv.indiscriminata)
*/

TNZAPI TSAPI AUDIOFRAME tsTimeToFrame(TIME time);
TNZAPI TSAPI TIME tsFrameToTime(AUDIOFRAME frames);
TNZAPI TSAPI TIME tsFracFrameToTime(float frac_frames);
TNZAPI TSAPI SAMPLE tsFrameToSample(AUDIOFRAME frames, TS_STRACK strack);
TNZAPI TSAPI AUDIOFRAME tsSampleToFrame(SAMPLE samples, TS_STRACK strack);
TNZAPI TSAPI AUDIOFRAME tsByteToFrame(TS_SIZE bytes, TS_STRACK strack);
TNZAPI TSAPI TS_SIZE tsFrameToByte(AUDIOFRAME frames, TS_STRACK strack);
TNZAPI TSAPI SAMPLE tsTimeToSample(TIME time, TS_STRACK strack);
TNZAPI TSAPI TIME tsSampleToTime(SAMPLE samples, TS_STRACK strack);

TNZAPI TSAPI void tsAreyousure_to_quit(void);

/*--------------------------- File Functions ----------------------------*/

/* riempie tutti i campi della info */
TNZAPI TSAPI TS_RESULT tsFileInfo(TS_STRING szFileName, TS_STRACK_INFO info);

TNZAPI TSAPI TS_STRACK tsLoad(TS_STRING szFileName, AUDIOFRAME from,
                              AUDIOFRAME to);

TNZAPI TSAPI TS_STRACK tsLoadSubsampled(TS_STRING szFileName,
                                        TS_STRACK_INFO desired_info,
                                        AUDIOFRAME from, AUDIOFRAME to);
/* if from==ALL && to==ALL the whole track is loaded */
/*
TNZAPI TSAPI TS_RESULT tsSave	( TS_STRACK strack, TS_STRING szFileName,
                           AUDIOFRAME from, AUDIOFRAME to );
*/
TNZAPI TSAPI TS_RESULT tsSave(TS_STRACK strack, TS_STRING szFileName);

/*------------------------- Edit Functions --------------------------------*/

TNZAPI TSAPI void tsDelete(TS_STRACK strack, AUDIOFRAME from, AUDIOFRAME to);
/* cuts a piece of DATA */
TNZAPI TSAPI void tsClear(TS_STRACK strack, AUDIOFRAME from, AUDIOFRAME to);
/* zeroes a piece of DATA */
TNZAPI TSAPI void tsBlank(TS_STRACK strack, AUDIOFRAME point, AUDIOFRAME range);
/* insert zeros into a track */

TNZAPI TSAPI TS_RESULT tsCopy(TS_STRACK src_strack, AUDIOFRAME src_from,
                              AUDIOFRAME src_to, TS_STRACK dest_strack,
                              AUDIOFRAME dest_from);
TNZAPI TSAPI TS_RESULT tsInsert(TS_STRACK src_strack, AUDIOFRAME src_from,
                                AUDIOFRAME src_to, TS_STRACK dest_strack,
                                AUDIOFRAME dest_from);
/* Insertion of data in an empty track : dest_strack->buffer = NULL */

/* ------------------------Manipolator Functions -------------------------*/

TNZAPI TSAPI TS_RESULT tsConvert(TS_STRACK src_strack,
                                 TS_STRACK_INFO dest_info);
TNZAPI TSAPI TS_RESULT tsAmplify(TS_STRACK strack, AMPLIFICATION amp);

/*----------------------- Miscellanea Functions ---------------------------*/

TNZAPI TSAPI AUDIOFRAME tsGetFrames(TS_STRACK strack);
TNZAPI TSAPI PBYTE tsBuffer(TS_STRACK strack);
/* riempie i campi di info richiesti in dwFlags */
TNZAPI TSAPI TS_RESULT tsInfo(TS_STRACK strack, TS_STRACK_INFO info);
TNZAPI TSAPI PRESSURE tsGetPressure(TS_STRACK strack, TIME time, TS_CHAN chan);
TNZAPI TSAPI PRESSURE tsGetMaxPressure(TS_STRACK strack, TS_CHAN chan);
TNZAPI TSAPI TS_STRING tsAudioFileExtension(void);

#ifndef WIN32
TNZAPI TSAPI int tsSupportDigitalIn(void);
#endif

/* only for tsNewPlay etc. */
TNZAPI TSAPI TBOOL tsNeedsRefill(void);
TNZAPI TSAPI TBOOL
tsRefill(void); /* call at least 4 times per sec while playing, */
                /* TRUE means needs to be called again */

/*------------------------ Play/Rec Functions -----------------------------*/
/* default asynch */
TNZAPI TSAPI TS_RESULT tsPlay(TS_STRACK strack, TS_DEVICE dev, AUDIOFRAME from,
                              AUDIOFRAME to);
TNZAPI TSAPI TS_RESULT tsNewPlay(TS_STRACK strack, TS_DEVICE dev,
                                 AUDIOFRAME from, AUDIOFRAME to);
TNZAPI TSAPI TS_RESULT tsPlayLoop(TS_STRACK strack, TS_DEVICE dev,
                                  AUDIOFRAME from, AUDIOFRAME to);
TNZAPI TSAPI TBOOL tsIsPlaying(TS_STRACK strack, TS_DEVICE dev);
TNZAPI TSAPI TBOOL tsIsNewPlaying(TS_STRACK strack, TS_DEVICE dev);
TNZAPI TSAPI TS_RESULT tsSyncPlay(TS_STRACK strack, TS_DEVICE dev,
                                  AUDIOFRAME from, AUDIOFRAME to);
TNZAPI TSAPI TS_RESULT tsStopPlaying(TS_STRACK strack, TS_DEVICE dev);
TNZAPI TSAPI TS_RESULT tsStopNewPlaying(TS_STRACK strack, TS_DEVICE dev);

TNZAPI TSAPI TS_RESULT tsRecord(TS_STRACK strack, TS_DEVICE dev,
                                AUDIOFRAME from, AUDIOFRAME to);
TNZAPI TSAPI TBOOL tsIsRecording(TS_STRACK strack, TS_DEVICE dev);
TNZAPI TSAPI TS_RESULT tsSyncRecord(TS_STRACK strack, TS_DEVICE dev,
                                    AUDIOFRAME from, AUDIOFRAME to);
TNZAPI TSAPI TS_RESULT tsStopRecording(TS_STRACK strack, TS_DEVICE dev);
/* TSAPI void      tsSetEndPlayCb  (void(*end_play_cb)(void));*/

/*------------------------- Device Functions ------------------------------*/

/* dinfo==NIL => usato vol. di default e microfono per input */
/* mode = "w" per Play; "r" per Record */

TNZAPI TSAPI TS_DEVICE tsInitDevice(TS_DEVTYPE type, TS_STRACK_INFO info,
                                    char *mode);
TNZAPI TSAPI TS_RESULT tsCloseDevice(TS_DEVICE dev);
TNZAPI TSAPI TBOOL tsCheckOutput(void);
TNZAPI TSAPI void tsSetVolume(TS_DEVICE dev, VOLUME vol);
TNZAPI TSAPI void tsSetInVolume(TS_DEVICE dev, VOLUME vol);
TNZAPI TSAPI VOLUME tsGetVolume(TS_DEVICE dev);

/*------------------------------------------------------------------------ */

#endif
