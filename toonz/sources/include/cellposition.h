#pragma once

#ifndef CELL_POSITION_INCLUDED
#define CELL_POSITION_INCLUDED

#include <algorithm>

using std::min;
using std::max;

// Identifies cells by frame and layer rather than row and column
class CellPosition {
  int _frame;  // a frame number. corresponds to row in vertical xsheet
  int _layer;  // a layer number. corresponds to col in vertical xsheet

public:
  CellPosition() : _frame(0), _layer(0) {}
  CellPosition(int frame, int layer) : _frame(frame), _layer(layer) {}

  int frame() const { return _frame; }
  int layer() const { return _layer; }

  void setFrame(int frame) { _frame = frame; }
  void setLayer(int layer) { _layer = layer; }

  CellPosition &operator=(const CellPosition &that) = default;

  operator bool() const { return _frame || _layer; }

  CellPosition operator+(const CellPosition &add) {
    return CellPosition(_frame + add._frame, _layer + add._layer);
  }
  CellPosition operator*(const CellPosition &mult) {
    return CellPosition(_frame * mult._frame, _layer * mult._layer);
  }
  void ensureValid() {
    if (_frame < 0) _frame = 0;
    if (_layer < 0) _layer = 0;
  }
};

// A square range identified by two corners
class CellRange {
  CellPosition _from, _to;  // from.frame <= to.frame && from.layer <= to.layer

public:
  CellRange() {}
  CellRange(const CellPosition &from, const CellPosition &to)
      : _from(min(from.frame(), to.frame()), min(from.layer(), to.layer()))
      , _to(max(from.frame(), to.frame()), max(from.layer(), to.layer())) {}

  const CellPosition &from() const { return _from; }
  const CellPosition &to() const { return _to; }

  CellRange &operator=(const CellRange &that) = default;
};

#endif
