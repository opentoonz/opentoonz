#pragma once

#ifndef PEGBARCMD_INCLUDED
#define PEGBARCMD_INCLUDED

#include "tcommon.h"
#include <QPair>
#include <QPointF>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TStageObjectId;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TFxHandle;
class TStageObjectSpline;
class TStageObject;

namespace TStageObjectCmd {

DVAPI void rename(const TStageObjectId &id, std::string name,
                  TXsheetHandle *xshHandle);
DVAPI void resetOffset(const TStageObjectId &id, TXsheetHandle *xshHandle);
DVAPI void resetCenterAndOffset(const TStageObjectId &id,
                                TXsheetHandle *xshHandle);
DVAPI void resetPosition(const TStageObjectId &id, TXsheetHandle *xshHandle);
DVAPI void setHandle(const TStageObjectId &id, std::string handle,
                     TXsheetHandle *xshHandle);
DVAPI void setParentHandle(const std::vector<TStageObjectId> &ids,
                           std::string handle, TXsheetHandle *xshHandle);
DVAPI void setParent(const TStageObjectId &id, TStageObjectId parentId,
                     std::string parentHandle, TXsheetHandle *xshHandle,
                     bool doUndo = true);
DVAPI void setSplineParent(TStageObjectSpline *spline, TStageObject *parentObj,
                           TXsheetHandle *xshHandle);

DVAPI void addNewCamera(TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                        QPointF initialPos = QPointF());
DVAPI void addNewPegbar(TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                        QPointF initialPos = QPointF());
DVAPI void setAsActiveCamera(TXsheetHandle *xshHandle,
                             TObjectHandle *objHandle);
DVAPI void addNewSpline(TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                        TColumnHandle *colHandle,
                        QPointF initialPos = QPointF());
DVAPI void deleteSelection(
    const std::vector<TStageObjectId> &objIds,
    const std::list<QPair<TStageObjectId, TStageObjectId>> &links,
    const std::list<int> &splineIds, TXsheetHandle *xshHandle,
    TObjectHandle *objHandle, TFxHandle *fxHandle, bool doUndo = true);
DVAPI void group(const QList<TStageObjectId> ids, TXsheetHandle *xshHandle);
DVAPI void ungroup(int groupId, TXsheetHandle *xshHandle);
DVAPI void renameGroup(const QList<TStageObject *> objs,
                       const std::wstring &name, bool fromEditor,
                       TXsheetHandle *xshHandle);
DVAPI void duplicateObject(const QList<TStageObjectId> ids,
                           TXsheetHandle *xshHandle);
DVAPI void enableSplineAim(TStageObject *obj, int state,
                           TXsheetHandle *xshHandle);
DVAPI void enableSplineUppk(TStageObject *obj, bool toggled,
                            TXsheetHandle *xshHandle);
}

#endif
