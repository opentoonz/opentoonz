#include "orientation.h"
#include "toonz/columnfan.h"
#include "xsheetviewer.h"

#include <QPainterPath>

using std::pair;

Orientations orientations;

namespace {
  const int KEY_ICON_WIDTH = 11;
  const int KEY_ICON_HEIGHT = 13;
  const int EASE_TRIANGLE_SIZE = 4;
}

class TopToBottomOrientation : public Orientation {
  const int CELL_WIDTH = 74;
  const int CELL_HEIGHT = 20;
  const int CELL_DRAG_WIDTH = 7;
  const int EXTENDER_WIDTH = 20;
  const int EXTENDER_HEIGHT = 8;

public:
  TopToBottomOrientation ();

	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const override;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const override;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const override;
	virtual int rowToFrameAxis (int frame) const override;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const override;

	virtual NumberRange layerSide (const QRect &area) const override;
	virtual NumberRange frameSide (const QRect &area) const override;
  virtual QPoint topRightCorner (const QRect &area) const override;

	virtual bool isVerticalTimeline () const override { return true;  }
};

class LeftToRightOrientation : public Orientation {
  const int CELL_WIDTH = 47;
  const int CELL_HEIGHT = 47;
  const int CELL_DRAG_HEIGHT = 7;
  const int EXTENDER_WIDTH = 8;
  const int EXTENDER_HEIGHT = 20;

public:
  LeftToRightOrientation ();

	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const override;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const override;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const override;
	virtual int rowToFrameAxis (int frame) const override;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const override;

	virtual NumberRange layerSide (const QRect &area) const override;
	virtual NumberRange frameSide (const QRect &area) const override;
  virtual QPoint topRightCorner (const QRect &area) const override;

	virtual bool isVerticalTimeline () const override { return false; }
};

Orientations::Orientations (): _topToBottom (nullptr), _leftToRight (nullptr) {
	_topToBottom = new TopToBottomOrientation ();
	_leftToRight = new LeftToRightOrientation ();
}
Orientations::~Orientations () {
	delete _topToBottom; _topToBottom = nullptr;
	delete _leftToRight; _leftToRight = nullptr;
}

const Orientation *Orientations::topToBottom () const {
  if (!_topToBottom)
    throw new QString ("!_topToBottom");
  return _topToBottom;
}
const Orientation *Orientations::leftToRight () const {
  if (!_leftToRight)
    throw new QString ("!_leftToRight");
  return _leftToRight;
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
void Orientation::addLine (PredefinedLine which, const QLine &line) {
  _lines.insert (pair<PredefinedLine, QLine> (which, line));
}
void Orientation::addDimension (PredefinedDimension which, int dimension) {
  _dimensions.insert (pair<PredefinedDimension, int> (which, dimension));
}
void Orientation::addPath (PredefinedPath which, const QPainterPath &path) {
  _paths.insert (pair<PredefinedPath, QPainterPath> (which, path));
}
void Orientation::addPoint (PredefinedPoint which, const QPoint &point) {
  _points.insert (pair<PredefinedPoint, QPoint> (which, point));
}

/// -------------------------------------------------------------------------------

TopToBottomOrientation::TopToBottomOrientation () {
  QRect cellRect (0, 0, CELL_WIDTH, CELL_HEIGHT);
  addRect (PredefinedRect::CELL, cellRect);
  addRect (PredefinedRect::DRAG_HANDLE_CORNER, QRect (0, 0, CELL_DRAG_WIDTH, CELL_HEIGHT));
  QRect keyRect (CELL_WIDTH - KEY_ICON_WIDTH, (CELL_HEIGHT - KEY_ICON_HEIGHT) / 2, KEY_ICON_WIDTH, KEY_ICON_HEIGHT);
  addRect (PredefinedRect::KEY_ICON, keyRect);
  QRect nameRect = cellRect.adjusted (7, 4, -6, 0);
  addRect (PredefinedRect::CELL_NAME, nameRect);
  addRect (PredefinedRect::CELL_NAME_WITH_KEYFRAME, nameRect.adjusted (0, 0, -KEY_ICON_WIDTH, 0));
  addRect (PredefinedRect::END_EXTENDER, QRect (-EXTENDER_WIDTH - KEY_ICON_WIDTH, 1, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect (PredefinedRect::BEGIN_EXTENDER, QRect (-EXTENDER_WIDTH - KEY_ICON_WIDTH, -EXTENDER_HEIGHT, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect (PredefinedRect::KEYFRAME_AREA, QRect (CELL_WIDTH - KEY_ICON_WIDTH, 0, KEY_ICON_WIDTH, CELL_HEIGHT));
  addRect (PredefinedRect::DRAG_AREA, QRect (0, 0, CELL_DRAG_WIDTH, CELL_HEIGHT));

  addLine (PredefinedLine::LOCKED, verticalLine (CELL_DRAG_WIDTH / 2, NumberRange (0, CELL_HEIGHT)));
  addLine (PredefinedLine::SEE_MARKER_THROUGH, horizontalLine (0, NumberRange (0, CELL_DRAG_WIDTH)));
  addLine (PredefinedLine::CONTINUE_LEVEL, verticalLine (CELL_WIDTH / 2, NumberRange (0, CELL_HEIGHT)));
  addLine (PredefinedLine::CONTINUE_LEVEL_WITH_NAME, verticalLine (CELL_WIDTH - 11, NumberRange (0, CELL_HEIGHT)));
  addLine (PredefinedLine::EXTENDER_LINE, horizontalLine (0, NumberRange (-EXTENDER_WIDTH - KEY_ICON_WIDTH, 0)));

  addDimension (PredefinedDimension::LAYER, CELL_WIDTH);

  QPainterPath corner (QPointF (0, CELL_HEIGHT));
  corner.lineTo (QPointF (CELL_DRAG_WIDTH, CELL_HEIGHT));
  corner.lineTo (QPointF (CELL_DRAG_WIDTH, CELL_HEIGHT - CELL_DRAG_WIDTH));
  corner.lineTo (QPointF (0, CELL_HEIGHT));
  addPath (PredefinedPath::DRAG_HANDLE_CORNER, corner);

  QPainterPath fromTriangle (QPointF (0, EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo (QPointF (EASE_TRIANGLE_SIZE, -EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo (QPointF (-EASE_TRIANGLE_SIZE, -EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo (QPointF (0, EASE_TRIANGLE_SIZE / 2));
  fromTriangle.translate (keyRect.center ());
  addPath (PredefinedPath::BEGIN_EASE_TRIANGLE, fromTriangle);

  QPainterPath toTriangle (QPointF (0, -EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo (QPointF (EASE_TRIANGLE_SIZE, EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo (QPointF (-EASE_TRIANGLE_SIZE, EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo (QPointF (0, -EASE_TRIANGLE_SIZE / 2));
  toTriangle.translate (keyRect.center ());
  addPath (PredefinedPath::END_EASE_TRIANGLE, toTriangle);

  addPoint (PredefinedPoint::KEY_HIDDEN, QPoint (KEY_ICON_WIDTH, 0));
  addPoint (PredefinedPoint::EXTENDER_XY_RADIUS, QPoint (30, 75));
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
QPoint TopToBottomOrientation::topRightCorner (const QRect &area) const {
  return area.topRight ();
}


/// --------------------------------------------------------------------------------


LeftToRightOrientation::LeftToRightOrientation () {
  QRect cellRect (0, 0, CELL_WIDTH, CELL_HEIGHT);
  addRect (PredefinedRect::CELL, cellRect);
  addRect (PredefinedRect::DRAG_HANDLE_CORNER, QRect (0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));
  QRect keyRect ((CELL_WIDTH - KEY_ICON_WIDTH) / 2, CELL_HEIGHT - KEY_ICON_HEIGHT, KEY_ICON_WIDTH, KEY_ICON_HEIGHT);
  addRect (PredefinedRect::KEY_ICON, keyRect);
  QRect nameRect = cellRect.adjusted (7, 4, -6, 0);
  addRect (PredefinedRect::CELL_NAME, nameRect);
  addRect (PredefinedRect::CELL_NAME_WITH_KEYFRAME, nameRect.adjusted (0, 0, 0, -KEY_ICON_HEIGHT));
  addRect (PredefinedRect::END_EXTENDER, QRect (1, -EXTENDER_HEIGHT - KEY_ICON_HEIGHT, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect (PredefinedRect::BEGIN_EXTENDER, QRect (-EXTENDER_WIDTH, -EXTENDER_HEIGHT - KEY_ICON_HEIGHT, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect (PredefinedRect::KEYFRAME_AREA, QRect (0, CELL_HEIGHT - KEY_ICON_HEIGHT, CELL_WIDTH, KEY_ICON_HEIGHT));
  addRect (PredefinedRect::DRAG_AREA, QRect (0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));

  addLine (PredefinedLine::LOCKED, verticalLine (CELL_DRAG_HEIGHT / 2, NumberRange (0, CELL_WIDTH)));
  addLine (PredefinedLine::SEE_MARKER_THROUGH, horizontalLine (0, NumberRange (0, CELL_DRAG_HEIGHT)));
  addLine (PredefinedLine::CONTINUE_LEVEL, verticalLine (CELL_HEIGHT / 2, NumberRange (0, CELL_WIDTH)));
  addLine (PredefinedLine::CONTINUE_LEVEL_WITH_NAME, verticalLine (CELL_HEIGHT / 2, NumberRange (0, CELL_WIDTH)));
  addLine (PredefinedLine::EXTENDER_LINE, horizontalLine (0, NumberRange (-EXTENDER_WIDTH - KEY_ICON_WIDTH, 0)));

  addDimension (PredefinedDimension::LAYER, CELL_HEIGHT);

  QPainterPath corner (QPointF (CELL_WIDTH, 0));
  corner.lineTo (QPointF (CELL_WIDTH, CELL_DRAG_HEIGHT));
  corner.lineTo (QPointF (CELL_WIDTH - CELL_DRAG_HEIGHT, CELL_DRAG_HEIGHT));
  corner.lineTo (QPointF (CELL_WIDTH, 0));
  addPath (PredefinedPath::DRAG_HANDLE_CORNER, corner);

  QPainterPath fromTriangle (QPointF (EASE_TRIANGLE_SIZE / 2, 0));
  fromTriangle.lineTo (QPointF (-EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo (QPointF (-EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo (QPointF (EASE_TRIANGLE_SIZE / 2, 0));
  fromTriangle.translate (keyRect.center ());
  addPath (PredefinedPath::BEGIN_EASE_TRIANGLE, fromTriangle);

  QPainterPath toTriangle (QPointF (-EASE_TRIANGLE_SIZE / 2, 0));
  toTriangle.lineTo (QPointF (EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  toTriangle.lineTo (QPointF (EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  toTriangle.lineTo (QPointF (-EASE_TRIANGLE_SIZE / 2, 0));
  toTriangle.translate (keyRect.center ());
  addPath (PredefinedPath::END_EASE_TRIANGLE, toTriangle);

  addPoint (PredefinedPoint::KEY_HIDDEN, QPoint (0, KEY_ICON_HEIGHT));
  addPoint (PredefinedPoint::EXTENDER_XY_RADIUS, QPoint (75, 30));
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
QPoint LeftToRightOrientation::topRightCorner (const QRect &area) const {
  return area.bottomLeft ();
}
