
#include "tsystem.h"
#include "tiio_gif.h"
#include "trasterimage.h"
#include "timageinfo.h"
#include <QStringList>
#include <QPainter>
#include <QProcess>
#include <QDir>

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
  TBoolProperty *trim =
	  (TBoolProperty *)m_properties->getProperty("Trim Unused Space");
  m_trim = trim->getValue();
  TBoolProperty *transparent =
	  (TBoolProperty *)m_properties->getProperty("Transparent");
  m_transparent = transparent->getValue();
  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  m_frameCount = 0;
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterGif::~TLevelWriterGif() {
	if (m_trim) {
		m_lx = m_right - m_left;
		m_ly = m_bottom - m_top;
		int resizedWidth = m_lx * m_scale / 100;
		int resizedHeight = m_ly * m_scale / 100;
		int i = 1;
		for (QImage *image : m_images) {
			QImage copy = image->copy(m_left, m_top, m_lx, m_ly);
			QImage newCopy = copy;
			if (!m_transparent) {
				newCopy.fill(qRgba(255, 255, 255, 255));
				QPainter painter;
				painter.begin(&newCopy);
				painter.drawImage(0,0,copy);
				painter.end();
			}
			if (m_scale != 100) {
				int width = (newCopy.width() * m_scale) / 100;
				int height = (newCopy.height() * m_scale) / 100;
				newCopy = newCopy.scaled(width, height);
			}
			int w = copy.width();
			int h = copy.height();
			//m_imagesResized.push_back(copy);
			QString tempPath = m_path.getQString() + "tempOut" +
				QString::number(i) + "." + "png";
			newCopy.save(tempPath, "PNG", -1);
			ffmpegWriter->addToCleanUp(tempPath);
			i++;
			//ffmpegWriter->createIntermediateImage(img, frameIndex);
		}
		m_images.clear();
	}
	int fr = m_frameRate;
	if (m_transparent) {
		QStringList imargs;
		//imargs << "-delay";
		//imargs << "8";
		//imargs << QString::number(qRound(100 / m_frameRate));
		imargs << m_path.getQString() + "tempOut%d.png[1-" + QString::number(m_frameCount) + "]";
		imargs << m_path.getQString();
		QString path = QDir::currentPath() + "/convert";
#if defined(_WIN32)
		path = path + ".exe";
#endif
		QProcess imageMagick;
		imageMagick.start(path, imargs);
		imageMagick.waitForFinished(30000);
		QString results = imageMagick.readAllStandardError();
		results += imageMagick.readAllStandardOutput();
		int exitCode = imageMagick.exitCode();
		imageMagick.close();
		std::string strResults = results.toStdString();
	}
	else {
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
			palette = m_path.getQString() + "palette.png";
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
		}
		else {
			postIArgs << "-lavfi";
			postIArgs << filters;
		}

		if (!m_looping) {
			postIArgs << "-loop";
			postIArgs << "-1";
		}

		std::string outPath = m_path.getQString().toStdString();

		ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
	}
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
  
  if (m_trim) {
	  TRasterImageP tempImage(img);
	  TRasterImage *image = (TRasterImage *)tempImage->cloneImage();

	  m_lx = image->getRaster()->getLx();
	  m_ly = image->getRaster()->getLy();
	  int m_bpp = image->getRaster()->getPixelSize();
	  int totalBytes = m_lx * m_ly * m_bpp;
	  image->getRaster()->yMirror();

	  // lock raster to get data
	  image->getRaster()->lock();
	  void *buffin = image->getRaster()->getRawData();
	  assert(buffin);
	  void *buffer = malloc(totalBytes);
	  memcpy(buffer, buffin, totalBytes);

	  image->getRaster()->unlock();

	  // create QImage save format
	  //QString m_intermediateFormat = "png";
	  //QByteArray ba = m_intermediateFormat.toUpper().toLatin1();
	  //const char *format = ba.data();

	  QImage *qi = new QImage((uint8_t *)buffer, m_lx, m_ly, QImage::Format_ARGB32);
	  m_images.push_back(qi);
	  delete image;

	  int l = qi->width(), r = 0, t = qi->height(), b = 0;
	  for (int y = 0; y < qi->height(); ++y) {
		  QRgb *row = (QRgb *)qi->scanLine(y);
		  bool rowFilled = false;
		  for (int x = 0; x < qi->width(); ++x) {
			  if (qAlpha(row[x])) {
				  rowFilled = true;
				  r = std::max(r, x);
				  if (l > x) {
					  l = x;
					  x = r;
				  }
			  }
		  }
		  if (rowFilled) {
			  t = std::min(t, y);
			  b = y;
		  }
	  }
	  if (m_firstPass) {
		  m_firstPass = false;
		  m_left = l;
		  m_right = r;
		  m_top = t;
		  m_bottom = b;
	  }
	  else {
		  if (l < m_left) m_left = l;
		  if (r > m_right) m_right = r;
		  if (t < m_top) m_top = t;
		  if (b > m_bottom) m_bottom = b;
	  }
  }
  else {
	  ffmpegWriter->createIntermediateImage(img, frameIndex);
  }
  m_frameCount++;
}

//===========================================================
//
//  TImageReaderGif
//
//===========================================================

class TImageReaderGif final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderGif(const TFilePath &path, int index, TLevelReaderGif *lra)
      : TImageReader(path), m_lra(lra), m_frameIndex(index) {
    m_lra->addRef();
  }
  ~TImageReaderGif() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }

private:
  TLevelReaderGif *m_lra;

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

  // set values
  m_info                   = new TImageInfo();
  m_info->m_frameRate      = fps;
  m_info->m_lx             = m_lx;
  m_info->m_ly             = m_ly;
  m_info->m_bitsPerSample  = 8;
  m_info->m_samplePerPixel = 4;
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
  int index = fid.getNumber();

  TImageReaderGif *irm = new TImageReaderGif(m_path, index, this);
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
    , m_palette("Generate Palette", true)
	, m_trim("Trim Unused Space", false)
	, m_transparent("Transparent", false) {
  bind(m_scale);
  bind(m_looping);
  bind(m_palette);
  bind(m_trim);
  bind(m_transparent);
}

// Tiio::Reader* Tiio::makeGifReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeGifWriter(){ return nullptr; }