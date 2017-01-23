#include "orientation.h"
#include "toonz/columnfan.h"
#include "xsheetviewer.h"

using std::pair;

Orientations orientations;

class TopToBottomOrientation : public Orientation {
  const int CELL_WIDTH = 74;
  const int CELL_HEIGHT = 20;

public:
  TopToBottomOrientation ();

	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const override;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const override;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const override;
	virtual int rowToFrameAxis (int frame) const override;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const override;

	virtual NumberRange layerSide (const QRect &area) const override;
	virtual NumberRange frameSide (const QRect &area) const override;

	virtual int keyLine_layerAxis (int layerAxis) const override;
	virtual int keyPixOffset (const QPixmap &pixmap) const override;

	virtual bool isVerticalTimeline () const override { return true;  }
};

class LeftToRightOrientation : public Orientation {
  const int CELL_WIDTH = 47;
  const int CELL_HEIGHT = 47;

public:
  LeftToRightOrientation ();

	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const override;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const override;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const override;
	virtual int rowToFrameAxis (int frame) const override;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const override;

	virtual NumberRange layerSide (const QRect &area) const override;
	virtual NumberRange frameSide (const QRect &area) const override;

	virtual int keyLine_layerAxis (int layerAxis) const override;
	virtual int keyPixOffset (const QPixmap &pixmap) const override;

	virtual bool isVerticalTimeline () const override { return false; }
};

Orientations::Orientations () {
	_topToBottom = new TopToBottomOrientation ();
	_leftToRight = new LeftToRightOrientation ();
}
Orientations::~Orientations () {
	delete _topToBottom; _topToBottom = nullptr;
	delete _leftToRight; _leftToRight = nullptr;
}

/// -------------------------------------------------------------------------------

QLine Orientation::verticalLine (int layerAxis, const NumberRange &frameAxis) const {
	QPoint first = frameLayerToXY (frameAxis.from (), layerAxis);
	QPoint second = frameLayerToXY (frameAxis.to (), layerAxis);
	return QLine (first, second);
}
QLine Orientation::horizontalLine (int frameAxis, const NumberRange &layerAxis) const {
	QPoint first = frameLayerToXY (frameAxis, layerAxis.from ());
	QPoint second = frameLayerToXY (frameAxis, layerAxis.to ());
	return QLine (first, second);
}

QRect Orientation::foldedRectangle (int layerAxis, const NumberRange &frameAxis, int i) const {
	QPoint topLeft = frameLayerToXY (frameAxis.from (), layerAxis + 1 + i * 3);
	QPoint size = frameLayerToXY (frameAxis.length (), 2);
	return QRect (topLeft, QSize (size.x (), size.y ()));
}
QLine Orientation::foldedRectangleLine (int layerAxis, const NumberRange &frameAxis, int i) const {
	return verticalLine (layerAxis + i * 3, frameAxis);
}

void Orientation::addRect (PredefinedRect which, const QRect &rect) {
  _rects.insert (pair<PredefinedRect, QRect> (which, rect));
}
void Orientation::addDimension (PredefinedDimension which, int dimension) {
  _dimensions.insert (pair<PredefinedDimension, int> (which, dimension));
}

/// -------------------------------------------------------------------------------

TopToBottomOrientation::TopToBottomOrientation () {
  addRect (PredefinedRect::CELL, QRect (0, 0, CELL_WIDTH, CELL_HEIGHT));

  addDimension (PredefinedDimension::LAYER, CELL_WIDTH);
}
CellPosition TopToBottomOrientation::xyToPosition (const QPoint &xy, const ColumnFan *fan) const {
	int layer = fan->layerAxisToCol (xy.x ());
	int frame = xy.y () / CELL_HEIGHT;
	return CellPosition (frame, layer);
}
QPoint TopToBottomOrientation::positionToXY (const CellPosition &position, const ColumnFan *fan) const {
	int x = colToLayerAxis (position.layer (), fan);
	int y = rowToFrameAxis (position.frame ());
	return QPoint (x, y);
}
int TopToBottomOrientation::colToLayerAxis (int layer, const ColumnFan *fan) const {
	return fan->colToLayerAxis (layer);
}
int TopToBottomOrientation::rowToFrameAxis (int frame) const {
	return frame * CELL_HEIGHT;
}
QPoint TopToBottomOrientation::frameLayerToXY (int frameAxis, int layerAxis) const {
	return QPoint (layerAxis, frameAxis);
}
NumberRange TopToBottomOrientation::layerSide (const QRect &area) const {
	return NumberRange (area.left (), area.right ());
}
NumberRange TopToBottomOrientation::frameSide (const QRect &area) const {
	return NumberRange (area.top (), area.bottom ());
}
int TopToBottomOrientation::keyLine_layerAxis (int layerAxis) const {
	int x = layerAxis;
	return x + CELL_WIDTH - 8;
}
int TopToBottomOrientation::keyPixOffset (const QPixmap &pixmap) const {
	return (CELL_HEIGHT - pixmap.height ()) / 2;
}


/// --------------------------------------------------------------------------------


LeftToRightOrientation::LeftToRightOrientation () {
  addRect (PredefinedRect::CELL, QRect (0, 0, CELL_WIDTH, CELL_HEIGHT));

  addDimension (PredefinedDimension::LAYER, CELL_HEIGHT);
}
CellPosition LeftToRightOrientation::xyToPosition (const QPoint &xy, const ColumnFan *fan) const {
	int layer = fan->layerAxisToCol (xy.y ());
	int frame = xy.x () / CELL_WIDTH;
	return CellPosition (frame, layer);
}
QPoint LeftToRightOrientation::positionToXY (const CellPosition &position, const ColumnFan *fan) const {
	int y = colToLayerAxis (position.layer (), fan);
	int x = rowToFrameAxis (position.frame ());
	return QPoint (x, y);
}
int LeftToRightOrientation::colToLayerAxis (int layer, const ColumnFan *fan) const {
	return fan->colToLayerAxis (layer);
}
int LeftToRightOrientation::rowToFrameAxis (int frame) const {
	return frame * CELL_WIDTH;
}
QPoint LeftToRightOrientation::frameLayerToXY (int frameAxis, int layerAxis) const {
	return QPoint (frameAxis, layerAxis);
}
NumberRange LeftToRightOrientation::layerSide (const QRect &area) const {
	return NumberRange (area.top (), area.bottom ());
}
NumberRange LeftToRightOrientation::frameSide (const QRect &area) const {
	return NumberRange (area.left (), area.right ());
}
int LeftToRightOrientation::keyLine_layerAxis (int layerAxis) const {
	int y = layerAxis;
	return y + 5;
}
int LeftToRightOrientation::keyPixOffset (const QPixmap &pixmap) const {
	return (CELL_WIDTH - pixmap.width ()) / 2;
}
