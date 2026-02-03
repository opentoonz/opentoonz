#pragma once

#ifndef TOBJECTHANDLE_H
#define TOBJECTHANDLE_H

#include <QObject>

#include "toonz/tstageobjectid.h"
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

class TVectorImage;
class TStageObjectSpline;

//=============================================================================
// TObjectHandle
//-----------------------------------------------------------------------------
/*!
  Mantiene un riferimento ad un particolare oggetto (da editare)
  L'oggetto e' una objectId : puo' essere il tavolo, la camera, una colonna o
  una
  pegbar propriamente detta.
  Puo' anche essere la spline associata all'oggetto
*/

class DVAPI TObjectHandle final : public QObject {
  Q_OBJECT

  TStageObjectId m_objectId;
  bool m_isSpline;
  TVectorImage *m_splineImage;
  TStageObjectSpline *m_currentSpline;

public:
  TObjectHandle();
  ~TObjectHandle();

  TStageObjectId getObjectId();
  void setObjectId(TStageObjectId objectId);

  void notifyObjectIdChanged(bool isDragging) {
    emit objectChanged(isDragging);
  }
  void notifyObjectIdSwitched() { emit objectSwitched(); }

  bool isSpline() const { return m_isSpline; }
  void setIsSpline(bool isSpline);

  TVectorImage *getSplineImage() const { return m_splineImage; }
  void setSplineObject(TStageObjectSpline *splineObject);

signals:
  void objectSwitched();
  void objectChanged(bool isDragging);
  void splineChanged();

public slots:
  void commitSplineChanges();
};

#endif  // TOBJECTHANDLE_H
