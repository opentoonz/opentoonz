#include "sxfio.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"

#include "xdtsimportpopup.h"
#include "toonz/txshcell.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"

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

using namespace SxfIo;

namespace {
const quint8 BLOCK_START_CODE = 0xFF;

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
  stream >> rulerInterval;
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
  stream << rulerInterval;
  stream << framePerPage;
  for (const quint16& widget : widgets) {
    stream << widget;
  }
  for (const quint32& visibility : visibilities) {
    stream << visibility;
  }
}

void SxfNote::read(QDataStream& stream) {
  quint32 size;
  stream >> size;
  if (size < getSize())
    throw std::runtime_error("SxfNote block size is smaller than expected.");

  quint16 contentSize;
  stream >> contentSize;
  QByteArray contentBytes;
  contentBytes.resize(contentSize);
  stream.readRawData(contentBytes.data(), contentSize);

  //TODO: Check language
  bool chinese  = true;
  bool japanese = false;
  if (chinese) {
    QTextCodec* gbkCodec = QTextCodec::codecForName("GBK");
    content              = gbkCodec->toUnicode(contentBytes);
  } else if (japanese) {
    QTextCodec* shiftJisCodec = QTextCodec::codecForName("Shift-JIS");
    content                   = shiftJisCodec->toUnicode(contentBytes);
  } else {
    content = QString::fromUtf8(contentBytes);
  }

  stream >> bigFont;
}

void SxfNote::write(QDataStream& stream) {
  writeBlockHeader(stream, 0x02);
  stream << getSize();

  bool chinese  = true;
  bool japanese = false;

  QByteArray contentBytes;
  if (chinese) {
    QTextCodec* gbkCodec = QTextCodec::codecForName("GBK");
    contentBytes         = gbkCodec->fromUnicode(content);
  } else if (japanese) {
    QTextCodec* shiftJisCodec = QTextCodec::codecForName("Shift-JIS");
    contentBytes              = shiftJisCodec->fromUnicode(content);
  } else {
    contentBytes = content.toUtf8();
  }

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
      note.read(stream);
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
  data.property.write(stream);
  data.note.write(stream);
  data.actionSheet.write(stream, 0x03);
  data.cellSheet.write(stream, 0x04);
  data.sound.write(stream, data.property.maxFrames);
  data.dialogue.write(stream);
  data.simbolAndText.write(stream);
  file.close();
}

class ExportXDTSCommand final : public MenuItemHandler {
public:
  ExportXDTSCommand() : MenuItemHandler(MI_ExportSXF) {}
  void execute() override;
} exportXDTSCommand;

void ExportXDTSCommand::execute() {
  SxfData sxfData;
  SxfSheet& sheet = sxfData.cellSheet;
  sheet.columns.clear();

  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet* xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("sxf");

  int maxCol = xsh->getFirstFreeColumnIndex();

  auto cellMarks = TApp::instance()
                       ->getCurrentScene()
                       ->getScene()
                       ->getProperties()
                       ->getCellMarks();  // QList<CellMark>

  auto convertMark = [&](int markIndex) -> quint16 {
    if (markIndex < 0 || markIndex >= cellMarks.size())
      return CellMark::KeyFrame;

    const QString name = cellMarks[markIndex].name;

    if (name.contains("Key", Qt::CaseInsensitive) || name.contains("#"))
      return CellMark::KeyFrame;

    if (name.contains("Inbetween2", Qt::CaseInsensitive) || name.contains("●"))
      return CellMark::Inbetween2;

    if (name.contains("Inbetween", Qt::CaseInsensitive) || name.contains("○"))
      return CellMark::Inbetween;

    if (name.contains("Stop", Qt::CaseInsensitive) ||
        name.contains("x", Qt::CaseInsensitive) || name.contains("×"))
      return CellMark::Stop;

    return CellMark::KeyFrame;
  };

  for (int col = 0; col < maxCol; ++col) {
    TXshColumn* xc = xsh->getColumn(col);
    if (!xc) continue;

    TXshCellColumn* cellColumn = xc->getCellColumn();
    if (!cellColumn) continue;

    if (!cellColumn->isPreviewVisible()) continue;

    SxfColumn sxfCol;
    sxfCol.isVisible = cellColumn->isCamstandVisible();
    sxfCol.name      = QString("Col_%1").arg(col);

    int r0, r1;
    cellColumn->getRange(r0, r1);
    if (r0 > r1) continue;

    for (int row = r0; row <= r1; ++row) {
      TXshCell cell = cellColumn->getCell(row);
      SxfCell sxfCell;

      
      if (!cell.isEmpty()) {
        TXshSimpleLevel* sl = cell.getSimpleLevel();
        if (sl && (sl->getType() &
                   (TXshLevelType::FULLCOLOR_TYPE | TXshLevelType::RASTER_TYPE |
                    TXshLevelType::OVL_XSHLEVEL))) {
          int markIndex = cellColumn->getCellMark(row);
          sxfCell.mark  = convertMark(markIndex);
          TFrameId fid = cell.getFrameId();
          if (fid.getNumber() > 0) sxfCell.frameIndex = fid.getNumber();
        }
      }

      sxfCol.cells.append(sxfCell);
    }

    sheet.columns.append(sxfCol);
  }
  sxfData.actionSheet = sxfData.cellSheet;
  sxfData.property.layerCount = sheet.columns.size();
  sxfData.property.maxFrames = xsh->getFrameCount();

  saveSxf(fp.getQString(), sxfData);
}
}  // namespace SxfIo