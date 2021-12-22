#pragma once
#ifndef XDTSIO_H
#define XDTSIO_H

#include "tfilepath.h"

#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>

#include <iostream>

class ToonzScene;
class TFilePath;
class TXsheet;
class TXshCellColumn;
class QJsonObject;

namespace XdtsIo {

enum FieldId { CELL = 0, DIALOG = 3, CAMERAWORK = 5 };
// (*1) Field: Input location of each timesheet instruction.
// Fields are divided into cells, dialog, and camerawork.
// [FieldId]  [field name]  [details]
// 0          Cell          Field for cell numbers
// 3          Dialog        Field for speaker names and line timing
// 5          Camerawork    Field for camerawork instructions

// "description": "Cut / scene information",
class XdtsHeader {
  //"description": "Cut No."
  //"pattern" : "\\d{1,4}"
  QString m_cut;
  //"description": "Scene No.",
  //"pattern" : "\\d{1,4}"
  QString m_scene;

  // "required": ["cut", "scene"]
public:
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  bool isEmpty() const { return m_cut.isEmpty() && m_scene.isEmpty(); }
};

//"description": "Frame instruction information",
class XdtsFrameDataItem {
  //"description": "Instruction type(*2)",
  //*2 Currently only supports instructions with id=0.
  enum DataId { Default = 0 } m_id;

  //"description": "Instruction value(*3)",
  QList<QString> m_values;
  // "minItems" : 1,
  //"required": ["id", "values"]

  //(*3) Values are different depending on the field type.
  // Values for each field are as follows.
  // [Field name: Cell]
  //  Enter the cell number string and special instruction string(*4)
  //  (value element numbers: 1)
  // [Field name: Dialog]
  //  In the dialog's first frame, enter the speaker name in the first
  //  element, and the dialog's string in the second element.
  //  When the line lasts for multiple frames, specify the character
  //  string "SYMBOL_HYPHEN" up to the end frame.
  //  (value element numbers: 1Å`2)
  // [Field name: Camerawork]
  //  In the camerawork's first frame, enter the camerawork instruction
  //  string When the camerawork lasts for multiple frames, specify the
  //  character string "SYMBOL_HYPHEN" up to the end frame.
  //  (value element numbers: 1)

  //(*4) Special instruction character strings
  // [Character string]  [Instruction]           [Valid field]
  // SYMBOL_TICK_1       Inbetween symbol(Åõ)     Cell
  // SYMBOL_TICK_2       Reverse sheet symbol(Åú) Cell
  // SYMBOL_NULL_CELL    Empty cell symbol(Å~)    Cell
  // SYMBOL_HYPHEN       Continue previous       All fields
  //                     field instruction

  TFrameId str2Fid(const QString &) const;
  QString fid2Str(const TFrameId &) const;

public:
  enum { SYMBOL_TICK_1 = -100, SYMBOL_TICK_2 = -200 };

  XdtsFrameDataItem() : m_id(Default) {}
  XdtsFrameDataItem(TFrameId fId) : m_id(Default) {
    m_values.append(fid2Str(fId));
  }
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  TFrameId getFrameId() const;
};

//"description": "Individual layer frame information",
class XdtsTrackFrameItem {
  //"description": "Frame instruction information",
  QList<XdtsFrameDataItem> m_data;

  //"description": "Frame No.(*5)",
  // (*5) Set as 0 when specifying the first frame.
  //"minimum" : 0
  int m_frame;

  //"required": ["data", "frame"]
public:
  XdtsTrackFrameItem() = default;
  XdtsTrackFrameItem(int frame, TFrameId fId) : m_frame(frame) {
    m_data.append(XdtsFrameDataItem(fId));
  }
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  QPair<int, TFrameId> frameFid() const;
};

//"description": "Individual field layer info",
class XdtsFieldTrackItem {
  //"description": "Individual layer frame information",
  QList<XdtsTrackFrameItem> m_frames;

  //"description":  "Layer No.(*6)",
  // (*6) Set as 0 for the bottom layer.
  // corresponds to column numbers in OT
  int m_trackNo;
  // "minimum" : 0

  //"required": ["frames", "trackNo"]
public:
  XdtsFieldTrackItem(int trackNo = 0) : m_trackNo(trackNo) {}
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  bool isEmpty() const { return m_frames.isEmpty(); }
  int getTrackNo() const { return m_trackNo; }
  QVector<TFrameId> getCellFrameIdTrack(QList<int> &tick1,
                                        QList<int> &tick2) const;

  QString build(TXshCellColumn *);
  void addFrame(int frame, TFrameId fId) {
    m_frames.append(XdtsTrackFrameItem(frame, fId));
  }
};

class XdtsTimeTableFieldItem {
  // "description": "Field type(*1)",
  FieldId m_fieldId;

  //"description": "Individual field layer info",
  QList<XdtsFieldTrackItem> m_tracks;

  //"required": ["fieldId", "tracks"]
public:
  XdtsTimeTableFieldItem(FieldId fieldId = CELL) : m_fieldId(fieldId) {}
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  bool isCellField() { return m_fieldId == CELL; }
  QList<int> getOccupiedColumns() const;
  QVector<TFrameId> getColumnTrack(int col, QList<int> &tick1,
                                   QList<int> &tick2) const;

  void build(TXsheet *, QStringList &);
};

class XdtsTimeTableHeaderItem {
  //"description": "Field type(*1)",
  FieldId m_fieldId;

  //"description": "Layer name array(*7)",
  //(*7) Specify layer names with trackNo's that match numbers counted as 0,1,2,
  // etc.
  QStringList m_names;

  //"required": ["fieldId", "names"]
public:
  XdtsTimeTableHeaderItem(FieldId fieldId = CELL) : m_fieldId(fieldId) {}
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  bool isCellField() { return m_fieldId == CELL; }
  QStringList getLayerNames() const { return m_names; }

  void build(QStringList &);
  void addName(QString name) { m_names.append(name); }
};

class XdtsTimeTableItem {
  //"description": "Individual field info",
  QList<XdtsTimeTableFieldItem> m_fields;
  int m_cellFieldIndex = -1;
  //"description":  "Timesheet total frames",
  //"minimum" : 1
  int m_duration;

  //"description": "Timesheet name",
  QString m_name;

  // "description": "Individual field layer name",
  QList<XdtsTimeTableHeaderItem> m_timeTableHeaders;
  int m_cellHeaderIndex = -1;
  //"required": ["duration", "name", "timeTableHeaders"]

public:
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  QStringList getLevelNames() const;
  XdtsTimeTableFieldItem getCellField() { return m_fields[m_cellFieldIndex]; }
  XdtsTimeTableHeaderItem getCellHeader() {
    return m_timeTableHeaders[m_cellHeaderIndex];
  }
  int getDuration() { return m_duration; }
  void build(TXsheet *, QString, int duration);
  bool isEmpty() {
    return m_duration == 0 || m_fields.isEmpty() ||
           m_timeTableHeaders.isEmpty();
  }
};

// "$schema": "http://json-schema.org/draft-07/schema",
class XdtsData {
  XdtsHeader m_header;

  //"description": "Timesheet info",
  //"minItems" : 1,
  QList<XdtsTimeTableItem> m_timeTables;

  // "description": "XTDS file format version",
  enum Version { Ver_2018_11_29 = 5 } m_version;

  //"required": ["timeTables", "version"]
public:
  XdtsData(Version version = Ver_2018_11_29) : m_version(version) {}
  void read(const QJsonObject &json);
  void write(QJsonObject &json) const;
  QStringList getLevelNames() const;
  XdtsTimeTableItem &timeTable() { return m_timeTables[0]; }
  void build(TXsheet *, QString, int duration);
  bool isEmpty() { return m_timeTables.isEmpty(); }
};

bool loadXdtsScene(ToonzScene *scene, const TFilePath &scenePath);

}  // namespace XdtsIo

#endif
