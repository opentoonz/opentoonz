
#include "tiio_webm.h"
#include "tsystem.h"
#include "trasterimage.h"
#include "tsound.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include <QStringList>
#include <QDebug>

//===========================================================
//
//  TImageWriterWebm
//
//===========================================================

class TImageWriterWebm : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterWebm(const TFilePath &path, int frameIndex, TLevelWriterWebm *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterWebm() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterWebm *m_lwg;
};

//===========================================================
//
//  TLevelWriterWebm;
//
//===========================================================

TLevelWriterWebm::TLevelWriterWebm(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo), m_lx(0), m_ly(0) {
  if (!m_properties) m_properties = new Tiio::WebmWriterProperties();

  m_scale = QString::fromStdString(
                m_properties->getProperty("Scale")->getValueAsString())
                .toInt();
  m_speed = QString::fromStdString(
      m_properties->getProperty("Encoding Speed")->getValueAsString());
  m_preserveAlpha =
      QString::fromStdString(
          m_properties->getProperty("Preserve Alpha")->getValueAsString())
          .toInt();
  m_lossless = QString::fromStdString(
                   m_properties->getProperty("Lossless")->getValueAsString())
                   .toInt();
  m_kfSetting =
      QString::fromStdString(
          m_properties->getProperty("Keyframe Interval")->getValueAsString())
          .toInt();

  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterWebm::~TLevelWriterWebm() {
  QStringList preIArgs;
  QStringList postIArgs;

  // Calculate output dimensions (ensure even)
  int outLx = m_lx;
  int outLy = m_ly;
  if (m_scale != 0) {
    outLx = m_lx * m_scale / 100;
    outLy = m_ly * m_scale / 100;
  }
  outLx += (outLx % 2);  // Add 1 if odd
  outLy += (outLy % 2);

  // Input settings
  preIArgs << "-framerate" << QString::number(m_frameRate);

  // Codec selection (VP9 only)
  postIArgs << "-c:v" << "libvpx-vp9";

  postIArgs << "-threads" << "auto";     // Auto-detect CPU threads
  postIArgs << "-row-mt" << "1";         // Row-based multi-threading
  postIArgs << "-static-thresh" << "0";  // Optimize for sharp animation content
  postIArgs << "-lag-in-frames" << "24";  // Lookahead

  // Dynamically calculate CRF based on pixel count
  auto getAutoCRF = [](int width, int height) -> int {
    int pixelCount = width * height;

    return (pixelCount >= 3840 * 2160) ? 16 :  // 4K (2160p)
               (pixelCount >= 2560 * 1440) ? 18
                                           :  // 1440p
               (pixelCount >= 1920 * 1080) ? 20
                                           :  // 1080p (default)
               (pixelCount >= 1280 * 720) ? 22
                                          :  // 720p
               24;                           // SD (480p and lower)
  };
  int crf = getAutoCRF(m_lx, m_ly);

  if (m_lossless) {
    postIArgs << "-lossless" << "1";
    postIArgs << "-tune" << "ssim";
  } else {
    postIArgs << "-b:v" << "0";
    postIArgs << "-crf" << QString::number(crf);
    postIArgs << "-tune" << "psnr";
  }

  // Disable alt refs for image sequences
  postIArgs << "-auto-alt-ref" << "0";

  // Calculate keyframe interval
  int kfInterval;
  switch (m_kfSetting) {
  case 1:  // Every Frame
    kfInterval = 1;
    break;
  case 2:  // Every Second
    kfInterval = qRound(m_frameRate);
    break;
  case 3:  // Every 2 Seconds
    kfInterval = qRound(m_frameRate * 2);
    break;
  case 4:  // Every 5 Seconds
    kfInterval = qRound(m_frameRate * 5);
    break;
  case 5:  // Every 10 Seconds
    kfInterval = qRound(m_frameRate * 10);
    break;
  default:  // Default to Every Second
    kfInterval = qRound(m_frameRate);
    break;
  }
  postIArgs << "-g" << QString::number(kfInterval);

  // Encoding speed
  postIArgs << "-cpu-used" << m_speed;

  // Colorspace
  postIArgs << "-color_primaries" << "bt709"
            << "-color_trc" << "bt709"
            << "-colorspace" << "bt709";

  // Pixel format
  postIArgs << "-pix_fmt" << (m_preserveAlpha ? "yuva420p" : "yuv420p");

  // Scale
  postIArgs << "-s" << QString("%1x%2").arg(outLx).arg(outLy);

  // Debug
  qDebug() << "preIArgs:" << preIArgs << "postIArgs:" << postIArgs;

  // Execute FFmpeg and clean up
  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterWebm::getFrameWriter(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildGifExceptionString(IOError));
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);
  int index             = fid.getNumber();
  TImageWriterWebm *iwg = new TImageWriterWebm(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterWebm::setFrameRate(double fps) {
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterWebm::saveSoundTrack(TSoundTrack *st) {
  ffmpegWriter->saveSoundTrack(st);
}

//-----------------------------------------------------------

void TLevelWriterWebm::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderWebm
//
//===========================================================

class TImageReaderWebm final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderWebm(const TFilePath &path, int index, TLevelReaderWebm *lra,
                   TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderWebm() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderWebm *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderWebm(const TImageReaderWebm &);
  TImageReaderWebm &operator=(const TImageReaderWebm &src);
};

//===========================================================
//
//  TLevelReaderWebm
//
//===========================================================

TLevelReaderWebm::TLevelReaderWebm(const TFilePath &path) : TLevelReader(path) {
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

TLevelReaderWebm::~TLevelReaderWebm() {
  // ffmpegReader->cleanUpFiles();
}

//-----------------------------------------------------------

TLevelP TLevelReaderWebm::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderWebm::getFrameReader(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderWebm *irm = new TImageReaderWebm(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderWebm::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderWebm::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}

Tiio::WebmWriterProperties::WebmWriterProperties()
    : m_scale("Scale", 1, 100, 100)
    , m_speed("Encoding Speed")
    , m_kf("Keyframe Interval")
    , m_preserveAlpha("Preserve Alpha", true)
    , m_lossless("Lossless", false) {
  bind(m_scale);
  bind(m_speed);
  bind(m_kf);
  bind(m_preserveAlpha);
  bind(m_lossless);

  // Encoding Speeds
  m_speed.addValueWithUIName(L"0",
                             tr("Placebo (Smallest File)"));  // cpu-used 0
  m_speed.addValueWithUIName(L"1", tr("Very Slow"));          // cpu-used 1
  m_speed.addValueWithUIName(L"2", tr("Slower"));             // cpu-used 2
  m_speed.addValueWithUIName(L"3", tr("Slow"));               // cpu-used 3
  m_speed.addValueWithUIName(L"4", tr("Medium"));             // cpu-used 4
  m_speed.addValueWithUIName(L"5", tr("Fast"));               // cpu-used 5
  m_speed.addValueWithUIName(L"6", tr("Faster"));             // cpu-used 6
  m_speed.addValueWithUIName(L"7", tr("Very Fast"));          // cpu-used 7
  m_speed.addValueWithUIName(L"8",
                             tr("Ultra Fast (Largest File)"));  // cpu-used 8
  m_speed.setValue(L"1");  // Default to Very Slow (benefits animation)

  // Keyframe Intervals
  m_kf.addValueWithUIName(L"1", tr("Every Frame"));
  m_kf.addValueWithUIName(L"2", tr("Every Second"));
  m_kf.addValueWithUIName(L"3", tr("Every 2 Seconds"));
  m_kf.addValueWithUIName(L"4", tr("Every 5 Seconds"));
  m_kf.addValueWithUIName(L"5", tr("Every 10 Seconds"));
  m_kf.setValue(L"2");  // Default to Every Second
}

void Tiio::WebmWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_speed.setQStringName(tr("Encoding Speed"));
  m_kf.setQStringName(tr("Keyframe Interval"));
  m_preserveAlpha.setQStringName(tr("Preserve Alpha"));
  m_lossless.setQStringName(tr("Lossless"));
}

// Tiio::Reader* Tiio::makeWebmReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeWebmWriter(){ return nullptr; }