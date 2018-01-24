#pragma once

#ifndef FILMSTRIPCOMMAND_H
#define FILMSTRIPCOMMAND_H

#include "tcommon.h"
#include "tfilepath.h"

#include <set>

class TXshSimpleLevel;
class TXshLevel;
class TXshPaletteLevel;
class TXshSoundLevel;

//=============================================================================
// FilmStripCmd namespace
//-----------------------------------------------------------------------------

namespace FilmstripCmd {
void addFrames(TXshSimpleLevel *sl, int start, int end, int step);

void renumber(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int startFrame,
              int stepFrame);
void renumber(TXshSimpleLevel *sl,
              const std::vector<std::pair<TFrameId, TFrameId>> &table,
              bool forceCallUpdateXSheet = false);

void copy(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void paste(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void merge(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void pasteInto(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void cut(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void clear(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void insert(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
            bool withUndo);

void reverse(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void swing(TXshSimpleLevel *sl, std::set<TFrameId> &frames);
void step(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int step);
void each(TXshSimpleLevel *sl, std::set<TFrameId> &frames, int each);

void duplicate(TXshSimpleLevel *sl, std::set<TFrameId> &frames, bool withUndo);

// TODO vanno spostati in un altro posto
void moveToScene(TXshLevel *sl, std::set<TFrameId> &frames);
void moveToScene(TXshSimpleLevel *sl);
void moveToScene(TXshPaletteLevel *pl);
void moveToScene(TXshSoundLevel *sl);

enum InbetweenInterpolation { II_Linear, II_EaseIn, II_EaseOut, II_EaseInOut };
void inbetweenWithoutUndo(TXshSimpleLevel *sl, const TFrameId &fid0,
                          const TFrameId &fid1,
                          InbetweenInterpolation interpolation);
void inbetween(TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
               InbetweenInterpolation interpolation);

void renumberDrawing(TXshSimpleLevel *sl, const TFrameId &oldFid,
                     const TFrameId &desiredNewFid);
}

TFrameId operator+(const TFrameId &fid, int d);

void makeSpaceForFids(TXshSimpleLevel *sl,
                      const std::set<TFrameId> &framesToInsert);

#endif  // FILMSTRIPCOMMAND_H
