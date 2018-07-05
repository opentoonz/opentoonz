#pragma once

#ifndef COLUMN_COMMAND_INCLUDED
#define COLUMN_COMMAND_INCLUDED

#include <set>

class StageObjectsData;

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
void deleteColumn(int index);
//! if data==0 then uses clipboard
void pasteColumns(std::set<int> &indices, const StageObjectsData *data = 0);
//! helper function: copies srcIndex column and pastes it before dstIndex. Does
//! not affect the clipboard
void copyColumn(int dstIndex, int srcIndex);

void resequence(int index);
bool canResequence(int index);
void cloneChild(int index);
void clearCells(int index);

}  // namespace

#endif
