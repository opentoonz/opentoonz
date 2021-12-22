#include "tvpjson_io.h"

#include "tsystem.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobject.h"
#include "toutputproperties.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcamera.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "filebrowserpopup.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QPair>

using namespace TvpJsonIo;

namespace {

QMap<QString, BlendMode> BlendingMatch = {{"Color", BlendMode_Color},
                                          {"Darken", BlendMode_Darken}};

QString getFrameNumberWithLetters(int frame) {
  int letterNum = frame % 10;
  QChar letter;

  switch (letterNum) {
  case 0:
    letter = QChar();
    break;
  case 1:
    letter = 'A';
    break;
  case 2:
    letter = 'B';
    break;
  case 3:
    letter = 'C';
    break;
  case 4:
    letter = 'D';
    break;
  case 5:
    letter = 'E';
    break;
  case 6:
    letter = 'F';
    break;
  case 7:
    letter = 'G';
    break;
  case 8:
    letter = 'H';
    break;
  case 9:
    letter = 'I';
    break;
  default:
    letter = QChar();
    break;
  }

  QString number;
  if (frame >= 10) {
    number = QString::number(frame);
    number.chop(1);
  } else
    number = "0";

  return (QChar(letter).isNull()) ? number : number.append(letter);
}
}  // namespace

//-----------------------------------------------------------------------------
void TvpJsonLayerLinkItem::read(const QJsonObject& json) {
  m_instance_index       = json["instance-index"].toInt();
  m_instance_name        = json["instance-name"].toString();
  m_file                 = json["file"].toString();
  QJsonArray imagesArray = json["images"].toArray();
  for (int imageIndex = 0; imageIndex < imagesArray.size(); imageIndex++) {
    m_images.append(imagesArray[imageIndex].toInt());
  }
}

void TvpJsonLayerLinkItem::write(QJsonObject& json) const {
  json["instance-index"] = m_instance_index;
  json["instance-name"]  = m_instance_name;
  json["file"]           = m_file;
  QJsonArray imagesArray;
  for (const auto frame : m_images) {
    imagesArray.append(frame);
  }
  json["images"] = imagesArray;
}

void TvpJsonLayerLinkItem::build(int instance_index, QString instance_name,
                                 QString file, QList<int> images) {
  m_instance_index = instance_index;
  m_instance_name  = instance_name;
  m_file           = file;
  m_images         = images;
}

//-----------------------------------------------------------------------------
void TvpJsonLayer::read(const QJsonObject& json) {
  m_name     = json["name"].toString();
  m_visible  = json["visible"].toBool();
  m_position = json["position"].toInt();
  m_opacity  = json["opacity"].toInt();
  m_start    = json["start"].toInt();
  m_end      = json["end"].toInt();

  QString blendModeName = json["blending-mode"].toString("Color");
  m_blending_mode       = BlendingMatch.value(blendModeName, BlendMode_Color);

  QJsonArray linkArray = json["link"].toArray();
  for (int linkIndex = 0; linkIndex < linkArray.size(); linkIndex++) {
    QJsonObject linkItemObject = linkArray[linkIndex].toObject();
    TvpJsonLayerLinkItem linkItem;
    linkItem.read(linkItemObject);
    m_link.append(linkItem);
  }
}

void TvpJsonLayer::write(QJsonObject& json) const {
  json["name"]     = m_name;
  json["visible"]  = m_visible;
  json["position"] = m_position;
  json["opacity"]  = m_opacity;
  json["start"]    = m_start;
  json["end"]      = m_end;

  QString blendModeName = BlendingMatch.key(m_blending_mode, "Color");
  json["blending-mode"] = blendModeName;

  QJsonArray linkArray;
  for (const auto linkItem : m_link) {
    QJsonObject linkItemObject;
    linkItem.write(linkItemObject);
    linkArray.append(linkItemObject);
  }
  json["link"] = linkArray;
}

void TvpJsonLayer::build(int index, ToonzScene* scene, TXshCellColumn* column) {
  m_position = index;
  column->getRange(m_start, m_end);

  TXshCell prevCell;

  TFilePath sceneFolder =
      scene->decodeFilePath(scene->getScenePath().getParentDir());

  // JSONに記入可能かどうかチェック済みのレベル
  QMap<TXshLevel*, TFilePath> checkedLevels;
  // register the firstly-found level
  TXshLevel* firstFoundLevel = nullptr;

  QList<TXshCell> cellList;
  QList<QList<int>> imgsList;
  QMap<TXshLevel*, QPair<int, char>> frameFormats;

  for (int row = m_start; row <= m_end; row++) {
    TXshCell cell = column->getCell(row);
    // continue if the cell is continuous
    if (prevCell == cell) continue;

    // try to register the level.
    TXshLevel* level = cell.getSimpleLevel();
    if (!level)
      cell = TXshCell();
    else if (checkedLevels.contains(level)) {
      if (checkedLevels[level].isEmpty())
        cell = TXshCell();
      else {
      }     // do nothing
    } else  // newly found level
    {
      // Only raster levels are exported
      // currently, only the files located under the scene folder will be
      // exported
      if (level->getType() != OVL_XSHLEVEL)
        cell = TXshCell();
      else {
        TFilePath levelFullPath = scene->decodeFilePath(level->getPath());
        if (sceneFolder.isAncestorOf(levelFullPath)) {
          checkedLevels[level] = levelFullPath - sceneFolder;
          if (!firstFoundLevel) firstFoundLevel = level;
          std::vector<TFrameId> fids;
          level->getFids(fids);
          frameFormats[level] = QPair<int, char>(fids[0].getZeroPadding(),
                                                 fids[0].getStartSeqInd());
        } else
          cell = TXshCell();
      }
      if (cell.isEmpty()) checkedLevels[level] = TFilePath();
    }

    // continue if the cell is continuous
    if (prevCell == cell) continue;

    if (!cellList.contains(cell)) {
      cellList.append(cell);
      imgsList.append(QList<int>());
    }
    imgsList[cellList.indexOf(cell)].append(row);

    prevCell = cell;
  }

  if (imgsList.isEmpty()) return;

  //-------

  for (int i = 0; i < imgsList.size(); ++i) {
    TXshCell cell = cellList[i];

    int instance_index = imgsList[i].at(0);

    QString instance_name;
    QString file;
    if (cell.isEmpty()) {  // 空コマの場合
      instance_name = QString::number(0);
      TFrameId zeroFid(0, 0, frameFormats[firstFoundLevel].first,
                       frameFormats[firstFoundLevel].second);
      TFilePath imgPath = checkedLevels[firstFoundLevel].withFrame(zeroFid);
      file              = imgPath.getQString();
    } else {
      TFrameId fid = cell.getFrameId();

      // convert the last one digit of the frame number to alphabet
      // Ex.  12 -> 1B    21 -> 2A   30 -> 3
      if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
        instance_name = getFrameNumberWithLetters(fid.getNumber());
      else {
        QString frameNumber("");
        // set number
        if (fid.getNumber() >= 0)
          frameNumber = QString::number(fid.getNumber());
        // add letter
        if (!fid.getLetter().isEmpty()) frameNumber += fid.getLetter();
        instance_name = frameNumber;
      }

      fid.setZeroPadding(frameFormats[cell.m_level.getPointer()].first);
      fid.setStartSeqInd(frameFormats[cell.m_level.getPointer()].second);
      TFilePath imgPath =
          checkedLevels[cell.m_level.getPointer()].withFrame(fid);
      file = imgPath.getQString();
    }

    TvpJsonLayerLinkItem linkItem;
    linkItem.build(instance_index, instance_name, file, imgsList[i]);
    m_link.append(linkItem);
  }

  if (firstFoundLevel)
    m_name = QString::fromStdWString(firstFoundLevel->getName());
  else
    m_name = QString();

  m_visible = column->isPreviewVisible();
  m_opacity = (int)column->getOpacity();
}

//-----------------------------------------------------------------------------
void TvpJsonClip::read(const QJsonObject& json) {
  m_name = json["name"].toString("Untitled");
  m_cameraSize.setWidth(json["width"].toInt(1920));
  m_cameraSize.setHeight(json["height"].toInt(1080));
  m_framerate        = json["framerate"].toDouble(24.0);
  m_pixelaspectratio = json["pixelaspectratio"].toDouble(1.0);
  m_image_count      = json["image-count"].toInt();

  QJsonObject bgObject = json["bg"].toObject();
  m_bg.setRed(bgObject["red"].toInt(255));
  m_bg.setGreen(bgObject["green"].toInt(255));
  m_bg.setBlue(bgObject["blue"].toInt(255));
  m_bg.setAlpha(255);

  QJsonArray layersArray = json["layers"].toArray();
  for (int layerIndex = 0; layerIndex < layersArray.size(); layerIndex++) {
    QJsonObject layerObject = layersArray[layerIndex].toObject();
    TvpJsonLayer layer;
    layer.read(layerObject);
    m_layers.append(layer);
  }
}

void TvpJsonClip::write(QJsonObject& json) const {
  json["name"]             = m_name;
  json["width"]            = m_cameraSize.width();
  json["height"]           = m_cameraSize.height();
  json["framerate"]        = m_framerate;
  json["pixelaspectratio"] = m_pixelaspectratio;
  json["image-count"]      = m_image_count;

  QJsonObject bgObject;
  bgObject["red"]   = m_bg.red();
  bgObject["green"] = m_bg.green();
  bgObject["blue"]  = m_bg.blue();
  json["bg"]        = bgObject;

  QJsonArray layersArray;
  for (const auto layer : m_layers) {
    QJsonObject layerObject;
    layer.write(layerObject);
    layersArray.append(layerObject);
  }
  json["layers"] = layersArray;
}

void TvpJsonClip::build(ToonzScene* scene, TXsheet* xsheet) {
  TFilePath fp = scene->getScenePath();
  m_name       = QString::fromStdString(fp.getName());

  TStageObjectId cameraId = xsheet->getStageObjectTree()->getCurrentCameraId();
  TCamera* currentCamera  = xsheet->getStageObject(cameraId)->getCamera();
  TDimension cameraRes    = currentCamera->getRes();
  m_cameraSize            = QSize(cameraRes.lx, cameraRes.ly);

  TOutputProperties* outputProperties =
      scene->getProperties()->getOutputProperties();
  m_framerate = outputProperties->getFrameRate();

  TPointD cameraDpi  = currentCamera->getDpi();
  m_pixelaspectratio = cameraDpi.y / cameraDpi.x;

  // if the current xsheet is top xsheet in the scene and the output
  // frame range is specified, set the "to" frame value as duration
  // TOutputProperties* oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;
  if (scene->getTopXsheet() == xsheet &&
      outputProperties->getRange(from, to, step))
    m_image_count = to + 1;
  else
    m_image_count = xsheet->getFrameCount();

  TPixel bgColor = scene->getProperties()->getBgColor();
  m_bg           = QColor((int)bgColor.r, (int)bgColor.g, (int)bgColor.b);

  if (xsheet->getColumnCount() == 0) return;
  for (int col = xsheet->getColumnCount() - 1; col >= 0; col--) {
    if (xsheet->isColumnEmpty(col)) {
      continue;
    }
    TXshCellColumn* column = xsheet->getColumn(col)->getCellColumn();
    if (!column) {
      continue;
    }
    TvpJsonLayer layer;
    layer.build(m_layers.size(), scene, column);
    if (!layer.isEmpty()) m_layers.append(layer);
  }
}

//-----------------------------------------------------------------------------
void TvpJsonProject::read(const QJsonObject& json) {
  if (json.contains("format")) {
    QJsonObject formatObject = json["format"].toObject();
    if (formatObject.contains("extension"))
      m_formatExtension = formatObject["extension"].toString();
  }
  QJsonObject clipObject = json["clip"].toObject();
  m_clip.read(clipObject);
}

void TvpJsonProject::write(QJsonObject& json) const {
  QJsonObject formatObject;
  formatObject["extension"] = m_formatExtension;
  json["format"]            = formatObject;

  QJsonObject clipObject;
  m_clip.write(clipObject);
  json["clip"] = clipObject;
}

void TvpJsonProject::build(ToonzScene* scene, TXsheet* xsheet) {
  TOutputProperties* outputProperties =
      scene->getProperties()->getOutputProperties();
  m_formatExtension =
      QString::fromStdString(outputProperties->getPath().getType());

  m_clip.build(scene, xsheet);
}

//-----------------------------------------------------------------------------
void TvpJsonVersion::read(const QJsonObject& json) {
  m_major = json["major"].toInt();
  m_minor = json["minor"].toInt();
}

void TvpJsonVersion::write(QJsonObject& json) const {
  json["major"] = m_major;
  json["minor"] = m_minor;
}

void TvpJsonVersion::setVersion(int major, int minor) {
  m_major = major;
  m_minor = minor;
}

//-----------------------------------------------------------------------------
void TvpJsonData::read(const QJsonObject& json) {
  QJsonObject versionObject = json["version"].toObject();
  m_version.read(versionObject);

  QJsonObject projectObject = json["project"].toObject();
  m_project.read(projectObject);
}

void TvpJsonData::write(QJsonObject& json) const {
  QJsonObject versionObject;
  m_version.write(versionObject);
  json["version"] = versionObject;

  QJsonObject projectObject;
  m_project.write(projectObject);
  json["project"] = projectObject;
}

void TvpJsonData::build(ToonzScene* scene, TXsheet* xsheet) {
  // currently this i/o is based on version 5.1 files
  m_version.setVersion(5, 1);
  m_project.build(scene, xsheet);
}

//-----------------------------------------------------------------------------

class ExportTvpJsonCommand final : public MenuItemHandler {
public:
  ExportTvpJsonCommand() : MenuItemHandler(MI_ExportTvpJson) {}
  void execute() override;
} exportTvpJsonCommand;

void ExportTvpJsonCommand::execute() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene->isUntitled()) {
    DVGui::error(
        QObject::tr("TVPaint JSON file cannot be exported from untitled scene. "
                    "Save the scene first."));
    return;
  }
  TXsheet* xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp    = scene->getScenePath().withType("json");

  TvpJsonData tvpJsonData;
  tvpJsonData.build(scene, xsheet);
  if (tvpJsonData.isEmpty()) {
    DVGui::error(
        QObject::tr("No columns can be exported. Please note the followings:\n "
                    "- The level files must be placed at the same or child "
                    "folder relative to the scene file.\n - Currently only the "
                    "columns containing raster levels can be exported."));
    return;
  }

  static GenericSaveFilePopup* savePopup = 0;
  if (!savePopup) {
    savePopup =
        new GenericSaveFilePopup(QObject::tr("Export TVPaint JSON File"));
    savePopup->addFilterType("json");
  }
  if (!scene->isUntitled())
    savePopup->setFolder(fp.getParentDir());
  else
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());
  savePopup->setFilename(fp.withoutParentDir());
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  QFile saveFile(fp.getQString());

  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return;
  }

  QJsonObject tvpJsonObject;
  tvpJsonData.write(tvpJsonObject);
  QJsonDocument saveDoc(tvpJsonObject);
  QByteArray jsonByteArrayData = saveDoc.toJson();
  saveFile.write(jsonByteArrayData);

  QString str = QObject::tr("The file %1 has been exported successfully.")
                    .arg(QString::fromStdString(fp.getLevelName()));

  std::vector<QString> buttons = {QObject::tr("OK"),
                                  QObject::tr("Open containing folder")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons);

  if (ret == 2) {
    TFilePath folderPath = fp.getParentDir();
    if (TSystem::isUNC(folderPath))
      QDesktopServices::openUrl(QUrl(folderPath.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath.getQString()));
  }
}
