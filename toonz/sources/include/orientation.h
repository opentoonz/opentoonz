#pragma once

#ifndef ORIENTATION_INCLUDED
#define ORIENTATION_INCLUDED

#include "cellposition.h"
#include <QPoint>
#include <QLine>
#include <QRect>
#include <QPainterPath>
#include <map>

using std::map;

// Defines timeline direction: top to bottom;  left to right; right to left.
// old (vertical timeline) = new (universal)    = old (kept)
//                x        =   layer axis       =   column
//                y        =   frame axis       =    row

class NumberRange {
	int _from, _to; // _from <= _to
public:
	NumberRange (int from, int to): _from (min (from, to)), _to (max (from, to)) { }

	int from () const { return _from; }
	int to () const { return _to; }

	int length () const { return _to - _from;  }
  int middle () const { return (_to + _from) / 2; }
};

class ColumnFan;
class QPixmap;
class QPainterPath;

//! lists predefined rectangle sizes and positions (relative to top left corner of a cell)
enum class PredefinedRect {
  CELL, //! size of a cell
  DRAG_HANDLE_CORNER, //! area for dragging a cell
  KEY_ICON, //! position of key icon
  CELL_NAME, //! cell name box
  CELL_NAME_WITH_KEYFRAME, //! cell name box when keyframe is displayed
  END_EXTENDER, //! bottom / right extender
  BEGIN_EXTENDER, //! top / left extender
  KEYFRAME_AREA, //! part of cell dedicated to key frames
  DRAG_AREA //! draggable side bar
};
enum class PredefinedLine {
  LOCKED, //! dotted vertical line when cell is locked
  SEE_MARKER_THROUGH, //! horizontal marker visible through drag handle
  CONTINUE_LEVEL, //! level with the same name represented by vertical line
  CONTINUE_LEVEL_WITH_NAME, //! adjusted when level name is on each marker
  EXTENDER_LINE //! see grid through extender handle
};
enum class PredefinedDimension {
  LAYER //! width of a layer column / height of layer row
};
enum class PredefinedPath {
  DRAG_HANDLE_CORNER, //! triangle corner at drag sidebar
  BEGIN_EASE_TRIANGLE, //! triangle marking beginning of ease range
  END_EASE_TRIANGLE //! triangle marking end of ease range
};
enum class PredefinedPoint {
  KEY_HIDDEN, //! move extender handle that much if key icons are disabled
  EXTENDER_XY_RADIUS //! x and y radius for rounded rectangle
};

class Orientation {
protected:
  map<PredefinedRect, QRect> _rects;
  map<PredefinedLine, QLine> _lines;
  map<PredefinedDimension, int> _dimensions;
  map<PredefinedPath, QPainterPath> _paths;
  map<PredefinedPoint, QPoint> _points;

public:
	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const = 0;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const = 0;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const = 0;
	virtual int rowToFrameAxis (int frame) const = 0;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const = 0;

	virtual NumberRange layerSide (const QRect &area) const = 0;
	virtual NumberRange frameSide (const QRect &area) const = 0;
  //! top right corner in vertical layout. bottom left in horizontal
  virtual QPoint topRightCorner (const QRect &area) const = 0;
	virtual QRect foldedRectangle (int layerAxis, const NumberRange &frameAxis, int i) const;
	virtual QLine foldedRectangleLine (int layerAxis, const NumberRange &frameAxis, int i) const;

	//! line was vertical in vertical timeline. adjust accordingly
	QLine verticalLine (int layerAxis, const NumberRange &frameAxis) const;
	QLine horizontalLine (int frameAxis, const NumberRange &layerAxis) const;

	virtual bool isVerticalTimeline () const = 0;

  const QRect &rect (PredefinedRect which) const { return _rects.at (which); }
  const QLine &line (PredefinedLine which) const { return _lines.at (which); }
  int dimension (PredefinedDimension which) const { return _dimensions.at (which); }
  const QPainterPath &path (PredefinedPath which) const { return _paths.at (which); }
  const QPoint &point (PredefinedPoint which) const { return _points.at (which); }
protected:
  void addRect (PredefinedRect which, const QRect &rect);
  void addLine (PredefinedLine which, const QLine &line);
  void addDimension (PredefinedDimension which, int dimension);
  void addPath (PredefinedPath which, const QPainterPath &path);
  void addPoint (PredefinedPoint which, const QPoint &point);
};

class Orientations {
	const Orientation *_topToBottom, *_leftToRight;

public:
	Orientations ();
	~Orientations ();

  const Orientation *topToBottom () const;
  const Orientation *leftToRight () const;
};

extern Orientations orientations;

#endif
