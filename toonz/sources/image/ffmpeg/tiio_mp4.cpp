
#include "tsystem.h"
#include "tiio_mp4.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "tsound.h"
#include "toonz/stage.h"
#include <QStringList>

//===========================================================
//
//  TImageWriterMp4
//
//===========================================================

class TImageWriterMp4 : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterMp4(const TFilePath &path, int frameIndex, TLevelWriterMp4 *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterMp4() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterMp4 *m_lwg;
};

//===========================================================
//
//  TLevelWriterMp4;
//
//===========================================================

TLevelWriterMp4::TLevelWriterMp4(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::Mp4WriterProperties();
  if (m_properties->getPropertyCount() == 0) {
    m_scale      = 100;
    m_vidQuality = 100;
  } else {
    std::string scale = m_properties->getProperty("Scale")->getValueAsString();
    m_scale           = QString::fromStdString(scale).toInt();
    std::string quality =
        m_properties->getProperty("Quality")->getValueAsString();
    m_vidQuality = QString::fromStdString(quality).toInt();
  }
  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterMp4::~TLevelWriterMp4() {
  // QProcess createMp4;
  QStringList preIArgs;
  QStringList postIArgs;

  int outLx = m_lx;
  int outLy = m_ly;

  // set scaling
  if (m_scale != 0) {
    outLx = m_lx * m_scale / 100;
    outLy = m_ly * m_scale / 100;
  }
  // ffmpeg doesn't like resolutions that aren't divisible by 2.
  if (outLx % 2 != 0) outLx++;
  if (outLy % 2 != 0) outLy++;

  // calculate quality (bitrate)
  int pixelCount   = m_lx * m_ly;
  int bitRate      = pixelCount / 150;  // crude but gets decent values
  double quality   = m_vidQuality / 100.0;
  double tempRate  = (double)bitRate * quality;
  int finalBitrate = (int)tempRate;
  int crf          = 51 - (m_vidQuality * 51 / 100);

  preIArgs << "-framerate";
  preIArgs << QString::number(m_frameRate);

  postIArgs << "-pix_fmt";
  postIArgs << "yuv420p";
  postIArgs << "-s";
  postIArgs << QString::number(outLx) + "x" + QString::number(outLy);
  postIArgs << "-b";
  postIArgs << QString::number(finalBitrate) + "k";

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterMp4::getFrameWriter(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildMp4ExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index            = fid.getNumber();
  TImageWriterMp4 *iwg = new TImageWriterMp4(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterMp4::setFrameRate(double fps) {
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterMp4::saveSoundTrack(TSoundTrack *st) {
  ffmpegWriter->saveSoundTrack(st);
}

//-----------------------------------------------------------

void TLevelWriterMp4::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderMp4
//
//===========================================================

class TImageReaderMp4 final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderMp4(const TFilePath &path, int index, TLevelReaderMp4 *lra,
                  TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderMp4() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderMp4 *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderMp4(const TImageReaderMp4 &);
  TImageReaderMp4 &operator=(const TImageReaderMp4 &src);
};

//===========================================================
//
//  TLevelReaderMp4
//
//===========================================================

TLevelReaderMp4::TLevelReaderMp4(const TFilePath &path) : TLevelReader(path) {
  ffmpegReader = new Ffmpeg();
  ffmpegReader->setPath(m_path);
  ffmpegReader->disablePrecompute();
  ffmpegFileInfo tempInfo = ffmpegReader->getInfo();
  double fps              = tempInfo.m_frameRate;
  m_frameCount            = tempInfo.m_frameCount;
  m_size                  = TDimension(tempInfo.m_lx, tempInfo.m_ly);
  m_lx                    = m_size.lx;
  m_ly                    = m_size.ly;

  ffmpegReader->getFramesFromMovie();

  // set values
  m_info                   = new TImageInfo();
  m_info->m_frameRate      = fps;
  m_info->m_lx             = m_lx;
  m_info->m_ly             = m_ly;
  m_info->m_bitsPerSample  = 8;
  m_info->m_samplePerPixel = 4;
  m_info->m_dpix           = Stage::standardDpi;
  m_info->m_dpiy           = Stage::standardDpi;
}
//-----------------------------------------------------------

TLevelReaderMp4::~TLevelReaderMp4() {
  // ffmpegReader->cleanUpFiles();
}

//-----------------------------------------------------------

TLevelP TLevelReaderMp4::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderMp4::getFrameReader(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderMp4 *irm = new TImageReaderMp4(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderMp4::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderMp4::load(int frameIndex) {
  return ffmpegReader->getImage(frameIndex);
}

Tiio::Mp4WriterProperties::Mp4WriterProperties()
    : m_vidQuality("Quality", 1, 100, 90), m_scale("Scale", 1, 100, 100) {
  bind(m_vidQuality);
  bind(m_scale);
}

void Tiio::Mp4WriterProperties::updateTranslation() {
  m_vidQuality.setQStringName(tr("Quality"));
  m_scale.setQStringName(tr("Scale"));
}

// Tiio::Reader* Tiio::makeMp4Reader(){ return nullptr; }
// Tiio::Writer* Tiio::makeMp4Writer(){ return nullptr; }