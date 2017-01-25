#pragma once

#ifndef ORIENTATION_INCLUDED
#define ORIENTATION_INCLUDED

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include "tcommon.h"
#include "cellposition.h"
#include <QPoint>
#include <QLine>
#include <QRect>
#include <QPainterPath>
#include <map>
#include <vector>

using std::vector;
using std::map;

// Defines timeline direction: top to bottom;  left to right; right to left.
// old (vertical timeline) = new (universal)    = old (kept)
//                x        =   layer axis       =   column
//                y        =   frame axis       =    row

class DVAPI NumberRange {
	int _from, _to; // _from <= _to

  NumberRange (): _from (0), _to (0) {}
public:
	NumberRange (int from, int to): _from (min (from, to)), _to (max (from, to)) { }

	int from () const { return _from; }
	int to () const { return _to; }

	int length () const { return _to - _from;  }
  int middle () const { return (_to + _from) / 2; }

  NumberRange adjusted (int addFrom, int addTo) const;
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
  DRAG_AREA, //! draggable side bar
  SOUND_TRACK, //! area dedicated to waveform display
  PREVIEW_TRACK, //! sound preview area
  BEGIN_SOUND_EDIT, //! top sound resize
  END_SOUND_EDIT, //! bottom sound resize
  NOTE_AREA //! size of top left note controls
};
enum class PredefinedLine {
  LOCKED, //! dotted vertical line when cell is locked
  SEE_MARKER_THROUGH, //! horizontal marker visible through drag handle
  CONTINUE_LEVEL, //! level with the same name represented by vertical line
  CONTINUE_LEVEL_WITH_NAME, //! adjusted when level name is on each marker
  EXTENDER_LINE //! see grid through extender handle
};
enum class PredefinedDimension {
  LAYER, //! width of a layer column / height of layer row
  FRAME, //! height of frame row / width of frame column
  INDEX, //! index of this orientation in the array of all
  SOUND_AMPLITUDE //! amplitude of sound track, in pixels
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
enum class PredefinedRange {
  HEADER_FRAME, //! size of of column header height(v) / row header width(h)
  HEADER_LAYER, //! size of row header width(v) / column header height(h)
};

class DVAPI Orientation {
protected:
  map<PredefinedRect, QRect> _rects;
  map<PredefinedLine, QLine> _lines;
  map<PredefinedDimension, int> _dimensions;
  map<PredefinedPath, QPainterPath> _paths;
  map<PredefinedPoint, QPoint> _points;
  map<PredefinedRange, NumberRange> _ranges;

public:
	virtual CellPosition xyToPosition (const QPoint &xy, const ColumnFan *fan) const = 0;
	virtual QPoint positionToXY (const CellPosition &position, const ColumnFan *fan) const = 0;

	virtual int colToLayerAxis (int layer, const ColumnFan *fan) const = 0;
	virtual int rowToFrameAxis (int frame) const = 0;

	virtual QPoint frameLayerToXY (int frameAxis, int layerAxis) const = 0;
  QRect frameLayerRect (const NumberRange &frameAxis, const NumberRange &layerAxis) const;

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

  virtual QString name () const = 0;
  virtual const Orientation *next () const = 0;

  const QRect &rect (PredefinedRect which) const { return _rects.at (which); }
  const QLine &line (PredefinedLine which) const { return _lines.at (which); }
  int dimension (PredefinedDimension which) const { return _dimensions.at (which); }
  const QPainterPath &path (PredefinedPath which) const { return _paths.at (which); }
  const QPoint &point (PredefinedPoint which) const { return _points.at (which); }
  const NumberRange &range (PredefinedRange which) const { return _ranges.at (which); }
protected:
  void addRect (PredefinedRect which, const QRect &rect);
  void addLine (PredefinedLine which, const QLine &line);
  void addDimension (PredefinedDimension which, int dimension);
  void addPath (PredefinedPath which, const QPainterPath &path);
  void addPoint (PredefinedPoint which, const QPoint &point);
  void addRange (PredefinedRange which, const NumberRange &range);
};

class DVAPI Orientations {
	const Orientation *_topToBottom, *_leftToRight;
  vector<const Orientation *> _all;

public:
	Orientations ();
	~Orientations ();

  static const int COUNT = 2;

  const Orientation *topToBottom () const;
  const Orientation *leftToRight () const;

  const vector<const Orientation *> &all () const { return _all;  }
};

extern DVVAR Orientations orientations;

#endif
