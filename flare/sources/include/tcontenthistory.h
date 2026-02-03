#pragma once

#ifndef _CONTENT_HISTORY_H
#define _CONTENT_HISTORY_H

#include "tcommon.h"
#include "tfilepath.h"
#include <QString>
#include <QDateTime>
#include <map>
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! this class keeps tracks of modification times on contents, as well as user
//! and machine info;
//! typically used for levels and scene files (or whatever you like...)
// it is used for visualitation purposed only in the infoview, in example

class DVAPI TContentHistory {
  bool m_isLevel;
  std::map<TFrameId, QDateTime> m_records;
  QString m_frozenHistory;

  const QString currentToString() const;

public:
  //! set isLevel=true if you want to keep track of single-frames modifications
  TContentHistory(bool isLevel);

  ~TContentHistory();

  TContentHistory *clone() const;

  //! history can come only from a deserialization
  //! WARNING! any preexistent history will be erased by calling this method
  void deserialize(const QString &history);

  //! the returned string is only for visualitation and serialization
  const QString serialize() const;

  //! after calling this method, following history info added will not "merge"
  //! with the history until now
  void fixCurrentHistory();

  //! do not call this 2 methods if isLevel==false! (assertion fails)
  void frameModifiedNow(const TFrameId &id) { frameRangeModifiedNow(id, id); }
  void frameRangeModifiedNow(const TFrameId &fromId, const TFrameId &toId);

  //! do not call this method if isLevel==true! (assertion fails)
  void modifiedNow();
};

#endif
