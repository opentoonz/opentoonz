

#include "xdtsio.h"

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

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "xdtsimportpopup.h"
#include "filebrowserpopup.h"

#include <iostream>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QFile>
#include <QJsonDocument>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>

using namespace XdtsIo;

namespace {
// XDTS file identifier string
static const QByteArray identifierStr("exchangeDigitalTimeSheet Save Data");

// Create color chip icon for cell marks
QIcon createColorChipIcon(TPixel32 color) {
  QPixmap pixmap(15, 15);
  pixmap.fill(QColor(color.r, color.g, color.b));
  return QIcon(pixmap);
}

// Global variables for export configuration
int tick1Id          = -1;
int tick2Id          = -1;
int keyFrameId       = -1;
int referenceFrameId = -1;
bool isUextVersion   = false;  // Whether XDTS is written in UEXT version
bool exportAllColumn = true;

const QString OPTION_KEYFRAME       = "OPTION_KEYFRAME";
const QString OPTION_REFERENCEFRAME = "OPTION_REFERENCEFRAME";

}  // namespace

//=============================================================================
// XdtsHeader implementation
//-----------------------------------------------------------------------------

void XdtsHeader::read(const QJsonObject& json) {
  static const QRegularExpression rx("^\\d{1,4}$");

  m_cut = json["cut"].toString();
  if (!rx.match(m_cut).hasMatch()) {
    qWarning() << "XdtsHeader 'cut' value does not match expected pattern:"
               << m_cut;
  }

  m_scene = json["scene"].toString();
  if (!rx.match(m_scene).hasMatch()) {
    qWarning() << "XdtsHeader 'scene' value does not match expected pattern:"
               << m_scene;
  }
}

void XdtsHeader::write(QJsonObject& json) const {
  json["cut"]   = m_cut;
  json["scene"] = m_scene;
}

//=============================================================================
// XdtsFrameDataItem implementation
//-----------------------------------------------------------------------------

TFrameId XdtsFrameDataItem::str2Fid(const QString& str) const {
  if (str.isEmpty()) return TFrameId::EMPTY_FRAME;

  bool ok;
  const int frame = str.toInt(&ok);
  if (ok) return TFrameId(frame);

  static const QRegularExpression rx(
      QString("^%1$").arg(TFilePath::fidRegExpStr()));
  const auto match = rx.match(str);
  if (!match.hasMatch()) return TFrameId();

  if (match.captured(2).isEmpty()) {
    return TFrameId(match.captured(1).toInt());
  } else {
    return TFrameId(match.captured(1).toInt(), match.captured(2));
  }
}

QString XdtsFrameDataItem::fid2Str(const TFrameId& fid) const {
  if (fid.getNumber() == -1) {
    return QString("SYMBOL_NULL_CELL");
  } else if (fid.getNumber() == SYMBOL_TICK_1) {
    return QString("SYMBOL_TICK_1");
  } else if (fid.getNumber() == SYMBOL_TICK_2) {
    return QString("SYMBOL_TICK_2");
  } else if (fid.getLetter().isEmpty()) {
    return QString::number(fid.getNumber());
  }
  return QString::number(fid.getNumber()) + fid.getLetter();
}

void XdtsFrameDataItem::read(const QJsonObject& json) {
  m_id = DataId(qRound(json["id"].toDouble()));

  const auto valuesArray = json["values"].toArray();
  for (const auto& value : valuesArray) {
    m_values.append(value.toString());
  }

  if (json.contains("options")) {
    const auto optionsArray = json["options"].toArray();
    for (const auto& option : optionsArray) {
      m_options.append(option.toString());
    }
  }
}

void XdtsFrameDataItem::write(QJsonObject& json) const {
  json["id"] = int(m_id);

  QJsonArray valuesArray;
  for (const auto& value : m_values) {
    valuesArray.append(value);
  }
  json["values"] = valuesArray;

  if (!m_options.isEmpty()) {
    QJsonArray optionsArray;
    for (const auto& option : m_options) {
      optionsArray.append(option);
    }
    json["options"] = optionsArray;
  }
}

XdtsFrameDataItem::FrameInfo XdtsFrameDataItem::getFrameInfo() const {
  if (m_values.isEmpty()) {
    return {TFrameId(-1), m_options};  // EMPTY cell
  }

  const QString val = m_values.at(0);
  if (val == "SYMBOL_NULL_CELL") {
    return {TFrameId(-1), m_options};  // EMPTY cell
  } else if (val == "SYMBOL_HYPHEN") {
    return {TFrameId(-2), m_options};  // IGNORE cell
  } else if (val == "SYMBOL_TICK_1") {
    return {TFrameId(SYMBOL_TICK_1), m_options};
  } else if (val == "SYMBOL_TICK_2") {
    return {TFrameId(SYMBOL_TICK_2), m_options};
  }

  return {str2Fid(m_values.at(0)), m_options};
}

//=============================================================================
// XdtsTrackFrameItem implementation
//-----------------------------------------------------------------------------

void XdtsTrackFrameItem::read(const QJsonObject& json) {
  const auto dataArray = json["data"].toArray();
  for (const auto& dataValue : dataArray) {
    XdtsFrameDataItem data;
    data.read(dataValue.toObject());
    m_data.append(data);
  }
  m_frame = json["frame"].toInt();
}

void XdtsTrackFrameItem::write(QJsonObject& json) const {
  QJsonArray dataArray;
  for (const auto& data : m_data) {
    QJsonObject dataObject;
    data.write(dataObject);
    dataArray.append(dataObject);
  }
  json["data"]  = dataArray;
  json["frame"] = m_frame;
}

QPair<int, XdtsFrameDataItem::FrameInfo> XdtsTrackFrameItem::frameFinfo()
    const {
  return {m_frame, m_data[0].getFrameInfo()};
}

//=============================================================================
// XdtsFieldTrackItem implementation
//-----------------------------------------------------------------------------

void XdtsFieldTrackItem::read(const QJsonObject& json) {
  const auto frameArray = json["frames"].toArray();
  for (const auto& frameValue : frameArray) {
    XdtsTrackFrameItem frame;
    frame.read(frameValue.toObject());
    m_frames.append(frame);
  }
  m_trackNo = json["trackNo"].toInt();
}

void XdtsFieldTrackItem::write(QJsonObject& json) const {
  QJsonArray frameArray;
  for (const auto& frame : m_frames) {
    QJsonObject frameObject;
    frame.write(frameObject);
    frameArray.append(frameObject);
  }
  json["frames"]  = frameArray;
  json["trackNo"] = m_trackNo;
}

QVector<TFrameId> XdtsFieldTrackItem::getCellFrameIdTrack(
    QList<int>& tick1, QList<int>& tick2, QList<int>& keyFrames,
    QList<int>& refFrames) const {
  QList<QPair<int, XdtsFrameDataItem::FrameInfo>> frameFinfos;
  for (const auto& frame : m_frames) {
    frameFinfos.append(frame.frameFinfo());
  }

  std::sort(frameFinfos.begin(), frameFinfos.end(),
            [](const auto& v1, const auto& v2) { return v1.first < v2.first; });

  QVector<TFrameId> cells;
  int currentFrame = 0;
  TFrameId initialNumber;

  for (auto& frameFinfo : frameFinfos) {
    while (currentFrame < frameFinfo.first) {
      cells.append(cells.isEmpty() ? initialNumber : cells.last());
      currentFrame++;
    }

    // Handle negative frame data (CSP export anomaly)
    if (frameFinfo.first < 0) {
      initialNumber = frameFinfo.second.frameId;
      continue;
    }

    const TFrameId cellFid = frameFinfo.second.frameId;
    if (cellFid.getNumber() == -2) {  // IGNORE case
      cells.append(cells.isEmpty() ? TFrameId(-1) : cells.last());
    } else if (cellFid.getNumber() == XdtsFrameDataItem::SYMBOL_TICK_1) {
      cells.append(cells.isEmpty() ? TFrameId(-1) : cells.last());
      tick1.append(currentFrame);
    } else if (cellFid.getNumber() == XdtsFrameDataItem::SYMBOL_TICK_2) {
      cells.append(cells.isEmpty() ? TFrameId(-1) : cells.last());
      tick2.append(currentFrame);
    } else {
      cells.append(cellFid);
    }

    const QStringList options = frameFinfo.second.options;
    if (options.contains(OPTION_KEYFRAME)) {
      keyFrames.append(currentFrame);
    }
    if (options.contains(OPTION_REFERENCEFRAME)) {
      refFrames.append(currentFrame);
    }

    currentFrame++;
  }

  return cells;
}

QString XdtsFieldTrackItem::build(TXshCellColumn* column) {
  TXshLevel* level = nullptr;
  TXshCell prevCell;
  int r0, r1;
  column->getRange(r0, r1);

  if (r0 > 0) addFrame(0, TFrameId(-1));

  for (int row = r0; row <= r1; ++row) {
    TXshCell cell = column->getCell(row);

    // Register the first found level (simple or child level)
    if (!level) level = cell.getSimpleLevel();
    if (!level) level = cell.getChildLevel();

    // If level doesn't match registered one, treat as empty cell
    if (!level || cell.m_level != level) {
      cell = TXshCell();
    }

    // Continue if cell is continuous
    if (prevCell == cell) {
      // Handle cell marks for ticks
      if (tick1Id >= 0 && column->getCellMark(row) == tick1Id) {
        addFrame(row, TFrameId(XdtsFrameDataItem::SYMBOL_TICK_1));
      } else if (tick2Id >= 0 && column->getCellMark(row) == tick2Id) {
        addFrame(row, TFrameId(XdtsFrameDataItem::SYMBOL_TICK_2));
      }
      continue;
    }

    if (cell.isEmpty()) {
      addFrame(row, TFrameId(-1));
    } else {
      QStringList options;
      // Handle keyframe and reference frame symbols
      if (keyFrameId >= 0 && column->getCellMark(row) == keyFrameId) {
        options.append(OPTION_KEYFRAME);
      } else if (referenceFrameId >= 0 &&
                 column->getCellMark(row) == referenceFrameId) {
        options.append(OPTION_REFERENCEFRAME);
      }

      if (!options.isEmpty()) {
        isUextVersion = true;
      }

      addFrame(row, cell.getFrameId(), options);
    }

    prevCell = cell;
  }

  addFrame(r1 + 1, TFrameId(-1));

  if (level) {
    return QString::fromStdWString(level->getName());
  } else {
    m_frames.clear();
    return QString();
  }
}

//=============================================================================
// XdtsTimeTableFieldItem implementation
//-----------------------------------------------------------------------------

void XdtsTimeTableFieldItem::read(const QJsonObject& json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  const auto trackArray = json["tracks"].toArray();

  for (const auto& trackValue : trackArray) {
    XdtsFieldTrackItem track;
    track.read(trackValue.toObject());
    m_tracks.append(track);
  }
}

void XdtsTimeTableFieldItem::write(QJsonObject& json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray trackArray;

  for (const auto& track : m_tracks) {
    QJsonObject trackObject;
    track.write(trackObject);
    trackArray.append(trackObject);
  }

  json["tracks"] = trackArray;
}

QList<int> XdtsTimeTableFieldItem::getOccupiedColumns() const {
  QList<int> result;
  for (const auto& track : m_tracks) {
    if (!track.isEmpty()) {
      result.append(track.getTrackNo());
    }
  }
  return result;
}

QVector<TFrameId> XdtsTimeTableFieldItem::getColumnTrack(
    int col, QList<int>& tick1, QList<int>& tick2, QList<int>& keyFrames,
    QList<int>& refFrames) const {
  for (const auto& track : m_tracks) {
    if (track.getTrackNo() == col) {
      return track.getCellFrameIdTrack(tick1, tick2, keyFrames, refFrames);
    }
  }
  return QVector<TFrameId>();
}

void XdtsTimeTableFieldItem::build(TXsheet* xsheet, QStringList& columnLabels) {
  m_fieldId     = CELL;
  int exportCol = 0;

  for (int col = 0; col < xsheet->getFirstFreeColumnIndex(); ++col) {
    if (xsheet->isColumnEmpty(col)) {
      columnLabels.append("");
      ++exportCol;
      continue;
    }

    auto column = xsheet->getColumn(col)->getCellColumn();
    if (!column) continue;  // Skip non-cell columns

    if (!exportAllColumn && !column->isPreviewVisible()) {
      continue;  // Skip inactive columns
    }

    XdtsFieldTrackItem track(exportCol);
    columnLabels.append(track.build(column));

    if (!track.isEmpty()) {
      m_tracks.append(track);
    }
    ++exportCol;
  }
}

//=============================================================================
// XdtsTimeTableHeaderItem implementation
//-----------------------------------------------------------------------------

void XdtsTimeTableHeaderItem::read(const QJsonObject& json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  const auto namesArray = json["names"].toArray();

  for (const auto& nameValue : namesArray) {
    m_names.append(nameValue.toString());
  }
}

void XdtsTimeTableHeaderItem::write(QJsonObject& json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray namesArray;

  for (const auto& name : m_names) {
    namesArray.append(name);
  }

  json["names"] = namesArray;
}

//=============================================================================
// XdtsTimeTableItem implementation
//-----------------------------------------------------------------------------

void XdtsTimeTableItem::read(const QJsonObject& json) {
  if (json.contains("fields")) {
    const auto fieldArray = json["fields"].toArray();
    for (int i = 0; i < fieldArray.size(); ++i) {
      XdtsTimeTableFieldItem field;
      field.read(fieldArray[i].toObject());
      m_fields.append(field);
      if (field.isCellField()) {
        m_cellFieldIndex = i;
      }
    }
  }

  m_duration = json["duration"].toInt();
  m_name     = json["name"].toString();

  const auto headerArray = json["timeTableHeaders"].toArray();
  for (int i = 0; i < headerArray.size(); ++i) {
    XdtsTimeTableHeaderItem header;
    header.read(headerArray[i].toObject());
    m_timeTableHeaders.append(header);
    if (header.isCellField()) {
      m_cellHeaderIndex = i;
    }
  }
}

void XdtsTimeTableItem::write(QJsonObject& json) const {
  if (!m_fields.isEmpty()) {
    QJsonArray fieldArray;
    for (const auto& field : m_fields) {
      QJsonObject fieldObject;
      field.write(fieldObject);
      fieldArray.append(fieldObject);
    }
    json["fields"] = fieldArray;
  }

  json["duration"] = m_duration;
  json["name"]     = m_name;

  QJsonArray headerArray;
  for (const auto& header : m_timeTableHeaders) {
    QJsonObject headerObject;
    header.write(headerObject);
    headerArray.append(headerObject);
  }
  json["timeTableHeaders"] = headerArray;
}

QStringList XdtsTimeTableItem::getLevelNames() const {
  assert(m_cellHeaderIndex >= 0);
  const QStringList labels =
      m_timeTableHeaders.at(m_cellHeaderIndex).getLayerNames();

  assert(m_cellFieldIndex >= 0);
  const QList<int> occupiedColumns =
      m_fields.at(m_cellFieldIndex).getOccupiedColumns();

  QStringList result;
  for (const int columnId : occupiedColumns) {
    result.append(labels.at(columnId));
  }
  return result;
}

void XdtsTimeTableItem::build(TXsheet* xsheet, const QString& name,
                              int duration) {
  m_duration = duration;
  m_name     = name;

  QStringList columnLabels;
  XdtsTimeTableFieldItem field;
  field.build(xsheet, columnLabels);
  m_fields.append(field);

  while (!columnLabels.isEmpty() && columnLabels.last().isEmpty()) {
    columnLabels.removeLast();
  }

  if (columnLabels.isEmpty()) {
    m_fields.clear();
    return;
  }

  XdtsTimeTableHeaderItem header;
  header.build(columnLabels);
  m_timeTableHeaders.append(header);
  m_cellHeaderIndex = 0;
  m_cellFieldIndex  = 0;
}

//=============================================================================
// XdtsData implementation
//-----------------------------------------------------------------------------

void XdtsData::read(const QJsonObject& json) {
  if (json.contains("header")) {
    m_header.read(json["header"].toObject());
  }

  const auto tableArray = json["timeTables"].toArray();
  for (const auto& tableValue : tableArray) {
    XdtsTimeTableItem table;
    table.read(tableValue.toObject());
    m_timeTables.append(table);
  }

  m_version = Version(qRound(json["version"].toDouble()));

  if (json.contains("subversion")) {
    m_subversion = json["subversion"].toString();
  }
}

void XdtsData::write(QJsonObject& json) const {
  if (!m_header.isEmpty()) {
    QJsonObject headerObject;
    m_header.write(headerObject);
    json["header"] = headerObject;
  }

  QJsonArray tableArray;
  for (const auto& table : m_timeTables) {
    QJsonObject tableObject;
    table.write(tableObject);
    tableArray.append(tableObject);
  }
  json["timeTables"] = tableArray;

  json["version"] = int(m_version);

  if (!m_subversion.isEmpty()) {
    json["subversion"] = m_subversion;
  }
}

QStringList XdtsData::getLevelNames() const {
  return m_timeTables.at(0).getLevelNames();
}

void XdtsData::build(TXsheet* xsheet, const QString& name, int duration) {
  XdtsTimeTableItem timeTable;
  timeTable.build(xsheet, name, duration);

  if (timeTable.isEmpty()) return;

  m_timeTables.append(timeTable);

  if (::isUextVersion) {
    m_subversion = "p1";
  }
}

//=============================================================================
// XDTS I/O Functions
//-----------------------------------------------------------------------------

bool XdtsIo::loadXdtsScene(ToonzScene* scene, const TFilePath& scenePath) {
  QApplication::restoreOverrideCursor();

  QFile loadFile(scenePath.getQString());
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open XDTS file.");
    return false;
  }

  QByteArray dataArray = loadFile.readAll();
  if (!dataArray.startsWith(identifierStr)) {
    qWarning("File does not start with XDTS identifier string.");
    return false;
  }

  dataArray.remove(0, identifierStr.length());
  const QJsonDocument loadDoc = QJsonDocument::fromJson(dataArray);

  XdtsData xdtsData;
  xdtsData.read(loadDoc.object());

  QStringList levelNames = xdtsData.getLevelNames();
  levelNames.removeDuplicates();
  for (auto& name : levelNames) {
    name = name.trimmed();
  }

  scene->clear();

  auto sceneProject = TProjectManager::instance()->loadSceneProject(scenePath);
  if (!sceneProject) return false;

  scene->setProject(sceneProject);
  const std::string sceneFileName = scenePath.getName() + ".tnz";
  scene->setScenePath(scenePath.getParentDir() + sceneFileName);

  TApp::instance()->getCurrentScene()->setScene(scene);

  XDTSImportPopup popup(levelNames, scene, scenePath, xdtsData.isUextVersion());

  if (popup.exec() == QDialog::Rejected) {
    return false;
  }

  QMap<QString, TXshLevel*> levels;
  try {
    for (const QString& name : levelNames) {
      const QString levelPath = popup.getLevelPath(name);
      TXshLevel* level        = nullptr;

      if (!levelPath.isEmpty()) {
        level = scene->loadLevel(scene->decodeFilePath(TFilePath(levelPath)),
                                 nullptr, name.toStdWString());
      }

      if (!level) {
        const int levelType = Preferences::instance()->getDefLevelType();
        level = scene->createNewLevel(levelType, name.toStdWString());
      }

      levels.insert(name, level);
    }
  } catch (...) {
    return false;
  }

  int tick1Id, tick2Id, keyFrameId, referenceFrameId;
  popup.getMarkerIds(tick1Id, tick2Id, keyFrameId, referenceFrameId);

  const TFrameId tmplFId = scene->getProperties()->formatTemplateFIdForInput();
  TXsheet* xsh           = scene->getXsheet();

  const XdtsTimeTableFieldItem cellField = xdtsData.timeTable().getCellField();
  const XdtsTimeTableHeaderItem cellHeader =
      xdtsData.timeTable().getCellHeader();
  const int duration           = xdtsData.timeTable().getDuration();
  const QStringList layerNames = cellHeader.getLayerNames();
  const QList<int> columns     = cellField.getOccupiedColumns();

  for (int column : columns) {
    const QString levelName = layerNames.at(column).trimmed();
    TXshLevel* level        = levels.value(levelName);
    TXshSimpleLevel* sl     = level ? level->getSimpleLevel() : nullptr;

    QList<int> tick1, tick2, keyFrames, refFrames;
    const QVector<TFrameId> track =
        cellField.getColumnTrack(column, tick1, tick2, keyFrames, refFrames);

    int row = 0;
    for (TFrameId fid : track) {
      if (fid.getNumber() == -1) {  // EMPTY cell
        ++row;
      } else {
        TFrameId fidCopy = fid;  // Create non-const copy for formatFId
        if (sl) sl->formatFId(fidCopy, tmplFId);
        xsh->setCell(row++, column, TXshCell(level, fidCopy));
      }
    }

    // Extend last cell to end of sheet if not empty
    TFrameId lastFid = track.last();  // Non-const variable
    if (lastFid.getNumber() != -1) {
      if (sl) sl->formatFId(lastFid, tmplFId);
      for (; row < duration; ++row) {
        xsh->setCell(row, column, TXshCell(level, TFrameId(lastFid)));
      }
    }

    // Set cell marks
    auto columnPtr  = xsh->getColumn(column);
    auto cellColumn = columnPtr ? columnPtr->getCellColumn() : nullptr;

    if (cellColumn) {
      if (tick1Id >= 0) {
        for (auto tick1f : tick1) cellColumn->setCellMark(tick1f, tick1Id);
      }
      if (tick2Id >= 0) {
        for (auto tick2f : tick2) cellColumn->setCellMark(tick2f, tick2Id);
      }
      if (keyFrameId >= 0) {
        for (auto keyFrame : keyFrames)
          cellColumn->setCellMark(keyFrame, keyFrameId);
      }
      if (referenceFrameId >= 0) {
        for (auto refFrame : refFrames)
          cellColumn->setCellMark(refFrame, referenceFrameId);
      }
    }

    if (auto pegbar = xsh->getStageObject(TStageObjectId::ColumnId(column))) {
      pegbar->setName(levelName.toStdString());
    }
  }

  xsh->updateFrameCount();

  // Adjust preview and output ranges if duration is shorter
  if (duration < xsh->getFrameCount()) {
    scene->getProperties()->getPreviewProperties()->setRange(0, duration - 1,
                                                             1);
    scene->getProperties()->getOutputProperties()->setRange(0, duration - 1, 1);
  }

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

  return true;
}

//=============================================================================
// ExportXDTSCommand implementation
//-----------------------------------------------------------------------------

class ExportXDTSCommand final : public MenuItemHandler {
public:
  ExportXDTSCommand() : MenuItemHandler(MI_ExportXDTS) {}
  void execute() override;
} exportXDTSCommand;

void ExportXDTSCommand::execute() {
  auto scene  = TApp::instance()->getCurrentScene()->getScene();
  auto xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  auto fp     = scene->getScenePath().withType("xdts");

  int duration;
  auto oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;

  if (scene->getTopXsheet() == xsheet && oprop->getRange(from, to, step)) {
    duration = to + 1;
  } else {
    duration = xsheet->getFrameCount();
  }

  // Pre-check if any columns can be exported
  {
    tick1Id          = -1;
    tick2Id          = -1;
    keyFrameId       = -1;
    referenceFrameId = -1;
    exportAllColumn  = true;
    isUextVersion    = false;

    XdtsData pre_xdtsData;
    pre_xdtsData.build(xsheet, QString::fromStdString(fp.getName()), duration);

    if (pre_xdtsData.isEmpty()) {
      DVGui::error(QObject::tr("No columns can be exported."));
      return;
    }
  }

  static GenericSaveFilePopup* savePopup = nullptr;
  static QComboBox* tick1Combo           = nullptr;
  static QComboBox* tick2Combo           = nullptr;
  static QComboBox* keyFrameCombo        = nullptr;
  static QComboBox* referenceFrameCombo  = nullptr;
  static QComboBox* targetColumnCombo    = nullptr;

  auto refreshCellMarkComboItems = [](QComboBox* combo) {
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    int current = combo->count() ? combo->currentData().toInt() : -1;

    combo->clear();
    const auto marks = TApp::instance()
                           ->getCurrentScene()
                           ->getScene()
                           ->getProperties()
                           ->getCellMarks();

    combo->addItem(QObject::tr("None"), -1);
    int currentId = 0;

    for (const auto& mark : marks) {
      const QString label = QString("%1: %2").arg(currentId).arg(mark.name);
      combo->addItem(createColorChipIcon(mark.color), label, currentId);
      ++currentId;
    }

    if (current >= 0) {
      combo->setCurrentIndex(combo->findData(current));
    }
  };

  if (!savePopup) {
    auto customWidget   = new QWidget();
    tick1Combo          = new QComboBox();
    tick2Combo          = new QComboBox();
    keyFrameCombo       = new QComboBox();
    referenceFrameCombo = new QComboBox();

    refreshCellMarkComboItems(tick1Combo);
    refreshCellMarkComboItems(tick2Combo);
    refreshCellMarkComboItems(keyFrameCombo);
    refreshCellMarkComboItems(referenceFrameCombo);

    tick1Combo->setCurrentIndex(tick1Combo->findData(6));
    tick2Combo->setCurrentIndex(tick2Combo->findData(8));
    keyFrameCombo->setCurrentIndex(keyFrameCombo->findData(0));
    referenceFrameCombo->setCurrentIndex(referenceFrameCombo->findData(4));

    targetColumnCombo = new QComboBox();
    targetColumnCombo->addItem(QObject::tr("All columns"), true);
    targetColumnCombo->addItem(QObject::tr("Only active columns"), false);
    targetColumnCombo->setCurrentIndex(targetColumnCombo->findData(true));

    auto warningLabel = new QLabel();
    warningLabel->setFixedSize(20, 20);
    warningLabel->setPixmap(QPixmap(":Resources/warning.svg"));
    warningLabel->setToolTip(QObject::tr(
        "The Keyframe and the Reference Frame symbols will be exported in "
        "an unofficial format,\n"
        "which may not be displayed correctly or may cause errors in "
        "applications other than XDTS Viewer."));

    auto customLayout = new QVBoxLayout();
    customLayout->setContentsMargins(5, 5, 5, 5);
    customLayout->setSpacing(10);

    auto cellMarkGroupBox =
        new QGroupBox(QObject::tr("Cell marks for XDTS symbols"));
    auto cellMarkLayout = new QGridLayout();
    cellMarkLayout->setContentsMargins(10, 10, 10, 10);
    cellMarkLayout->setVerticalSpacing(10);
    cellMarkLayout->setHorizontalSpacing(5);

    cellMarkLayout->addWidget(new QLabel(QObject::tr("Inbetween Symbol1 (O):")),
                              0, 0, Qt::AlignRight | Qt::AlignVCenter);
    cellMarkLayout->addWidget(tick1Combo, 0, 1);
    cellMarkLayout->addItem(new QSpacerItem(10, 1), 0, 2);
    cellMarkLayout->addWidget(new QLabel(QObject::tr("Inbetween Symbol2 (*):")),
                              0, 3, Qt::AlignRight | Qt::AlignVCenter);
    cellMarkLayout->addWidget(tick2Combo, 0, 4);

    cellMarkLayout->addWidget(new QLabel(QObject::tr("Keyframe Symbol:")), 1, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
    cellMarkLayout->addWidget(keyFrameCombo, 1, 1);
    cellMarkLayout->addWidget(
        new QLabel(QObject::tr("Reference Frame Symbol:")), 1, 3,
        Qt::AlignRight | Qt::AlignVCenter);
    cellMarkLayout->addWidget(referenceFrameCombo, 1, 4);
    cellMarkLayout->addWidget(warningLabel, 1, 5);

    cellMarkGroupBox->setLayout(cellMarkLayout);
    customLayout->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

    auto bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);
    bottomLayout->addWidget(new QLabel(QObject::tr("Target column")), 1,
                            Qt::AlignRight | Qt::AlignVCenter);
    bottomLayout->addWidget(targetColumnCombo, 0);

    customLayout->addLayout(bottomLayout);
    customWidget->setLayout(customLayout);

    savePopup = new GenericSaveFilePopup(
        QObject::tr("Export Exchange Digital Time Sheet (XDTS)"), customWidget);
    savePopup->addFilterType("xdts");
  } else {
    refreshCellMarkComboItems(tick1Combo);
    refreshCellMarkComboItems(tick2Combo);
    refreshCellMarkComboItems(keyFrameCombo);
    refreshCellMarkComboItems(referenceFrameCombo);
  }

  if (!scene->isUntitled()) {
    savePopup->setFolder(fp.getParentDir());
  } else {
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());
  }

  savePopup->setFilename(fp.withoutParentDir());
  fp = savePopup->getPath();

  if (fp.isEmpty()) return;

  QFile saveFile(fp.getQString());
  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
    return;
  }

  tick1Id          = tick1Combo->currentData().toInt();
  tick2Id          = tick2Combo->currentData().toInt();
  keyFrameId       = keyFrameCombo->currentData().toInt();
  referenceFrameId = referenceFrameCombo->currentData().toInt();
  exportAllColumn  = targetColumnCombo->currentData().toBool();
  isUextVersion    = false;

  XdtsData xdtsData;
  xdtsData.build(xsheet, QString::fromStdString(fp.getName()), duration);

  if (xdtsData.isEmpty()) {
    DVGui::error(QObject::tr("No columns can be exported."));
    return;
  }

  QJsonObject xdtsObject;
  xdtsData.write(xdtsObject);
  QJsonDocument saveDoc(xdtsObject);
  QByteArray jsonByteArrayData = saveDoc.toJson();
  jsonByteArrayData.prepend(identifierStr + '\n');
  saveFile.write(jsonByteArrayData);

  const QString message =
      QObject::tr("The file %1 has been exported successfully.")
          .arg(QString::fromStdString(fp.getLevelName()));

  const std::vector<QString> buttons = {QObject::tr("OK"),
                                        QObject::tr("Open containing folder")};
  const int ret = DVGui::MsgBox(DVGui::INFORMATION, message, buttons);

  if (ret == 2) {
    const TFilePath folderPath = fp.getParentDir();
    if (TSystem::isUNC(folderPath)) {
      QDesktopServices::openUrl(QUrl(folderPath.getQString()));
    } else {
      QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath.getQString()));
    }
  }
}
