

#include "ruler.h"
#include "sceneviewer.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"

#include "toonz/toonzscene.h"
#include "toonz/stage.h"
#include "tunit.h"

#include <QPainter>
#include <QMouseEvent>

//=============================================================================
// Ruler
//-----------------------------------------------------------------------------

Ruler::Ruler(QWidget *parent, SceneViewer *viewer, bool vertical)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_vertical(vertical)
    , m_moving(false)
    , m_hiding(false) {
  if (vertical) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(12);
    setToolTip(tr("Click to create an horizontal guide"));
  } else {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(12);
    setToolTip(tr("Click to create a vertical guide"));
  }
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

Ruler::Guides &Ruler::getGuides() const {
  TSceneProperties *sprop =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  return m_vertical ? sprop->getVGuides() : sprop->getHGuides();
}

//-----------------------------------------------------------------------------

int Ruler::getGuideCount() const { return getGuides().size(); }

//-----------------------------------------------------------------------------

double Ruler::getGuide(int index) const {
  Guides &guides = getGuides();
  assert(0 <= index && index < (int)guides.size());
  return guides[index];
}

//-----------------------------------------------------------------------------

double Ruler::getUnit() const {
  double unit = getZoomScale() * Stage::inch;
  return unit;
}

//-----------------------------------------------------------------------------

void Ruler::getIndices(double origin, double iunit, int size, int &i0, int &i1,
                       int &ic) const {
  i0 = 0;
  i1 = -1;
  ic = 0;
  if (origin >= 0) {
    i0    = -tfloor(origin * iunit);
    i1    = i0 + tfloor(size * iunit);
    int d = tceil(-i0, 10);
    i0 += d;
    i1 += d;
    ic += d;
  } else {
    i0 = tceil(-origin * iunit);
    i1 = i0 + tfloor(size * iunit);
    ic = 0;
  }
  if (m_viewer->getIsFlippedX()) {
    i0 = i0 + i1;
    i1 = i0 - i1;
    i0 = i0 - i1;
  }
  // assert(i0>=0);
  // assert((ic%10)==0);
}

//-----------------------------------------------------------------------------

double Ruler::getZoomScale() const {
  if (m_viewer->is3DView())
    return m_viewer->getZoomScale3D();
  else
    return m_viewer->getViewMatrix().a11;
}

//-----------------------------------------------------------------------------

double Ruler::getPan() const {
  TAffine aff = m_viewer->getViewMatrix();
  if (m_vertical)
    if (m_viewer->is3DView())  // Vertical   3D
      return m_viewer->getPan3D().y;
    else  // Vertical   2D
      return aff.a23 / getDevPixRatio();
  else if (m_viewer->is3DView())  // Horizontal 3D
    return m_viewer->getPan3D().x;
  return aff.a13 / getDevPixRatio();  // Horizontal 2D
}

//-----------------------------------------------------------------------------

void Ruler::drawVertical(QPainter &p) {
  int w = width();
  int h = height();

  Guides &guides = getGuides();

  int x0 = 0, x1 = w - 1;
  p.setPen(Qt::DotLine);
  p.setPen(grey120);
  p.drawLine(x1, 0, x1, h - 1);

  double origin = -getPan() + 0.5 * h;
  int i;
  int count = guides.size();
  if (m_hiding) count--;
  double zoom = getZoomScale();
  if (m_viewer->getIsFlippedX() != m_viewer->getIsFlippedY()) {
    zoom = -zoom;
  }
  for (i = 0; i < count; i++) {
    QColor color =
        (m_moving && count - 1 == i ? QColor(0, 255, 255) : QColor(0, 0, 255));
    double v = guides[i] / (double)getDevPixRatio();
    int y    = (int)(origin - zoom * v);
    p.fillRect(QRect(x0, y - 1, x1 - x0, 2), QBrush(color));
  }

  double unit = getUnit() / UnitParameters::getFieldGuideAspectRatio();

  QColor color = m_scaleColor;

  int i0, i1, ic;
  getIndices(origin, 1 / unit, h, i0, i1, ic);
  for (i = i0; i <= i1; i++) {
    int y  = tround(origin + unit * (i - ic));
    int xb = x1 - 1, xa;
    if (i == ic) {
      p.setPen(m_scaleColor);
      xa = 1;
    } else {
      p.setPen(color);
      int h = i % 10;
      if (h == 0)
        xa = xb - 5;
      else if (h == 5)
        xa = xb - 3;
      else
        xa = xb - 2;
    }
    p.drawLine(xa, y, xb, y);
  }
}

//-----------------------------------------------------------------------------

void Ruler::drawHorizontal(QPainter &p) {
  int w          = width();
  int h          = height();
  Guides &guides = getGuides();
  int y0         = 0;
  int y1         = h - 1;
  p.setPen(Qt::DotLine);
  p.setPen(grey120);
  p.drawLine(0, y1, w - 1, y1);

  double origin = getPan() + 0.5 * w;
  double unit   = getUnit();
  int i;

  QColor color = m_scaleColor;

  int count = guides.size();
  if (m_hiding) count--;
  double zoom = getZoomScale();
  for (i = 0; i < count; i++) {
    QColor color =
        (m_moving && count - 1 == i ? QColor(0, 255, 255) : QColor(0, 0, 255));
    double v = guides[i] / (double)getDevPixRatio();
    int x    = (int)(origin + zoom * v);
    p.fillRect(QRect(x - 1, y0, 2, y1 - y0), QBrush(color));
  }

  int i0, i1, ic;
  getIndices(origin, 1 / unit, w, i0, i1, ic);
  for (i = i0; i <= i1; i++) {
    int x  = tfloor(origin + unit * (i - ic));
    int yb = y1 - 1, ya;
    if (i == ic) {
      p.setPen(m_scaleColor);
      ya = 1;
    } else {
      p.setPen(color);
      int h = i % 10;
      if (h == 0)
        ya = yb - 5;
      else if (h == 5)
        ya = yb - 3;
      else
        ya = yb - 2;
    }
    p.drawLine(x, ya, x, yb);
  }
}

//-----------------------------------------------------------------------------

void Ruler::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(QRect(0, 0, width(), height()), QBrush(m_parentBgColor));

  if (m_vertical)
    drawVertical(p);
  else
    drawHorizontal(p);
}

//-----------------------------------------------------------------------------

double Ruler::posToValue(const QPoint &p) const {
  double v;
  if (m_vertical)
    if (m_viewer->getIsFlippedX() != m_viewer->getIsFlippedY()) {
      v = (-p.y() + height() / 2 - getPan()) / -getZoomScale();
    } else
      v = (-p.y() + height() / 2 - getPan()) / getZoomScale();
  else
    v = (p.x() - width() / 2 - getPan()) / getZoomScale();
  return v;
}

//-----------------------------------------------------------------------------

void Ruler::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    Guides &guides  = getGuides();
    double v        = posToValue(e->pos());
    m_hiding        = false;
    m_moving        = false;
    int selected    = -1;
    double minDist2 = 0;
    int i;
    int count = guides.size();
    for (i = 0; i < count; i++) {
      double g     = guides[i] / (double)getDevPixRatio();
      double dist2 = (g - v) * (g - v);
      if (selected < 0 || dist2 < minDist2) {
        minDist2 = dist2;
        selected = i;
      }
    }
    if (selected < 0 || minDist2 > 25) {
      // crea una nuova guida
      guides.push_back(v * getDevPixRatio());
      m_viewer->update();
      // aggiorna sprop!!!!
    } else {
      // muove la guida vecchia
      if (selected < count - 1) std::swap(guides[selected], guides.back());
      // aggiorna sprop!!!!
    }
    m_moving = true;
    update();
    assert(guides.size() > 0);
  }
}

//-----------------------------------------------------------------------------

void Ruler::mouseMoveEvent(QMouseEvent *e) {
  if (m_moving) {
    m_hiding           = m_vertical ? (e->pos().x() < 0) : (e->pos().y() < 0);
    getGuides().back() = posToValue(e->pos()) * getDevPixRatio();
    // aggiorna sprop!!!!
    update();
    m_viewer->update();
    return;
  }

  Guides &guides  = getGuides();
  double v        = posToValue(e->pos());
  m_hiding        = false;
  m_moving        = false;
  double minDist2 = 5;
  int i;
  int count = guides.size();
  for (i = 0; i < count; i++) {
    double g     = guides[i] / (double)getDevPixRatio();
    double dist2 = (g - v) * (g - v);
    if (dist2 < minDist2)
      setToolTip(tr("Click and drag to move guide"));
    else {
      if (m_vertical)
        setToolTip(tr("Click to create an horizontal guide"));
      else
        setToolTip(tr("Click to create a vertical guide"));
    }
  }
}

//-----------------------------------------------------------------------------

void Ruler::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_moving) return;
  if (m_hiding) {
    assert(!getGuides().empty());
    getGuides().pop_back();
  }
  m_moving = m_hiding = false;
  update();
  m_viewer->update();
}

//-----------------------------------------------------------------------------
/*
std::wstring Ruler::getTooltipString(const TPoint &pos)
{
  Guides &guides = getGuides();
  double v = posToValue(pos);
  //int q = m_vertical ? pos.y : pos.x;
  m_hiding = false;
  m_moving = false;
  int selected = -1;
  double minDist2 = 0;
  int i;
  for(i=0; i<(int)guides.size(); i++)
  {
    double g = guides[i];
    double dist2 = (g-v)*(g-v);
    if(selected<0 || dist2<minDist2) {minDist2 = dist2; selected = i;}
  }
  if(selected<0 || minDist2>25)
  {
    // crea una nuova guida
    if (!m_vertical)
      return TStringTable::translate("W_VRuler");
    else
      return TStringTable::translate("W_HRuler");
  }
  else
  {
    // muove la guida vecchia
    if (!m_vertical)
      return TStringTable::translate("W_VGuide");
    else
      return TStringTable::translate("W_HGuide");
  }
}

//-----------------------------------------------------------------------------

string Ruler::getContextHelpReference(const TPoint &pos)
{
  Guides &guides = getGuides();
  double v = posToValue(pos);
  //int q = m_vertical ? pos.y : pos.x;
  m_hiding = false;
  m_moving = false;
  int selected = -1;
  double minDist2 = 0;
  int i;
  for(i=0; i<(int)guides.size(); i++)
  {
    double g = guides[i];
    double dist2 = (g-v)*(g-v);
    if(selected<0 || dist2<minDist2) {minDist2 = dist2; selected = i;}
  }
  if(selected<0 || minDist2>25)
  {
    // crea una nuova guida
    if (!m_vertical)
      return "W_VRuler";
    else
      return "W_HRuler";
  }
  else
  {
    // muove la guida vecchia
    if (!m_vertical)
      return "W_VGuide";
    else
      return "W_HGuide";
  }
}
*/
