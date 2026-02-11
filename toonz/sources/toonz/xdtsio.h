#pragma once

#ifndef XDTSIO_H
#define XDTSIO_H

#include "tfilepath.h"

#include <QString>
#include <QList>
#include <QPair>
#include <QStringList>

class ToonzScene;
class TXsheet;
class TXshCellColumn;
class QJsonObject;

namespace XdtsIo {

//=============================================================================
// Field IDs - Correspond to different types of timesheet instructions
//=============================================================================

enum FieldId {
  CELL       = 0,  // Field for cell numbers
  DIALOG     = 3,  // Field for speaker names and line timing
  CAMERAWORK = 5   // Field for camerawork instructions
};

//=============================================================================
// XdtsHeader - Cut/Scene information
//=============================================================================

class XdtsHeader {
  QString m_cut;    // Cut number (pattern: \d{1,4})
  QString m_scene;  // Scene number (pattern: \d{1,4})

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  bool isEmpty() const { return m_cut.isEmpty() && m_scene.isEmpty(); }
};

//=============================================================================
// XdtsFrameDataItem - Frame instruction information
//=============================================================================

class XdtsFrameDataItem {
  enum DataId { Default = 0 } m_id;
  QList<QString> m_values;
  QList<QString> m_options;  // Extended options for Ponoc customized version

  TFrameId str2Fid(const QString& str) const;
  QString fid2Str(const TFrameId& fid) const;

public:
  enum { SYMBOL_TICK_1 = -100, SYMBOL_TICK_2 = -200 };

  XdtsFrameDataItem() : m_id(Default) {}
  explicit XdtsFrameDataItem(TFrameId fid, const QList<QString>& options = {})
      : m_id(Default), m_options(options) {
    m_values.append(fid2Str(fid));
  }

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  struct FrameInfo {
    TFrameId frameId;
    QStringList options;
  };

  FrameInfo getFrameInfo() const;
};

//=============================================================================
// XdtsTrackFrameItem - Individual layer frame information
//=============================================================================

class XdtsTrackFrameItem {
  QList<XdtsFrameDataItem> m_data;
  int m_frame = 0;  // Frame number (0 for first frame)

public:
  XdtsTrackFrameItem() = default;
  XdtsTrackFrameItem(int frame, TFrameId fid,
                     const QList<QString>& options = {})
      : m_frame(frame) {
    m_data.append(XdtsFrameDataItem(fid, options));
  }

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  QPair<int, XdtsFrameDataItem::FrameInfo> frameFinfo() const;
};

//=============================================================================
// XdtsFieldTrackItem - Individual field layer info
//=============================================================================

class XdtsFieldTrackItem {
  QList<XdtsTrackFrameItem> m_frames;
  int m_trackNo = 0;  // Layer number (0 for bottom layer)

public:
  explicit XdtsFieldTrackItem(int trackNo = 0) : m_trackNo(trackNo) {}

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  bool isEmpty() const { return m_frames.isEmpty(); }
  int getTrackNo() const { return m_trackNo; }

  QVector<TFrameId> getCellFrameIdTrack(QList<int>& tick1, QList<int>& tick2,
                                        QList<int>& keyFrames,
                                        QList<int>& refFrames) const;

  QString build(TXshCellColumn* column);

  void addFrame(int frame, TFrameId fid, const QList<QString>& options = {}) {
    m_frames.append(XdtsTrackFrameItem(frame, fid, options));
  }
};

//=============================================================================
// XdtsTimeTableFieldItem - Field type with multiple tracks
//=============================================================================

class XdtsTimeTableFieldItem {
  FieldId m_fieldId = CELL;
  QList<XdtsFieldTrackItem> m_tracks;

public:
  explicit XdtsTimeTableFieldItem(FieldId fieldId = CELL)
      : m_fieldId(fieldId) {}

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  bool isCellField() const { return m_fieldId == CELL; }
  QList<int> getOccupiedColumns() const;

  QVector<TFrameId> getColumnTrack(int column, QList<int>& tick1,
                                   QList<int>& tick2, QList<int>& keyFrames,
                                   QList<int>& refFrames) const;

  void build(TXsheet* xsheet, QStringList& columnLabels);
};

//=============================================================================
// XdtsTimeTableHeaderItem - Layer name information
//=============================================================================

class XdtsTimeTableHeaderItem {
  FieldId m_fieldId = CELL;
  QStringList m_names;  // Layer names matching track numbers

public:
  explicit XdtsTimeTableHeaderItem(FieldId fieldId = CELL)
      : m_fieldId(fieldId) {}

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  bool isCellField() const { return m_fieldId == CELL; }
  QStringList getLayerNames() const { return m_names; }

  void build(const QStringList& columnLabels) { m_names = columnLabels; }
  void addName(const QString& name) { m_names.append(name); }
};

//=============================================================================
// XdtsTimeTableItem - Timesheet with fields and headers
//=============================================================================

class XdtsTimeTableItem {
  QList<XdtsTimeTableFieldItem> m_fields;
  QList<XdtsTimeTableHeaderItem> m_timeTableHeaders;

  int m_cellFieldIndex  = -1;
  int m_cellHeaderIndex = -1;
  int m_duration        = 0;
  QString m_name;

public:
  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  QStringList getLevelNames() const;

  XdtsTimeTableFieldItem getCellField() const {
    return m_fields.value(m_cellFieldIndex);
  }

  XdtsTimeTableHeaderItem getCellHeader() const {
    return m_timeTableHeaders.value(m_cellHeaderIndex);
  }

  int getDuration() const { return m_duration; }

  void build(TXsheet* xsheet, const QString& name, int duration);

  bool isEmpty() const {
    return m_duration == 0 || m_fields.isEmpty() ||
           m_timeTableHeaders.isEmpty();
  }
};

//=============================================================================
// XdtsData - Complete XDTS document structure
//=============================================================================

class XdtsData {
  XdtsHeader m_header;
  QList<XdtsTimeTableItem> m_timeTables;

  enum Version { Ver_2018_11_29 = 5 } m_version = Ver_2018_11_29;
  QString m_subversion;  // "p1" for Ponoc extended version

public:
  explicit XdtsData(Version version = Ver_2018_11_29) : m_version(version) {}

  void read(const QJsonObject& json);
  void write(QJsonObject& json) const;

  QStringList getLevelNames() const;

  XdtsTimeTableItem& timeTable() { return m_timeTables[0]; }
  const XdtsTimeTableItem& timeTable() const { return m_timeTables[0]; }

  void build(TXsheet* xsheet, const QString& name, int duration);

  bool isEmpty() const { return m_timeTables.isEmpty(); }
  bool isUextVersion() const { return !m_subversion.isEmpty(); }
};

//=============================================================================
// Public interface functions
//=============================================================================

bool loadXdtsScene(ToonzScene* scene, const TFilePath& scenePath);

}  // namespace XdtsIo

#endif  // XDTSIO_H
