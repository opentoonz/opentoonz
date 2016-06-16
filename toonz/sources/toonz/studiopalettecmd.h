#pragma once

#ifndef STUDIOPALETTECMD_INCLUDED
#define STUDIOPALETTECMD_INCLUDED

#include "tcommon.h"

class TFilePath;
class TPalette;

namespace StudioPaletteCmd {

void loadIntoCurrentPalette();
void loadIntoCurrentPalette(const TFilePath &fp);
void mergeIntoCurrentPalette(const TFilePath &fp);
void replaceWithCurrentPalette(const TFilePath &fp);

void loadIntoCleanupPalette(const TFilePath &fp);
void replaceWithCleanupPalette(const TFilePath &fp);

void updateAllLinkedStyles();

void deletePalette(const TFilePath &fp);
void movePalette(const TFilePath &dstPath, const TFilePath &srcPath);
TFilePath createPalette(const TFilePath &folderPath, string paletteName,
                        const TPalette *palette);

TFilePath addFolder(const TFilePath &parentFolderPath);
void deleteFolder(const TFilePath &folderPath);

void scanPalettes(const TFilePath &folder, const TFilePath &sourcePath);
}

#endif
