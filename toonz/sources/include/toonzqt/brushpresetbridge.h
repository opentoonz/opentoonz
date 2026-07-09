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

class TColorStyle;
class TPalette;
class TPaletteHandle;
class TUndo;

// Bridge to pass preset name and mode flags from BrushPresetPanel (toonz)
// to command handlers in tnztools. Avoids LNK2019 cross-DLL symbol issues.

namespace BrushPresetBridge {
DVAPI void setPendingPresetName(const QString &name);
DVAPI QString takePendingPresetName();

// Queue of preset names to be removed (for batch deletion from page delete)
DVAPI void addPendingRemoveName(const QString &name);
DVAPI QStringList takePendingRemoveNames();

// Toggle states shared between BrushPresetPanel (toonz) and brush tools (tnztools)
DVAPI void setNonDestructiveMode(bool enabled);
DVAPI bool isNonDestructiveMode();
DVAPI void setSelectivePresetMode(bool enabled);
DVAPI bool isSelectivePresetMode();

// Style matching for Non-Destructive mode: compare style parameters
// ignoring metadata (name, globalName, flags, pickedPosition)
DVAPI bool stylesMatchVisually(const TColorStyle *a, const TColorStyle *b);
DVAPI int findMatchingStyleInPalette(TPalette *palette,
                                     const TColorStyle *targetStyle);

// Creates a TUndo that records a style overwrite (for Ctrl+Z support).
// oldStyle and newStyle are cloned internally; caller retains ownership.
DVAPI TUndo *createStyleOverwriteUndo(TPalette *palette, int styleId,
                                      const TColorStyle *oldStyle,
                                      const TColorStyle *newStyle,
                                      TPaletteHandle *paletteHandle);
}

#endif  // BRUSHPRESETBRIDGE_H
