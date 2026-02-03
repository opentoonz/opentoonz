#pragma once

#ifndef LEVELCOMMAND_H
#define LEVELCOMMAND_H

#include "toonz/txshcolumn.h"

#include <set>
#include <QList>

class TXshLevel;
class ToonzScene;

namespace LevelCmd {
void addMissingLevelsToCast(const QList<TXshColumnP>& columns);
void addMissingLevelsToCast(std::set<TXshLevel*>& levels);

// Remove all unused level from the scene cast.
// When there is no unused level, show an error message if showmessage==true.
// Return true if something is removed.
bool removeUnusedLevelsFromCast(bool showMessage = true);

// Remove the level from the scene cast if it is not used in the xsheet.
// Return true if the level is unused and removed.
// When the level is used, an show error message if showMessage==true and
// returns false.
bool removeLevelFromCast(TXshLevel* level, ToonzScene* scene = nullptr,
                         bool showMessage = true);

void loadAllUsedRasterLevelsAndPutInCache(bool cacheImagesAsWell);
}  // namespace LevelCmd

#endif