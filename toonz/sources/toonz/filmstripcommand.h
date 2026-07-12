#pragma once

#ifndef FILMSTRIPCOMMAND_H
#define FILMSTRIPCOMMAND_H

#include "tcommon.h"
#include "tfilepath.h"

#include <set>
#include <vector>

// Forward declarations
class TXshSimpleLevel;
class TXshLevel;
class TXshPaletteLevel;
class TXshSoundLevel;

//=============================================================================
// FilmStripCmd namespace
//-----------------------------------------------------------------------------

namespace FilmstripCmd {

// Add empty frames to the level
void addFrames(TXshSimpleLevel *sl, int start, int end, int step);

// Renumber frames in the level
void renumber(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int startFrame,
              int stepFrame);
void renumber(TXshSimpleLevel *sl,
              const std::vector<std::pair<TFrameId, TFrameId>> &table,
              bool forceCallUpdateXSheet = false);

// Copy/paste operations for frames
void copy(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void paste(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void merge(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void pasteInto(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void cut(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void clear(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void remove(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void insert(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
            bool withUndo);

// Frame manipulation operations
void reverse(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void swing(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void step(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int step);
void each(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int each);

// Duplicate operations
void duplicateFrameWithoutUndo(TXshSimpleLevel *sl, TFrameId srcFrame,
                               TFrameId targetFrame);
void duplicate(TXshSimpleLevel *sl, std::set<TFrameId> &frames, bool withUndo);

// // TODO: Need to be moved to another location - Level to scene operations
void moveToScene(TXshLevel *sl, std::set<TFrameId> &frames);
void moveToScene(TXshSimpleLevel *sl);
void moveToScene(TXshPaletteLevel *pl);
void moveToScene(TXshSoundLevel *sl);

// Inbetween interpolation types and functions
enum InbetweenInterpolation { II_Linear, II_EaseIn, II_EaseOut, II_EaseInOut };

void inbetweenWithoutUndo(TXshSimpleLevel *sl, const TFrameId &fid0,
                          const TFrameId &fid1,
                          InbetweenInterpolation interpolation);
void inbetween(TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
               InbetweenInterpolation interpolation);

// Renumber a single drawing
void renumberDrawing(TXshSimpleLevel *sl, const TFrameId &oldFid,
                     const TFrameId &desiredNewFid);

}  // namespace FilmstripCmd

// Operator overload for TFrameId
TFrameId operator+(const TFrameId &fid, int d);

// Utility function to make space for frame IDs in a level
void makeSpaceForFids(TXshSimpleLevel *sl,
                      const std::set<TFrameId> &framesToInsert);

#endif  // FILMSTRIPCOMMAND_H
