#include "xdtsio.h"

#include "tsystem.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "xdtsimportpopup.h"

#include <iostream>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include <QFile>
#include <QJsonDocument>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
using namespace XdtsIo;
namespace {
static QByteArray identifierStr("exchangeDigitalTimeSheet Save Data");
}
//-----------------------------------------------------------------------------
void XdtsHeader::read(const QJsonObject &json) {
  QRegExp rx("\\d{1,4}");
  // TODO: We could check if the keys are valid
  // before attempting to read them with QJsonObject::contains(),
  // but we assume that they are.
  m_cut = json["cut"].toString();
  if (!rx.exactMatch(m_cut))  // TODO: should handle an error
    std::cout << "The XdtsHeader value \"cut\" does not match the pattern."
              << std::endl;
  m_scene = json["scene"].toString();
  if (!rx.exactMatch(m_scene))
    std::cout << "The XdtsHeader value \"scene\" does not match the pattern."
              << std::endl;
}

void XdtsHeader::write(QJsonObject &json) const {
  json["cut"]   = m_cut;
  json["scene"] = m_scene;
}

//-----------------------------------------------------------------------------

void XdtsFrameDataItem::read(const QJsonObject &json) {
  m_id                   = DataId(qRound(json["id"].toDouble()));
  QJsonArray valuesArray = json["values"].toArray();
  for (int vIndex = 0; vIndex < valuesArray.size(); ++vIndex) {
    m_values.append(valuesArray[vIndex].toString());
  }
}

void XdtsFrameDataItem::write(QJsonObject &json) const {
  json["id"] = int(m_id);
  QJsonArray valuesArray;
  foreach (const QString &value, m_values) { valuesArray.append(value); }
  json["values"] = valuesArray;
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
  foreach (const XdtsFrameDataItem &data, m_data) {
    QJsonObject dataObject;
    data.write(dataObject);
    dataArray.append(dataObject);
  }
  json["data"] = dataArray;

  json["frame"] = m_frame;
}

//-----------------------------------------------------------------------------

QPair<int, int> XdtsTrackFrameItem::frameCellNumber() const {
  return QPair<int, int>(m_frame, m_data[0].getCellNumber());
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
  foreach (const XdtsTrackFrameItem &frame, m_frames) {
    QJsonObject frameObject;
    frame.write(frameObject);
    frameArray.append(frameObject);
  }
  json["frames"] = frameArray;

  json["trackNo"] = m_trackNo;
}
//-----------------------------------------------------------------------------

bool frameLessThan(const QPair<int, int> &v1, const QPair<int, int> &v2) {
  return v1.first < v2.first;
}

QVector<int> XdtsFieldTrackItem::getCellNumberTrack() const {
  QList<QPair<int, int>> frameCellNumbers;
  for (const XdtsTrackFrameItem &frame : m_frames)
    frameCellNumbers.append(frame.frameCellNumber());
  std::sort(frameCellNumbers.begin(), frameCellNumbers.end(), frameLessThan);

  QVector<int> cells;
  int currentFrame = 0;
  for (QPair<int, int> &frameCellNumber : frameCellNumbers) {
    while (currentFrame < frameCellNumber.first) {
      cells.append((cells.isEmpty()) ? 0 : cells.last());
      currentFrame++;
    }
    // ignore sheet symbols for now
    int cellNumber = frameCellNumber.second;
    if (cellNumber == -1)
      cells.append((cells.isEmpty()) ? 0 : cells.last());
    else
      cells.append(cellNumber);
    currentFrame++;
  }
  return cells;
}

QString XdtsFieldTrackItem::build(TXshCellColumn *column, int duration) {
  // register the firstly-found level
  TXshSimpleLevel *level = nullptr;
  TXshCell prevCell;
  int r0, r1;
  column->getRange(r0, r1);
  if (r0 > 0) addFrame(0, 0);
  for (int row = r0; row <= r1; row++) {
    TXshCell cell = column->getCell(row);
    // try to register the level
    if (!level && cell.getSimpleLevel()) level = cell.getSimpleLevel();
    // if the level does not match with the registered one,
    // handle as the empty cell
    if (!level || cell.m_level != level) cell = TXshCell();
    // continue if the cell is continuous
    if (prevCell == cell) continue;

    if (cell.isEmpty())
      addFrame(row, 0);
    else
      addFrame(row, cell.getFrameId().getNumber());

    prevCell = cell;
  }
  if (r1 + 1 < duration) addFrame(r1 + 1, 0);
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
  foreach (const XdtsFieldTrackItem &track, m_tracks) {
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

QVector<int> XdtsTimeTableFieldItem::getColumnTrack(int col) const {
  for (const XdtsFieldTrackItem &track : m_tracks) {
    if (track.getTrackNo() != col) continue;
    return track.getCellNumberTrack();
  }
  return QVector<int>();
}

void XdtsTimeTableFieldItem::build(TXsheet *xsheet, int duration,
                                   QStringList &columnLabels) {
  m_fieldId = CELL;
  for (int col = 0; col < xsheet->getFirstFreeColumnIndex(); col++) {
    if (xsheet->isColumnEmpty(col)) {
      columnLabels.append("");
      continue;
    }
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    if (!column) {
      columnLabels.append("");
      continue;
    }
    XdtsFieldTrackItem track(col);
    columnLabels.append(track.build(column, duration));
    if (!track.isEmpty()) m_tracks.append(track);
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
  foreach (const QString name, m_names) { namesArray.append(name); }
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
    foreach (const XdtsTimeTableFieldItem &field, m_fields) {
      QJsonObject fieldObject;
      field.write(fieldObject);
      fieldArray.append(fieldObject);
    }
    json["fields"] = fieldArray;
  }
  json["duration"] = m_duration;
  json["name"]     = m_name;
  QJsonArray headerArray;
  foreach (const XdtsTimeTableHeaderItem header, m_timeTableHeaders) {
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

void XdtsTimeTableItem::build(TXsheet *xsheet, QString name) {
  m_duration = xsheet->getFrameCount();
  m_name     = name;
  QStringList columnLabels;
  XdtsTimeTableFieldItem field;
  field.build(xsheet, m_duration, columnLabels);
  m_fields.append(field);
  while (columnLabels.last().isEmpty()) columnLabels.removeLast();
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
}

void XdtsData::write(QJsonObject &json) const {
  if (!m_header.isEmpty()) {
    QJsonObject headerObject;
    m_header.write(headerObject);
    json["header"] = headerObject;
  }
  QJsonArray tableArray;
  foreach (const XdtsTimeTableItem &table, m_timeTables) {
    QJsonObject tableObject;
    table.write(tableObject);
    tableArray.append(tableObject);
  }
  json["timeTables"] = tableArray;
  json["version"]    = int(m_version);
}

QStringList XdtsData::getLevelNames() const {
  // currently support only the first page of time tables
  return m_timeTables.at(0).getLevelNames();
}

void XdtsData::build(TXsheet *xsheet, QString name) {
  XdtsTimeTableItem timeTable;
  timeTable.build(xsheet, name);
  m_timeTables.append(timeTable);
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

  scene->clear();

  TProjectManager *pm    = TProjectManager::instance();
  TProjectP sceneProject = pm->loadSceneProject(scenePath);
  if (!sceneProject) return false;
  scene->setProject(sceneProject.getPointer());
  std::string sceneFileName = scenePath.getName() + ".tnz";
  scene->setScenePath(scenePath.getParentDir() + sceneFileName);
  TApp::instance()->getCurrentScene()->setScene(scene);

  XDTSImportPopup popup(levelNames, scene, scenePath);

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

  TXsheet *xsh                       = scene->getXsheet();
  XdtsTimeTableFieldItem cellField   = xdtsData.timeTable().getCellField();
  XdtsTimeTableHeaderItem cellHeader = xdtsData.timeTable().getCellHeader();
  int duration                       = xdtsData.timeTable().getDuration();
  QStringList layerNames             = cellHeader.getLayerNames();
  QList<int> columns                 = cellField.getOccupiedColumns();
  for (int column : columns) {
    QString levelName  = layerNames.at(column);
    TXshLevel *level   = levels.value(levelName);
    QVector<int> track = cellField.getColumnTrack(column);

    int row = 0;
    std::vector<TFrameId>::iterator it;
    for (int f : track) {
      if (f == 0)  // empty cell
        row++;
      else
        xsh->setCell(row++, column, TXshCell(level, TFrameId(f)));
    }
    // if the last cell is not "SYMBOL_NULL_CELL", continue the cell
    // to the end of the sheet
    int lastFid = track.last();
    if (lastFid != 0) {
      for (; row < duration; row++)
        xsh->setCell(row, column, TXshCell(level, TFrameId(lastFid)));
    }
  }
  xsh->updateFrameCount();

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
  if (TSystem::doesExistFileOrLevel(fp)) {
    QString question =
        QObject::tr("The file %1 already exists.\nDo you want to overwrite it?")
            .arg(toQString(fp));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return;
  }

  XdtsData xdtsData;
  xdtsData.build(xsheet, QString::fromStdString(fp.getName()));

  QFile saveFile(fp.getQString());

  if (!saveFile.open(QIODevice::WriteOnly)) {
    qWarning("Couldn't open save file.");
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
