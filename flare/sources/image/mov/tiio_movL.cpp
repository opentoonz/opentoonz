

#include <openquicktime.h>
#include <colormodels.h>

#include "tiio_movL.h"
#include "traster.h"
#include "trasterimage.h"
#include "tsound.h"
#include "tconvert.h"
#include "trop.h"
#include "tsystem.h"
//#define DUMP_KEYFRAMES

bool IsQuickTimeInstalled() { return true; }

enum {
  QTNoError = 0,
  QTUnableToOpenFile,
  QTUnableToSeekToKeyFrame,
  QTNoSuchFile,
  QTUnsupportedVideoFormat
};

class TQTException : public TImageException {
public:
  TQTException(const TFilePath &fp, int ec)
      : TImageException(fp, getErrorMessage(ec)) {}
  TQTException(const TFilePath &fp, int ec, int v)
      : TImageException(fp, getErrorMessage(ec) + toString(v)) {}

  ~TQTException() {}
  static string getErrorMessage(int ec) {
    switch (ec) {
    case QTNoError:
      return "No error";
    case QTUnableToOpenFile:
      return "unable to open file";
    case QTUnableToSeekToKeyFrame:
      return "unable to seek to keyframe";
    case QTNoSuchFile:
      return "no such file";
    case QTUnsupportedVideoFormat:
      return "Unsupported video format";
    }
    return "unknown error";
  }
};

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

void TImageWriterMov::save(const TImageP &img) { assert(false); }

//-----------------------------------------------------------
//        TLevelWriterMov
//-----------------------------------------------------------

/*
class TWriterInfoMov : public TWriterInfo {
public:
  TWriterInfoMov() : TWriterInfo() { assert(!"Not implemented"); }
  ~TWriterInfoMov() {}
private:
};
*/

//-----------------------------------------------------------

TLevelWriterMov::TLevelWriterMov(const TFilePath &path, TWriterInfo *info)
    : TLevelWriter(path, info)
//,m_initDone(false)
//,m_rate(25)
//,m_IOError(QTNoError)
//,quality(DM_IMAGE_QUALITY_NORMAL)
//,compression(DM_IMAGE_QT_ANIM)
//,m_writerInfo(new TWriterInfoMov())
{
  assert(false);
}

//-----------------------------------------------------------

void TLevelWriterMov::saveSoundTrack(TSoundTrack *) {
  throw TImageException(m_path,
                        "TLevelWriterMov::saveSoundTrack not Implemented");
}
//-----------------------------------------------------------

/*
TWriterInfo *TLevelWriterMov::getWriterInfo() const
{
return m_writerInfo;
}
*/

//-----------------------------------------------------------

TLevelWriterMov::~TLevelWriterMov() { assert(0); }

//-----------------------------------------------------------

TImageWriterP TLevelWriterMov::getFrameWriter(TFrameId fid) { assert(false); }

//-----------------------------------------------------------
//------------------------------------------------

//------------------------------------------------
// TImageReaderMov
//------------------------------------------------

TImageReaderMov::TImageReaderMov(const TFilePath &path, int frameIndex,
                                 TLevelReaderMov *lrm)
    : TImageReader(path), m_lrm(lrm), m_frameIndex(frameIndex) {}

//------------------------------------------------

using namespace std;

TLevelReaderMov::TLevelReaderMov(const TFilePath &path)
    : TLevelReader(path)
    , m_IOError(QTNoError)
    , m_lastFrameDecoded(-1)
    , m_fileMov(0)
    , m_lx(0)
    , m_ly(0) {
  if (!TFileStatus(path).doesExist()) {
    m_IOError = QTNoSuchFile;
    return;
  }
  m_fileMov = oqt_open((char *)path.getString().c_str());
  if (!m_fileMov) {
    m_IOError = QTUnableToOpenFile;
    return;
  }

  oqt_read_headers(m_fileMov);

  /*
if(!oqt_get_video_track_count(m_fileMov))
{
m_status = !DM_SUCCESS;
m_IOError = QTCheckLibError;
return;
}
*/

  m_lx = oqt_get_video_width(m_fileMov, 0);
  m_ly = oqt_get_video_height(m_fileMov, 0);
  /*
char *vc = oqt_get_video_compressor(m_fileMov, 0);
cout << "video compressor = " << vc[0] << vc[1] << vc[2] << vc[3] << endl;
*/
}

//------------------------------------------------

TLevelReaderMov::~TLevelReaderMov() {
  if (m_fileMov) oqt_close(m_fileMov);
}

//------------------------------------------------

TLevelP TLevelReaderMov::loadInfo() {
  TLevelP level;
  if (m_IOError != QTNoError) {
    throw TQTException(m_path, m_IOError);
  }
  if (oqt_supported_video(m_fileMov, 0) == 0) {
    m_IOError = QTUnsupportedVideoFormat;
    throw TQTException(m_path, m_IOError);
  }

  int frameCount = oqt_get_video_length(m_fileMov, 0);

  for (int i = 1; i <= frameCount; i++) level->setFrame(TFrameId(i), TImageP());

#ifdef DUMP_KEYFRAMES
  for (int k = 1; k < frameCount; k++) {
    cout << "frame = " << k << "; keyframe before = "
         << oqt_get_video_keyframe_before(m_fileMov, 0, k)
         << "; keyframe after = "
         << oqt_get_video_keyframe_after(m_fileMov, 0, k) << endl;
  }
#endif

  return level;
}

//------------------------------------------------

TImageP TImageReaderMov::load() {
  if (m_lrm->m_IOError != QTNoError)
    throw TQTException(m_path, m_lrm->m_IOError);

  TThread::ScopedLock sl(m_lrm->m_mutex);
  int rc;
  TRaster32P ret(m_lrm->m_lx, m_lrm->m_ly);
  unsigned char **data =
      (unsigned char **)malloc(m_lrm->m_ly * sizeof(unsigned char *));
  for (int i = 0; i < m_lrm->m_ly; ++i) {
    unsigned char *ptr = (unsigned char *)ret->pixels(i);
    data[i]            = ptr;
  }
  int cmodel = BC_RGBA8888;
  int frame;
  if (m_lrm->m_lastFrameDecoded != (m_frameIndex - 1)) {
    oqt_int64_t kfb =
        oqt_get_video_keyframe_before(m_lrm->m_fileMov, 0, m_frameIndex);
    assert(kfb <= m_frameIndex);
    rc = oqt_set_video_position(m_lrm->m_fileMov, 0, kfb);
    if (rc) {
      throw TQTException(m_lrm->m_path, QTUnableToSeekToKeyFrame, kfb);
    }
    frame = kfb - 1;
  } else
    frame = m_lrm->m_lastFrameDecoded;

  do {
    rc = oqt_decode_video(m_lrm->m_fileMov, 0, cmodel, data);
    frame++;
  } while (frame != m_frameIndex);

  m_lrm->m_lastFrameDecoded = m_frameIndex;
  ret->yMirror();
  free(data);
  return TRasterImageP(ret);
}

//------------------------------------------------

TImageReaderP TLevelReaderMov::getFrameReader(TFrameId fid) {
  if (m_IOError != QTNoError) {
    throw TQTException(m_path, m_IOError);
  }

  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber() - 1;

  TImageReaderMov *irm = new TImageReaderMov(m_path, index, this);
  return TImageReaderP(irm);
}
