

#include "cleanupswatch.h"
#include "trop.h"
#include "toonzqt/gutil.h"
#include <QHBoxLayout>
#include <QBrush>
#include <QPainter>
#include <QMouseEvent>
#include <QIcon>
#include <QPushButton>
#include <QActionGroup>
#include <QToolBar>
#include "toonz/tcleanupper.h"
#include "toonzqt/dvdialog.h"
CleanupSwatch::CleanupSwatch(QWidget *parent, int lx, int ly)
    : QWidget(parent)
    , m_lx(lx)
    , m_ly(ly)
    , m_enabled(false)
    , m_viewAff()
//, m_lastRasCleanupped()
{
  setStyleSheet("background: transparent");
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(0);

  m_leftSwatch = new CleanupSwatchArea(this, true);

  layout->addWidget(m_leftSwatch);

  DVGui::Separator *sep = new DVGui::Separator();
  sep->setFixedWidth(1);
  sep->setOrientation(false);
  layout->addWidget(sep);

  m_rightSwatch = new CleanupSwatchArea(this, false);
  layout->addWidget(m_rightSwatch);

  setLayout(layout);
}

//---------------------------------------------------------------------

void CleanupSwatch::enable(bool state) {
  m_enabled = state;
  if (!m_enabled) {
    m_resampledRaster = TRasterP();
    // m_lastRasCleanupped = TRasterP();
    m_origRaster  = TRasterP();
    m_viewAff     = TAffine();
    m_resampleAff = TAffine();
    m_leftSwatch->updateRaster();
    m_rightSwatch->updateRaster();
  }
}

//------------------------------------------------------------------------------
/*
void CleanupSwatch::hideEvent(QHideEvent* e)
{
m_enabledAct->setChecked(false);
}*/

//----------------------------------------------------------------

CleanupSwatch::CleanupSwatchArea::CleanupSwatchArea(CleanupSwatch *parent,
                                                    bool isLeft)
    : QWidget(parent), m_isLeft(isLeft), m_sw(parent) {
  setMinimumHeight(150);

  // The following helps in re-establishing focus for wheel events
  setFocusPolicy(Qt::WheelFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

//----------------------------------------------------------------

void CleanupSwatch::CleanupSwatchArea::mousePressEvent(QMouseEvent *event) {
  if (!m_sw->m_resampledRaster || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;
  // if (m_sw->m_lastRasCleanupped)
  //   TRop::addBackground(m_sw->m_lastRasCleanupped, TPixel::White);
  m_pos = event->pos();

  if (event->button() != Qt::MidButton || !m_sw->m_resampledRaster) {
    event->ignore();
    return;
    m_panning = false;
  } else
    m_panning = true;
}

//-----------------------------------------------------------------------------
namespace {
#define ZOOMLEVELS 33
#define NOZOOMINDEX 20
double ZoomFactors[ZOOMLEVELS] = {
    0.001, 0.002,  0.003,  0.004, 0.005, 0.007, 0.01,  0.015, 0.02,  0.03, 0.04,
    0.05,  0.0625, 0.0833, 0.125, 0.167, 0.25,  0.333, 0.5,   0.667, 1,    2,
    3,     4,      5,      6,     7,     8,     12,    16,    24,    32,   40};

double getQuantizedZoomFactor(double zf, bool forward) {
  if (forward && (zf > ZoomFactors[ZOOMLEVELS - 1] ||
                  areAlmostEqual(zf, ZoomFactors[ZOOMLEVELS - 1], 1e-5)))
    return zf;
  else if (!forward &&
           (zf < ZoomFactors[0] || areAlmostEqual(zf, ZoomFactors[0], 1e-5)))
    return zf;

  assert((!forward && zf > ZoomFactors[0]) ||
         (forward && zf < ZoomFactors[ZOOMLEVELS - 1]));
  int i = 0;
  for (i = 0; i <= ZOOMLEVELS - 1; i++)
    if (areAlmostEqual(zf, ZoomFactors[i], 1e-5)) zf = ZoomFactors[i];

  if (forward && zf < ZoomFactors[0])
    return ZoomFactors[0];
  else if (!forward && zf > ZoomFactors[ZOOMLEVELS - 1])
    return ZoomFactors[ZOOMLEVELS - 1];

  for (i = 0; i < ZOOMLEVELS - 1; i++)
    if (ZoomFactors[i + 1] - zf >= 0 && zf - ZoomFactors[i] >= 0) {
      if (forward && ZoomFactors[i + 1] == zf)
        return ZoomFactors[i + 2];
      else if (!forward && ZoomFactors[i] == zf)
        return ZoomFactors[i - 1];
      else
        return forward ? ZoomFactors[i + 1] : ZoomFactors[i];
    }
  return ZoomFactors[NOZOOMINDEX];
}

}  // namespace
//-----------------------------------------------------------------------------

void CleanupSwatch::CleanupSwatchArea::keyPressEvent(QKeyEvent *event) {
  if (!m_sw->m_resampledRaster || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;
  int key = event->key();
  if (key != '+' && key != '-' && key != '0') return;

  if (key == '0')
    m_sw->m_viewAff = TAffine();
  else {
    bool forward = (key == '+');

    double currZoomScale = sqrt(m_sw->m_viewAff.det());
    double factor        = getQuantizedZoomFactor(currZoomScale, forward);

    double minZoom =
        std::min((double)m_sw->m_lx / m_sw->m_resampledRaster->getLx(),
                 (double)m_sw->m_ly / m_sw->m_resampledRaster->getLy());
    if ((!forward && factor < minZoom) || (forward && factor > 40.0)) return;

    TPointD delta(0.5 * width(), 0.5 * height());
    m_sw->m_viewAff = (TTranslation(delta) * TScale(factor / currZoomScale) *
                       TTranslation(-delta)) *
                      m_sw->m_viewAff;
  }
  m_sw->m_leftSwatch->updateRaster();
  m_sw->m_rightSwatch->updateRaster();
}

//-----------------------------------------------------------------------------
void CleanupSwatch::CleanupSwatchArea::mouseReleaseEvent(QMouseEvent *event) {
  m_sw->m_rightSwatch->updateRaster();
  m_panning = false;
}

//----------------------------------------------------------------
void CleanupSwatch::CleanupSwatchArea::mouseMoveEvent(QMouseEvent *event) {
  if (!m_panning) return;

  TPoint curPos = TPoint(event->pos().x(), event->pos().y());

  QPoint delta = event->pos() - m_pos;
  if (delta == QPoint()) return;
  TAffine oldAff  = m_sw->m_viewAff;
  m_sw->m_viewAff = TTranslation(delta.x(), -delta.y()) * m_sw->m_viewAff;

  m_sw->m_leftSwatch->updateRaster();
  m_sw->m_rightSwatch->updateRaster(true);

  m_pos = event->pos();
}

//---------------------------------------------------------------

void CleanupSwatch::CleanupSwatchArea::wheelEvent(QWheelEvent *event) {
  if (!m_sw->m_resampledRaster || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  int step      = event->delta() > 0 ? 120 : -120;
  double factor = exp(0.001 * step);
  if (factor == 1.0) return;
  double scale = m_sw->m_viewAff.det();
  double minZoom =
      std::min((double)m_sw->m_lx / m_sw->m_resampledRaster->getLx(),
               (double)m_sw->m_ly / m_sw->m_resampledRaster->getLy());
  if ((factor < 1 && sqrt(scale) < minZoom) || (factor > 1 && scale > 1200.0))
    return;

  TPointD delta(event->pos().x(), height() - event->pos().y());
  m_sw->m_viewAff =
      (TTranslation(delta) * TScale(factor) * TTranslation(-delta)) *
      m_sw->m_viewAff;

  m_sw->m_leftSwatch->updateRaster();
  m_sw->m_rightSwatch->updateRaster();
}

//-------------------------------------------------------------------------------

void CleanupSwatch::resizeEvent(QResizeEvent *event) {
  QSize s = m_leftSwatch->size();
  m_lx    = s.width();
  m_ly    = s.height();

  // m_rightSwatch->resize(m_lx, m_ly);
  m_leftSwatch->setMinimumHeight(0);
  m_rightSwatch->setMinimumHeight(0);
  m_leftSwatch->updateRaster();
  m_rightSwatch->updateRaster();
  // m_rightSwatch->updateRaster();
  QWidget::resizeEvent(event);
  // update();
}

/*
void CleanupSwatch::enableRightSwatch(bool state)
{
setVisible(state);
update();
}*/

//---------------------------------------------------

void CleanupSwatch::CleanupSwatchArea::updateCleanupped(bool dragging) {
  TAffine aff = getFinalAff();

  TRect rectToCompute = convert(aff.inv() * convert(m_r->getBounds()));
  rectToCompute       = rectToCompute.enlarge(
      TCleanupper::instance()->getParameters()->m_despeckling);

  TRect outRect = m_sw->m_resampledRaster->getBounds() * rectToCompute;
  if (outRect.isEmpty()) return;

  TRasterP rasCleanupped = TCleanupper::instance()->processColors(
      m_sw->m_resampledRaster->extract(outRect));
  TPointD cleanuppedPos = convert(outRect.getP00());

  TRop::quickPut(m_r, rasCleanupped, aff * TTranslation(cleanuppedPos));
}

//---------------------------------------------------------------------------------
#ifdef LEVO

void CleanupSwatch::CleanupSwatchArea::updateCleanupped Versione ricalcolo Al
    Release Del
    mouse(bool dragging) {
  TAffine aff = getFinalAff();

  TRect rectToCompute = convert(aff.inv() * convert(m_r->getBounds()));
  rectToCompute       = rectToCompute.enlarge(
      TCleanupper::instance()->getParameters()->m_despeckling);

  TRect outRect = m_sw->m_resampledRaster->getBounds() * rectToCompute;
  if (outRect.isEmpty()) return;

  if (dragging && m_sw->m_lastRasCleanupped)
    m_r->fill(TPixel(200, 200, 200));
  else {
    m_sw->m_lastRasCleanupped = TCleanupper::instance()->processColors(
        m_sw->m_resampledRaster->extract(outRect));
    m_sw->m_lastCleanuppedPos = convert(outRect.getP00());
  }
  TRop::quickPut(m_r, m_sw->m_lastRasCleanupped,
                 aff * TTranslation(m_sw->m_lastCleanuppedPos));
}
#endif

//---------------------------------------------------------------------------------

TAffine CleanupSwatch::CleanupSwatchArea::getFinalAff() {
  return m_sw->m_viewAff *
         TAffine().place(m_sw->m_resampledRaster->getCenterD(),
                         m_r->getCenterD());
}

//------------------------------------------------------------------------------
void CleanupSwatch::CleanupSwatchArea::updateRaster(bool dragging) {
  if (isHidden() || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  if (!m_r || m_r->getLx() != m_sw->m_lx || m_r->getLy() != m_sw->m_ly)
    m_r = TRaster32P(m_sw->m_lx, m_sw->m_ly);

  if (!m_sw->m_resampledRaster)
    m_r->fill(TPixel(200, 200, 200));
  else {
    m_r->fill(TPixel::White);
    if (m_isLeft)
      TRop::quickPut(m_r, m_sw->m_origRaster,
                     getFinalAff() * m_sw->m_resampleAff);
    else
      updateCleanupped(dragging);
  }
  if (dragging)
    repaint();
  else
    update();
}

//----------------------------------------------------------------------------------

void CleanupSwatch::CleanupSwatchArea::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  if (!m_r)
    p.fillRect(rect(), QBrush(QColor(200, 200, 200)));
  else
    p.drawImage(rect(), rasterToQImage(m_r));
}

//-----------------------------------------------------------------

bool CleanupSwatch::isEnabled() { return isVisible() && m_enabled; }

//------------------------------------------------------------
void CleanupSwatch::setRaster(TRasterP rasLeft, const TAffine &aff,
                              TRasterP ras) {
  if (!isVisible()) {
    m_resampledRaster = TRasterP();
    m_origRaster      = TRasterP();
    // m_lastRasCleanupped = TRasterP();
    return;
  }

  m_resampledRaster = ras;
  m_origRaster      = rasLeft;
  m_resampleAff     = aff;

  m_leftSwatch->updateRaster();
  m_rightSwatch->updateRaster();
}

//--------------------------------------------------------------------

void CleanupSwatch::updateCleanupped() { m_rightSwatch->updateRaster(); }
