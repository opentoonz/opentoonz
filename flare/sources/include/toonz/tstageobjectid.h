#pragma once

#ifndef T_STAGE_OBJECT_ID_INCLUDED
#define T_STAGE_OBJECT_ID_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//! The class TStageObjectId permits the specialization of a generic stage
//! object.
/*!The possible type of a stage object are: table TableId, camera CameraId,
pegbar PegbarId,
   column ColumnId and spline SplineId; the class allows to know object type:
isCamera(),
   isTable(), isPegbar(), isColumn() and isSpline().
\n For each specialization you give to each stage object a sequential number.
   The class provides methods to get sequential number getIndex() and objectId
name
   toString().
   It's possible to compare different stage object using defined operator,
operator==(),
   operator!=(), operator<(), operator<=(), operator>() and operator>=().
*/
//=============================================================================

class DVAPI TStageObjectId {
  typedef unsigned int Code;
  Code m_id;

  explicit TStageObjectId(Code id) : m_id(id) {}

public:
  TStageObjectId();
  ~TStageObjectId();

  /*!
Return true if this TStageObjectId and the given \b p have
different identification; otherwise return false.
*/
  inline bool operator!=(const TStageObjectId &p) const {
    return m_id != p.m_id;
  }
  /*!
Return true if this TStageObjectId and the given \b p have the same
identification;
otherwise return false.
*/
  inline bool operator==(const TStageObjectId &p) const {
    return m_id == p.m_id;
  }
  /*!
Return true if this TStageObjectId identification is smaller than
\b p identification; otherwise return false.
*/
  inline bool operator<(const TStageObjectId &p) const { return m_id < p.m_id; }
  /*!
Return true if this TStageObjectId identification is greater than
\b p identification; otherwise return false.
*/
  inline bool operator>(const TStageObjectId &p) const { return m_id > p.m_id; }
  /*!
Return true if this TStageObjectId identification is equal or smaller than
\b p identification; otherwise return false.
*/
  inline bool operator<=(const TStageObjectId &p) const {
    return m_id <= p.m_id;
  }
  /*!
Return true if this TStageObjectId identification is equal or greater than
\b p identification; otherwise return false.
*/
  inline bool operator>=(const TStageObjectId &p) const {
    return m_id >= p.m_id;
  }

  /*!
Return std::string with this TStageObjectId short name.
*/
  std::string toString() const;
  int getIndex() const;

  /*!
Return true if this TStageObjectId is CameraId; otherwise return false.
\sa isTable(), isPegbar(), isColumn() and isSpline().
*/
  bool isCamera() const;
  /*!
Return true if this TStageObjectId is table; otherwise return false.
\sa isCamera(), isPegbar(), isColumn() and isSpline()
*/
  bool isTable() const;
  /*!
Return true if this TStageObjectId is pegbar; otherwise return false.
\sa isCamera(), isTable(), isColumn() and isSpline()
*/
  bool isPegbar() const;
  /*!
Return true if this TStageObjectId is column; otherwise return false.
\sa isCamera(), isTable(), isPegbar() and isSpline()
*/
  bool isColumn() const;
  /*!
Return true if this TStageObjectId is spline; otherwise return false.
\sa isCamera(), isTable(), isPegbar() and isColumn()
*/
  bool isSpline() const;

  static const TStageObjectId NoneId;
  static const TStageObjectId CameraId(int index);
  static const TStageObjectId TableId;
  static const TStageObjectId PegbarId(int index);
  static const TStageObjectId ColumnId(int index);
  static const TStageObjectId SplineId(int index);

  Code getCode() const { return m_id; }
  void setCode(Code id) { m_id = id; }
};

//-----------------------------------------------------------------------------

std::ostream &operator<<(std::ostream &out, const TStageObjectId &id);

//-----------------------------------------------------------------------------

/*!
  Return a \b TStageObjectId from \b string \b s.
*/
TStageObjectId toStageObjectId(std::string s);

#endif
