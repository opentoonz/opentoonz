#pragma once

#ifndef BOARDSETTINGS_H
#define BOARDSETTINGS_H

#include "traster.h"
#include "tfilepath.h"

// TnzCore includes
#include "tstream.h"

#include <QList>
#include <QRectF>
#include <QColor>
#include <QPainter>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class ToonzScene;

class DVAPI BoardItem {
public:
  enum Type {
    FreeText = 0,
    ProjectName,
    SceneName,
    Duration_Frame,
    Duration_SecFrame,
    Duration_HHMMSSFF,
    CurrentDate,
    CurrentDateTime,
    UserName,
    ScenePath_Aliased,
    ScenePath_Full,
    MoviePath_Aliased,
    MoviePath_Full,
    Image,
    TypeCount
  };

private:
  QString m_name;
  Type m_type;

  // item region, represented by ratio to the whole
  QRectF m_rect;

  // Basically the font size will be automatically
  // adjusted to fit the item region.
  // The maximum font size can be used to make the font
  // size smaller.
  int m_maximumFontSize;

  // font color
  QColor m_color;

  // font (familiy, bold, italic)
  QFont m_font;

  QString m_text;
  TFilePath m_imgPath;
  Qt::AspectRatioMode m_imgARMode = Qt::KeepAspectRatio;

  QString getContentText(ToonzScene*);

public:
  BoardItem();

  QRectF getRatioRect() { return m_rect; }
  void setRatioRect(QRectF rect) { m_rect = rect; }

  // returns the item rect in pixels
  QRectF getItemRect(QSize imgSize);
  void drawItem(QPainter& p, QSize imgSize, int shrink, ToonzScene* scene);

  QString getName() { return m_name; }
  void setName(QString name) { m_name = name; }

  Type getType() { return m_type; }
  void setType(Type type) { m_type = type; }

  int getMaximumFontSize() { return m_maximumFontSize; }
  void setMaximumFontSize(int size) { m_maximumFontSize = size; }

  QColor getColor() { return m_color; }
  void setColor(QColor color) { m_color = color; }

  QFont& font() { return m_font; }

  QString getFreeText() { return m_text; }
  void setFreeText(QString text) { m_text = text; }

  TFilePath getImgPath() { return m_imgPath; }
  void setImgPath(TFilePath path) { m_imgPath = path; }

  Qt::AspectRatioMode getImgARMode() { return m_imgARMode; }
  void setImgARMode(Qt::AspectRatioMode mode) { m_imgARMode = mode; }

  void saveData(TOStream& os);
  void loadData(TIStream& is);
};

class DVAPI BoardSettings {
  bool m_active  = false;
  int m_duration = 0;
  QList<BoardItem> m_items;

public:
  BoardSettings();

  QImage getBoardImage(TDimension& dim, int shrink, ToonzScene* scene);

  TRaster32P getBoardRaster(TDimension& dim, int shrink, ToonzScene* scene);

  int getDuration() { return m_duration; }

  bool isActive() { return m_active; }
  void setActive(bool on) { m_active = on; }

  int getItemCount() { return m_items.count(); }
  BoardItem& getItem(int index) { return m_items[index]; }

  void setDuration(int f) { m_duration = f; }

  void addNewItem(int insertAt = 0);
  void removeItem(int index);
  void swapItems(int, int);

  void saveData(TOStream& os, bool forPreset = false);
  void loadData(TIStream& is);
};

#endif