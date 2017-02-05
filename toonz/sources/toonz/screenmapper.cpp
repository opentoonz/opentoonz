#include "toonz/screenmapper.h"

#include "toonz/columnfan.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheet.h"

#include "tapp.h"
#include "xsheetviewer.h"

ScreenMapper::ScreenMapper(XsheetViewer *viewer)
  : m_viewer (viewer), m_orientation (nullptr), m_columnFan (nullptr), m_subLayers (nullptr) {
  m_orientation = Orientations::leftToRight();
  m_columnFan = new ColumnFan();
  m_subLayers = new SubLayers(this);
}

ScreenMapper::~ScreenMapper() {
  delete m_columnFan;
  delete m_subLayers;
}

TXsheet *ScreenMapper::xsheet() const {
  return m_viewer->getXsheet();
}


int ScreenMapper::getCurrentFrame() const {
  return TApp::instance()->getCurrentFrame()->getFrame();
}

void ScreenMapper::flipOrientation() {
  m_orientation = orientation ()->next();
}