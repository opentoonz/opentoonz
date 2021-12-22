

#include "keyframedata.h"
#include "tapp.h"
#include "toonzqt/tselectionhandle.h"
#include "keyframeselection.h"
#include "xsheetviewer.h"

#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txshcolumn.h"
#include "toonz/preferences.h"

#include <assert.h>

//=============================================================================
// TKeyframeData
//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData() {}

//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData(const TKeyframeData *src)
    : m_keyData(src->m_keyData)
    , m_isPegbarsCycleEnabled(src->m_isPegbarsCycleEnabled)
    , m_offset(src->m_offset) {}

//-----------------------------------------------------------------------------

TKeyframeData::~TKeyframeData() {}

//-----------------------------------------------------------------------------
// data <- xsheet
void TKeyframeData::setKeyframes(std::set<Position> positions, TXsheet *xsh) {
  Position startPos(-1, -1);
  setKeyframes(positions, xsh, startPos);
}

void TKeyframeData::setKeyframes(std::set<Position> positions, TXsheet *xsh,
                                 Position startPos) {
  if (positions.empty()) return;

  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());

  std::set<Position>::iterator it = positions.begin();
  int r0                          = it->first;
  int c0                          = it->second;
  int r1                          = it->first;
  int c1                          = it->second;
  for (++it; it != positions.end(); ++it) {
    r0 = std::min(r0, it->first);
    c0 = std::min(c0, it->second);
    r1 = std::max(r1, it->first);
    c1 = std::max(c1, it->second);
  }

  m_columnSpanCount = c1 - c0 + 1;
  m_rowSpanCount    = r1 - r0 + 1;

  if (startPos.first >= 0 && startPos.second >= 0)
    m_offset = std::make_pair(r0 - startPos.first, c0 - startPos.second);

  for (it = positions.begin(); it != positions.end(); ++it) {
    int row              = it->first;
    int col              = it->second;
    TStageObject *pegbar = xsh->getStageObject(
        col >= 0 ? TStageObjectId::ColumnId(col) : cameraId);
    assert(pegbar);
    m_isPegbarsCycleEnabled[col] = pegbar->isCycleEnabled();
    if (pegbar->isKeyframe(row)) {
      Position p(row - r0, col - c0);
      TStageObject::Keyframe k = pegbar->getKeyframe(row);
      m_keyData[p]             = k;
    }
  }
}

//-----------------------------------------------------------------------------

// data -> xsh
bool TKeyframeData::getKeyframes(std::set<Position> &positions,
                                 TXsheet *xsh) const {
  std::set<TKeyframeSelection::Position>::iterator it2 = positions.begin();
  int r0                                               = it2->first;
  int c0                                               = it2->second;
  std::map<int, int> firstRowCol, lastRowCol;
  firstRowCol.insert(std::pair<int, int>(c0, r0));
  lastRowCol.insert(std::pair<int, int>(c0, r0));
  for (++it2; it2 != positions.end(); ++it2) {
    r0 = std::min(r0, it2->first);
    c0 = std::min(c0, it2->second);
    std::map<int, int>::iterator itF = firstRowCol.find(it2->second);
    std::map<int, int>::iterator itL = lastRowCol.find(it2->second);
    if (itF == firstRowCol.end())
      firstRowCol.insert(std::pair<int, int>(it2->second, it2->first));
    if (itL == lastRowCol.end())
      lastRowCol.insert(std::pair<int, int>(it2->second, it2->first));
    else
      itL->second = c0;
  }

  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();

  positions.clear();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  Iterator it;
  bool keyFrameChanged = false;
  for (it = m_keyData.begin(); it != m_keyData.end(); ++it) {
    Position pos = it->first;
    int row      = r0 + pos.first;
    int col      = c0 + pos.second;
    positions.insert(std::make_pair(row, col));
    TXshColumn *column = xsh->getColumn(col);
    if (column && column->getSoundColumn()) continue;
    TStageObject *pegbar = xsh->getStageObject(
        col >= 0 ? TStageObjectId::ColumnId(col) : cameraId);
    if (xsh->getColumn(col) && xsh->getColumn(col)->isLocked()) continue;
    keyFrameChanged = true;
    assert(pegbar);

    int kF, kL, kP, kN;
    double e0, e1;
    pegbar->getKeyframeRange(kF, kL);
    pegbar->setKeyframeWithoutUndo(row, it->second);

    std::map<int, int>::iterator itF = firstRowCol.find(col);
    std::map<int, int>::iterator itL = lastRowCol.find(col);
    TStageObject::Keyframe newKey = pegbar->getKeyframe(row);
    // Process 1st key added in column
    if (itF != firstRowCol.end() && itF->second == row) {
      if (row > kL) {
        // If new key was added after the existing last one, create new
        // interpolation between them using preference setting
        TStageObject::Keyframe prevKey = pegbar->getKeyframe(kL);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              prevKey.m_channels[i].m_isKeyframe) {
            prevKey.m_channels[i].m_type = TDoubleKeyframe::Type(
                Preferences::instance()->getKeyframeType());
            newKey.m_channels[i].m_prevType = prevKey.m_channels[i].m_type;
          }
        }
        pegbar->setKeyframeWithoutUndo(kL, prevKey);
        pegbar->setKeyframeWithoutUndo(row, newKey);
      } else if (row > kF) {
        // If new key was added between existing keys, sync new key's previous
        // interpolation to key just before it
        if (!pegbar->getKeyframeSpan(row - 1, kP, e0, kN, e1)) kP = row - 1;
        TStageObject::Keyframe prevKey = pegbar->getKeyframe(kP);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              prevKey.m_channels[i].m_isKeyframe)
            newKey.m_channels[i].m_prevType = prevKey.m_channels[i].m_type; 
        }
        pegbar->setKeyframeWithoutUndo(row, newKey);
      }
    }
    // Process last key added in column
    if (itL != lastRowCol.end() && itL->second == row) {
      if (row < kF) {
        // If new key was added before the existing 1st one, create new
        // interpolation between them using preference setting
        TStageObject::Keyframe nextKey = pegbar->getKeyframe(kF);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              nextKey.m_channels[i].m_isKeyframe) {
            newKey.m_channels[i].m_type = TDoubleKeyframe::Type(
                Preferences::instance()->getKeyframeType());
            nextKey.m_channels[i].m_prevType = newKey.m_channels[i].m_type;
          }
        }
        pegbar->setKeyframeWithoutUndo(row, newKey);
        pegbar->setKeyframeWithoutUndo(kF, nextKey);
      } else if (row < kL) {
        // If new key was added between existing keys, sync new key to the next
        // key's previous interpolation
        if (!pegbar->getKeyframeSpan(row + 1, kP, e0, kN, e1)) kN = row + 1;
        TStageObject::Keyframe nextKey = pegbar->getKeyframe(kN);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              nextKey.m_channels[i].m_isKeyframe) {
            nextKey.m_channels[i].m_prevType = newKey.m_channels[i].m_type;
          }
        }
        pegbar->setKeyframeWithoutUndo(row, newKey);
      }
    }
  }
  if (!keyFrameChanged) return false;

  for (auto const pegbar : m_isPegbarsCycleEnabled) {
    int const col = pegbar.first;
    TStageObjectId objectId =
        (col >= 0) ? TStageObjectId::ColumnId(col) : cameraId;
    xsh->getStageObject(objectId)->enableCycle(pegbar.second);
  }
  return true;
}

//-----------------------------------------------------------------------------

void TKeyframeData::getKeyframes(std::set<Position> &positions) const {
  int r0 = positions.begin()->first;
  int c0 = positions.begin()->second;
  positions.clear();

  TKeyframeData::KeyData::const_iterator it;
  for (it = m_keyData.begin(); it != m_keyData.end(); ++it) {
    TKeyframeData::Position pos(it->first);
    positions.insert(
        TKeyframeSelection::Position(pos.first + r0, pos.second + c0));
  }
}

void TKeyframeData::setKeyframesOffset(int row, int col) {
  m_offset = std::make_pair(row, col);
}
