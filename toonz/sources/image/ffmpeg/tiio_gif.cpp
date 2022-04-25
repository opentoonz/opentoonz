
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
  TEnumProperty *modeProp =
      dynamic_cast<TEnumProperty *>(m_properties->getProperty("Mode"));
  m_mode = 0;
  if (modeProp) {
    m_mode = modeProp->getIndex();
  }
  std::string maxcolors =
      m_properties->getProperty("Max Colors")->getValueAsString();
  m_maxcolors = QString::fromStdString(maxcolors).toInt();

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
  /*
  // ffmpeg doesn't like resolutions that aren't divisible by 2.
  if (outLx % 2 != 0) outLx++;
  if (outLy % 2 != 0) outLy++;
  */

  double framerate = (m_frameRate < 1.0 ? 1.0 : m_frameRate);

  QString filters = "fps=" + QString::number(framerate) +
                    ",scale=" + QString::number(outLx) + ":" +
                    QString::number(outLy) + ":flags=lanczos";

  // 1 = "dither=sierra2_4a" is default
  const char *ditherConsts[4] = {"none", "sierra2_4a", "bayer:bayer_scale=2",
                                 "bayer:bayer_scale=1"};

  // Please be careful when moving items, logic AND 3 requires alignment
  switch (m_mode) {
  case 0:
  case 1:
  case 2:
  case 3:
    filters += ",split[o1][o2];[o1]palettegen";  // "stats_mode=full" is default
    if (m_maxcolors != 256) {
      filters += "=max_colors=" + QString::number(m_maxcolors);
    }
    filters += "[p];[o2]fifo[o3];[o3][p]paletteuse";
    if ((m_mode & 3) != 1) {
      filters += "=dither=" + QString(ditherConsts[m_mode & 3]);
    }
    break;
  case 4:
  case 5:
  case 6:
  case 7:
    filters += ",split[o1][o2];[o1]palettegen=stats_mode=diff";
    if (m_maxcolors != 256) {
      filters += ":max_colors=" + QString::number(m_maxcolors);
    }
    filters += "[p];[o2]fifo[o3];[o3][p]paletteuse";
    if ((m_mode & 3) != 1) {
      filters += "=dither=" + QString(ditherConsts[m_mode & 3]);
    }
    break;
  case 8:
  case 9:
  case 10:
  case 11:
    filters += ",split[o1][o2];[o1]palettegen=stats_mode=single";
    if (m_maxcolors != 256) {
      filters += ":max_colors=" + QString::number(m_maxcolors);
    }
    filters += "[p];[o2]fifo[o3];[o3][p]paletteuse=new=1";
    if ((m_mode & 3) != 1) {
      filters += ":dither=" + QString(ditherConsts[m_mode & 3]);
    }
    break;
  default:
    break;
  }

  preIArgs << "-r";
  preIArgs << QString::number(framerate);
  preIArgs << "-v";
  preIArgs << "warning";
  postIArgs << "-vf";
  postIArgs << filters;
  postIArgs << "-gifflags";
  postIArgs << "0";

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
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);
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
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index            = fid.getNumber();
  TImageReaderGif *irm = new TImageReaderGif(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderGif::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderGif::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}

Tiio::GifWriterProperties::GifWriterProperties()
    : m_scale("Scale", 1, 100, 100)
    , m_looping("Looping", true)
    , m_palette("Generate Palette", true)
    , m_mode("Mode")
    , m_maxcolors("Max Colors", 2, 256, 256) {

  // Set values for mode
  m_mode.addValue(L"GLOBAL0");
  m_mode.addValue(L"GLOBAL1");
  m_mode.addValue(L"GLOBAL2");
  m_mode.addValue(L"GLOBAL3");
  m_mode.addValue(L"DIFF0");
  m_mode.addValue(L"DIFF1");
  m_mode.addValue(L"DIFF2");
  m_mode.addValue(L"DIFF3");
  m_mode.addValue(L"NEW0");
  m_mode.addValue(L"NEW1");
  m_mode.addValue(L"NEW2");
  m_mode.addValue(L"NEW3");
  m_mode.addValue(L"NOPAL");

  // Translate values
  m_mode.setItemUIName(L"GLOBAL0", tr("Global Palette"));
  m_mode.setItemUIName(L"GLOBAL1", tr("Global Palette + Sierra Dither"));
  m_mode.setItemUIName(L"GLOBAL2", tr("Global Palette + Bayer2 Dither"));
  m_mode.setItemUIName(L"GLOBAL3", tr("Global Palette + Bayer1 Dither"));
  m_mode.setItemUIName(L"DIFF0", tr("Diff Palette"));
  m_mode.setItemUIName(L"DIFF1", tr("Diff Palette + Sierra Dither"));
  m_mode.setItemUIName(L"DIFF2", tr("Diff Palette + Bayer2 Dither"));
  m_mode.setItemUIName(L"DIFF3", tr("Diff Palette + Bayer1 Dither"));
  m_mode.setItemUIName(L"NEW0", tr("New Pal Per Frame"));
  m_mode.setItemUIName(L"NEW1", tr("New Pal Per Frame + Sierra Dither"));
  m_mode.setItemUIName(L"NEW2", tr("New Pal Per Frame + Bayer2 Dither"));
  m_mode.setItemUIName(L"NEW3", tr("New Pal Per Frame + Bayer1 Dither"));
  m_mode.setItemUIName(L"NOPAL", tr("Opaque, Dither, 256 Colors Only"));

  // Doesn't make Generate Palette property visible in the popup
  m_palette.setVisible(false);

  bind(m_scale);
  bind(m_looping);
  bind(m_palette);
  bind(m_mode);
  bind(m_maxcolors);
}

void Tiio::GifWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_looping.setQStringName(tr("Looping"));
  m_mode.setQStringName(tr("Mode"));
  m_maxcolors.setQStringName(tr("Max Colors"));
}

// Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }