

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#define _WIN32_DCOM

#define _WIN32_WINNT 0x0500
#include "tsystem.h"

#include <windows.h>
#include <objbase.h>
#include "texception.h"
#include "tiio_avi.h"
#include "tsound.h"
#include "tconvert.h"

#include "timageinfo.h"
#include "trasterimage.h"

#include <type_traits>

//------------------------------------------------------------------------------

namespace {

#define DibPtr(lpbi) (LPBYTE)(DibColors(lpbi) + (UINT)(lpbi)->biClrUsed)
#define DibColors(lpbi) ((LPRGBQUAD)((LPBYTE)(lpbi) + (int)(lpbi)->biSize))

//-----------------------------------------------------------

void WideChar2Char(LPCWSTR wideCharStr, char *str, int strBuffSize) {
  WideCharToMultiByte(CP_ACP, 0, wideCharStr, -1, str, strBuffSize, 0, 0);
}

//------------------------------------------------------------------------------

std::string buildAVIExceptionString(int rc) {
  switch (rc) {
  case AVIERR_BADFORMAT:
    return "The file couldn't be read, indicating a corrupt file or an "
           "unrecognized format.";
  case AVIERR_MEMORY:
    return "The file could not be opened because of insufficient memory.";
  case AVIERR_FILEREAD:
    return "A disk error occurred while reading the file.";
  case AVIERR_FILEOPEN:
    return "A disk error occurred while opening the file.";
  case REGDB_E_CLASSNOTREG:
    return "According to the registry, the type of file specified in "
           "m_aviFileOpen does not have a handler to process it.";
  case AVIERR_UNSUPPORTED:
    return "Format unsupported";
  case AVIERR_INTERNAL:
    return "Internal error";
  case AVIERR_NODATA:
    return "No data";
  default:
    return "Unable to create avi.";
  }
}

//------------------------------------------------------------------------------

std::string SplitFourCC(DWORD fcc) {
  std::string s;
  s += (char((fcc & 0x000000ff) >> 0));
  s += (char((fcc & 0x0000ff00) >> 8));
  s += (char((fcc & 0x00ff0000) >> 16));
  s += (char((fcc & 0xff000000) >> 24));

  return s;
}

//-----------------------------------------------------------

TRaster32P DIBToRaster(UCHAR *pDIBImage, int width, int height) {
  assert(pDIBImage);
  if (!pDIBImage) return TRaster32P();

  // BITMAPINFOHEADER *bihdr = reinterpret_cast<BITMAPINFOHEADER *>(pDIBImage);
  UCHAR *rawData = pDIBImage;

  TRaster32P rasOut32(width, height);
  // ULONG imgSize = bihdr->biSizeImage;

  int ly = rasOut32->getLy();
  int n  = 0;
  rasOut32->lock();
  while (n < ly) {
    TPixel32 *pix    = rasOut32->pixels(n);
    TPixel32 *endPix = pix + rasOut32->getLx();
    while (pix < endPix) {
      pix->b = *rawData;
      rawData++;
      pix->g = *rawData;
      rawData++;
      pix->r = *rawData;
      rawData++;
      pix->m = 255;
      ++pix;
      // rawData += 3;
    }
    ++n;
  }
  rasOut32->unlock();
  return rasOut32;
}

//-----------------------------------------------------------

int getPrevKeyFrame(PAVISTREAM videoStream, int index) {
  return AVIStreamFindSample(videoStream, index, FIND_PREV | FIND_KEY);
}

//-----------------------------------------------------------

bool isAKeyFrame(PAVISTREAM videoStream, int index) {
  return index == getPrevKeyFrame(videoStream, index);
}

};  // end of namespace

//===========================================================
//
//  TImageWriterAvi
//
//===========================================================

class TImageWriterAvi final : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterAvi(const TFilePath &path, int frameIndex, TLevelWriterAvi *lwa)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwa(lwa) {
    m_lwa->addRef();
  }
  ~TImageWriterAvi() { m_lwa->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwa->save(img, m_frameIndex); }

private:
  TLevelWriterAvi *m_lwa;

  // not implemented
  TImageWriterAvi(const TImageWriterAvi &);
  TImageWriterAvi &operator=(const TImageWriterAvi &src);
};

//===========================================================
//
//  TLevelWriterAvi;
//
//===========================================================

#ifdef _WIN32

TLevelWriterAvi::TLevelWriterAvi(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo)
    , m_aviFile(0)
    , m_videoStream(0)
    , m_audioStream(0)
    , m_bitmapinfo(0)
    , m_outputFmt(0)
    , m_hic(0)
    , m_initDone(false)
    , IOError(0)
    , m_st(0)
    , m_bpp(32)
    , m_maxDataSize(0)
    , m_buffer(0)
    , m_firstframe(-1) {
  CoInitializeEx(0, COINIT_MULTITHREADED);

  m_frameRate = 12.;

  AVIFileInit();

  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);

  int rc = AVIFileOpenW(&m_aviFile, m_path.getWideString().c_str(),
                        OF_CREATE | OF_WRITE, 0);
  if (rc != 0) throw TImageException(getFilePath(), "Unable to create file");
  m_delayedFrames.clear();
}

//-----------------------------------------------------------

TLevelWriterAvi::~TLevelWriterAvi() {
  // controllo che non ci siano ancora dei frame in coda nel codec (alcuni codec
  // sono asincroni!)
  while (!m_delayedFrames.empty()) {
    LONG lSampWritten, lBytesWritten;
    int frameIndex = m_delayedFrames.front();
    DWORD flagsOut = 0;
    DWORD flagsIn  = !frameIndex ? AVIIF_KEYFRAME : 0;
    BITMAPINFOHEADER outHeader;
    void *bufferOut = 0;
    int res =
        compressFrame(&outHeader, &bufferOut, frameIndex, flagsIn, flagsOut);
    if (res != ICERR_OK) {
      if (bufferOut) _aligned_free(bufferOut);
      break;
    }
    if (AVIStreamWrite(m_videoStream, frameIndex, 1, bufferOut,
                       outHeader.biSizeImage, flagsOut, &lSampWritten,
                       &lBytesWritten)) {
      if (bufferOut) _aligned_free(bufferOut);
      break;
    }
    m_delayedFrames.pop_front();
    if (bufferOut) _aligned_free(bufferOut);
  }

  if (m_st) {
    doSaveSoundTrack();
    m_st = 0;
  }

  if (m_hic) {
    ICCompressEnd(m_hic);
    ICClose(m_hic);
  }

  if (m_bitmapinfo) free(m_bitmapinfo);
  if (m_outputFmt) free(m_outputFmt);

  if (m_videoStream) AVIStreamClose(m_videoStream);

  if (m_audioStream) AVIStreamClose(m_audioStream);

  if (m_aviFile) AVIFileClose(m_aviFile);

  AVIFileExit();
  CoUninitialize();
  if (!m_delayedFrames.empty())
    throw TImageException(
        getFilePath(),
        "error compressing frame " + std::to_string(m_delayedFrames.front()));
}

//-----------------------------------------------------------

void TLevelWriterAvi::searchForCodec() {
  if (!m_properties) m_properties = new Tiio::AviWriterProperties();

  TEnumProperty *p = (TEnumProperty *)m_properties->getProperty("Codec");
  assert(p);
  std::wstring codecName = p->getValue();

  //--------  // cerco compressorName fra i codec

  HIC hic = 0;
  ICINFO icinfo;
  memset(&icinfo, 0, sizeof(ICINFO));
  bool found = false;
  if (codecName != L"Uncompressed") {
    char descr[2048], name[2048];
    DWORD fccType = 0;

    BITMAPINFO inFmt;
    memset(&inFmt, 0, sizeof(BITMAPINFO));

    inFmt.bmiHeader.biSize  = sizeof(BITMAPINFOHEADER);
    inFmt.bmiHeader.biWidth = inFmt.bmiHeader.biHeight = 100;
    inFmt.bmiHeader.biPlanes                           = 1;
    inFmt.bmiHeader.biCompression                      = BI_RGB;
    found                                              = false;

    for (int bpp = 32; (bpp >= 24) && !found; bpp -= 8) {
      inFmt.bmiHeader.biBitCount = bpp;
      for (int i = 0; ICInfo(fccType, i, &icinfo); i++) {
        hic = ICOpen(icinfo.fccType, icinfo.fccHandler, ICMODE_COMPRESS);

        ICGetInfo(hic, &icinfo,
                  sizeof(ICINFO));  // Find out the compressor name
        WideChar2Char(icinfo.szDescription, descr, sizeof(descr));
        WideChar2Char(icinfo.szName, name, sizeof(name));

        std::string compressorName;
        compressorName = std::string(name) + " '" + std::to_string(bpp) + "' " +
                         std::string(descr);

        if (hic) {
          if (ICCompressQuery(hic, &inFmt, NULL) != ICERR_OK) {
            ICClose(hic);
            continue;  // Skip this compressor if it can't handle the format.
          }
          if (::to_wstring(compressorName) == codecName) {
            found = true;
            m_bpp = bpp;
            break;
          }
          ICClose(hic);
        }
      }
    }
  }
  m_hic = hic;
}

//-----------------------------------------------------------

void TLevelWriterAvi::createBitmap(int lx, int ly) {
  const RGBQUAD NOMASK = {0x00, 0x00, 0x00, 0x00};
  m_bitmapinfo =
      (BITMAPINFO *)calloc(1, sizeof(BITMAPINFOHEADER) + 255 * sizeof(RGBQUAD));

  m_bitmapinfo->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
  m_bitmapinfo->bmiHeader.biWidth         = lx;
  m_bitmapinfo->bmiHeader.biHeight        = ly;
  m_bitmapinfo->bmiHeader.biPlanes        = 1;
  m_bitmapinfo->bmiHeader.biBitCount      = m_bpp;
  m_bitmapinfo->bmiHeader.biCompression   = BI_RGB;
  m_bitmapinfo->bmiHeader.biSizeImage     = lx * ly * m_bpp / 8;
  m_bitmapinfo->bmiHeader.biXPelsPerMeter = 0;
  m_bitmapinfo->bmiHeader.biYPelsPerMeter = 0;
  m_bitmapinfo->bmiHeader.biClrUsed       = 0;
  m_bitmapinfo->bmiHeader.biClrImportant  = 0;

  m_bitmapinfo->bmiColors[0] = NOMASK;
  m_bitmapinfo->bmiColors[1] = NOMASK;
  m_bitmapinfo->bmiColors[2] = NOMASK;

  m_buffer = _aligned_malloc(m_bitmapinfo->bmiHeader.biSizeImage, 128);
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterAvi::getFrameWriter(TFrameId fid) {
  if (IOError != 0)
    throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index            = fid.getNumber() - 1;
  TImageWriterAvi *iwa = new TImageWriterAvi(m_path, index, this);
  return TImageWriterP(iwa);
}

//-----------------------------------------------------------

void TLevelWriterAvi::save(const TImageP &img, int frameIndex) {
  CoInitializeEx(0, COINIT_MULTITHREADED);

  if (m_firstframe < 0) m_firstframe = frameIndex;
  int index                          = frameIndex - m_firstframe;
  TRasterImageP image(img);
  int lx = image->getRaster()->getLx();
  int ly = image->getRaster()->getLy();

  int pixelSize = image->getRaster()->getPixelSize();

  if (pixelSize != 4)
    throw TImageException(getFilePath(), "Unsupported pixel type");

  QMutexLocker sl(&m_mutex);

  if (!m_initDone) {
    searchForCodec();
    createBitmap(lx, ly);

    DWORD num = DWORD(tround(m_frameRate * 100.0));
    DWORD den = 100;

    AVISTREAMINFO psi;
    memset(&psi, 0, sizeof(AVISTREAMINFO));
    psi.fccType = streamtypeVIDEO;
    if (m_hic) {
      ICINFO icinfo;
      ICGetInfo(m_hic, &icinfo, sizeof(ICINFO));
      psi.fccHandler = icinfo.fccHandler;
    }
    psi.dwScale        = den;
    psi.dwRate         = num;
    psi.dwQuality      = ICQUALITY_DEFAULT;
    psi.rcFrame.right  = lx;
    psi.rcFrame.bottom = ly;
    ::strcpy(psi.szName, m_path.getName().c_str());
    int rc;

    if (AVIFileCreateStream(m_aviFile, &(m_videoStream), &(psi)))
      throw TImageException(getFilePath(), "Unable to create video stream");

    if (!m_hic) {
      if (AVIStreamSetFormat(m_videoStream, 0, &m_bitmapinfo->bmiHeader,
                             m_bitmapinfo->bmiHeader.biSize))
        throw TImageException(getFilePath(), "unable to set format");
    } else {
      m_outputFmt = (BITMAPINFO *)calloc(
          1, sizeof(BITMAPINFOHEADER) + 255 * sizeof(RGBQUAD));
      m_outputFmt->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

      rc = ICCompressGetFormat(m_hic, m_bitmapinfo, m_outputFmt);
      if (rc != ICERR_OK)
        throw TImageException(getFilePath(),
                              "Error codec (ec = " + std::to_string(rc) + ")");

      ICCOMPRESSFRAMES icf;
      memset(&icf, 0, sizeof icf);

      icf.dwFlags     = (DWORD)&icf.lKeyRate;
      icf.lStartFrame = index;
      icf.lFrameCount = 0;
      icf.lQuality    = ICQUALITY_DEFAULT;
      icf.lDataRate   = 0;
      icf.lKeyRate    = (DWORD)(num / den);
      icf.dwRate      = num;
      icf.dwScale     = den;
      ICSendMessage(m_hic, ICM_COMPRESS_FRAMES_INFO, (WPARAM)&icf,
                    sizeof(ICCOMPRESSFRAMES));

      m_maxDataSize = ICCompressGetSize(m_hic, m_bitmapinfo, m_outputFmt);

      rc = ICCompressBegin(m_hic, m_bitmapinfo, m_outputFmt);
      if (rc != ICERR_OK)
        throw TImageException(getFilePath(), "Error starting codec (ec = " +
                                                 std::to_string(rc) + ")");

      if (AVIStreamSetFormat(m_videoStream, 0, &m_outputFmt->bmiHeader,
                             m_outputFmt->bmiHeader.biSize))
        throw TImageException(getFilePath(), "unable to set format");
    }
    m_initDone = true;
  }

  // copio l'immagine nella bitmap che ho creato in createBitmap
  image->getRaster()->lock();
  void *buffin = image->getRaster()->getRawData();
  assert(buffin);

  TRasterGR8P raux;
  if (m_bpp == 24) {
    int newlx = lx * 3;
    raux      = TRasterGR8P(newlx, ly);
    raux->lock();
    UCHAR *buffout24 = (UCHAR *)raux->getRawData();  // new UCHAR[newlx*ly];
    TPixel *buffin32 = (TPixel *)buffin;
    buffin           = buffout24;
    for (int i = 0; i < ly; i++) {
      UCHAR *pix = buffout24 + newlx * i;
      for (int j = 0; j < lx; j++, buffin32++) {
        *pix++ = buffin32->b;
        *pix++ = buffin32->g;
        *pix++ = buffin32->r;
      }
    }
    raux->unlock();
  }
  memcpy(m_buffer, buffin, lx * ly * m_bpp / 8);
  image->getRaster()->unlock();
  raux = TRasterGR8P();

  LONG lSampWritten, lBytesWritten;

  lSampWritten  = 0;
  lBytesWritten = 0;

  if (!m_hic) {
    if (AVIStreamWrite(m_videoStream, index, 1, m_buffer,
                       m_bitmapinfo->bmiHeader.biSizeImage, AVIIF_KEYFRAME,
                       &lSampWritten, &lBytesWritten)) {
      throw TImageException(getFilePath(), "error writing frame");
    }
  } else {
    BITMAPINFOHEADER outHeader;
    void *bufferOut = 0;

    DWORD flagsOut = 0;
    DWORD flagsIn  = !index ? ICCOMPRESS_KEYFRAME : 0;
    int res = compressFrame(&outHeader, &bufferOut, index, flagsIn, flagsOut);
    if (res != ICERR_OK) {
      if (bufferOut) _aligned_free(bufferOut);
      throw TImageException(getFilePath(),
                            "error compressing frame " + std::to_string(index));
    }

    if (outHeader.biCompression == '05xd' ||
        outHeader.biCompression == '05XD' ||
        outHeader.biCompression == 'divx' || outHeader.biCompression == 'DIVX')
      if (outHeader.biSizeImage == 1 && *(char *)bufferOut == 0x7f) {
        m_delayedFrames.push_back(index);
        if (bufferOut) _aligned_free(bufferOut);
        return;
      }
    if (!m_delayedFrames.empty()) {
      m_delayedFrames.push_back(index);
      index = m_delayedFrames.front();
      m_delayedFrames.pop_front();
    }
    if (AVIStreamWrite(m_videoStream, index, 1, bufferOut,
                       outHeader.biSizeImage, !index ? flagsOut : 0,
                       &lSampWritten, &lBytesWritten)) {
      if (bufferOut) _aligned_free(bufferOut);
      throw TImageException(getFilePath(), "error writing frame");
    }
    if (bufferOut) _aligned_free(bufferOut);
  }
}

//-----------------------------------------------------------

int TLevelWriterAvi::compressFrame(BITMAPINFOHEADER *outHeader,
                                   void **bufferOut, int frameIndex,
                                   DWORD flagsIn, DWORD &flagsOut) {
  *bufferOut            = _aligned_malloc(m_maxDataSize, 128);
  *outHeader            = m_outputFmt->bmiHeader;
  DWORD chunkId         = 0;
  if (flagsIn) flagsOut = AVIIF_KEYFRAME;
  int res               = ICCompress(m_hic, flagsIn, outHeader, *bufferOut,
                       &m_bitmapinfo->bmiHeader, m_buffer, &chunkId, &flagsOut,
                       frameIndex, frameIndex ? 0 : 0xFFFFFF, 0, NULL, NULL);
  return res;
}

#else

TLevelWriterAvi::TLevelWriterAvi(const TFilePath &path) : TLevelWriter(path) {}
TLevelWriterAvi::~TLevelWriterAvi() {}
TImageWriterP TLevelWriterAvi::getFrameWriter(TFrameId) {
  throw TImageException(getFilePath(), "not supported");
  return TImageWriterP(iwa);
}
void TImageWriterAvi::save(const TImageP &) {
  throw TImageException(m_path, "AVI file format not supported");
}

#endif

//-----------------------------------------------------------

void TLevelWriterAvi::saveSoundTrack(TSoundTrack *st) {
  if (!m_aviFile) throw TException("unable to save soundtrack");

  if (!st) throw TException("null reference to soundtrack");

  m_st =
      st;  // prima devo aver salvato tutto il video, salvo l'audio alla fine!
}

//-----------------------------------------------------------

void TLevelWriterAvi::doSaveSoundTrack() {
  WAVEFORMATEX waveinfo;
  int ret;
  LONG lSampWritten, lBytesWritten;
  int rc;

  rc = FALSE;

  AVISTREAMINFO audioStreamInfo;
  memset(&audioStreamInfo, 0, sizeof(AVISTREAMINFO));

  audioStreamInfo.fccType    = streamtypeAUDIO;
  audioStreamInfo.fccHandler = 0;
  audioStreamInfo.dwFlags    = 0;
  audioStreamInfo.dwCaps     = 0;
  audioStreamInfo.wPriority  = 0;
  audioStreamInfo.wLanguage  = 0;
  audioStreamInfo.dwScale    = 1;

  audioStreamInfo.dwRate = m_st->getSampleRate();

  audioStreamInfo.dwStart               = 0;
  audioStreamInfo.dwLength              = 0;
  audioStreamInfo.dwInitialFrames       = 0;
  audioStreamInfo.dwSuggestedBufferSize = 0;
  audioStreamInfo.dwQuality             = (ULONG)-1;
  audioStreamInfo.dwSampleSize          = 0;
  audioStreamInfo.rcFrame.left          = 0;
  audioStreamInfo.rcFrame.top           = 0;
  audioStreamInfo.rcFrame.right         = 0;
  audioStreamInfo.rcFrame.bottom        = 0;
  audioStreamInfo.dwEditCount           = 0;
  audioStreamInfo.dwFormatChangeCount   = 0;
  audioStreamInfo.szName[0]             = 0;

  waveinfo.wFormatTag      = WAVE_FORMAT_PCM;  // WAVE_FORMAT_DRM
  waveinfo.nChannels       = m_st->getChannelCount();
  waveinfo.nSamplesPerSec  = m_st->getSampleRate();
  waveinfo.wBitsPerSample  = m_st->getBitPerSample();
  waveinfo.nBlockAlign     = waveinfo.nChannels * waveinfo.wBitsPerSample >> 3;
  waveinfo.nAvgBytesPerSec = waveinfo.nSamplesPerSec * waveinfo.nBlockAlign;
  waveinfo.cbSize          = sizeof(WAVEFORMATEX);

  const UCHAR *buffer = m_st->getRawData();

  if (AVIFileCreateStream(m_aviFile, &m_audioStream, &audioStreamInfo))
    throw TException("error creating soundtrack stream");

  if (AVIStreamSetFormat(m_audioStream, 0, &waveinfo, sizeof(WAVEFORMATEX)))
    throw TException("error setting soundtrack format");

  LONG count   = m_st->getSampleCount();
  LONG bufSize = m_st->getSampleCount() * m_st->getSampleSize();

  if (ret = AVIStreamWrite(m_audioStream, 0, count, (char *)buffer, bufSize,
                           AVIIF_KEYFRAME, &lSampWritten, &lBytesWritten))
    throw TException("error writing soundtrack samples");
}

//===========================================================
//
//  TImageReaderAvi
//
//===========================================================

class TImageReaderAvi final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderAvi(const TFilePath &path, int index, TLevelReaderAvi *lra)
      : TImageReader(path), m_lra(lra), m_frameIndex(index) {
    m_lra->addRef();
  }
  ~TImageReaderAvi() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }

private:
  TLevelReaderAvi *m_lra;

  // not implemented
  TImageReaderAvi(const TImageReaderAvi &);
  TImageReaderAvi &operator=(const TImageReaderAvi &src);
};

//===========================================================
//
//  TLevelReaderAvi
//
//===========================================================

#ifdef _WIN32
TLevelReaderAvi::TLevelReaderAvi(const TFilePath &path)
    : TLevelReader(path)
#ifdef _WIN32
    , m_srcBitmapInfo(0)
    , m_dstBitmapInfo(0)
    , m_hic(0)
    , IOError(0)
    , m_prevFrame(-1)
    , m_decompressedBuffer(0)
#endif
{
  CoInitializeEx(0, COINIT_MULTITHREADED);

  AVIFileInit();

  int rc = AVIStreamOpenFromFileW(&m_videoStream, path.getWideString().c_str(),
                                  streamtypeVIDEO, 0, OF_READ, 0);
  if (rc != 0) {
    IOError = rc;
    throw TImageException(m_path, buildAVIExceptionString(IOError));
  }
  LONG size       = sizeof(BITMAPINFO);
  m_srcBitmapInfo = (BITMAPINFO *)calloc(1, size);
  m_dstBitmapInfo = (BITMAPINFO *)calloc(1, size);

  rc = AVIStreamReadFormat(m_videoStream, 0, m_srcBitmapInfo, &size);
  if (rc) throw TImageException(getFilePath(), "error reading info.");

  *m_dstBitmapInfo                         = *m_srcBitmapInfo;
  m_dstBitmapInfo->bmiHeader.biCompression = 0;

  AVISTREAMINFO si;
  rc = AVIStreamInfo(m_videoStream, &si, sizeof(AVISTREAMINFO));
  if (rc) throw TImageException(getFilePath(), "error reading info.");

  m_info              = new TImageInfo();
  m_info->m_frameRate = si.dwRate / double(si.dwScale);
  m_info->m_lx        = si.rcFrame.right - si.rcFrame.left;
  m_info->m_ly        = si.rcFrame.bottom - si.rcFrame.top;

  int bpp = m_srcBitmapInfo->bmiHeader.biBitCount;
  switch (bpp) {
  case 32: {
    m_info->m_bitsPerSample  = 8;
    m_info->m_samplePerPixel = 4;
    m_dstBitmapInfo->bmiHeader.biSizeImage =
        m_dstBitmapInfo->bmiHeader.biWidth *
        m_dstBitmapInfo->bmiHeader.biHeight * 4;
    break;
  }
  case 24: {
    m_info->m_bitsPerSample  = 8;
    m_info->m_samplePerPixel = 3;
    m_dstBitmapInfo->bmiHeader.biSizeImage =
        m_dstBitmapInfo->bmiHeader.biWidth *
        m_dstBitmapInfo->bmiHeader.biHeight * 3;
    break;
  }
  case 16: {
    m_info->m_bitsPerSample  = 8;
    m_info->m_samplePerPixel = 2;
    // chiedo al decoder di decomprimerla in un'immagine a 24 bit (sperando che
    // lo permetta)
    m_dstBitmapInfo->bmiHeader.biBitCount = 24;
    m_dstBitmapInfo->bmiHeader.biSizeImage =
        m_dstBitmapInfo->bmiHeader.biWidth *
        m_dstBitmapInfo->bmiHeader.biHeight * 3;
    break;
  }
  default: {
    throw TImageException(getFilePath(),
                          "unable to find a decompressor for this format");
  }
  }

  Tiio::AviWriterProperties *prop = new Tiio::AviWriterProperties();
  m_info->m_properties            = prop;

  if (m_srcBitmapInfo->bmiHeader.biCompression != 0) {
    // ci sono dei codec (es. Xvid) che non decomprimono bene dentro immagini a
    // 32 bit.
    m_dstBitmapInfo->bmiHeader.biBitCount = 24;
    m_dstBitmapInfo->bmiHeader.biSizeImage =
        m_dstBitmapInfo->bmiHeader.biWidth *
        m_dstBitmapInfo->bmiHeader.biHeight * 3;

    ICINFO icinfo;
    memset(&icinfo, 0, sizeof(ICINFO));
    ICInfo(ICTYPE_VIDEO, si.fccHandler, &icinfo);
    m_hic = ICOpen(icinfo.fccType, icinfo.fccHandler, ICMODE_DECOMPRESS);
    if (!m_hic) {
      m_hic = findCandidateDecompressor();
      if (!m_hic)
        throw TImageException(getFilePath(),
                              "unable to find a decompressor for this format");
    }

    DWORD result = ICDecompressQuery(m_hic, m_srcBitmapInfo, m_dstBitmapInfo);
    if (result != ICERR_OK)
      throw TImageException(getFilePath(),
                            "unable to find a decompressor for this format");

    result = ICDecompressBegin(m_hic, m_srcBitmapInfo, m_dstBitmapInfo);
    if (result != ICERR_OK)
      throw TImageException(getFilePath(),
                            "unable to initializate the decompressor");

    char descr[2048], name[2048];
    ICGetInfo(m_hic, &icinfo, sizeof(ICINFO));  // Find out the compressor name
    WideChar2Char(icinfo.szDescription, descr, sizeof(descr));
    WideChar2Char(icinfo.szName, name, sizeof(name));
    std::string compressorName;
    compressorName = std::string(name) + " '" +
                     std::to_string(m_dstBitmapInfo->bmiHeader.biBitCount) +
                     "' " + std::string(descr);
    TEnumProperty *p =
        (TEnumProperty *)m_info->m_properties->getProperty("Codec");
    p->setValue(::to_wstring(compressorName));
    m_decompressedBuffer =
        _aligned_malloc(m_dstBitmapInfo->bmiHeader.biSizeImage, 128);
  } else {
    m_hic = 0;
    TEnumProperty *p =
        (TEnumProperty *)m_info->m_properties->getProperty("Codec");
    p->setValue(L"Uncompressed");
    m_decompressedBuffer = _aligned_malloc(si.dwSuggestedBufferSize, 128);
  }
}

//------------------------------------------------

TLevelReaderAvi::~TLevelReaderAvi() {
  if (m_hic) {
    ICDecompressEnd(m_hic);
    ICClose(m_hic);
  }

  if (m_srcBitmapInfo) free(m_srcBitmapInfo);
  if (m_dstBitmapInfo) free(m_dstBitmapInfo);

  if (m_videoStream) AVIStreamClose(m_videoStream);

  AVIFileExit();
  _aligned_free(m_decompressedBuffer);
}

HIC TLevelReaderAvi::findCandidateDecompressor() {
  ICINFO info        = {0};
  BITMAPINFO srcInfo = *m_srcBitmapInfo;
  BITMAPINFO dstInfo = *m_dstBitmapInfo;

  for (DWORD id = 0; ICInfo(ICTYPE_VIDEO, id, &info); ++id) {
    info.dwSize = sizeof(
        ICINFO);  // I don't think this is necessary, but just in case....

    HIC hic = ICOpen(info.fccType, info.fccHandler, ICMODE_DECOMPRESS);

    if (!hic) continue;

    DWORD result = ICDecompressQuery(hic, &srcInfo, &dstInfo);

    if (result == ICERR_OK) {
      // Check for a codec that doesn't actually support what it says it does.
      // We ask the codec whether it can do a specific conversion that it can't
      // possibly support.  If it does it, then we call BS and ignore the codec.
      // The Grand Tech Camera Codec and Panasonic DV codecs are known to do
      // this.
      //
      // (general idea from Raymond Chen's blog)

      BITMAPINFOHEADER testSrc = {// note: can't be static const since
                                  // IsBadWritePtr() will get called on it
                                  sizeof(BITMAPINFOHEADER),
                                  320,
                                  240,
                                  1,
                                  24,
                                  0x2E532E42,
                                  320 * 240 * 3,
                                  0,
                                  0,
                                  0,
                                  0};

      DWORD res = ICDecompressQuery(hic, &testSrc, NULL);

      if (ICERR_OK == res) {  // Don't need to wrap this, as it's OK if testSrc
                              // gets modified.
        ICINFO info = {sizeof(ICINFO)};
        ICGetInfo(hic, &info, sizeof info);

        // Okay, let's give the codec a chance to redeem itself. Reformat the
        // input format into
        // a plain 24-bit RGB image, and ask it what the compressed format is.
        // If it produces
        // a FOURCC that matches, allow it to handle the format. This should
        // allow at least
        // the codec's primary format to work. Otherwise, drop it on the ground.

        if (m_srcBitmapInfo) {
          BITMAPINFOHEADER unpackedSrc = {sizeof(BITMAPINFOHEADER),
                                          m_srcBitmapInfo->bmiHeader.biWidth,
                                          m_srcBitmapInfo->bmiHeader.biHeight,
                                          1,
                                          24,
                                          BI_RGB,
                                          0,
                                          0,
                                          0,
                                          0,
                                          0};

          unpackedSrc.biSizeImage =
              ((unpackedSrc.biWidth * 3 + 3) & ~3) * abs(unpackedSrc.biHeight);

          LONG size = ICCompressGetFormatSize(hic, &unpackedSrc);

          if (size >= sizeof(BITMAPINFOHEADER)) {
            BITMAPINFOHEADER tmp;
            if (ICERR_OK == ICCompressGetFormat(hic, &unpackedSrc, &tmp) &&
                tmp.biCompression == m_srcBitmapInfo->bmiHeader.biCompression)
              return hic;
          }
        }
      } else
        return hic;
    }
    ICClose(hic);
  }
  return NULL;
}

//-----------------------------------------------------------

TLevelP TLevelReaderAvi::loadInfo() {
  if (IOError) throw TImageException(m_path, buildAVIExceptionString(IOError));
  int nFrames = AVIStreamLength(m_videoStream);
  if (nFrames == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= nFrames; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderAvi::getFrameReader(TFrameId fid) {
  if (IOError != 0)
    throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReaderAvi *ira = new TImageReaderAvi(m_path, index, this);
  return TImageReaderP(ira);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderAvi::getSize() {
  QMutexLocker sl(&m_mutex);
  AVISTREAMINFO psi;
  AVIStreamInfo(m_videoStream, &psi, sizeof(AVISTREAMINFO));
  return TDimension(psi.rcFrame.right - psi.rcFrame.left,
                    psi.rcFrame.bottom - psi.rcFrame.top);
}

//------------------------------------------------

TImageP TLevelReaderAvi::load(int frameIndex) {
  AVISTREAMINFO si;
  int rc = AVIStreamInfo(m_videoStream, &si, sizeof(AVISTREAMINFO));
  if (rc) throw TImageException(getFilePath(), "error reading info.");
  void *bufferOut = 0;

  if (!m_hic) {
    rc = readFrameFromStream(m_decompressedBuffer, si.dwSuggestedBufferSize,
                             frameIndex);
    if (rc) {
      throw TImageException(m_path, "unable read frame " +
                                        std::to_string(frameIndex) +
                                        "from video stream.");
    }
  } else {
    int prevKeyFrame = m_prevFrame == frameIndex - 1
                           ? frameIndex
                           : getPrevKeyFrame(m_videoStream, frameIndex);
    while (prevKeyFrame <= frameIndex) {
      bufferOut       = _aligned_malloc(si.dwSuggestedBufferSize, 128);
      DWORD bytesRead = si.dwSuggestedBufferSize;
      rc              = readFrameFromStream(bufferOut, bytesRead, prevKeyFrame);
      if (rc) {
        _aligned_free(bufferOut);
        throw TImageException(m_path, "unable read frame " +
                                          std::to_string(frameIndex) +
                                          "from video stream.");
      }

      DWORD res = decompressFrame(bufferOut, bytesRead, m_decompressedBuffer,
                                  prevKeyFrame, frameIndex);

      _aligned_free(bufferOut);
      if (res != ICERR_OK) {
        throw TImageException(
            m_path, "error decompressing frame " + std::to_string(frameIndex));
      }
      prevKeyFrame++;
    }
  }

  int width  = m_srcBitmapInfo->bmiHeader.biWidth;
  int height = m_srcBitmapInfo->bmiHeader.biHeight;

  int bpp     = m_dstBitmapInfo->bmiHeader.biBitCount;
  m_prevFrame = frameIndex;
  switch (bpp) {
  case 32: {
    TRasterPT<TPixelRGBM32> ret;
    ret.create(width, height);
    ret->lock();
    memcpy(ret->getRawData(), m_decompressedBuffer, width * height * 4);
    ret->unlock();
    return TRasterImageP(ret);
  }
  case 24: {
    TRasterImageP i(DIBToRaster((UCHAR *)m_decompressedBuffer, width, height));
    return i;
  }
  default: {
    throw TImageException(m_path,
                          std::to_string(bpp) + " to 32 bit not supported\n");
  }
  }
  return TRasterImageP();
}

//------------------------------------------------

int TLevelReaderAvi::readFrameFromStream(void *bufferOut, DWORD &bufferSize,
                                         int frameIndex) const {
  assert(bufferOut && bufferSize > 0);
  LONG bytesReaded   = 0;
  LONG samplesReaded = 0;

  int rc = AVIStreamRead(m_videoStream, frameIndex, 1, bufferOut, bufferSize,
                         &bytesReaded, &samplesReaded);
  if (!rc) {
    assert(samplesReaded == 1);  // deve aver letto un frame!!!
    assert(bytesReaded <=
           (LONG)bufferSize);  // deve aver letto un numero di byte
    // minore o uguale di quello che ci aspettiamo
    bufferSize = bytesReaded;
  }
  return rc;
}

//------------------------------------------------

DWORD TLevelReaderAvi::decompressFrame(void *srcBuffer, int srcSize,
                                       void *dstBuffer, int currentFrame,
                                       int desiredFrame) {
  BITMAPINFOHEADER srcHeader = m_srcBitmapInfo->bmiHeader;
  BITMAPINFOHEADER dstHeader = m_dstBitmapInfo->bmiHeader;
  srcHeader.biSizeImage      = srcSize;

  DWORD dwFlags = 0;

  if (!isAKeyFrame(m_videoStream, currentFrame))
    dwFlags |= ICDECOMPRESS_NOTKEYFRAME;
  if (currentFrame < desiredFrame) dwFlags |= ICDECOMPRESS_PREROLL;

  DWORD res = ICDecompress(m_hic, dwFlags, &srcHeader, srcBuffer, &dstHeader,
                           dstBuffer);
  return res;
}

#else
TLevelReaderAvi::TLevelReaderAvi(const TFilePath &path)::TLevelReader(path) {}
TLevelReaderAvi::~TLevelReaderAvi() {}
TLevelP TLevelReaderAvi::loadInfo() { return TLevelP(); }
TImageReaderP TLevelReaderAvi::getFrameReader(TFrameId fid) {
  throw TImageException(m_path, "AVI not supported");
  return TImageReaderP(0);
}
TDimension TLevelReaderAvi::getSize() { return TDimension(); }
TImageP TLevelReaderAvi::load(int frameIndex) {
  throw TImageException(m_path, "AVI not supported");
  return TImageP(0);
}
#endif

//===========================================================
//
//  Tiio::AviWriterProperties
//
//===========================================================

#ifdef _WIN32

namespace {
BOOL safe_ICInfo(DWORD fccType, DWORD fccHandler, ICINFO *lpicinfo) {
#ifdef _MSC_VER
  __try {
    return ICInfo(fccType, fccHandler, lpicinfo);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return FALSE;
#else
  return ICInfo(fccType, fccHandler, lpicinfo);
#endif
}

LRESULT safe_ICClose(HIC hic) {
#ifdef _MSC_VER
  __try {
    if (hic) {
      return ICClose(hic);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#else
  if (hic) {
    return ICClose(hic);
  }
#endif
  return ICERR_OK;
}

#ifdef _MSC_VER
typedef std::unique_ptr<std::remove_pointer_t<HIC>, decltype(&safe_ICClose)>
    hic_t;
#else
typedef std::unique_ptr<std::remove_pointer<HIC>::type, decltype(&safe_ICClose)>
    hic_t;
#endif

hic_t safe_ICOpen(DWORD fccType, DWORD fccHandler, UINT wMode) {
#ifdef _MSC_VER
  HIC const hic = [fccType, fccHandler, wMode]() -> HIC {
    __try {
      return ICOpen(fccType, fccHandler, wMode);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
  }();
#else
  HIC hic = nullptr;
  try {
    hic = ICOpen(fccType, fccHandler, wMode);
  } catch (...) {
  }
#endif
  return hic_t(hic, safe_ICClose);
}

LRESULT safe_ICGetInfo(hic_t const &hic, ICINFO *picinfo, DWORD cb) {
#ifdef _MSC_VER
  __try {
    return ICGetInfo(hic.get(), picinfo, cb);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return 0;  // return copied size in bytes (0 means an error)
#else
  return ICGetInfo(hic.get(), picinfo, cb);
#endif
}

LRESULT safe_ICCompressQuery(hic_t const &hic, BITMAPINFO *lpbiInput,
                             BITMAPINFO *lpbiOutput) {
#ifdef _MSC_VER
  __try {
    return ICCompressQuery(hic.get(), lpbiInput, lpbiOutput);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
  return ICERR_INTERNAL;
#else
  return ICCompressQuery(hic.get(), lpbiInput, lpbiOutput);
#endif
}
}

Tiio::AviWriterProperties::AviWriterProperties() : m_codec("Codec") {
  if (m_defaultCodec.getRange().empty()) {
    char descr[2048], name[2048];
    DWORD fccType = 0;
    ICINFO icinfo;
    BITMAPINFO inFmt;

    m_defaultCodec.addValue(L"Uncompressed");

    memset(&inFmt, 0, sizeof(BITMAPINFO));
    inFmt.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    inFmt.bmiHeader.biWidth       = 100;
    inFmt.bmiHeader.biHeight      = 100;
    inFmt.bmiHeader.biPlanes      = 1;
    inFmt.bmiHeader.biCompression = BI_RGB;
    for (int bpp = 32; bpp >= 24; bpp -= 8) {
      inFmt.bmiHeader.biBitCount = bpp;
      for (int i = 0;; i++) {
        memset(&icinfo, 0, sizeof icinfo);
        if (!safe_ICInfo(fccType, i, &icinfo)) {
          break;
        }

        auto const hic =
            safe_ICOpen(icinfo.fccType, icinfo.fccHandler, ICMODE_QUERY);
        if (!hic) {
          break;
        }

        // Find out the compressor name
        if (safe_ICGetInfo(hic, &icinfo, sizeof(ICINFO)) == 0) {
          break;
        }

        WideChar2Char(icinfo.szDescription, descr, sizeof(descr));
        WideChar2Char(icinfo.szName, name, sizeof(name));
        if ((strstr(name, "IYUV") != 0) ||
            ((strstr(name, "IR32") != 0) && (bpp == 24))) {
          continue;
        }

        std::string compressorName;
        compressorName = std::string(name) + " '" + std::to_string(bpp) + "' " +
                         std::string(descr);

        // per il momento togliamo i codec indeo
        if (std::string(compressorName).find("Indeo") != -1) {
          continue;
        }

        if (safe_ICCompressQuery(hic, &inFmt, nullptr) != ICERR_OK) {
          continue;  // Skip this compressor if it can't handle the format.
        }

        m_defaultCodec.addValue(::to_wstring(compressorName));
        if (compressorName.find("inepak") != -1) {
          m_defaultCodec.setValue(::to_wstring(compressorName));
        }
      }
    }
  }
  m_codec = m_defaultCodec;
  bind(m_codec);
}

TEnumProperty Tiio::AviWriterProperties::m_defaultCodec =
    TEnumProperty("Codec");

#endif
