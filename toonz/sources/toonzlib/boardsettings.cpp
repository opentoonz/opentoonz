#include "toonz/boardsettings.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzfolders.h"
#include "toutputproperties.h"
#include "tsystem.h"

#include <QImage>
#include <QDate>
#include <QDateTime>
#include <QFontMetricsF>
#include <QMap>

namespace {
QMap<BoardItem::Type, std::wstring> strs = {
    {BoardItem::FreeText, L"FreeText"},
    {BoardItem::ProjectName, L"ProjectName"},
    {BoardItem::SceneName, L"SceneName"},
    {BoardItem::Duration_Frame, L"Duration_Frame"},
    {BoardItem::Duration_SecFrame, L"Duration_SecFrame"},
    {BoardItem::Duration_HHMMSSFF, L"Duration_HHMMSSFF"},
    {BoardItem::CurrentDate, L"CurrentDate"},
    {BoardItem::CurrentDateTime, L"CurrentDateTime"},
    {BoardItem::UserName, L"UserName"},
    {BoardItem::ScenePath_Aliased, L"ScenePath_Aliased"},
    {BoardItem::ScenePath_Full, L"ScenePath_Full"},
    {BoardItem::MoviePath_Aliased, L"MoviePath_Aliased"},
    {BoardItem::MoviePath_Full, L"MoviePath_Full"},
    {BoardItem::Image, L"Image"}};

std::wstring type2String(BoardItem::Type type) { return strs.value(type, L""); }
BoardItem::Type string2Type(std::wstring str) {
  return strs.key(str, BoardItem::TypeCount);
}
};

BoardItem::BoardItem() {
  m_name            = "Item";
  m_type            = ProjectName;
  m_rect            = QRectF(0.1, 0.1, 0.8, 0.8);
  m_maximumFontSize = 300;
  m_color           = Qt::black;
}

QString BoardItem::getContentText(ToonzScene *scene) {
  switch (m_type) {
  case FreeText:
    return m_text;
    break;
  case ProjectName:
    return scene->getProject()->getName().getQString();
    break;
  case SceneName:
    return QString::fromStdWString(scene->getSceneName());
    break;
  case Duration_Frame:
    return QString::number(scene->getFrameCount());
    break;
  case Duration_SecFrame: {
    TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
    int fps                  = (int)oprop->getFrameRate();
    int frame                = scene->getFrameCount();
    return QString("%1 + %2").arg(QString::number(frame / fps),
                                  QString::number(frame % fps));
  } break;
  case Duration_HHMMSSFF: {
    TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
    int fps                  = (int)oprop->getFrameRate();
    int frame                = scene->getFrameCount();
    int hh                   = frame / (fps * 60 * 60);
    frame -= hh * fps * 60 * 60;
    int mm = frame / (fps * 60);
    frame -= mm * fps * 60;
    int ss = frame / fps;
    int ff = frame % fps;
    return QString::number(hh).rightJustified(2, '0') + ":" +
           QString::number(mm).rightJustified(2, '0') + ":" +
           QString::number(ss).rightJustified(2, '0') + ":" +
           QString::number(ff).rightJustified(2, '0');
  } break;
  case CurrentDate:
    return QDate::currentDate().toString(Qt::DefaultLocaleLongDate);
    break;
  case CurrentDateTime:
    return QDateTime::currentDateTime().toString(Qt::DefaultLocaleLongDate);
    break;
  case UserName:
    return TSystem::getUserName();
    break;
  case ScenePath_Aliased:
    return scene->codeFilePath(scene->getScenePath()).getQString();
    break;
  case ScenePath_Full:
    return scene->decodeFilePath(scene->getScenePath()).getQString();
    break;
  case MoviePath_Aliased: {
    TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
    return scene->codeFilePath(oprop->getPath()).getQString();
  } break;
  case MoviePath_Full: {
    TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
    return scene->decodeFilePath(oprop->getPath()).getQString();
  } break;
  }
  return QString();
}

QRectF BoardItem::getItemRect(QSize imgSize) {
  QSizeF imgSizeF(imgSize);
  return QRectF(
      imgSizeF.width() * m_rect.left(), imgSizeF.height() * m_rect.top(),
      imgSizeF.width() * m_rect.width(), imgSizeF.height() * m_rect.height());
}

void BoardItem::drawItem(QPainter &p, QSize imgSize, int shrink,
                         ToonzScene *scene) {
  QRectF itemRect = getItemRect(imgSize);

  if (m_type == Image) {
    if (m_imgPath.isEmpty()) return;
    TFilePath decodedPath = scene->decodeFilePath(m_imgPath);
    QImage img(decodedPath.getQString());
    if (m_imgARMode == Qt::KeepAspectRatio) {
      float ratio = std::min((float)itemRect.width() / (float)img.width(),
                             (float)itemRect.height() / (float)img.height());
      QSizeF imgSize((float)img.width() * ratio, (float)img.height() * ratio);
      QPointF imgTopLeft =
          itemRect.topLeft() +
          QPointF((itemRect.width() - imgSize.width()) * 0.5f,
                  (itemRect.height() - imgSize.height()) * 0.5f);

      p.drawImage(QRectF(imgTopLeft, imgSize), img);
    } else if (m_imgARMode == Qt::IgnoreAspectRatio)
      p.drawImage(itemRect, img);
    return;
  }

  QString contentText = getContentText(scene);

  QFont tmpFont(m_font);
  tmpFont.setPixelSize(100);
  QFontMetricsF tmpFm(tmpFont);
  QRectF tmpBounding =
      tmpFm.boundingRect(itemRect, Qt::AlignLeft | Qt::AlignTop, contentText);

  float ratio = std::min(itemRect.width() / tmpBounding.width(),
                         itemRect.height() / tmpBounding.height());

  // compute the font size which will just fit the item region
  int fontSize = (int)(100.0f * ratio);
  tmpFont.setPixelSize(fontSize);
  tmpFm = QFontMetricsF(tmpFont);
  tmpBounding =
      tmpFm.boundingRect(itemRect, Qt::AlignLeft | Qt::AlignTop, contentText);
  bool isInRect;
  if (itemRect.width() >= tmpBounding.width() &&
      itemRect.height() >= tmpBounding.height())
    isInRect = true;
  else
    isInRect = false;
  while (1) {
    fontSize += (isInRect) ? 1 : -1;
    if (fontSize <= 0)  // cannot draw
      return;
    tmpFont.setPixelSize(fontSize);
    tmpFm = QFontMetricsF(tmpFont);
    tmpBounding =
        tmpFm.boundingRect(itemRect, Qt::AlignLeft | Qt::AlignTop, contentText);

    bool newIsInRect = (itemRect.width() >= tmpBounding.width() &&
                        itemRect.height() >= tmpBounding.height());
    if (isInRect != newIsInRect) {
      if (isInRect) fontSize--;
      break;
    }
  }

  //----
  fontSize = std::min(fontSize, m_maximumFontSize / shrink);

  QFont font(m_font);
  font.setPixelSize(fontSize);

  p.setFont(font);
  p.setPen(m_color);

  if (m_type == FreeText)
    p.drawText(itemRect, Qt::AlignLeft | Qt::AlignTop, contentText);
  else
    p.drawText(itemRect, Qt::AlignCenter, contentText);
}

void BoardItem::saveData(TOStream &os) {
  os.child("type") << type2String(m_type);
  os.child("name") << m_name;
  os.child("rect") << m_rect.x() << m_rect.y() << m_rect.width()
                   << m_rect.height();

  if (m_type == Image) {
    // if the path is in library folder, then save the realtive path
    TFilePath libFp = ToonzFolder::getLibraryFolder();
    if (libFp.isAncestorOf(m_imgPath))
      os.child("imgPath") << 1 << m_imgPath - libFp;
    else
      os.child("imgPath") << 0 << m_imgPath;
    os.child("imgARMode") << (int)m_imgARMode;
  } else {
    if (m_type == FreeText) os.child("text") << m_text;

    os.child("maximumFontSize") << m_maximumFontSize;
    os.child("color") << m_color.red() << m_color.green() << m_color.blue()
                      << m_color.alpha();
    os.child("font") << m_font.family() << (int)(m_font.bold() ? 1 : 0)
                     << (int)(m_font.italic() ? 1 : 0);
  }
}

void BoardItem::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "type") {
      std::wstring typeStr;
      is >> typeStr;
      m_type = string2Type(typeStr);
    } else if (tagName == "name") {
      std::wstring str;
      is >> str;
      m_name = QString::fromStdWString(str);
    } else if (tagName == "rect") {
      double x, y, width, height;
      is >> x >> y >> width >> height;
      m_rect = QRectF(x, y, width, height);
    } else if (tagName == "imgPath") {
      int isInLibrary;
      TFilePath fp;
      is >> isInLibrary >> fp;
      if (isInLibrary == 1)
        m_imgPath = ToonzFolder::getLibraryFolder() + fp;
      else
        m_imgPath = fp;
    } else if (tagName == "imgARMode") {
      int mode;
      is >> mode;
      m_imgARMode = (Qt::AspectRatioMode)mode;
    } else if (tagName == "text") {
      std::wstring str;
      is >> str;
      m_text = QString::fromStdWString(str);
    } else if (tagName == "maximumFontSize") {
      is >> m_maximumFontSize;
    } else if (tagName == "color") {
      int r, g, b, a;
      is >> r >> g >> b >> a;
      m_color = QColor(r, g, b, a);
    } else if (tagName == "font") {
      QString family;
      int isBold, isItalic;
      is >> family >> isBold >> isItalic;
      m_font.setFamily(family);
      m_font.setBold(isBold == 1);
      m_font.setItalic(isItalic == 1);
    } else
      throw TException("unexpected tag: " + tagName);
    is.closeChild();
  }
}

//======================================================================================

BoardSettings::BoardSettings() {
  // add one item as an example
  m_items.push_back(BoardItem());
}

QImage BoardSettings::getBoardImage(TDimension &dim, int shrink,
                                    ToonzScene *scene) {
  QImage img(dim.lx, dim.ly, QImage::Format_ARGB32);

  QPainter painter(&img);

  painter.fillRect(img.rect(), Qt::white);

  // draw each item
  for (int i = m_items.size() - 1; i >= 0; i--)
    m_items[i].drawItem(painter, img.rect().size(), shrink, scene);

  painter.end();

  return img;
}

TRaster32P BoardSettings::getBoardRaster(TDimension &dim, int shrink,
                                         ToonzScene *scene) {
  QImage img = getBoardImage(dim, shrink, scene);

  // convert QImage to TRaster
  TRaster32P boardRas(dim);
  int img_y = img.height() - 1;
  for (int j = 0; j < dim.ly; j++, img_y--) {
    TPixel32 *pix = boardRas->pixels(j);
    QRgb *img_p   = (QRgb *)img.scanLine(img_y);
    for (int i = 0; i < dim.lx; i++, pix++, img_p++) {
      (*pix).r = (typename TPixel32::Channel)(qRed(*img_p));
      (*pix).g = (typename TPixel32::Channel)(qGreen(*img_p));
      (*pix).b = (typename TPixel32::Channel)(qBlue(*img_p));
      (*pix).m = (typename TPixel32::Channel)(qAlpha(*img_p));
    }
  }
  return boardRas;
}

void BoardSettings::addNewItem(int insertAt) {
  m_items.insert(insertAt, BoardItem());
}

void BoardSettings::removeItem(int index) {
  if (index < 0 || index >= m_items.size()) return;
  m_items.removeAt(index);
}

void BoardSettings::swapItems(int i, int j) { m_items.swap(i, j); }

void BoardSettings::saveData(TOStream &os, bool forPreset) {
  if (!forPreset) os.child("active") << (int)((m_active) ? 1 : 0);
  os.child("duration") << m_duration;
  if (!m_items.isEmpty()) {
    os.openChild("boardItems");
    for (int i = 0; i < getItemCount(); i++) {
      os.openChild("item");
      m_items[i].saveData(os);
      os.closeChild();
    }
    os.closeChild();
  }
}

void BoardSettings::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "active") {
      int val;
      is >> val;
      setActive(val == 1);
    } else if (tagName == "duration") {
      is >> m_duration;
    } else if (tagName == "boardItems") {
      m_items.clear();
      while (is.matchTag(tagName)) {
        if (tagName == "item") {
          BoardItem item;
          item.loadData(is);
          m_items.append(item);
        } else
          throw TException("unexpected tag: " + tagName);
        is.closeChild();
      }
    } else
      throw TException("unexpected tag: " + tagName);
    is.closeChild();
  }
}