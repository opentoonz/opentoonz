
#include "tiio_apng.h"
#include "tsystem.h"
#include "trasterimage.h"
#include "tsound.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include <QStringList>

//===========================================================
//
//  TImageWriterAPng
//
//===========================================================

class TImageWriterAPng : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterAPng(const TFilePath &path, int frameIndex, TLevelWriterAPng *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterAPng() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterAPng *m_lwg;
};

//===========================================================
//
//  TLevelWriterAPng;
//
//===========================================================

TLevelWriterAPng::TLevelWriterAPng(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::APngWriterProperties();

  std::string scale = m_properties->getProperty("Scale")->getValueAsString();
  m_scale           = QString::fromStdString(scale).toInt();

  TBoolProperty *extPng = (TBoolProperty *)m_properties->getProperty("ExtPng");
  m_extPng              = extPng->getValue();

  TBoolProperty *loop = (TBoolProperty *)m_properties->getProperty("Looping");
  m_looping           = loop->getValue();

  if (m_extPng) {
    m_path = m_path.getParentDir() + TFilePath(m_path.getWideName() + L".png");
  }

  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterAPng::~TLevelWriterAPng() {
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

  preIArgs << "-framerate";
  preIArgs << QString::number(m_frameRate);
  postIArgs << "-plays";
  postIArgs << (m_looping ? "0" : "1");
  postIArgs << "-f";
  postIArgs << "apng";
  postIArgs << "-s";
  postIArgs << QString::number(outLx) + "x" + QString::number(outLy);

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterAPng::getFrameWriter(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);
  int index             = fid.getNumber();
  TImageWriterAPng *iwg = new TImageWriterAPng(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterAPng::setFrameRate(double fps) {
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterAPng::saveSoundTrack(TSoundTrack *st) {
  ffmpegWriter->saveSoundTrack(st);
}

//-----------------------------------------------------------

void TLevelWriterAPng::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderAPng
//
//===========================================================

class TImageReaderAPng final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderAPng(const TFilePath &path, int index, TLevelReaderAPng *lra,
                   TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderAPng() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderAPng *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderAPng(const TImageReaderAPng &);
  TImageReaderAPng &operator=(const TImageReaderAPng &src);
};

//===========================================================
//
//  TLevelReaderAPng
//
//===========================================================

TLevelReaderAPng::TLevelReaderAPng(const TFilePath &path) : TLevelReader(path) {
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

TLevelReaderAPng::~TLevelReaderAPng() {}

//-----------------------------------------------------------

TLevelP TLevelReaderAPng::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderAPng::getFrameReader(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderAPng *irm = new TImageReaderAPng(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderAPng::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderAPng::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}

Tiio::APngWriterProperties::APngWriterProperties()
    : m_scale("Scale", 1, 100, 100)
    , m_looping("Looping", true)
    , m_extPng("ExtPng", false) {
  bind(m_scale);
  bind(m_looping);
  bind(m_extPng);
}

void Tiio::APngWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_looping.setQStringName(tr("Looping"));
  m_extPng.setQStringName(tr("Write as .png"));
}
