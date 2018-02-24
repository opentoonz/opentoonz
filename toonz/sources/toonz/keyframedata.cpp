

#include "keyframedata.h"
#include "tapp.h"
#include "toonzqt/tselectionhandle.h"
#include "keyframeselection.h"
#include "xsheetviewer.h"

#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txshcolumn.h"

#include <assert.h>

//=============================================================================
// TKeyframeData
//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData() {}

//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData(const TKeyframeData *src)
    : m_keyData(src->m_keyData)
    , m_isPegbarsCycleEnabled(src->m_isPegbarsCycleEnabled) {}

//-----------------------------------------------------------------------------

TKeyframeData::~TKeyframeData() {}

//-----------------------------------------------------------------------------
// data <- xsheet
void TKeyframeData::setKeyframes(std::set<Position> positions, TXsheet *xsh) {
  if (positions.empty()) return;

  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();

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
  for (++it2; it2 != positions.end(); ++it2) {
    r0 = std::min(r0, it2->first);
    c0 = std::min(c0, it2->second);
  }

  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();

  positions.clear();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
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
    if (pegbar->getId().isColumn() && xsh->getColumn(col) &&
        xsh->getColumn(col)->isLocked())
      continue;
    keyFrameChanged = true;
    assert(pegbar);
    pegbar->setKeyframeWithoutUndo(row, it->second);
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
