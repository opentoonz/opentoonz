

#include "toonzqt/tonecurvefield.h"
#include "toonzqt/fxhistogramrender.h"
#include "toonzqt/intpairfield.h"
#include "toonzqt/checkbox.h"

#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

using namespace DVGui;

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

double getPercentAtPoint(QPointF point, QPainterPath path) {
  int i;
  for (i = 1; i < 100; i++) {
    double p          = double(i) * 0.01;
    QPointF pathPoint = path.pointAtPercent(p);
    if (abs(pathPoint.x() - point.x()) < 3 &&
        abs(pathPoint.y() - point.y()) < 3)
      return p;
  }
  return 0;
}

//-----------------------------------------------------------------------------

QList<QPointF> getIntersectedPoint(QRectF rect, QPainterPath path) {
  int y0 = rect.top();
  int y1 = rect.bottom();
  QList<QPointF> points;
  double g      = 1.0 / 256.0;
  QPointF prec  = path.pointAtPercent(0);
  QPointF point = path.pointAtPercent(g);
  int j         = 0;
  for (j = 2; j < 256; j++) {
    QPointF next = path.pointAtPercent(double(j) * g);
    if (prec.y() >= y0 && prec.y() <= y1) {
      if (point.y() < y0)
        points.push_back(QPointF(prec.x(), y0));
      else if (point.y() > y1)
        points.push_back(QPointF(prec.x(), y1));
    }
    if (next.y() >= y0 && next.y() <= y1) {
      if (point.y() < y0)
        points.push_back(QPointF(next.x(), y0));
      else if (point.y() > y1)
        points.push_back(QPointF(next.x(), y1));
    }
    prec  = point;
    point = next;
  }
  return points;
}

//-----------------------------------------------------------------------------

double qtNorm2(const QPointF &p) { return p.x() * p.x() + p.y() * p.y(); }

double qtNorm(const QPointF &p) { return sqrt(qtNorm2(p)); }

QPointF qtNormalize(const QPointF &p) {
  double n = qtNorm(p);
  assert(n != 0.0);
  return (1.0 / n) * p;
}

double qtDistance2(const QPointF &p1, const QPointF &p2) {
  return qtNorm2(p2 - p1);
}

double qtDistance(const QPointF &p1, const QPointF &p2) {
  return qtNorm(p2 - p1);
}

//-----------------------------------------------------------------------------

QPointF getNewFirstHandlePoint(const QPointF &p, const QPointF &nextP,
                               const QPointF &oldHandlePoint) {
  bool canMove  = (nextP.x() - p.x() > 16);
  int yDistance = nextP.y() - p.y();
  double sign   = (yDistance != 0) ? yDistance / abs(yDistance) : 1;
  double t      = qtDistance(p, oldHandlePoint);
  if (!canMove) {
    QPointF normalizedP =
        (yDistance != 0) ? qtNormalize(nextP - p) : QPoint(1, 1);
    return QPointF(p + QPointF(t * normalizedP));
  } else if (abs(oldHandlePoint.x() - p.x()) < 16)
    return QPointF(p.x() + 16, p.y() + sign * sqrt(t * t - 16 * 16));
  return oldHandlePoint;
}

//-----------------------------------------------------------------------------

QPointF getNewSecondHandlePoint(const QPointF &p, const QPointF &nextP,
                                const QPointF &oldHandlePoint) {
  bool canMove  = (nextP.x() - p.x() > 16);
  int yDistance = p.y() - nextP.y();
  double sign   = (yDistance != 0) ? yDistance / abs(yDistance) : 1;
  double s      = qtDistance(oldHandlePoint, nextP);
  if (!canMove) {
    QPointF normalizedP =
        (yDistance != 0) ? qtNormalize(nextP - p) : QPoint(1, 1);
    return QPointF(nextP - QPointF(s * normalizedP));
  } else if (abs(nextP.x() - oldHandlePoint.x()) < 16)
    return QPointF(nextP.x() - 16, nextP.y() + sign * sqrt(s * s - 16 * 16));
  return oldHandlePoint;
}

//-----------------------------------------------------------------------------
}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================
// ChennelCurveEditor
//=============================================================================

ChennelCurveEditor::ChennelCurveEditor(QWidget *parent,
                                       HistogramView *histogramView)
    : QWidget(parent)
    , m_histogramView(histogramView)
    , m_currentControlPointIndex(-1)
    , m_mouseButton(Qt::NoButton)
    , m_curveHeight(256)
    , m_LeftRightMargin(42)
    , m_TopMargin(9)
    , m_BottomMargin(48)
    , m_isLinear(false) {
  setFixedSize(m_curveHeight + 2 * m_LeftRightMargin + 2,
               m_curveHeight + m_TopMargin + m_BottomMargin);
  setAttribute(Qt::WA_KeyCompression);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);

  m_histogramView->setDrawnWidget(this);
  m_histogramView->setGraphHeight(m_curveHeight);
  m_histogramView->setGraphAlphaMask(120);
  m_verticalChannelBar =
      new ChannelBar(0, m_histogramView->getChannelBarColor(), false);
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::setPoints(QList<TPointD> points) {
  if (!m_points.isEmpty()) m_points.clear();
  int i;
  for (i = 0; i < points.size(); i++) {
    QPointF p = strokeToViewPoint(points.at(i));
    m_points.push_back(p);
  }
  /*--ポイント位置に合わせてスライダも更新する--*/
  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  emit firstLastXPostionChanged(viewToStrokePoint(m_points.at(firstIndex)).x,
                                viewToStrokePoint(m_points.at(lastIndex)).x);
  update();
}

//-----------------------------------------------------------------------------

QList<TPointD> ChennelCurveEditor::getPoints() {
  QList<TPointD> points;
  if (m_points.isEmpty()) return points;
  int i;
  for (i = 0; i < m_points.size(); i++)
    points.push_back(viewToStrokePoint(m_points.at(i)));
  return points;
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::setFirstLastXPosition(std::pair<int, int> values,
                                               bool isDragging) {
  if (!isDragging) {
    emit controlPointChanged(false);
    return;
  }

  QPointF newX0 = strokeToViewPoint(TPointD(values.first, 0));
  QPointF newX1 = strokeToViewPoint(TPointD(values.second, 0));

  int indexX0 = 3;
  int indexX1 = m_points.size() - 4;
  QPointF x0  = m_points.at(indexX0);
  QPointF x1  = m_points.at(indexX1);
  if (x0.x() != newX0.x()) {
    QPointF delta(newX0.x() - x0.x(), 0);
    moveCentralControlPoint(indexX0, delta);
    update();
  }
  if (x1.x() != newX1.x()) {
    QPointF delta(newX1.x() - x1.x(), 0);
    moveCentralControlPoint(indexX1, delta);
    update();
  }
  m_currentControlPointIndex = -1;
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::setLinear(bool isLinear) {
  if (m_isLinear == isLinear) return;
  m_isLinear = isLinear;
  update();
}

//-----------------------------------------------------------------------------

QPointF ChennelCurveEditor::checkPoint(const QPointF p) {
  QPointF checkedP         = p;
  int y0                   = m_TopMargin + 1;
  int y1                   = m_curveHeight + m_TopMargin;
  if (p.y() < y0) checkedP = QPointF(checkedP.x(), y0);
  if (p.y() > y1) checkedP = QPointF(checkedP.x(), y1);
  int x0                   = m_LeftRightMargin + 1;
  int x1                   = m_curveHeight + m_LeftRightMargin;
  if (p.x() < x0) checkedP = QPointF(x0, checkedP.y());
  if (p.x() > x1) checkedP = QPointF(x1, checkedP.y());
  return checkedP;
}

//-----------------------------------------------------------------------------

QPointF ChennelCurveEditor::strokeToViewPoint(const TPointD p) {
  double x = p.x + m_LeftRightMargin + 1;
  double y = height() - m_BottomMargin - p.y;
  return QPointF(x, y);
}

//-----------------------------------------------------------------------------

TPointD ChennelCurveEditor::viewToStrokePoint(const QPointF &p) {
  double x = p.x() - m_LeftRightMargin - 1;
  double y = m_curveHeight - (p.y() - m_TopMargin);
  return TThickPoint(x, y);
}

//-----------------------------------------------------------------------------

int ChennelCurveEditor::getClosestPointIndex(const QPointF &pos,
                                             double &minDistance2) const {
  int closestPointIndex = -1;
  minDistance2          = 0;
  int i;
  for (i = 0; i < (int)m_points.size(); i++) {
    double distance2 = qtDistance2(pos, m_points.at(i));
    if (closestPointIndex < 0 || distance2 < minDistance2) {
      minDistance2      = distance2;
      closestPointIndex = i;
    }
  }
  return closestPointIndex;
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::movePoint(int index, const QPointF delta) {
  QPointF p = m_points.at(index);
  p += delta;
  setPoint(index, p);

  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  if (index == firstIndex)
    emit firstLastXPostionChanged(viewToStrokePoint(p).x,
                                  viewToStrokePoint(m_points.at(lastIndex)).x);
  if (index == lastIndex)
    emit firstLastXPostionChanged(viewToStrokePoint(m_points.at(firstIndex)).x,
                                  viewToStrokePoint(p).x);
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::setPoint(int index, const QPointF p) {
  m_points.removeAt(index);
  m_points.insert(index, p);

  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  if (index == firstIndex)
    emit firstLastXPostionChanged(viewToStrokePoint(p).x,
                                  viewToStrokePoint(m_points.at(lastIndex)).x);
  if (index == lastIndex)
    emit firstLastXPostionChanged(viewToStrokePoint(m_points.at(firstIndex)).x,
                                  viewToStrokePoint(p).x);
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::moveCurrentControlPoint(const QPointF delta) {
  assert(m_currentControlPointIndex != -1);
  int pointCount = m_points.size();
  /*- セグメントを動かした場合 -*/
  if (isCentralControlPoint(m_currentControlPointIndex))
    moveCentralControlPoint(m_currentControlPointIndex, delta);
  /*- 左のハンドルを動かした場合 -*/
  else if (isLeftControlPoint(m_currentControlPointIndex)) {
    QPointF p0 =
        m_points.at(m_currentControlPointIndex) + QPointF(0, delta.y());
    setPoint(m_currentControlPointIndex, p0);
    if (m_currentControlPointIndex < pointCount - 5) {
      QPointF p2 =
          m_points.at(m_currentControlPointIndex + 2) - QPointF(0, delta.y());
      setPoint(m_currentControlPointIndex + 2, p2);
    }
    emit controlPointChanged(true);
  }
  /*- 右のハンドルを動かした場合 -*/
  else {
    assert(isRightControlPoint(m_currentControlPointIndex));
    QPointF p0 =
        m_points.at(m_currentControlPointIndex) + QPointF(0, delta.y());
    setPoint(m_currentControlPointIndex, p0);
    if (m_currentControlPointIndex > 4) {
      QPointF p2 =
          m_points.at(m_currentControlPointIndex - 2) - QPointF(0, delta.y());
      setPoint(m_currentControlPointIndex - 2, p2);
    }
    emit controlPointChanged(true);
  }
  update();
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::moveCentralControlPoint(int index,
                                                 const QPointF delta) {
  int pointCount = m_points.size();

  assert(index < pointCount - 3 && index > 2);

  QPointF p = m_points.at(index);
  QPointF d = delta;
  // Trovo il valore di delta im modo tale che il punto di controllo non sia
  // trascinato fuori dal range consentito
  int newX         = p.x() + delta.x();
  int newY         = p.y() + delta.y();
  QPointF newPoint = checkPoint(QPoint(newX, newY));
  d                = newPoint - p;

  QPointF nextP       = m_points.at(index + 3);
  QPointF precP       = m_points.at(index - 3);
  double nextDistance = nextP.x() - (p.x() + d.x());
  double precDistance = (p.x() + d.x()) - precP.x();

  // Caso particolare: Punto di controllo corrente == primo visibile,
  //								  Punto di controllo
  //successivo
  //==
  // l'ultimo
  // visibile
  if (index == 3 && index + 3 == pointCount - 4) {
    setPoint(index + 1,
             getNewFirstHandlePoint(p, nextP, m_points.at(index + 1)));
    setPoint(index + 2,
             getNewSecondHandlePoint(p, nextP, m_points.at(index + 2)));
    if (nextDistance < 0) d = QPointF(nextP.x() - p.x(), d.y());
  }
  // Caso particolare: Punto di controllo corrente == ultimo visibile,
  //								  Punto di controllo
  //precedente
  //==
  // primo
  // visibile
  else if (index - 3 == 3 && index == pointCount - 4) {
    setPoint(index - 2,
             getNewFirstHandlePoint(precP, p, m_points.at(index - 2)));
    setPoint(index - 1,
             getNewSecondHandlePoint(precP, p, m_points.at(index - 1)));
    if (precDistance < 0) d = QPointF(precP.x() - p.x(), d.y());
  }
  // Altrimenti calcolo il nuovo delta
  else if (nextDistance < 16)
    d = QPointF(nextP.x() - p.x() - 16, d.y());
  else if (precDistance < 16)
    d = QPointF(precP.x() - p.x() + 16, d.y());

  // Punto di controllo speciale: il primo visualizzato.
  if (index == 3) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index - 1, dY);
    movePoint(index - 2, dY);
    movePoint(index - 3, dY);
  }
  // Punto di controllo speciale: l'ultimo visualizzato.
  if (index == pointCount - 4) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index + 1, dY);
    movePoint(index + 2, dY);
    movePoint(index + 3, dY);
  }
  if (index > 3) movePoint(index - 1, d);
  if (index < pointCount - 4) movePoint(index + 1, d);
  movePoint(index, d);
  emit controlPointChanged(true);
}

//-----------------------------------------------------------------------------

/* Se due punti di controllo sono troppo vicini uno dei due viene eliminato.
         Ritorna vero se un punto viene eliminato.

Prima di richiamare il metodo nel move andava fatto questo controllo!!!
//Se il punto di controllo successivo, o precente, e' l'utlimo, o il primo,
blocco il
// movimento altrimenti elimino il punto di controllo successivo e richiamo il
move.
int nextPX = m_points.at(index+3).x();
if(nextPX>m_margin && nextPX<=w && newX>nextPX)
{
        if(index+3 == pointCount-4) d = QPointF(0, d.y());
        else
        {
                moveCentralControlPoint(index, d);
                return;
        }
}
int precPX = m_points.at(index-3).x();
if(precPX>=m_margin && precPX<=w && newX<precPX)
{
        if(index-3 == 3) d = QPointF(0, d.y());
        else
        {
                moveCentralControlPoint(index, d);
                return;
        }
}

bool ChennelCurveEditor::eraseControlPointWhileMove(int index, const QPointF
delta)
{
        int pointCount = m_points.size();
        QPointF p = m_points.at(index);
        QPointF nextP = m_points.at(index+3);
        QPointF precP = m_points.at(index-3);
        double nextDistance = abs((p.x()+delta.x())-nextP.x());
        double precDistance = abs((p.x()+delta.x())-precP.x());

        if(nextDistance>16 && precDistance>16) return false;

        //Se vado troppo vicino al punto di controllo precedente, o successivo,
lo elimino.
        if(nextDistance<=16)
        {
                //Caso particolare: il successivo e' l'ultimo visibile; non
posso eliminare l'ultimo punto di controllo visibile.
                if(index+3 == pointCount-4)
                {
                        //Se il punto di controllo in index e' il primo visibile
sto gestendo il
                        // primo e l'ultimo punto, entrambi non possono essere
eliminati.
                        if(index == 3)
                        {
                                setPoint(index+1,nextP);
                                setPoint(index+2,p+delta);
                                return false;
                        }
                        else	//Altrimenti elimino il penultimo.
                        {
                                removeControlPoint(index);
                                m_currentControlPointIndex += 3;
                                return true;
                        }
                }
                removeControlPoint(index+3);
                return true;
        }
        if(precDistance<=16)
        {
                //Caso particolare: il precedente e' il primo visibile; non
posso eliminare il primo punto di controllo visibile.
                if(index-3 == 3)
                {
                        //Se il punto di controllo in index e' l'ultimo visibile
sto gestendo il
                        // primo e l'ultimo punto, entrambi non possono essere
eliminati.
                        if(index == pointCount-4)
                        {
                                setPoint(index-1,precP);
                                setPoint(index-2,p+delta);
                                return false;
                        }
                        else	//Altrimenti elimino il secondo.
                        {
                                removeControlPoint(index);
                                return true;
                        }
                }
                removeControlPoint(index-3);
                m_currentControlPointIndex += 3;
                return true;
        }
        return false;
}
*/

//-----------------------------------------------------------------------------

void ChennelCurveEditor::addControlPoint(double percent) {
  QPainterPath path = getPainterPath();
  QPointF p         = path.pointAtPercent(percent);

  // Cerco il punto di controllo precedente
  int pointCount = m_points.size();
  int beforeControlPointIndex;
  for (beforeControlPointIndex = pointCount - 1; beforeControlPointIndex >= 0;
       beforeControlPointIndex--) {
    QPointF point = m_points.at(beforeControlPointIndex);
    if (isCentralControlPoint(beforeControlPointIndex) && point.x() < p.x())
      break;
  }

  if (beforeControlPointIndex == 0 || beforeControlPointIndex == pointCount - 4)
    return;

  QPointF p0 = checkPoint(m_points.at(beforeControlPointIndex));
  // Se sono troppo vicino al punto di controllo precedente ritorno
  if (abs(p.x() - p0.x()) <= 16) return;
  double beforeControlPointPercent = getPercentAtPoint(p0, path);
  QPointF p1 = checkPoint(m_points.at(beforeControlPointIndex + 1));
  QPointF p2 = checkPoint(m_points.at(beforeControlPointIndex + 2));
  QPointF p3 = checkPoint(m_points.at(beforeControlPointIndex + 3));
  // Se sono troppo vicino al punto di controllo successivo ritorno
  if (abs(p3.x() - p.x()) <= 16) return;
  double nextControlPointPercent = getPercentAtPoint(p3, path);

  // Calcolo la velocita' e quindi il coiffciente angolare.
  double t =
      percent * 100 / (nextControlPointPercent - beforeControlPointPercent);
  double s = t - 1;
  QPointF speed =
      3.0 * ((p1 - p0) * s * s + 2 * (p2 - p0) * s * t + (p3 - p2) * t * t);
  double m = speed.y() / speed.x();

  int newControlPointIndex = beforeControlPointIndex + 3;
  m_points.insert(newControlPointIndex - 1,
                  QPointF(p.x() - 16, p.y() - 16 * m));
  m_points.insert(newControlPointIndex, p);
  m_points.insert(newControlPointIndex + 1,
                  QPointF(p.x() + 16, p.y() + 16 * m));

  m_currentControlPointIndex = newControlPointIndex;
  emit controlPointAdded(newControlPointIndex);
  update();
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::removeCurrentControlPoint() {
  removeControlPoint(m_currentControlPointIndex);
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::removeControlPoint(int index) {
  // Non posso eliminare il primo punto di controllo visibile quindi lo rimetto
  // in condizione iniziale
  if (index <= 4) {
    setPoint(0, strokeToViewPoint(TPointD(-40, 0)));
    setPoint(1, strokeToViewPoint(TPointD(-20, 0)));
    setPoint(2, strokeToViewPoint(TPointD(-20, 0)));
    setPoint(3, strokeToViewPoint(TPointD(0, 0)));
    setPoint(4, strokeToViewPoint(TPointD(16, 16)));
    update();
    emit controlPointChanged(false);
    return;
  }
  // Non posso eliminare il l'ultimo punto di controllo visibile quindi lo
  // rimetto in condizione iniziale
  if (index >= m_points.size() - 5) {
    int i = m_points.size() - 5;
    setPoint(i, strokeToViewPoint(TPointD(239, 239)));
    setPoint(i + 1, strokeToViewPoint(TPointD(255, 255)));
    setPoint(i + 2, strokeToViewPoint(TPointD(275, 255)));
    setPoint(i + 3, strokeToViewPoint(TPointD(275, 255)));
    setPoint(i + 4, strokeToViewPoint(TPointD(295, 255)));
    update();
    emit controlPointChanged(false);
    return;
  }

  int firstIndex = 0;
  if (isCentralControlPoint(index))
    firstIndex = index - 1;
  else if (isLeftControlPoint(index))
    firstIndex = index;
  else
    firstIndex = index - 2;

  m_points.removeAt(firstIndex);
  m_points.removeAt(firstIndex);
  m_points.removeAt(firstIndex);

  emit controlPointRemoved(firstIndex + 1);
  m_currentControlPointIndex = firstIndex - 2;
  update();
}

//-----------------------------------------------------------------------------

QPainterPath ChennelCurveEditor::getPainterPath() {
  int pointCount = m_points.size();
  if (pointCount == 0) return QPainterPath();

  QPointF p0 = m_points.at(0);
  QPainterPath path(p0);
  int i;
  for (i = 1; i < pointCount; i++) {
    QPointF p1 = m_points.at(i);
    QPointF p2 = m_points.at(++i);
    QPointF p3 = m_points.at(++i);
    path.moveTo(p0);
    if (!m_isLinear)
      path.cubicTo(p1, p2, p3);
    else
      path.lineTo(p3);
    p0 = p3;
  }

  // Cerco le eventuali intersezioni con il bordo.
  QRectF rect(m_LeftRightMargin, m_TopMargin, m_curveHeight, m_curveHeight);
  QRectF r = path.boundingRect();
  if (!rect.contains(QRect(rect.left(), r.top(), rect.width(), r.height()))) {
    QList<QPointF> points = getIntersectedPoint(rect, path);
    // Se trovo punti di intersezione (per come e' definita la curva devono
    // essere pari)
    // faccio l'unione del path calcolato e di nuovi path lineari.
    int j = 0;
    for (j = 0; j < points.size(); j++) {
      QPointF p0 = points.at(j);
      QPointF p1 = points.at(++j);
      QPainterPath line(p0);
      line.lineTo(p1);
      path.addPath(line);
    }
  }

  return path;
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::paintEvent(QPaintEvent *e) {
  QPainter painter(this);

  // Disegno il reticolato
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QColor(250, 250, 250));
  int i;
  int d = m_curveHeight * 0.25;
  for (i = 1; i < 16; i++) {
    // linee orizzontali
    int delta = m_TopMargin + 16 * i;
    int j;
    for (j = 1; j < 4; j++)
      painter.drawLine(QPoint((j - 1) * d + m_LeftRightMargin + 1, delta),
                       QPoint(j * d + m_LeftRightMargin - 1, delta));
    painter.drawLine(QPoint((4 - 1) * d + m_LeftRightMargin + 1, delta),
                     QPoint(4 * d + m_LeftRightMargin, delta));
    // linee verticali
    delta = m_LeftRightMargin + 1 + 16 * i;
    if (i % 4 == 0) continue;
    for (j = 1; j < 5; j++)
      painter.drawLine(QPoint(delta, (j - 1) * d + m_TopMargin),
                       QPoint(delta, j * d + m_TopMargin - 1));
  }

  // Disegno l'histogram.
  m_histogramView->draw(&painter, QPoint(m_LeftRightMargin - 10, 0));

  // Disegno la barra verticale a sinistra.
  m_verticalChannelBar->draw(
      &painter,
      QPoint(0, -2));  //-1 == m_topMargin- il margine della barra(=10+1).

  QRectF r = rect().adjusted(m_LeftRightMargin, m_TopMargin, -m_LeftRightMargin,
                             -m_BottomMargin);
  // Disegno la curva entro i limiti del grafo
  painter.setClipRect(r, Qt::IntersectClip);
  QPainterPath path = getPainterPath();
  if (path.isEmpty()) return;
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::black);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);

  // Disegno i punti di controllo (esclusi i primi tre e gli ultimi tre)
  r         = r.adjusted(-5, -5, 5, 5);
  int n     = m_points.size();
  QPointF p = m_points.at(3);
  for (i = 3; i < n - 3; i++) {
    QBrush brush(Qt::white);
    int rad       = 3;
    QPointF nextP = m_points.at(i + 1);
    if (isCentralControlPoint(i))
      rad = 4;
    else if (m_isLinear) {
      p = nextP;
      continue;
    }
    if (m_currentControlPointIndex == i) brush = QBrush(Qt::black);
    painter.setBrush(brush);
    painter.setPen(Qt::black);

    if (!m_isLinear) {
      if (isLeftControlPoint(i))
        painter.drawLine(p, nextP);
      else if (isCentralControlPoint(i) && i < n - 4)
        painter.drawLine(p, nextP);
    }
    QPainterPath circle;
    QRectF pointRect(p.x() - rad, p.y() - rad, 2 * rad, 2 * rad);
    if (r.contains(pointRect))
#if QT_VERSION >= 0x050000
      painter.setClipRect(pointRect.adjusted(-1, -1, 1, 1));
#else
      painter.setClipRect(pointRect.adjusted(-1, -1, 1, 1), Qt::UniteClip);
#endif
    circle.addEllipse(pointRect);
    painter.fillPath(circle, brush);
    painter.drawPath(circle);
    p = nextP;
  }
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::mouseMoveEvent(QMouseEvent *e) {
  if (m_mouseButton == Qt::LeftButton && m_currentControlPointIndex != -1) {
    QPoint pos   = e->pos();
    QPointF posF = QPointF(pos.x(), pos.y());
    moveCurrentControlPoint(posF - m_points.at(m_currentControlPointIndex));
  }
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::mousePressEvent(QMouseEvent *e) {
  m_mouseButton = e->button();
  setFocus();
  if (m_mouseButton == Qt::LeftButton) {
    QPoint pos   = e->pos();
    QPointF posF = QPointF(pos.x(), pos.y());
    double minDistance;
    int controlPointIndex = getClosestPointIndex(posF, minDistance);

    // Se la distanza e' piccola seleziono il control point corrente
    if (minDistance < 20)
      m_currentControlPointIndex = controlPointIndex;
    else {
      m_currentControlPointIndex = -1;
      // Se sono sufficentemente lontano da un punto di controllo, ma abbastanza
      // vicino alla curva
      // aggiungo un punto di controllo
      double percent = getPercentAtPoint(posF, getPainterPath());
      if (percent != 0 && minDistance > 20) addControlPoint(percent);
    }
    update();
  }
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::mouseReleaseEvent(QMouseEvent *e) {
  /*-- マウスドラッグ中はプレビューを更新しない。ここで初めて更新 --*/
  if (m_mouseButton == Qt::LeftButton && m_currentControlPointIndex != -1 &&
      e->button() == Qt::LeftButton)
    emit controlPointChanged(false);
  m_mouseButton = Qt::NoButton;
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Delete) removeCurrentControlPoint();
}

//-----------------------------------------------------------------------------

void ChennelCurveEditor::enterEvent(QEvent *) { m_mouseButton = Qt::NoButton; }

//-----------------------------------------------------------------------------

void ChennelCurveEditor::leaveEvent(QEvent *) { m_mouseButton = Qt::NoButton; }

//--------------------------------------------------------

bool ChennelCurveEditor::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::Shortcut ||
      event->type() == QEvent::ShortcutOverride) {
    if (!object->inherits("FxSettings")) {
      event->accept();
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------

void ChennelCurveEditor::focusInEvent(QFocusEvent *fe) {
  QWidget::focusInEvent(fe);
  qApp->installEventFilter(this);
}

//--------------------------------------------------------

void ChennelCurveEditor::focusOutEvent(QFocusEvent *fe) {
  m_currentControlPointIndex = -1;
  emit focusOut();
  QWidget::focusOutEvent(fe);
  qApp->removeEventFilter(this);
  update();
}

//=============================================================================
// ToneCurveField
//=============================================================================

ToneCurveField::ToneCurveField(QWidget *parent,
                               FxHistogramRender *fxHistogramRender)
    : QWidget(parent), m_toneCurveStackedWidget(0) {
  setFixedWidth(368);

  int currentChannelIndex = 0;

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);

  QStringList channels;
  channels << "RGBA"
           << "RGB"
           << "Red"
           << "Green"
           << "Blue"
           << "Alpha";
  int channelCount = channels.size();

  // lista canali: label+comboBox
  QWidget *channelListWidget     = new QWidget(this);
  QHBoxLayout *channelListLayout = new QHBoxLayout(channelListWidget);
  channelListLayout->setMargin(0);
  channelListLayout->setSpacing(0);
  QLabel *channelLabel = new QLabel(tr("Channel:"), channelListWidget);
  channelListLayout->addWidget(channelLabel);
  m_channelListChooser = new QComboBox(channelListWidget);
  m_channelListChooser->setFixedSize(100, 20);
  m_channelListChooser->addItems(channels);
  m_channelListChooser->setCurrentIndex(currentChannelIndex);
  channelListLayout->addWidget(m_channelListChooser);
  channelListWidget->setLayout(channelListLayout);
  mainLayout->addWidget(channelListWidget, 0, Qt::AlignCenter);

  // stack widget dei grafi
  m_toneCurveStackedWidget = new QStackedWidget(this);
  Histograms *histograms   = new Histograms(0, true);
  fxHistogramRender->setHistograms(histograms);
  int i;
  for (i = 0; i < channelCount; i++) {
    ChennelCurveEditor *c =
        new ChennelCurveEditor(this, histograms->getHistogramView(i));
    m_toneCurveStackedWidget->addWidget(c);
    connect(c, SIGNAL(firstLastXPostionChanged(int, int)), this,
            SLOT(onFirstLastXPostionChanged(int, int)));
  }

  QWidget *w          = new QWidget(this);
  QHBoxLayout *layout = new QHBoxLayout(w);
  layout->setMargin(0);
  layout->setSpacing(0);
  layout->addWidget(m_toneCurveStackedWidget);
  w->setLayout(layout);
  mainLayout->addWidget(w, 0, Qt::AlignHCenter);
  m_toneCurveStackedWidget->setCurrentIndex(currentChannelIndex);

  // stack widget degli slider
  m_sliderStackedWidget = new QStackedWidget(this);
  for (i = 0; i < channelCount; i++) {
    IntPairField *intPairSlider = new IntPairField(this);
    intPairSlider->setFixedHeight(20);
    intPairSlider->setLabelsEnabled(false);
    intPairSlider->setRange(0, 255);
    intPairSlider->setValues(std::make_pair(0, 255));
    m_sliderStackedWidget->addWidget(intPairSlider);
    connect(intPairSlider, SIGNAL(valuesChanged(bool)), this,
            SLOT(sliderValueChanged(bool)));
  }
  mainLayout->addWidget(m_sliderStackedWidget);
  m_sliderStackedWidget->setCurrentIndex(currentChannelIndex);

  mainLayout->addSpacing(10);
  m_isLinearCheckBox = new CheckBox(QString("Linear"), this);
  mainLayout->addWidget(m_isLinearCheckBox, 0, Qt::AlignHCenter);
  connect(m_isLinearCheckBox, SIGNAL(clicked(bool)),
          SLOT(setLinearManually(bool)));
  connect(m_isLinearCheckBox, SIGNAL(toggled(bool)), SLOT(setLinear(bool)));

  connect(m_channelListChooser, SIGNAL(currentIndexChanged(int)),
          m_toneCurveStackedWidget, SLOT(setCurrentIndex(int)));
  connect(m_channelListChooser, SIGNAL(currentIndexChanged(int)),
          m_sliderStackedWidget, SLOT(setCurrentIndex(int)));
  connect(m_channelListChooser, SIGNAL(currentIndexChanged(int)), this,
          SIGNAL(currentChannelIndexChanged(int)));

  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void ToneCurveField::setCurrentChannel(int currentChannel) {
  m_channelListChooser->setCurrentIndex(currentChannel);
}

//-----------------------------------------------------------------------------

ChennelCurveEditor *ToneCurveField::getChannelEditor(int channel) const {
  ChennelCurveEditor *c = dynamic_cast<ChennelCurveEditor *>(
      m_toneCurveStackedWidget->widget(channel));
  assert(c);
  return c;
}

//-----------------------------------------------------------------------------

ChennelCurveEditor *ToneCurveField::getCurrentChannelEditor() const {
  return getChannelEditor(m_toneCurveStackedWidget->currentIndex());
}

//-----------------------------------------------------------------------------

IntPairField *ToneCurveField::getCurrentSlider() const {
  return dynamic_cast<IntPairField *>(m_sliderStackedWidget->currentWidget());
}

//-----------------------------------------------------------------------------

void ToneCurveField::sliderValueChanged(bool isDragging) {
  std::pair<int, int> values = getCurrentSlider()->getValues();
  getCurrentChannelEditor()->setFirstLastXPosition(values, isDragging);
}

//-----------------------------------------------------------------------------

void ToneCurveField::onFirstLastXPostionChanged(int x0, int x1) {
  std::pair<int, int> values = getCurrentSlider()->getValues();
  if (values.first != x0 || values.second != x1)
    getCurrentSlider()->setValues(std::make_pair(x0, x1));
}

//-----------------------------------------------------------------------------

void ToneCurveField::setIsLinearCheckBox(bool isChecked) {
  if (m_isLinearCheckBox->isChecked() == isChecked) return;
  m_isLinearCheckBox->setChecked(isChecked);
}

//-----------------------------------------------------------------------------

void ToneCurveField::setLinear(bool isLinear) {
  int i;
  for (i = 0; i < m_sliderStackedWidget->count(); i++)
    getChannelEditor(i)->setLinear(isLinear);
}

//-----------------------------------------------------------------------------

void ToneCurveField::setLinearManually(bool isLinear) {
  emit isLinearChanged(isLinear);
}
