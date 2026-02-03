

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
static QByteArray identifierStr("exchangeDigitalTimeSheet Save Data");

QIcon getColorChipIcon(TPixel32 color) {
  QPixmap pm(15, 15);
  pm.fill(QColor(color.r, color.g, color.b));
  return QIcon(pm);
}

int _tick1Id          = -1;
int _tick2Id          = -1;
int _keyFrameId       = -1;
int _referenceFrameId = -1;
bool _isUextVersion   = false;  // set to true when XDTS is written in UEXT
                                // (unofficial extension) version
bool _exportAllColumn = true;

const QString OPTION_KEYFRAME       = "OPTION_KEYFRAME";
const QString OPTION_REFERENCEFRAME = "OPTION_REFERENCEFRAME";
}  // namespace
//-----------------------------------------------------------------------------
void XdtsHeader::read(const QJsonObject &json) {
  QRegularExpression rx("^\\d{1,4}$");
  // TODO: We could check if the keys are valid
  // before attempting to read them with QJsonObject::contains(),
  // but we assume that they are.
  m_cut = json["cut"].toString();
  if (!rx.match(m_cut).hasMatch())  // TODO: should handle an error
    qWarning()
        << "The XdtsHeader value 'cut' value does not match expected pattern:"
        << m_cut;
  m_scene = json["scene"].toString();
  if (!rx.match(m_scene).hasMatch())
    qWarning()
        << "The XdtsHeader value 'scene' value does not match expected pattern:"
        << m_scene;
}

void XdtsHeader::write(QJsonObject &json) const {
  json["cut"]   = m_cut;
  json["scene"] = m_scene;
}

//-----------------------------------------------------------------------------

TFrameId XdtsFrameDataItem::str2Fid(const QString &str) const {
  if (str.isEmpty()) return TFrameId::EMPTY_FRAME;
  bool ok;
  int frame = str.toInt(&ok);
  if (ok) return TFrameId(frame);

  QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
  QRegularExpression rx(regExpStr);
  QRegularExpressionMatch match = rx.match(str);
  if (!match.hasMatch()) return TFrameId();

  if (match.captured(2).isEmpty())
    return TFrameId(match.captured(1).toInt());
  else
    return TFrameId(match.captured(1).toInt(), match.captured(2));
}

QString XdtsFrameDataItem::fid2Str(const TFrameId &fid) const {
  if (fid.getNumber() == -1)
    return QString("SYMBOL_NULL_CELL");
  else if (fid.getNumber() == SYMBOL_TICK_1)
    return QString("SYMBOL_TICK_1");
  else if (fid.getNumber() == SYMBOL_TICK_2)
    return QString("SYMBOL_TICK_2");
  else if (fid.getLetter().isEmpty())
    return QString::number(fid.getNumber());
  return QString::number(fid.getNumber()) + fid.getLetter();
}

void XdtsFrameDataItem::read(const QJsonObject &json) {
  m_id                   = DataId(qRound(json["id"].toDouble()));
  QJsonArray valuesArray = json["values"].toArray();
  for (const QJsonValue &value : valuesArray) {
    m_values.append(value.toString());
  }
  if (json.contains("options")) {
    QJsonArray optionsArray = json["options"].toArray();
    for (const QJsonValue &option : optionsArray) {
      m_options.append(option.toString());
    }
  }
}

void XdtsFrameDataItem::write(QJsonObject &json) const {
  json["id"] = int(m_id);
  QJsonArray valuesArray;
  for (const QString &value : m_values) {
    valuesArray.append(value);
  }
  json["values"] = valuesArray;

  if (!m_options.isEmpty()) {
    QJsonArray optionsArray;
    for (const QString &option : m_options) {
      optionsArray.append(option);
    }
    json["options"] = optionsArray;
  }
}

XdtsFrameDataItem::FrameInfo XdtsFrameDataItem::getFrameInfo() const {
  // int getCellNumber() const {
  if (m_values.isEmpty())
    return {TFrameId(-1), m_options};  // EMPTY
                                       // if (m_values.isEmpty()) return 0;
  QString val = m_values.at(0);
  if (val == "SYMBOL_NULL_CELL")
    return {TFrameId(-1), m_options};  // EMPTY
                                       // ignore sheet symbols for now
  else if (val == "SYMBOL_HYPHEN")
    return {TFrameId(-2), m_options};  // IGNORE
  else if (val == "SYMBOL_TICK_1")
    return {TFrameId(SYMBOL_TICK_1), m_options};
  else if (val == "SYMBOL_TICK_2")
    return {TFrameId(SYMBOL_TICK_2), m_options};
  // return cell number
  return {str2Fid(m_values.at(0)), m_options};
}

//-----------------------------------------------------------------------------
void XdtsTrackFrameItem::read(const QJsonObject &json) {
  QJsonArray dataArray = json["data"].toArray();
  for (int dataIndex = 0; dataIndex < dataArray.size(); ++dataIndex) {
    QJsonObject dataObject = dataArray[dataIndex].toObject();
    XdtsFrameDataItem data;
    data.read(dataObject);
    m_data.append(data);
  }
  m_frame = json["frame"].toInt();
}

void XdtsTrackFrameItem::write(QJsonObject &json) const {
  QJsonArray dataArray;
  for (const XdtsFrameDataItem &data : m_data) {
    QJsonObject dataObject;
    data.write(dataObject);
    dataArray.append(dataObject);
  }
  json["data"] = dataArray;

  json["frame"] = m_frame;
}

//-----------------------------------------------------------------------------

QPair<int, XdtsFrameDataItem::FrameInfo> XdtsTrackFrameItem::frameFinfo()
    const {
  return QPair<int, XdtsFrameDataItem::FrameInfo>(m_frame,
                                                  m_data[0].getFrameInfo());
}

//-----------------------------------------------------------------------------
void XdtsFieldTrackItem::read(const QJsonObject &json) {
  QJsonArray frameArray = json["frames"].toArray();
  for (int frameIndex = 0; frameIndex < frameArray.size(); ++frameIndex) {
    QJsonObject frameObject = frameArray[frameIndex].toObject();
    XdtsTrackFrameItem frame;
    frame.read(frameObject);
    m_frames.append(frame);
  }
  m_trackNo = json["trackNo"].toInt();
}

void XdtsFieldTrackItem::write(QJsonObject &json) const {
  QJsonArray frameArray;
  for (const XdtsTrackFrameItem &frame : m_frames) {
    QJsonObject frameObject;
    frame.write(frameObject);
    frameArray.append(frameObject);
  }
  json["frames"] = frameArray;

  json["trackNo"] = m_trackNo;
}
//-----------------------------------------------------------------------------

static bool frameLessThan(const QPair<int, XdtsFrameDataItem::FrameInfo> &v1,
                          const QPair<int, XdtsFrameDataItem::FrameInfo> &v2) {
  return v1.first < v2.first;
}

QVector<TFrameId> XdtsFieldTrackItem::getCellFrameIdTrack(
    QList<int> &tick1, QList<int> &tick2, QList<int> &keyFrames,
    QList<int> &refFrames) const {
  QList<QPair<int, XdtsFrameDataItem::FrameInfo>> frameFinfos;
  for (const XdtsTrackFrameItem &frame : m_frames)
    frameFinfos.append(frame.frameFinfo());
  std::sort(frameFinfos.begin(), frameFinfos.end(), frameLessThan);

  QVector<TFrameId> cells;
  int currentFrame       = 0;
  TFrameId initialNumber = TFrameId();
  for (QPair<int, XdtsFrameDataItem::FrameInfo> &frameFinfo : frameFinfos) {
    while (currentFrame < frameFinfo.first) {
      cells.append((cells.isEmpty()) ? initialNumber : cells.last());
      currentFrame++;
    }
    // CSP may export negative frame data (although it is not allowed in XDTS
    // format specification) so handle such case.
    if (frameFinfo.first < 0) {
      initialNumber = frameFinfo.second.frameId;
      continue;
    }
    // ignore sheet symbols for now
    TFrameId cellFid = frameFinfo.second.frameId;
    if (cellFid.getNumber() == -2)  // IGNORE case
      cells.append((cells.isEmpty()) ? TFrameId(-1) : cells.last());
    else if (cellFid.getNumber() ==
             XdtsFrameDataItem::SYMBOL_TICK_1) {  // SYMBOL_TICK_1
      cells.append((cells.isEmpty()) ? TFrameId(-1) : cells.last());
      tick1.append(currentFrame);
    } else if (cellFid.getNumber() ==
               XdtsFrameDataItem::SYMBOL_TICK_2) {  // SYMBOL_TICK_2
      cells.append((cells.isEmpty()) ? TFrameId(-1) : cells.last());
      tick2.append(currentFrame);
    } else
      cells.append(cellFid);

    QStringList options = frameFinfo.second.options;
    if (options.contains(OPTION_KEYFRAME)) keyFrames.append(currentFrame);
    if (options.contains(OPTION_REFERENCEFRAME)) refFrames.append(currentFrame);

    currentFrame++;
  }
  return cells;
}

QString XdtsFieldTrackItem::build(TXshCellColumn *column) {
  // register the firstly-found level
  TXshLevel *level = nullptr;
  TXshCell prevCell;
  int r0, r1;
  column->getRange(r0, r1);
  if (r0 > 0) addFrame(0, TFrameId(-1));
  for (int row = r0; row <= r1; row++) {
    TXshCell cell = column->getCell(row);
    // try to register the level. simple levels and sub xsheet levels are
    // exported
    if (!level) level = cell.getSimpleLevel();
    if (!level) level = cell.getChildLevel();
    // if the level does not match with the registered one,
    // handle as the empty cell
    if (!level || cell.m_level != level) cell = TXshCell();
    // continue if the cell is continuous
    if (prevCell == cell) {
      // cell mark to ticks
      if (_tick1Id >= 0 && column->getCellMark(row) == _tick1Id)
        addFrame(row, TFrameId(XdtsFrameDataItem::SYMBOL_TICK_1));
      else if (_tick2Id >= 0 && column->getCellMark(row) == _tick2Id)
        addFrame(row, TFrameId(XdtsFrameDataItem::SYMBOL_TICK_2));
      continue;
    }
    if (cell.isEmpty())
      addFrame(row, TFrameId(-1));
    else {
      QList<QString> options;
      // cell mark for keyframe / reference frame symbols
      if (_keyFrameId >= 0 && column->getCellMark(row) == _keyFrameId)
        options.append(OPTION_KEYFRAME);
      else if (_referenceFrameId >= 0 &&
               column->getCellMark(row) == _referenceFrameId)
        options.append(OPTION_REFERENCEFRAME);
      if (!options.isEmpty()) _isUextVersion = true;
      addFrame(row, cell.getFrameId(), options);
    }

    prevCell = cell;
  }
  addFrame(r1 + 1, TFrameId(-1));
  if (level)
    return QString::fromStdWString(level->getName());
  else {
    m_frames.clear();
    return QString();
  }
}
//-----------------------------------------------------------------------------

void XdtsTimeTableFieldItem::read(const QJsonObject &json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  QJsonArray trackArray = json["tracks"].toArray();
  for (int trackIndex = 0; trackIndex < trackArray.size(); ++trackIndex) {
    QJsonObject trackObject = trackArray[trackIndex].toObject();
    XdtsFieldTrackItem track;
    track.read(trackObject);
    m_tracks.append(track);
  }
}

void XdtsTimeTableFieldItem::write(QJsonObject &json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray trackArray;
  for (const XdtsFieldTrackItem &track : m_tracks) {
    QJsonObject trackObject;
    track.write(trackObject);
    trackArray.append(trackObject);
  }
  json["tracks"] = trackArray;
}

QList<int> XdtsTimeTableFieldItem::getOccupiedColumns() const {
  QList<int> ret;
  for (const XdtsFieldTrackItem &track : m_tracks) {
    if (!track.isEmpty()) ret.append(track.getTrackNo());
  }
  return ret;
}

QVector<TFrameId> XdtsTimeTableFieldItem::getColumnTrack(
    int col, QList<int> &tick1, QList<int> &tick2, QList<int> &keyFrames,
    QList<int> &refFrames) const {
  for (const XdtsFieldTrackItem &track : m_tracks) {
    if (track.getTrackNo() != col) continue;
    return track.getCellFrameIdTrack(tick1, tick2, keyFrames, refFrames);
  }
  return QVector<TFrameId>();
}

void XdtsTimeTableFieldItem::build(TXsheet *xsheet, QStringList &columnLabels) {
  m_fieldId     = CELL;
  int exportCol = 0;
  for (int col = 0; col < xsheet->getFirstFreeColumnIndex(); col++) {
    if (xsheet->isColumnEmpty(col)) {
      columnLabels.append("");
      exportCol++;
      continue;
    }
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    // skip non-cell column
    if (!column) {
      continue;
    }
    // skip inactive column
    if (!_exportAllColumn && !column->isPreviewVisible()) {
      continue;
    }
    XdtsFieldTrackItem track(exportCol);
    columnLabels.append(track.build(column));
    if (!track.isEmpty()) m_tracks.append(track);
    exportCol++;
  }
}
//-----------------------------------------------------------------------------

void XdtsTimeTableHeaderItem::read(const QJsonObject &json) {
  m_fieldId             = FieldId(qRound(json["fieldId"].toDouble()));
  QJsonArray namesArray = json["names"].toArray();
  for (int nIndex = 0; nIndex < namesArray.size(); ++nIndex) {
    m_names.append(namesArray[nIndex].toString());
  }
}

void XdtsTimeTableHeaderItem::write(QJsonObject &json) const {
  json["fieldId"] = int(m_fieldId);
  QJsonArray namesArray;
  for (const QString &name : m_names) {
    namesArray.append(name);
  }
  json["names"] = namesArray;
}

void XdtsTimeTableHeaderItem::build(QStringList &columnLabels) {
  m_names.append(columnLabels);
}

//-----------------------------------------------------------------------------

void XdtsTimeTableItem::read(const QJsonObject &json) {
  if (json.contains("fields")) {
    QJsonArray fieldArray = json["fields"].toArray();
    for (int fieldIndex = 0; fieldIndex < fieldArray.size(); ++fieldIndex) {
      QJsonObject fieldObject = fieldArray[fieldIndex].toObject();
      XdtsTimeTableFieldItem field;
      field.read(fieldObject);
      m_fields.append(field);
      if (field.isCellField()) m_cellFieldIndex = fieldIndex;
    }
  }
  m_duration             = json["duration"].toInt();
  m_name                 = json["name"].toString();
  QJsonArray headerArray = json["timeTableHeaders"].toArray();
  for (int headerIndex = 0; headerIndex < headerArray.size(); ++headerIndex) {
    QJsonObject headerObject = headerArray[headerIndex].toObject();
    XdtsTimeTableHeaderItem header;
    header.read(headerObject);
    m_timeTableHeaders.append(header);
    if (header.isCellField()) m_cellHeaderIndex = headerIndex;
  }
}

void XdtsTimeTableItem::write(QJsonObject &json) const {
  if (!m_fields.isEmpty()) {
    QJsonArray fieldArray;
    for (const XdtsTimeTableFieldItem &field : m_fields) {
      QJsonObject fieldObject;
      field.write(fieldObject);
      fieldArray.append(fieldObject);
    }
    json["fields"] = fieldArray;
  }
  json["duration"] = m_duration;
  json["name"]     = m_name;
  QJsonArray headerArray;
  for (const XdtsTimeTableHeaderItem &header : m_timeTableHeaders) {
    QJsonObject headerObject;
    header.write(headerObject);
    headerArray.append(headerObject);
  }
  json["timeTableHeaders"] = headerArray;
}

QStringList XdtsTimeTableItem::getLevelNames() const {
  // obtain column labels from the header
  assert(m_cellHeaderIndex >= 0);
  QStringList labels = m_timeTableHeaders.at(m_cellHeaderIndex).getLayerNames();
  // obtain occupied column numbers in the field
  assert(m_cellFieldIndex >= 0);
  QList<int> occupiedColumns =
      m_fields.at(m_cellFieldIndex).getOccupiedColumns();
  // return the names of occupied columns
  QStringList ret;
  for (const int columnId : occupiedColumns) ret.append(labels.at(columnId));
  return ret;
}

void XdtsTimeTableItem::build(TXsheet *xsheet, QString name, int duration) {
  m_duration = duration;
  m_name     = name;
  QStringList columnLabels;
  XdtsTimeTableFieldItem field;
  field.build(xsheet, columnLabels);
  m_fields.append(field);
  while (!columnLabels.isEmpty() && columnLabels.last().isEmpty())
    columnLabels.removeLast();
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
//-----------------------------------------------------------------------------

void XdtsData::read(const QJsonObject &json) {
  if (json.contains("header")) {
    QJsonObject headerObject = json["header"].toObject();
    m_header.read(headerObject);
  }
  QJsonArray tableArray = json["timeTables"].toArray();
  for (int tableIndex = 0; tableIndex < tableArray.size(); ++tableIndex) {
    QJsonObject tableObject = tableArray[tableIndex].toObject();
    XdtsTimeTableItem table;
    table.read(tableObject);
    m_timeTables.append(table);
  }
  m_version = Version(qRound(json["version"].toDouble()));

  if (json.contains("subversion")) m_subversion = json["subversion"].toString();
}

void XdtsData::write(QJsonObject &json) const {
  if (!m_header.isEmpty()) {
    QJsonObject headerObject;
    m_header.write(headerObject);
    json["header"] = headerObject;
  }
  QJsonArray tableArray;
  for (const XdtsTimeTableItem &table : m_timeTables) {
    QJsonObject tableObject;
    table.write(tableObject);
    tableArray.append(tableObject);
  }
  json["timeTables"] = tableArray;
  json["version"]    = int(m_version);

  if (!m_subversion.isEmpty()) json["subversion"] = m_subversion;
}

QStringList XdtsData::getLevelNames() const {
  // currently support only the first page of time tables
  return m_timeTables.at(0).getLevelNames();
}

void XdtsData::build(TXsheet *xsheet, QString name, int duration) {
  XdtsTimeTableItem timeTable;
  timeTable.build(xsheet, name, duration);
  if (timeTable.isEmpty()) return;
  m_timeTables.append(timeTable);

  if (_isUextVersion) m_subversion = "p1";
}

//-----------------------------------------------------------------------------

bool XdtsIo::loadXdtsScene(ToonzScene *scene, const TFilePath &scenePath) {
  QApplication::restoreOverrideCursor();
  // read the Json file
  QFile loadFile(scenePath.getQString());
  if (!loadFile.open(QIODevice::ReadOnly)) {
    qWarning("Couldn't open save file.");
    return false;
  }

  QByteArray dataArray = loadFile.readAll();

  if (!dataArray.startsWith(identifierStr)) {
    qWarning("The first line does not start with XDTS identifier string.");
    return false;
  }
  // remove identifier
  dataArray.remove(0, identifierStr.length());

  QJsonDocument loadDoc(QJsonDocument::fromJson(dataArray));

  XdtsData xdtsData;
  xdtsData.read(loadDoc.object());

  // obtain level names
  QStringList levelNames = xdtsData.getLevelNames();
  // in case multiple columns have the same name
  levelNames.removeDuplicates();
  for (auto &name : levelNames) name = name.trimmed();

  scene->clear();

  auto sceneProject = TProjectManager::instance()->loadSceneProject(scenePath);
  if (!sceneProject) return false;
  scene->setProject(sceneProject);
  std::string sceneFileName = scenePath.getName() + ".tnz";
  scene->setScenePath(scenePath.getParentDir() + sceneFileName);
  // set the current scene here in order to use $scenefolder node properly
  // in the file browser which opens from XDTSImportPopup
  TApp::instance()->getCurrentScene()->setScene(scene);

  XDTSImportPopup popup(levelNames, scene, scenePath, xdtsData.isUextVersion());

  int ret = popup.exec();
  if (ret == QDialog::Rejected) return false;

  QMap<QString, TXshLevel *> levels;
  try {
    for (QString name : levelNames) {
      QString levelPath = popup.getLevelPath(name);
      TXshLevel *level  = nullptr;
      if (!levelPath.isEmpty())
        level = scene->loadLevel(scene->decodeFilePath(TFilePath(levelPath)),
                                 nullptr, name.toStdWString());
      if (!level) {
        int levelType = Preferences::instance()->getDefLevelType();
        level         = scene->createNewLevel(levelType, name.toStdWString());
      }
      levels.insert(name, level);
    }
  } catch (...) {
    return false;
  }

  int tick1Id, tick2Id, keyFrameId, referenceFrameId;
  popup.getMarkerIds(tick1Id, tick2Id, keyFrameId, referenceFrameId);

  TFrameId tmplFId = scene->getProperties()->formatTemplateFIdForInput();

  TXsheet *xsh                       = scene->getXsheet();
  XdtsTimeTableFieldItem cellField   = xdtsData.timeTable().getCellField();
  XdtsTimeTableHeaderItem cellHeader = xdtsData.timeTable().getCellHeader();
  int duration                       = xdtsData.timeTable().getDuration();
  QStringList layerNames             = cellHeader.getLayerNames();
  QList<int> columns                 = cellField.getOccupiedColumns();
  for (int column : columns) {
    QString levelName   = layerNames.at(column);
    TXshLevel *level    = levels.value(levelName.trimmed());
    TXshSimpleLevel *sl = level->getSimpleLevel();
    QList<int> tick1, tick2, keyFrames, refFrames;
    QVector<TFrameId> track =
        cellField.getColumnTrack(column, tick1, tick2, keyFrames, refFrames);

    int row = 0;
    std::vector<TFrameId>::iterator it;
    for (TFrameId fid : track) {
      if (fid.getNumber() == -1)  // EMPTY cell case
        row++;
      else {
        // modify frameId to be with the same frame format as existing frames
        if (sl) sl->formatFId(fid, tmplFId);
        xsh->setCell(row++, column, TXshCell(level, fid));
      }
    }
    // if the last cell is not "SYMBOL_NULL_CELL", continue the cell
    // to the end of the sheet
    TFrameId lastFid = track.last();
    if (lastFid.getNumber() != -1) {
      // modify frameId to be with the same frame format as existing frames
      if (sl) sl->formatFId(lastFid, tmplFId);
      for (; row < duration; row++)
        xsh->setCell(row, column, TXshCell(level, TFrameId(lastFid)));
    }

    // set cell marks
    TXshCellColumn *cellColumn = nullptr;
    if (xsh->getColumn(column))
      cellColumn = xsh->getColumn(column)->getCellColumn();
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

    TStageObject *pegbar =
        xsh->getStageObject(TStageObjectId::ColumnId(column));
    if (pegbar) pegbar->setName(levelName.toStdString());
  }
  xsh->updateFrameCount();

  // if the duration is shorter than frame count, then set it both in
  // preview range and output range.
  if (duration < xsh->getFrameCount()) {
    scene->getProperties()->getPreviewProperties()->setRange(0, duration - 1,
                                                             1);
    scene->getProperties()->getOutputProperties()->setRange(0, duration - 1, 1);
  }

  // emit signal here for updating the frame slider range of flip console
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  // update xsheet viewer
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  return true;
}

class ExportXDTSCommand final : public MenuItemHandler {
public:
  ExportXDTSCommand() : MenuItemHandler(MI_ExportXDTS) {}
  void execute() override;
} exportXDTSCommand;

void ExportXDTSCommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsheet   = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("xdts");

  // if the current xsheet is top xsheet in the scene and the output
  // frame range is specified, set the "to" frame value as duration
  int duration;
  TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;
  if (scene->getTopXsheet() == xsheet && oprop->getRange(from, to, step))
    duration = to + 1;
  else
    duration = xsheet->getFrameCount();

  {
    _tick1Id          = -1;
    _tick2Id          = -1;
    _keyFrameId       = -1;
    _referenceFrameId = -1;
    _exportAllColumn  = true;
    _isUextVersion    = false;
    XdtsData pre_xdtsData;
    pre_xdtsData.build(xsheet, QString::fromStdString(fp.getName()), duration);
    if (pre_xdtsData.isEmpty()) {
      DVGui::error(QObject::tr("No columns can be exported."));
      return;
    }
  }

  static GenericSaveFilePopup *savePopup = 0;
  static QComboBox *tick1Id              = nullptr;
  static QComboBox *tick2Id              = nullptr;
  static QComboBox *keyFrameId           = nullptr;
  static QComboBox *referenceFrameId     = nullptr;
  static QComboBox *targetColumnCombo    = nullptr;

  auto refreshCellMarkComboItems = [](QComboBox *combo) {
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    int current = -1;
    if (combo->count()) current = combo->currentData().toInt();

    combo->clear();
    QList<TSceneProperties::CellMark> marks = TApp::instance()
                                                  ->getCurrentScene()
                                                  ->getScene()
                                                  ->getProperties()
                                                  ->getCellMarks();
    combo->addItem(QObject::tr("None"), -1);
    int curId = 0;
    for (auto mark : marks) {
      QString label = QString("%1: %2").arg(curId).arg(mark.name);
      combo->addItem(getColorChipIcon(mark.color), label, curId);
      curId++;
    }
    if (current >= 0) combo->setCurrentIndex(combo->findData(current));
  };

  if (!savePopup) {
    // create custom widget
    QWidget *customWidget = new QWidget();
    tick1Id               = new QComboBox();
    tick2Id               = new QComboBox();
    keyFrameId            = new QComboBox();
    referenceFrameId      = new QComboBox();
    refreshCellMarkComboItems(tick1Id);
    refreshCellMarkComboItems(tick2Id);
    refreshCellMarkComboItems(keyFrameId);
    refreshCellMarkComboItems(referenceFrameId);
    tick1Id->setCurrentIndex(tick1Id->findData(6));
    tick2Id->setCurrentIndex(tick2Id->findData(8));
    keyFrameId->setCurrentIndex(keyFrameId->findData(0));
    referenceFrameId->setCurrentIndex(referenceFrameId->findData(4));
    targetColumnCombo = new QComboBox();
    targetColumnCombo->addItem(QObject::tr("All columns"), true);
    targetColumnCombo->addItem(QObject::tr("Only active columns"), false);
    targetColumnCombo->setCurrentIndex(targetColumnCombo->findData(true));

    QLabel *warningLabel = new QLabel();
    warningLabel->setFixedSize(20, 20);
    warningLabel->setPixmap(QPixmap(":Resources/warning.svg"));
    warningLabel->setToolTip(QObject::tr(
        "The Keyframe and the Reference Frame symbols will be exported in "
        "an unofficial format,\n"
        "which may not be displayed correctly or may cause errors in "
        "applications other than XDTS Viewer."));
    // The original image and reference symbols will be exported in an
    // unofficial format, which may not be displayed correctly or may cause
    // errors in applications other than XDTS Viewer.

    QVBoxLayout *customLay = new QVBoxLayout();
    customLay->setContentsMargins(5, 5, 5, 5);
    customLay->setSpacing(10);
    {
      QGroupBox *cellMarkGroupBox =
          new QGroupBox(QObject::tr("Cell marks for XDTS symbols"));

      QGridLayout *cellMarkLay = new QGridLayout();
      cellMarkLay->setContentsMargins(10, 10, 10, 10);
      cellMarkLay->setVerticalSpacing(10);
      cellMarkLay->setHorizontalSpacing(5);
      {
        cellMarkLay->addWidget(
            new QLabel(QObject::tr("Inbetween Symbol1 (O):")), 0, 0,
            Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(tick1Id, 0, 1);
        cellMarkLay->addItem(new QSpacerItem(10, 1), 0, 2);
        cellMarkLay->addWidget(
            new QLabel(QObject::tr("Inbetween Symbol2 (*):")), 0, 3,
            Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(tick2Id, 0, 4);

        cellMarkLay->addWidget(new QLabel(QObject::tr("Keyframe Symbol:")), 1,
                               0, Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(keyFrameId, 1, 1);
        cellMarkLay->addWidget(
            new QLabel(QObject::tr("Reference Frame Symbol:")), 1, 3,
            Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(referenceFrameId, 1, 4);
        cellMarkLay->addWidget(warningLabel, 1, 5);
      }
      cellMarkGroupBox->setLayout(cellMarkLay);
      customLay->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

      QHBoxLayout *bottomLay = new QHBoxLayout();
      bottomLay->setContentsMargins(0, 0, 0, 0);
      bottomLay->setSpacing(10);
      {
        bottomLay->addWidget(new QLabel(QObject::tr("Target column")), 1,
                             Qt::AlignRight | Qt::AlignVCenter);
        bottomLay->addWidget(targetColumnCombo, 0);
      }
      customLay->addLayout(bottomLay);
    }
    customWidget->setLayout(customLay);

    savePopup = new GenericSaveFilePopup(
        QObject::tr("Export Exchange Digital Time Sheet (XDTS)"), customWidget);
    savePopup->addFilterType("xdts");
  } else {
    refreshCellMarkComboItems(tick1Id);
    refreshCellMarkComboItems(tick2Id);
    refreshCellMarkComboItems(keyFrameId);
    refreshCellMarkComboItems(referenceFrameId);
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

  _tick1Id          = tick1Id->currentData().toInt();
  _tick2Id          = tick2Id->currentData().toInt();
  _keyFrameId       = keyFrameId->currentData().toInt();
  _referenceFrameId = referenceFrameId->currentData().toInt();
  _exportAllColumn  = targetColumnCombo->currentData().toBool();
  _isUextVersion    = false;
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
