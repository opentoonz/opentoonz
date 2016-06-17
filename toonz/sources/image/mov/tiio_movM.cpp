

#ifndef __LP64__

#include <iostream>

#include "texception.h"
#include "tsound.h"
#include "tconvert.h"
#include "tpropertytype.h"
#include "timageinfo.h"
#include "trasterimage.h"
#include "tmachine.h"
#include "tsystem.h"

#include "movsettings.h"

#include "tiio_movM.h"

/* QuickDraw は 10.7 以降なくなった */
//#define HAS_QUICKDRAW

namespace {

const string firstFrameKey  = "frst";
const int firstFrameKeySize = 4 * sizeof(char);

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
    // std::cout << "destroing variable... Thread = " << GetCurrentThreadId() <<
    // std::endl;

    //              std::cout <<"~QTCleanup"<< std::endl;
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

}  // namespace

////-----------------------------------------------------------
//
// CodecType Tiio::MovWriterProperties::getCurrentCodec()const
//{
// std::map<wstring, CodecType>::const_iterator it;
//
// it = m_codecMap.find(m_codec.getValue());
// assert(it!=m_codecMap.end());
// return it->second;
//}
//
////-----------------------------------------------------------
//
// wstring Tiio::MovWriterProperties::getCurrentNameFromCodec(CodecType
// &cCodec)const
//{
// std::map<wstring, CodecType>::const_iterator it;
//
// for(it=m_codecMap.begin();it!=m_codecMap.end();++it)
//{
////  CodecType tmp=it->second;
//  if(it->second==cCodec) break;
//}
// assert(it!=m_codecMap.end());
// return it->first;
//}
//
////-----------------------------------------------------------
//
// wstring Tiio::MovWriterProperties::getCurrentQualityFromCodeQ(CodecQ
// &cCodecQ)const
//{
// std::map<wstring, CodecQ>::const_iterator it;
//
// for(it=m_qualityMap.begin();it!=m_qualityMap.end();++it)
//{
////  CodecQ tmp=it->second;
//  if(it->second==cCodecQ) break;
//}
// assert(it!=m_qualityMap.end());
// return it->first;
//}
//
////-----------------------------------------------------------
//
// CodecQ Tiio::MovWriterProperties::getCurrentQuality() const
//{
// std::map<wstring, CodecQ>::const_iterator it;
//
// it = m_qualityMap.find(m_quality.getValue());
// assert(it!=m_qualityMap.end());
// return it->second;
//}
//
////-----------------------------------------------------------
//
//
// TPropertyGroup* Tiio::MovWriterProperties::clone() const
//{
//  MovWriterProperties*g = new MovWriterProperties();
//
//	g->m_codec.setValue(m_codec.getValue());
//	g->m_quality.setValue(m_quality.getValue());
//
// return (TPropertyGroup*)g;
//}

Tiio::MovWriterProperties::MovWriterProperties()
//: m_codec("Codec")
//, m_quality("Quality")
{
  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    return;
  }

  // if (InitializeQTML(0)!=noErr)
  //    return;

  ComponentInstance ci =
      OpenDefaultComponent(StandardCompressionType, StandardCompressionSubType);
  QTAtomContainer settings;

  if (SCGetSettingsAsAtomContainer(ci, &settings) != noErr) assert(false);

  fromAtomsToProperties(settings, *this);

  /*
CodecNameSpecListPtr codecList;
if (GetCodecNameList(&codecList, 0) !=noErr) return;

TIntEnumeration codecNames;
for (short i = 0; i < codecList->count; ++i)
{
CodecNameSpec *codecNameSpec = &(codecList->list[i]);
// codecNameSpec->typeName contains the name of the codec in Pascal String
Format
// byte 0 = Len
// byte [1..Len] data
UCHAR len = codecNameSpec->typeName[0];
string name((char*)&codecNameSpec->typeName[1], len);
  m_codec.addValue(toWideString(name));
  if (i==0) m_codec.setValue(toWideString(name));
  m_codecMap[toWideString(name)] = codecNameSpec->cType;

}

DisposeCodecNameList(codecList);

TIntEnumeration  codecQuality;

m_quality.addValue(L"min quality");
m_qualityMap[L"min quality"] = codecMinQuality;
m_quality.addValue(L"low quality");
m_qualityMap[L"low quality"] = codecLowQuality;
m_quality.addValue(L"normal quality");
m_qualityMap[L"normal quality"] = codecNormalQuality;
m_quality.addValue(L"high quality");
m_qualityMap[L"high quality"] = codecHighQuality;
m_quality.addValue(L"max quality");
m_qualityMap[L"max quality"] = codecMaxQuality;
m_quality.addValue(L"lossless quality");
m_qualityMap[L"lossless quality"] = codecLosslessQuality;

m_quality.setValue(L"normal quality");

bind(m_codec);
bind(m_quality);
*/
  /*
m_qualityTable.insert(QualityTable::value_type(q++, codecMinQuality));
m_qualityTable.insert(QualityTable::value_type(q++, codecLowQuality));
m_qualityTable.insert(QualityTable::value_type(q++, codecMaxQuality));
m_qualityTable.insert(QualityTable::value_type(q++, codecNormalQuality));
m_qualityTable.insert(QualityTable::value_type(q++, codecHighQuality));
m_qualityTable.insert(QualityTable::value_type(q++, codecLosslessQuality));

addProperty(CodecQualityId, codecQuality,0);*/
}

namespace {

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

//-------------------------------------------------------------------------------

TimeScale frameRateToTimeScale(double frameRate) {
  return tround(frameRate * 100.0);
}

//-------------------------------------------------------------------------------

const string CodecNamesId   = "PU_CodecName";
const string CodecQualityId = "PU_CodecQuality";

}  // namespace

//------------------------------------------------------------------------------

bool IsQuickTimeInstalled() {
  return QuickTimeStuff::instance()->getStatus() == noErr;
}

//------------------------------------------------------------------------------
//  TImageWriterMov
//------------------------------------------------------------------------------

class TImageWriterMov : public TImageWriter {
public:
  TImageWriterMov(const TFilePath &, int frameIndex, TLevelWriterMov *);
  ~TImageWriterMov() { m_lwm->release(); }
  bool is64bitOutputSupported() { return false; }

private:
  // not implemented
  TImageWriterMov(const TImageWriterMov &);
  TImageWriterMov &operator=(const TImageWriterMov &src);

public:
  void save(const TImageP &);
  int m_frameIndex;

private:
  TLevelWriterMov *m_lwm;
};

//-----------------------------------------------------------
//  TImageReaderMov
//-----------------------------------------------------------
class TImageReaderMov : public TImageReader {
public:
  TImageReaderMov(const TFilePath &, int frameIndex, TLevelReaderMov *);
  ~TImageReaderMov() { m_lrm->release(); }

private:
  // not implemented
  TImageReaderMov(const TImageReaderMov &);
  TImageReaderMov &operator=(const TImageReaderMov &src);

public:
  TImageP load();
  void load(const TRasterP &rasP, const TPoint &pos = TPoint(0, 0),
            int shrinkX = 1, int shrinkY = 1);
  int m_frameIndex;

  TDimension getSize() const { return m_lrm->getSize(); }
  TRect getBBox() const { return m_lrm->getBBox(); }

private:
  TLevelReaderMov *m_lrm;
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
TWriterInfo *TWriterInfoMov::create(const string &)
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

//-----------------------------------------------------------

TWriterInfoMov::TWriterInfoMov()
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
  // codecNameSpec->typeName contains the name of the codec in Pascal String
Format
  // byte 0 = Len
  // byte [1..Len] data
  UCHAR len = codecNameSpec->typeName[0];
  string name((char*)&codecNameSpec->typeName[1], len);

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

TWriterInfoMov::~TWriterInfoMov()
{}

*/
//-----------------------------------------------------------

void TImageWriterMov::save(const TImageP &img) {
  m_lwm->save(img, m_frameIndex);
}

//-----------------------------------------------------------

namespace {
bool getFSRefFromPosixPath(const char *filepath, FSRef *fileRef, bool writing) {
  const UInt8 *filename = (const UInt8 *)filepath;
  if (writing) {
    FILE *fp = fopen(filepath, "wb");
    if (!fp) return false;
    fclose(fp);
  }
  OSErr err = FSPathMakeRef(filename, fileRef, 0);
  return err == noErr;
}

bool getFSSpecFromPosixPath(const char *filepath, FSSpec *fileSpec,
                            bool writing) {
  FSRef fileRef;

  if (!getFSRefFromPosixPath(filepath, &fileRef, writing)) return false;
  OSErr err = FSGetCatalogInfo(&fileRef, kFSCatInfoNone, 0, 0, fileSpec, 0);
  return err == noErr;
}
}

void TLevelWriterMov::save(const TImageP &img, int frameIndex) {
  m_firstFrame = tmin(frameIndex, m_firstFrame);

  TRasterImageP image(img);
  if (!image) throw TImageException(getFilePath(), "Unsupported image type");

  int lx = image->getRaster()->getLx();
  int ly = image->getRaster()->getLy();
  // void *buffer = image->getRaster()->getRawData();

  int pixSize = image->getRaster()->getPixelSize();
  if (pixSize != 4)
    throw TImageException(getFilePath(), "Unsupported pixel type");

  QMutexLocker sl(&m_mutex);
  if (!m_properties) m_properties = new Tiio::MovWriterProperties();

  Tiio::MovWriterProperties *prop = (Tiio::MovWriterProperties *)(m_properties);

  // CodecType codecType;
  // try {
  // codecType = prop->getCurrentCodec();
  //  } catch (...)//boost::bad_any_cast&)
  //    {
  //    throw TImageException(m_path, "bad codec name");
  //    }
  //
  // CodecQ quality = prop->getCurrentQuality();
  QDErr err;

  if (m_videoTrack == 0) {
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

    // FSSpec fspec;
    Rect frame;
    long max_compressed_size;

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

#if QT_VERSION >= 0x050000
    if ((err = QTNewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0)) != noErr)
#else
    if ((err = NewGWorld(&(m_gworld), pixSize * 8, &frame, 0, 0, 0)) != noErr)
#endif
      throw TImageException(getFilePath(), "can't create movie buffer");

    //#ifdef WIN32
    //  LockPixels(m_gworld->portPixMap);
    //  if ((err = GetMaxCompressionSize(m_gworld->portPixMap, &frame, 0,
    //                                quality,  compression,anyCodec,
    //                         	 &max_compressed_size))!=noErr)
    //    throw TImageException(getFilePath(), "can't get max compression
    //    size");
    //
    //#else
    //  PixMapHandle pixmapH = GetPortPixMap (m_gworld);
    //
    //  LockPixels(pixmapH);
    //  if ((err = GetMaxCompressionSize(pixmapH, &frame, 0,
    //                                quality,  codecType,anyCodec,
    //				 &max_compressed_size))!=noErr)
    //    throw TImageException(getFilePath(), "can't get max compression
    //    size");
    //
    //#endif

    // m_compressedData = NewHandle(max_compressed_size);

    if ((err = MemError()) != noErr)
      throw TImageException(getFilePath(),
                            "can't allocate compressed data for movie");

    // MoveHHi(m_compressedData);
    // HLock(m_compressedData);

    if ((err = MemError()) != noErr)
      throw TImageException(getFilePath(), "can't allocate img handle");

/* FIXME: QuickDraw とともに無くなったようだ */
#if defined(HAS_QUICKDRAW)
    m_pixmap = GetGWorldPixMap(m_gworld);

    if (!LockPixels(m_pixmap))
      throw TImageException(getFilePath(), "can't lock pixels");

    buf = (PixelXRGB *)GetPixBaseAddr(m_pixmap);
#endif
    buf_lx = lx;
    buf_ly = ly;
  }

  if ((err = BeginMediaEdits(m_videoMedia)) != noErr)
    throw TImageException(getFilePath(), "can't begin edit video media");

  unsigned short rowBytes =
      (unsigned short)(((short)(*(m_pixmap))->rowBytes & ~(3 << 14)));

  Rect frame;
  ImageDescriptionHandle img_descr = 0;
  Handle compressedData            = 0;

  frame.left   = 0;
  frame.top    = 0;
  frame.right  = lx;
  frame.bottom = ly;

  TRasterP ras = image->getRaster();

#ifdef WIN32
  compressed_data_ptr = StripAddress(*(m_compressedData));
  copy(ras, buf, buf_lx, buf_ly);
#else
  // compressed_data_ptr  = *m_compressedData;
  copy(ras, buf, buf_lx, buf_ly, rowBytes);
#endif

// img_descr = (ImageDescriptionHandle)NewHandle(4);

#ifdef WIN32
  if ((err = CompressImage(m_gworld->portPixMap, &frame, quality, compression,
                           img_descr, compressed_data_ptr)) != noErr)
    throw TImageException(getFilePath(), "can't compress image");
#else

#if defined(HAS_QUICKDRAW)
  PixMapHandle pixmapH = GetPortPixMap(m_gworld);

  if ((err = SCCompressImage(m_ci, pixmapH, &frame, &img_descr,
                             &compressedData)) != noErr)
    throw TImageException(getFilePath(), "can't compress image");
#endif

// if ((err = CompressImage(pixmapH,
//	                 &frame,
//  			 quality, codecType,
//			 img_descr, compressed_data_ptr))!=noErr)
//  throw TImageException(getFilePath(), "can't compress image");

#endif

  TimeValue sampleTime;
  if ((err = AddMediaSample(
           m_videoMedia, compressedData, 0, (*img_descr)->dataSize, 100,
           (SampleDescriptionHandle)img_descr, 1, 0, &sampleTime)) != noErr)
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
    //,m_compressedData(0)
    , m_gworld(0)
    , m_videoMedia(0)
    , m_videoTrack(0)
    , m_soundMedia(0)
    , m_soundTrack(0)
    , m_movie(0)
    , m_firstFrame((std::numeric_limits<int>::max)()) {
  m_frameRate = 12.;
  QDErr err;
  FSSpec fspec;
  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    m_IOError = QTNotInstalled;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }

  QString qStr     = QString::fromStdWString(m_path.getWideString());
  const char *pStr = qStr.toUtf8().data();

  if (!getFSSpecFromPosixPath(pStr, &fspec, true)) {
    m_IOError = QTUnableToOpenFile;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }

  // Fix: in alcuni rari casi non veniva il mov preesistente
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);

  if ((err = CreateMovieFile(&fspec, 'TVOD', smCurrentScript,
                             createMovieFileDeleteCurFile |
                                 createMovieFileDontCreateResFile,
                             &(m_refNum), &(m_movie)) != noErr)) {
    m_IOError = QTCantCreateFile;
    throw TImageException(m_path, buildQTErrorString(m_IOError));
  }
}

//-----------------------------------------------------------

#ifdef _DEBUG
#define FailIf(cond, handler)                                                  \
  if (cond) {                                                                  \
    /*		DebugStr((ConstStr255Param)#cond " goto " #handler);*/                 \
    goto handler;                                                              \
  }
#else
#define FailIf(cond, handler)                                                  \
  if (cond) {                                                                  \
    goto handler;                                                              \
  }
#endif

#ifdef _DEBUG
#define C(cond, action, handler)                                               \
  if (cond) {                                                                  \
    /*		DebugStr((ConstStr255Param)#cond " goto " #handler);*/                 \
    { action; }                                                                \
    goto handler;                                                              \
  }
#else
#define FailWithAction(cond, action, handler)                                  \
  if (cond) {                                                                  \
    { action; }                                                                \
    goto handler;                                                              \
  }
#endif

void TLevelWriterMov::saveSoundTrack(TSoundTrack *st) {
  OSErr myErr = noErr;

  // int err;

  if (!st) throw TException("null reference to soundtrack");

  if (st->getBitPerSample() != 16) {
    throw TImageException(m_path, "Only 16 bits per sample is supported");
  }

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

  /*if ((err = AddMediaSample(m_videoMedia, compressedData,  0,
                          (*img_descr)->dataSize, 1,
                                                                                            (SampleDescriptionHandle)img_descr,
                                                                                                  1, 0, 0))!=noErr)
throw TImageException(getFilePath(), "can't add image to movie media");*/

  myErr = AddMediaSample(
      m_soundMedia, myDestHandle, 0,
      destInfo.sampleCount * compressionInfo.bytesPerFrame, 1,
      (SampleDescriptionHandle)mySampleDesc,
      destInfo.sampleCount * compressionInfo.samplesPerPacket, 0, NULL);
  FailIf(myErr != noErr, MediaErr);

  myErr = EndMediaEdits(m_soundMedia);
  FailIf(myErr != noErr, MediaErr);

  //////////
  //
  // insert the media into the track
  //
  //////////

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

TLevelWriterMov::~TLevelWriterMov() {
/* FIXME: */
#if defined(HAS_QUICKDRAW)
  if (m_pixmap) UnlockPixels(m_pixmap);
  if (m_gworld) DisposeGWorld(m_gworld);
#endif
  if (m_ci) CloseComponent(m_ci);

  QDErr err;
  OSStatus mderr;

  if (m_videoTrack) {
    // Insert the saved Frames into the track. Holes in the frames sequence will
    // be intended as still
    // frames.
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
    // FailIf(err != noErr, MediaErr);
  }

  short resId = movieInDataForkResID;
  if (m_movie)
    if ((err = AddMovieResource(m_movie, m_refNum, &resId, 0)) != noErr) {
    }

/**************************************************************************************/
#ifdef PROVA3GP
  QString qStr        = QString::fromStdWString(m_path.getWideString());
  char *inputFileName = qStr.toUtf8().data();

  OSType movieType = FOUR_CHAR_CODE('3gpp');
  OSType creator   = 'TVOD';
  Movie movie;

  // get the file
  FSSpec inFsSpec;
  // NativePathNameToFSSpec(inputFileName, &inFsSpec, 0);
  FSMakeFSSpec(0, 0, inputFileName, &inFsSpec);
  getFSSpecFromPosixPath(pStr, &inFsSpec, false);

  // Initialize Quicktime Movie Toolbox
  // EnterMovies ();

  // open the movie
  short sResRefNum;
  OpenMovieFile(&inFsSpec, &sResRefNum, fsRdPerm);
  NewMovieFromFile(&movie, sResRefNum, nil, nil, 0, nil);
  CloseMovieFile(sResRefNum);

  UCHAR myCancelled = FALSE;
  MovieExportComponent m_myExporter;
  OpenADefaultComponent(MovieExportType, '3gpp', &m_myExporter);
  err = (short)MovieExportDoUserDialog(m_myExporter, movie, 0, 0, 0,
                                       &myCancelled);

  if ((err != noErr)) throw TImageException(getFilePath(), "can't export 3gp");

  FSSpec fspec;
  FSMakeFSSpec(0, 0, "C:\\temp\\out.3gp", &fspec);
  getFSSpecFromPosixPath(pStr, &fspec, false);

  // NativePathNameToFSSpec	("C:\\temp\\out.3gp", &fspec, kFullNativePath);

  long myFlags = 0L;
  myFlags      = createMovieFileDeleteCurFile;  // |
  // movieFileSpecValid | movieToFileOnlyExport;

  err = ConvertMovieToFile(movie,                   // the movie to convert
                           0,                       // all tracks in the movie
                           &fspec,                  // the output file
                           FOUR_CHAR_CODE('3gpp'),  // the output file type
                           FOUR_CHAR_CODE('TVOD'),  // the output file creator
                           smSystemScript,          // the script
                           0,              // no resource ID to be returned
                           myFlags,        // export flags
                           m_myExporter);  // no specific exp
}
if (sResRefNum) CloseMovieFile(sResRefNum);
DisposeMovie(movie);
}
#endif

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
    , m_readAsToonzOutput(false) {
  FSSpec fspec;
  QDErr err;
  Boolean dataRefWasChanged;
  if (QuickTimeStuff::instance()->getStatus() != noErr) {
    m_IOError = QTNotInstalled;
    return;
  }

  QString qStr = QString::fromStdWString(m_path.getWideString());
  char *pStr = qStr.toUtf8().data();

  getFSSpecFromPosixPath(pStr, &fspec, false);
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
  // assert(numTracks==1 || numTracks ==2);
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

  m_info->m_frameRate = GetMediaTimeScale(theMedia) / 100.0;
}

//------------------------------------------------

//------------------------------------------------

//------------------------------------------------
// TImageReaderMov
//------------------------------------------------

TImageReaderMov::TImageReaderMov(const TFilePath &path, int frameIndex,
                                 TLevelReaderMov *lrm)
    : TImageReader(path), m_lrm(lrm), m_frameIndex(frameIndex) {
  m_lrm->addRef();
}

//------------------------------------------------

TLevelReaderMov::~TLevelReaderMov() {
  StopMovie(m_movie);

  if (m_refNum) CloseMovieFile(m_refNum);
  if (m_movie) DisposeMovie(m_movie);
}

//------------------------------------------------

TLevelP TLevelReaderMov::loadInfo() {
  if (m_readAsToonzOutput) return loadToonzOutputFormatInfo();

  TLevelP level;
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  OSType mediaType = VisualMediaCharacteristic;
  TimeValue nextTime, currentTime;
  currentTime = 0;
  nextTime    = 0;
  // per il primo
  int f = 0;
  // io vorrei fare '|', ma sul manuale c'e' scritto '+'
  GetMovieNextInterestingTime(m_movie, nextTimeMediaSample + nextTimeEdgeOK, 1,
                              &mediaType, currentTime, 0, &nextTime, 0);
  if (nextTime != -1) {
    TFrameId frame(f + 1);
    level->setFrame(frame, TImageP());
    currentTimes[f] = nextTime;
    f++;
  }
  currentTime = nextTime;
  while (nextTime != -1) {
    GetMovieNextInterestingTime(m_movie, nextTimeMediaSample, 1, &mediaType,
                                currentTime, 0, &nextTime, 0);
    if (nextTime != -1) {
      TFrameId frame(f + 1);
      level->setFrame(frame, TImageP());
      currentTimes[f] = nextTime;
      f++;
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

  OSType mediaType = VisualMediaCharacteristic;
  TimeValue nextTime, currentTime;
  currentTime = 0;
  nextTime    = 0;

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
          firstFrame + tround((*it * frameRate) / (double)movieTimeScale);
      TFrameId frameId(frame + 1);
      level->setFrame(frameId, TImageP());
      currentTimes[frame] = *it;
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
  int hLy = (int)(ras->getLy() / 2. + 0.5);  // piccola pessimizzazione...
  int wrap          = ras->getWrap();
  int lx            = ras->getLx();
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
  ras->lock();
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

#if defined TNZ_MACHINE_CHANNEL_ORDER_BGRM
    OSType pixelFormat = k32BGRAPixelFormat;
#elif defined TNZ_MACHINE_CHANNEL_ORDER_MRGB
    OSType pixelFormat = k32ARGBPixelFormat;
#endif
    // ras->lock();
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

      goto error;
    }

#if defined(HAS_QUICKDRAW)
    SetMovieGWorld(m_movie, offscreenGWorld, GetGWorldDevice(offscreenGWorld));
#endif
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

    SetMovieGWorld(m_movie, 0, 0);
#if defined(HAS_QUICKDRAW)
    DisposeGWorld(offscreenGWorld);
#endif
    // ras->unlock();
  }

  if (m_depth != 32) {
    setMatteAndYMirror(rasP);
  } else {
    rasP->yMirror();
  }

  return;

error:
// rasP->unlock();
#if defined(HAS_QUICKDRAW)
  if (offscreenGWorld) DisposeGWorld(offscreenGWorld);
#endif
  throw TImageException(m_path, buildQTErrorString(m_IOError));
}

//------------------------------------------------

TImageReaderP TLevelReaderMov::getFrameReader(TFrameId fid) {
  if (m_IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(m_IOError));

  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReaderMov *irm = new TImageReaderMov(m_path, index, this);
  return TImageReaderP(irm);
}

#endif  //!__LP64__
