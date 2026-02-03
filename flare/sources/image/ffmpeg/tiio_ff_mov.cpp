
#include "tsystem.h"
#include "tiio_ff_mov.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "tsound.h"
#include "toonz/stage.h"
#include <QStringList>

//===========================================================
//
//  TImageWriterFFMov
//
//===========================================================

class TImageWriterFFMov : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterFFMov(const TFilePath &path, int frameIndex,
                    TLevelWriterFFMov *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterFFMov() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterFFMov *m_lwg;
};

//===========================================================
//
//  TLevelWriterFFMov;
//
//===========================================================

TLevelWriterFFMov::TLevelWriterFFMov(const TFilePath &path,
                                     TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::FFMovWriterProperties();
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

TLevelWriterFFMov::~TLevelWriterFFMov() {
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

  postIArgs << "-c:v";
  postIArgs << "prores_ks";
  postIArgs << "-pix_fmt";
  postIArgs << "yuva444p10le";
  postIArgs << "-profile:v";
  postIArgs << "4";
  postIArgs << "-s";
  postIArgs << QString::number(outLx) + "x" + QString::number(outLy);
  postIArgs << "-b";
  postIArgs << QString::number(finalBitrate) + "k";

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterFFMov::getFrameWriter(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);
  int index              = fid.getNumber();
  TImageWriterFFMov *iwg = new TImageWriterFFMov(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterFFMov::setFrameRate(double fps) {
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterFFMov::saveSoundTrack(TSoundTrack *st) {
  ffmpegWriter->saveSoundTrack(st);
}

//-----------------------------------------------------------

void TLevelWriterFFMov::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderFFMov
//
//===========================================================

class TImageReaderFFMov final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderFFMov(const TFilePath &path, int index, TLevelReaderFFMov *lra,
                    TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderFFMov() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderFFMov *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderFFMov(const TImageReaderFFMov &);
  TImageReaderFFMov &operator=(const TImageReaderFFMov &src);
};

//===========================================================
//
//  TLevelReaderFFMov
//
//===========================================================

TLevelReaderFFMov::TLevelReaderFFMov(const TFilePath &path)
    : TLevelReader(path) {
  ffmpegReader = new Ffmpeg();
  ffmpegReader->setPath(m_path);
  ffmpegReader->disablePrecompute();
  ffmpegFileInfo tempInfo = ffmpegReader->getInfo();
  double fps              = tempInfo.m_frameRate;
  m_frameCount            = tempInfo.m_frameCount;
  m_size                  = TDimension(tempInfo.m_lx, tempInfo.m_ly);
  m_lx                    = m_size.lx;
  m_ly                    = m_size.ly;

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

TLevelReaderFFMov::~TLevelReaderFFMov() {}

//-----------------------------------------------------------

TLevelP TLevelReaderFFMov::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderFFMov::getFrameReader(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderFFMov *irm = new TImageReaderFFMov(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderFFMov::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderFFMov::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}

Tiio::FFMovWriterProperties::FFMovWriterProperties()
    : m_vidQuality("Quality", 1, 100, 90), m_scale("Scale", 1, 100, 100) {
  bind(m_vidQuality);
  bind(m_scale);
}

void Tiio::FFMovWriterProperties::updateTranslation() {
  m_vidQuality.setQStringName(tr("Quality"));
  m_scale.setQStringName(tr("Scale"));
}
