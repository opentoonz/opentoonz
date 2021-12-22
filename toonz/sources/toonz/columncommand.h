#pragma once

#ifndef COLUMN_COMMAND_INCLUDED
#define COLUMN_COMMAND_INCLUDED

#include <set>
#include <QList>

#include "toonz/tstageobjectid.h"

class StageObjectsData;
class TFx;

namespace ColumnCmd {

void insertEmptyColumns(const std::set<int> &indices, bool insertAfter = false);
void insertEmptyColumn(int index);

void copyColumns(const std::set<int> &indices);

void cutColumns(std::set<int> &indices);
//! Delete columns from xsheet.
//! If \b onlyColumns is false all fxs in the branc attached to each node are
//! deleted
void deleteColumns(std::set<int> &indices, bool onlyColumns, bool withoutUndo);
//! helper function: deletes a single column, with undo
void deleteColumn(int index, bool onlyColumns = false);
//! if data==0 then uses clipboard
void pasteColumns(std::set<int> &indices, const StageObjectsData *data = 0);
//! helper function: copies srcIndex column and pastes it before dstIndex. Does
//! not affect the clipboard
void copyColumn(int dstIndex, int srcIndex);

void resequence(int index);
bool canResequence(int index);
void cloneChild(int index);
void clearCells(int index);

//! Adds an undo object for converting layer to vector.
void addConvertToVectorUndo(std::set<int> &newColumnIndices);

// "checkInvert" flag is ON when collapsing columns.
// expression references need to be checked in both way,
// the columns to be collapsed and other columns to be kept in the parent
// xsheet.

bool checkExpressionReferences(const std::set<int> &indices,
                               bool onlyColumns = true,
                               bool checkInvert = false);
bool checkExpressionReferences(const std::set<int> &indices,
                               const std::set<TFx *> &fxs,
                               bool onlyColumns = true,
                               bool checkInvert = false);
// checkInvert is always true for collapsing in stage schematic
bool checkExpressionReferences(const QList<TStageObjectId> &objects);

}  // namespace ColumnCmd

#endif
