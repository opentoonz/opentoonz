

#if !defined(x64) && !(defined(__GNUC__) && defined(_WIN32))

#include <cstdint>

#include "texception.h"
#include "tsound.h"
#include "tconvert.h"
#include "tpropertytype.h"
#include "../mov/tiio_mov.h"
#include "movsettings.h"
#include "trasterimage.h"
#include "tsystem.h"

#include "tiio_3gp.h"

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
    m_status = InitializeQTML(0);
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

//-----------------------------------------------------------

inline char *filePath2unichar(const TFilePath &path) {
  int _convert    = 0;
  std::wstring ws = path.getWideString();

  LPCWSTR lpw = ws.c_str();
  char *name  = NULL;

  if (lpw) {
    _convert   = (lstrlenW(lpw) + 1) * 2;
    LPSTR pStr = new char[_convert];
    name       = AtlW2AHelper(pStr, lpw, _convert, 0);
  }

  char *outName = new char[1024];

  if (QTMLGetCanonicalPathName(name, outName, 1024) == noErr) {
    delete[] name;
    return outName;
  }
  delete[] outName;
  return name;
}

//-----------------------------------------------------------

TFilePath getLegalName(const TFilePath &name) {
  if (QuickTimeStuff::instance()->getStatus() != noErr) return name;

  TFilePath legalName;
  char outDir[1024];

  TFilePath dirName(name.getParentDir());

  char dirNameFp[1024] = "";

  OSErr err = QTMLGetCanonicalPathName(dirNameFp, outDir, 1024);

  if (err == noErr)
    legalName = TFilePath(outDir) + name.withoutParentDir();
  else
    legalName = name;

  return legalName;
}

//--------------------------`----------------------------------------------------

string long2fourchar(TINT32 fcc) {
  string s;
  s += (char((fcc & 0xff000000) >> 24));
  s += (char((fcc & 0x00ff0000) >> 16));
  s += (char((fcc & 0x0000ff00) >> 8));
  s += (char((fcc & 0x000000ff) >> 0));

  return s;
}

const std::string CodecNamesId   = "PU_CodecName";
const std::string CodecQualityId = "PU_CodecQuality";

}  // namespace

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  TImageWriterMov
//------------------------------------------------------------------------------

class TImageWriter3gp final : public TImageWriter {
public:
  TImageWriter3gp(const TFilePath &, int frameIndex, TLevelWriter3gp *);
  ~TImageWriter3gp() { m_lwm->release(); }
  bool is64bitOutputSupported() override { return false; }

private:
  // not implemented
  TImageWriter3gp(const TImageWriter3gp &);
  TImageWriter3gp &operator=(const TImageWriter3gp &src);

public:
  void save(const TImageP &) override;
  int m_frameIndex;

private:
  TLevelWriter3gp *m_lwm;
};

//-----------------------------------------------------------
//  TImageReaderv
//-----------------------------------------------------------
class TImageReader3gp final : public TImageReader {
public:
  TImageReader3gp(const TFilePath &, int frameIndex, TLevelReader3gp *);
  ~TImageReader3gp() { m_lrm->release(); }

private:
  // not implemented
  TImageReader3gp(const TImageReader3gp &);
  TImageReader3gp &operator=(const TImageReader3gp &src);

public:
  TImageP load() override;
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
void copy(TRasterP rin, PixelXRGB *bufout, int lx, int ly) {
  rin->lock();
  TRaster32P rin32 = rin;
  assert(rin32);

  PixelXRGB *rowout = &(bufout[(ly - 1) * lx]);
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
    rowout -= lx;
  }
  rin->unlock();
}
};

//-----------------------------------------------------------
void TImageWriter3gp::save(const TImageP &img) {
  m_lwm->save(img, m_frameIndex);
}

//-----------------------------------------------------------

void TLevelWriter3gp::save(const TImageP &img, int frameIndex) {
  if (m_cancelled) return;
  string msg;
  QMutexLocker sl(&m_mutex);

  TRasterImageP image(img);
  TRasterP ras = image->getRaster();
  int lx       = ras->getLx();
  int ly       = ras->getLy();

  ras->lock();
  void *buffer = image->getRaster()->getRawData();
  int pixSize  = image->getRaster()->getPixelSize();
  if (pixSize != 4) {
    msg = "Unsupported pixel type";
    goto error;
  }

  if (!m_properties) m_properties = new Tiio::MovWriterProperties();

  Tiio::MovWriterProperties *prop = (Tiio::MovWriterProperties *)(m_properties);

  QTAtomContainer atoms;
  QTNewAtomContainer(&atoms);
  fromPropertiesToAtoms(*prop, atoms);
  ComponentInstance ci =
      OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
  if (SCSetSettingsFromAtomContainer(ci, atoms)) assert(false);

  /*
CodecType compression = prop->getCurrentCodec();
CodecQ quality = prop->getCurrentQuality();
*/

  if (!m_initDone) {
    Rect frame;
    QDErr err;

    m_videoTrack = NewMovieTrack(m_movie, FixRatio((short)lx, 1),
                                 FixRatio((short)ly, 1), kNoVolume);

    if ((err = GetMoviesError() != noErr)) {
      msg = "can't create video track";
      goto error;
    }

    m_dataRef    = nil;
    m_hMovieData = NewHandle(0);

    // Construct the Handle data reference
    err = PtrToHand(&m_hMovieData, &m_dataRef, sizeof(Handle));

    if ((err = GetMoviesError() != noErr)) {
      msg = "can't create Data Ref";
      goto error;
    }

    m_videoMedia =
        NewTrackMedia(m_videoTrack, VideoMediaType, (TINT32)m_frameRate,
                      m_dataRef, HandleDataHandlerSubType);

    OpenADefaultComponent(MovieExportType, '3gpp', &m_myExporter);
    if (TSystem::doHaveMainLoop())
      err = (short)MovieExportDoUserDialog(m_myExporter, m_movie, 0, 0, 0,
                                           &m_cancelled);
    if (m_cancelled) {
      msg = "user abort of 3gp creation";
      goto error;
    }
    if ((err = GetMoviesError() != noErr)) {
      msg = "can't create video media";
      goto error;
    }

    if ((err = BeginMediaEdits(m_videoMedia)) != noErr) {
      msg = "can't begin edit video media";
      goto error;
    }

    frame.left   = 0;
    frame.top    = 0;
    frame.right  = lx;
    frame.bottom = ly;

    if ((err = NewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0)) != noErr) {
      msg = "can't create movie buffer";
      goto error;
    }

    LockPixels(m_gworld->portPixMap);
    /*if ((err = GetMaxCompressionSize(m_gworld->portPixMap, &frame, 0,
                    quality,  compression,anyCodec,
                                                                                                                     &max_compressed_size))!=noErr)
throw TImageException(getFilePath(), "can't get max compression size");
*/

    if ((err = MemError()) != noErr) {
      msg = "can't allocate compressed data for movie";
      goto error;
    }

    if ((err = MemError()) != noErr) {
      msg = "can't allocate img handle";
      goto error;
    }

    m_pixmap = GetGWorldPixMap(m_gworld);
    if (!LockPixels(m_pixmap)) {
      msg = "can't lock pixels";
      goto error;
    }

    buf    = (PixelXRGB *)(*(m_pixmap))->baseAddr;
    buf_lx = lx;
    buf_ly = ly;

    m_initDone = true;
  }

  Rect frame;
  ImageDescriptionHandle img_descr = 0;
  Handle compressedData            = 0;

  QDErr err;

  frame.left   = 0;
  frame.top    = 0;
  frame.right  = lx;
  frame.bottom = ly;

  copy(ras, buf, buf_lx, buf_ly);

  if ((err = (QDErr)SCCompressImage(ci, m_gworld->portPixMap, &frame,
                                    &img_descr, &compressedData)) != noErr) {
    msg = "can't compress image";
    goto error;
  }

  /*
if ((err = CompressImage(m_gworld->portPixMap,
                         &frame,
                                                                                           quality, compression,
                                                                                           img_descr, compressed_data_ptr))!=noErr)
throw TImageException(getFilePath(), "can't compress image");
*/
  if ((err = AddMediaSample(
           m_videoMedia, compressedData, 0, (*img_descr)->dataSize, 1,
           (SampleDescriptionHandle)img_descr, 1, 0, 0)) != noErr) {
    msg = "can't add image to movie media";
    goto error;
  }

  DisposeHandle((Handle)img_descr);
  DisposeHandle(compressedData);
  ras->unlock();
  return;

error:
  ras->unlock();
  throw TImageException(getFilePath(), msg);
}

//-----------------------------------------------------------
//        TLevelWriterMov
//-----------------------------------------------------------

TLevelWriter3gp::TLevelWriter3gp(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo)
    , m_initDone(false)
    , m_IOError(QTNoError)
    , m_pixmap(0)
    , m_gworld(0)
    , m_videoMedia(0)
    , m_videoTrack(0)
    , m_movie(0)
    , m_cancelled(false)
    , m_soundDataRef(0)
    , m_hSoundMovieData(0)

{
  m_frameRate = 12.;

  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    m_IOError = QTNotInstalled;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }

  QDErr err;

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

  if (m_cancelled) return;

  if (!st) throw TException("null reference to soundtrack");

  if (st->getBitPerSample() != 16) {
    throw TImageException(m_path, "Only 16 bits per sample is supported");
  }

  theTrack = NewMovieTrack(m_movie, 0, 0, kFullVolume);
  myErr    = GetMoviesError();

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
  FailIf(myErr != noErr, Exit);

  // start a media editing session
  myErr = BeginMediaEdits(myMedia);
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

  myErr =
      SoundConverterGetInfo(converter, siCompressionFactor, &compressionInfo);
  myErr = GetCompressionInfo(fixedCompression, sourceInfo.format,
                             sourceInfo.numChannels, sourceInfo.sampleSize,
                             &compressionInfo);
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
  FailIf(myErr != noErr, MediaErr);

  myErr = EndMediaEdits(myMedia);
  FailIf(myErr != noErr, MediaErr);

  //////////
  //
  // insert the media into the track
  //
  //////////

  myErr =
      InsertMediaIntoTrack(theTrack, 0, 0, GetMediaDuration(myMedia), fixed1);
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
  if (m_pixmap) UnlockPixels(m_pixmap);
  if (m_gworld) DisposeGWorld(m_gworld);
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
    long myFlags      = 0L;
    OSErr myErr       = noErr;
    UCHAR myCancelled = FALSE;
    char *pStr        = filePath2unichar(m_path);

    NativePathNameToFSSpec(pStr, &fspec, kFullNativePath);

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
  if (m_hSoundMovieData) DisposeHandle(m_hMovieData);
  if (m_hSoundMovieData) DisposeHandle(m_hSoundMovieData);

  if (m_refNum) CloseMovieFile(m_refNum);
  DisposeMovie(m_movie);
}

//-----------------------------------------------------------

TImageWriterP TLevelWriter3gp::getFrameWriter(TFrameId fid) {
  if (m_cancelled) return 0;

  if (m_IOError) throw TImageException(m_path, buildQTErrorString(m_IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index = fid.getNumber() - 1;

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

  char *pStr = filePath2unichar(m_path);

  if ((err = NativePathNameToFSSpec(pStr, &fspec, kFullNativePath)) != noErr) {
    delete[] pStr;
    pStr = 0;
    throw TImageException(path, "can't open file");
  }
  delete[] pStr;
  pStr = 0;
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
  ImageDescriptionPtr imagePtr = *imageH;
  m_lx                         = imagePtr->width;
  m_ly                         = imagePtr->height;
  m_depth                      = imagePtr->depth;

  DisposeHandle((Handle)imageH);

  // m_info->m_frameRate = GetMediaTimeScale(theMedia);
}

//------------------------------------------------

//------------------------------------------------

//------------------------------------------------
// TImageReaderMov
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
  {
    QMutexLocker sl(&m_mutex);
    if (m_IOError != QTNoError)
      throw TImageException(m_path, buildQTErrorString(m_IOError));

    TRaster32P ras = rasP;

    Rect rect;
    rect.right  = pos.x + ras->getLx();
    rect.left   = pos.x;
    rect.bottom = pos.y + ras->getLy();
    rect.top    = pos.y;

    GWorldPtr offscreenGWorld;
    OSErr err;
    ras->lock();
    err = QTNewGWorldFromPtr(&offscreenGWorld, k32BGRAPixelFormat, &rect, 0, 0,
                             0, ras->getRawData(), ras->getWrap() * 4);

    if (err != noErr) {
      m_IOError = QTUnableToCreateResource;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    SetMovieBox(m_movie, &rect);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieBox;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    SetMovieGWorld(m_movie, offscreenGWorld, GetGWorldDevice(offscreenGWorld));
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieGWorld;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    TimeValue currentTime = currentTimes[frameIndex];

    SetMovieTimeValue(m_movie, currentTime);

    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetTimeValue;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    err = UpdateMovie(m_movie);
    if (err != noErr) {
      m_IOError = QTUnableToUpdateMovie;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    MoviesTask(m_movie, 0);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToDoMovieTask;
      DisposeGWorld(offscreenGWorld);
      goto error;
    }

    DisposeGWorld(offscreenGWorld);
    ras->unlock();
  }

  if (m_depth != 32) {
    setMatteAndYMirror(rasP);
  } else {
    rasP->yMirror();
  }

  return;

error:
  rasP->unlock();
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

#endif
