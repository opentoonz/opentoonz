
#include "tiio_sprite.h"
#include "../toonz/tapp.h"
#include "tsystem.h"
#include "tsound.h"

#include <QProcess>
#include <QDir>
#include <QtGui/QImage>
#include <QStringList>
#include <QPainter>
#include <QTextStream>
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"

#include "trasterimage.h"
#include "timageinfo.h"

//===========================================================
//
//  TImageWriterSprite
//
//===========================================================

class TImageWriterSprite : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterSprite(const TFilePath &path, int frameIndex, TLevelWriterSprite *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterSprite() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterSprite *m_lwg;
};

//===========================================================
//
//  TLevelWriterSprite;
//
//===========================================================

TLevelWriterSprite::TLevelWriterSprite(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::SpriteWriterProperties();
  std::string scale = m_properties->getProperty("Scale")->getValueAsString();
  m_scale           = QString::fromStdString(scale).toInt();
  std::string padding =
      m_properties->getProperty("Image Padding")->getValueAsString();
  m_padding = QString::fromStdString(padding).toInt();
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterSprite::~TLevelWriterSprite() {
	int finalWidth = m_right - m_left;
	int finalHeight = m_bottom - m_top;
	int resizedWidth = finalWidth * m_scale / 100;
	int resizedHeight = finalHeight * m_scale / 100;
	for (QImage *image : m_images) {
		QImage copy = image->copy(m_left, m_top, m_right, m_bottom);
		if (m_scale != 100) {
			int width = (copy.width() * m_scale) / 100;
			int height = (copy.height() * m_scale) / 100;
			copy = copy.scaled(width, height);
		}
		m_imagesResized.push_back(copy);
	}
	m_images.clear();
	int dim = 1; 
	while (dim * dim < m_imagesResized.size()) dim++;
	int totalPadding = dim * m_padding * 2;
	int spriteSheetWidth = dim * resizedWidth + totalPadding;
	int dim2 = dim;
	int totalPadding2 = totalPadding;
	if (dim * dim - dim >= m_imagesResized.size()) {
		dim2 = dim - 1;
		totalPadding2 = dim2 * m_padding * 2;
	}
	int spriteSheetHeight = dim2 * resizedHeight + totalPadding2;
	QImage spriteSheet = QImage(spriteSheetWidth, spriteSheetHeight, QImage::Format_ARGB32);
	spriteSheet.fill(qRgba(0, 0, 0, 0));
	QPainter painter;
	painter.begin(&spriteSheet);
	int row = 0;
	int column = 0;
	int rowPadding;
	int columnPadding;
	int currentImage = 0;
	while (row < dim) {
		while (column < dim) {
			rowPadding = m_padding;
			columnPadding = m_padding;
			rowPadding += row * (m_padding * 2);
			columnPadding += column * (m_padding * 2);
			painter.drawImage(column * resizedWidth + columnPadding, row * resizedHeight + rowPadding, m_imagesResized[currentImage]);
			currentImage++;
			column++;
			if (currentImage >= m_imagesResized.size()) break;
		}
		column = 0;
		row++;
		if (currentImage >= m_imagesResized.size()) break;
	}
	painter.end();
	QString path = m_path.getQString();
	path = path.replace(".spritesheet", ".png");
	spriteSheet.save(path, "PNG", -1);
	
	path = path.replace(".png", ".txt");
	QFile file(path);
	file.open(QIODevice::WriteOnly | QIODevice::Text);
	QTextStream out(&file);
	out << "Total Pictures: " << m_imagesResized.size() << "\n";
	out << "Width: " << resizedWidth + m_padding * 2 << "\nHeight: " << resizedHeight + m_padding * 2 << "\n";
	// optional, as QFile destructor will already do it:
	file.close();
	m_imagesResized.clear();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterSprite::getFrameWriter(TFrameId fid) {
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index            = fid.getNumber();
  TImageWriterSprite *iwg = new TImageWriterSprite(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterSprite::setFrameRate(double fps) {
}

void TLevelWriterSprite::saveSoundTrack(TSoundTrack *st) {
}

//-----------------------------------------------------------

void TLevelWriterSprite::save(const TImageP &img, int frameIndex) {
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
  QString m_intermediateFormat = "png";
  QByteArray ba = m_intermediateFormat.toUpper().toLatin1();
  const char *format = ba.data();

  QImage *qi = new QImage((uint8_t *)buffer, m_lx, m_ly, QImage::Format_ARGB32);
  m_images.push_back(qi);
  delete image;

  int l = qi->width(), r = 0, t = qi->height(), b = 0;
  for (int y = 0; y < qi->height(); ++y) {
	  QRgb *row = (QRgb*)qi->scanLine(y);
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
  } else {
	  if (l < m_left) m_left = l;
	  if (r > m_right) m_right = r;
	  if (t < m_top) m_top = t;
	  if (b > m_bottom) m_bottom = b;
  }
}

Tiio::SpriteWriterProperties::SpriteWriterProperties()
    : m_padding("Image Padding", 0, 100, 1), m_scale("Scale", 1, 100, 100) {
  bind(m_padding);
  bind(m_scale);
}

// Tiio::Reader* Tiio::makeSpriteReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeSpriteWriter(){ return nullptr; }