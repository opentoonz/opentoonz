#ifndef LAYOUTUTILS_H
#define LAYOUTUTILS_H

#include <tgeometry.h>
#include <tfilepath.h>
#include <toonz/txshsimplelevel.h>
#include "tfiletype.h"
#include "toonz/toonzscene.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/levelset.h"

#include "qvariant.h"
#include <QString>
#include <trasterfx.h>

#include <QObject>

namespace layoutUtils {

// Helper
TPointD getCameraDpi();

TRectD getLayoutRect();
TDimensionD getLayoutSize();
TFilePath decodePath(const QString& path);

struct FramesDef {
  QList<double> m_params;  // [width, height, r, g, b]

  QString toString() const {
    QStringList parts;
    for (int i = 0; i < m_params.size(); ++i) {
      if (i < 2)  // Width/Height (Percentage)
        parts << QString::number(m_params[i], 'f', 2);
      else  // R, G, B (Integer 0-255)
        parts << QString::number(m_params[i], 'f', 0);
    }
    return parts.join(", ");
  }

  static FramesDef fromString(const QString& str) {
    FramesDef def;
    QStringList parts = str.split(',');
    for (const QString& part : parts) {
      bool ok;
      double val = part.trimmed().toDouble(&ok);
      if (ok) def.m_params.append(val);
    }
    return def;
  }

  static FramesDef fromVariantList(const QList<QVariant>& list) {
    FramesDef def;
    for (const QVariant& v : list) def.m_params.append(v.toDouble());
    return def;
  }

  QList<QVariant> toVariantList() const {
    QList<QVariant> list;
    for (double p : m_params) list.append(p);
    return list;
  }
};

struct LayoutPreset {
  QString m_name         = QString();
  QString m_layoutPath   = QString();
  double m_layoutOffsetX = 0;
  double m_layoutOffsetY = 0;
  QList<FramesDef> m_areas;
};

// Load preset from ini file
TFilePath getLayoutIniPath();
void loadLayoutPresetList(QList<QString>& nameList);
void loadLayoutPreset(QString name, LayoutPreset& preset);

void updateCacheIfNeeded();
void refreshCacheWithPreset(const LayoutPreset& preset);
void invalidateCurrent();

struct LayoutCache {
  friend void layoutUtils::updateCacheIfNeeded();
  friend void layoutUtils::refreshCacheWithPreset(const LayoutPreset& preset);
  friend void layoutUtils::invalidateCurrent();

public:
  QString name        = QString();
  TXshSimpleLevelP sl = nullptr;
  TPointD offset      = TPointD();
  QList<QList<double>> areaList;
  bool isValid = false;

private:
  void update(const LayoutPreset& preset) {
    assert(!preset.m_name.isEmpty());

    name   = preset.m_name;
    offset = TPointD(preset.m_layoutOffsetX, preset.m_layoutOffsetY);

    areaList.clear();
    for (const FramesDef& def : preset.m_areas) {
      QList<double> doubleList;
      for (const QVariant& var : def.toVariantList()) {
        doubleList.append(var.toDouble());
      }
      areaList.append(doubleList);
    }

    isValid = true;
    TLevelSet* levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    if (sl) {
      // Remove Old level
      if (decodePath(preset.m_layoutPath) != sl->getPath()) {
        levelSet->removeLevel(sl.getPointer(), false);
        TApp::instance()->getCurrentScene()->notifyCastChange();
        sl.reset();
      } else
        return;
    } else {
      // Remove Old level
      std::vector<TXshLevel*> levels;
      levelSet->listLevels(levels, TFilePath("Layout"));
      if (!levels.empty()) {
        for (TXshLevel* level : levels) {
          levelSet->removeLevel(level, false);
        }
        TApp::instance()->getCurrentScene()->notifyCastChange();
      }
    }
    // Load the level
    if (!preset.m_layoutPath.isEmpty()) {
      TFilePath imgPath    = decodePath(preset.m_layoutPath);
      TFileType::Type type = TFileType::getInfo(imgPath);

      if (TFileType::isViewable(type)) {
        auto convertToXshLevelType = [](TFileType::Type type) -> int {
          // Extract base type (ignore LEVEL flag)
          int baseType = type & ~TFileType::LEVEL;

          switch (baseType) {
          case TFileType::CMAPPED_IMAGE:
            return TXshLevelType::TZP_XSHLEVEL;  // Toonz Raster
          case TFileType::RASTER_IMAGE:
            return TXshLevelType::OVL_XSHLEVEL;  // Raster
          case TFileType::VECTOR_IMAGE:
            return TXshLevelType::PLI_XSHLEVEL;  // Vector
          default:
            return TXshLevelType::UNKNOWN_XSHLEVEL;  // Unknown
          }
        };

        int levelType = convertToXshLevelType(type);
        if (levelType != TXshLevelType::UNKNOWN_XSHLEVEL) {
          ToonzScene* currentScene =
              TApp::instance()->getCurrentScene()->getScene();
          QString name =
              QString("%1 (%2,%3)").arg(this->name).arg(offset.x).arg(offset.y);
          sl = new TXshSimpleLevel(name.toStdWString());
          sl->setType(levelType);
          sl->setScene(currentScene);
          sl->setPath(imgPath);
          sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
          sl->getProperties()->setDpi(getCameraDpi());
          // sl->load();
          sl->setIsReadOnly(true);

          if (!sl->getFrameCount()) {
            sl.reset();
          } else {
            TLevelSet* levelSet =
                TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
            levelSet->createFolder(TFilePath(), L"Layout");
            levelSet->insertLevel(sl.getPointer());
            levelSet->moveLevelToFolder(TFilePath(L"Layout"), sl.getPointer());
            TApp::instance()->getCurrentScene()->notifyCastChange();
          }
        }
      }
    }
  }
  void invalidate() { isValid = false; }
  void clear() {
    if (name.isEmpty()) return;
    update(LayoutPreset());
  }
};

// Get from cache and load if necessary
void getAreaList(QList<QList<double>>& areaList);
void getLayoutTemplate(TXshSimpleLevelP& slP);
void getLayoutOffset(TPointD& offset);
void getLayoutPreset(QList<QList<double>>& areaList, TXshSimpleLevelP& sl,
                     TPointD& offset);

TRasterFxP getLayoutFx(int frames);

TStageObject* getCurrentCamera();
void updateCamOffset();

void addLayoutToXsheet();

}  // namespace layoutUtils

#endif  // LAYOUTUTILS_H