#pragma once

#ifndef TKEYFRAMESELECTION_H
#define TKEYFRAMESELECTION_H

#include "toonzqt/selection.h"
#include <set>

//=============================================================================
// TKeyframeSelection
//-----------------------------------------------------------------------------

class TKeyframeSelection final : public TSelection {
public:
  typedef std::pair<int, int> Position;  // row, col

public:
  std::set<Position> m_positions;

  TKeyframeSelection(std::set<Position> positions) : m_positions(positions) {}

  TKeyframeSelection() {}

  void enableCommands() override;
/* FIXME: clang
 * でテンポラリオブジェクトをアドレッシングしたとエラーになっていたので参照を返すようにしたが、元の意図が不明なので注意
 */
#if 0
  std::set<Position> getSelection(){ return m_positions; }
#else
  std::set<Position> &getSelection() { return m_positions; }
#endif
  void select(std::set<Position> positions) {
    clear();
    std::set<Position>::iterator it;
    for (it = positions.begin(); it != positions.end(); ++it)
      select(it->first, it->second);
  }

  void clear() { m_positions.clear(); }

  void selectNone() override { m_positions.clear(); }

  void select(int row, int col) {
    m_positions.insert(std::make_pair(row, col));
  }

  void unselect(int row, int col) {
    m_positions.erase(std::make_pair(row, col));
  }

  bool isSelected(int row, int col) const {
    return m_positions.find(std::make_pair(row, col)) != m_positions.end();
  }

  bool isEmpty() const override { return m_positions.empty(); }

  TSelection *clone() const { return new TKeyframeSelection(m_positions); }

  int getFirstRow() const;

  void unselectLockedColumn();
  bool select(const TSelection *s);

  void setKeyframes();

  void copyKeyframes();
  void pasteKeyframes();
  void deleteKeyframes();
  void cutKeyframes();
  void shiftKeyframes(int direction);
  void shiftKeyframesDown() { shiftKeyframes(1); }
  void shiftKeyframesUp() { shiftKeyframes(-1); }

  void pasteKeyframesWithShift(int r0, int r1, int c0, int c1);
  void deleteKeyframesWithShift(int r0, int r1, int c0, int c1);
  void shiftKeyframes(int r0, int r1, int c0, int c1,
                      bool shiftFollowing = true);
};

#endif  // TKEYFRAMESELECTION_H
