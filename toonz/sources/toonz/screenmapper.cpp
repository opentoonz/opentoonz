#include "toonz/screenmapper.h"

#include "toonz/columnfan.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheet.h"

#include "tapp.h"
#include "xsheetviewer.h"

#include <algorithm>

//-------------------------------------------------------------------------------------------
// SubLayerOffsets

SubLayerOffsets::SubLayerOffsets(const QPoint &topLeft, const QPoint &shifted,
                                 const QPoint &name)
    : m_topLeft(topLeft), m_shifted(shifted), m_name(name) {}

SubLayerOffsets SubLayerOffsets::forLayer(const QPoint &topLeft) {
  return SubLayerOffsets(topLeft, topLeft, topLeft);
}

//-------------------------------------------------------------------------------------------
// ScreenMapper

ScreenMapper::ScreenMapper(XsheetViewer *viewer)
    : m_viewer(viewer)
    , m_orientation(nullptr)
    , m_columnFan(nullptr)
    , m_subLayers(nullptr) {
  m_orientation = Orientations::leftToRight();
  m_columnFan   = new ColumnFanGeometry();
  m_subLayers   = new SubLayers(this);

  m_columnFan->setDimension(dimension(PredefinedDimension::LAYER));
}

ScreenMapper::~ScreenMapper() {
  delete m_columnFan;
  delete m_subLayers;
}

TXsheet *ScreenMapper::xsheet() const { return m_viewer->getXsheet(); }

int ScreenMapper::getCurrentFrame() const {
  return TApp::instance()->getCurrentFrame()->getFrame();
}

void ScreenMapper::flipOrientation() {
  m_orientation = orientation()->next();
  m_columnFan->setDimension(dimension(PredefinedDimension::LAYER));
}

CellPosition ScreenMapper::xyToPosition(const QPoint &point) const {
  return orientation()->xyToPosition(point, columnFan());
}
QPoint ScreenMapper::positionToXY(const CellPosition &pos) const {
  return orientation()->positionToXY(pos, columnFan());
}

int ScreenMapper::columnToLayerAxis(int layer) const {
  return orientation()->colToLayerAxis(layer, columnFan());
}
int ScreenMapper::rowToFrameAxis(int frame) const {
  return orientation()->rowToFrameAxis(frame);
}

NumberRange ScreenMapper::rowsToFrameAxis(const NumberRange &frames) const {
  return NumberRange(rowToFrameAxis(frames.from()),
                     rowToFrameAxis(frames.to()));
}
NumberRange ScreenMapper::colsToLayerAxis(const NumberRange &layers) const {
  return NumberRange(columnToLayerAxis(layers.from()),
                     columnToLayerAxis(layers.to()));
}

QPoint ScreenMapper::frameLayerToXY(int frameAxis, int layerAxis) const {
  return orientation()->frameLayerToXY(frameAxis, layerAxis);
}

//----------------------------------------------------------------------------------

void ScreenMapper::updateColumnFan() const {
  m_columnFan->updateExtras(xsheet()->getColumnFan(),
                            subLayers()->childrenDimensions(orientation()));
  m_viewer->update();
}

//----------------------------------------------------------------------------------

SubLayerOffsets ScreenMapper::subLayerOffsets(const TXshColumn *column,
                                              int subLayerIndex) const {
  int col     = column == nullptr ? -1 : column->getIndex();
  QPoint orig = m_viewer->positionToXY(CellPosition(0, std::max(col, 0)));

  QPoint baseLayerOffset =
      orig + frameLayerToXY(0, dimension(PredefinedDimension::LAYER));
  QPoint subLayerOffset =
      frameLayerToXY(0, dimension(PredefinedDimension::SUBLAYER));
  QPoint frameOffset =
      frameLayerToXY(dimension(PredefinedDimension::SUBLAYER_DEPTH), 0);

  vector<shared_ptr<SubLayer>> childrenTree =
      subLayers()->layer(column)->childrenFlatTree();
  shared_ptr<SubLayer> subLayer = childrenTree[subLayerIndex];

  QPoint layerOffset = baseLayerOffset + subLayerOffset * subLayerIndex;
  QPoint depthOffset = frameOffset * subLayer->depth();
  QPoint totalOffset = layerOffset + depthOffset;
  QPoint nameOffset  = totalOffset;
  if (subLayer->hasActivator()) {
    int frameSide = orientation()
                        ->frameSide(rect(PredefinedRect::SUBLAYER_ACTIVATOR))
                        .length();
    nameOffset = nameOffset + frameLayerToXY(frameSide, 0);
  }
  return SubLayerOffsets(layerOffset, totalOffset, nameOffset);
}
