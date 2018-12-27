#include "separatecolorsswatch.h"

#include "tapp.h"

#include "toonzqt/styleeditor.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"

#include "toonz/tcleanupper.h"

#include "tools/stylepicker.h"
#include "tools/toolcommandids.h"

#include "trasterimage.h"
#include "tpixelutils.h"
#include "trop.h"

#include <QGridLayout>
#include <QBrush>
#include <QPainter>
#include <QMouseEvent>
#include <QIcon>
#include <QPushButton>
#include <QActionGroup>
#include <QToolBar>
#include <QLabel>

//-------------------------------------------------------------------
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
//-------------------------------------------------------------------

SeparateSwatchArea::SeparateSwatchArea(SeparateSwatch *parent, SWATCH_TYPE type)
    : QWidget(parent)
    , m_type(type)
    , m_sw(parent)
    , m_transparentColor(64, 64, 64) {
  setMinimumHeight(100);

  // The following helps in re-establishing focus for wheel events
  setFocusPolicy(Qt::WheelFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

//-------------------------------------------------------------------

void SeparateSwatchArea::mousePressEvent(QMouseEvent *event) {
  if (!m_sw->m_mainRaster || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  m_pos = event->pos();

  // return if there is no raster
  if (!m_r) {
    event->ignore();
    m_panning = false;
    return;
  }

  // panning with drag
  m_panning = true;

  event->ignore();
}

//-----------------------------------------------------------------------------

void SeparateSwatchArea::keyPressEvent(QKeyEvent *event) {
  if (!m_sw->m_mainRaster || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  QString zoomInTxt =
      CommandManager::instance()->getAction(V_ZoomIn)->shortcut().toString();
  QString zoomOutTxt =
      CommandManager::instance()->getAction(V_ZoomOut)->shortcut().toString();

  bool isZoomIn =
      (QString::compare(event->text(), zoomInTxt, Qt::CaseInsensitive) == 0);
  bool isZoomOut =
      (QString::compare(event->text(), zoomOutTxt, Qt::CaseInsensitive) == 0);

  int key = event->key();

  if (key != '+' && key != '-' && key != '0' && !isZoomIn && !isZoomOut) return;

  bool reset   = (key == '0');
  bool forward = (key == '+' || isZoomIn);

  double currZoomScale = sqrt(m_sw->m_viewAff.det());
  double factor = reset ? 1 : getQuantizedZoomFactor(currZoomScale, forward);

  double minZoom = std::min((double)m_sw->m_lx / m_sw->m_mainRaster->getLx(),
                            (double)m_sw->m_ly / m_sw->m_mainRaster->getLy());
  if ((!forward && factor < minZoom) || (forward && factor > 40.0)) return;

  TPointD delta(0.5 * width(), 0.5 * height());
  m_sw->m_viewAff = (TTranslation(delta) * TScale(factor / currZoomScale) *
                     TTranslation(-delta)) *
                    m_sw->m_viewAff;

  m_sw->m_orgSwatch->updateRaster();
  m_sw->m_mainSwatch->updateRaster();
  m_sw->m_sub1Swatch->updateRaster();
  m_sw->m_sub2Swatch->updateRaster();
  m_sw->m_sub3Swatch->updateRaster();
}

//-----------------------------------------------------------------------------
void SeparateSwatchArea::mouseReleaseEvent(QMouseEvent *event) {
  m_sw->m_mainSwatch->updateRaster();
  m_sw->m_sub1Swatch->updateRaster();
  m_sw->m_sub2Swatch->updateRaster();
  m_sw->m_sub3Swatch->updateRaster();
  m_panning = false;
}

//----------------------------------------------------------------
void SeparateSwatchArea::mouseMoveEvent(QMouseEvent *event) {
  if (!m_panning) return;

  TPoint curPos = TPoint(event->pos().x(), event->pos().y());

  QPoint delta = event->pos() - m_pos;
  if (delta == QPoint()) return;
  TAffine oldAff  = m_sw->m_viewAff;
  m_sw->m_viewAff = TTranslation(delta.x(), -delta.y()) * m_sw->m_viewAff;

  m_sw->m_orgSwatch->updateRaster();
  m_sw->m_mainSwatch->updateRaster(true);
  m_sw->m_sub1Swatch->updateRaster(true);
  m_sw->m_sub2Swatch->updateRaster(true);
  m_sw->m_sub3Swatch->updateRaster(true);

  m_pos = event->pos();
}

//---------------------------------------------------------------

void SeparateSwatchArea::updateSeparated(bool dragging) {
  TAffine aff = getFinalAff();

  TRect rectToCompute = convert(aff.inv() * convert(m_r->getBounds()));
  TRect outRect       = m_sw->m_mainRaster->getBounds() * rectToCompute;

  if (outRect.isEmpty()) return;
}

//---------------------------------------------------------------------------------

TAffine SeparateSwatchArea::getFinalAff() {
  return m_sw->m_viewAff *
         TAffine().place(m_sw->m_mainRaster->getCenterD(), m_r->getCenterD());
}

//------------------------------------------------------------------------------
void SeparateSwatchArea::updateRaster(bool dragging) {
  if (isHidden() || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  if (!m_r || m_r->getLx() != m_sw->m_lx || m_r->getLy() != m_sw->m_ly)
    m_r = TRaster32P(m_sw->m_lx, m_sw->m_ly);

  if (!m_sw->m_mainRaster)
    m_r->fill(TPixel(200, 200, 200));
  else {
    m_r->fill(m_transparentColor);
    switch (m_type) {
    case Original:
      TRop::quickPut(m_r, m_sw->m_orgRaster, getFinalAff());
      break;
    case Main:
      TRop::quickPut(m_r, m_sw->m_mainRaster, getFinalAff());
      break;
    case Sub1:
      TRop::quickPut(m_r, m_sw->m_sub1Raster, getFinalAff());
      break;
    case Sub2:
      TRop::quickPut(m_r, m_sw->m_sub2Raster, getFinalAff());
      break;
    case Sub3:
      TRop::quickPut(m_r, m_sw->m_sub3Raster, getFinalAff());
      break;
    }
  }
  if (dragging)
    repaint();
  else
    update();
}

//----------------------------------------------------------------------------------

void SeparateSwatchArea::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  if (!m_r)
    p.fillRect(rect(), QBrush(QColor(200, 200, 200)));
  else
    p.drawImage(rect(), rasterToQImage(m_r));
}

//-----------------------------------------------------------------

void SeparateSwatchArea::wheelEvent(QWheelEvent *event) {
  if (!m_sw->m_mainSwatch || m_sw->m_lx == 0 || m_sw->m_ly == 0) return;

  int step      = event->delta() > 0 ? 120 : -120;
  double factor = exp(0.001 * step);
  if (factor == 1.0) return;
  double scale   = m_sw->m_viewAff.det();
  double minZoom = std::min((double)m_sw->m_lx / m_sw->m_mainRaster->getLx(),
                            (double)m_sw->m_ly / m_sw->m_mainRaster->getLy());
  if ((factor < 1 && sqrt(scale) < minZoom) || (factor > 1 && scale > 1200.0))
    return;

  TPointD delta(event->pos().x(), height() - event->pos().y());
  m_sw->m_viewAff =
      (TTranslation(delta) * TScale(factor) * TTranslation(-delta)) *
      m_sw->m_viewAff;

  m_sw->m_orgSwatch->updateRaster();
  m_sw->m_mainSwatch->updateRaster();
  m_sw->m_sub1Swatch->updateRaster();
  m_sw->m_sub2Swatch->updateRaster();
  m_sw->m_sub3Swatch->updateRaster();
}

//-------------------------------------------------------------------------------
void SeparateSwatchArea::setTranspColor(TPixel32 &lineColor, bool showAlpha) {
  if (showAlpha) {
    m_transparentColor = TPixel32::Black;
    return;
  }

  TPixelD lineColorD = toPixelD(lineColor);
  double h, l, s;
  rgb2hls(lineColorD.r, lineColorD.g, lineColorD.b, &h, &l, &s);
  // if the specified color is reddish, use blue color for transparent area
  if (l < 0.8)
    m_transparentColor = TPixel32::White;
  else
    m_transparentColor = TPixel32(64, 64, 64);
}

//-------------------------------------------------------------------------------
// focus on mouse enter

void SeparateSwatchArea::enterEvent(QEvent *event) {
  setFocus();
  event->accept();
}

//=====================================================================

SeparateSwatch::SeparateSwatch(QWidget *parent, int lx, int ly)
    : QWidget(parent), m_lx(lx), m_ly(ly), m_enabled(false), m_viewAff() {
  m_orgSwatch  = new SeparateSwatchArea(this, Original);
  m_mainSwatch = new SeparateSwatchArea(this, Main);
  m_sub1Swatch = new SeparateSwatchArea(this, Sub1);
  m_sub2Swatch = new SeparateSwatchArea(this, Sub2);
  m_sub3Swatch = new SeparateSwatchArea(this, Sub3);

  m_sub3Label = new QLabel(tr("Sub Color 3"), this);

  QGridLayout *mainLay = new QGridLayout();
  mainLay->setHorizontalSpacing(2);
  mainLay->setVerticalSpacing(2);
  {
    mainLay->addWidget(new QLabel(tr("Original"), this), 0, 0);
    mainLay->addWidget(new QLabel(tr("Main Color"), this), 0, 1);

    mainLay->addWidget(m_orgSwatch, 1, 0);
    mainLay->addWidget(m_mainSwatch, 1, 1);

    mainLay->addWidget(new QLabel(tr("Sub Color 1"), this), 2, 0);
    mainLay->addWidget(new QLabel(tr("Sub Color 2"), this), 2, 1);
    mainLay->addWidget(m_sub3Label, 2, 2);

    mainLay->addWidget(m_sub1Swatch, 3, 0);
    mainLay->addWidget(m_sub2Swatch, 3, 1);
    mainLay->addWidget(m_sub3Swatch, 3, 2);
  }
  mainLay->setRowStretch(0, 0);
  mainLay->setRowStretch(1, 1);
  mainLay->setRowStretch(2, 0);
  mainLay->setRowStretch(3, 1);
  setLayout(mainLay);

  m_sub3Swatch->setVisible(false);
  m_sub3Label->setVisible(false);
}

//---------------------------------------------------------------------

void SeparateSwatch::enable(bool state) {
  m_enabled = state;
  if (!m_enabled) {
    m_orgRaster  = TRasterP();
    m_mainRaster = TRasterP();
    m_sub1Raster = TRasterP();
    m_sub2Raster = TRasterP();
    m_sub3Raster = TRasterP();

    m_viewAff = TAffine();

    m_orgSwatch->updateRaster();
    m_mainSwatch->updateRaster();
    m_sub1Swatch->updateRaster();
    m_sub2Swatch->updateRaster();
    m_sub3Swatch->updateRaster();
  }
}

//----------------------------------------------------------------

void SeparateSwatch::resizeEvent(QResizeEvent *event) {
  QSize s = m_orgSwatch->size();
  m_lx    = s.width();
  m_ly    = s.height();

  m_orgSwatch->setMinimumHeight(0);
  m_mainSwatch->setMinimumHeight(0);
  m_sub1Swatch->setMinimumHeight(0);
  m_sub2Swatch->setMinimumHeight(0);
  m_sub3Swatch->setMinimumHeight(0);

  m_orgSwatch->updateRaster();
  m_mainSwatch->updateRaster();
  m_sub1Swatch->updateRaster();
  m_sub2Swatch->updateRaster();
  m_sub3Swatch->updateRaster();

  QWidget::resizeEvent(event);
}

//---------------------------------------------------

bool SeparateSwatch::isEnabled() { return isVisible() && m_enabled; }

//------------------------------------------------------------
void SeparateSwatch::setRaster(TRasterP orgRas, TRasterP mainRas,
                               TRasterP sub1Ras, TRasterP sub2Ras) {
  m_sub3Swatch->setVisible(false);
  m_sub3Label->setVisible(false);

  if (!isVisible()) {
    m_orgRaster  = TRasterP();
    m_mainRaster = TRasterP();
    m_sub1Raster = TRasterP();
    m_sub2Raster = TRasterP();
    return;
  }

  // obtain the size here in order to avoid the image shrink
  // when activating / deactivating sub3 color.
  qApp->processEvents();
  QSize s = m_orgSwatch->size();
  m_lx    = s.width();
  m_ly    = s.height();

  m_orgRaster  = orgRas;
  m_mainRaster = mainRas;
  m_sub1Raster = sub1Ras;
  m_sub2Raster = sub2Ras;

  m_orgSwatch->updateRaster();
  m_mainSwatch->updateRaster();
  m_sub1Swatch->updateRaster();
  m_sub2Swatch->updateRaster();
}

//------------------------------------------------------------
// 4 colors version

void SeparateSwatch::setRaster(TRasterP orgRas, TRasterP mainRas,
                               TRasterP sub1Ras, TRasterP sub2Ras,
                               TRasterP sub3Ras) {
  //o‚·
  m_sub3Swatch->setVisible(true);
  m_sub3Label->setVisible(true);

  if (!isVisible()) {
    m_orgRaster  = TRasterP();
    m_mainRaster = TRasterP();
    m_sub1Raster = TRasterP();
    m_sub2Raster = TRasterP();
    m_sub3Raster = TRasterP();
    return;
  }

  // obtain the size here in order to avoid the image shrink
  // when activating / deactivating sub3 color.
  qApp->processEvents();
  QSize s = m_orgSwatch->size();
  m_lx    = s.width();
  m_ly    = s.height();

  m_orgRaster  = orgRas;
  m_mainRaster = mainRas;
  m_sub1Raster = sub1Ras;
  m_sub2Raster = sub2Ras;
  m_sub3Raster = sub3Ras;

  m_orgSwatch->updateRaster();
  m_mainSwatch->updateRaster();
  m_sub1Swatch->updateRaster();
  m_sub2Swatch->updateRaster();
  m_sub3Swatch->updateRaster();
}

//--------------------------------------------------------------------

void SeparateSwatch::updateSeparated() {
  m_mainSwatch->updateRaster();
  m_sub1Swatch->updateRaster();
  m_sub2Swatch->updateRaster();
  m_sub3Swatch->updateRaster();
}

//--------------------------------------------------------------------

void SeparateSwatch::setTranspColors(TPixel32 &mainCol, TPixel32 &sub1Col,
                                     TPixel32 &sub2Col, TPixel32 &sub3Col,
                                     bool showAlpha) {
  m_mainSwatch->setTranspColor(mainCol, showAlpha);
  m_sub1Swatch->setTranspColor(sub1Col, showAlpha);
  m_sub2Swatch->setTranspColor(sub2Col, showAlpha);
  m_sub3Swatch->setTranspColor(sub3Col, showAlpha);
}

//--------------------------------------------------------------------
void SeparateSwatch::showSub3Swatch(bool on) {
  setRaster(TRasterP(), TRasterP(), TRasterP(), TRasterP(), TRasterP());

  m_sub3Swatch->setVisible(on);
  m_sub3Label->setVisible(on);
}