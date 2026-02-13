#pragma once

#ifndef BRUSHPRESETBRIDGE_H
#define BRUSHPRESETBRIDGE_H

#include <QString>
#include <QStringList>
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// Bridge to pass preset name from BrushPresetPanel to command handlers in tnztools.
// Used by MI_AddBrushPreset - avoids LNK2019 when calling tool methods from toonz.

namespace BrushPresetBridge {
DVAPI void setPendingPresetName(const QString &name);
DVAPI QString takePendingPresetName();

// Queue of preset names to be removed (for batch deletion from page delete)
DVAPI void addPendingRemoveName(const QString &name);
DVAPI QStringList takePendingRemoveNames();
}

#endif  // BRUSHPRESETBRIDGE_H
