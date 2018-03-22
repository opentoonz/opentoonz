

#include "toonzqt/functionpanel.h"

// TnzQt includes
#include "toonzqt/functionselection.h"
#include "toonzqt/functionsegmentviewer.h"
#include "toonzqt/imageutils.h"
#include "functionpaneltools.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"
// TnzBase includes
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "tunit.h"

// TnzCore includes
#include "tcommon.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QSettings>

#include <cmath>

namespace {

void drawCircle(QPainter &painter, double x, double y, double r) {
  painter.drawEllipse(x - r, y - r, 2 * r, 2 * r);
}
void drawCircle(QPainter &painter, const QPointF &p, double r) {
  painter.drawEllipse(p.x() - r, p.y() - r, 2 * r, 2 * r);
}
void drawSquare(QPainter &painter, double x, double y, double r) {
  painter.drawRect(x - r, y - r, 2 * r, 2 * r);
}
void drawSquare(QPainter &painter, const QPointF &p, double r) {
  drawSquare(painter, p.x(), p.y(), r);
}

void drawRoundedSquare(QPainter &painter, const QPointF &p, double r) {
  painter.drawRoundRect(p.x() - r, p.y() - r, 2 * r, 2 * r, 99, 99);
}

double norm2(const QPointF &p) { return p.x() * p.x() + p.y() * p.y(); }

class FunctionPanelZoomer final : public ImageUtils::ShortcutZoomer {
  FunctionPanel *m_panel;

public:
  FunctionPanelZoomer(FunctionPanel *panel)
      : ShortcutZoomer(panel), m_panel(panel) {}

  bool zoom(bool zoomin, bool resetZoom) override {
    if (resetZoom)
      m_panel->fitGraphToWindow();
    else {
      double f  = 1.25;
      double sc = zoomin ? f : 1.0 / f;
      QPoint center(m_panel->width() / 2, m_panel->height() / 2);
      m_panel->zoom(sc, sc, center);
    }

    return true;
  }
};

}  // namespace

//=============================================================================
//
// Ruler
//
//-----------------------------------------------------------------------------

class Ruler {
  double m_minValue, m_step;
  int m_labelPeriod, m_labelOffset, m_tickCount;

  double m_unit, m_pan, m_vOrigin;
  int m_x0, m_x1;
  int m_minLabelDistance, m_minDistance;
  double m_minStep;

public:
  Ruler();

  // unit,pan define the world to viewport transformation: pixel = value * unit
  // + pan
  // note: unit can be <0 (e.g. in the vertical rulers)
  // use vOrigin!=0 when there is a offset between values and labels
  // (e.g. frame=0 is visualized as 1)
  void setTransform(double unit, double pan, double vOrigin = 0) {
    m_unit    = unit;
    m_pan     = pan;
    m_vOrigin = vOrigin;
  }

  void setRange(int x0, int x1) {
    m_x0 = x0;
    m_x1 = x1;
  }  // [x0,x1] is the pixel range

  // set minimum distance (pixel) between two consecutive labels
  void setMinLabelDistance(int distance) { m_minLabelDistance = distance; }
  // set minimum distance (pixel) between two consecutive ticks
  void setMinDistance(int distance) { m_minDistance = distance; }

  // use setMinStep to define a minimum tick (e.g. for integer rulers as 'frame'
  // call setMinStep(1);)
  void setMinStep(double step) { m_minStep = step; }

  void compute();  // call compute() once, before calling the following methods

  int getTickCount() const { return m_tickCount; }
  double getTick(int index) const { return m_minValue + index * m_step; }
  bool isLabel(int index) const {
    return ((m_labelOffset + index) % m_labelPeriod) == 0;
  }
};

//-----------------------------------------------------------------------------

Ruler::Ruler()
    : m_minValue(0)
    , m_step(1)
    , m_labelPeriod(2)
    , m_labelOffset(0)
    , m_tickCount(0)
    , m_unit(1)
    , m_pan(0)
    , m_x0(0)
    , m_x1(100)
    , m_minLabelDistance(20)
    , m_minDistance(5)
    , m_minStep(0) {}

//-----------------------------------------------------------------------------

void Ruler::compute() {
  assert(m_x0 < m_x1);
  assert(m_unit != 0.0);
  assert(m_minLabelDistance > 0);
  assert(m_minDistance >= 0);
  // compute m_step (world distance between two adjacent ticks)
  // and m_labelPeriod (number of ticks between two adjacent labels)

  // the distance (world units) between two labels must be >=
  // minLabelWorldDistance
  const double absUnit               = std::abs(m_unit);
  const double minLabelWorldDistance = m_minLabelDistance / absUnit;
  const double minWorldDistance      = m_minDistance / absUnit;

  // we want the minimum step with:
  //    step*labelPeriod (i.e. label distance) >=  minLabelWorldDistance
  //    step (i.e. tick distance) >= minWorldDistance
  // Note: labelPeriod alternates between 5 and 2 => labelPeriod' =
  // 7-labelPeriod
  m_step        = 1;
  m_labelPeriod = 5;
  if (m_step * m_labelPeriod >= minLabelWorldDistance &&
      m_step >= minWorldDistance) {
    while (m_step >= minLabelWorldDistance &&
           m_step / (7 - m_labelPeriod) >= minWorldDistance) {
      m_labelPeriod = 7 - m_labelPeriod;
      m_step /= m_labelPeriod;
    }
  } else {
    do {
      m_step *= m_labelPeriod;
      m_labelPeriod =
          7 - m_labelPeriod;  // m_labelPeriod alternates between 5 and 2
    } while (m_step * m_labelPeriod < minLabelWorldDistance ||
             m_step < minWorldDistance);
  }

  if (m_step >= minLabelWorldDistance) {
    m_labelPeriod = 1;
  }

  if (m_step * m_labelPeriod < m_minStep) {
    m_step        = m_minStep;
    m_labelPeriod = 1;
  } else if (m_step < m_minStep) {
    m_step *= m_labelPeriod;
    m_labelPeriod = 1;
  }

  // compute range
  double v0 = (m_x0 - m_pan) / m_unit;  // left margin (world units)
  double v1 = (m_x1 - m_pan) / m_unit;  // right margin (world units)
  if (m_unit < 0) tswap(v0, v1);
  int i0 =
      tfloor((v0 - m_vOrigin) / m_step);  // largest tick <=v0 is i0 * m_step
  int i1 =
      tceil((v1 - m_vOrigin) / m_step);  // smallest tick >=v1 is i1 * m_step
  m_minValue  = i0 * m_step + m_vOrigin;
  m_tickCount = i1 - i0 + 1;

  m_labelOffset = i0 >= 0 ? (i0 % m_labelPeriod)
                          : (m_labelPeriod - ((-i0) % m_labelPeriod));
}

//=============================================================================

//=============================================================================
//
// FunctionPanel::Gadget
//
//-----------------------------------------------------------------------------

FunctionPanel::Gadget::Gadget(FunctionPanel::Handle handle, int kIndex,
                              const QPointF &p, int rx, int ry,
                              const QPointF &pointPos)
    : m_handle(handle)
    , m_kIndex(kIndex)
    , m_hitRegion(QRect((int)p.x() - rx, (int)p.y() - ry, 2 * rx, 2 * ry))
    , m_pos(p)
    , m_pointPos(pointPos)
    , m_channel(0)
    , m_keyframePosition(0) {}

//=============================================================================
//
// FunctionPanel
//
//-----------------------------------------------------------------------------

FunctionPanel::FunctionPanel(QWidget *parent, bool isFloating)
    : QDialog(parent)
    , m_functionTreeModel(0)
    , m_viewTransform()
    , m_valueAxisX(50)
    , m_frameAxisY(50)
    , m_graphViewportY(50)
    , m_frameHandle(0)
    , m_dragTool(0)
    , m_currentFrameStatus(0)
    , m_selection(0)
    , m_curveShape(SMOOTH)
    , m_isFloating(isFloating) {
  setWindowTitle(tr("Function Curves"));

  m_viewTransform.translate(50, 200);
  m_viewTransform.scale(5, -1);

  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  m_highlighted.handle = None;
  m_highlighted.gIndex = -1;
  m_cursor.visible     = false;
  m_cursor.frame = m_cursor.value = 0;
  m_curveLabel.text               = "";
  m_curveLabel.curve              = 0;

  if (m_isFloating) {
    // load the dialog size
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath(mySettingsFileName));
    QSettings mySettings(toQString(fp), QSettings::IniFormat);

    mySettings.beginGroup("Dialogs");
    setGeometry(
        mySettings.value("FunctionCurves", QRect(500, 500, 400, 300)).toRect());
    mySettings.endGroup();
  }
}

//-----------------------------------------------------------------------------

FunctionPanel::~FunctionPanel() {
  if (m_isFloating) {
    // save the dialog size
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath(mySettingsFileName));
    QSettings mySettings(toQString(fp), QSettings::IniFormat);

    mySettings.beginGroup("Dialogs");
    mySettings.setValue("FunctionCurves", geometry());
    mySettings.endGroup();
  }

  delete m_dragTool;
}

//-----------------------------------------------------------------------------

double FunctionPanel::getPixelRatio(TDoubleParam *curve) const {
  double framePixelSize = xToFrame(1) - xToFrame(0);
  assert(framePixelSize > 0);
  double valuePixelSize = fabs(yToValue(curve, 1) - yToValue(curve, 0));
  assert(valuePixelSize > 0);
  return framePixelSize / valuePixelSize;
}

//-----------------------------------------------------------------------------

double FunctionPanel::frameToX(double f) const {
  return m_viewTransform.m11() * f + m_viewTransform.dx();
}

//-----------------------------------------------------------------------------

double FunctionPanel::xToFrame(double x) const {
  return (x - m_viewTransform.dx()) / m_viewTransform.m11();
}

//-----------------------------------------------------------------------------

double FunctionPanel::valueToY(TDoubleParam *curve, double v) const {
  const double bigNumber = 1.0e9;
  TMeasure *m            = curve->getMeasure();
  if (m) {
    const TUnit *unit = m->getCurrentUnit();
    v                 = unit->convertTo(v);
  }
  return tcrop(m_viewTransform.m22() * v + m_viewTransform.dy(), -bigNumber,
               bigNumber);
}

//-----------------------------------------------------------------------------

double FunctionPanel::yToValue(TDoubleParam *curve, double y) const {
  double v    = (y - m_viewTransform.dy()) / m_viewTransform.m22();
  TMeasure *m = curve->getMeasure();
  if (m) {
    const TUnit *unit = m->getCurrentUnit();
    v                 = unit->convertFrom(v);
  }
  return v;
}

//-----------------------------------------------------------------------------

void FunctionPanel::pan(int dx, int dy) {
  QTransform m;
  m.translate(dx, dy);
  m_viewTransform *= m;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::zoom(double sx, double sy, const QPoint &center) {
  QTransform m;
  m.translate(center.x(), center.y());
  m.scale(sx, sy);
  m.translate(-center.x(), -center.y());
  m_viewTransform *= m;
  update();
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve, double frame,
                                 double value) const {
  return QPointF(frameToX(frame), valueToY(curve, value));
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve, double frame) const {
  return getWinPos(curve, frame, curve->getValue(frame));
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve,
                                 const TDoubleKeyframe &kf) const {
  return getWinPos(curve, kf.m_frame, kf.m_value);
}

//-----------------------------------------------------------------------------

int FunctionPanel::getCurveDistance(TDoubleParam *curve, const QPoint &winPos) {
  double frame  = xToFrame(winPos.x());
  double value  = curve->getValue(frame);
  double curveY = valueToY(curve, value);
  return std::abs(curveY - winPos.y());
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionPanel::findClosestChannel(
    const QPoint &winPos, int maxWinDistance) {
  FunctionTreeModel::Channel *closestChannel = 0;
  int minDistance                            = maxWinDistance;
  int i;
  for (i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    TDoubleParam *curve = channel->getParam();
    int distance        = getCurveDistance(curve, winPos);
    if (distance < minDistance) {
      closestChannel = channel;
      minDistance    = distance;
    }
  }
  return closestChannel;
}

//-----------------------------------------------------------------------------

TDoubleParam *FunctionPanel::findClosestCurve(const QPoint &winPos,
                                              int maxWinDistance) {
  FunctionTreeModel::Channel *closestChannel =
      findClosestChannel(winPos, maxWinDistance);
  return closestChannel ? closestChannel->getParam() : 0;
}

//-----------------------------------------------------------------------------

// return the gadget index (-1 if no gadget is close enough)
int FunctionPanel::findClosestGadget(const QPoint &winPos, Handle &handle,
                                     int maxDistance) {
  // search only handles close enough (i.e. distance<maxDistance)
  int minDistance = maxDistance;
  int k           = -1;
  for (int i = 0; i < m_gadgets.size(); i++) {
    if (m_gadgets[i].m_hitRegion.contains(winPos)) {
      QPoint p;
      double d = (m_gadgets[i].m_hitRegion.center() - winPos).manhattanLength();
      if (d < minDistance) {
        k           = i;
        minDistance = d;
      }
    }
  }
  if (k >= 0) {
    handle = m_gadgets[k].m_handle;
    return k;  // m_gadgets[k].m_kIndex;
  } else {
    handle = None;
    return -1;
  }
}

//-----------------------------------------------------------------------------

TDoubleParam *FunctionPanel::getCurrentCurve() const {
  FunctionTreeModel::Channel *currentChannel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!currentChannel)
    return 0;
  else
    return currentChannel->getParam();
}

//-----------------------------------------------------------------------------

QPainterPath FunctionPanel::getSegmentPainterPath(TDoubleParam *curve,
                                                  int segmentIndex, int x0,
                                                  int x1) {
  double frame0 = xToFrame(x0), frame1 = xToFrame(x1);
  int kCount = curve->getKeyframeCount();
  int step   = 1;
  if (kCount > 0) {
    if (segmentIndex < 0)
      frame1 = std::min(
          frame1, curve->keyframeIndexToFrame(0));  // before first keyframe
    else if (segmentIndex >= kCount - 1)
      frame0 = std::max(frame0, curve->keyframeIndexToFrame(
                                    kCount - 1));  // after last keyframe
    else {
      // between keyframes
      TDoubleKeyframe kf = curve->getKeyframe(segmentIndex);
      frame0             = std::max(frame0, kf.m_frame);
      double f           = curve->keyframeIndexToFrame(segmentIndex + 1);
      frame1             = std::min(frame1, f);
      step               = kf.m_step;
    }
  }
  if (frame0 >= frame1) return QPainterPath();
  double frame;
  double df = xToFrame(3) - xToFrame(0);

  if (m_curveShape == SMOOTH) {
    frame = frame0;
  } else  // FRAME_BASED
  {
    frame = (double)tfloor(frame0);
    df    = std::max(df, 1.0);
  }

  QPainterPath path;
  if (0 <= segmentIndex && segmentIndex < kCount && step > 1) {
    // step>1
    path.moveTo(getWinPos(curve, frame));

    int f0        = curve->keyframeIndexToFrame(segmentIndex);
    int vFrame    = f0 + tfloor(tfloor(frame - f0), step);
    double vValue = curve->getValue(vFrame);
    assert(vFrame <= frame);
    assert(vFrame + step > frame);
    while (vFrame + step < frame1) {
      vValue = curve->getValue(vFrame);
      path.lineTo(getWinPos(curve, vFrame, vValue));
      vFrame += step;
      path.lineTo(getWinPos(curve, vFrame, vValue));
      vValue = curve->getValue(vFrame);
      path.lineTo(getWinPos(curve, vFrame, vValue));
    }
    path.lineTo(getWinPos(curve, frame1, vValue));
    path.lineTo(getWinPos(curve, frame1, curve->getValue(frame1, true)));
  } else {
    // step = 1
    path.moveTo(getWinPos(curve, frame));
    while (frame + df < frame1) {
      frame += df;
      path.lineTo(getWinPos(curve, frame));
    }
    path.lineTo(getWinPos(curve, frame1, curve->getValue(frame1, true)));
  }
  return path;
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawCurrentFrame(QPainter &painter) {
  int currentframe                = 0;
  if (m_frameHandle) currentframe = m_frameHandle->getFrame();
  int x                           = frameToX(currentframe);
  if (m_currentFrameStatus == 0)
    painter.setPen(Qt::magenta);
  else if (m_currentFrameStatus == 1)
    painter.setPen(Qt::white);
  else
    painter.setPen(m_selectedColor);
  int y = m_graphViewportY + 1;
  painter.drawLine(x - 1, y, x - 1, height());
  painter.drawLine(x + 1, y, x + 1, height());
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawFrameGrid(QPainter &painter) {
  QFontMetrics fm(painter.font());

  // ruler background
  painter.setPen(Qt::NoPen);
  painter.setBrush(getRulerBackground());
  painter.drawRect(0, 0, width(), m_frameAxisY);

  // draw ticks and labels
  Ruler ruler;
  ruler.setTransform(m_viewTransform.m11(), m_viewTransform.dx(), -1);
  ruler.setRange(m_valueAxisX, width());
  ruler.setMinLabelDistance(fm.width("-8888") + 2);
  ruler.setMinDistance(5);
  ruler.setMinStep(1);
  ruler.compute();
  for (int i = 0; i < ruler.getTickCount(); i++) {
    double f     = ruler.getTick(i);
    bool isLabel = ruler.isLabel(i);
    int x        = frameToX(f);
    painter.setPen(m_textColor);
    int y = m_frameAxisY;
    painter.drawLine(x, y - (isLabel ? 4 : 2), x, y);
    painter.setPen(getFrameLineColor());
    painter.drawLine(x, m_graphViewportY, x, height());
    if (isLabel) {
      painter.setPen(m_textColor);
      QString labelText = QString::number(f + 1);
      painter.drawText(x - fm.width(labelText) / 2, y - 6, labelText);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawValueGrid(QPainter &painter) {
  TDoubleParam *curve = getCurrentCurve();
  if (!curve) return;

  QFontMetrics fm(painter.font());

  // ruler background
  painter.setPen(Qt::NoPen);
  painter.setBrush(getRulerBackground());
  painter.drawRect(0, 0, m_valueAxisX, height());

  Ruler ruler;
  ruler.setTransform(m_viewTransform.m22(), m_viewTransform.dy());
  ruler.setRange(m_graphViewportY, height());
  ruler.setMinLabelDistance(fm.height() + 2);
  ruler.setMinDistance(5);
  ruler.compute();

  painter.setBrush(Qt::NoBrush);
  for (int i = 0; i < ruler.getTickCount(); i++) {
    double v     = ruler.getTick(i);
    bool isLabel = ruler.isLabel(i);
    int y        = tround(m_viewTransform.m22() * v +
                   m_viewTransform.dy());  // valueToY(curve, v);
    painter.setPen(m_textColor);
    int x = m_valueAxisX;
    painter.drawLine(x - (isLabel ? 5 : 2), y, x, y);

    painter.setPen(getValueLineColor());
    painter.drawLine(x, y, width(), y);

    if (isLabel) {
      painter.setPen(m_textColor);
      QString labelText = QString::number(v);
      painter.drawText(std::max(0, x - 5 - fm.width(labelText)),
                       y + fm.height() / 2, labelText);
    }
  }
  if (false && ruler.getTickCount() > 10) {
    double value = ruler.getTick(9);
    double dv    = fabs(ruler.getTick(10));
    double frame = 10;
    double df    = dv * getPixelRatio(curve);
    QPointF p0   = getWinPos(curve, frame, value);
    QPointF p1   = getWinPos(curve, frame + df, value + dv);

    painter.setPen(Qt::magenta);
    painter.drawRect(p0.x(), p0.y(), (p1 - p0).x(), (p1 - p0).y());
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawOtherCurves(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setBrush(Qt::NoBrush);
  int x0 = m_valueAxisX;
  int x1 = width();

  QPen solidPen;
  QPen dashedPen;
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);

  for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    if (channel->isCurrent()) continue;
    TDoubleParam *curve = channel->getParam();
    QColor color =
        curve == m_curveLabel.curve ? m_selectedColor : getOtherCurvesColor();
    solidPen.setColor(color);
    dashedPen.setColor(color);
    painter.setBrush(Qt::NoBrush);

    int kCount = curve->getKeyframeCount();
    if (kCount == 0) {
      // no control points
      painter.setPen(dashedPen);
      painter.drawPath(getSegmentPainterPath(curve, 0, x0, x1));
    }
    // draw control points and handles
    else {
      for (int k = -1; k < kCount; k++) {
        painter.setPen((k < 0 || k >= kCount - 1) ? dashedPen : solidPen);
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      }
      painter.setPen(m_textColor);
      painter.setBrush(m_subColor);
      for (int k = 0; k < kCount; k++) {
        double frame = curve->keyframeIndexToFrame(k);
        QPointF p    = getWinPos(curve, frame, curve->getValue(frame));
        painter.drawRect(p.x() - 1, p.y() - 1, 3, 3);
        QPointF p2 = getWinPos(curve, frame, curve->getValue(frame, true));
        if (p2.y() != p.y()) {
          painter.drawRect(p2.x() - 1, p2.y() - 1, 3, 3);
          painter.setPen(solidPen);
          painter.drawLine(p, p2);
          painter.setPen(m_textColor);
        }
      }
    }
  }
  painter.setBrush(Qt::NoBrush);
  painter.setPen(m_textColor);
  painter.setRenderHint(QPainter::Antialiasing, false);
}

//-----------------------------------------------------------------------------

void FunctionPanel::updateGadgets(TDoubleParam *curve) {
  m_gadgets.clear();

  TDoubleKeyframe oldKf;
  oldKf.m_type = TDoubleKeyframe::None;

  int keyframeCount = curve->getKeyframeCount();

  for (int i = 0; i != keyframeCount; ++i) {
    const int pointHitRadius = 10, handleHitRadius = 6;

    TDoubleKeyframe kf = curve->getKeyframe(i);
    kf.m_value         = curve->getValue(
        kf.m_frame);  // Some keyframe values do NOT correspond to the
                      // actual displayed curve value (eg with expressions)
    // Build keyframe positions
    QPointF p     = getWinPos(curve, kf.m_frame);
    QPointF pLeft = p;

    if (i == keyframeCount - 1 &&
        curve
            ->isCycleEnabled())  // This is probably OBSOLETE. I don't think the
      p = getWinPos(curve, kf.m_frame,
                    curve->getValue(
                        kf.m_frame,
                        true));  // GUI allows cycling single curves nowadays...
                                 // However, is the assignment correct?
    // Add keyframe gadget(s)
    m_gadgets.push_back(Gadget(Point, i, p, pointHitRadius, pointHitRadius));

    TPointD currentPointRight(kf.m_frame, kf.m_value);
    TPointD currentPointLeft(currentPointRight);

    // If the previous segment or the current segment are not keyframe based,
    // the curve can have two different values in kf.m_frame
    if (i > 0 &&
        (!TDoubleKeyframe::isKeyframeBased(
             kf.m_type) ||  // Keyframe-based are the above mentioned curves
         !TDoubleKeyframe::isKeyframeBased(
             curve->getKeyframe(i - 1)
                 .m_type)))  // where values stored in keyframes are not used
    {                        // to calculate the actual curve values.
      currentPointLeft.y = curve->getValue(kf.m_frame, true);
      pLeft              = getWinPos(curve, currentPointLeft);
      m_gadgets.push_back(
          Gadget(Point, i, pLeft, pointHitRadius, pointHitRadius));
    }

    // Add handle gadgets (eg the speed or ease handles)
    if (getSelection()->isSelected(curve, i)) {
      // Left handle
      switch (oldKf.m_type) {
      case TDoubleKeyframe::SpeedInOut: {
        TPointD speedIn = curve->getSpeedIn(i);
        if (norm2(speedIn) > 0) {
          QPointF q = getWinPos(curve, currentPointLeft + speedIn);
          m_gadgets.push_back(
              Gadget(SpeedIn, i, q, handleHitRadius, handleHitRadius, pLeft));
        }
        break;
      }

      case TDoubleKeyframe::EaseInOut: {
        QPointF q = getWinPos(curve, kf.m_frame + kf.m_speedIn.x);
        m_gadgets.push_back(Gadget(EaseIn, i, q, 6, 15));
        break;
      }

      case TDoubleKeyframe::EaseInOutPercentage: {
        double easeIn = kf.m_speedIn.x * (kf.m_frame - oldKf.m_frame) * 0.01;
        QPointF q     = getWinPos(curve, kf.m_frame + easeIn);
        m_gadgets.push_back(Gadget(EaseInPercentage, i, q, 6, 15));
        break;
      }
      }

      // Right handle
      if (i != keyframeCount - 1) {
        switch (kf.m_type) {
        case TDoubleKeyframe::SpeedInOut: {
          TPointD speedOut = curve->getSpeedOut(i);
          if (norm2(speedOut) > 0) {
            QPointF q = getWinPos(curve, currentPointRight + speedOut);
            m_gadgets.push_back(
                Gadget(SpeedOut, i, q, handleHitRadius, handleHitRadius, p));
          }
          break;
        }

        case TDoubleKeyframe::EaseInOut: {
          QPointF q = getWinPos(curve, kf.m_frame + kf.m_speedOut.x);
          m_gadgets.push_back(Gadget(EaseOut, i, q, 6, 15));
          break;
        }

        case TDoubleKeyframe::EaseInOutPercentage: {
          double segmentWidth = curve->keyframeIndexToFrame(i + 1) - kf.m_frame;
          double easeOut      = segmentWidth * kf.m_speedOut.x * 0.01;

          QPointF q = getWinPos(curve, kf.m_frame + easeOut);
          m_gadgets.push_back(Gadget(EaseOutPercentage, i, q, 6, 15));
          break;
        }
        }
      }
    }

    oldKf = kf;
  }

  // Add group gadgets (ie those that can be added when multiple channels share
  // the same keyframe data)
  int channelCount = m_functionTreeModel->getActiveChannelCount();

  // Using a map of vectors. Yes, really. The *ideal* way would be that of
  // copying the first keyframes
  // vector, and then comparing it with the others from each channel - keeping
  // the common data only...

  typedef std::map<double, std::vector<TDoubleKeyframe>>
      KeyframeTable;  // frame -> { keyframes }
  KeyframeTable keyframes;

  for (int i = 0; i != channelCount; ++i) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    if (!channel) continue;

    TDoubleParam *curve = channel->getParam();
    for (int j = 0; j != curve->getKeyframeCount(); ++j) {
      TDoubleKeyframe kf = curve->getKeyframe(
          j);  // Well... this stuff gets called upon *painting*  o_o'
      keyframes[kf.m_frame].push_back(
          kf);  // It's bound to be slow. Do we really need it?
    }
  }

  int groupHandleY = m_graphViewportY - 6;

  KeyframeTable::iterator it, iEnd(keyframes.end()),
      iLast(keyframes.empty() ? iEnd : --iEnd);
  for (KeyframeTable::iterator it = keyframes.begin(); it != keyframes.end();
       ++it) {
    assert(!it->second.empty());

    double frame = it->first;  // redundant, already in the key... oh well
    QPointF p(frameToX(frame), groupHandleY);

    Gadget gadget((FunctionPanel::Handle)100, -1, p, 6,
                  6);  // No idea what the '100' type value mean...
    gadget.m_keyframePosition = frame;

    m_gadgets.push_back(gadget);

    TDoubleKeyframe kf = it->second[0];

    if ((int)it->second.size() < channelCount) continue;

    // All channels had this keyframe - so, add further gadgets about stuff...

    for (int i = 1; i < channelCount; ++i) {
      // Find out if keyframes data differs
      const TDoubleKeyframe &kf2 = it->second[i];

      if (kf.m_type != kf2.m_type || kf.m_speedOut.x != kf2.m_speedOut.x)
        kf.m_type = TDoubleKeyframe::None;
      if (kf.m_prevType != kf2.m_prevType || kf.m_speedIn.x != kf2.m_speedIn.x)
        kf.m_prevType = TDoubleKeyframe::None;
    }

    // NOTE: EaseInOutPercentage are currently NOT SUPPORTED - they would be
    // harder to code and
    //       controversial, since the handle position depends on the *segment
    //       size* too.
    //       So, keyframe data could be shared, but adjacent segment lengths
    //       could not...

    if (it != iLast && (kf.m_type == TDoubleKeyframe::SpeedInOut ||
                        kf.m_type == TDoubleKeyframe::EaseInOut) &&
        kf.m_speedOut.x != 0) {
      QPointF p(frameToX(frame + kf.m_speedOut.x), groupHandleY);
      Gadget gadget((FunctionPanel::Handle)101, -1, p, 6, 15);  // type value...
      gadget.m_keyframePosition = frame;
      m_gadgets.push_back(gadget);
    }

    if ((kf.m_prevType == TDoubleKeyframe::SpeedInOut ||
         kf.m_prevType == TDoubleKeyframe::EaseInOut) &&
        kf.m_speedIn.x != 0) {
      QPointF p(frameToX(frame + kf.m_speedIn.x), groupHandleY);
      Gadget gadget((FunctionPanel::Handle)102, -1, p, 6, 15);  // type value...
      gadget.m_keyframePosition = frame;
      m_gadgets.push_back(gadget);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawCurrentCurve(QPainter &painter) {
  FunctionTreeModel::Channel *channel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!channel) return;
  TDoubleParam *curve = channel->getParam();

  painter.setRenderHint(QPainter::Antialiasing, true);
  QColor color = Qt::red;
  QPen solidPen(color);
  QPen dashedPen(color);
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);
  painter.setBrush(Qt::NoBrush);

  int x0 = m_valueAxisX;
  int x1 = width();

  // draw curve
  int kCount = curve->getKeyframeCount();
  if (kCount == 0) {
    // no control points
    painter.setPen(dashedPen);
    painter.drawPath(getSegmentPainterPath(curve, 0, x0, x1));
  } else {
    for (int k = -1; k < kCount; k++) {
      if (k < 0 || k >= kCount - 1) {
        painter.setPen(dashedPen);
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      } else {
        TDoubleKeyframe::Type segmentType = curve->getKeyframe(k).m_type;
        QColor color                      = Qt::red;
        if (segmentType == TDoubleKeyframe::Expression ||
            segmentType == TDoubleKeyframe::SimilarShape ||
            segmentType == TDoubleKeyframe::File)
          color = QColor(185, 0, 0);
        if (getSelection()->isSegmentSelected(curve, k))
          solidPen.setWidth(2);
        else
          solidPen.setWidth(0);
        solidPen.setColor(color);
        painter.setPen(solidPen);
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      }
    }
  }
  painter.setPen(QPen(m_textColor, 0));

  // draw control points
  updateGadgets(curve);
  painter.setPen(m_textColor);
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    if (g.m_handle == SpeedIn || g.m_handle == SpeedOut)
      painter.drawLine(g.m_pointPos, g.m_pos);
  }
  solidPen.setWidth(0);
  solidPen.setColor(Qt::red);
  painter.setPen(solidPen);
  for (int j = 0; j < (int)m_gadgets.size() - 1; j++)
    if (m_gadgets[j].m_handle == Point && m_gadgets[j + 1].m_handle &&
        m_gadgets[j + 1].m_handle != 100 &&
        m_gadgets[j].m_pos.x() == m_gadgets[j + 1].m_pos.x())
      painter.drawLine(m_gadgets[j].m_pos, m_gadgets[j + 1].m_pos);

  painter.setRenderHint(QPainter::Antialiasing, false);
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    int i           = g.m_kIndex;
    int r           = 1;
    QPointF p       = g.m_pos;
    double easeDx = 0, easeHeight = 15, easeTick = 2;
    bool isSelected = getSelection()->isSelected(curve, i);
    bool isHighlighted =
        m_highlighted.handle == g.m_handle && m_highlighted.gIndex == j;
    switch (g.m_handle) {
    case Point:
      painter.setBrush(isSelected ? QColor(255, 126, 0) : m_subColor);
      painter.setPen(m_textColor);
      r = isHighlighted ? 3 : 2;
      drawSquare(painter, p, r);
      break;

    case SpeedIn:
    case SpeedOut:
      painter.setBrush(m_subColor);
      painter.setPen(m_textColor);
      r = isHighlighted ? 3 : 2;
      drawRoundedSquare(painter, p, r);
      break;

    case EaseIn:
    case EaseOut:
    case EaseInPercentage:
    case EaseOutPercentage:
      painter.setBrush(Qt::NoBrush);
      painter.setPen(isHighlighted ? QColor(255, 126, 0) : m_textColor);
      painter.drawLine(p.x(), p.y() - easeHeight, p.x(), p.y() + easeHeight);
      if (g.m_handle == EaseIn || g.m_handle == EaseInPercentage)
        easeDx = easeTick;
      else
        easeDx = -easeTick;
      painter.drawLine(p.x(), p.y() - easeHeight, p.x() + easeDx,
                       p.y() - easeHeight - easeTick);
      painter.drawLine(p.x(), p.y() + easeHeight, p.x() + easeDx,
                       p.y() + easeHeight + easeTick);
      break;

      painter.setBrush(
          Qt::NoBrush);  // isSelected ? QColor(255,126,0) : Qt::white);
      painter.setPen(isHighlighted ? QColor(255, 126, 0) : m_selectedColor);
      painter.drawLine(p.x(), p.y() - 15, p.x(), p.y() + 15);
      break;
    }
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawGroupKeyframes(QPainter &painter) {
  FunctionTreeModel::Channel *channel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!channel) return;
  QColor color = Qt::red;
  QPen solidPen(color);
  QPen dashedPen(color);
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);
  painter.setBrush(Qt::NoBrush);

  int x0 = m_valueAxisX;
  int x1 = width();

  solidPen.setWidth(0);
  solidPen.setColor(Qt::red);
  painter.setPen(solidPen);

  std::vector<double> keyframes;
  int y = 0;
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    int i           = g.m_kIndex;
    int r           = 1;
    QPointF p       = g.m_pos;
    double easeDx = 0, easeHeight = 15, easeTick = 2;
    bool isSelected = false;  // getSelection()->isSelected(curve, i);
    bool isHighlighted =
        m_highlighted.handle == g.m_handle && m_highlighted.gIndex == j;
    painter.setBrush(isSelected ? QColor(255, 126, 0) : m_subColor);
    painter.setPen(m_textColor);
    r = isHighlighted ? 3 : 2;
    QPainterPath pp;
    int d = 2;
    int h = 4;
    switch (g.m_handle) {
    case 100:
      drawSquare(painter, p, r);
      y = p.y();
      keyframes.push_back(p.x());
      break;
    case 101:
      d = -d;
    // Note: NO break!
    case 102:
      painter.setBrush(Qt::NoBrush);
      painter.setPen(isHighlighted ? QColor(255, 126, 0) : m_textColor);
      pp.moveTo(p + QPointF(d, -h));
      pp.lineTo(p + QPointF(0, -h));
      pp.lineTo(p + QPointF(0, h));
      pp.lineTo(p + QPointF(d, h));
      painter.drawPath(pp);
      break;
    }
  }
  painter.setPen(m_textColor);
  for (int i = 0; i + 1 < (int)keyframes.size(); i++) {
    painter.drawLine(keyframes[i] + 3, y, keyframes[i + 1] - 3, y);
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::paintEvent(QPaintEvent *e) {
  m_gadgets.clear();

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }

  QPainter painter(this);
  QFont font(fontName, 8);
  painter.setFont(font);
  QFontMetrics fm(font);

  // define ruler sizes
  m_valueAxisX     = fm.width("-888.88") + 2;
  m_frameAxisY     = fm.height() + 2;
  m_graphViewportY = m_frameAxisY + 12;
  int ox           = m_valueAxisX;
  int oy0          = m_graphViewportY;
  int oy1          = m_frameAxisY;

  // QRect bounds(0,0,width(),height());

  // draw functions background
  painter.setBrush(getBGColor());
  painter.setPen(Qt::NoPen);
  painter.drawRect(ox, oy0, width() - ox, height() - oy0);

  painter.setClipRect(ox, 0, width() - ox, height());
  drawCurrentFrame(painter);
  drawFrameGrid(painter);

  painter.setClipRect(0, oy0, width(), height() - oy0);
  drawValueGrid(painter);

  // draw axes
  painter.setClipping(false);
  painter.setPen(m_textColor);
  painter.drawLine(0, oy0, width(), oy0);
  painter.drawLine(ox, oy1, width(), oy1);
  painter.drawLine(ox, 0, ox, height());

  // draw curves
  painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);
  drawOtherCurves(painter);
  drawCurrentCurve(painter);

  painter.setClipping(false);
  painter.setClipRect(ox + 1, oy1 + 1, width() - ox - 1, oy0 - oy1 - 1);
  drawGroupKeyframes(painter);
  painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);

  // tool
  if (m_dragTool) m_dragTool->draw(painter);

  // cursor
  if (m_cursor.visible) {
    painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);
    painter.setPen(getOtherCurvesColor());
    int x = frameToX(m_cursor.frame);
    painter.drawLine(x, oy0 + 1, x, oy0 + 10);
    QString text = QString::number(tround(m_cursor.frame) + 1);
    painter.drawText(x - fm.width(text) / 2, oy0 + 10 + fm.height(), text);

    TDoubleParam *currentCurve = getCurrentCurve();
    if (currentCurve) {
      const TUnit *unit = 0;
      if (currentCurve->getMeasure())
        unit                 = currentCurve->getMeasure()->getCurrentUnit();
      double displayValue    = m_cursor.value;
      if (unit) displayValue = unit->convertTo(displayValue);
      // painter.setClipRect(0,oy0,height(),height()-oy0);
      int y = valueToY(currentCurve, m_cursor.value);
      painter.drawLine(ox, y, ox + 10, y);
      painter.drawText(m_origin.x() + 10, y + 4, QString::number(displayValue));
    }
  }

  // curve name
  if (m_curveLabel.text != "") {
    painter.setClipRect(ox, oy0, width() - ox, height() - oy0);
    painter.setPen(m_selectedColor);
    painter.drawLine(m_curveLabel.curvePos, m_curveLabel.labelPos);
    painter.drawText(m_curveLabel.labelPos,
                     QString::fromStdString(m_curveLabel.text));
  }

  // painter.setPen(Qt::black);
  // painter.drawText(QPointF(70,70),
  //  "f0=" + QString::number(xToFrame(ox)) +
  //  " f1=" + QString::number(xToFrame(width())));

  // painter.setPen(Qt::black);
  // painter.setBrush(Qt::NoBrush);
  // painter.drawRect(ox+10,oy+10,width()-ox-20,height()-oy-20);
}

//-----------------------------------------------------------------------------

void FunctionPanel::mousePressEvent(QMouseEvent *e) {
  m_cursor.visible = false;

  // m_dragTool should be 0. just in case...
  assert(m_dragTool == 0);
  m_dragTool = 0;

  if (e->button() == Qt::MidButton) {
    // mid mouse click => panning
    bool xLocked = e->pos().x() <= m_valueAxisX;
    bool yLocked = e->pos().y() <= m_valueAxisX;
    m_dragTool   = new PanDragTool(this, xLocked, yLocked);
    m_dragTool->click(e);
    return;
  } else if (e->button() == Qt::RightButton) {
    // right mouse click => open context menu
    openContextMenu(e);
    return;
  }

  QPoint winPos         = e->pos();
  Handle handle         = None;
  const int maxDistance = 20;
  int closestGadgetId   = findClosestGadget(e->pos(), handle, maxDistance);

  if (e->pos().x() > m_valueAxisX && e->pos().y() < m_frameAxisY &&
      closestGadgetId < 0 && (e->modifiers() & Qt::ControlModifier) == 0) {
    // click on topbar => frame zoom
    m_dragTool = new ZoomDragTool(this, ZoomDragTool::FrameZoom);
  } else if (e->pos().x() < m_valueAxisX && e->pos().y() > m_graphViewportY) {
    // click on topbar => value zoom
    m_dragTool = new ZoomDragTool(this, ZoomDragTool::ValueZoom);
  } else if (m_currentFrameStatus == 1 && m_frameHandle != 0 &&
             closestGadgetId < 0) {
    // click on current frame => move frame
    m_currentFrameStatus = 2;
    m_dragTool           = new MoveFrameDragTool(this, m_frameHandle);
  }

  if (0 <= closestGadgetId && closestGadgetId < (int)m_gadgets.size()) {
    if (handle == 100)  // group move gadget
    {
      MovePointDragTool *dragTool = new MovePointDragTool(this, 0);
      dragTool->selectKeyframes(m_gadgets[closestGadgetId].m_keyframePosition);
      m_dragTool = dragTool;
    } else if (handle == 101 || handle == 102) {
      m_dragTool = new MoveGroupHandleDragTool(
          this, m_gadgets[closestGadgetId].m_keyframePosition, handle);
    }
  }

  if (m_dragTool) {
    m_dragTool->click(e);
    return;
  }

  FunctionTreeModel::Channel *currentChannel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!currentChannel ||
      getCurveDistance(currentChannel->getParam(), winPos) > maxDistance &&
          closestGadgetId < 0) {
    // if current channel is undefined or its curve is too far from the clicked
    // point
    // the user is possibly trying to select a different curve
    FunctionTreeModel::Channel *channel =
        findClosestChannel(winPos, maxDistance);
    if (channel) {
      channel->setIsCurrent(true);
      // Open folder
      FunctionTreeModel::ChannelGroup *channelGroup =
          channel->getChannelGroup();
      if (!channelGroup->isOpen())
        channelGroup->getModel()->setExpandedItem(channelGroup->createIndex(),
                                                  true);
      currentChannel = channel;
      getSelection()->selectNone();
    }
  }

  if (currentChannel) {
    TDoubleParam *currentCurve = currentChannel->getParam();
    if (currentCurve) {
      int kIndex =
          closestGadgetId >= 0 ? m_gadgets[closestGadgetId].m_kIndex : -1;
      if (kIndex >= 0) {
        // keyframe clicked
        if (handle == FunctionPanel::Point) {
          // select point (if needed)
          if (!getSelection()->isSelected(currentCurve, kIndex)) {
            // shift-click => add to selection
            if (0 == (e->modifiers() & Qt::ShiftModifier))
              getSelection()->deselectAllKeyframes();
            getSelection()->select(currentCurve, kIndex);
          }
          // move selected point(s)
          MovePointDragTool *dragTool =
              new MovePointDragTool(this, currentCurve);
          if (getSelection()->getSelectedSegment().first != 0) {
            // if a segment is selected then move only the clicked point
            dragTool->addKeyframe2(kIndex);
          } else {
            dragTool->setSelection(getSelection());
          }
          m_dragTool = dragTool;
        } else {
          m_dragTool =
              new MoveHandleDragTool(this, currentCurve, kIndex, handle);
        }
      } else {
        // no keyframe clicked
        int curveDistance =
            getCurveDistance(currentChannel->getParam(), winPos);
        bool isKeyframeable = true;
        bool isGroup        = abs(winPos.y() - (m_graphViewportY - 5)) < 5;
        if (0 != (e->modifiers() & Qt::ControlModifier) &&
            (curveDistance < maxDistance || isGroup) && isKeyframeable) {
          // ctrl-clicked near curve => create a new keyframe
          double frame = tround(xToFrame(winPos.x()));
          MovePointDragTool *dragTool =
              new MovePointDragTool(this, isGroup ? 0 : currentCurve);
          //          if(curveDistance>=maxDistance)
          //            dragTool->m_channelGroup =
          //            currentChannel->getChannelGroup();
          dragTool->createKeyframe(frame);
          dragTool->selectKeyframes(frame);
          m_dragTool = dragTool;

          /*
int kIndex = dragTool->createKeyframe(frame);
          if(kIndex!=-1)
          {
                  getSelection()->deselectAllKeyframes();
                  getSelection()->select(currentCurve, kIndex);
                  m_dragTool = dragTool;
          }
*/
          // assert(0);
        } else if (curveDistance < maxDistance) {
          // clicked near curve (but far from keyframes)
          getSelection()->deselectAllKeyframes();
          double frame = xToFrame(winPos.x());
          int k0       = currentCurve->getPrevKeyframe(frame);
          int k1       = currentCurve->getNextKeyframe(frame);
          if (k0 >= 0 && k1 == k0 + 1) {
            // select and move the segment
            getSelection()->selectSegment(currentCurve, k0);
            MovePointDragTool *dragTool =
                new MovePointDragTool(this, currentCurve);
            dragTool->addKeyframe2(k0);
            dragTool->addKeyframe2(k1);
            m_dragTool = dragTool;
          } else {
            // start a rectangular selection
            m_dragTool = new RectSelectTool(this, currentCurve);
          }
        } else {
          // nothing clicked: start a rectangular selection
          getSelection()->deselectAllKeyframes();
          m_dragTool = new RectSelectTool(this, currentCurve);
        }
      }
    }
  }

  if (m_dragTool) m_dragTool->click(e);
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::mouseReleaseEvent(QMouseEvent *e) {
  if (m_dragTool) m_dragTool->release(e);
  delete m_dragTool;
  m_dragTool           = 0;
  m_cursor.visible     = true;
  m_currentFrameStatus = 0;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons()) {
    if (m_dragTool) m_dragTool->drag(e);
  } else {
    m_cursor.frame   = xToFrame(e->pos().x());
    m_cursor.value   = 0;
    m_cursor.visible = true;

    TDoubleParam *currentCurve = getCurrentCurve();
    if (currentCurve) {
      Handle handle = None;
      int gIndex    = findClosestGadget(e->pos(), handle, 20);
      if (m_highlighted.handle != handle || m_highlighted.gIndex != gIndex) {
        m_highlighted.handle = handle;
        m_highlighted.gIndex = gIndex;
      }
      m_cursor.value = yToValue(currentCurve, e->pos().y());
    }

    double currentFrame = m_frameHandle ? m_frameHandle->getFrame() : 0;
    if (m_highlighted.handle == None &&
        std::abs(e->pos().x() - frameToX(currentFrame)) < 5)
      m_currentFrameStatus = 1;
    else
      m_currentFrameStatus = 0;

    FunctionTreeModel::Channel *closestChannel =
        findClosestChannel(e->pos(), 20);
    if (closestChannel && m_highlighted.handle == None) {
      TDoubleParam *curve = closestChannel->getParam();
      if (m_functionTreeModel->getActiveChannelCount() <= 1)
        //|| closestChannel == m_functionTreeModel->getCurrentChannel())
        curve = 0;
      if (curve && m_curveLabel.curve != curve) {
        m_curveLabel.curve  = curve;
        QString channelName = closestChannel->data(Qt::DisplayRole).toString();
        QString parentChannelName =
            closestChannel->getChannelGroup()->data(Qt::DisplayRole).toString();
        QString name      = parentChannelName + QString(", ") + channelName;
        m_curveLabel.text = name.toStdString();

        // in order to avoid run off the right-end of visible area
        int textWidth = fontMetrics().width(name) + 30;
        double frame  = xToFrame(width() - textWidth);

        m_curveLabel.curvePos = getWinPos(curve, frame).toPoint();
        m_curveLabel.labelPos = m_curveLabel.curvePos + QPoint(20, -10);
      }
    } else {
      m_curveLabel.text  = "";
      m_curveLabel.curve = 0;
    }

    update();
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::keyPressEvent(QKeyEvent *e) {
  FunctionPanelZoomer(this).exec(e);
}

//-----------------------------------------------------------------------------

void FunctionPanel::enterEvent(QEvent *) {
  m_cursor.visible = true;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::leaveEvent(QEvent *) {
  m_cursor.visible = false;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::wheelEvent(QWheelEvent *e) {
  double factor = exp(0.002 * (double)e->delta());
  zoom(factor, factor, e->pos());
}

//-----------------------------------------------------------------------------

void FunctionPanel::fitGraphToWindow(bool currentCurveOnly) {
  double f0 = 0, f1 = -1;
  double v0 = 0, v1 = -1;

  for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    TDoubleParam *curve = channel->getParam();
    if (currentCurveOnly && curve != getCurrentCurve()) continue;

    const TUnit *unit             = 0;
    if (curve->getMeasure()) unit = curve->getMeasure()->getCurrentUnit();
    int n                         = curve->getKeyframeCount();
    if (n == 0) {
      double v    = curve->getDefaultValue();
      if (unit) v = unit->convertTo(v);
      if (v0 > v1)
        v0 = v1 = v;
      else if (v > v1)
        v1 = v;
      else if (v < v0)
        v0 = v;
    } else {
      TDoubleKeyframe k = curve->getKeyframe(0);
      double fa         = k.m_frame;
      k                 = curve->getKeyframe(n - 1);
      double fb         = k.m_frame;
      if (f0 > f1) {
        f0 = fa;
        f1 = fb;
      } else {
        f0 = qMin(f0, fa);
        f1 = qMax(f1, fb);
      }
      double v        = curve->getValue(fa);
      if (unit) v     = unit->convertTo(v);
      if (v0 > v1) v0 = v1 = v;
      int m                = 50;
      for (int j = 0; j < m; j++) {
        double t    = (double)j / (double)(m - 1);
        double v    = curve->getValue((1 - t) * fa + t * fb);
        if (unit) v = unit->convertTo(v);
        v0          = qMin(v0, v);
        v1          = qMax(v1, v);
      }
    }
  }
  if (f0 >= f1 || v0 >= v1) {
    m_viewTransform = QTransform();
    m_viewTransform.translate(m_valueAxisX, 200);
    m_viewTransform.scale(5, -1);
  } else {
    double mx       = (width() - m_valueAxisX - 20) / (f1 - f0);
    double my       = -(height() - m_graphViewportY - 20) / (v1 - v0);
    double dx       = m_valueAxisX + 10 - f0 * mx;
    double dy       = m_graphViewportY + 10 - v1 * my;
    m_viewTransform = QTransform(mx, 0, 0, my, dx, dy);
  }
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::fitSelectedPoints() { fitGraphToWindow(true); }

//-----------------------------------------------------------------------------

void FunctionPanel::fitCurve() { fitGraphToWindow(); }

//-----------------------------------------------------------------------------

void FunctionPanel::fitRegion(double f0, double v0, double f1, double v1) {}

//-----------------------------------------------------------------------------

static void setSegmentType(FunctionSelection *selection, TDoubleParam *curve,
                           int segmentIndex, TDoubleKeyframe::Type type) {
  selection->selectSegment(curve, segmentIndex);
  KeyframeSetter setter(curve, segmentIndex);
  setter.setType(type);
}

//-----------------------------------------------------------------------------

void FunctionPanel::openContextMenu(QMouseEvent *e) {
  QAction linkHandlesAction(tr("Link Handles"), 0);
  QAction unlinkHandlesAction(tr("Unlink Handles"), 0);
  QAction resetHandlesAction(tr("Reset Handles"), 0);
  QAction deleteKeyframeAction(tr("Delete"), 0);
  QAction insertKeyframeAction(tr("Set Key"), 0);
  QAction activateCycleAction(tr("Activate Cycle"), 0);
  QAction deactivateCycleAction(tr("Deactivate Cycle"), 0);
  QAction setLinearAction(tr("Linear Interpolation"), 0);
  QAction setSpeedInOutAction(tr("Speed In / Speed Out Interpolation"), 0);
  QAction setEaseInOutAction(tr("Ease In / Ease Out Interpolation"), 0);
  QAction setEaseInOut2Action(tr("Ease In / Ease Out (%) Interpolation"), 0);
  QAction setExponentialAction(tr("Exponential Interpolation"), 0);
  QAction setExpressionAction(tr("Expression Interpolation"), 0);
  QAction setFileAction(tr("File Interpolation"), 0);
  QAction setConstantAction(tr("Constant Interpolation"), 0);
  QAction setSimilarShapeAction(tr("Similar Shape Interpolation"), 0);
  QAction fitSelectedAction(tr("Fit Selection"), 0);
  QAction fitAllAction(tr("Fit"), 0);
  QAction setStep1Action(tr("Step 1"), 0);
  QAction setStep2Action(tr("Step 2"), 0);
  QAction setStep3Action(tr("Step 3"), 0);
  QAction setStep4Action(tr("Step 4"), 0);

  TDoubleParam *curve = getCurrentCurve();
  int segmentIndex    = -1;
  if (!curve) return;
  TDoubleKeyframe kf;
  double frame = xToFrame(e->pos().x());

  // build menu
  QMenu menu(0);
  if (m_highlighted.handle == Point && m_highlighted.gIndex >= 0 &&
      m_gadgets[m_highlighted.gIndex].m_handle != 100) {
    kf = curve->getKeyframe(m_gadgets[m_highlighted.gIndex].m_kIndex);
    if (kf.m_linkedHandles)
      menu.addAction(&unlinkHandlesAction);
    else
      menu.addAction(&linkHandlesAction);
    menu.addAction(&resetHandlesAction);
    menu.addAction(&deleteKeyframeAction);
  } else {
    int k0 = curve->getPrevKeyframe(frame);
    int k1 = curve->getNextKeyframe(frame);
    if (k0 == curve->getKeyframeCount() - 1)  // after last keyframe
    {
      if (curve->isCycleEnabled())
        menu.addAction(&deactivateCycleAction);
      else
        menu.addAction(&activateCycleAction);
    }
    menu.addAction(&insertKeyframeAction);
    if (k0 >= 0 && k1 >= 0) {
      menu.addSeparator();
      segmentIndex = k0;
      kf           = curve->getKeyframe(k0);
      menu.addAction(&setLinearAction);
      if (kf.m_type == TDoubleKeyframe::Linear)
        setLinearAction.setEnabled(false);
      menu.addAction(&setSpeedInOutAction);
      if (kf.m_type == TDoubleKeyframe::SpeedInOut)
        setSpeedInOutAction.setEnabled(false);
      menu.addAction(&setEaseInOutAction);
      if (kf.m_type == TDoubleKeyframe::EaseInOut)
        setEaseInOutAction.setEnabled(false);
      menu.addAction(&setEaseInOut2Action);
      if (kf.m_type == TDoubleKeyframe::EaseInOutPercentage)
        setEaseInOut2Action.setEnabled(false);
      menu.addAction(&setExponentialAction);
      if (kf.m_type == TDoubleKeyframe::Exponential)
        setExponentialAction.setEnabled(false);
      menu.addAction(&setExpressionAction);
      if (kf.m_type == TDoubleKeyframe::Expression)
        setExpressionAction.setEnabled(false);
      menu.addAction(&setSimilarShapeAction);
      if (kf.m_type == TDoubleKeyframe::SimilarShape)
        setSimilarShapeAction.setEnabled(false);
      menu.addAction(&setFileAction);
      if (kf.m_type == TDoubleKeyframe::File) setFileAction.setEnabled(false);
      menu.addAction(&setConstantAction);
      if (kf.m_type == TDoubleKeyframe::Constant)
        setConstantAction.setEnabled(false);
      menu.addSeparator();
      if (kf.m_step != 1) menu.addAction(&setStep1Action);
      if (kf.m_step != 2) menu.addAction(&setStep2Action);
      if (kf.m_step != 3) menu.addAction(&setStep3Action);
      if (kf.m_step != 4) menu.addAction(&setStep4Action);
      menu.addSeparator();
    }
  }
  if (!getSelection()->isEmpty()) menu.addAction(&fitSelectedAction);
  menu.addAction(&fitAllAction);

  // curve shape type
  QAction curveShapeSmoothAction(tr("Smooth"), 0);
  QAction curveShapeFrameBasedAction(tr("Frame Based"), 0);
  QMenu curveShapeSubmenu(tr("Curve Shape"), 0);
  menu.addSeparator();
  curveShapeSubmenu.addAction(&curveShapeSmoothAction);
  curveShapeSubmenu.addAction(&curveShapeFrameBasedAction);
  menu.addMenu(&curveShapeSubmenu);

  curveShapeSmoothAction.setCheckable(true);
  curveShapeSmoothAction.setChecked(m_curveShape == SMOOTH);
  curveShapeFrameBasedAction.setCheckable(true);
  curveShapeFrameBasedAction.setChecked(m_curveShape == FRAME_BASED);

  // Store m_highlighted due to the following exec()
  Highlighted highlighted(m_highlighted);

  // execute menu
  QAction *action = menu.exec(e->globalPos());  // Will process events, possibly
                                                // altering m_highlighted
                                                // (MAC-verified)
  if (action == &linkHandlesAction)  // Let's just *hope* that doesn't happen to
                                     // m_gadgets though...  :/
  {
    if (m_gadgets[highlighted.gIndex].m_handle != 100)
      KeyframeSetter(curve, m_gadgets[highlighted.gIndex].m_kIndex)
          .linkHandles();
  } else if (action == &unlinkHandlesAction) {
    if (m_gadgets[highlighted.gIndex].m_handle != 100)
      KeyframeSetter(curve, m_gadgets[highlighted.gIndex].m_kIndex)
          .unlinkHandles();
  } else if (action == &resetHandlesAction) {
    kf.m_speedIn  = TPointD(-5, 0);
    kf.m_speedOut = -kf.m_speedIn;
    curve->setKeyframe(kf);
  } else if (action == &deleteKeyframeAction) {
    KeyframeSetter::removeKeyframeAt(curve, kf.m_frame);
  } else if (action == &insertKeyframeAction) {
    KeyframeSetter(curve).createKeyframe(tround(frame));
  } else if (action == &activateCycleAction) {
    KeyframeSetter::enableCycle(curve, true);
  } else if (action == &deactivateCycleAction) {
    KeyframeSetter::enableCycle(curve, false);
  } else if (action == &setLinearAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Linear);
  else if (action == &setSpeedInOutAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::SpeedInOut);
  else if (action == &setEaseInOutAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::EaseInOut);
  else if (action == &setEaseInOut2Action)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::EaseInOutPercentage);
  else if (action == &setExponentialAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Exponential);
  else if (action == &setExpressionAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Expression);
  else if (action == &setSimilarShapeAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::SimilarShape);
  else if (action == &setFileAction)
    setSegmentType(getSelection(), curve, segmentIndex, TDoubleKeyframe::File);
  else if (action == &setConstantAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Constant);
  else if (action == &fitSelectedAction)
    fitSelectedPoints();
  else if (action == &fitAllAction)
    fitCurve();
  else if (action == &setStep1Action)
    KeyframeSetter(curve, segmentIndex).setStep(1);
  else if (action == &setStep2Action)
    KeyframeSetter(curve, segmentIndex).setStep(2);
  else if (action == &setStep3Action)
    KeyframeSetter(curve, segmentIndex).setStep(3);
  else if (action == &setStep4Action)
    KeyframeSetter(curve, segmentIndex).setStep(4);

  else if (action == &curveShapeSmoothAction)
    m_curveShape = SMOOTH;
  else if (action == &curveShapeFrameBasedAction)
    m_curveShape = FRAME_BASED;

  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::setFrameHandle(TFrameHandle *frameHandle) {
  if (m_frameHandle == frameHandle) return;
  if (m_frameHandle) m_frameHandle->disconnect(this);
  m_frameHandle = frameHandle;
  if (isVisible() && m_frameHandle) {
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));
    update();
  }
  assert(m_selection);
  m_selection->setFrameHandle(frameHandle);
}

//-----------------------------------------------------------------------------

void FunctionPanel::showEvent(QShowEvent *) {
  if (m_frameHandle)
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));
}

//-----------------------------------------------------------------------------

void FunctionPanel::hideEvent(QHideEvent *) {
  if (m_frameHandle) m_frameHandle->disconnect(this);
}

//-----------------------------------------------------------------------------

void FunctionPanel::onFrameSwitched() { update(); }
