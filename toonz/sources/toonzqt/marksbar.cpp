

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

#include "toonzqt/marksbar.h"

//*****************************************************************************
//    Local namespace stuff
//*****************************************************************************

namespace {

void crop(QVector<int> &values, int min, int max) {
  QVector<int>::iterator it, end = values.end();
  for (it = values.begin(); it != end; ++it) *it = tcrop(*it, min, max);
}

//-----------------------------------------------------------------------------

void rollDown(QVector<int> &values, int max, int sortDist) {
  assert(!values.empty());
  values.back() = std::min(max, values.back());

  int val;

  QVector<int>::iterator it, jt, beg = values.begin();
  for (it = values.end(), jt = --it; jt != beg; jt = it) {
    --it;
    val                = *jt - sortDist;
    if (*it > val) *it = val;
  }
}

//-----------------------------------------------------------------------------

void rollUp(QVector<int> &values, int min, int sortDist) {
  assert(!values.empty());
  values.front() = std::max(min, values.front());

  int val;

  QVector<int>::iterator it, jt, end = values.end();
  for (jt = values.begin(), it = jt++; jt != end; it = jt, ++jt) {
    val                = *it + sortDist;
    if (val > *jt) *jt = val;
  }
}
}

//*****************************************************************************
//    MarksBar implementation
//*****************************************************************************

MarksBar::MarksBar(QWidget *parent, bool markUp)
    : QFrame(parent)
    , m_markUp(markUp)
    , m_min(0)
    , m_max(100)
    , m_sortDist(-1)
    , m_selectedMark(-1) {
  setMinimumWidth(100);
  setFixedHeight(6);
}

//-----------------------------------------------------------------------------

void MarksBar::setRange(int min, int max, int sortDist) {
  assert(min <= max);
  m_min = min, m_max = max, m_sortDist = sortDist;

  conformValues();
  update();
}

//-----------------------------------------------------------------------------

int MarksBar::valToPos(int val) {
  const QRect contsRect = contentsRect();
  return contsRect.left() +
         contsRect.width() * ((val - m_min) / (double)(m_max - m_min));
}

//-----------------------------------------------------------------------------

int MarksBar::posToVal(int pos) {
  const QRect contsRect = contentsRect();
  return m_min +
         (m_max - m_min) *
             ((pos - contsRect.left()) / (double)contsRect.width());
}

//-----------------------------------------------------------------------------

void MarksBar::conformValues(bool preferRollDown) {
  if (m_values.empty()) return;

  if (m_sortDist < 0)
    ::crop(m_values, m_min, m_max);
  else {
    if (preferRollDown) {
      ::rollDown(m_values, m_max, m_sortDist);
      ::rollUp(m_values, m_min, m_sortDist);
    } else {
      ::rollUp(m_values, m_min, m_sortDist);
      ::rollDown(m_values, m_max, m_sortDist);
    }
  }

  update();
  emit marksUpdated();
}

//-----------------------------------------------------------------------------

void MarksBar::mousePressEvent(QMouseEvent *me) {
  int newDist, currDist = (std::numeric_limits<int>::max)();

  int val = posToVal(me->x());

  // Select the nearest mark. If the distance is too great, select none.
  int i, size = m_values.size();
  for (i = 0; i < size; ++i) {
    newDist = abs(m_values[i] - val);
    if (newDist < 7 && newDist < currDist) m_selectedMark = i;
  }

  update();
}

//-----------------------------------------------------------------------------

void MarksBar::mouseMoveEvent(QMouseEvent *me) {
  if (m_selectedMark < 0) return;

  int newVal = tcrop(posToVal(me->x()), m_min, m_max);
  bool left  = newVal < m_values[m_selectedMark];

  m_values[m_selectedMark] = newVal;
  conformValues(left);

  update();
}

//-----------------------------------------------------------------------------

void MarksBar::mouseReleaseEvent(QMouseEvent *me) {
  m_selectedMark = -1;
  emit marksReleased();
}

//-----------------------------------------------------------------------------

void MarksBar::drawMark(QPainter &p, int pos, const QColor &color) {
  QPolygon poly(3);
  if (m_markUp) {
    poly[0] = QPoint(pos - 5, 5);
    poly[1] = QPoint(pos + 5, 5);
    poly[2] = QPoint(pos, 0);
  } else {
    poly[0] = QPoint(pos - 5, 0);
    poly[1] = QPoint(pos + 5, 0);
    poly[2] = QPoint(pos, 5);
  }

  p.setBrush(color);
  p.drawPolygon(poly);
}

//-----------------------------------------------------------------------------

void MarksBar::paintEvent(QPaintEvent *pe) {
  QPainter p(this);

  assert(m_values.size() == m_colors.size());

  // Draw markers
  int i, size = m_values.size();
  for (i = 0; i < size; ++i) drawMark(p, valToPos(m_values[i]), m_colors[i]);
}
