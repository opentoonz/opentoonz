#pragma once

#ifndef SCREEN_MAPPER_INCLUDED
#define SCREEN_MAPPER_INCLUDED

#include "orientation.h"
#include "sublayers.h"

//! Class responsible for mapping logical (row, col) to screen XY and back
//! Gathers and hides all objects required to do so
//! All screen mapping requests should go through this,
//! so that when we want to change something it's done in single place.

//! Each XsheetViewer has a single instance of ScreenMapper.
//! ScreenMapper has internal state that lets XsheetViewer be customized
//! independently from other windows.

class XsheetViewer;

class ScreenMapper final {
  XsheetViewer *m_viewer;

  const Orientation *m_orientation;
  ColumnFan *m_columnFan;
  SubLayers *m_subLayers;
  // place for zoom-in variable here

public:
  ScreenMapper(XsheetViewer *viewer);
  ~ScreenMapper();

  TXsheet *xsheet() const;
  int getCurrentFrame() const;

  SubLayers *subLayers() const { return m_subLayers; }
  const Orientation *orientation() const { return m_orientation; }

  void flipOrientation();
};

#endif
