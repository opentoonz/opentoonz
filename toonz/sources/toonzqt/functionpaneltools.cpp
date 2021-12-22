

#include "functionpaneltools.h"

// TnzQt includes
#include "toonzqt/functionselection.h"

// TnzLib includes
#include "toonz/tframehandle.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>

//=============================================================================

MoveFrameDragTool::MoveFrameDragTool(FunctionPanel *panel,
                                     TFrameHandle *frameHandle)
    : m_panel(panel), m_frameHandle(frameHandle) {}

void MoveFrameDragTool::drag(QMouseEvent *e) {
  double frame = m_panel->xToFrame(e->pos().x());
  m_panel->getSelection()->deselectAllKeyframes();
  m_frameHandle->setFrame(frame);
}

//=============================================================================

PanDragTool::PanDragTool(FunctionPanel *panel, bool xLocked, bool yLocked)
    : m_panel(panel), m_xLocked(xLocked), m_yLocked(yLocked) {}

void PanDragTool::click(QMouseEvent *e) { m_oldPos = e->pos(); }

void PanDragTool::drag(QMouseEvent *e) {
  QPoint delta = e->pos() - m_oldPos;
  if (m_xLocked) delta.setX(0);
  if (m_yLocked) delta.setY(0);
  m_panel->pan(delta);
  m_oldPos = e->pos();
}

//=============================================================================
/*--- Ruler部分ドラッグによるズーム ---*/
void ZoomDragTool::click(QMouseEvent *e) {
  m_startPos = m_oldPos = m_startPos = e->pos();
}

void ZoomDragTool::drag(QMouseEvent *e) {
  QPoint delta = e->pos() - m_oldPos;
  m_oldPos     = e->pos();
  double sx = 1, sy = 1;
  // reflect horizontal drag for frame zoom
  double zoomFactor =
      exp(-0.0075 * ((m_zoomType == FrameZoom) ? -delta.x() : delta.y()));
  if (m_zoomType == FrameZoom)
    sx = zoomFactor;
  else
    sy = zoomFactor;
  m_panel->zoom(sx, sy, m_startPos);
}

void ZoomDragTool::release(QMouseEvent *e) {
  if ((e->pos() - m_startPos).manhattanLength() < 2) {
    double frame = m_panel->xToFrame(e->pos().x());
    if (m_panel->getFrameHandle()) m_panel->getFrameHandle()->setFrame(frame);
  }
}

//=============================================================================

void RectSelectTool::click(QMouseEvent *e) {
  m_startPos = e->pos();
  m_rect     = QRect();
}

void RectSelectTool::drag(QMouseEvent *e) {
  m_rect = QRect(m_startPos, e->pos()).normalized();
  m_panel->getSelection()->deselectAllKeyframes();
  for (int i = 0; i < m_curve->getKeyframeCount(); i++) {
    QPointF p = m_panel->getWinPos(m_curve, m_curve->getKeyframe(i));
    if (m_rect.contains(tround(p.x()), tround(p.y())))
      m_panel->getSelection()->select(m_curve, i);
  }
  m_panel->update();
}

void RectSelectTool::release(QMouseEvent *e) { m_panel->update(); }

void RectSelectTool::draw(QPainter &painter) {
  painter.setPen(Qt::white);
  painter.setBrush(QColor(255, 255, 255, 127));
  if (!m_rect.isEmpty()) painter.drawRect(m_rect);
}

//=============================================================================

MovePointDragTool::MovePointDragTool(FunctionPanel *panel, TDoubleParam *curve)
    : m_panel(panel)
    , m_deltaFrame(0)
    , m_speed0Length(0)
    , m_speed0Index(-1)
    , m_speed1Length(0)
    , m_speed1Index(-1)
    , m_groupEnabled(false)
    , m_selection(0) {
  // This undo block is closed in the destructor
  TUndoManager::manager()->beginBlock();

  if (curve) {
    KeyframeSetter *setter = new KeyframeSetter(curve);
    m_setters.push_back(setter);
  } else {
    m_groupEnabled           = true;
    FunctionTreeModel *model = panel->getModel();
    for (int i = 0; i < model->getActiveChannelCount(); i++) {
      FunctionTreeModel::Channel *channel = model->getActiveChannel(i);
      if (channel && channel->getParam()) {
        TDoubleParam *curve    = channel->getParam();
        KeyframeSetter *setter = new KeyframeSetter(curve);
        m_setters.push_back(setter);
      }
    }
  }
}

//-----------------------------------------------------------------------------

MovePointDragTool::~MovePointDragTool() {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i];
  m_setters.clear();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void MovePointDragTool::addKeyframe2(int kIndex) {
  assert(m_setters.size() == 1);
  if (m_setters.size() == 1) m_setters[0]->selectKeyframe(kIndex);
}

//-----------------------------------------------------------------------------

void MovePointDragTool::createKeyframe(double frame) {
  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    int kIndex             = setter->createKeyframe(tround(frame));
    setter->selectKeyframe(kIndex);
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::selectKeyframes(double frame) {
  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();
    setter->setPixelRatio(m_panel->getPixelRatio(curve));
    int kIndex = curve->getClosestKeyframe(frame);
    if (kIndex >= 0) {
      double kf = curve->keyframeIndexToFrame(kIndex);
      if (fabs(kf - frame) < 0.01) setter->selectKeyframe(kIndex);
    }
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::setSelection(FunctionSelection *selection) {
  if (selection) {
    assert(m_setters.size() == 1);
    if (m_setters.size() == 1) {
      TDoubleParam *curve = m_setters[0]->getCurve();
      assert(curve);
      if (curve) {
        m_selection = selection;
        for (int i = 0; i < curve->getKeyframeCount(); i++)
          if (selection->isSelected(curve, i)) addKeyframe2(i);
      }
    }
  } else
    m_selection = selection;
}

//-----------------------------------------------------------------------------

void MovePointDragTool::click(QMouseEvent *e) {
  m_startPos = m_oldPos = e->pos();
  m_deltaFrame          = 0;
  double frame          = m_panel->xToFrame(e->pos().x());

  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();
    setter->setPixelRatio(m_panel->getPixelRatio(curve));
    if (!m_groupEnabled) {
      int kIndex = curve->getClosestKeyframe(frame);
      if (kIndex >= 0) {
        double kf = curve->keyframeIndexToFrame(kIndex);
        if (fabs(kf - frame) < 1) setter->selectKeyframe(kIndex);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::drag(QMouseEvent *e) {
  QPoint pos = e->pos();

  // if shift is pressed then apply constraints (only horizontal or vertical
  // moves)
  if (e->modifiers() & Qt::ShiftModifier) {
    QPoint delta = e->pos() - m_startPos;
    if (abs(delta.x()) > abs(delta.y()))
      pos.setY(m_startPos.y());
    else
      pos.setX(m_startPos.x());
  }

  if (m_groupEnabled) pos.setY(m_startPos.y());

  QPoint oldPos = m_oldPos;
  m_oldPos      = pos;

  // compute frame increment. it must be an integer
  double totalDFrame =
      tround(m_panel->xToFrame(pos.x()) - m_panel->xToFrame(m_startPos.x()));
  double dFrame = totalDFrame - m_deltaFrame;
  m_deltaFrame  = totalDFrame;

  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();

    // compute value increment
    double dValue = m_panel->yToValue(curve, pos.y()) -
                    m_panel->yToValue(curve, oldPos.y());

    setter->moveKeyframes(dFrame, dValue);
  }
  if (m_selection != 0 && m_setters.size() == 1) {
    KeyframeSetter *setter = m_setters[0];

    m_selection->deselectAllKeyframes();
    for (int i = 0; i < setter->getCurve()->getKeyframeCount(); i++)
      if (setter->isSelected(i)) m_selection->select(setter->getCurve(), i);
  }

  m_panel->update();
}

//-----------------------------------------------------------------------------

void MovePointDragTool::release(QMouseEvent *e) {}

//=============================================================================

MoveHandleDragTool::MoveHandleDragTool(FunctionPanel *panel,
                                       TDoubleParam *curve, int kIndex,
                                       Handle handle)
    : m_panel(panel)
    , m_curve(curve)
    , m_kIndex(kIndex)
    , m_handle(handle)
    , m_deltaFrame(0)
    , m_setter(curve, kIndex)
    , m_segmentWidth(0)
    , m_channelGroup(0) {}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::click(QMouseEvent *e) {
  m_startPos = e->pos();
  // m_startPos = m_oldPos = e->pos();
  m_deltaFrame       = 0;
  m_keyframe         = m_curve->getKeyframe(m_kIndex);
  m_keyframe.m_value = m_curve->getValue(m_keyframe.m_frame);
  if (m_handle == FunctionPanel::SpeedIn)
    m_keyframe.m_value = m_curve->getValue(m_keyframe.m_frame, true);

  if (m_channelGroup) {
    for (int i = 0; i < m_channelGroup->getChildCount(); i++) {
      TreeModel::Item *child = m_channelGroup->getChild(i);
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(child);
      if (channel && m_curve != channel->getParam()) {
        if (channel->getParam()->isKeyframe(m_keyframe.m_frame)) {
        }
      }
    }
  }

  if (m_handle == FunctionPanel::EaseInPercentage && m_kIndex > 0) {
    double previousFrame = m_curve->keyframeIndexToFrame(m_kIndex - 1);
    m_segmentWidth       = m_keyframe.m_frame - previousFrame;
  }
  if (m_handle == FunctionPanel::EaseOutPercentage &&
      m_kIndex + 1 < m_curve->getKeyframeCount()) {
    double nextFrame = m_curve->keyframeIndexToFrame(m_kIndex + 1);
    m_segmentWidth   = nextFrame - m_keyframe.m_frame;
  }
  TPointD speed;

  if (m_keyframe.m_linkedHandles) {
    if (m_handle == FunctionPanel::SpeedIn &&
        m_kIndex + 1 < m_curve->getKeyframeCount() &&
        (m_keyframe.m_type != TDoubleKeyframe::SpeedInOut &&
         (m_keyframe.m_type != TDoubleKeyframe::Expression ||
          m_keyframe.m_expressionText.find("cycle") == std::string::npos)))
      speed = m_curve->getSpeedIn(m_kIndex);
    else if (m_handle == FunctionPanel::SpeedOut &&
             m_keyframe.m_prevType != TDoubleKeyframe::SpeedInOut &&
             m_kIndex > 0)
      speed = m_curve->getSpeedOut(m_kIndex);
  }
  if (norm2(speed) > 0.001) {
    QPointF a = m_panel->getWinPos(m_curve, speed) -
                m_panel->getWinPos(m_curve, TPointD());
    double aa = 1.0 / sqrt(a.x() * a.x() + a.y() * a.y());
    m_nSpeed  = QPointF(-a.y() * aa, a.x() * aa);
  } else
    m_nSpeed = QPointF();
  m_setter.setPixelRatio(m_panel->getPixelRatio(m_curve));
}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::drag(QMouseEvent *e) {
  if (!m_curve) return;
  QPoint pos = e->pos();

  // if shift is pressed then apply constraints (only horizontal or vertical
  // moves)
  if (e->modifiers() & Qt::ShiftModifier) {
    QPoint delta = e->pos() - m_startPos;
    if (abs(delta.x()) > abs(delta.y()))
      pos.setY(m_startPos.y());
    else
      pos.setX(m_startPos.x());
  }

  // QPoint oldPos = m_oldPos;
  // m_oldPos = pos;

  QPointF p0 = m_panel->getWinPos(m_curve, m_keyframe);
  QPointF posF(pos);

  if (m_nSpeed != QPointF(0, 0)) {
    QPointF delta = posF - p0;
    posF -= m_nSpeed * (delta.x() * m_nSpeed.x() + delta.y() * m_nSpeed.y());
    if ((m_handle == FunctionPanel::SpeedIn && posF.x() > p0.x()) ||
        (m_handle == FunctionPanel::SpeedOut && posF.x() < p0.x()))
      posF = p0;
  } else {
    if ((m_handle == FunctionPanel::SpeedIn && posF.x() > p0.x()) ||
        (m_handle == FunctionPanel::SpeedOut && posF.x() < p0.x()))
      posF.setX(p0.x());
  }

  double frame = m_panel->xToFrame(posF.x());
  double value = m_panel->yToValue(m_curve, posF.y());
  TPointD handlePos(frame - m_keyframe.m_frame, value - m_keyframe.m_value);
  switch (m_handle) {
  case FunctionPanel::SpeedIn:
    if (m_keyframe.m_type != TDoubleKeyframe::SpeedInOut) {
    }
    m_setter.setSpeedIn(handlePos);
    break;
  case FunctionPanel::SpeedOut:
    m_setter.setSpeedOut(handlePos);
    break;
  case FunctionPanel::EaseIn:
    m_setter.setEaseIn(handlePos.x);
    break;
  case FunctionPanel::EaseOut:
    m_setter.setEaseOut(handlePos.x);
    break;
  case FunctionPanel::EaseInPercentage:
    if (m_segmentWidth > 0)
      m_setter.setEaseIn(100.0 * handlePos.x / m_segmentWidth);
    break;
  case FunctionPanel::EaseOutPercentage:
    if (m_segmentWidth > 0)
      m_setter.setEaseOut(100.0 * handlePos.x / m_segmentWidth);
    break;
  case 100:
  case 101:
  case 102:
    break;
  default:
    assert(0);
  }
  m_panel->update();
}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::release(QMouseEvent *e) {}

//=============================================================================

//-----------------------------------------------------------------------------

MoveGroupHandleDragTool::MoveGroupHandleDragTool(FunctionPanel *panel,
                                                 double keyframePosition,
                                                 Handle handle)
    : m_panel(panel), m_keyframePosition(keyframePosition), m_handle(handle) {
  TUndoManager::manager()->beginBlock();
}

//-----------------------------------------------------------------------------

MoveGroupHandleDragTool::~MoveGroupHandleDragTool() {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::click(QMouseEvent *e) {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();

  FunctionTreeModel *model = m_panel->getModel();
  for (int i = 0; i < model->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel = model->getActiveChannel(i);
    if (channel && channel->getParam()) {
      TDoubleParam *curve    = channel->getParam();
      int kIndex             = curve->getClosestKeyframe(m_keyframePosition);
      KeyframeSetter *setter = new KeyframeSetter(curve, kIndex);
      setter->setPixelRatio(m_panel->getPixelRatio(curve));
      TDoubleKeyframe kf = curve->getKeyframe(kIndex);
      m_setters.push_back(std::make_pair(kf, setter));
    }
  }
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::drag(QMouseEvent *e) {
  //  if(!m_curve) return;
  QPoint pos = e->pos();
  QPointF posF(pos);

  /*
if(m_nSpeed != QPointF(0,0))
{
QPointF delta = posF-p0;
posF -= m_nSpeed*(delta.x()*m_nSpeed.x()+delta.y()*m_nSpeed.y());
if(  m_handle == FunctionPanel::SpeedIn && posF.x()>p0.x()
|| m_handle == FunctionPanel::SpeedOut && posF.x()<p0.x())
posF = p0;
}
else
{
if(  m_handle == FunctionPanel::SpeedIn && posF.x()>p0.x()
|| m_handle == FunctionPanel::SpeedOut && posF.x()<p0.x())
posF.setX(p0.x());
}
*/

  double frame = m_panel->xToFrame(posF.x());

  for (int i = 0; i < (int)m_setters.size(); i++) {
    TDoubleKeyframe kf     = m_setters[i].first;
    KeyframeSetter *setter = m_setters[i].second;

    if (m_handle == 101)  // why the magic numbers... use enums!
    {
      kf.m_speedOut.x = frame - kf.m_frame;

      switch (kf.m_type) {
      case TDoubleKeyframe::SpeedInOut:
        setter->setSpeedOut(kf.m_speedOut);
        break;
      case TDoubleKeyframe::EaseInOut:
        setter->setEaseOut(kf.m_speedOut.x);
        break;
      default:
        assert(false);
      }
    } else if (m_handle == 102)  // aagghhrrr
    {
      kf.m_speedIn.x = frame - kf.m_frame;

      switch (kf.m_prevType) {
      case TDoubleKeyframe::SpeedInOut:
        setter->setSpeedIn(kf.m_speedIn);
        break;
      case TDoubleKeyframe::EaseInOut:
        setter->setEaseIn(kf.m_speedIn.x);
        break;
      default:
        assert(false);
      }
    }
  }

  /*
switch(m_handle)
{


case FunctionPanel::SpeedIn:
if(m_keyframe.m_type != TDoubleKeyframe::SpeedInOut)
{

}
m_setter.setSpeedIn(handlePos);
break;
case FunctionPanel::SpeedOut:m_setter.setSpeedOut(handlePos); break;
case FunctionPanel::EaseIn:m_setter.setEaseIn(handlePos.x); break;
case FunctionPanel::EaseOut:m_setter.setEaseOut(handlePos.x); break;
case FunctionPanel::EaseInPercentage:
if(m_segmentWidth>0)
m_setter.setEaseIn(100.0*handlePos.x/m_segmentWidth);
break;
case FunctionPanel::EaseOutPercentage:
if(m_segmentWidth>0)
m_setter.setEaseOut(100.0*handlePos.x/m_segmentWidth);
break;
case 100:
case 101:
case 102:
break;
default:assert(0);
}
*/
  m_panel->update();
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::release(QMouseEvent *e) {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();
}
