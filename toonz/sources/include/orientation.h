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
  KEY_ICON //! position of key icon
};
enum class PredefinedLine {
  LOCKED //! dotted vertical line when cell is locked
};
enum class PredefinedDimension {
  LAYER //! width of a layer column / height of layer row
};
enum class PredefinedPath {
  DRAG_HANDLE_CORNER, //! triangle corner at drag sidebar
  BEGIN_EASE_TRIANGLE, //! triangle marking beginning of ease range
  END_EASE_TRIANGLE //! triangle marking end of ease range
};

class Orientation {
protected:
  map<PredefinedRect, QRect> _rects;
  map<PredefinedLine, QLine> _lines;
  map<PredefinedDimension, int> _dimensions;
  map<PredefinedPath, QPainterPath> _paths;

public:
	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const = 0;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const = 0;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const = 0;
	virtual int rowToFrameAxis (int frame) const = 0;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const = 0;

	virtual NumberRange layerSide (const QRect &area) const = 0;
	virtual NumberRange frameSide (const QRect &area) const = 0;
	virtual QRect foldedRectangle (int layerAxis, const NumberRange &frameAxis, int i) const;
	virtual QLine foldedRectangleLine (int layerAxis, const NumberRange &frameAxis, int i) const;

	//! line was vertical in vertical timeline. adjust accordingly
	QLine verticalLine (int layerAxis, const NumberRange &frameAxis) const;
	QLine horizontalLine (int frameAxis, const NumberRange &layerAxis) const;

	virtual bool isVerticalTimeline () const = 0;

  virtual const QRect &rect (PredefinedRect which) const { return _rects.at (which); }
  virtual const QLine &line (PredefinedLine which) const { return _lines.at (which); }
  virtual int dimension (PredefinedDimension which) const { return _dimensions.at (which); }
  virtual const QPainterPath &path (PredefinedPath which) const { return _paths.at (which); }
protected:
  void addRect (PredefinedRect which, const QRect &rect);
  void addLine (PredefinedLine which, const QLine &line);
  void addDimension (PredefinedDimension which, int dimension);
  void addPath (PredefinedPath which, const QPainterPath &path);
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
