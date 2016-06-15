

#include "tiio_movX.h"
#include "traster.h"
#include "trasterimage.h"
#include "tsound.h"

#ifdef NOTE
Supportato solo RGBM32, sotto NT sarebbe possibile supportare anche 1bpp,
    ma mancano i TRaster opportuni...

    viene supportata
    solo(LA PRIMA) traccia video !!!!
#endif

    // namespace {
    enum QTLibError {
      QTNoError = 0x0000,

      QTCantCreateParams = 0x0101,
      QTCantSetParams    = 0x0102,
      QTPixelTypeError   = 0x0103,
      QTCheckLibError    = 0x0104
    };

string buildQTErrorString(int ec) {
  switch (ec) {
  case QTCantCreateParams:
    return "Unable to create default params";
  case QTCantSetParams:
    return "Unable to set default params";
  case QTPixelTypeError:
    return "Unsupported pixel type";
  case QTCheckLibError:
    return mvGetErrorStr(mvGetErrno());
  default:
    return "Unknown error";
  }
}
//}
//-----------------------------------------------------------
//  TImageWriterMov
//-----------------------------------------------------------

class TImageWriterMov : public TImageWriter {
public:
  TImageWriterMov(const TFilePath &, int frameIndex, TLevelWriterMov *);
  ~TImageWriterMov() {}
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
  ~TImageReaderMov() {}

private:
  // not implemented
  TImageReaderMov(const TImageReaderMov &);
  TImageReaderMov &operator=(const TImageReaderMov &src);

public:
  TImageP load();
  int m_frameIndex;

  TDimension getSize() const { return TDimension(m_lrm->m_lx, m_lrm->m_ly); }
  TRect getBBox() const {
    return TRect(0, 0, m_lrm->m_lx - 1, m_lrm->m_ly - 1);
  }

private:
  TLevelReaderMov *m_lrm;
};

//-----------------------------------------------------------
//-----------------------------------------------------------
//        TImageWriterMov
//-----------------------------------------------------------

TImageWriterMov::TImageWriterMov(const TFilePath &path, int frameIndex,
                                 TLevelWriterMov *lwm)
    : TImageWriter(path), m_lwm(lwm), m_frameIndex(frameIndex) {}

//-----------------------------------------------------------

void TImageWriterMov::save(const TImageP &img) {
  TRasterImageP image(img);
  int lx       = image->getRaster()->getLx();
  int ly       = image->getRaster()->getLy();
  void *buffer = image->getRaster()->getRawData();
  int pixSize  = image->getRaster()->getPixelSize();
  if (pixSize != 4)
    throw TImageException(m_lwm->getFilePath(), "Unsupported pixel type");

  if (!m_lwm->m_initDone) {
    DMparams *imageTrackParams;

    if (dmParamsCreate(&imageTrackParams) != DM_SUCCESS)
      throw TImageException(m_lwm->getFilePath(),
                            "Unable to create image track params");

    if (dmSetImageDefaults(imageTrackParams, lx, ly, DM_IMAGE_PACKING_XRGB) !=
        DM_SUCCESS) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(),
                            "Unable to set image defaults");
    }

    if (dmParamsSetFloat(imageTrackParams, DM_IMAGE_RATE,
                         (double)m_lwm->m_rate) != DM_SUCCESS) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(), "Unable to set frame rate");
    }

    if (dmParamsSetEnum(imageTrackParams, DM_IMAGE_ORIENTATION,
                        DM_TOP_TO_BOTTOM) != DM_SUCCESS) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(), "Unable to set frame rate");
    }

    if (dmParamsSetFloat(imageTrackParams, DM_IMAGE_QUALITY_SPATIAL,
                         m_lwm->quality) != DM_SUCCESS) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(), "Unable to set quality");
    }

    if (dmParamsSetString(imageTrackParams, DM_IMAGE_COMPRESSION,
                          m_lwm->compression) != DM_SUCCESS) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(), "Unable to set compression");
    }

    if (mvAddTrack(m_lwm->id, DM_IMAGE, imageTrackParams, NULL,
                   &(m_lwm->imageTrack)) == DM_FAILURE) {
      dmParamsDestroy(imageTrackParams);
      throw TImageException(m_lwm->getFilePath(),
                            "Unable to add image track to movie");
    }

    dmParamsDestroy(imageTrackParams);
    m_lwm->m_initDone = true;
  }

  if (mvInsertFrames(m_lwm->imageTrack, m_frameIndex, 1, lx * ly * pixSize,
                     buffer) != DM_SUCCESS) {
    throw TImageException(m_lwm->getFilePath(),
                          "Unable to write image to movie");
  }
}

//-----------------------------------------------------------
//        TLevelWriterMov
//-----------------------------------------------------------

class TWriterInfoMov : public TWriterInfo {
public:
  TWriterInfoMov() : TWriterInfo() { assert(!"Not implemented"); }
  ~TWriterInfoMov() {}

private:
};

//-----------------------------------------------------------

TLevelWriterMov::TLevelWriterMov(const TFilePath &path)
    : TLevelWriter(path)
    , m_initDone(false)
    , m_rate(25)
    , m_IOError(QTNoError)
    , quality(DM_IMAGE_QUALITY_NORMAL)
    , compression(DM_IMAGE_QT_ANIM)
    , m_writerInfo(new TWriterInfoMov()) {
  DMparams *movieParams;

  if (dmParamsCreate(&movieParams) != DM_SUCCESS) {
    m_IOError = QTCantCreateParams;
    return;
  }

  if (mvSetMovieDefaults(movieParams, MV_FORMAT_QT) != DM_SUCCESS) {
    dmParamsDestroy(movieParams);
    m_IOError = QTCantCreateParams;
    return;
  }
  if (mvCreateFile(path.getFullPath().c_str(), movieParams, NULL, &id) ==
      DM_FAILURE) {
    static char m[1024];
    dmParamsDestroy(movieParams);
    m_IOError = QTCheckLibError;
    return;
  }

  dmParamsDestroy(movieParams);
}

//-----------------------------------------------------------

void TLevelWriterMov::saveSoundTrack(TSoundTrack *) {
  throw TImageException(m_path,
                        "TLevelWriterMov::saveSoundTrack not Implemented");
}
//-----------------------------------------------------------

TWriterInfo *TLevelWriterMov::getWriterInfo() const { return m_writerInfo; }

//-----------------------------------------------------------

TLevelWriterMov::~TLevelWriterMov() {
  bool rc = (mvClose(id) == DM_SUCCESS);
  // if (!rc)
  //  throw TImageException(getFilePath(), "Error closing mov file");
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
    , IOError(QTNoError)

{
  m_status = mvOpenFile(path.getFullPath().c_str(), O_RDONLY, &movie);
  if (m_status != DM_SUCCESS) {
    IOError = QTCheckLibError;
    return;
  }
  track    = 0;
  m_status = mvFindTrackByMedium(movie, DM_IMAGE, &track);
  if (m_status != DM_SUCCESS) {
    IOError = QTCheckLibError;
    return;
  }
  m_lx              = mvGetImageWidth(track);
  m_ly              = mvGetImageHeight(track);
  DMpacking packing = mvGetImagePacking(track);
  if (packing != DM_IMAGE_PACKING_XRGB) {
    IOError = QTPixelTypeError;
    return;
  }
}

//------------------------------------------------

//------------------------------------------------
// TImageReaderMov
//------------------------------------------------

TImageReaderMov::TImageReaderMov(const TFilePath &path, int frameIndex,
                                 TLevelReaderMov *lrm)
    : TImageReader(path), m_lrm(lrm), m_frameIndex(frameIndex) {}

//------------------------------------------------

TLevelReaderMov::~TLevelReaderMov() { mvClose(movie); }

//------------------------------------------------

TLevelP TLevelReaderMov::loadInfo() {
  TLevelP level;
  if (IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(IOError));

  if (!track)
    throw TImageException(getFilePath().getFullPath().c_str(),
                          " error reading info");

  MVframe nFrames = mvGetTrackLength(track);
  if (nFrames == -1) return level;
  for (int i = 1; i <= nFrames; i++) level->setFrame(TFrameId(i), TImageP());
  return level;
}

//------------------------------------------------

TImageP TImageReaderMov::load() {
  TRaster32P ret(m_lrm->m_lx, m_lrm->m_ly);

  DMstatus status =
      mvReadFrames(m_lrm->track, m_frameIndex, 1, m_lrm->m_lx * m_lrm->m_ly * 4,
                   ret->getRawData());
  if (status != DM_SUCCESS) {
    throw TImageException(getFilePath().getFullPath().c_str(),
                          mvGetErrorStr(mvGetErrno()));
  }

  // getImage()->setRaster(ret);
  return TRasterImageP(ret);
}

//------------------------------------------------

TImageReaderP TLevelReaderMov::getFrameReader(TFrameId fid) {
  if (IOError != QTNoError)
    throw TImageException(m_path, buildQTErrorString(IOError));

  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReaderMov *irm = new TImageReaderMov(m_path, index, this);
  return TImageReaderP(irm);
}
