
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

  TImageWriterSprite(const TFilePath &path, int frameIndex,
                     TLevelWriterSprite *lwg)
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

TLevelWriterSprite::TLevelWriterSprite(const TFilePath &path,
                                       TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::SpriteWriterProperties();
  std::string scale = m_properties->getProperty("Scale")->getValueAsString();
  m_scale           = QString::fromStdString(scale).toInt();
  std::string topPadding =
      m_properties->getProperty("Top Padding")->getValueAsString();
  m_topPadding = QString::fromStdString(topPadding).toInt();
  std::string bottomPadding =
      m_properties->getProperty("Bottom Padding")->getValueAsString();
  m_bottomPadding = QString::fromStdString(bottomPadding).toInt();
  std::string leftPadding =
      m_properties->getProperty("Left Padding")->getValueAsString();
  m_leftPadding = QString::fromStdString(leftPadding).toInt();
  std::string rightPadding =
      m_properties->getProperty("Right Padding")->getValueAsString();
  m_rightPadding = QString::fromStdString(rightPadding).toInt();
  m_format       = QString::fromStdWString(
      ((TEnumProperty *)(m_properties->getProperty("Format")))->getValue());
  TBoolProperty *trim =
      (TBoolProperty *)m_properties->getProperty("Trim Empty Space");
  m_trim = trim->getValue();
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterSprite::~TLevelWriterSprite() {
  int finalWidth    = m_right - m_left + 1;
  int finalHeight   = m_bottom - m_top + 1;
  int resizedWidth  = finalWidth * m_scale / 100;
  int resizedHeight = finalHeight * m_scale / 100;
  for (QImage *image : m_images) {
    QImage copy = image->copy(m_left, m_top, finalWidth, finalHeight);
    if (m_scale != 100) {
      int width  = (copy.width() * m_scale) / 100;
      int height = (copy.height() * m_scale) / 100;
      copy       = copy.scaled(width, height);
    }
    m_imagesResized.push_back(copy);
  }
  for (QImage *image : m_images) {
    delete image;
  }
  m_images.clear();
  int horizDim          = 1;
  int vertDim           = 1;
  int vertPadding       = m_topPadding + m_bottomPadding;
  int horizPadding      = m_leftPadding + m_rightPadding;
  int totalVertPadding  = 0;
  int totalHorizPadding = 0;
  int spriteSheetWidth;
  int spriteSheetHeight;
  if (m_format == "Grid") {
    // Calculate Grid Size
    while (horizDim * horizDim < m_imagesResized.size()) horizDim++;
    totalHorizPadding = horizDim * horizPadding;
    spriteSheetWidth  = horizDim * resizedWidth + totalHorizPadding;
    vertDim           = horizDim;
    // Figure out if there is one row too many
    // (Such as 6 images needs 3 x 2 grid)
    if (vertDim * vertDim - vertDim >= m_imagesResized.size()) {
      vertDim = vertDim - 1;
    }
    totalVertPadding  = vertDim * vertPadding;
    spriteSheetHeight = vertDim * resizedHeight + totalVertPadding;
  } else if (m_format == "Vertical") {
    spriteSheetWidth  = resizedWidth + horizPadding;
    spriteSheetHeight = m_imagesResized.size() * (resizedHeight + vertPadding);
    vertDim           = m_imagesResized.size();
  } else if (m_format == "Horizontal") {
    spriteSheetWidth  = m_imagesResized.size() * (resizedWidth + horizPadding);
    spriteSheetHeight = resizedHeight + vertPadding;
    horizDim          = m_imagesResized.size();
  } else if (m_format == "Individual") {
    for (int i = 0; i < m_imagesResized.size(); i++) {
      QString path      = m_path.getQString();
      QString newEnding = "_" + QString::number(i) + ".png";
      path              = path.replace(".spritesheet", newEnding);
      m_imagesResized[i].save(path, "PNG", -1);
    }
  }
  if (m_format != "Individual") {
    QImage spriteSheet = QImage(spriteSheetWidth, spriteSheetHeight,
                                QImage::Format_ARGB32_Premultiplied);
    spriteSheet.fill(qRgba(0, 0, 0, 0));
    QPainter painter;
    painter.begin(&spriteSheet);
    int row    = 0;
    int column = 0;
    int rowPadding;
    int columnPadding;
    int currentImage = 0;
    while (row < vertDim) {
      while (column < horizDim) {
        rowPadding    = m_topPadding;
        columnPadding = m_leftPadding;
        rowPadding += row * vertPadding;
        columnPadding += column * horizPadding;
        painter.drawImage(column * resizedWidth + columnPadding,
                          row * resizedHeight + rowPadding,
                          m_imagesResized[currentImage]);
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
    path         = path.replace(".spritesheet", ".png");
    spriteSheet.save(path, "PNG", -1);

    path = path.replace(".png", ".txt");
    QFile file(path);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << "Total Images: " << m_imagesResized.size() << "\n";
    out << "Individual Image Width: " << resizedWidth << "\n";
    out << "Individual Image Height: " << resizedHeight << "\n";
    out << "Individual Image Width with Padding: "
        << resizedWidth + horizPadding << "\n";
    out << "Individual Image Height with Padding: "
        << resizedHeight + vertPadding << "\n";
    out << "Images Across: " << horizDim << "\n";
    out << "Images Down : " << vertDim << "\n";
    out << "Top Padding: " << m_topPadding << "\n";
    out << "Bottom Padding: " << m_bottomPadding << "\n";
    out << "Left Padding: " << m_leftPadding << "\n";
    out << "Right Padding: " << m_rightPadding << "\n";
    out << "Horizontal Space Between Images: " << horizPadding << "\n";
    out << "Vertical Space Between Images: " << vertPadding << "\n";

    file.close();
  }
  m_imagesResized.clear();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterSprite::getFrameWriter(TFrameId fid) {
  if (fid.getLetter() != 0) return TImageWriterP(0);
  int index               = fid.getNumber();
  TImageWriterSprite *iwg = new TImageWriterSprite(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterSprite::setFrameRate(double fps) {}

void TLevelWriterSprite::saveSoundTrack(TSoundTrack *st) {}

//-----------------------------------------------------------

void TLevelWriterSprite::save(const TImageP &img, int frameIndex) {
  m_frameIndexOrder.push_back(frameIndex);
  std::sort(m_frameIndexOrder.begin(), m_frameIndexOrder.end());
  TRasterImageP tempImage(img);
  TRasterImage *image = (TRasterImage *)tempImage->cloneImage();

  m_lx           = image->getRaster()->getLx();
  m_ly           = image->getRaster()->getLy();
  int m_bpp      = image->getRaster()->getPixelSize();
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
  QByteArray ba                = m_intermediateFormat.toUpper().toLatin1();
  const char *format           = ba.data();

  QImage *qi = new QImage((uint8_t *)buffer, m_lx, m_ly,
                          QImage::Format_ARGB32_Premultiplied);

  int l = qi->width(), r = 0, t = qi->height(), b = 0;
  if (m_trim) {
    for (int y = 0; y < qi->height(); ++y) {
      QRgb *row      = (QRgb *)qi->scanLine(y);
      bool rowFilled = false;
      for (int x = 0; x < qi->width(); ++x) {
        if (qAlpha(row[x])) {
          rowFilled = true;
          r         = std::max(r, x);
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
  } else {
    l = 0;
    r = qi->width() - 1;
    t = 0;
    b = qi->height() - 1;
  }
  if (m_firstPass) {
    m_firstPass = false;
    m_left      = l;
    m_right     = r;
    m_top       = t;
    m_bottom    = b;
  } else {
    if (l < m_left) m_left     = l;
    if (r > m_right) m_right   = r;
    if (t < m_top) m_top       = t;
    if (b > m_bottom) m_bottom = b;
  }
  QImage *newQi = new QImage(m_lx, m_ly, QImage::Format_ARGB32_Premultiplied);
  newQi->fill(qRgba(0, 0, 0, 0));
  QPainter painter(newQi);
  painter.drawImage(QPoint(0, 0), *qi);

  // Make sure to order the images according to their frame index
  // Not just what comes out first
  std::vector<int>::iterator it;
  it = find(m_frameIndexOrder.begin(), m_frameIndexOrder.end(), frameIndex);
  int pos = std::distance(m_frameIndexOrder.begin(), it);

  m_images.insert(m_images.begin() + pos, newQi);
  delete image;
  delete qi;
  free(buffer);
}

Tiio::SpriteWriterProperties::SpriteWriterProperties()
    : m_topPadding("Top Padding", 0, 100, 0)
    , m_bottomPadding("Bottom Padding", 0, 100, 0)
    , m_leftPadding("Left Padding", 0, 100, 0)
    , m_rightPadding("Right Padding", 0, 100, 0)
    , m_scale("Scale", 1, 100, 100)
    , m_format("Format")
    , m_trim("Trim Empty Space", true) {
  m_format.addValue(L"Grid");
  m_format.addValue(L"Vertical");
  m_format.addValue(L"Horizontal");
  m_format.addValue(L"Individual");
  m_format.setValue(L"Grid");
  bind(m_format);
  bind(m_topPadding);
  bind(m_bottomPadding);
  bind(m_leftPadding);
  bind(m_rightPadding);
  bind(m_scale);
  bind(m_trim);
}

void Tiio::SpriteWriterProperties::updateTranslation() {
  m_topPadding.setQStringName(tr("Top Padding"));
  m_bottomPadding.setQStringName(tr("Bottom Padding"));
  m_leftPadding.setQStringName(tr("Left Padding"));
  m_rightPadding.setQStringName(tr("Right Padding"));
  m_scale.setQStringName(tr("Scale"));
  m_format.setQStringName(tr("Format"));
  m_format.setItemUIName(L"Grid", tr("Grid"));
  m_format.setItemUIName(L"Vertical", tr("Vertical"));
  m_format.setItemUIName(L"Horizontal", tr("Horizontal"));
  m_format.setItemUIName(L"Individual", tr("Individual"));
  m_trim.setQStringName(tr("Trim Empty Space"));
}

// Tiio::Reader* Tiio::makeSpriteReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeSpriteWriter(){ return nullptr; }