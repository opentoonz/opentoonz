

#include "selectionutils.h"

// Tnz6 includes
#include "tapp.h"
#include "cellselection.h"
#include "castselection.h"
#include "filmstripselection.h"
#include "columnselection.h"

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"

// tcg includes
#include "tcg/boost/range_utility.h"

// Boost includes
#include <boost/range/counting_range.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace SelectionUtils{
    bool getSelectedLevels(std::set<TXshLevel*>& levels, int& r0, int& c0, int& r1,
        int& c1) {
        TXsheet* xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();

        CastSelection* castSelection =
            dynamic_cast<CastSelection*>(TSelection::getCurrent());
        TCellSelection* cellSelection =
            dynamic_cast<TCellSelection*>(TSelection::getCurrent());
        TColumnSelection* columnSelection =
            dynamic_cast<TColumnSelection*>(TSelection::getCurrent());

        if (castSelection) {
            std::vector<TXshLevel*> selectedLevels;
            castSelection->getSelectedLevels(selectedLevels);

            for (int i = 0; i < (int)selectedLevels.size(); ++i)
                levels.insert(selectedLevels[i]);

            return false;
        }
        else if (columnSelection) {
            std::set<int> indices = columnSelection->getIndices();
            if (indices.empty())return false;
            TColumnSelection::getLevelSetFromColumnIndices(indices, levels);
            r0 = INT_MAX, r1 = -1;
            c0 = INT_MAX, c1 = -1;
            for (int col : indices) {
                int a, b;
                xsheet->getCellRange(col, a, b);
                if (a < r0) r0 = a;
                if (b > r1) r1 = b;
                if (col < c0) c0 = col;
                if (col > c1) c1 = col;
            }
            return true;
        }
        else if (cellSelection) {
            cellSelection->getSelectedCells(r0, c0, r1, c1);

            for (int c = c0; c <= c1; ++c) {
                for (int r = r0; r <= r1; ++r) {
                    TXshCell cell = xsheet->getCell(r, c);

                    if (TXshLevel* level = cell.isEmpty() ? 0 : cell.getSimpleLevel())
                        levels.insert(level);
                }
            }

            return true;
        }

        return false;
    }
    bool getSelectedLevels(std::set<TXshLevel*>& levels){
        int r0, c0, r1, c1;
        return getSelectedLevels(levels, r0, c0, r1, c1);
    }
    bool getSelectedLevels(std::vector<TXshLevel*>& levels, int& r0, int& c0, int& r1,
        int& c1) {
        std::set<TXshLevel*> levelsSet;
        bool isCellSelection = getSelectedLevels(levelsSet, r0, c0, r1, c1);
        //Sort
        TXsheet* xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
        if (isCellSelection) {
            std::set<TXshLevel*> pushedLevels;
            for (int c = c0; c <= c1; c++) {
                for (int r = r0; r <= r1; r++) {
                    TXshCell cell = xsheet->getCell(r, c);
                    TXshSimpleLevel* level =
                        (!cell.isEmpty()) ? cell.getSimpleLevel() : 0;
                    if (level &&
                        pushedLevels.find(level) == pushedLevels.end()) {
                        pushedLevels.insert(level);
                        levels.push_back(level);
                        break;
                    }
                }
            }
        }
        else {
            levels = std::vector<TXshLevel*>(levelsSet.begin(), levelsSet.end());
            std::sort(levels.begin(), levels.end(), [](TXshLevel* a, TXshLevel* b) {
                return a->getName() < b->getName();
                });
        }
        return isCellSelection;
    }
}


//*********************************************************************************
//    Local namespace
//*********************************************************************************

namespace {

template <typename LevelType>
LevelType *getLevel(const TXshCell &cell) {
  return dynamic_cast<LevelType *>(cell.m_level.getPointer());
}

template <>
TXshLevel *getLevel<TXshLevel>(const TXshCell &cell) {
  return cell.m_level.getPointer();
}

//--------------------------------------------------------------------------------

template <typename LevelType>
void getSelectedFrames(CastSelection *castSelection,
                       std::map<LevelType *, std::set<TFrameId>> &frames) {}

template <>
void getSelectedFrames<TXshSimpleLevel>(
    CastSelection *castSelection,
    std::map<TXshSimpleLevel *, std::set<TFrameId>> &frames) {
  std::vector<TXshLevel *> levels;
  castSelection->getSelectedLevels(levels);

  int l, lSize = levels.size();
  for (l = 0; l != lSize; ++l) {
    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(levels[l]);
    if (!sl) continue;

    tcg::substitute(frames[sl], boost::counting_range(0, sl->getFrameCount()) |
      boost::adaptors::transformed([&sl](int index){ return sl->getFrameId(index); }));
  }
}

}  // namespace

//*********************************************************************************
//    Selection utility functions
//*********************************************************************************

template <typename LevelType>
void getSelectedFrames(const TXsheet &xsh, int r0, int c0, int r1, int c1,
                       std::map<LevelType *, std::set<TFrameId>> &frames) {
  for (int c = c0; c <= c1; ++c) {
    for (int r = r0; r <= r1; ++r) {
      const TXshCell &cell = xsh.getCell(r, c);

      if (LevelType *level = ::getLevel<LevelType>(cell))
        frames[level].insert(cell.getFrameId());
    }
  }
}

//--------------------------------------------------------------------------------

template <typename LevelType>
void getSelectedFrames(std::map<LevelType *, std::set<TFrameId>> &frames) {
  TSelection *selection = TSelection::getCurrent();

  if (TCellSelection *cellSelection =
          dynamic_cast<TCellSelection *>(selection)) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    int r0, c0, r1, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);

    getSelectedFrames(*xsh, r0, c0, r1, c1, frames);
    return;
  }

  if (TColumnSelection *columnSelection =
          dynamic_cast<TColumnSelection *>(selection)) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    const std::set<int> &indices = columnSelection->getIndices();

    std::set<int>::const_iterator it, iEnd(indices.end());
    for (it = indices.begin(); it != iEnd; ++it) {
      TXshCellColumn *column =
          dynamic_cast<TXshCellColumn *>(xsh->getColumn(*it));

      int r0, r1;
      column->getRange(r0, r1);

      for (int r = r0; r <= r1; ++r) {
        const TXshCell &cell = column->getCell(r);

        LevelType *level    = ::getLevel<LevelType>(cell);
        const TFrameId &fid = cell.getFrameId();

        if (level) frames[level].insert(fid);
      }
    }

    return;
  }

  if (CastSelection *castSelection = dynamic_cast<CastSelection *>(selection)) {
    getSelectedFrames<LevelType>(castSelection, frames);
    return;
  }

  if (TFilmstripSelection *filmstripSelection =
          dynamic_cast<TFilmstripSelection *>(selection)) {
    LevelType *level = dynamic_cast<LevelType *>(
        TApp::instance()->getCurrentLevel()->getLevel());
    if (level) frames[level] = filmstripSelection->getSelectedFids();

    return;
  }
}

//*********************************************************************************
//    Selection utility  explicit instantiations
//*********************************************************************************

// template void getSelectedFrames<TXshLevel>(const TXsheet&, int, int, int,
// int,
//                                                 std::map<TXshLevel*,
//                                                 std::set<TFrameId> >&);
template void getSelectedFrames<TXshSimpleLevel>(
    const TXsheet &, int, int, int, int,
    std::map<TXshSimpleLevel *, std::set<TFrameId>> &);
// template void getSelectedFrames<TXshChildLevel>(const TXsheet&, int, int,
// int, int,
//                                                 std::map<TXshChildLevel*,
//                                                 std::set<TFrameId> >&);

template void getSelectedFrames<TXshLevel>(
    std::map<TXshLevel *, std::set<TFrameId>> &);
// template void getSelectedFrames<TXshSimpleLevel>(std::map<TXshSimpleLevel*,
// std::set<TFrameId> >&);
// template void getSelectedFrames<TXshChildLevel>(std::map<TXshChildLevel*,
// std::set<TFrameId> >&);
