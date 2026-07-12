#include "sxfio.h"

#include "tapp.h"
#include "tsystem.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"

#include "xdtsimportpopup.h"
#include "filebrowserpopup.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUrl>
#include <QDebug>
#include <QTextCodec>
#include <QByteArray>
#include <QDataStream>
#include <stdexcept>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QDesktopServices>

using namespace SxfIo;

namespace {
const quint8 BLOCK_START_CODE = 0xFF;

int _tick1Id          = -1;
int _tick2Id          = -1;
bool _exportAllColumn = false;
int _exportArea       = 1;  // ACTION =0,CELL =1, ALL=2
QString _directText   = QString();
bool _bigFont         = false;
int _textEncoding     = 0;  // 0: UTF-8, 1: SHIFT-JIS, 2: GBK

int checkLanguage() {
  QString lang = Preferences::instance()->getCurrentLanguage();
  if (lang == QStringLiteral("日本語")) return 1;
  // We may have "繁體中文" in the feature
  if (lang == QStringLiteral("中文") || lang == QStringLiteral("简体中文"))
    return 2;
  return 0;
}
QString replaceNewlinesWithCr(const QString& text) {
  QString result = text;

  result.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

  result.replace(QStringLiteral("\n"), QStringLiteral("\r"));

  return result;
}
QIcon getColorChipIcon(TPixel32 color) {
  QPixmap pm(15, 15);
  pm.fill(QColor(color.r, color.g, color.b));
  return QIcon(pm);
}

bool hasEnoughBytes(QDataStream& stream, qint64 requiredSize) {
  qint64 total      = stream.device()->size();
  qint64 currentPos = stream.device()->pos();

  return (total - currentPos) >= requiredSize;
}

void writeBlockHeader(QDataStream& stream, quint8 blockId) {
  stream << BLOCK_START_CODE;
  stream << blockId;
}

quint8 readblockHeaderThrow(QDataStream& stream) {
  quint8 startCode;

  if (stream.atEnd()) {
    throw std::runtime_error(
        "Parsing error: Stream ended before reading block start code (0xFF).");
  }

  stream >> startCode;

  if (startCode != BLOCK_START_CODE) {
    QString errorMsg =
        QString("Parsing error: Expected start code 0xFF, but found 0x%1.")
            .arg(startCode, 2, 16, QChar('0'))
            .toUpper();
    throw std::runtime_error(errorMsg.toStdString());
  }

  if (stream.atEnd()) {
    throw std::runtime_error(
        "Parsing error: Stream ended after start code 0xFF. Block ID missing.");
  }

  quint8 blockId;
  stream >> blockId;
  return blockId;
}

const QMap<quint16, QString> cellTypeToStrMap = {
    {0x0001, "#"}, {0x0002, "○"}, {0x0004, "●"}, {0x0008, "×"}};

const QMap<QString, quint16> strToCellTypeMap = {
    {"#", 0x0001}, {"○", 0x0002}, {"●", 0x0004}, {"×", 0x0008}};
}  // namespace

namespace SxfIo {
void SxfProperty::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error(
        "SxfProperty block size is smaller than expected.");

  stream >> resv1;
  stream >> maxFrames;
  stream >> layerCount;
  stream >> fps;
  stream >> sceneNumber;
  stream >> resv2;
  stream >> cutNumber;
  for (quint32& val : resv3) {
    stream >> val;
  }
  stream >> timeFormat;
  stream >> markerInterval;
  stream >> framePerPage;
  for (quint16& widget : widgets) {
    stream >> widget;
  }
  for (quint32& visibility : visibilities) {
    stream >> visibility;
  }
}

void SxfProperty::write(QDataStream& stream) {
  writeBlockHeader(stream, 0x01);
  stream << getSize();
  stream << resv1;
  stream << maxFrames;
  stream << layerCount;
  stream << fps;
  stream << sceneNumber;
  stream << resv2;
  stream << cutNumber;
  for (const quint32& val : resv3) {
    stream << val;
  }
  stream << timeFormat;
  stream << markerInterval;
  stream << framePerPage;
  for (const quint16& widget : widgets) {
    stream << widget;
  }
  for (const quint32& visibility : visibilities) {
    stream << visibility;
  }
}

quint32 SxfDirection::getSize() {
  QByteArray contentBytes;
  QTextCodec* codec = nullptr;
  switch (_textEncoding) {
  case 1:
    codec = QTextCodec::codecForName("Shift-JIS");
    break;
  case 2:
    codec = QTextCodec::codecForName("GBK");
    break;
  default:
    codec = QTextCodec::codecForName("UTF-8");
    break;
  }
  contentBytes = codec->fromUnicode(content);

  quint16 contentSize = static_cast<quint16>(contentBytes.size());
  return 2 + contentSize + 4;
}

void SxfDirection::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error(
        "SxfDirection block size is smaller than expected.");

  quint16 contentSize;
  stream >> contentSize;
  QByteArray contentBytes;
  contentBytes.resize(contentSize);
  stream.readRawData(contentBytes.data(), contentSize);

  QTextCodec* codec = nullptr;
  switch (_textEncoding) {
  case 1:
    codec = QTextCodec::codecForName("Shift-JIS");
    break;
  case 2:
    codec = QTextCodec::codecForName("GBK");
    break;
  default:
    codec = QTextCodec::codecForName("UTF-8");
    break;
  }
  content = codec->toUnicode(contentBytes);

  stream >> bigFont;
}

void SxfDirection::write(QDataStream& stream) {
  writeBlockHeader(stream, 0x02);
  content = replaceNewlinesWithCr(content);
  stream << getSize();

  QByteArray contentBytes;
  QTextCodec* codec = nullptr;
  switch (_textEncoding) {
  case 1:
    codec = QTextCodec::codecForName("Shift-JIS");
    break;
  case 2:
    codec = QTextCodec::codecForName("GBK");
    break;
  default:
    codec = QTextCodec::codecForName("UTF-8");
    break;
  }
  contentBytes = codec->fromUnicode(content);

  quint16 contentSize = static_cast<quint16>(contentBytes.size());
  stream << contentSize;
  stream.writeRawData(contentBytes.constData(), contentSize);

  stream << bigFont;
}

void SxfCell::read(QDataStream& stream) {
  stream >> mark;
  if (stream.status() != QDataStream::Ok) return;

  char buffer[8];
  stream.readRawData(buffer, 8);

  QByteArray asciiData(buffer, 8);
  frameIndex = asciiData.toUInt();
}

void SxfCell::write(QDataStream& stream) {
  stream << mark;

  QByteArray buffer(8, 0x00);
  if (frameIndex > 0) {
    QString frameIndexStr = QString::number(frameIndex);
    QByteArray asciiData  = frameIndexStr.toLocal8Bit();

    int bytesToWrite = qMin(asciiData.length(), 8);
    memcpy(buffer.data(), asciiData.constData(), bytesToWrite);
  }

  stream.writeRawData(buffer.constData(), 8);
}

void SxfColumn::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error("SxfColumn block size is smaller than expected.");

  quint16 nameSize;
  stream >> nameSize;
  QByteArray nameBytes;
  nameBytes.resize(nameSize);
  stream.readRawData(nameBytes.data(), nameSize);
  name = QString::fromUtf8(nameBytes);

  stream >> isVisible;
  stream >> resv;

  cells.clear();
  quint32 cellsSize = size - (2 + nameSize + 8);
  if (cellsSize % 10 != 0) {
    throw std::runtime_error(
        "Invalid cells size: not divisible by cell record size.");
  }

  quint32 cellCount = cellsSize / 10;
  cells.reserve(cellCount);

  for (quint32 i = 0; i < cellCount; ++i) {
    SxfCell cell;
    cell.read(stream);
    cells.append(cell);
  }
}

void SxfColumn::write(QDataStream& stream) {
  stream << getSize();
  QByteArray nameBytes = name.toUtf8();

  quint16 nameSize = static_cast<quint16>(nameBytes.size());
  stream << nameSize;
  stream.writeRawData(nameBytes.constData(), nameSize);

  stream << isVisible;
  stream << resv;

  for (SxfCell& cell : cells) {
    cell.write(stream);
  }
}

void SxfSheet::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error("SxfSheet block size is smaller than expected.");
  columns.clear();
  quint32 readBytes = 0;
  while (readBytes < size) {
    SxfColumn column;
    column.read(stream);
    columns.append(column);
    readBytes += 4 + column.getSize();
  }
}

void SxfSheet::write(QDataStream& stream, quint16 blockId) {
  writeBlockHeader(stream, blockId);
  stream << getSize();
  for (SxfColumn& column : columns) {
    column.write(stream);
  }
}

void SxfSound::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize(24))
    throw std::runtime_error("SxfSound block size is smaller than expected.");
  stream >> resv1;
  // The rest is reserved data, skip it
  stream.skipRawData(size - 4);
}

void SxfSound::write(QDataStream& stream, quint32 frames) {
  writeBlockHeader(stream, 0x05);
  stream << getSize(frames);
  stream << resv1;
  // Write reserved data as zeros
  quint32 reservedSize = getSize(frames) - 4;
  QByteArray reservedData(reservedSize, 0);
  stream.writeRawData(reservedData.constData(), reservedSize);
}

void SxfDialogue::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error(
        "SxfDialogue block size is smaller than expected.");
  stream >> resv1;
}

void SxfDialogue::write(QDataStream& stream) {
  writeBlockHeader(stream, 0x06);
  stream << getSize();
  stream << resv1;
}

void SxfDraw::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error("SxfDraw block size is smaller than expected.");
  stream >> resv1;
  stream >> resv2;
}

void SxfDraw::write(QDataStream& stream) {
  writeBlockHeader(stream, 0x07);
  stream << getSize();
  stream << resv1;
  stream << resv2;
}

void SxfData::read(QDataStream& stream) {
  while (!stream.atEnd()) {
    quint8 blockId = readblockHeaderThrow(stream);
    switch (blockId) {
    case 0x01:
      property.read(stream);
      break;
    case 0x02:
      direction.read(stream);
      break;
    case 0x03:
      actionSheet.read(stream);
      break;
    case 0x04:
      cellSheet.read(stream);
      break;
    case 0x05:
      sound.read(stream);
      break;
    case 0x06:
      dialogue.read(stream);
      break;
    case 0x07:
      simbolAndText.read(stream);
      break;
    default:
      throw std::runtime_error(QString("Unknown block ID: 0x%1")
                                   .arg(blockId, 2, 16, QChar('0'))
                                   .toStdString());
    }
  }
}

void SxfData::write(QDataStream& stream) {
  property.write(stream);
  direction.write(stream);
  actionSheet.write(stream, 0x03);
  cellSheet.write(stream, 0x04);
  sound.write(stream, property.maxFrames);
  dialogue.write(stream);
  simbolAndText.write(stream);
}

void SxfData::build(TXsheet* xsh) {
  if (!xsh) return;

  int maxCol        = xsh->getFirstFreeColumnIndex();
  ToonzScene* scene = xsh->getScene();
  if (!scene) return;
  TSceneProperties* tprop = scene->getProperties();
  auto cellMarks          = tprop->getCellMarks();  // QList<CellMark>

  auto convertTimeFormat = [&](int style) -> TimeFormat {
    switch (style) {
    case (XsheetViewer::FrameDisplayStyle::Frame):
      return TimeFormat::FRAME;
    case (XsheetViewer::FrameDisplayStyle::SecAndFrame):
      return TimeFormat::SECOND_FRAME;
    case (XsheetViewer::FrameDisplayStyle::SixSecSheet):
    case (XsheetViewer::FrameDisplayStyle::ThreeSecSheet):
      return TimeFormat::PAGE_FRAME;
    default:
      return TimeFormat::FRAME;
    }
  };
  property.timeFormat = convertTimeFormat(FrameDisplayStyleInXsheetRowArea);
  property.fps        = tprop->getOutputProperties()->getFrameRate();
  if (property.timeFormat == TimeFormat::PAGE_FRAME) {
    if (FrameDisplayStyleInXsheetRowArea ==
        XsheetViewer::FrameDisplayStyle::SixSecSheet)
      property.framePerPage = 6 * property.fps;
    else
      property.framePerPage = 3 * property.fps;
  }
  int distance, offset, secDistance;
  tprop->getMarkers(distance, offset, secDistance);
  property.markerInterval = distance;

  int start, end, step;
  if (!tprop->getOutputProperties()->getRange(start, end, step)) {
    start = 0;
    end   = xsh->getFrameCount() - 1;
  }
  property.maxFrames = end + 1;

  if (property.maxFrames == 0) {
    QString errorMsg = QString("Xsheet has no frames to export.");
    throw std::runtime_error(errorMsg.toStdString());
  }

  direction.bigFont = _bigFont;
  direction.content = _directText;
  if (direction.content.isEmpty())
    property.setVisiblity(Visibility::DIRECTION, false);

  auto convertMark = [&](int markIndex) -> quint16 {
    if (markIndex == _tick1Id)
      return CellMark::Inbetween;
    else if (markIndex == _tick2Id)
      return CellMark::Inbetween2;

    return CellMark::None;
  };

  SxfSheet* sheetA = _exportArea == 0 ? &actionSheet : &cellSheet;
  SxfSheet* sheetB = _exportArea == 0 ? &cellSheet : &actionSheet;

  for (int col = 0; col < maxCol; ++col) {
    TXshColumn* xc = xsh->getColumn(col);
    if (!xc) continue;

    TXshCellColumn* cellColumn = xc->getCellColumn();
    if (!cellColumn) continue;

    if (!_exportAllColumn && !cellColumn->isPreviewVisible()) continue;

    SxfColumn sxfColA, sxfColB;
    sxfColA.isVisible = cellColumn->isCamstandVisible();

    TXshSimpleLevel* sl = nullptr;
    TFrameId fid;
    for (int row = start; row <= end; ++row) {
      TXshCell cell = cellColumn->getCell(row);
      SxfCell sxfCell;

      int markIndex = cellColumn->getCellMark(row);
      sxfCell.mark  = convertMark(markIndex);

      if (!cell.isEmpty()) {
        if (!sl) sl = cell.getSimpleLevel();
        if (sl && sxfColA.name.isEmpty())
          sxfColA.name = QString::fromStdWString(sl->getName());
        int type = -1;

        if (sl) type = sl->getType();
        if (type & (TXshLevelType::RASTER_TYPE | TXshLevelType::PLI_XSHLEVEL)) {
          TFrameId curfid = cell.getFrameId();

          if (fid != curfid) {
            if (curfid.isEmptyFrame())
              sxfCell.mark = CellMark::Stop;
            else {
              sxfCell.mark |= CellMark::KeyFrame;

              if (curfid.isNoFrame()) {
                sxfColA.name       = "-BG";
                sxfCell.frameIndex = 1;
              } else
                sxfCell.frameIndex = curfid.getNumber();
            }
          }
          fid = curfid;
        }
      } else if (fid != TFrameId::EMPTY_FRAME) {
        sxfCell.mark = CellMark::Stop;
        fid          = TFrameId::EMPTY_FRAME;
      }

      sxfColA.cells.append(sxfCell);
      sxfColB.cells.append(SxfCell());
    }

    sheetA->columns.append(sxfColA);
    sheetB->columns.append(sxfColB);
  }

  if (_exportArea == 2) *sheetB = *sheetA;

  property.layerCount = sheetA->columns.size();
  if (property.layerCount == 0) {
    QString errorMsg = QString("No columns to export.");
    throw std::runtime_error(errorMsg.toStdString());
  }
}

SxfData loadSxf(const QString& sxfFilePath) {
  QFile file(sxfFilePath);

  if (!file.open(QIODevice::ReadOnly)) {
    QString errorMsg =
        QString("Failed to open file for reading: %1").arg(sxfFilePath);
    throw std::runtime_error(errorMsg.toStdString());
  }

  QByteArray rawContent = file.readAll();
  file.close();

  QDataStream stream(rawContent);
  stream.setByteOrder(QDataStream::BigEndian);

  // Check Magic Number(4 Bytes)
  quint32 magicNumValue;
  const quint32 EXPECTED_MAGIC_VALUE = 0x57425343;  // WBSC
  stream >> magicNumValue;
  if (magicNumValue != EXPECTED_MAGIC_VALUE) {
    QString errorMsg =
        QString("File too short to contain Magic Number. Size: %1 bytes.")
            .arg(rawContent.size());
    throw std::runtime_error(errorMsg.toStdString());
  }

  // Check Version/number(4 Bytes)
  quint32 magicVersionNumber;
  const quint32 EXPECTED_VERSION_VALUE = 0x01000007;
  stream >> magicVersionNumber;
  // TODO: check version value if needed, not sure what 7 represents.

  // Read Blocks
  SxfData data;
  try {
    data.read(stream);
  } catch (const std::runtime_error& e) {
    QString errorMsg =
        QString("Error while reading SXF data: %1").arg(e.what());
    throw std::runtime_error(errorMsg.toStdString());
    return SxfData();
  }

  return data;
}

void saveSxf(const QString& sxfFilePath, SxfData& data) {
  QFile file(sxfFilePath);
  if (!file.open(QIODevice::WriteOnly)) {
    QString errorMsg =
        QString("Failed to open file for writing: %1").arg(sxfFilePath);
    throw std::runtime_error(errorMsg.toStdString());
  }
  QDataStream stream(&file);
  stream.setByteOrder(QDataStream::BigEndian);

  // Write Magic Number(4 Bytes)
  const quint32 MAGIC_VALUE = 0x57425343;  // WBSC
  stream << MAGIC_VALUE;

  // Write Version/number(4 Bytes)
  const quint32 VERSION_VALUE = 0x01000007;
  stream << VERSION_VALUE;

  // Write Blocks
  data.write(stream);
  file.close();
}

bool loadSxfScene(ToonzScene* scene, const TFilePath& scenePath) {
  QApplication::restoreOverrideCursor();
  SxfData sxfData;
  try {
    sxfData = loadSxf(scenePath.getQString());
  } catch (const std::runtime_error& e) {
    DVGui::warning(
        QObject::tr("Error while loading SXF scene: %1").arg(e.what()));
    return false;
  }

  if (sxfData.property.maxFrames == 0) {
    DVGui::warning(QObject::tr("SXF data has no frames to import."));
  }

  // Extract level names
  QStringList levelNames;
  QSet<QString> nameSet;

  QString title          = QObject::tr("Import SXF Area");
  QStringList radioTexts = {QObject::tr("ACTION"), QObject::tr("CELL")};
  DVGui::RadioButtonDialog radioDia(title, radioTexts);

  if (radioDia.exec() == QDialog::Rejected) return false;
  if (radioDia.result() == 0) return false;
  bool loadAction = (radioDia.result() == 1);

  const SxfSheet& sheet = loadAction ? sxfData.actionSheet : sxfData.cellSheet;
  if (loadAction)
    for (const SxfColumn& col : sxfData.actionSheet.columns) {
      if (!col.name.isEmpty()) nameSet.insert(col.name.trimmed());
    }
  else
    for (const SxfColumn& col : sxfData.cellSheet.columns) {
      if (!col.name.isEmpty()) nameSet.insert(col.name.trimmed());
    }

  if (nameSet.isEmpty()) {
    DVGui::warning(QObject::tr("Current XSheet is emepty."));
  }

  levelNames = nameSet.values();

  scene->clear();

  auto sceneProject = TProjectManager::instance()->loadSceneProject(scenePath);
  if (!sceneProject) return false;
  scene->setProject(sceneProject);
  std::string sceneFileName = scenePath.getName() + ".tnz";
  scene->setScenePath(scenePath.getParentDir() + sceneFileName);
  // set the current scene here in order to use $scenefolder node properly
  // in the file browser which opens from XDTSImportPopup
  TApp::instance()->getCurrentScene()->setScene(scene);

  // isUextVersion=false, isSXF=true
  XDTSImportPopup popup(levelNames, scene, scenePath, false, true);
  if (!levelNames.isEmpty()) {
    int ret = popup.exec();
    if (ret == QDialog::Rejected) return false;
  }

  QMap<QString, TXshLevel*> levels;
  try {
    for (QString name : levelNames) {
      QString levelPath = popup.getLevelPath(name);
      TXshLevel* level  = nullptr;
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

  TXsheet* xsh = scene->getXsheet();
  int duration = sxfData.property.maxFrames;

  QVector<TXshCell> lastCells(sheet.columns.size());

  for (int c = 0; c < sheet.columns.size(); ++c) {
    const SxfColumn& sxfCol = sheet.columns.at(c);
    QString levelName       = sxfCol.name.trimmed();
    TXshLevel* level        = levels.value(levelName);

    if (!level) continue;

    TXshSimpleLevel* sl = level->getSimpleLevel();
    if (!sl) continue;
    QList<int> tick1, tick2;
    bool isBG =
        sl->isEmpty() ? sxfCol.name == "-BG" : sl->getFirstFid().isNoFrame();

    for (int r = 0; r < duration; ++r) {
      if (r >= sxfCol.cells.size()) {
        // Fill lastCell if sxf col duration shorter than max frames
        if (!lastCells[c].isEmpty()) {
          xsh->setCell(r, c, lastCells[c]);
        }
        continue;
      }

      const SxfCell& sxfCell = sxfCol.cells.at(r);
      TXshCell cell;

      if (sxfCell.frameIndex > 0) {
        TFrameId fid(sxfCell.frameIndex);
        if (isBG) fid = TFrameId::NO_FRAME;
        sl->formatFId(fid, tmplFId);
        cell         = TXshCell(level, fid);
        lastCells[c] = cell;  // Update last valid cell
      } else if (sxfCell.mark & CellMark::Stop) {
        lastCells[c] = TXshCell();
      } else {
        cell = lastCells[c];
      }

      xsh->setCell(r, c, cell);

      // Record marks
      if (loadAction) {
        if (sxfCell.mark & CellMark::Inbetween) {
          tick1.append(r);
        }
        if (sxfCell.mark & CellMark::Inbetween2) {
          tick2.append(r);
        }
      }
    }

    // Set marks
    if (loadAction) {
      TXshCellColumn* cellColumn = nullptr;
      if (xsh->getColumn(c)) cellColumn = xsh->getColumn(c)->getCellColumn();
      if (cellColumn) {
        if (tick1Id >= 0) {
          for (auto tick1f : tick1) cellColumn->setCellMark(tick1f, tick1Id);
        }
        if (tick2Id >= 0) {
          for (auto tick2f : tick2) cellColumn->setCellMark(tick2f, tick2Id);
        }
      }
    }

    // Set Column Name
    TStageObject* pegbar = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (pegbar) pegbar->setName(levelName.toStdString());
  }

  xsh->updateFrameCount();

  if (duration != xsh->getFrameCount()) {
    scene->getProperties()->getPreviewProperties()->setRange(0, duration - 1,
                                                             1);
  }
  // if the duration is shorter than frame count, then set it in output range.
  if (duration < xsh->getFrameCount()) {
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
  ExportXDTSCommand() : MenuItemHandler(MI_ExportSXF) {}
  void execute() override;
} exportXDTSCommand;

void ExportXDTSCommand::execute() {
  SxfData sxfData;

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet* xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("sxf");

  _tick1Id         = -1;
  _tick2Id         = -1;
  _exportAllColumn = false;
  _exportArea      = 1;
  _directText      = QString();
  _bigFont         = false;
  // _textEncoding    = 0;

  static GenericSaveFilePopup* savePopup = nullptr;
  static QComboBox* tick1Id              = nullptr;
  static QComboBox* tick2Id              = nullptr;
  static QComboBox* targetColumnCombo    = nullptr;
  static QComboBox* exportAreaCombo      = nullptr;
  static QPlainTextEdit* directTextEdit  = nullptr;
  static QCheckBox* bigFont              = nullptr;
  static QComboBox* encodingCombo        = nullptr;

  auto refreshCellMarkComboItems = [](QComboBox* combo) {
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
    QWidget* customWidget = new QWidget();
    tick1Id               = new QComboBox();
    tick2Id               = new QComboBox();

    refreshCellMarkComboItems(tick1Id);
    refreshCellMarkComboItems(tick2Id);

    // Inbetween ticks
    tick1Id->setCurrentIndex(tick1Id->findData(6));
    tick2Id->setCurrentIndex(tick2Id->findData(8));

    // Target column
    targetColumnCombo = new QComboBox();
    targetColumnCombo->addItem(QObject::tr("All columns"), true);
    targetColumnCombo->addItem(QObject::tr("Only active columns"), false);
    targetColumnCombo->setCurrentIndex(targetColumnCombo->findData(true));

    // Export Area
    exportAreaCombo = new QComboBox();
    exportAreaCombo->addItem(QObject::tr("ACTION"), 0);         // ACTION
    exportAreaCombo->addItem(QObject::tr("CELL"), 1);           // CELL
    exportAreaCombo->addItem(QObject::tr("ACTION & CELL"), 2);  // ALL

    exportAreaCombo->setCurrentIndex(
        exportAreaCombo->findData(2));  // ALL for default

    // Direction
    directTextEdit = new QPlainTextEdit();
    bigFont        = new QCheckBox();
    encodingCombo  = new QComboBox();

    // Layouts
    QHBoxLayout* customLay = new QHBoxLayout();
    QVBoxLayout* leftLay   = new QVBoxLayout();
    QVBoxLayout* rightLay  = new QVBoxLayout();

    // leftLay
    {
      leftLay->setMargin(5);
      leftLay->setSpacing(10);
      // 1. Direction
      QGroupBox* textEditGroupBox = new QGroupBox(QObject::tr("Direction"));

      QVBoxLayout* textEditLay = new QVBoxLayout();
      textEditLay->setMargin(10);
      textEditLay->setSpacing(8);

      {
        // 1.1 Text Edit
        directTextEdit->setPlaceholderText(
            QObject::tr("Enter text here...\nSupports multiple lines"));
        directTextEdit->setMinimumWidth(300);
        directTextEdit->setFixedHeight(60);

        textEditLay->addWidget(directTextEdit);

        QHBoxLayout* optionsLay = new QHBoxLayout();
        optionsLay->setSpacing(15);

        bigFont->setText(QObject::tr("Large Font"));

        QObject::connect(bigFont, &QCheckBox::stateChanged, [&](int state) {
          QFont font = directTextEdit->font();
          if (state == Qt::Checked) {
            font.setPointSize(14);  // Big Font
          } else {
            font.setPointSize(9);  // Normal Font
          }
          directTextEdit->setFont(font);
        });

        optionsLay->addWidget(bigFont);

        optionsLay->addStretch();

        // 1.2 Encoding combobox
        QHBoxLayout* encodingLay = new QHBoxLayout();
        encodingLay->setSpacing(5);

        QLabel* encodingLabel = new QLabel(QObject::tr("Text Encoding:"));
        encodingCombo->addItem("UTF-8", 0);
        encodingCombo->addItem("SHIFT-JIS", 1);
        encodingCombo->addItem("GBK", 2);
        encodingCombo->setCurrentIndex(checkLanguage());
        encodingCombo->setMaximumWidth(120);

        encodingLay->addWidget(encodingLabel);
        encodingLay->addWidget(encodingCombo);

        optionsLay->addLayout(encodingLay);

        textEditLay->addLayout(optionsLay);
      }

      textEditGroupBox->setLayout(textEditLay);
      leftLay->addWidget(textEditGroupBox);
    }
    // rightLay - Cell Marks and Export Settings
    {
      rightLay->setMargin(5);
      rightLay->setSpacing(10);

      // 2. Cell Marks
      QGroupBox* cellMarkGroupBox =
          new QGroupBox(QObject::tr("Cell marks for SXF symbols"));

      QGridLayout* cellMarkLay = new QGridLayout();
      cellMarkLay->setMargin(10);
      cellMarkLay->setVerticalSpacing(10);
      cellMarkLay->setHorizontalSpacing(5);

      {
        cellMarkLay->addWidget(new QLabel(QObject::tr("Inbetween Symbol1 (○)")),
                               0, 0, Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(tick1Id, 0, 1);

        cellMarkLay->addWidget(
            new QLabel(QObject::tr("Inbetween Symbol2 (●):")), 1, 0,
            Qt::AlignRight | Qt::AlignVCenter);
        cellMarkLay->addWidget(tick2Id, 1, 1);
      }
      cellMarkGroupBox->setLayout(cellMarkLay);
      rightLay->addWidget(cellMarkGroupBox, 0, Qt::AlignRight);

      // 3. Export Settings
      QHBoxLayout* settingsLay = new QHBoxLayout();
      settingsLay->setMargin(0);
      settingsLay->setSpacing(15);

      settingsLay->addStretch();

      // 3.1 Target Column
      QHBoxLayout* targetGroup = new QHBoxLayout();
      targetGroup->addWidget(new QLabel(QObject::tr("Target Column:")));
      targetGroup->addWidget(targetColumnCombo);

      // 3.2 Export Area
      QHBoxLayout* exportGroup = new QHBoxLayout();
      exportGroup->addWidget(new QLabel(QObject::tr("Export Area:")));
      exportGroup->addWidget(exportAreaCombo);

      settingsLay->addLayout(targetGroup);
      settingsLay->addLayout(exportGroup);

      rightLay->addLayout(settingsLay);

      targetColumnCombo->setMaximumWidth(150);
      exportAreaCombo->setMaximumWidth(150);
    }

    customLay->addLayout(leftLay);
    customLay->addLayout(rightLay);
    customWidget->setLayout(customLay);

    savePopup =
        new GenericSaveFilePopup(QObject::tr("Export SXF File"), customWidget);
    savePopup->addFilterType("sxf");
  } else {
    refreshCellMarkComboItems(tick1Id);
    refreshCellMarkComboItems(tick2Id);
  }

  // set default path and filename
  if (!scene->isUntitled())
    savePopup->setFolder(fp.getParentDir());
  else
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  savePopup->setFilename(fp.withoutParentDir());

  // Get user selected save path
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  // update params
  _tick1Id         = tick1Id->currentData().toInt();
  _tick2Id         = tick2Id->currentData().toInt();
  _exportAllColumn = targetColumnCombo->currentData().toBool();
  _exportArea      = exportAreaCombo->currentData().toInt();
  _directText      = directTextEdit->toPlainText();
  _bigFont         = bigFont->isChecked();
  _textEncoding    = encodingCombo->currentData().toInt();

  try {
    sxfData.build(xsh);
  } catch (std::runtime_error e) {
    DVGui::error(QObject::tr("Failed to export SXF file.%1").arg(e.what()));
    return;
  }

  // Save File
  try {
    saveSxf(fp.getQString(), sxfData);
  } catch (std::runtime_error e) {
    DVGui::error(QObject::tr("Failed to export SXF file.%1").arg(e.what()));
    return;
  }

  // Succssed message box
  QString str = QObject::tr("The file %1 has been exported successfully.")
                    .arg(QString::fromStdString(fp.getLevelName()));

  std::vector<QString> buttons = {QObject::tr("OK"),
                                  QObject::tr("Open containing folder")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons);

  // Open containing folder
  if (ret == 2) {
    TFilePath folderPath = fp.getParentDir();
    if (TSystem::isUNC(folderPath))
      QDesktopServices::openUrl(QUrl(folderPath.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath.getQString()));
  }
}
}  // namespace SxfIo