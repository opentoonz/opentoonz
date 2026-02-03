#include "layoutUtils.h"
#include "tenv.h"
#include "toonz/toonzfolders.h"
#include "toonz/cleanupparameters.h"
#include "toonz/sceneproperties.h"
#include "toonz/stage.h"
#include "toonz/stage2.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"
#include <tgeometry.h>
#include <toonz/txshsimplelevel.h>

#include "tfxutil.h"
#include "timage.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshlevelcolumn.h"
#include <QSettings>

#include "tsystem.h"

#include "tapp.h"
#include <QList>
#include <QString>
#include <string>

#include "toonz/tstageobjecttree.h"
#include "toonz/txsheethandle.h"

#include "toonz/levelset.h"
#include "toonz/txshcell.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheethandle.h"
#include "tpalette.h"
#include "tmsgcore.h"
#include <QObject>

namespace {
layoutUtils::LayoutCache s_cache;
}

TRectD layoutUtils::getLayoutRect() {
  updateCacheIfNeeded();

  TXshSimpleLevelP sl = s_cache.sl;
  if (!sl) return TRectD();
  TPointD offset = s_cache.offset;
  TFrameId fid   = sl->getFirstFid();
  if (sl->getType() == TXshLevelType::PLI_XSHLEVEL) {
    const double factor = 1 / Stage::standardDpi * Stage::inch;
    offset *= factor;
    TImageP img = sl->getFrame(fid, false);
    if (!img) return TRectD();
    return img->getBBox() + offset;
  } else {
    TDimension res    = sl->getResolution();
    const double facX = 1 / sl->getDpi().x * Stage::inch;
    const double facY = 1 / sl->getDpi().y * Stage::inch;
    TRectD rect(0, 0, res.lx * facX, res.ly * facY);
    rect -= 0.5 * (rect.getP00() + rect.getP11());
    offset.x *= facX;
    offset.y *= facY;
    return rect + offset;
  }
}

TDimensionD layoutUtils::getLayoutSize() {
  updateCacheIfNeeded();

  TXshSimpleLevelP sl = s_cache.sl;
  if (!sl) return TDimensionD();
  TFrameId fid = sl->getFirstFid();

  if (sl->getType() == TXshLevelType::PLI_XSHLEVEL) {
    TRectD rect = sl->getFrame(fid, false)->getBBox();
    TDimensionD size(rect.getLx(), rect.getLy());
    size.lx /= Stage::inch;
    size.ly /= Stage::inch;
    return size;
  } else {
    TDimension res = sl->getResolution();
    TDimensionD size(res.lx, res.ly);
    size.lx /= sl->getDpi().x;
    size.ly /= sl->getDpi().y;
    return size;
  }
}

TFilePath layoutUtils::decodePath(const QString& path) {
  if (path.isEmpty()) return TFilePath();
  TFilePath fp(path);
  if (fp.getParentDir().isEmpty()) {
    fp = ToonzFolder::getLibraryFolder() + TFilePath("layouts") + fp;
  }
  return fp;
}

TPointD layoutUtils::getCameraDpi() {
  return TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getCurrentCamera()
      ->getDpi();
}

TFilePath layoutUtils::getLayoutIniPath() {
  TFilePath fp         = TEnv::getConfigDir();
  std::string fileName = "layoutpresets.ini";
  TFilePath searchPath = fp;
  static TFilePath lastIniPath;
  if (TEnv::getConfigDir().isAncestorOf(lastIniPath)) return lastIniPath;

  while (!TFileStatus(searchPath + fileName).doesExist() &&
         !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
    searchPath = searchPath.getParentDir();
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fileName   = "safearea.ini";
    searchPath = fp;

    while (!TFileStatus(searchPath + fileName).doesExist() &&
           !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
      searchPath = searchPath.getParentDir();
    }
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fp = fp + "layoutpresets.ini";
    TSystem::touchFile(fp);
  } else
    fp = searchPath + fileName;

  lastIniPath = fp;
  return fp;
}

void layoutUtils::loadLayoutPresetList(QList<QString>& nameList) {
  TFilePath fp = getLayoutIniPath();

  if (TFileStatus(fp).doesExist()) {
    QSettings settings(toQString(fp), QSettings::IniFormat);

    // find the current layout preset name from the list
    QStringList groups = settings.childGroups();
    for (int g = 0; g < groups.size(); g++) {
      if (!groups.at(g).startsWith("Layout") &&
          !groups.at(g).startsWith("SafeArea"))
        continue;
      settings.beginGroup(groups.at(g));
      nameList.push_back(settings.value("name", "").toString());
      settings.endGroup();
    }
  }
}

void layoutUtils::loadLayoutPreset(QString name, LayoutPreset& preset) {
  // Reset preset to default state
  assert(!name.isEmpty());
  preset = LayoutPreset();
  if (name.isEmpty()) return;

  TFilePath fp = getLayoutIniPath();
  if (!TFileStatus(fp).doesExist()) {
    return;  // INI file doesn't exist
  }

  QSettings settings(toQString(fp), QSettings::IniFormat);

  // Search through all groups to find the matching preset
  QStringList groups = settings.childGroups();
  for (const QString& group : groups) {
    // Only process Layout and SafeArea groups
    if (!group.startsWith("Layout") && !group.startsWith("SafeArea")) {
      continue;
    }

    settings.beginGroup(group);

    QString presetName = settings.value("name", "").toString();
    if (presetName == name) {
      // Found matching preset - load basic info
      preset.m_name       = presetName;
      preset.m_layoutPath = settings.value("layout", "").toString();

      // Load layout offset values
      QList<QVariant> offsetValues =
          settings.value("layoutOffset", QList<QVariant>()).toList();
      if (offsetValues.size() >= 2) {
        preset.m_layoutOffsetX = offsetValues[0].toDouble();
        preset.m_layoutOffsetY = offsetValues[1].toDouble();
      }

      // Load area data without sorting - preserve original order
      settings.beginGroup("area");
      QStringList areaKeys = settings.childKeys();

      // Process area keys in the order they appear in the file
      for (const QString& key : areaKeys) {
        QList<QVariant> areaData =
            settings.value(key, QList<QVariant>()).toList();
        FramesDef framesDef;

        // Convert variant list to double parameters
        for (const QVariant& value : areaData) {
          framesDef.m_params.append(value.toDouble());
        }

        preset.m_areas.append(framesDef);
      }

      settings.endGroup();  // End area group
      settings.endGroup();  // End layout group
      return;               // Preset loaded successfully
    }

    settings.endGroup();  // End layout group
  }

  // If no matching preset found, preset remains in reset state
}

void layoutUtils::updateCacheIfNeeded() {
  static ToonzScene* scene = nullptr;
  ToonzScene* currentScene = TApp::instance()->getCurrentScene()->getScene();
  if (s_cache.isValid == false || currentScene != scene) {
    QString name = TApp::instance()
                       ->getCurrentScene()
                       ->getScene()
                       ->getProperties()
                       ->getLayoutPresetName();
    if (name.isEmpty()) {
      s_cache.clear();
      s_cache.isValid = true;
    } else {
      LayoutPreset preset;
      loadLayoutPreset(name, preset);
      if (currentScene != scene) s_cache.clear();
      s_cache.update(preset);
    }
    updateCamOffset();
    scene = currentScene;
  }
}

void layoutUtils::refreshCacheWithPreset(const LayoutPreset& preset) {
  s_cache.update(preset);
  updateCamOffset();
}

void layoutUtils::invalidateCurrent() { s_cache.invalidate(); }

void layoutUtils::getAreaList(QList<QList<double>>& areaList) {
  updateCacheIfNeeded();
  areaList = s_cache.areaList;
  if (areaList.isEmpty() && !s_cache.sl) {
    // [0]: Width (80.0%)
    // [1]: Height (80.0%)
    // [2]: R (255)
    // [3]: G (0)
    // [4]: B (0)
    QList<double> defaultSize1;
    defaultSize1 << 80.0 << 80.0;
    QList<double> defaultSize2;
    defaultSize2 << 90.0 << 90.0;
    areaList.append(defaultSize1);
    areaList.append(defaultSize2);
  }
};

void layoutUtils::getLayoutTemplate(TXshSimpleLevelP& slP) {
  updateCacheIfNeeded();
  slP = s_cache.sl;
};

void layoutUtils::getLayoutOffset(TPointD& offset) {
  updateCacheIfNeeded();
  offset = s_cache.offset;
}

void layoutUtils::getLayoutPreset(QList<QList<double>>& areaList,
                                  TXshSimpleLevelP& slP, TPointD& offset) {
  updateCacheIfNeeded();

  if (s_cache.sl) {
    slP    = s_cache.sl;
    offset = s_cache.offset;
  }
  areaList = s_cache.areaList;
  if (areaList.isEmpty() && !s_cache.sl) {
    // [0]: Width (80.0%)
    // [1]: Height (80.0%)
    // [2]: R (255)
    // [3]: G (0)
    // [4]: B (0)
    QList<double> defaultSize1;
    defaultSize1 << 80.0 << 80.0;
    QList<double> defaultSize2;
    defaultSize2 << 90.0 << 90.0;
    areaList.append(defaultSize1);
    areaList.append(defaultSize2);
  }
}

TRasterFxP layoutUtils::getLayoutFx(int frames) {
  updateCacheIfNeeded();
  TRasterFxP layoutFx;
  TXshSimpleLevelP sl = s_cache.sl;
  if (!sl) return layoutFx;
  TPointD offset    = s_cache.offset;
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  if (sl) {
    TFrameId fid = sl->getFirstFid();
    if (!fid.isEmptyFrame()) {
      TXshCell cell_template = TXshCell(sl.getPointer(), sl->getFirstFid());
      std::vector<TXshCell> cellsArray(frames, cell_template);
      TXshLevelColumn* col = new TXshLevelColumn();
      col->setCells(0, frames, cellsArray.data());
      TLevelColumnFx* colFx = col->getLevelColumnFx();
      TAffine offsetAff     = TTranslation(offset.x, offset.y);
      if (sl->getType() == TXshLevelType::PLI_XSHLEVEL) {
        TPointD camDpi = scene->getCurrentCamera()->getDpi();
        layoutFx       = TFxUtil::makeAffine(
            colFx,
            offsetAff * TScale(camDpi.y / Stage::inch, camDpi.y / Stage::inch));
      } else
        layoutFx = TFxUtil::makeAffine(colFx, offsetAff);

      layoutFx->setName(L"Layout");
    }
    return layoutFx;
  }
}

TStageObject* layoutUtils::getCurrentCamera() {
  bool isCleanupCam = (CleanupPreviewCheck::instance()->isEnabled() ||
                       CameraTestCheck::instance()->isEnabled());

  TXsheet* xsh           = TApp::instance()->getCurrentXsheet()->getXsheet();
  TStageObjectTree* tree = xsh->getStageObjectTree();
  TStageObjectId camId   = tree->getCurrentCameraId();
  TStageObject* cam      = tree->getStageObject(camId, false);
  if (!cam)
    return nullptr;
  else if (isCleanupCam) {
    cam->setOffset(TPointD());
    return nullptr;
  } else
    return cam;
}

void layoutUtils::updateCamOffset() {
  TStageObject* cam = getCurrentCamera();
  if (!cam) return;
  TPointD offset = s_cache.offset;
  offset /= Stage::standardDpi;

  cam->setOffset(-offset);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void layoutUtils::addLayoutToXsheet() {
  QString currentLayoutPresetName = TApp::instance()
                                        ->getCurrentScene()
                                        ->getScene()
                                        ->getProperties()
                                        ->getLayoutPresetName();
  if (currentLayoutPresetName.isEmpty()) {
    DVGui::warning(QObject::tr("Please select one layout preset."));
    return;
  }

  TXshSimpleLevelP sl;
  layoutUtils::getLayoutTemplate(sl);

  if (!sl) {
    DVGui::warning(QObject::tr("Not valid layout template image!"));
    return;
  }

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();

  int levelType = sl->getType();
  assert(levelType > 0);

  std::wstring dstName =
      s_cache.name.isEmpty() ? sl->getName() : s_cache.name.toStdWString();
  auto levelSet          = scene->getLevelSet();
  std::wstring finalName = dstName;
  for (int i = 1;; ++i) {
    TXshLevel* existingLevel = levelSet->getLevel(finalName);
    if (!existingLevel) {
      TFilePath fp     = scene->getDefaultLevelPath(sl->getType(), finalName);
      TFilePath abPath = scene->decodeFilePath(fp);
      if (!levelSet->hasLevel(*scene, fp) && !TFileStatus(abPath).doesExist()) {
        break;
      }
    }
    finalName = dstName + std::to_wstring(i);
  }

  // Clone Level
  TXshSimpleLevel* dstSl =
      scene->createNewLevel(sl->getType(), finalName)->getSimpleLevel();

  dstSl->setScene(scene);
  dstSl->clonePropertiesFrom(sl.getPointer());
  if (levelType == TZP_XSHLEVEL || levelType == PLI_XSHLEVEL) {
    TPalette* palette = sl->getPalette();
    assert(palette);

    dstSl->setPalette(palette->clone());
    dstSl->getPalette()->setDirtyFlag(true);
  }
  TFrameId fid = TFrameId(1);
  dstSl->setFrame(fid, sl->getFrame(sl->getFirstFid(), false)->cloneImage());
  dstSl->setDirtyFlag(true);

  // Add to Xsheet
  int frameCount = scene->getFrameCount();

  if (frameCount == 0) {
    frameCount = 1;
  }
  levelSet->insertLevel(dstSl);
  auto xsh     = scene->getXsheet();
  int colCount = xsh->getColumnCount();
  xsh->insertColumn(colCount);
  TXshCell cell = TXshCell(dstSl, dstSl->getFirstFid());

  std::vector<TXshCell> cellsArray(frameCount, cell);

  xsh->setCells(0, colCount, frameCount, cellsArray.data());

  // Emit notifications
  TApp* app = TApp::instance();

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifyCastChange();
}
