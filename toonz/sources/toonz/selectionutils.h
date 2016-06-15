#pragma once

#ifndef SELECTIONUTILS_H
#define SELECTIONUTILS_H

// TnzCore includes
#include "tfilepath.h"

// STL includes
#include <set>
#include <map>

//==============================================================

//    Forward declarations

class TXsheet;

//==============================================================

//*********************************************************************************
//    Selection-related  utility functions
//*********************************************************************************

//! Returns the set of all level frames contained in the specified xsheet cells
//! range
//! (r0 <= r <= r1 and c0 <= c <= c1).
template <typename LevelType>
void getSelectedFrames(
    const TXsheet &xsh, int r0, int c0, int r1, int c1,
    std::map<LevelType *, std::set<TFrameId>> &framesByLevel);

//! Returns the set of all level frames contained in current selection.
//! Recognized selection types include cell, cast, column and filmstrip
//! selections.
template <typename LevelType>
void getSelectedFrames(
    std::map<LevelType *, std::set<TFrameId>> &framesByLevel);

#endif  // SELECTIONUTILS_H
