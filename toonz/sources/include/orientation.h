#pragma once

#ifndef ORIENTATION_INCLUDED
#define ORIENTATION_INCLUDED

#include "cellposition.h"
#include <QPoint>
#include <QLine>
#include <QRect>

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
};

class ColumnFan;
class QPixmap;

class Orientation {
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

	//! where the old vertical line was for key frames
	virtual int keyLine_layerAxis (int layerAxis) const = 0;
	//! positions key icon in the middle of cell by the frame axis
	virtual int keyPixOffset (const QPixmap &pixmap) const = 0;

	virtual bool isVerticalTimeline () const = 0;
};

class Orientations {
	const Orientation *_topToBottom, *_leftToRight, *_rightToLeft;

public:
	Orientations ();
	~Orientations ();

	const Orientation *topToBottom () const { return _topToBottom; }
	const Orientation *leftToRight () const { return _leftToRight; }
	const Orientation *rightToLeft () const { return _rightToLeft; }
};

extern Orientations orientations;

#endif
