#pragma once

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

class TFilePath;
class TPalette;
class TPaletteHandle;
class TXshLevelHandle;
class TXsheetHandle;
class TXshSimpleLevel;

namespace StudioPaletteCmd {

DVAPI void loadIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                  const TFilePath &fp);
DVAPI void loadIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                  TPalette *palette);

DVAPI void loadIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                  TPalette *palette,
                                  TXshLevelHandle *currentLevelHandle,
                                  int tolerance);

DVAPI void mergeIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                   const TFilePath &fp);
DVAPI void mergeIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                   TPalette *palette);

DVAPI void replaceWithCurrentPalette(TPaletteHandle *paletteHandle,
                                     TPaletteHandle *stdPaletteHandle,
                                     const TFilePath &fp);

DVAPI void updateAllLinkedStyles(TPaletteHandle *paletteHandle,
                                 TXsheetHandle *xsheetHandle);

DVAPI void deletePalette(const TFilePath &fp);
DVAPI void movePalette(const TFilePath &dstPath, const TFilePath &srcPath);
DVAPI TFilePath createPalette(const TFilePath &folderPath,
                              std::string paletteName, const TPalette *palette);

DVAPI TFilePath addFolder(const TFilePath &parentFolderPath);
DVAPI void deleteFolder(const TFilePath &folderPath);

DVAPI void scanPalettes(const TFilePath &folder, const TFilePath &sourcePath);
}
