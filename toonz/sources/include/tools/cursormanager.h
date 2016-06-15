#pragma once

#ifndef CURSORMANAGER_H
#define CURSORMANAGER_H

#include "tcommon.h"

#include <QCursor>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QWidget;

DVAPI void setToolCursor(QWidget *viewer, int cursorId);
DVAPI QCursor getToolCursor(int cursorId);

#endif  // CURSORMANAGER_H
