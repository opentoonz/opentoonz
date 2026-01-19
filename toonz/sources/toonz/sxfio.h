#pragma once
#ifndef SXFIO_H
#define SXFIO_H

#include <QString>
#include <QStringList>
#include <QList>
#include <array>
#include "toonz/txsheet.h"
#include "tenv.h"
#include "xsheetviewer.h"

extern TEnv::IntVar FrameDisplayStyleInXsheetRowArea;

namespace SxfIo {

enum TimeFormat : quint16 {
  FRAME        = 1,
  FOOT_FRAME   = 2,
  PAGE_FRAME   = 4,
  SECOND_FRAME = 8
};

enum Visibility {
  ACTION     = 0,
  CELL       = 1,
  DIALOGUE   = 2,
  SOUND      = 3,
  CAMERA     = 4,
  DIRECTION  = 5,
  BASIC_INFO = 6
};

enum CellMark : quint16 {
  None       = 0x0000,
  KeyFrame   = 0x0001,
  Inbetween  = 0x0002,
  Inbetween2 = 0x0004,
  Stop       = 0x0008
};

struct SxfProperty {
  // Basic
  quint16 resv1      = 7;
  quint32 maxFrames  = 0;
  quint32 layerCount = 4;
  quint16 fps        = 24;

  // Info
  quint32 sceneNumber = 1;
  quint32 resv2       = 0;
  quint32 cutNumber   = 1;

  std::array<quint32, 3> resv3 = {0, 0, 1};

  // Others
  quint16 timeFormat     = TimeFormat::FRAME;
  quint16 markerInterval = 6;
  quint16 framePerPage   = 144;

  std::array<quint16, 7> widgets = {0x06, 0x01, 0x04, 0x8, 0x02, 0x10, 0x19};
  std::array<quint32, 7> visibilities = {1, 1, 1, 0, 0, 1, 0};

  const qint32 getSize() { return 84; }
  void read(QDataStream& stream);
  void write(QDataStream& stream);

  void setVisiblity(Visibility item, bool isVisible) {
    visibilities[item] = isVisible ? 1 : 0;
  }
  bool getVisiblity(Visibility item) const {
    return visibilities[item] != (quint32)0;
  }
};

struct SxfDirection {
  QString content;
  quint32 bigFont = 0;

  quint32 getSize();
  void read(QDataStream& stream);
  void write(QDataStream& stream);
};

struct SxfCell {
  quint16 mark       = CellMark::None;
  quint32 frameIndex = 0;  // Store as ASCIIx8

  void read(QDataStream& stream);
  void write(QDataStream& stream);
};
struct SxfColumn {
  QString name;  // TODO:GBK? SHIFT_JIS? UTF-8?
  quint32 isVisible = 1;
  quint32 resv      = 10;
  QList<SxfCell> cells;

  const quint32 getSize() {
    return 2 + (quint32)(name.size()) + 4 + 4 + (quint32)(cells.size()) * 10;
  }
  void read(QDataStream& stream);
  void write(QDataStream& stream);
};

struct SxfSheet {
  QList<SxfColumn> columns;

  const quint32 getSize() {
    quint32 totalSize = 0;
    for (SxfColumn col : columns) {
      totalSize += 4 + col.getSize();
    }
    return totalSize;
  }
  void read(QDataStream& stream);
  void write(QDataStream& stream, quint16 blockId);
};

// Not sure what this section used for
struct SxfSound {
  quint32 resv1 = 6;
  const quint32 getSize(quint32 frames) { return 6 * frames + 4; }
  void read(QDataStream& stream);
  void write(QDataStream& stream, quint32 frames);
};

// Not sure what this section used for
struct SxfDialogue {
  quint16 resv1 = 0;
  const quint32 getSize() { return 2; }
  void read(QDataStream& stream);
  void write(QDataStream& stream);
};

// Not implemented
struct SxfDraw {
  quint32 resv1 = 0;
  quint32 resv2 = 0;
  const quint32 getSize() { return 8; }
  void read(QDataStream& stream);
  void write(QDataStream& stream);
};

struct SxfData {
  SxfProperty property;
  SxfDirection direction;
  SxfSheet actionSheet;
  SxfSheet cellSheet;
  SxfSound sound;
  SxfDialogue dialogue;
  SxfDraw simbolAndText;
  void read(QDataStream& stream);
  void write(QDataStream& stream);

  void build(TXsheet* xsh);
};

SxfData loadSxf(const QString& sxfFilePath);
void saveSxf(const QString& sxfFilePath, SxfData& data);

bool loadSxfScene(ToonzScene* scene, const TFilePath& scenePath);
}  // namespace SxfIo

#endif
