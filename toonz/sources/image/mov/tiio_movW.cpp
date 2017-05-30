

#if !defined(x64) && !(defined(__GNUC__) && defined(_WIN32))

#include "texception.h"
#include "tsound.h"
#include "tconvert.h"
#include "tpropertytype.h"
#include "timageinfo.h"
#include "tlevel_io.h"
#include "../avi/tiio_avi.h"
#include "trasterimage.h"

#include "tiio_mov.h"
#include "movsettings.h"

//**************************************************************************************
//    Local namespace  stuff
//**************************************************************************************

namespace {

const std::string firstFrameKey = "frst";            // First frame atom id
const int firstFrameKeySize     = 4 * sizeof(char);  //

//------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------

string long2fourchar(TINT32 fcc) {
  string s;
  s += (char((fcc & 0xff000000) >> 24));
  s += (char((fcc & 0x00ff0000) >> 16));
  s += (char((fcc & 0x0000ff00) >> 8));
  s += (char((fcc & 0x000000ff) >> 0));

  return s;
}

//-------------------------------------------------------------------------------

TimeScale frameRateToTimeScale(double frameRate) {
  return tround(frameRate * 100.0);
}

//-------------------------------------------------------------------------------

// const std::string CodecNamesId   = "PU_CodecName";
// const std::string CodecQualityId = "PU_CodecQuality";

}  // namespace

//------------------------------------------------------------------------------

bool IsQuickTimeInstalled() {
  return QuickTimeStuff::instance()->getStatus() == noErr;
}

//------------------------------------------------------------------------------
//  TImageWriterMov
//------------------------------------------------------------------------------

class TImageWriterMov final : public TImageWriter {
public:
  TImageWriterMov(const TFilePath &, int frameIndex, TLevelWriterMov *);
  ~TImageWriterMov() { m_lwm->release(); }
  bool is64bitOutputSupported() override { return false; }

private:
  // not implemented
  TImageWriterMov(const TImageWriterMov &);
  TImageWriterMov &operator=(const TImageWriterMov &src);

public:
  void save(const TImageP &) override;
  int m_frameIndex;

private:
  TLevelWriterMov *m_lwm;
};

//-----------------------------------------------------------
//-----------------------------------------------------------
//        TImageWriterMov
//-----------------------------------------------------------

TImageWriterMov::TImageWriterMov(const TFilePath &path, int frameIndex,
                                 TLevelWriterMov *lwm)
    : TImageWriter(path), m_lwm(lwm), m_frameIndex(frameIndex) {
  m_lwm->addRef();
}

//----------
namespace {
void copy(TRasterP rin, PixelXRGB *bufout, int lx, int ly) {
  TRaster32P rin32 = rin;
  assert(rin32);

  PixelXRGB *rowout = &(bufout[(ly - 1) * lx]);
  rin32->lock();
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
  rin32->unlock();
}
};

//-----------------------------------------------------------
/*
TWriterInfo *TWriterInfoMov::create(const std::string &)
{
return new TWriterInfoMov();
}

//-----------------------------------------------------------

TWriterInfoMov::TWriterInfoMov(const TWriterInfoMov&src)
               :TWriterInfo(src)
               ,m_codecTable(src.m_codecTable)
               ,m_qualityTable(src.m_qualityTable)
{
}

//-----------------------------------------------------------

TWriterInfo *TWriterInfoMov::clone() const
{
return new TWriterInfoMov(*this);
}
*/
//-----------------------------------------------------------
/*
class MovWriterProperties final : public TPropertyGroup {
public:
  TEnumProperty m_codec;
  TEnumProperty m_quality;


Tiio::TifWriterProperties::TifWriterProperties()
                    : m_byteOrdering("Byte Order")
                    , m_compressionType("Compression Type")
                    , m_matte("Alpha Channel", true)
{
    m_byteOrdering.addValue(L"IBM PC");
    m_byteOrdering.addValue(L"Mac");
#ifdef _WIN32
  m_byteOrdering.setValue(L"IBM PC");
#else
  m_byteOrdering.setValue(L"Mac");
#endif

  m_compressionType.addValue(L"NONE");
  m_compressionType.addValue(L"LZW");

  bind(m_byteOrdering);
  bind(m_matte);
  bind(m_compressionType);
  m_compressionType.setValue(L"LZW");
}
*/

//-----------------------------------------------------------
/*
CodecType Tiio::MovWriterProperties::getCurrentCodec()const
{
std::map<wstring, CodecType>::const_iterator it;

it = m_codecMap.find(m_codec.getValue());
assert(it!=m_codecMap.end());
return it->second;
}

//-----------------------------------------------------------

wstring Tiio::MovWriterProperties::getCurrentNameFromCodec(CodecType
&cCodec)const
{
std::map<wstring, CodecType>::const_iterator it;

for(it=m_codecMap.begin();it!=m_codecMap.end();++it)
{
  CodecType tmp=it->second;
  if(it->second==cCodec) break;
}
assert(it!=m_codecMap.end());
return it->first;
}

//-----------------------------------------------------------

wstring Tiio::MovWriterProperties::getCurrentQualityFromCodeQ(CodecQ
&cCodecQ)const
{
std::map<wstring, CodecQ>::const_iterator it;

for(it=m_qualityMap.begin();it!=m_qualityMap.end();++it)
{
  CodecQ tmp=it->second;
  if(it->second==cCodecQ) break;
}
assert(it!=m_qualityMap.end());
return it->first;
}

//-----------------------------------------------------------

CodecQ Tiio::MovWriterProperties::getCurrentQuality() const
{
std::map<wstring, CodecQ>::const_iterator it;

it = m_qualityMap.find(m_quality.getValue());
assert(it!=m_qualityMap.end());
return it->second;
}
*/
//-----------------------------------------------------------

/*
TPropertyGroup* Tiio::MovWriterProperties::clone() const
{
  MovWriterProperties*g = new MovWriterProperties();

        g->m_codec.setValue(m_codec.getValue());
        g->m_quality.setValue(m_quality.getValue());

return (TPropertyGroup*)g;
}
*/

Tiio::MovWriterProperties::MovWriterProperties()
//: m_codec("Codec")
//, m_quality("Quality")
{
  if (InitializeQTML(0) != noErr) return;

  ComponentInstance ci =
      OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
  QTAtomContainer settings;

  if (SCGetSettingsAsAtomContainer(ci, &settings) != noErr) assert(false);

  fromAtomsToProperties(settings, *this);
}

//-----------------------------------------------------------

void TImageWriterMov::save(const TImageP &img) {
  m_lwm->save(img, m_frameIndex);
}

//-----------------------------------------------------------

void TLevelWriterMov::save(const TImageP &img, int frameIndex) {
  m_firstFrame = std::min(frameIndex, m_firstFrame);

  TRasterImageP image(img);
  if (!image) throw TImageException(getFilePath(), "Unsupported image type");

  int lx = image->getRaster()->getLx();
  int ly = image->getRaster()->getLy();

  int pixSize = image->getRaster()->getPixelSize();
  if (pixSize != 4)
    throw TImageException(getFilePath(), "Unsupported pixel type");

  QMutexLocker sl(&m_mutex);

  if (!m_videoTrack) {
    if (!m_properties) m_properties = new Tiio::MovWriterProperties();

    Tiio::MovWriterProperties *prop =
        (Tiio::MovWriterProperties *)(m_properties);

    QTAtomContainer atoms;
    QTNewAtomContainer(&atoms);
    fromPropertiesToAtoms(*prop, atoms);
    m_ci = OpenDefaultComponent(StandardCompressionType,
                                StandardCompressionSubType);
    if (SCSetSettingsFromAtomContainer(m_ci, atoms) != noErr) {
      CloseComponent(m_ci);
      assert(!"cannot use that quickTime codec, use default");
      m_ci = OpenDefaultComponent(StandardCompressionType,
                                  StandardCompressionSubType);
    }

    QTDisposeAtomContainer(atoms);

    Rect frame;
    QDErr err;

    if (QuickTimeStuff::instance()->getStatus() != noErr) {
      m_IOError = QTNotInstalled;
      throw TImageException(m_path, buildQTErrorString(m_IOError));
    }

    // First, set the movie's time scale to match a suitable multiple of
    // m_frameRate.
    // The timeScale will be used for both the movie and media.
    TimeScale timeScale = ::frameRateToTimeScale(m_frameRate);
    SetMovieTimeScale(m_movie, timeScale);

    m_videoTrack = NewMovieTrack(m_movie, FixRatio((short)lx, 1),
                                 FixRatio((short)ly, 1), kNoVolume);

    if ((err = GetMoviesError() != noErr))
      throw TImageException(getFilePath(), "can't create video track");

    m_videoMedia = NewTrackMedia(m_videoTrack, VideoMediaType, timeScale, 0, 0);

    if ((err = GetMoviesError() != noErr))
      throw TImageException(getFilePath(), "can't create video media");

    frame.left   = 0;
    frame.top    = 0;
    frame.right  = lx;
    frame.bottom = ly;

    if ((err = NewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0)) != noErr)
      throw TImageException(getFilePath(), "can't create movie buffer");

    LockPixels(m_gworld->portPixMap);
    /*
if ((err = ImageCodecGetMaxCompressionSize(Ci, m_gworld->portPixMap, &frame, 0,
                                   quality, codecType, anyCodec,
                                   &max_compressed_size))!=noErr)
                                   throw TImageException(getFilePath(), "can't
get max compression size");
*/

    if ((err = MemError()) != noErr)
      throw TImageException(getFilePath(), "can't allocate img handle");

    m_pixmap = GetGWorldPixMap(m_gworld);
    if (!LockPixels(m_pixmap))
      throw TImageException(getFilePath(), "can't lock pixels");

    m_buf  = (PixelXRGB *)(*(m_pixmap))->baseAddr;
    buf_lx = lx;
    buf_ly = ly;
  }

  OSErr err = noErr;
  if ((err = BeginMediaEdits(m_videoMedia)) != noErr)
    throw TImageException(getFilePath(), "can't begin edit video media");

  Rect frame;
  ImageDescriptionHandle img_descr = 0;
  Handle compressedData            = 0;

  frame.left   = 0;
  frame.top    = 0;
  frame.right  = lx;
  frame.bottom = ly;

  TRasterP ras = image->getRaster();

  copy(ras, m_buf, buf_lx, buf_ly);

  if ((err = SCCompressImage(m_ci, m_gworld->portPixMap, &frame, &img_descr,
                             &compressedData)) != noErr)
    throw TImageException(getFilePath(), "can't compress image");

  /*
if ((err = CompressImage(m_gworld->portPixMap,
&frame,
quality, codecType,
img_descr, compressed_data_ptr))!=noErr)
throw TImageException(getFilePath(), "can't compress image");
*/

  TimeValue sampleTime;
  if ((err = AddMediaSample(m_videoMedia, compressedData, 0,
                            (*img_descr)->dataSize,
                            100,  // 100 matches the length of 1 frame
                            (SampleDescriptionHandle)
                                img_descr,  // in the movie/media's timeScale
                            1,
                            0, &sampleTime)) != noErr)
    throw TImageException(getFilePath(), "can't add image to movie media");

  if ((err = EndMediaEdits(m_videoMedia)) != noErr)
    throw TImageException(getFilePath(), "can't end edit media");

  DisposeHandle(compressedData);
  DisposeHandle((Handle)img_descr);

  m_savedFrames.push_back(std::pair<int, TimeValue>(frameIndex, sampleTime));
}

//-----------------------------------------------------------
//        TLevelWriterMov
//-----------------------------------------------------------

TLevelWriterMov::TLevelWriterMov(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo)
    , m_IOError(QTNoError)
    , m_pixmap(0)
    , m_gworld(0)
    , m_videoMedia(0)
    , m_soundMedia(0)
    , m_videoTrack(0)
    , m_soundTrack(0)
    , m_movie(0)
    , m_firstFrame((std::numeric_limits<int>::max)()) {
  m_frameRate = 12.;

  char *pStr = filePath2unichar(m_path);
  QDErr err;
  FSSpec fspec;

  if ((err = NativePathNameToFSSpec(pStr, &fspec, kFullNativePath)) != noErr) {
    delete[] pStr;
    pStr      = 0;
    m_IOError = QTUnableToOpenFile;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }
  delete[] pStr;
  pStr = 0;

  if (err = CreateMovieFile(
                &fspec, 'TVOD', smCurrentScript,
                createMovieFileDeleteCurFile | createMovieFileDontCreateResFile,
                &(m_refNum), &(m_movie)) != noErr) {
    m_IOError = QTCantCreateFile;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }
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

void TLevelWriterMov::saveSoundTrack(TSoundTrack *st) {
  OSErr myErr = noErr;

  if (!st) throw TException("null reference to soundtrack");

  if (st->getBitPerSample() != 16)
    throw TImageException(m_path, "Only 16 bits per sample is supported");

  if (m_soundTrack == 0) {
    m_soundTrack = NewMovieTrack(m_movie, 0, 0, kFullVolume);
    myErr        = GetMoviesError();

    FailIf(myErr != noErr, CompressErr);

    m_soundMedia =
        NewTrackMedia(m_soundTrack, SoundMediaType, st->getSampleRate(), NULL,
                      0);  // track->rate >> 16
    myErr = GetMoviesError();

    FailIf(myErr != noErr, Exit);
  }

  SoundDescriptionV1Handle mySampleDesc;
  Handle myDestHandle;
  SoundComponentData sourceInfo;
  SoundComponentData destInfo;
  SoundConverter converter;
  CompressionInfo compressionInfo;

  myDestHandle = NewHandle(0);
  FailWithAction(myDestHandle == NULL, myErr = MemError(), NoDest);

  *myDestHandle = (char *)st->getRawData();

  // start a media editing session
  myErr = BeginMediaEdits(m_soundMedia);
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

  destInfo.format      = k16BitNativeEndianFormat;
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
      m_soundMedia, myDestHandle, 0,
      destInfo.sampleCount * compressionInfo.bytesPerFrame, 1,
      (SampleDescriptionHandle)mySampleDesc,
      destInfo.sampleCount * compressionInfo.samplesPerPacket, 0, NULL);
  FailIf(myErr != noErr, MediaErr);

  myErr = EndMediaEdits(m_soundMedia);
  FailIf(myErr != noErr, MediaErr);

// NOTE: Sound media is inserted into the movie track only at destruction

ConverterErr:  // Multiple bailout labels just to
NoDest:        // separate debugging strings, it seems.
CompressErr:   //
Exit:          // Should be done better...
MediaErr:      //

  if (mySampleDesc != NULL) DisposeHandle((Handle)mySampleDesc);

  if (converter) SoundConverterClose(converter);

  if (myErr != noErr) throw TImageException(m_path, "error saving audio track");
}

//-----------------------------------------------------------

TLevelWriterMov::~TLevelWriterMov() {
  if (m_pixmap) UnlockPixels(m_pixmap);
  if (m_gworld && m_gworld->portPixMap) UnlockPixels(m_gworld->portPixMap);

  if (m_gworld) DisposeGWorld(m_gworld);
  if (m_ci) CloseComponent(m_ci);

  QDErr err;
  OSStatus mderr;

  if (m_videoTrack) {
    // Insert the saved Frames into the track.

    // NOTE: Holes in the frames sequence will be intended as still frames,
    // rather than left blank.
    //       I'm quite unsure whether this behavior is actually useful - or used
    //       at all.
    //       In particular, the last frame is an exception, and does NOT drag to
    //       the end of the
    //       movie (see below). I think it's not used - frames are consecutive
    //       AFAIK.

    TimeValue movieTimeScale = GetMovieTimeScale(m_movie);
    int savedFramesSize      = (int)m_savedFrames.size() - 1;

    int i;
    for (i = 0; i < savedFramesSize; ++i) {
      TimeValue mediaPosition = m_savedFrames[i].second;
      TimeValue trackPosition =
          ((m_savedFrames[i].first - m_firstFrame) * movieTimeScale) /
          m_frameRate;

      TimeValue duration = m_savedFrames[i + 1].first - m_savedFrames[i].first;
      Fixed ratio        = tround(fixed1 / (double)duration);

      if ((err = InsertMediaIntoTrack(m_videoTrack, trackPosition,
                                      mediaPosition, 100, ratio)) != noErr)
        throw TImageException(getFilePath(), "can't insert media into track");
    }

    if (savedFramesSize >= 0) {
      TimeValue mediaPosition = m_savedFrames[i].second;
      TimeValue trackPosition =
          ((m_savedFrames[i].first - m_firstFrame) * movieTimeScale) /
          m_frameRate;

      // The last frame has duration 1 (frame):
      //   duration = 1  =>  ratio = fixed1

      if ((err = InsertMediaIntoTrack(m_videoTrack, trackPosition,
                                      mediaPosition, 100, fixed1)) != noErr)
        throw TImageException(getFilePath(), "can't insert media into track");
    }

    QTMetaDataRef metaDataRef;
    if ((mderr = QTCopyMovieMetaData(m_movie, &metaDataRef)) != noErr)
      throw TImageException(getFilePath(),
                            "can't access metadata informations");

    if ((mderr = QTMetaDataAddItem(
             metaDataRef, kQTMetaDataStorageFormatUserData,
             kQTMetaDataKeyFormatUserData, (const UInt8 *)firstFrameKey.c_str(),
             firstFrameKeySize, (const UInt8 *)(&m_firstFrame), sizeof(int),
             kQTMetaDataTypeUnsignedIntegerBE, 0)) != noErr)
      throw TImageException(getFilePath(),
                            "can't insert metadata informations");

    QTMetaDataRelease(metaDataRef);
  }

  if (m_soundTrack) {
    if (0 != (err = InsertMediaIntoTrack(
                  m_soundTrack, 0, 0, GetMediaDuration(m_soundMedia), fixed1)))
      throw TImageException(getFilePath(), "can't insert sound into track");
  }

  short resId = movieInDataForkResID;
  if (m_movie) {
    if ((err = AddMovieResource(m_movie, m_refNum, &resId, 0)) !=
        noErr)  // Why no thrown exception here?
    {
    }  // throw TImageException(getFilePath(), "Can't add resource"); //
  }

  if (m_videoMedia)                   // And anyway, how exceptions
    DisposeTrackMedia(m_videoMedia);  // deal with absence of corresponding
  if (m_videoTrack)                   // disposals?
    DisposeMovieTrack(m_videoTrack);  // I guess se should use RAII here... -.-'
  if (m_soundMedia) DisposeTrackMedia(m_soundMedia);

  if (m_refNum) CloseMovieFile(m_refNum);

  DisposeMovie(m_movie);
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterMov::getFrameWriter(TFrameId fid) {
  if (m_IOError) throw TImageException(m_path, buildQTErrorString(m_IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index = fid.getNumber() - 1;

  TImageWriterMov *iwm = new TImageWriterMov(m_path, index, this);
  return TImageWriterP(iwm);
}

//-----------------------------------------------------------

TLevelReaderMov::TLevelReaderMov(const TFilePath &path)
    : TLevelReader(path)
    , m_IOError(QTNoError)
    , m_track(0)
    , m_movie(0)
    , m_depth(0)
    , m_readAsToonzOutput(false)
    , m_yMirror(true)
    , m_loadTimecode(false) {
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

  short resId = 0;
  Str255 name;
  err = NewMovieFromFile(&m_movie, m_refNum, &resId, name, fsRdPerm,
                         &dataRefWasChanged);

  int numTracks = GetMovieTrackCount(m_movie);
  if (numTracks < 1) {
    m_IOError = QTUnableToOpenFile;
    return;
  }

  assert(numTracks == 1 || numTracks == 2);

  m_track =
      GetMovieIndTrackType(m_movie, 1, VideoMediaType, movieTrackMediaType);

  ImageDescriptionHandle imageH;
  imageH = (ImageDescriptionHandle)NewHandleClear(sizeof(ImageDescription));

  TINT32 index   = 1;
  Media theMedia = GetTrackMedia(m_track);

  GetMediaSampleDescription(theMedia, index, (SampleDescriptionHandle)imageH);
  ImageDescriptionPtr imagePtr = *imageH;

  if ((imagePtr->cType >> 16) == 0x4458 /*'DX'*/ &&
      path.getType() == "avi")  // DIVX - compressed, QT is unable to read it.
  {                             // Done externally with the... AVI reader o_o.
    m_IOError = QTUnableToDoMovieTask;
    return;
  }

  // Retrieve the timecode media handler
  {
    Track tcTrack = GetMovieIndTrackType(m_movie, 1, TimeCodeMediaType,
                                         movieTrackMediaType);
    Media tcMedia     = GetTrackMedia(tcTrack);
    m_timecodeHandler = GetMediaHandler(tcMedia);
  }

  m_lx    = imagePtr->width;
  m_ly    = imagePtr->height;
  m_depth = imagePtr->depth;

  m_info = new TImageInfo();

  m_info->m_lx        = m_lx;
  m_info->m_ly        = m_ly;
  m_info->m_frameRate = GetMediaTimeScale(theMedia) /
                        100.0;  // REMOVE THIS! Not all movs have this
                                // kind of format - only those from Toonz...
  Tiio::MovWriterProperties *prop = new Tiio::MovWriterProperties();
  m_info->m_properties            = prop;

  DisposeHandle((Handle)imageH);
}

//------------------------------------------------

TLevelReaderMov::~TLevelReaderMov() {
  StopMovie(m_movie);

  if (m_refNum) CloseMovieFile(m_refNum);

  if (m_movie) DisposeMovie(m_movie);
}

//------------------------------------------------

void TLevelReaderMov::loadedTimecode(UCHAR &hh, UCHAR &mm, UCHAR &ss,
                                     UCHAR &ff) {
  hh = m_hh, mm = m_mm, ss = m_ss, ff = m_ff;
}

//------------------------------------------------

void TLevelReaderMov::setYMirror(bool enabled) { m_yMirror = enabled; }

//------------------------------------------------

void TLevelReaderMov::setLoadTimecode(bool enabled) {
  m_loadTimecode = enabled;
}

//------------------------------------------------

void TLevelReaderMov::enableRandomAccessRead(bool enable) {
  m_readAsToonzOutput = enable;
}

//-----------------------------------------------------------
//  TImageReaderMov
//-----------------------------------------------------------

class TImageReaderMov final : public TImageReader {
  TLevelReaderMov *m_lrm;
  TImageInfo *m_info;

public:
  int m_frameIndex;

public:
  TImageReaderMov(const TFilePath &, int frameIndex, TLevelReaderMov *,
                  TImageInfo *);
  ~TImageReaderMov() { m_lrm->release(); }

  TImageP load() override;
  void load(const TRasterP &rasP, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);

  TDimension getSize() const { return m_lrm->getSize(); }
  TRect getBBox() const { return m_lrm->getBBox(); }

  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  // Not copyable
  TImageReaderMov(const TImageReaderMov &);
  TImageReaderMov &operator=(const TImageReaderMov &);
};

//------------------------------------------------

TImageReaderMov::TImageReaderMov(const TFilePath &path, int frameIndex,
                                 TLevelReaderMov *lrm, TImageInfo *info)
    : TImageReader(path), m_lrm(lrm), m_frameIndex(frameIndex), m_info(info) {
  m_lrm->addRef();
}

//------------------------------------------------

TLevelP TLevelReaderMov::loadInfo() {
  if (m_readAsToonzOutput)  // Mov files written with Toonz support special
    return loadToonzOutputFormatInfo();  // metadata atoms - separate procedure

  // Level is NOT a recognized mov from Toonz. Just read each movie's
  // interesting image as a consecutive
  // level frame.

  TLevelP level;
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  OSType mediaType = VisualMediaCharacteristic;
  TimeValue nextTime, currentTime;
  currentTime = 0;
  nextTime    = 0;

  int f = 0;  // 0 is the first frame index

  // io vorrei fare '|', ma sul manuale c'e' scritto '+'
  GetMovieNextInterestingTime(m_movie, nextTimeMediaSample + nextTimeEdgeOK, 1,
                              &mediaType, currentTime, 0, &nextTime, 0);

  if (nextTime != -1) {
    TFrameId frame(f + 1);
    level->setFrame(frame, TImageP());
    currentTimes[f] = nextTime;
    ++f;
  }

  currentTime = nextTime;
  while (nextTime != -1) {
    GetMovieNextInterestingTime(m_movie, nextTimeMediaSample, 1, &mediaType,
                                currentTime, 0, &nextTime, 0);
    if (nextTime != -1) {
      TFrameId frame(f + 1);
      level->setFrame(frame, TImageP());
      currentTimes[f] = nextTime;
      ++f;
    }

    currentTime = nextTime;
  }
  return level;
}

//------------------------------------------------

TLevelP TLevelReaderMov::loadToonzOutputFormatInfo() {
  TLevelP level;
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  OSStatus mderr;

  // Retrieve the first frame of movie. This info has been put in a metadata
  // atom.
  QTMetaDataRef metaDataRef;
  if ((mderr = QTCopyMovieMetaData(m_movie, &metaDataRef)) != noErr)
    throw TImageException(m_path, "can't access metadata informations");

  QTMetaDataItem firstFrameItem;

  // Find the metadata atom
  mderr = QTMetaDataGetNextItem(
      metaDataRef, kQTMetaDataStorageFormatUserData,
      kQTMetaDataItemUninitialized, kQTMetaDataKeyFormatUserData,
      (const UInt8 *)firstFrameKey.c_str(), firstFrameKeySize, &firstFrameItem);

  // Try to read the value. If the atom was not found, just assume firstFrame =
  // 0.
  int firstFrame = 0;
  if (mderr == noErr) {
    // The atom was found. Then, retrieve the value
    if ((mderr = QTMetaDataGetItemValue(metaDataRef, firstFrameItem,
                                        (UInt8 *)(&firstFrame), sizeof(int),
                                        0) != noErr))
      throw TImageException(m_path, "can't read metadata informations");
  }

  mderr = 0;

  QTMetaDataRelease(metaDataRef);

  OSType mediaType   = VisualMediaCharacteristic;
  TimeValue nextTime = 0, currentTime = 0;

  TimeValue movieDuration = GetMovieDuration(m_movie);

  std::vector<TimeValue> interestingTimeValues;

  // io vorrei fare '|', ma sul manuale c'e' scritto '+'
  GetMovieNextInterestingTime(m_movie, nextTimeMediaSample + nextTimeEdgeOK, 1,
                              &mediaType, currentTime, 0, &nextTime, 0);
  if (nextTime != -1) {
    interestingTimeValues.push_back(nextTime);
    currentTime = nextTime;
  }
  while (nextTime != -1) {
    GetMovieNextInterestingTime(m_movie, nextTimeMediaSample, 1, &mediaType,
                                currentTime, 0, &nextTime, 0);
    if (nextTime != -1) {
      interestingTimeValues.push_back(nextTime);
      currentTime = nextTime;
    }
  }

  if (currentTime != -1) {
    double frameRate         = m_info->m_frameRate;
    TimeScale movieTimeScale = GetMovieTimeScale(m_movie);
    int firstFrameTimeValue  = movieTimeScale * firstFrame;

    std::vector<TimeValue>::iterator it;
    for (it = interestingTimeValues.begin(); it != interestingTimeValues.end();
         ++it) {
      int frame =
          firstFrame +
          tround((*it * frameRate) /
                 (double)movieTimeScale);  // (Daniele) There was a ceil here,
      TFrameId frameId(frame + 1);  // before. But I honestly don't remember
      level->setFrame(frameId,
                      TImageP());  // WHY I placed it there. Round seems more
      currentTimes[frame] = *it;   // appropriate...
    }
  }

  return level;
}

//------------------------------------------------

TImageP TImageReaderMov::load() {
  TRasterPT<TPixelRGBM32> ret(m_lrm->getSize());
  m_lrm->load(ret, m_frameIndex, TPointI(), 1, 1);

  return TRasterImageP(ret);
}

//------------------------------------------------

void TImageReaderMov::load(const TRasterP &rasP, const TPoint &pos, int shrinkX,
                           int shrinkY) {
  if ((shrinkX != 1) || (shrinkY != 1) || (pos != TPoint(0, 0)))
    TImageReader::load(rasP, pos, shrinkX, shrinkY);
  else
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
  TPixel32 *upPix = 0;

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

void TLevelReaderMov::load(const TRasterP &rasP, int frameIndex,
                           const TPoint &pos, int shrinkX, int shrinkY) {
  GWorldPtr offscreenGWorld = 0;
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

    OSErr err;
    ras->lock();
    err = QTNewGWorldFromPtr(&offscreenGWorld, k32BGRAPixelFormat, &rect, 0, 0,
                             0, ras->getRawData(), ras->getWrap() * 4);

    if (err != noErr) {
      m_IOError = QTUnableToCreateResource;
      goto error;
    }

    SetMovieBox(m_movie, &rect);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieBox;
      goto error;
    }

    SetMovieGWorld(m_movie, offscreenGWorld, GetGWorldDevice(offscreenGWorld));
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetMovieGWorld;
      goto error;
    }

    if (currentTimes.empty()) loadInfo();

    std::map<int, TimeValue>::iterator it = currentTimes.find(frameIndex);
    if (it == currentTimes.end()) goto error;

    TimeValue currentTime = it->second;
    SetMovieTimeValue(m_movie, currentTime);

    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToSetTimeValue;
      goto error;
    }

    err = UpdateMovie(m_movie);
    if (err != noErr) {
      m_IOError = QTUnableToUpdateMovie;
      goto error;
    }

    MoviesTask(m_movie, 0);
    err = GetMoviesError();
    if (err != noErr) {
      m_IOError = QTUnableToDoMovieTask;
      goto error;
    }

    DisposeGWorld(offscreenGWorld);
    ras->unlock();
  }

  if (m_yMirror) {
    if (m_depth != 32)
      setMatteAndYMirror(rasP);
    else
      rasP->yMirror();
  }

  if (m_loadTimecode) {
    // Also build current Timecode
    TimeCodeRecord tc;
    HandlerError err = TCGetCurrentTimeCode(m_timecodeHandler, 0, 0, &tc, 0);

    m_hh = tc.t.hours;
    m_mm = tc.t.minutes;
    m_ss = tc.t.seconds;
    m_ff = tc.t.frames;
  }

  return;
error:
  rasP->unlock();
  if (offscreenGWorld) DisposeGWorld(offscreenGWorld);
  throw TImageException(m_path, buildQTErrorString(m_IOError));
}

//------------------------------------------------

void TLevelReaderMov::timecode(int frameIndex, UCHAR &hh, UCHAR &mm, UCHAR &ss,
                               UCHAR &ff) {
  hh = mm = ss = ff = 0xff;
  if (!m_timecodeHandler) return;

  QMutexLocker sl(&m_mutex);
  {
    if (m_IOError != QTNoError) goto error;

    if (currentTimes.empty()) loadInfo();

    std::map<int, TimeValue>::iterator it = currentTimes.find(frameIndex);
    if (it == currentTimes.end()) goto error;

    TimeValue currentTime = it->second;

    TimeCodeRecord tc;
    HandlerError err =
        TCGetTimeCodeAtTime(m_timecodeHandler, currentTime, 0, 0, &tc, 0);

    hh = tc.t.hours;
    mm = tc.t.minutes;
    ss = tc.t.seconds;
    ff = tc.t.frames;
  }

  return;

error:

  throw TImageException(m_path, buildQTErrorString(m_IOError));
}

//------------------------------------------------

TImageReaderP TLevelReaderMov::getFrameReader(TFrameId fid) {
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReaderMov *irm = new TImageReaderMov(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------

TLevelReader *TLevelReaderMov::create(const TFilePath &f) {
  TLevelReaderMov *reader = new TLevelReaderMov(f);
  if (reader->m_IOError != QTNoError && f.getType() == "avi") {
    delete reader;
    return new TLevelReaderAvi(f);
  } else
    return reader;
}

// You can use the SCSetInfo function with the scSettingsStateType selector to
// retrieve a handle
//  containing the current compression settings; this might be useful if you
//  were allowing the user
//  to compress a series of images and wanted to preserve the user's settings
//  from one image to the
//  next (instead of reverting to the defaults for every image). Note, however,
//  that the data in
//  that handle is byte-ordered according to the platform the code is running
//  on. As a result, you
//  should not store that data in a file and expect that file to be valid on
//  other platforms. To
//  get a handle of data in a platform-independent format, use the function
//  SCGetSettingsAsAtomContainer
//  (introduced in QuickTime 3); to restore the settings in that handle, use the
//  related function
//  SCSetSettingsAsAtomContainer.
//

#endif
