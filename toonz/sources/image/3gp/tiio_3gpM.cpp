

#ifndef __LP64__

#include <math.h>

#include "texception.h"
#include "tsound.h"
#include "tconvert.h"
#include "tpropertytype.h"
#include "trop.h"
#include "timageinfo.h"
#include "trasterimage.h"
#include "tmachine.h"

#include "../mov/tiio_movM.h"
#include "tiio_3gpM.h"

namespace {
int CompressionNoneId = 0;

class QuickTimeCleanUp;
class QuickTimeStuff {
public:
  static QuickTimeStuff *instance() {
    if (!m_singleton) m_singleton = new QuickTimeStuff();
    return m_singleton;
  }
  OSErr getStatus() { return m_status; }
  ~QuickTimeStuff() {}

private:
  QuickTimeStuff() : m_status(noErr) {
    m_status = noErr;
    EnterMovies();
  }

  static QuickTimeStuff *m_singleton;

  OSErr m_status;
  friend class QuickTimeCleanUp;  // questa DEVE essere friend, cosi' posso
                                  // controllare direttamente
  // lo stato del singleton.
};

class QuickTimeCleanUp {
public:
  QuickTimeCleanUp() {}
  ~QuickTimeCleanUp() { /*
                                      Nel caso si arrivasse qui senza il
                           singleton instanziato, e si facesse direttamente
                                      'delete QuickTimeStuff::instance();'
                           Quicktime non farebbe in tempo a terminare le
                                      sue routine di inizializzazione (che sono
                           ANCHE su altri thread) e la chiamata a
                                      TerminateQTML() causerebbe un crash
                                      */
    if (QuickTimeStuff::m_singleton) delete QuickTimeStuff::m_singleton;
  }
};

QuickTimeCleanUp cleanUp;
QuickTimeStuff *QuickTimeStuff::m_singleton = 0;

//------------------------------------------------------------------------------

enum QTLibError {
  QTNoError = 0x0000,
  QTNotInstalled,
  QTUnknownError,
  QTUnableToOpenFile,
  QTCantCreateFile,
  QTUnableToGetCompressorNames,
  QTUnableToCreateResource,
  QTUnableToUpdateMovie,
  QTBadTimeValue,
  QTUnableToDoMovieTask,
  QTUnableToSetTimeValue,
  QTUnableToSetMovieGWorld,
  QTUnableToSetMovieBox,
};

string buildQTErrorString(int ec) {
  switch (ec) {
  case QTNotInstalled:
    return "Can't create; ensure that quicktime is correctly installed on your "
           "machine";
  case QTUnknownError:
    return "Unknown error";
  case QTUnableToOpenFile:
    return "can't open file";
  case QTCantCreateFile:
    return "can't create movie";
  case QTUnableToGetCompressorNames:
    return "unable to get compressor name";
  case QTUnableToCreateResource:
    return "can't create resource";
  case QTUnableToUpdateMovie:
    return "unable to update movie";
  case QTBadTimeValue:
    return "bad frame number";
  case QTUnableToDoMovieTask:
    return "unable to do movie task";
  case QTUnableToSetTimeValue:
    return "unable to set time value";
  case QTUnableToSetMovieGWorld:
    return "unable to set movie graphic world";
  case QTUnableToSetMovieBox:
    return "unable to set movie box";

  default: { return "unknown error ('" + std::to_string(ec) + "')"; }
  }
}

//-----------------------------------------------------------
#ifdef WIN2
inline LPSTR AtlW2AHelper(LPSTR lpa, LPCWSTR lpw, int nChars, UINT acp) {
  assert(lpw != NULL);
  assert(lpa != NULL);
  // verify that no illegal character present
  // since lpa was allocated based on the size of lpw
  // don't worry about the number of chars
  lpa[0] = '\0';
  WideCharToMultiByte(acp, 0, lpw, -1, lpa, nChars, NULL, NULL);
  return lpa;
}
#endif
//-----------------------------------------------------------

string long2fourchar(TINT32 fcc) {
  string s;
  s += (char((fcc & 0xff000000) >> 24));
  s += (char((fcc & 0x00ff0000) >> 16));
  s += (char((fcc & 0x0000ff00) >> 8));
  s += (char((fcc & 0x000000ff) >> 0));

  return s;
}

const string CodecNamesId   = "PU_CodecName";
const string CodecQualityId = "PU_CodecQuality";

}  // namespace

//------------------------------------------------------------------------------
//  TImageWriter3gp
//------------------------------------------------------------------------------

class TImageWriter3gp : public TImageWriter {
public:
  TImageWriter3gp(const TFilePath &, int frameIndex, TLevelWriter3gp *);
  ~TImageWriter3gp() { m_lwm->release(); }
  bool is64bitOutputSupported() { return false; }

private:
  // not implemented
  TImageWriter3gp(const TImageWriter3gp &);
  TImageWriter3gp &operator=(const TImageWriter3gp &src);

public:
  void save(const TImageP &);
  int m_frameIndex;

private:
  TLevelWriter3gp *m_lwm;
};

//-----------------------------------------------------------
//  TImageReader3gp
//-----------------------------------------------------------
class TImageReader3gp : public TImageReader {
public:
  TImageReader3gp(const TFilePath &, int frameIndex, TLevelReader3gp *);
  ~TImageReader3gp() { m_lrm->release(); }

private:
  // not implemented
  TImageReader3gp(const TImageReader3gp &);
  TImageReader3gp &operator=(const TImageReader3gp &src);

public:
  TImageP load();
  void load(const TRasterP &rasP, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);
  int m_frameIndex;

  TDimension getSize() const { return m_lrm->getSize(); }
  TRect getBBox() const { return m_lrm->getBBox(); }

private:
  TLevelReader3gp *m_lrm;
};

//-----------------------------------------------------------
//-----------------------------------------------------------
//        TImageWriter3gp
//-----------------------------------------------------------

TImageWriter3gp::TImageWriter3gp(const TFilePath &path, int frameIndex,
                                 TLevelWriter3gp *lwm)
    : TImageWriter(path), m_lwm(lwm), m_frameIndex(frameIndex) {
  m_lwm->addRef();
}

//----------
namespace {

void copy(TRasterP rin, PixelXRGB *bufout, int lx, int ly,
          unsigned short rowbytes = 0) {
  rin->lock();
  TRaster32P rin32 = rin;
  assert(rin32);
  int lineOffset =
      (rowbytes == 0)
          ? lx
          : rowbytes / sizeof(PixelXRGB);  // in pixelsize, cioe 4 bytes

  PixelXRGB *rowout = &(bufout[(ly - 1) * lineOffset]) /*-startOffset*/;
  for (int y = 0; y < rin32->getLy(); y++) {
    PixelXRGB *pixout      = rowout;
    TPixelRGBM32 *pixin    = rin32->pixels(y);
    TPixelRGBM32 *pixinEnd = pixin + rin32->getLx();
    while (pixin < pixinEnd) {
      pixout->x = pixin->m;
      pixout->r = pixin->r;
      pixout->g = pixin->g;
      pixout->b = pixin->b;
      pixout++;
      pixin++;
    }
    rowout -= lineOffset;
  }
  rin->unlock();
}  // end function
};
//-----------------------------------------------------------
/*
TWriterInfo *TWriterInfo3gp::create(const string &)
{
return new TWriterInfo3gp();
}

//-----------------------------------------------------------

TWriterInfo3gp::TWriterInfo3gp(const TWriterInfo3gp&src)
               :TWriterInfo(src)
               ,m_codecTable(src.m_codecTable)
               ,m_qualityTable(src.m_qualityTable)
{
}

//-----------------------------------------------------------

TWriterInfo *TWriterInfo3gp::clone() const
{
return new TWriterInfo3gp(*this);
}

//-----------------------------------------------------------

TWriterInfo3gp::TWriterInfo3gp()
               :TWriterInfo()
{
if (QuickTimeStuff::instance()->getStatus()!=noErr)
  {
  return;
  }

CodecNameSpecListPtr codecList;

if (GetCodecNameList(&codecList, 0) !=noErr)
  {
  return;
  }

TIntEnumeration codecNames;
for (short i = 0; i < codecList->count; ++i)
  {
  CodecNameSpec *codecNameSpec = &(codecList->List[i]);
  UCHAR len = codecNameSpec->typeName[0];
  string name((char*)&codecNameSpec->typeName[1], len);
  if (name=="None")
          CompressionNoneId = i;

  codecNames.addItem(name, i);

  m_codecTable.insert(CodecTable::value_type(i, codecNameSpec->cType));
  }

DisposeCodecNameList(codecList);

addProperty(CodecNamesId, codecNames, 0);
int q = 0;

TIntEnumeration  codecQuality;

codecQuality.addItem("min quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecMinQuality));

codecQuality.addItem("low quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecLowQuality));

codecQuality.addItem("max quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecMaxQuality));

codecQuality.addItem("normal quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecNormalQuality));

codecQuality.addItem("high quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecHighQuality));

codecQuality.addItem("lossless quality",q);
m_qualityTable.insert(QualityTable::value_type(q++, codecLosslessQuality));

addProperty(CodecQualityId, codecQuality,0);

}

TWriterInfo3gp::~TWriterInfo3gp()
{}

*/
//-----------------------------------------------------------
void TImageWriter3gp::save(const TImageP &img) {
  m_lwm->save(img, m_frameIndex);
}

//-----------------------------------------------------------

namespace {
void getFSRefFromPosixPath(const char *filepath, FSRef *fileRef, bool writing) {
  const UInt8 *filename = (const UInt8 *)filepath;
  if (writing) {
    FILE *fp = fopen(filepath, "wb");
    assert(fp);
    fclose(fp);
  }
  OSErr err = FSPathMakeRef(filename, fileRef, 0);
  // assert(err==noErr);
}

void getFSSpecFromPosixPath(const char *filepath, FSSpec *fileSpec,
                            bool writing) {
  FSRef fileRef;
  getFSRefFromPosixPath(filepath, &fileRef, writing);
  OSErr err = FSGetCatalogInfo(&fileRef, kFSCatInfoNone, 0, 0, fileSpec, 0);
  assert(err == noErr);
}
}
//-----------------------------------------------------------

void TLevelWriter3gp::save(const TImageP &img, int frameIndex) {
  if (m_cancelled) return;

  TRasterImageP image(img);
  int lx = image->getRaster()->getLx();
  int ly = image->getRaster()->getLy();
  // void *buffer = image->getRaster()->getRawData();
  int pixSize = image->getRaster()->getPixelSize();
  if (pixSize != 4)
    throw TImageException(getFilePath(), "Unsupported pixel type");

  QMutexLocker sl(&m_mutex);

  if (!m_properties) m_properties = new Tiio::MovWriterProperties();

  Tiio::MovWriterProperties *prop = (Tiio::MovWriterProperties *)(m_properties);

  // CodecType compression = StandardCompressionType;  prop->getCurrentCodec();
  // CodecQ quality = StandardQualityType;  prop->getCurrentQuality();

  if (!m_initDone) {
    // FSSpec fspec;
    Rect frame;
    long max_compressed_size;
    QDErr err;

    m_videoTrack = NewMovieTrack(m_movie, FixRatio((short)lx, 1),
                                 FixRatio((short)ly, 1), kNoVolume);

    if ((err = GetMoviesError() != noErr))
      throw TImageException(getFilePath(), "can't create video track");

    m_dataRef    = nil;
    m_hMovieData = NewHandle(0);

    // Construct the Handle data reference
    err = PtrToHand(&m_hMovieData, &m_dataRef, sizeof(Handle));

    if ((err = GetMoviesError() != noErr))
      throw TImageException(getFilePath(), "can't create Data Ref");

    m_videoMedia =
        NewTrackMedia(m_videoTrack, VideoMediaType, (TINT32)m_frameRate,
                      m_dataRef, HandleDataHandlerSubType);

    OpenADefaultComponent(MovieExportType, '3gpp', &m_myExporter);

    //  err = (short)MovieExportDoUserDialog(m_myExporter, m_movie, 0, 0, 0,
    //  &m_cancelled);

    //  if (m_cancelled)
    //	  throw TImageException(getFilePath(), "User abort of 3GP render");
    if ((err = GetMoviesError() != noErr))
      throw TImageException(getFilePath(), "can't create video media");
    if ((err = BeginMediaEdits(m_videoMedia)) != noErr)
      throw TImageException(getFilePath(), "can't begin edit video media");
    frame.left   = 0;
    frame.top    = 0;
    frame.right  = lx;
    frame.bottom = ly;

#if 0
  if ((err = NewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0))!=noErr)
#else /* Mac OSX 10.7 later */
    if ((err = QTNewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0)) != noErr)
#endif
    throw TImageException(getFilePath(), "can't create movie buffer");
#ifdef WIN32
    LockPixels(m_gworld->portPixMap);
    if ((err = GetMaxCompressionSize(m_gworld->portPixMap, &frame, 0, quality,
                                     compression, anyCodec,
                                     &max_compressed_size)) != noErr)
      throw TImageException(getFilePath(), "can't get max compression size");

#else

#if 0
  PixMapHandle pixmapH = GetPortPixMap (m_gworld);
  LockPixels(pixmapH);
#else
    PixMapHandle pixmapH = NULL;
#endif
    max_compressed_size = lx * ly * 4 * 20;

/*if ((err = GetMaxCompressionSize(pixmapH, &frame, 0,
                                quality,  compression,anyCodec,
                                 &max_compressed_size))!=noErr)
    throw TImageException(getFilePath(), "can't get max compression size");*/
#endif

    m_compressedData = NewHandle(max_compressed_size);

    if ((err = MemError()) != noErr)
      throw TImageException(getFilePath(),
                            "can't allocate compressed data for movie");

    MoveHHi(m_compressedData);
    HLock(m_compressedData);
    if ((err = MemError()) != noErr)
      throw TImageException(getFilePath(), "can't allocate img handle");

#if 0
  m_pixmap = GetGWorldPixMap(m_gworld);
  
  
  if (!LockPixels(m_pixmap))
    throw TImageException(getFilePath(), "can't lock pixels");

  buf    = (PixelXRGB*) GetPixBaseAddr(m_pixmap);
#else
    m_pixmap          = NULL;
    buf               = NULL;
#endif
    buf_lx = lx;
    buf_ly = ly;

    m_initDone = true;
  }

  unsigned short rowBytes =
      (unsigned short)(((short)(*(m_pixmap))->rowBytes & ~(3 << 14)));

  Rect frame;
  ImageDescriptionHandle img_descr;
  Ptr compressed_data_ptr;
  QDErr err;

  frame.left   = 0;
  frame.top    = 0;
  frame.right  = lx;
  frame.bottom = ly;

  TRasterP ras = image->getRaster();
#ifdef WIN32
  compressed_data_ptr = StripAddress(*(m_compressedData));
  copy(ras, buf, buf_lx, buf_ly);
#else
  compressed_data_ptr = *m_compressedData;
  copy(ras, buf, buf_lx, buf_ly, rowBytes);
#endif
  img_descr = (ImageDescriptionHandle)NewHandle(4);

#ifdef WIN32
  if ((err = CompressImage(m_gworld->portPixMap, &frame, quality, compression,
                           img_descr, compressed_data_ptr)) != noErr)
    throw TImageException(getFilePath(), "can't compress image");
#else

#if 0
 PixMapHandle pixmapH = GetPortPixMap (m_gworld);
 if ((err = CompressImage(pixmapH, 
	                 &frame, 
  			 codecNormalQuality, kJPEGCodecType,
			 img_descr, compressed_data_ptr))!=noErr)
	{
  throw TImageException(getFilePath(), "can't compress image");
}
#endif
#endif

  if ((err = AddMediaSample(
           m_videoMedia, m_compressedData, 0, (*img_descr)->dataSize, 1,
           (SampleDescriptionHandle)img_descr, 1, 0, 0)) != noErr)
    throw TImageException(getFilePath(), "can't add image to movie media");

  DisposeHandle((Handle)img_descr);
}

//-----------------------------------------------------------
//        TLevelWriterMov
//-----------------------------------------------------------

TLevelWriter3gp::TLevelWriter3gp(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo)
    , m_initDone(false)
    , m_IOError(QTNoError)
    , m_pixmap(0)
    , m_compressedData(0)
    , m_gworld(0)
    , m_videoMedia(0)
    , m_videoTrack(0)
    , m_movie(0)
    , m_cancelled(false)
    , m_soundDataRef(0)
    , m_hSoundMovieData(0)

{
  m_frameRate = 12.;
  QDErr err;
  // FSSpec fspec;
  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    m_IOError = QTNotInstalled;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }

  m_movie = NewMovie(0);
  if ((err = GetMoviesError() != noErr))
    throw TImageException(getFilePath(), "can't create movie");
}

//-----------------------------------------------------------

#ifdef _DEBUG
#define FailIf(cond, handler)                                                  \
  if (cond) {                                                                  \
    DebugStr((ConstStr255Param) #cond " goto " #handler);                      \
    goto handler;                                                              \
  } else                                                                       \
    0
#else
#define FailIf(cond, handler)                                                  \
  if (cond) {                                                                  \
    goto handler;                                                              \
  } else                                                                       \
    0
#endif

#ifdef _DEBUG
#define FailWithAction(cond, action, handler)                                  \
  if (cond) {                                                                  \
    DebugStr((ConstStr255Param) #cond " goto " #handler);                      \
    { action; }                                                                \
    goto handler;                                                              \
  } else                                                                       \
    0
#else
#define FailWithAction(cond, action, handler)                                  \
  if (cond) {                                                                  \
    { action; }                                                                \
    goto handler;                                                              \
  } else                                                                       \
    0
#endif

void TLevelWriter3gp::saveSoundTrack(TSoundTrack *st) {
  Track theTrack;
  OSErr myErr = noErr;
  SoundDescriptionV1Handle mySampleDesc;
  Media myMedia;
  Handle myDestHandle;
  SoundComponentData sourceInfo;
  SoundComponentData destInfo;
  SoundConverter converter;
  CompressionInfo compressionInfo;
  int err;

  if (!st) throw TException("null reference to soundtrack");

  if (st->getBitPerSample() != 16) {
    throw TImageException(m_path, "Only 16 bits per sample is supported");
  }

  theTrack = NewMovieTrack(m_movie, 0, 0, kFullVolume);
  myErr    = GetMoviesError();
  if (myErr != noErr)
    throw TImageException(m_path, "error creating audio track");

  FailIf(myErr != noErr, CompressErr);

  myDestHandle = NewHandle(0);

  FailWithAction(myDestHandle == NULL, myErr = MemError(), NoDest);

  *myDestHandle = (char *)st->getRawData();

  //////////
  //
  // create a media for the track passed in
  //
  //////////

  // set new track to be a sound track
  m_soundDataRef    = nil;
  m_hSoundMovieData = NewHandle(0);

  // Construct the Handle data reference
  err = PtrToHand(&m_hSoundMovieData, &m_soundDataRef, sizeof(Handle));

  if ((err = GetMoviesError() != noErr))
    throw TImageException(getFilePath(), "can't create Data Ref");

  myMedia = NewTrackMedia(theTrack, SoundMediaType, st->getSampleRate(),
                          m_soundDataRef,
                          HandleDataHandlerSubType);  // track->rate >> 16

  myErr = GetMoviesError();
  if (myErr != noErr)
    throw TImageException(m_path, "error setting audio track");
  FailIf(myErr != noErr, Exit);

  // start a media editing session
  myErr = BeginMediaEdits(myMedia);
  if (myErr != noErr)
    throw TImageException(m_path, "error beginning edit audio track");

  FailIf(myErr != noErr, Exit);

  sourceInfo.flags       = 0x0;
  sourceInfo.format      = kSoundNotCompressed;
  sourceInfo.numChannels = st->getChannelCount();
  sourceInfo.sampleSize  = st->getBitPerSample();
  sourceInfo.sampleRate  = st->getSampleRate();
  sourceInfo.sampleCount = st->getSampleCount();
  sourceInfo.buffer      = (unsigned char *)st->getRawData();
  sourceInfo.reserved    = 0x0;

  destInfo.flags = kNoSampleRateConversion | kNoSampleSizeConversion |
                   kNoSampleFormatConversion | kNoChannelConversion |
                   kNoDecompression | kNoVolumeConversion |
                   kNoRealtimeProcessing;

  destInfo.format = k16BitNativeEndianFormat;

  destInfo.numChannels = st->getChannelCount();
  destInfo.sampleSize  = st->getBitPerSample();
  destInfo.sampleRate  = st->getSampleRate();
  destInfo.sampleCount = st->getSampleCount();
  destInfo.buffer      = (unsigned char *)st->getRawData();
  destInfo.reserved    = 0x0;

  SoundConverterOpen(&sourceInfo, &destInfo, &converter);

  myErr =
      SoundConverterGetInfo(converter, siCompressionFactor, &compressionInfo);
  if (myErr != noErr)
    throw TImageException(m_path, "error getting audio converter info");

  myErr = GetCompressionInfo(fixedCompression, sourceInfo.format,
                             sourceInfo.numChannels, sourceInfo.sampleSize,
                             &compressionInfo);
  if (myErr != noErr)
    throw TImageException(m_path, "error getting audio compression info");
  FailIf(myErr != noErr, ConverterErr);

  compressionInfo.bytesPerFrame =
      compressionInfo.bytesPerPacket * destInfo.numChannels;

  //////////
  //
  // create a sound sample description
  //
  //////////

  // use the SoundDescription format 1 because it adds fields for data size
  // information
  // and is required by AddSoundDescriptionExtension if an extension is required
  // for the compression format

  mySampleDesc =
      (SoundDescriptionV1Handle)NewHandleClear(sizeof(SoundDescriptionV1));
  FailWithAction(myErr != noErr, myErr = MemError(), Exit);

  (**mySampleDesc).desc.descSize      = sizeof(SoundDescriptionV1);
  (**mySampleDesc).desc.dataFormat    = destInfo.format;
  (**mySampleDesc).desc.resvd1        = 0;
  (**mySampleDesc).desc.resvd2        = 0;
  (**mySampleDesc).desc.dataRefIndex  = 1;
  (**mySampleDesc).desc.version       = 1;
  (**mySampleDesc).desc.revlevel      = 0;
  (**mySampleDesc).desc.vendor        = 0;
  (**mySampleDesc).desc.numChannels   = destInfo.numChannels;
  (**mySampleDesc).desc.sampleSize    = destInfo.sampleSize;
  (**mySampleDesc).desc.compressionID = 0;
  (**mySampleDesc).desc.packetSize    = 0;
  (**mySampleDesc).desc.sampleRate    = st->getSampleRate() << 16;
  (**mySampleDesc).samplesPerPacket   = compressionInfo.samplesPerPacket;
  (**mySampleDesc).bytesPerPacket     = compressionInfo.bytesPerPacket;
  (**mySampleDesc).bytesPerFrame      = compressionInfo.bytesPerFrame;
  (**mySampleDesc).bytesPerSample     = compressionInfo.bytesPerSample;

  //////////
  //
  // add samples to the media
  //
  //////////

  myErr = AddMediaSample(
      myMedia, myDestHandle, 0,
      destInfo.sampleCount * compressionInfo.bytesPerFrame, 1,
      (SampleDescriptionHandle)mySampleDesc,
      destInfo.sampleCount * compressionInfo.samplesPerPacket, 0, NULL);

  if (myErr != noErr)
    throw TImageException(m_path, "error adding audio samples");

  FailIf(myErr != noErr, MediaErr);

  myErr = EndMediaEdits(myMedia);
  if (myErr != noErr) throw TImageException(m_path, "error ending audio edit");

  FailIf(myErr != noErr, MediaErr);

  //////////
  //
  // insert the media into the track
  //
  //////////

  myErr =
      InsertMediaIntoTrack(theTrack, 0, 0, GetMediaDuration(myMedia), fixed1);
  if (myErr != noErr)
    throw TImageException(m_path, "error inserting audio track");

  FailIf(myErr != noErr, MediaErr);
  goto Done;

ConverterErr:
NoDest:
CompressErr:
Exit:

Done:

MediaErr:
  if (mySampleDesc != NULL) DisposeHandle((Handle)mySampleDesc);

  if (converter) SoundConverterClose(converter);

  if (myErr != noErr) throw TImageException(m_path, "error saving audio track");
}

//-----------------------------------------------------------

TLevelWriter3gp::~TLevelWriter3gp() {
#if 0
if (m_pixmap) 
  UnlockPixels(m_pixmap);
if (m_compressedData)
  DisposeHandle(m_compressedData);
if (m_gworld)
  DisposeGWorld(m_gworld);
#endif

  QDErr err;

  if (m_videoMedia)
    if ((err = EndMediaEdits(m_videoMedia)) != noErr) {
    }  // throw TImageException(getFilePath(), "can't end edit media");

  if (m_videoTrack)
    if ((err = InsertMediaIntoTrack(m_videoTrack, 0, 0,
                                    GetMediaDuration(m_videoMedia), fixed1))) {
    }  // throw TImageException(getFilePath(), "can't insert media into track");

  short resId = movieInDataForkResID;
  if (m_movie) {
    FSSpec fspec;
    long myFlags = 0L;
    OSErr myErr  = noErr;
    // UCHAR myCancelled = FALSE;

    getFSSpecFromPosixPath(::to_string(m_path).c_str(), &fspec, true);

    myFlags = createMovieFileDeleteCurFile;  // |
    // movieFileSpecValid | movieToFileOnlyExport;

    myErr =
        ConvertMovieToFile(m_movie,                 // the movie to convert
                           NULL,                    // all tracks in the movie
                           &fspec,                  // the output file
                           '3gpp',                  // the output file type
                           FOUR_CHAR_CODE('TVOD'),  // the output file creator
                           smSystemScript,          // the script
                           &resId,         // no resource ID to be returned
                           myFlags,        // export flags
                           m_myExporter);  // no specific exp
  }

  DisposeHandle(m_hMovieData);
  DisposeHandle(m_dataRef);
  if (m_hSoundMovieData) DisposeHandle(m_hSoundMovieData);

  if (m_refNum) CloseMovieFile(m_refNum);
  DisposeMovie(m_movie);
}

//-----------------------------------------------------------

TImageWriterP TLevelWriter3gp::getFrameWriter(TFrameId fid) {
  if (m_cancelled) return 0;
  if (m_IOError) throw TImageException(m_path, buildQTErrorString(m_IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index            = fid.getNumber() - 1;
  TImageWriter3gp *iwm = new TImageWriter3gp(m_path, index, this);
  return TImageWriterP(iwm);
}

//-----------------------------------------------------------

TLevelReader3gp::TLevelReader3gp(const TFilePath &path)
    : TLevelReader(path)
    , m_IOError(QTNoError)
    , m_track(0)
    , m_movie(0)
    , m_depth(0)
//                ,m_timeScale(0)
{
  FSSpec fspec;
  QDErr err;
  Boolean dataRefWasChanged;
  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    m_IOError = QTNotInstalled;
    return;
  }

  std::string const str_path = ::to_string(m_path).c_str();
  FSMakeFSSpec(0, 0, reinterpret_cast<unsigned char const *>(str_path.c_str()),
               &fspec);
  getFSSpecFromPosixPath(str_path.c_str(), &fspec, false);

  if ((err = OpenMovieFile(&fspec, &m_refNum, fsRdPerm))) {
    m_IOError = QTUnableToOpenFile;
    return;
  }

  m_resId = 0;
  Str255 name;
  err = NewMovieFromFile(&m_movie, m_refNum, &m_resId, name, fsRdPerm,
                         &dataRefWasChanged);

  int numTracks = GetMovieTrackCount(m_movie);
  assert(numTracks == 1 || numTracks == 2);

  m_track =
      GetMovieIndTrackType(m_movie, 1, VideoMediaType, movieTrackMediaType);

  // m_track=GetMovieTrack(m_movie,numTracks);

  ImageDescriptionHandle imageH;
  imageH = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription));
  TINT32 index   = 1;
  Media theMedia = GetTrackMedia(m_track);

  GetMediaSampleDescription(theMedia, index, (SampleDescriptionHandle)imageH);
  ImageDescriptionPtr imagePtr    = *imageH;
  m_lx                            = imagePtr->width;
  m_ly                            = imagePtr->height;
  m_depth                         = imagePtr->depth;
  m_info                          = new TImageInfo();
  m_info->m_lx                    = m_lx;
  m_info->m_ly                    = m_ly;
  Tiio::MovWriterProperties *prop = new Tiio::MovWriterProperties();
  m_info->m_properties            = prop;

  DisposeHandle((Handle)imageH);

  m_info->m_frameRate = GetMediaTimeScale(theMedia);
}

//------------------------------------------------

//------------------------------------------------

//------------------------------------------------
// TImageReader3gp
//------------------------------------------------

TImageReader3gp::TImageReader3gp(const TFilePath &path, int frameIndex,
                                 TLevelReader3gp *lrm)
    : TImageReader(path), m_lrm(lrm), m_frameIndex(frameIndex) {
  m_lrm->addRef();
}

//------------------------------------------------

TLevelReader3gp::~TLevelReader3gp() {
  StopMovie(m_movie);

  if (m_refNum) CloseMovieFile(m_refNum);
  if (m_movie) DisposeMovie(m_movie);
  // ExitMovies();
  // TerminateQTML();
}

//------------------------------------------------

TLevelP TLevelReader3gp::loadInfo() {
  TLevelP level;
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  OSType mediaType = VisualMediaCharacteristic;
  TimeValue nextTime, currentTime;
  currentTime = 0;
  nextTime    = 0;
  // per il primo
  int f = 1;
  // io vorrei fare '|', ma sul manuale c'e' scritto '+'
  GetMovieNextInterestingTime(m_movie, nextTimeMediaSample + nextTimeEdgeOK, 1,
                              &mediaType, currentTime, 0, &nextTime, 0);
  if (nextTime != -1) {
    TFrameId frame(f);
    level->setFrame(frame, TImageP());
    currentTimes.push_back(nextTime);
    f++;
  }
  currentTime = nextTime;
  while (nextTime != -1) {
    GetMovieNextInterestingTime(m_movie, nextTimeMediaSample, 1, &mediaType,
                                currentTime, 0, &nextTime, 0);
    if (nextTime != -1) {
      TFrameId frame(f);
      level->setFrame(frame, TImageP());
      currentTimes.push_back(nextTime);
      f++;
    }
    currentTime = nextTime;
  }
  return level;
}

//------------------------------------------------

TImageP TImageReader3gp::load() {
  TRasterPT<TPixelRGBM32> ret(m_lrm->getSize());

  m_lrm->load(ret, m_frameIndex, TPointI(), 1, 1);

  return TRasterImageP(ret);
}

//------------------------------------------------

void TImageReader3gp::load(const TRasterP &rasP, const TPoint &pos, int shrinkX,
                           int shrinkY) {
  if ((shrinkX != 1) || (shrinkY != 1) || (pos != TPoint(0, 0))) {
    TImageReader::load(rasP, pos, shrinkX, shrinkY);
  } else
    m_lrm->load(rasP, m_frameIndex, pos, shrinkX, shrinkY);
}
//------------------------------------------------
inline void setMatteAndYMirror(const TRaster32P &ras) {
  ras->lock();
  TPixel32 *upRow = ras->pixels();
  TPixel32 *dwRow = ras->pixels(ras->getLy() - 1);
  int hLy  = (int)(ras->getLy() / 2. + 0.5);  // piccola pessimizzazione...
  int wrap = ras->getWrap();
  int lx   = ras->getLx();
  TPixel32 *upPix   = 0;
  TPixel32 *lastPix = ras->pixels(hLy);
  while (upPix < lastPix) {
    upPix            = upRow;
    TPixel32 *dwPix  = dwRow;
    TPixel32 *endPix = upPix + lx;
    while (upPix < endPix) {
      TPixel32 tmpPix(upPix->r, upPix->g, upPix->b, 0xff);
      *upPix   = *dwPix;
      upPix->m = 0xff;
      *dwPix   = tmpPix;
      ++upPix;
      ++dwPix;
    }
    upRow += wrap;
    dwRow -= wrap;
  }
  ras->unlock();
}

//------------------------------------------------

void TLevelReader3gp::load(const TRasterP &rasP, int frameIndex,
                           const TPoint &pos, int shrinkX, int shrinkY) {
  TRaster32P ras = rasP;

  {
    QMutexLocker sl(&m_mutex);
    ras->lock();
    if (m_IOError != QTNoError) goto error;

    Rect rect;
    rect.right  = pos.x + ras->getLx();
    rect.left   = pos.x;
    rect.bottom = pos.y + ras->getLy();
    rect.top    = pos.y;

    GWorldPtr offscreenGWorld;
    OSErr err;

#if defined TNZ_MACHINE_CHANNEL_ORDER_BGRM
    OSType pixelFormat = k32BGRAPixelFormat;
#elif defined TNZ_MACHINE_CHANNEL_ORDER_MRGB
    OSType pixelFormat = k32ARGBPixelFormat;
#endif

    err = QTNewGWorldFromPtr(&offscreenGWorld, pixelFormat, &rect, 0, 0, 0,
                             ras->getRawData(), ras->getWrap() * 4);

    if (err != noErr) {
      m_IOError = QTUnableToCreateResource;
      goto error;
    }

    SetMovieBox(m_movie, &rect);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieBox;
#if 0
    DisposeGWorld(offscreenGWorld);
#endif
      goto error;
    }

#if 0
  SetMovieGWorld(m_movie, offscreenGWorld, GetGWorldDevice(offscreenGWorld));
#endif
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieGWorld;
#if 0
    DisposeGWorld(offscreenGWorld);
#endif
      goto error;
    }

    TimeValue currentTime = currentTimes[frameIndex];

    SetMovieTimeValue(m_movie, currentTime);

    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetTimeValue;
#if 0
    DisposeGWorld(offscreenGWorld);
#endif
      goto error;
    }

    err = UpdateMovie(m_movie);
    if (err != noErr) {
      m_IOError = QTUnableToUpdateMovie;
#if 0
    DisposeGWorld(offscreenGWorld);
#endif
      goto error;
    }

    MoviesTask(m_movie, 0);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToDoMovieTask;
#if 0
    DisposeGWorld(offscreenGWorld);
#endif
      goto error;
    }

    SetMovieGWorld(m_movie, 0, 0);
#if 0
  DisposeGWorld(offscreenGWorld);
#endif
    ras->unlock();
  }

  if (m_depth != 32) {
    setMatteAndYMirror(rasP);
  } else {
    rasP->yMirror();
  }

  return;

error:
  ras->unlock();
  throw TImageException(m_path, buildQTErrorString(m_IOError));
}

//------------------------------------------------

TImageReaderP TLevelReader3gp::getFrameReader(TFrameId fid) {
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReader3gp *irm = new TImageReader3gp(m_path, index, this);
  return TImageReaderP(irm);
}

#endif  //__LP64__
