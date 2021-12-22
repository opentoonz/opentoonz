#include "jpgconverter.h"

#include <QFile>
#include <QDataStream>
//=============================================================================
//=============================================================================

JpgConverter::JpgConverter() {}
JpgConverter::~JpgConverter() {}

#ifdef WITH_CANON

void JpgConverter::setStream(EdsStreamRef stream) { m_stream = stream; }

void JpgConverter::convertFromJpg() {
#ifdef MACOSX
  UInt64 mySize = 0;
#else
  unsigned __int64 mySize = 0;
#endif
  unsigned char* data = NULL;
  EdsError err        = EdsGetPointer(m_stream, (EdsVoid**)&data);
  err                 = EdsGetLength(m_stream, &mySize);

  int width, height, pixelFormat;
  int inSubsamp, inColorspace;
  tjhandle tjInstance   = NULL;
  unsigned char* imgBuf = NULL;
  tjInstance            = tjInitDecompress();
  tjDecompressHeader3(tjInstance, data, mySize, &width, &height, &inSubsamp,
                      &inColorspace);

  if (width < 0 || height < 0) {
    emit(imageReady(false));
    return;
  }

  pixelFormat = TJPF_BGRX;
  imgBuf = (unsigned char*)tjAlloc(width * height * tjPixelSize[pixelFormat]);
  int flags = 0;
  flags |= TJFLAG_BOTTOMUP;

  int factorsNum;
  tjscalingfactor scalingFactor = {1, 1};
  tjscalingfactor* factor       = tjGetScalingFactors(&factorsNum);
  int i                         = 0;
  int tempWidth, tempHeight;
  while (i < factorsNum) {
    scalingFactor = factor[i];
    i++;
    tempWidth  = TJSCALED(width, scalingFactor);
    tempHeight = TJSCALED(height, scalingFactor);
  }
  tjDecompress2(tjInstance, data, mySize, imgBuf, width,
                width * tjPixelSize[pixelFormat], height, pixelFormat, flags);

  m_finalImage = TRaster32P(width, height);
  m_finalImage->lock();
  uchar* rawData = m_finalImage->getRawData();
  memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
  m_finalImage->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  if (m_stream != NULL) {
    EdsRelease(m_stream);
    m_stream = NULL;
  }
  data = NULL;
  emit(imageReady(true));
}

void JpgConverter::run() { convertFromJpg(); }

#endif

//-----------------------------------------------------------------------------

void JpgConverter::saveJpg(TRaster32P image, TFilePath path) {
  unsigned char* jpegBuf = NULL; /* Dynamically allocate the JPEG buffer */
  unsigned long jpegSize = 0;
  int pixelFormat        = TJPF_BGRX;
  int outQual            = 95;
  int subSamp            = TJSAMP_411;
  bool success           = false;
  tjhandle tjInstance;

  int width  = image->getLx();
  int height = image->getLy();
  int flags  = 0;
  flags |= TJFLAG_BOTTOMUP;

  image->lock();
  uchar* rawData = image->getRawData();
  if ((tjInstance = tjInitCompress()) != NULL) {
    if (tjCompress2(tjInstance, rawData, width, 0, height, pixelFormat,
                    &jpegBuf, &jpegSize, subSamp, outQual, flags) >= 0) {
      success = true;
    }
  }
  image->unlock();
  tjDestroy(tjInstance);
  tjInstance = NULL;

  if (success) {
    /* Write the JPEG image to disk. */
    QFile fullImage(path.getQString());
    fullImage.open(QIODevice::WriteOnly);
    QDataStream dataStream(&fullImage);
    dataStream.writeRawData((const char*)jpegBuf, jpegSize);
    fullImage.close();
  }
  tjFree(jpegBuf);
  jpegBuf = NULL;
}

//-----------------------------------------------------------------------------

bool JpgConverter::loadJpg(TFilePath path, TRaster32P& image) {
  long size;
  int inSubsamp, inColorspace, width, height;
  unsigned long jpegSize;
  unsigned char* jpegBuf;
  FILE* jpegFile;
  QString qPath      = path.getQString();
  QByteArray ba      = qPath.toLocal8Bit();
  const char* c_path = ba.data();
  bool success       = true;
  tjhandle tjInstance;

  /* Read the JPEG file into memory. */
  if ((jpegFile = fopen(c_path, "rb")) == NULL) success = false;
  if (success && fseek(jpegFile, 0, SEEK_END) < 0 ||
      ((size = ftell(jpegFile)) < 0) || fseek(jpegFile, 0, SEEK_SET) < 0)
    success = false;
  if (success && size == 0) success = false;
  jpegSize = (unsigned long)size;
  if (success && (jpegBuf = (unsigned char*)tjAlloc(jpegSize)) == NULL)
    success = false;
  if (success && fread(jpegBuf, jpegSize, 1, jpegFile) < 1) success = false;
  fclose(jpegFile);
  jpegFile = NULL;

  if (success && (tjInstance = tjInitDecompress()) == NULL) success = false;

  if (success && tjDecompressHeader3(tjInstance, jpegBuf, jpegSize, &width,
                                     &height, &inSubsamp, &inColorspace) < 0)
    success = false;

  int pixelFormat       = TJPF_BGRX;
  unsigned char* imgBuf = NULL;
  if (success &&
      (imgBuf = tjAlloc(width * height * tjPixelSize[pixelFormat])) == NULL)
    success = false;

  int flags = 0;
  flags |= TJFLAG_BOTTOMUP;
  if (success && tjDecompress2(tjInstance, jpegBuf, jpegSize, imgBuf, width, 0,
                               height, pixelFormat, flags) < 0)
    success = false;
  tjFree(jpegBuf);
  jpegBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  image = TRaster32P(width, height);
  image->lock();
  uchar* rawData = image->getRawData();
  memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
  image->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;

  return success;
}