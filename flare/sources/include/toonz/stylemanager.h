#pragma once

#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include "tfilepath.h"
#include "tthread.h"

#include "traster.h"
#include "toonz/mypaintbrushstyle.h"

#include <QSize>
#include <QVector>
#include <QList>
#include <QImage>
#include <QString>
#include <QOffscreenSurface>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//********************************************************************************
//    FavoritesManager declaration
//********************************************************************************

class DVAPI FavoritesManager : public QObject {
  Q_OBJECT

  TFilePath m_fpPinsToTop;
  QVector<std::string> m_pinsToTop;
  bool m_xxPinsToTop;  // dirty pins-to-top

  FavoritesManager();

public:
  static FavoritesManager *instance();

  bool loadPinsToTop();
  void savePinsToTop();

  const QVector<std::string> &getAllPinsToTop() const { return m_pinsToTop; }

  bool getPinToTop(std::string idname) const;
  void setPinToTop(std::string idname, bool state);
  void togglePinToTop(std::string idname);

  void emitPinsToTopChange() { emit pinsToTopChanged(); }

signals:
  void pinsToTopChanged();
};

//********************************************************************************
//    BaseStyleManager declaration
//********************************************************************************

class DVAPI BaseStyleManager : public QObject {
  Q_OBJECT

public:
  struct DVAPI ChipData {
    QString name;        // chip name
    QString desc;        // chip description
    QImage image;        // preview image
    int tagId;           // tagID
    bool isVector;       // is a vector?
    TRasterP raster;     // raster data
    std::string idname;  // brush id name
    std::size_t hash;    // hash for fast compare with current style

    bool markPinToTop;  // Pin To Top
    bool markFavorite;  // Favorite

    ChipData(QString name = "", QString description = "",
             QImage image = QImage(), int tagId = 0, bool isVector = false,
             TRasterP raster = TRasterP(), std::string idname = "",
             std::size_t hash = 0)
        : name(name)
        , desc(description)
        , image(image)
        , tagId(tagId)
        , isVector(isVector)
        , raster(raster)
        , idname(idname)
        , hash(hash)
        , markPinToTop(false)
        , markFavorite(false) {}
  };

protected:
  TFilePath m_rootPath;
  TFilePath m_stylesFolder;
  QString m_filters;
  QSize m_chipSize;
  bool m_loaded;

  QVector<ChipData> m_chips;

  bool m_isIndexed;
  QList<int> m_indexes;
  QString m_searchText;

  static TFilePath s_rootPath;
  static ChipData s_emptyChipData;

public:
  BaseStyleManager(const TFilePath &stylesFolder, QString filters = QString(),
                   QSize chipSize = QSize(25, 25));

  const TFilePath &stylesFolder() const { return m_stylesFolder; }
  QSize getChipSize() const { return m_chipSize; }
  QString getFilters() const { return m_filters; }

  int countData() const {
    return m_isIndexed ? m_indexes.count() : m_chips.count();
  }
  int getIndex(int index) const {
    return m_isIndexed ? m_indexes[index] : index;
  }
  const ChipData &getData(int index) const;

  void applyFilter();
  void applyFilter(const QString text) {
    m_searchText = text;
    applyFilter();
  }
  QString getSearchText() const { return m_searchText; }

  bool isLoaded() { return m_loaded; }
  virtual void loadItems() = 0;

  static TFilePath getRootPath();
  static void setRootPath(const TFilePath &rootPath);
};

//********************************************************************************
//    CustomStyleManager declaration
//********************************************************************************

class DVAPI CustomStyleManager final : public BaseStyleManager {
  Q_OBJECT

  TThread::Executor m_executor;
  bool m_started;

  std::string m_rasterIdName;
  std::string m_vectorIdName;

public:
  CustomStyleManager(std::string rasterIdName, std::string vectorIdName,
                     const TFilePath &stylesFolder, QString filters = QString(),
                     QSize chipSize = QSize(25, 25));

  void loadItems() override;

  class StyleLoaderTask;
  friend class CustomStyleManager::StyleLoaderTask;

private:
  QImage makeIcon(const TFilePath &path, const QSize &qChipSize,
                  std::shared_ptr<QOffscreenSurface> offsurf);

  ChipData createPattern(const TFilePath &path,
                         std::shared_ptr<QOffscreenSurface> offsurf);

signals:
  void patternAdded();
};

//********************************************************************************
//    TextureStyleManager declaration
//********************************************************************************

class DVAPI TextureStyleManager final : public BaseStyleManager {
  Q_OBJECT

public:
  TextureStyleManager(const TFilePath &stylesFolder,
                      QSize chipSize = QSize(25, 25));

  void loadItems() override;

private:
  void loadTexture(const TFilePath &fp);
};

//********************************************************************************
//    MyPaintBrushStyleManager declaration
//********************************************************************************

class DVAPI MyPaintBrushStyleManager final : public BaseStyleManager {
  Q_OBJECT

  std::vector<TMyPaintBrushStyle> m_brushes;

public:
  MyPaintBrushStyleManager(QSize chipSize = QSize(25, 25));

  TMyPaintBrushStyle &getBrush(int index) {
    assert(0 <= index && index < countData());
    return m_brushes[getIndex(index)];
  }

  void loadItems() override;
};

//********************************************************************************
//    SpecialStyleManager declaration
//********************************************************************************

class DVAPI SpecialStyleManager final : public BaseStyleManager {
  Q_OBJECT

public:
  SpecialStyleManager(QSize chipSize = QSize(25, 25));

  void loadItems() override;
};

#endif  // STYLEMANAGER_H
