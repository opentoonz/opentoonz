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

  m_columnFan->setDimension(dimension(PredefinedDimension::LAYER));
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
  m_columnFan->setDimension(dimension(PredefinedDimension::LAYER));
}

CellPosition ScreenMapper::xyToPosition(const QPoint &point) const {
  return orientation ()->xyToPosition(point, columnFan());
}
QPoint ScreenMapper::positionToXY(const CellPosition &pos) const {
  return orientation ()->positionToXY(pos, columnFan());
}

int ScreenMapper::columnToLayerAxis(int layer) const {
  return orientation()->colToLayerAxis(layer, columnFan());
}
int ScreenMapper::rowToFrameAxis(int frame) const {
  return orientation()->rowToFrameAxis(frame);
}
