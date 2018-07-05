
#include "tsystem.h"
#include "tiio_gif.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include <QStringList>

//===========================================================
//
//  TImageWriterGif
//
//===========================================================

class TImageWriterGif : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterGif(const TFilePath &path, int frameIndex, TLevelWriterGif *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterGif() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterGif *m_lwg;
};

//===========================================================
//
//  TLevelWriterGif;
//
//===========================================================

TLevelWriterGif::TLevelWriterGif(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::GifWriterProperties();
  std::string scale = m_properties->getProperty("Scale")->getValueAsString();
  m_scale           = QString::fromStdString(scale).toInt();
  TBoolProperty *looping =
      (TBoolProperty *)m_properties->getProperty("Looping");
  m_looping = looping->getValue();
  TBoolProperty *palette =
      (TBoolProperty *)m_properties->getProperty("Generate Palette");
  m_palette    = palette->getValue();
  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  // m_frameCount = 0;
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterGif::~TLevelWriterGif() {
  QStringList preIArgs;
  QStringList postIArgs;
  QStringList palettePreIArgs;
  QStringList palettePostIArgs;

  int outLx = m_lx;
  int outLy = m_ly;

  // set scaling
  outLx = m_lx * m_scale / 100;
  outLy = m_ly * m_scale / 100;
  // ffmpeg doesn't like resolutions that aren't divisible by 2.
  if (outLx % 2 != 0) outLx++;
  if (outLy % 2 != 0) outLy++;

  QString palette;
  QString filters = "scale=" + QString::number(outLx) + ":-1:flags=lanczos";
  QString paletteFilters = filters + " [x]; [x][1:v] paletteuse";
  if (m_palette) {
    palette = ffmpegWriter->getFfmpegCache().getQString() + "//" +
              QString::fromStdString(m_path.getName()) + "palette.png";
    palettePreIArgs << "-v";
    palettePreIArgs << "warning";

    palettePostIArgs << "-vf";
    palettePostIArgs << filters + ",palettegen";
    palettePostIArgs << palette;

    // write the palette
    ffmpegWriter->runFfmpeg(palettePreIArgs, palettePostIArgs, false, true,
                            true);
    ffmpegWriter->addToCleanUp(palette);
  }

  preIArgs << "-v";
  preIArgs << "warning";
  preIArgs << "-r";
  preIArgs << QString::number(m_frameRate);
  if (m_palette) {
    postIArgs << "-i";
    postIArgs << palette;
    postIArgs << "-lavfi";
    postIArgs << paletteFilters;
  } else {
    postIArgs << "-lavfi";
    postIArgs << filters;
  }

  if (!m_looping) {
    postIArgs << "-loop";
    postIArgs << "-1";
  }

  std::string outPath = m_path.getQString().toStdString();

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterGif::getFrameWriter(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildGifExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index            = fid.getNumber();
  TImageWriterGif *iwg = new TImageWriterGif(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterGif::setFrameRate(double fps) {
  // m_fps = fps;
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterGif::saveSoundTrack(TSoundTrack *st) { return; }

//-----------------------------------------------------------

void TLevelWriterGif::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderGif
//
//===========================================================

class TImageReaderGif final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderGif(const TFilePath &path, int index, TLevelReaderGif *lra,
                  TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderGif() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderGif *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderGif(const TImageReaderGif &);
  TImageReaderGif &operator=(const TImageReaderGif &src);
};

//===========================================================
//
//  TLevelReaderGif
//
//===========================================================

TLevelReaderGif::TLevelReaderGif(const TFilePath &path)
    : TLevelReader(path)

{
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
  m_frameCount = ffmpegReader->getGifFrameCount();
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

TLevelReaderGif::~TLevelReaderGif() {
  // ffmpegReader->cleanUpFiles();
}

//-----------------------------------------------------------

TLevelP TLevelReaderGif::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderGif::getFrameReader(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (fid.getLetter() != 0) return TImageReaderP(0);
  int index            = fid.getNumber();
  TImageReaderGif *irm = new TImageReaderGif(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderGif::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderGif::load(int frameIndex) {
  return ffmpegReader->getImage(frameIndex);
}

Tiio::GifWriterProperties::GifWriterProperties()
    : m_scale("Scale", 1, 100, 100)
    , m_looping("Looping", true)
    , m_palette("Generate Palette", true) {
  bind(m_scale);
  bind(m_looping);
  bind(m_palette);
}

void Tiio::GifWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_looping.setQStringName(tr("Looping"));
  m_palette.setQStringName(tr("Generate Palette"));
}

// Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }