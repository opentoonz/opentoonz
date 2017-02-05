#include "orientation.h"
#include "toonz/columnfan.h"

#include <QPainterPath>
#include <QBoxLayout>

using std::pair;

namespace {
const int KEY_ICON_WIDTH     = 11;
const int KEY_ICON_HEIGHT    = 13;
const int EASE_TRIANGLE_SIZE = 4;
const int PLAY_MARKER_SIZE   = 10;
const int ONION_SIZE         = 19;
const int ONION_DOT_SIZE     = 8;
const int PINNED_SIZE        = 10;
}

class TopToBottomOrientation : public Orientation {
  const int CELL_WIDTH          = 74;
  const int CELL_HEIGHT         = 20;
  const int CELL_DRAG_WIDTH     = 7;
  const int EXTENDER_WIDTH      = 20;
  const int EXTENDER_HEIGHT     = 8;
  const int SOUND_PREVIEW_WIDTH = 7;
  const int LAYER_HEADER_HEIGHT = CELL_HEIGHT * 3 + 60;
  const int FRAME_HEADER_WIDTH  = CELL_WIDTH;
  const int PLAY_RANGE_X        = FRAME_HEADER_WIDTH / 2 - PLAY_MARKER_SIZE;
  const int ONION_X = 0, ONION_Y = 0;

public:
  TopToBottomOrientation();

  virtual CellPosition xyToPosition(const QPoint &xy,
                                    const ColumnFan *fan) const override;
  virtual QPoint positionToXY(const CellPosition &position,
                              const ColumnFan *fan) const override;

  virtual int colToLayerAxis(int layer, const ColumnFan *fan) const override;
  virtual int rowToFrameAxis(int frame) const override;

  virtual QPoint frameLayerToXY(int frameAxis, int layerAxis) const override;
  virtual int layerAxis(const QPoint &xy) const override;
  virtual int frameAxis(const QPoint &xy) const override;

  virtual NumberRange layerSide(const QRect &area) const override;
  virtual NumberRange frameSide(const QRect &area) const override;
  virtual QPoint topRightCorner(const QRect &area) const override;

  virtual CellPosition arrowShift(int direction) const override;

  virtual QString name() const override { return "TopToBottom"; }
  virtual QString caption() const override { return "Xsheet"; }
  virtual const Orientation *next() const override {
    return Orientations::leftToRight();
  }

  virtual bool isVerticalTimeline() const override { return true; }
  virtual bool flipVolume() const override { return true; }
};

class LeftToRightOrientation : public Orientation {
  const int CELL_WIDTH           = 50;
  const int CELL_HEIGHT          = 20;
  const int CELL_DRAG_HEIGHT     = 5;
  const int EXTENDER_WIDTH       = 8;
  const int EXTENDER_HEIGHT      = 12;
  const int SOUND_PREVIEW_HEIGHT = 6;
  const int FRAME_HEADER_HEIGHT  = 50;
  const int ONION_X = (CELL_WIDTH - ONION_SIZE) / 2, ONION_Y = 0;
  const int PLAY_RANGE_Y       = ONION_SIZE;
  const int ICON_WIDTH         = 21;
  const int ICON_OFFSET        = ICON_WIDTH + 1;
  const int ICONS_WIDTH        = ICON_OFFSET * 4;  // 88
  const int LAYER_NUMBER_WIDTH = 44;
  const int LAYER_NAME_WIDTH   = 130;
  const int LAYER_HEADER_WIDTH =
      ICONS_WIDTH + LAYER_NUMBER_WIDTH + LAYER_NAME_WIDTH;

public:
  LeftToRightOrientation();

  virtual CellPosition xyToPosition(const QPoint &xy,
                                    const ColumnFan *fan) const override;
  virtual QPoint positionToXY(const CellPosition &position,
                              const ColumnFan *fan) const override;

  virtual int colToLayerAxis(int layer, const ColumnFan *fan) const override;
  virtual int rowToFrameAxis(int frame) const override;

  virtual QPoint frameLayerToXY(int frameAxis, int layerAxis) const override;
  virtual int layerAxis(const QPoint &xy) const override;
  virtual int frameAxis(const QPoint &xy) const override;

  virtual NumberRange layerSide(const QRect &area) const override;
  virtual NumberRange frameSide(const QRect &area) const override;
  virtual QPoint topRightCorner(const QRect &area) const override;

  virtual CellPosition arrowShift(int direction) const override;

  virtual QString name() const override { return "LeftToRight"; }
  virtual QString caption() const override { return "Timeline"; }
  virtual const Orientation *next() const override {
    return Orientations::topToBottom();
  }

  virtual bool isVerticalTimeline() const override { return false; }
  virtual bool flipVolume() const override { return false; }
};

/// -------------------------------------------------------------------------------

int NumberRange::weight(double toWeight) const {  // weight ranging 0..1
  return _from + (_to - _from) * toWeight;
}

NumberRange NumberRange::adjusted(int addFrom, int addTo) const {
  return NumberRange(_from + addFrom, _to + addTo);
}

double NumberRange::ratio(int at) const {
  double result          = ((double)at - _from) / (_to - _from);
  if (result < 0) result = 0;
  if (result > 1) result = 1;
  return result;
}

/// -------------------------------------------------------------------------------

// const int Orientations::COUNT = 2;

Orientations::Orientations() : _topToBottom(nullptr), _leftToRight(nullptr) {
  _topToBottom = new TopToBottomOrientation();
  _leftToRight = new LeftToRightOrientation();

  _all.push_back(_topToBottom);
  _all.push_back(_leftToRight);
}
Orientations::~Orientations() {
  delete _topToBottom;
  _topToBottom = nullptr;
  delete _leftToRight;
  _leftToRight = nullptr;
}

const Orientations &Orientations::instance() {
  static Orientations singleton;
  return singleton;
}

const Orientation *Orientations::topToBottom() {
  return instance()._topToBottom;
}
const Orientation *Orientations::leftToRight() {
  return instance()._leftToRight;
}
const vector<const Orientation *> &Orientations::all() {
  return instance()._all;
}
const Orientation *Orientations::byName(const QString &name) {
  vector<const Orientation *> m_all = all();
  for (auto it = m_all.begin(); it != m_all.end(); it++)
    if ((*it)->name() == name) return *it;
  throw std::runtime_error(
      (QString("no such orientation: ") + name).toStdString().c_str());
}

/// -------------------------------------------------------------------------------

QLine Orientation::verticalLine(int layerAxis,
                                const NumberRange &frameAxis) const {
  QPoint first  = frameLayerToXY(frameAxis.from(), layerAxis);
  QPoint second = frameLayerToXY(frameAxis.to(), layerAxis);
  return QLine(first, second);
}
QLine Orientation::horizontalLine(int frameAxis,
                                  const NumberRange &layerAxis) const {
  QPoint first  = frameLayerToXY(frameAxis, layerAxis.from());
  QPoint second = frameLayerToXY(frameAxis, layerAxis.to());
  return QLine(first, second);
}
QRect Orientation::frameLayerRect(const NumberRange &frameAxis,
                                  const NumberRange &layerAxis) const {
  QPoint topLeft     = frameLayerToXY(frameAxis.from(), layerAxis.from());
  QPoint bottomRight = frameLayerToXY(frameAxis.to(), layerAxis.to());
  return QRect(topLeft, bottomRight);
}

QRect Orientation::foldedRectangle(int layerAxis, const NumberRange &frameAxis,
                                   int i) const {
  QPoint topLeft = frameLayerToXY(frameAxis.from(), layerAxis + 1 + i * 3);
  QPoint size    = frameLayerToXY(frameAxis.length(), 2);
  return QRect(topLeft, QSize(size.x(), size.y()));
}
QLine Orientation::foldedRectangleLine(int layerAxis,
                                       const NumberRange &frameAxis,
                                       int i) const {
  return verticalLine(layerAxis + i * 3, frameAxis);
}

void Orientation::addRect(PredefinedRect which, const QRect &rect) {
  _rects.insert(pair<PredefinedRect, QRect>(which, rect));
}
void Orientation::addLine(PredefinedLine which, const QLine &line) {
  _lines.insert(pair<PredefinedLine, QLine>(which, line));
}
void Orientation::addDimension(PredefinedDimension which, int dimension) {
  _dimensions.insert(pair<PredefinedDimension, int>(which, dimension));
}
void Orientation::addPath(PredefinedPath which, const QPainterPath &path) {
  _paths.insert(pair<PredefinedPath, QPainterPath>(which, path));
}
void Orientation::addPoint(PredefinedPoint which, const QPoint &point) {
  _points.insert(pair<PredefinedPoint, QPoint>(which, point));
}
void Orientation::addRange(PredefinedRange which, const NumberRange &range) {
  _ranges.insert(pair<PredefinedRange, NumberRange>(which, range));
}

/// -------------------------------------------------------------------------------

TopToBottomOrientation::TopToBottomOrientation() {
  QRect cellRect(0, 0, CELL_WIDTH, CELL_HEIGHT);
  addRect(PredefinedRect::CELL, cellRect);
  addRect(PredefinedRect::DRAG_HANDLE_CORNER,
          QRect(0, 0, CELL_DRAG_WIDTH, CELL_HEIGHT));
  QRect keyRect(CELL_WIDTH - KEY_ICON_WIDTH,
                (CELL_HEIGHT - KEY_ICON_HEIGHT) / 2, KEY_ICON_WIDTH,
                KEY_ICON_HEIGHT);
  addRect(PredefinedRect::KEY_ICON, keyRect);
  QRect nameRect = cellRect.adjusted(7, 4, -6, 0);
  addRect(PredefinedRect::CELL_NAME, nameRect);
  addRect(PredefinedRect::CELL_NAME_WITH_KEYFRAME,
          nameRect.adjusted(0, 0, -KEY_ICON_WIDTH, 0));
  addRect(PredefinedRect::END_EXTENDER,
          QRect(-EXTENDER_WIDTH - KEY_ICON_WIDTH, 1, EXTENDER_WIDTH,
                EXTENDER_HEIGHT));
  addRect(PredefinedRect::BEGIN_EXTENDER,
          QRect(-EXTENDER_WIDTH - KEY_ICON_WIDTH, -EXTENDER_HEIGHT,
                EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect(PredefinedRect::KEYFRAME_AREA,
          QRect(CELL_WIDTH - KEY_ICON_WIDTH, 0, KEY_ICON_WIDTH, CELL_HEIGHT));
  addRect(PredefinedRect::DRAG_AREA, QRect(0, 0, CELL_DRAG_WIDTH, CELL_HEIGHT));
  QRect soundRect(CELL_DRAG_WIDTH, 0,
                  CELL_WIDTH - CELL_DRAG_WIDTH - SOUND_PREVIEW_WIDTH,
                  CELL_HEIGHT);
  addRect(PredefinedRect::SOUND_TRACK, soundRect);
  addRect(PredefinedRect::PREVIEW_TRACK,
          QRect(CELL_WIDTH - SOUND_PREVIEW_WIDTH, 0, SOUND_PREVIEW_WIDTH,
                CELL_HEIGHT));
  addRect(PredefinedRect::BEGIN_SOUND_EDIT,
          QRect(CELL_DRAG_WIDTH, 0, CELL_WIDTH - CELL_DRAG_WIDTH, 2));
  addRect(
      PredefinedRect::END_SOUND_EDIT,
      QRect(CELL_DRAG_WIDTH, CELL_HEIGHT - 2, CELL_WIDTH - CELL_DRAG_WIDTH, 2));
  addRect(PredefinedRect::NOTE_AREA,
          QRect(QPoint(0, 0), QSize(FRAME_HEADER_WIDTH, LAYER_HEADER_HEIGHT)));
  addRect(PredefinedRect::FRAME_LABEL,
          QRect(3, 1, CELL_WIDTH - 6, CELL_HEIGHT - 2));
  addRect(PredefinedRect::FRAME_HEADER,
          QRect(0, 0, FRAME_HEADER_WIDTH, CELL_HEIGHT));
  addRect(PredefinedRect::LAYER_HEADER,
          QRect(0, 0, CELL_WIDTH, LAYER_HEADER_HEIGHT));
  addRect(PredefinedRect::PLAY_RANGE,
          QRect(PLAY_RANGE_X, 0, PLAY_MARKER_SIZE, CELL_HEIGHT));
  addRect(PredefinedRect::ONION,
          QRect(ONION_X + (3 * ONION_DOT_SIZE - ONION_SIZE) / 2, ONION_Y,
                ONION_SIZE, ONION_SIZE));
  int adjustOnion = (ONION_SIZE - ONION_DOT_SIZE) / 2;
  addRect(PredefinedRect::ONION_DOT,
          QRect(ONION_X + ONION_DOT_SIZE, ONION_Y + adjustOnion, ONION_DOT_SIZE,
                ONION_DOT_SIZE));
  addRect(
      PredefinedRect::ONION_DOT_FIXED,
      QRect(ONION_X, ONION_Y + adjustOnion, ONION_DOT_SIZE, ONION_DOT_SIZE));
  addRect(PredefinedRect::ONION_AREA,
          QRect(ONION_X, ONION_Y, PLAY_RANGE_X, CELL_HEIGHT));
  addRect(PredefinedRect::ONION_FIXED_DOT_AREA,
          QRect(ONION_X, ONION_Y, ONION_DOT_SIZE, CELL_HEIGHT));
  addRect(
      PredefinedRect::ONION_DOT_AREA,
      QRect(ONION_X + ONION_DOT_SIZE, ONION_Y, ONION_DOT_SIZE, CELL_HEIGHT));
  addRect(PredefinedRect::PINNED_CENTER_KEY,
          QRect((FRAME_HEADER_WIDTH - PINNED_SIZE) / 2,
                (CELL_HEIGHT - PINNED_SIZE) / 2, PINNED_SIZE, PINNED_SIZE));
  addRect(PredefinedRect::RENAME_COLUMN,
          QRect(0, 6, CELL_WIDTH, CELL_HEIGHT - 3));
  QRect eyeArea(10, 6, CELL_WIDTH - 12, CELL_HEIGHT - 3);
  addRect(PredefinedRect::EYE_AREA, eyeArea);
  QRect eye(eyeArea.right() - 18, 7, 18, 15);
  addRect(PredefinedRect::EYE, eye);
  addRect(PredefinedRect::PREVIEW_LAYER_AREA,
          eyeArea.translated(0, CELL_HEIGHT));
  addRect(PredefinedRect::PREVIEW_LAYER, eye.translated(0, CELL_HEIGHT));
  addRect(PredefinedRect::LOCK, QRect(9, CELL_HEIGHT + 6, 16, 16));
  addRect(PredefinedRect::DRAG_LAYER,
          QRect(0, 0, CELL_DRAG_WIDTH, LAYER_HEADER_HEIGHT));
  addRect(PredefinedRect::LAYER_NAME, eyeArea);
  addRect(PredefinedRect::LAYER_NUMBER, QRect(0, 0, -1, -1));  // hide
  addRect(PredefinedRect::SOUND_ICON, QRect(29, 3 * CELL_HEIGHT + 4, 40, 30));
  addRect(PredefinedRect::LOOP_ICON, QRect(keyRect.left(), 0, 10, 11));
  addRect(PredefinedRect::LAYER_HEADER_PANEL, QRect(0, 0, -1, -1));  // hide

  QPoint soundTopLeft(20, CELL_HEIGHT * 2 + 7);
  int trackLen = 60;
  addRect(PredefinedRect::VOLUME_TRACK,
          QRect(soundTopLeft, QSize(3, trackLen)));
  addRect(PredefinedRect::VOLUME_AREA,
          QRect(CELL_DRAG_WIDTH, CELL_HEIGHT * 2 + 4, 29 - CELL_DRAG_WIDTH,
                trackLen));

  addLine(PredefinedLine::LOCKED,
          verticalLine((CELL_DRAG_WIDTH + 1) / 2, NumberRange(0, CELL_HEIGHT)));
  addLine(PredefinedLine::SEE_MARKER_THROUGH,
          horizontalLine(0, NumberRange(0, CELL_DRAG_WIDTH)));
  addLine(PredefinedLine::CONTINUE_LEVEL,
          verticalLine(CELL_WIDTH / 2, NumberRange(0, CELL_HEIGHT)));
  addLine(PredefinedLine::CONTINUE_LEVEL_WITH_NAME,
          verticalLine(CELL_WIDTH - 11, NumberRange(0, CELL_HEIGHT)));
  addLine(PredefinedLine::EXTENDER_LINE,
          horizontalLine(0, NumberRange(-EXTENDER_WIDTH - KEY_ICON_WIDTH, 0)));

  addDimension(PredefinedDimension::LAYER, CELL_WIDTH);
  addDimension(PredefinedDimension::FRAME, CELL_HEIGHT);
  addDimension(PredefinedDimension::INDEX, 0);
  addDimension(PredefinedDimension::SOUND_AMPLITUDE,
               int(sqrt(CELL_HEIGHT * soundRect.width()) / 2));
  addDimension(PredefinedDimension::FRAME_LABEL_ALIGN,
               Qt::AlignRight | Qt::AlignVCenter);
  addDimension(PredefinedDimension::ONION_TURN, 0);
  addDimension(PredefinedDimension::QBOXLAYOUT_DIRECTION,
               QBoxLayout::Direction::TopToBottom);
  addDimension(PredefinedDimension::CENTER_ALIGN, Qt::AlignHCenter);

  QPainterPath corner(QPointF(0, CELL_HEIGHT));
  corner.lineTo(QPointF(CELL_DRAG_WIDTH, CELL_HEIGHT));
  corner.lineTo(QPointF(CELL_DRAG_WIDTH, CELL_HEIGHT - CELL_DRAG_WIDTH));
  corner.lineTo(QPointF(0, CELL_HEIGHT));
  addPath(PredefinedPath::DRAG_HANDLE_CORNER, corner);

  QPainterPath fromTriangle(QPointF(0, EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE, -EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE, -EASE_TRIANGLE_SIZE / 2));
  fromTriangle.lineTo(QPointF(0, EASE_TRIANGLE_SIZE / 2));
  fromTriangle.translate(keyRect.center());
  addPath(PredefinedPath::BEGIN_EASE_TRIANGLE, fromTriangle);

  QPainterPath toTriangle(QPointF(0, -EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE, EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE, EASE_TRIANGLE_SIZE / 2));
  toTriangle.lineTo(QPointF(0, -EASE_TRIANGLE_SIZE / 2));
  toTriangle.translate(keyRect.center());
  addPath(PredefinedPath::END_EASE_TRIANGLE, toTriangle);

  QPainterPath playFrom(QPointF(0, 0));
  playFrom.lineTo(QPointF(PLAY_MARKER_SIZE, 0));
  playFrom.lineTo(QPointF(0, PLAY_MARKER_SIZE));
  playFrom.lineTo(QPointF(0, 0));
  playFrom.translate(PLAY_RANGE_X, 1);
  addPath(PredefinedPath::BEGIN_PLAY_RANGE, playFrom);

  QPainterPath playTo(QPointF(0, 0));
  playTo.lineTo(QPointF(PLAY_MARKER_SIZE, 0));
  playTo.lineTo(QPointF(0, -PLAY_MARKER_SIZE));
  playTo.lineTo(QPointF(0, 0));
  playTo.translate(PLAY_RANGE_X, CELL_HEIGHT - 1);
  addPath(PredefinedPath::END_PLAY_RANGE, playTo);

  QPainterPath track(QPointF(0, 0));
  track.lineTo(QPointF(1, 1));
  track.lineTo(QPointF(1, trackLen - 1));
  track.lineTo(QPointF(0, trackLen));
  track.lineTo(QPointF(-1, trackLen - 1));
  track.lineTo(QPointF(-1, 1));
  track.lineTo(QPointF(0, 0));
  track.translate(soundTopLeft);
  addPath(PredefinedPath::VOLUME_SLIDER_TRACK, track);

  QPainterPath head(QPointF(0, 0));
  head.lineTo(QPointF(4, 4));
  head.lineTo(QPointF(8, 4));
  head.lineTo(QPointF(8, -4));
  head.lineTo(QPointF(4, -4));
  head.lineTo(QPointF(0, 0));
  addPath(PredefinedPath::VOLUME_SLIDER_HEAD, head);

  addPoint(PredefinedPoint::KEY_HIDDEN, QPoint(KEY_ICON_WIDTH, 0));
  addPoint(PredefinedPoint::EXTENDER_XY_RADIUS, QPoint(30, 75));
  addPoint(PredefinedPoint::VOLUME_DIVISIONS_TOP_LEFT,
           soundTopLeft - QPoint(5, 0));

  addRange(PredefinedRange::HEADER_LAYER, NumberRange(0, FRAME_HEADER_WIDTH));
  addRange(PredefinedRange::HEADER_FRAME, NumberRange(0, LAYER_HEADER_HEIGHT));
}

CellPosition TopToBottomOrientation::xyToPosition(const QPoint &xy,
                                                  const ColumnFan *fan) const {
  int layer = fan->layerAxisToCol(xy.x());
  int frame = xy.y() / CELL_HEIGHT;
  return CellPosition(frame, layer);
}
QPoint TopToBottomOrientation::positionToXY(const CellPosition &position,
                                            const ColumnFan *fan) const {
  int x = colToLayerAxis(position.layer(), fan);
  int y = rowToFrameAxis(position.frame());
  return QPoint(x, y);
}
int TopToBottomOrientation::colToLayerAxis(int layer,
                                           const ColumnFan *fan) const {
  return fan->colToLayerAxis(layer);
}
int TopToBottomOrientation::rowToFrameAxis(int frame) const {
  return frame * CELL_HEIGHT;
}
QPoint TopToBottomOrientation::frameLayerToXY(int frameAxis,
                                              int layerAxis) const {
  return QPoint(layerAxis, frameAxis);
}
int TopToBottomOrientation::layerAxis(const QPoint &xy) const { return xy.x(); }
int TopToBottomOrientation::frameAxis(const QPoint &xy) const { return xy.y(); }
NumberRange TopToBottomOrientation::layerSide(const QRect &area) const {
  return NumberRange(area.left(), area.right());
}
NumberRange TopToBottomOrientation::frameSide(const QRect &area) const {
  return NumberRange(area.top(), area.bottom());
}
QPoint TopToBottomOrientation::topRightCorner(const QRect &area) const {
  return area.topRight();
}

CellPosition TopToBottomOrientation::arrowShift(int direction) const {
  switch (direction) {
  case Qt::Key_Up:
    return CellPosition(-1, 0);
  case Qt::Key_Down:
    return CellPosition(1, 0);
  case Qt::Key_Left:
    return CellPosition(0, -1);
  case Qt::Key_Right:
    return CellPosition(0, 1);
  default:
    return CellPosition(0, 0);
  }
}

/// --------------------------------------------------------------------------------

LeftToRightOrientation::LeftToRightOrientation() {
  QRect cellRect(0, 0, CELL_WIDTH, CELL_HEIGHT);
  addRect(PredefinedRect::CELL, cellRect);
  addRect(PredefinedRect::DRAG_HANDLE_CORNER,
          QRect(0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));
  QRect keyRect((CELL_WIDTH - KEY_ICON_WIDTH) / 2,
                CELL_HEIGHT - KEY_ICON_HEIGHT, KEY_ICON_WIDTH, KEY_ICON_HEIGHT);
  addRect(PredefinedRect::KEY_ICON, keyRect);
  QRect nameRect = cellRect.adjusted(7, 4, -6, 0);
  addRect(PredefinedRect::CELL_NAME, nameRect);
  addRect(PredefinedRect::CELL_NAME_WITH_KEYFRAME,
          nameRect.adjusted(0, 0, 0, -KEY_ICON_HEIGHT));
  addRect(PredefinedRect::END_EXTENDER,
          QRect(1, -EXTENDER_HEIGHT - 10, EXTENDER_WIDTH, EXTENDER_HEIGHT));
  addRect(PredefinedRect::BEGIN_EXTENDER,
          QRect(-EXTENDER_WIDTH, -EXTENDER_HEIGHT - 10, EXTENDER_WIDTH,
                EXTENDER_HEIGHT));
  addRect(PredefinedRect::KEYFRAME_AREA, keyRect);
  addRect(PredefinedRect::DRAG_AREA, QRect(0, 0, CELL_WIDTH, CELL_DRAG_HEIGHT));
  QRect soundRect(0, CELL_DRAG_HEIGHT, CELL_WIDTH,
                  CELL_HEIGHT - CELL_DRAG_HEIGHT - SOUND_PREVIEW_HEIGHT);
  addRect(PredefinedRect::SOUND_TRACK, soundRect);
  addRect(PredefinedRect::PREVIEW_TRACK,
          QRect(0, CELL_HEIGHT - SOUND_PREVIEW_HEIGHT, CELL_WIDTH,
                SOUND_PREVIEW_HEIGHT));
  addRect(PredefinedRect::BEGIN_SOUND_EDIT,
          QRect(0, CELL_DRAG_HEIGHT, 2, CELL_HEIGHT - CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::END_SOUND_EDIT,
          QRect(CELL_WIDTH - 2, CELL_DRAG_HEIGHT, 2,
                CELL_HEIGHT - CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::NOTE_AREA,
          QRect(QPoint(0, 0), QSize(LAYER_HEADER_WIDTH, FRAME_HEADER_HEIGHT)));
  addRect(
      PredefinedRect::FRAME_LABEL,
      QRect(CELL_WIDTH / 2, 1, CELL_WIDTH / 2 - 1, FRAME_HEADER_HEIGHT - 2));
  addRect(PredefinedRect::FRAME_HEADER,
          QRect(0, 0, CELL_WIDTH, FRAME_HEADER_HEIGHT));
  addRect(PredefinedRect::LAYER_HEADER,
          QRect(0, 0, LAYER_HEADER_WIDTH, CELL_HEIGHT));
  addRect(PredefinedRect::PLAY_RANGE,
          QRect(0, PLAY_RANGE_Y, CELL_WIDTH, PLAY_MARKER_SIZE));
  addRect(PredefinedRect::ONION,
          QRect(ONION_X, ONION_Y + (3 * ONION_DOT_SIZE - ONION_SIZE) / 2,
                ONION_SIZE, ONION_SIZE));
  int adjustOnion = (ONION_SIZE - ONION_DOT_SIZE) / 2;
  addRect(PredefinedRect::ONION_DOT,
          QRect(ONION_X + adjustOnion, ONION_Y + ONION_DOT_SIZE, ONION_DOT_SIZE,
                ONION_DOT_SIZE));
  addRect(
      PredefinedRect::ONION_DOT_FIXED,
      QRect(ONION_X + adjustOnion, ONION_Y, ONION_DOT_SIZE, ONION_DOT_SIZE));
  addRect(PredefinedRect::ONION_AREA,
          QRect(ONION_X, ONION_Y, CELL_WIDTH, ONION_SIZE));
  addRect(PredefinedRect::ONION_FIXED_DOT_AREA,
          QRect(ONION_X, ONION_Y, CELL_WIDTH, ONION_DOT_SIZE));
  addRect(PredefinedRect::ONION_DOT_AREA,
          QRect(ONION_X, ONION_Y + ONION_DOT_SIZE, CELL_WIDTH, ONION_DOT_SIZE));
  addRect(
      PredefinedRect::PINNED_CENTER_KEY,
      QRect((CELL_WIDTH - PINNED_SIZE) / 2,
            (FRAME_HEADER_HEIGHT - PINNED_SIZE) / 2, PINNED_SIZE, PINNED_SIZE));
  QRect columnName(ICONS_WIDTH + LAYER_NUMBER_WIDTH, 0, LAYER_NAME_WIDTH,
                   CELL_HEIGHT);
  addRect(PredefinedRect::RENAME_COLUMN, columnName);
  QRect eye(0, 0, ICON_WIDTH, CELL_HEIGHT);
  addRect(PredefinedRect::EYE_AREA, eye);
  addRect(PredefinedRect::EYE, eye);
  addRect(PredefinedRect::PREVIEW_LAYER_AREA,
          eye.translated(2 * ICON_OFFSET, 0));
  addRect(PredefinedRect::PREVIEW_LAYER, eye.translated(2 * ICON_OFFSET, 0));
  addRect(PredefinedRect::LOCK, eye.translated(3 * ICON_OFFSET, 0));
  addRect(PredefinedRect::DRAG_LAYER,
          QRect(0, 0, LAYER_HEADER_WIDTH, CELL_DRAG_HEIGHT));
  addRect(PredefinedRect::LAYER_NAME, columnName);
  addRect(PredefinedRect::LAYER_NUMBER,
          QRect(ICONS_WIDTH, 0, LAYER_NUMBER_WIDTH, CELL_HEIGHT));
  addRect(PredefinedRect::SOUND_ICON, eye.translated(ICON_OFFSET, 0));
  addRect(PredefinedRect::LOOP_ICON, QRect(0, keyRect.top(), 10, 11));
  addRect(PredefinedRect::LAYER_HEADER_PANEL,
          QRect(0, FRAME_HEADER_HEIGHT - CELL_HEIGHT, LAYER_HEADER_WIDTH,
                CELL_HEIGHT));

  QPoint soundTopLeft(columnName.left(), columnName.bottom() - 8);
  int trackLen = 60;
  addRect(PredefinedRect::VOLUME_TRACK,
          QRect(soundTopLeft, QSize(trackLen, 3)));
  addRect(PredefinedRect::VOLUME_AREA,
          QRect(columnName.topLeft(), QSize(trackLen, columnName.height())));

  addLine(PredefinedLine::LOCKED,
          verticalLine(CELL_DRAG_HEIGHT / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::SEE_MARKER_THROUGH,
          horizontalLine(0, NumberRange(0, CELL_DRAG_HEIGHT)));
  addLine(PredefinedLine::CONTINUE_LEVEL,
          verticalLine(CELL_HEIGHT / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::CONTINUE_LEVEL_WITH_NAME,
          verticalLine(CELL_HEIGHT / 2, NumberRange(0, CELL_WIDTH)));
  addLine(PredefinedLine::EXTENDER_LINE,
          horizontalLine(0, NumberRange(-EXTENDER_WIDTH - KEY_ICON_WIDTH, 0)));

  addDimension(PredefinedDimension::LAYER, CELL_HEIGHT);
  addDimension(PredefinedDimension::FRAME, CELL_WIDTH);
  addDimension(PredefinedDimension::INDEX, 1);
  addDimension(PredefinedDimension::SOUND_AMPLITUDE, soundRect.height() / 2);
  addDimension(PredefinedDimension::FRAME_LABEL_ALIGN,
               Qt::AlignRight | Qt::AlignBottom | Qt::TextWordWrap);
  addDimension(PredefinedDimension::ONION_TURN, 90);
  addDimension(PredefinedDimension::QBOXLAYOUT_DIRECTION,
               QBoxLayout::Direction::LeftToRight);
  addDimension(PredefinedDimension::CENTER_ALIGN, Qt::AlignVCenter);

  QPainterPath corner(QPointF(CELL_WIDTH, 0));
  corner.lineTo(QPointF(CELL_WIDTH, CELL_DRAG_HEIGHT));
  corner.lineTo(QPointF(CELL_WIDTH - CELL_DRAG_HEIGHT, CELL_DRAG_HEIGHT));
  corner.lineTo(QPointF(CELL_WIDTH, 0));
  addPath(PredefinedPath::DRAG_HANDLE_CORNER, corner);

  QPainterPath fromTriangle(QPointF(EASE_TRIANGLE_SIZE / 2, 0));
  fromTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  fromTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, 0));
  fromTriangle.translate(keyRect.center());
  addPath(PredefinedPath::BEGIN_EASE_TRIANGLE, fromTriangle);

  QPainterPath toTriangle(QPointF(-EASE_TRIANGLE_SIZE / 2, 0));
  toTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, EASE_TRIANGLE_SIZE));
  toTriangle.lineTo(QPointF(EASE_TRIANGLE_SIZE / 2, -EASE_TRIANGLE_SIZE));
  toTriangle.lineTo(QPointF(-EASE_TRIANGLE_SIZE / 2, 0));
  toTriangle.translate(keyRect.center());
  addPath(PredefinedPath::END_EASE_TRIANGLE, toTriangle);

  QPainterPath playFrom(QPointF(0, 0));
  playFrom.lineTo(QPointF(PLAY_MARKER_SIZE, 0));
  playFrom.lineTo(QPointF(0, PLAY_MARKER_SIZE));
  playFrom.lineTo(QPointF(0, 0));
  playFrom.translate(1, PLAY_RANGE_Y);
  addPath(PredefinedPath::BEGIN_PLAY_RANGE, playFrom);

  QPainterPath playTo(QPointF(0, 0));
  playTo.lineTo(QPointF(-PLAY_MARKER_SIZE, 0));
  playTo.lineTo(QPointF(0, PLAY_MARKER_SIZE));
  playTo.lineTo(QPointF(0, 0));
  playTo.translate(CELL_WIDTH - 1, PLAY_RANGE_Y);
  addPath(PredefinedPath::END_PLAY_RANGE, playTo);

  QPainterPath track(QPointF(0, 0));
  track.lineTo(QPointF(1, 1));
  track.lineTo(QPointF(trackLen - 1, 1));
  track.lineTo(QPointF(trackLen, 0));
  track.lineTo(QPointF(trackLen - 1, -1));
  track.lineTo(QPointF(1, -1));
  track.lineTo(QPointF(0, 0));
  track.translate(soundTopLeft);
  addPath(PredefinedPath::VOLUME_SLIDER_TRACK, track);

  QPainterPath head(QPointF(0, 0));
  head.lineTo(QPointF(4, -4));
  head.lineTo(QPointF(4, -8));
  head.lineTo(QPointF(-4, -8));
  head.lineTo(QPointF(-4, -4));
  head.lineTo(QPointF(0, 0));
  addPath(PredefinedPath::VOLUME_SLIDER_HEAD, head);

  addPoint(PredefinedPoint::KEY_HIDDEN, QPoint(0, 10));
  addPoint(PredefinedPoint::EXTENDER_XY_RADIUS, QPoint(75, 30));
  addPoint(PredefinedPoint::VOLUME_DIVISIONS_TOP_LEFT,
           soundTopLeft + QPoint(0, 5));

  addRange(PredefinedRange::HEADER_LAYER, NumberRange(0, FRAME_HEADER_HEIGHT));
  addRange(PredefinedRange::HEADER_FRAME, NumberRange(0, LAYER_HEADER_WIDTH));
}

CellPosition LeftToRightOrientation::xyToPosition(const QPoint &xy,
                                                  const ColumnFan *fan) const {
  int layer = fan->layerAxisToCol(xy.y());
  int frame = xy.x() / CELL_WIDTH;
  return CellPosition(frame, layer);
}
QPoint LeftToRightOrientation::positionToXY(const CellPosition &position,
                                            const ColumnFan *fan) const {
  int y = colToLayerAxis(position.layer(), fan);
  int x = rowToFrameAxis(position.frame());
  return QPoint(x, y);
}
int LeftToRightOrientation::colToLayerAxis(int layer,
                                           const ColumnFan *fan) const {
  return fan->colToLayerAxis(layer);
}
int LeftToRightOrientation::rowToFrameAxis(int frame) const {
  return frame * CELL_WIDTH;
}
QPoint LeftToRightOrientation::frameLayerToXY(int frameAxis,
                                              int layerAxis) const {
  return QPoint(frameAxis, layerAxis);
}
int LeftToRightOrientation::layerAxis(const QPoint &xy) const { return xy.y(); }
int LeftToRightOrientation::frameAxis(const QPoint &xy) const { return xy.x(); }
NumberRange LeftToRightOrientation::layerSide(const QRect &area) const {
  return NumberRange(area.top(), area.bottom());
}
NumberRange LeftToRightOrientation::frameSide(const QRect &area) const {
  return NumberRange(area.left(), area.right());
}
QPoint LeftToRightOrientation::topRightCorner(const QRect &area) const {
  return area.bottomLeft();
}
CellPosition LeftToRightOrientation::arrowShift(int direction) const {
  switch (direction) {
  case Qt::Key_Up:
    return CellPosition(0, -1);
  case Qt::Key_Down:
    return CellPosition(0, 1);
  case Qt::Key_Left:
    return CellPosition(-1, 0);
  case Qt::Key_Right:
    return CellPosition(1, 0);
  default:
    return CellPosition(0, 0);
  }
}
