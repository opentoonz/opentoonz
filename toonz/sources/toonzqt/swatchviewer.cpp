

#include "toonzqt/swatchviewer.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"

#include "../toonz/menubarcommandids.h"

#include <QPainter>
#include <QMouseEvent>
#include <QGestureEvent>
#include <QResizeEvent>

#include "trasterfx.h"
#include "toonz/tcolumnfx.h"
#include "tparamcontainer.h"
#include "tfxutil.h"

// Rendering cache management includes
#include "tfxcachemanager.h"
#include "tcacheresourcepool.h"
#include "tpassivecachemanager.h"

#include <QEventLoop>
#include <QCoreApplication>

#include <QDebug>

using namespace TFxUtil;

//#define USE_SQLITE_HDPOOL

//*************************************************************************************
//    Swatch cache delegate
//*************************************************************************************

/*!
The swatch cache delegate is used to temporary store intermediate rendering
results
in cache when an fx is being edited.
Input fxs of an edited fx are typically requested to produce the same results
multiple times
as parameters of their schematic parent change - so, caching these results is a
remunerative
practice as long as the fx is open for edit.

This delegate stores resources on two different levels: one is directly related
to swatch rendering,
while the other is used to cache results on the other rendering contexts.

In the swatch case, we store the above mentioned input results associated with
the current scene
scale, releasing them when the scene scale changes; plus, the resource
associated with the current
output is also stored.

In other rendering contexts, the scene scale can be assumed constant - so the
above distinction
is unnecessary. All results from the input fxs are stored until the edited fx is
unset from the swatch.

Please observe that variations in the scene context - such as frame change,
schematic changes, scene changes
and so on - actually cause the fx to be re-set for edit in the swatch. Once this
happens, the previously
stored results are conveniently flushed from the cache.
*/

/*NOTE: This can be extended in case we realize multiple swatch viewers... It
should be sufficient to map
a swatch pointer to its associated cache data - the advantage being that cache
resources are shared at the
same scene zoom.*/

class SwatchCacheManager final : public TFxCacheManagerDelegate {
  T_RENDER_RESOURCE_MANAGER

  unsigned long m_setFxId;
  std::set<unsigned long> m_childrenFxIds;

  std::set<TCacheResourceP> m_genericCacheContainer;
  std::set<TCacheResourceP> m_swatchCacheContainer;
  TCacheResourceP m_currEditedFxResult;

  QMutex m_mutex;

public:
  SwatchCacheManager() {}
  ~SwatchCacheManager() {}

  static SwatchCacheManager *instance();

  void setFx(const TFxP &actualFx);

  void clearSwatchResults();

  void getResource(TCacheResourceP &resource, const std::string &alias,
                   const TFxP &fx, double frame, const TRenderSettings &rs,
                   ResourceDeclaration *resData) override;

  // void onRenderInstanceStart(unsigned long renderId);
  // void onRenderInstanceEnd(unsigned long renderId);

  bool renderHasOwnership() override { return false; }
};

//*****************************************************************************************
//    Manager generator
//*****************************************************************************************

class SwatchCacheManagerGenerator final
    : public TRenderResourceManagerGenerator {
  TRenderResourceManager *operator()(void) override {
    // return new TPassiveCacheManager;
    return SwatchCacheManager::instance();
  }
};

MANAGER_FILESCOPE_DECLARATION_DEP(SwatchCacheManager,
                                  SwatchCacheManagerGenerator,
                                  TFxCacheManager::deps())

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//! Abilita o disabilita la cache nell'effetto \b fx di \b frame in funzione di
//! \b on
void setFxForCaching(TFx *fx) {
  SwatchCacheManager::instance()->setFx(fx);
  TPassiveCacheManager::instance()->releaseContextNamesWithPrefix("S");
}

//-----------------------------------------------------------------------------

//! Se name finisce con suffix ritorna la parte iniziale, altrimenti ""
std::string matchSuffix(std::string name, std::string suffix) {
  if (name.length() <= suffix.length()) return "";
  int i = name.length() - suffix.length();
  if (name.substr(i) == suffix)
    return name.substr(0, i);
  else
    return "";
}

//-----------------------------------------------------------------------------

TRaster32P createCrossIcon() {
  TRaster32P crossIcon = TRaster32P(7, 7);
  // m_crossIcon e' utilizzata per evidenziare gli eventuali \b Point
  // memorizzati in \b m_points.
  crossIcon->fill(TPixel32(0, 0, 0, 0));
  TPixel32 *c = crossIcon->pixels(3) + 3;
  for (int i = 1; i <= 3; i++)
    c[i] = c[-i] = c[7 * i] = c[-7 * i] =
        (i & 1) == 0 ? TPixel32::White : TPixel32::Red;
  return crossIcon;
}

//-----------------------------------------------------------------------------
//! Disegna una freccia lunga \b len pixel.
/*!La punta della freccia si trova a coordinate (0,ly/2), la coda a
 * (len-1,ly/2).
*/
TRaster32P createArrowShape(int len) {
  int d            = 5;
  if (len < d) len = d;
  TPixel32 c0(210, 210, 210);
  TPixel32 c1(10, 10, 10);

  TRaster32P ras(len, d * 2 + 1);
  ras->clear();
  ras->lock();
  TPixel32 *pix = ras->pixels(d);
  int x         = 0;
  for (x = 0; x < len; x++) pix[x] = (x & 8) == 0 ? c0 : c1;
  for (x = 1; x < d; x++)
    for (int y = -x; y < x; y++) {
      assert(ras->getBounds().contains(TPoint(x, y + d)));
      pix[len * y + x] = c0;
    }
  ras->unlock();
  return ras;
}

//-----------------------------------------------------------------------------
// Preso da sceneViewer.cpp quando si spostera' lo SceneViewer in toonzqt
// mettere
// il codice in comune!

#define ZOOMLEVELS 30
#define NOZOOMINDEX 20
double ZoomFactors[ZOOMLEVELS] = {
    0.001, 0.002, 0.003,  0.004,  0.005, 0.007, 0.01, 0.015, 0.02, 0.03,
    0.04,  0.05,  0.0625, 0.0833, 0.125, 0.167, 0.25, 0.333, 0.5,  0.667,
    1,     2,     3,      4,      5,     6,     7,    8,     12,   16};

double getQuantizedZoomFactor(double zf, bool forward) {
  if ((forward && zf > ZoomFactors[ZOOMLEVELS - 1]) ||
      areAlmostEqual(zf, ZoomFactors[ZOOMLEVELS - 1], 1e-5))
    return zf;
  else if ((!forward && zf < ZoomFactors[0]) ||
           areAlmostEqual(zf, ZoomFactors[0], 1e-5))
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

//-----------------------------------------------------------------------------

bool suspendedRendering = false;  // Global vars for swatch rendering suspension
QEventLoop *waitingLoop = 0;
int submittedTasks      = 0;

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// SwatchViewer
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
SwatchViewer::SwatchViewer(QWidget *parent, Qt::WindowFlags flags)
#else
SwatchViewer::SwatchViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QWidget(parent, flags)
    , m_fx(0)
    , m_actualFxClone(0)
    , m_mouseButton(Qt::NoButton)
    , m_selectedPoint(0)
    , m_pointPosDelta(TPointD())
    , m_enabled(false)
    , m_content()
    , m_aff(TAffine())
    , m_fxAff(TAffine())
    , m_cameraRect()
    , m_bgPainter(0)
    , m_pos(TPoint())
    , m_firstPos(TPoint())
    , m_oldContent()
    , m_curContent()
    , m_executor()
    , m_firstEnabled(true) {
  // setMinimumSize(150,150);
  setMinimumHeight(150);
  setFixedWidth(150);
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  m_raster    = TRaster32P(width(), height());
  m_crossIcon = createCrossIcon();
  setFocusPolicy(Qt::StrongFocus);
  m_executor.setDedicatedThreads(true);
  m_executor.setMaxActiveTasks(1);

  m_renderer.enablePrecomputing(false);

  setAttribute(Qt::WA_AcceptTouchEvents);
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PanGesture);
  grabGesture(Qt::PinchGesture);
}

//-----------------------------------------------------------------------------

SwatchViewer::~SwatchViewer() {}

//-----------------------------------------------------------------------------

//! This static method is used to temporarily suspend all swatch-related render
//! processing, typically because the rendering scene is being deleted.
//! When a suspension is invoked, all further rendering requests made to
//! swatch viewers are silently rejected, while currently active or scheduled
//! ones
//! are canceled and waited for completion.
void SwatchViewer::suspendRendering(bool suspend, bool blocking) {
  suspendedRendering = suspend;

  if (suspend && submittedTasks > 0 && blocking) {
    QEventLoop loop;

    waitingLoop = &loop;
    loop.exec();
    waitingLoop = 0;
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::setCameraSize(const TDimension &cameraSize) {
  TRect cameraRect(cameraSize);
  if (cameraRect != m_cameraRect) {
    m_cameraRect = cameraRect;
    updateSize(size());  // Invoke a size update to adapt the widget to the new
                         // camera ratio
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::setFx(const TFxP &fx, const TFxP &actualFx, int frame) {
  m_fx = m_actualFxClone = fx;
  m_frame                = frame;
  m_points.clear();
  m_pointPairs.clear();

  if (!fx) {
    ::setFxForCaching(0);
    computeContent();
    return;
  }

  // abilita la cache nel nuovo effetto corrente
  ::setFxForCaching(actualFx.getPointer());

  if (NaAffineFx *affFx = dynamic_cast<NaAffineFx *>(m_fx.getPointer()))
    m_fxAff = affFx->getPlacement(m_frame);
  else
    m_fxAff = TAffine();
  int i;
  for (i = 0; i < actualFx->getParams()->getParamCount(); i++) {
    TPointParam *pointParam =
        dynamic_cast<TPointParam *>(actualFx->getParams()->getParam(i));
    if (pointParam) m_points.push_back(Point(i, pointParam));
  }
  // cerco i segmenti
  int n = m_points.size();
  for (i = 0; i < n; i++) {
    std::string name   = m_points[i].m_param->getName();
    std::string prefix = matchSuffix(name, "_a");
    if (prefix == "") continue;
    std::string otherName = prefix + "_b";
    int j;
    for (j = 0; j < n; j++)
      if (i != j && m_points[j].m_param->getName() == otherName) break;
    if (j < n) {
      m_pointPairs.push_back(std::make_pair(i, j));
      m_points[i].m_pairFlag = m_points[j].m_pairFlag = true;
    }
  }
  computeContent();
}

//-----------------------------------------------------------------------------

void SwatchViewer::updateFrame(int frame) {
  m_frame = frame;
  computeContent();
  update();
}

//-----------------------------------------------------------------------------

void SwatchViewer::setEnable(bool enabled) {
  if (m_enabled == enabled) return;
  m_enabled = enabled;
  if (m_enabled) {
    if (m_firstEnabled) {
      m_firstEnabled = false;
      fitView();
    }
    computeContent();
  } else
    update();
}

//-----------------------------------------------------------------------------

void SwatchViewer::updateSize(const QSize &size) {
  int h        = size.height();
  double ratio = m_cameraRect.getLy() > 0
                     ? m_cameraRect.getLx() / (double)m_cameraRect.getLy()
                     : 1.0;
  int w = std::min((int)(h * ratio), parentWidget()->width());
  setFixedWidth(w);
  if (w > 2 && h > 2)
    m_raster = TRaster32P(TDimension(w, h));
  else
    m_raster = TRaster32P();
}

//-----------------------------------------------------------------------------

void SwatchViewer::setBgPainter(TPixel32 color1, TPixel32 color2) {
  if (color2 == TPixel32())
    m_bgPainter = new SolidColorBgPainter("", color1);
  else
    m_bgPainter = new CheckboardBgPainter("", color1, color2);
  updateRaster();
}

//-----------------------------------------------------------------------------

TPoint SwatchViewer::world2win(const TPointD &p) const {
  TPointD center(width() * 0.5, height() * 0.5);
  return convert(m_aff * m_fxAff * p + center);
}

//-----------------------------------------------------------------------------

TPointD SwatchViewer::win2world(const TPoint &p) const {
  TPointD center(width() * 0.5, height() * 0.5);
  TPointD point = TPointD(convert(p) - center);
  return m_fxAff.inv() * m_aff.inv() * TPointD(point.x, -point.y);
}

//-----------------------------------------------------------------------------

void SwatchViewer::zoom(const TPoint &pos, double factor) {
  if (!m_content || factor == 1.0) return;

  TPointD delta = convert(pos);
  double scale  = m_aff.det();
  TAffine aff;
  if ((scale < 2000 || factor < 1) && (scale > 0.004 || factor > 1)) {
    aff = TTranslation(delta) * TScale(factor) * TTranslation(-delta);
    setAff(aff * m_aff);
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::resetView() { setAff(TAffine()); }

//-----------------------------------------------------------------------------

void SwatchViewer::fitView() {
  if (m_cameraRect.isEmpty()) return;

  double imageScale = std::min(
      width() / ((double)getCameraSize().lx * (m_cameraMode ? 1 : 0.44)),
      height() / ((double)getCameraSize().ly * (m_cameraMode ? 1 : 0.44)));

  TAffine fitAffine = TScale(imageScale, imageScale);
  fitAffine.a13     = 0;
  fitAffine.a23     = 0;

  setAff(fitAffine);
}

//-----------------------------------------------------------------------------

void SwatchViewer::zoom(bool forward, bool reset) {
  double scale2 = m_aff.det();
  if (reset || ((scale2 < 2000 || !forward) && (scale2 > 0.004 || forward))) {
    double oldZoomScale = sqrt(scale2);
    double zoomScale =
        reset ? 1 : getQuantizedZoomFactor(oldZoomScale, forward);
    TAffine aff = TScale(zoomScale / oldZoomScale);

    setAff(aff * m_aff);
  }
}

void SwatchViewer::pan(const TPoint &delta) {
  TPointD step = convert(delta);
  setAff(TTranslation(step.x, -step.y) * m_aff);
}

//-----------------------------------------------------------------------------

void SwatchViewer::computeContent() {
  if (suspendedRendering) return;
  if (!m_enabled) return;
  if (!m_raster) return;

  // Clear the swatch cache when the zoom scale has changed (cache results are
  // not compatible
  // between different scale levels)
  if (m_aff.a11 != m_contentAff.a11 || m_panning)
    SwatchCacheManager::instance()->clearSwatchResults();

  TRect rect(0, 0, width() - 1, height() - 1);
  TDimension size = rect.getSize();
  assert(m_raster->getSize() == size);
  if (m_fx) {
    // TFxP fx = makeAffine(m_fx, m_aff);
    // TRasterFxP rasterFx = fx;
    TRasterFxP rasterFx = m_fx;
    if (rasterFx) {
      m_executor.cancelAll();
      m_executor.addTask(
          new ContentRender(rasterFx.getPointer(), m_frame, size, this));

      submittedTasks++;
      return;
    } else {
      m_content = TRaster32P(size);
      m_content->fill(TPixel32::Red);
    }
  } else {
    m_content = TRaster32P(size);
    m_content->fill(TPixel32::Transparent);
  }
  updateRaster();
}

//-----------------------------------------------------------------------------

void SwatchViewer::updateRaster() {
  QMutexLocker sl(&m_mutex);

  if (!m_enabled) return;
  if (!m_raster) return;
  if (m_bgPainter)
    m_bgPainter->paint(m_raster);
  else
    m_raster->fill(TPixel32(127, 127, 127));

  if (m_cameraMode && !m_cameraRect.isEmpty()) {
    TPointD p0(m_cameraRect.x0, m_cameraRect.y0);
    TPointD p1(m_cameraRect.x1, m_cameraRect.y1);
    TPointD center(width() * 0.5, height() * 0.5);
    TPoint transP0 = convert(m_aff * p0 + center);
    TPoint transP1 = convert(m_aff * p1 + center);
    TPoint p       = convert(
        (TPointD(transP1.x, transP1.y) - TPointD(transP0.x, transP0.y)) * 0.5);
    TRect rect(transP0 - p, transP1 - p);
    m_content->fillOutside(rect, TPixel32(255, 0, 0, 255));
    m_content->fillOutside(rect.enlarge(TDimension(1, 1)),
                           TPixel32(0, 0, 0, 0));
  }

  if (m_content) TRop::over(m_raster, m_content);

  int i;
  for (i = 0; i < (int)m_points.size(); i++) {
    if (m_points[i].m_pairFlag) continue;
    TPoint p = world2win(m_points[i].m_param->getValue(m_frame));
    TRop::over(m_raster, m_crossIcon, p - TPoint(4, 4));
  }
  for (i = 0; i < (int)m_pointPairs.size(); i++) {
    int i0 = m_pointPairs[i].first;
    int i1 = m_pointPairs[i].second;
    assert(i0 != i1);
    assert(0 <= i0 && i0 < (int)m_points.size());
    assert(0 <= i1 && i1 < (int)m_points.size());
    TPoint p0        = world2win(m_points[i0].m_param->getValue(m_frame));
    TPoint p1        = world2win(m_points[i1].m_param->getValue(m_frame));
    TPoint delta     = p1 - p0;
    int len          = tround(sqrt((double)(delta * delta)));
    double phi       = 0;
    if (len > 0) phi = atan2((double)delta.y, (double)delta.x) * M_180_PI;

    if (len > 500) {
      // puo' succedere per zoom molto grandi.
      // dovrei fare qualcosa, ma non so bene che cosa e non credo sia
      // importantissimo
    } else {
      TRaster32P arrowShape = createArrowShape(len);
      TAffine aff =
          TRotation(phi).place(0, arrowShape->getLy() / 2, p0.x, p0.y);
      TRop::over(m_raster, arrowShape, aff);
      // verrebbe la tentazione di usare il filtro TRop::Bilinear (piu'veloce),
      // ma la qualita' ne risente molto
    }
  }

  update();
}

//-----------------------------------------------------------------------------

void SwatchViewer::setContent(const TRaster32P &content,
                              const TAffine &contentAff) {
  m_content    = content;
  m_contentAff = contentAff;

  updateRaster();
  update();
}

//-----------------------------------------------------------------------------

void SwatchViewer::setAff(const TAffine &aff) {
  m_aff = aff;
  computeContent();
}

//-----------------------------------------------------------------------------

void SwatchViewer::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  QRect rectBox = rect();

  if (!m_enabled)
    p.fillRect(rectBox, QBrush(QColor(120, 120, 120)));
  else {
    if (!m_raster) return;
    QImage image = rasterToQImage(m_raster);
    p.drawImage(rectBox, image);
    if (m_computing) {
      QPen pen;
      pen.setColor(Qt::red);
      pen.setWidth(3);
      p.setPen(pen);
      p.drawRect(rectBox.adjusted(0, 0, -1, -1));
    }
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::resizeEvent(QResizeEvent *re) {
  int oldHeight = re->oldSize().height();
  int newHeight = re->size().height();
  if (oldHeight != newHeight) {
    updateSize(QSize(newHeight, newHeight));
    computeContent();
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::mousePressEvent(QMouseEvent *event) {
  // qDebug() << "[mousePressEvent]";
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen &&
      !m_stylusUsed) {
    return;
  }

  TPoint pos    = TPoint(event->pos().x(), event->pos().y());
  m_mouseButton = event->button();
  if (m_mouseButton == Qt::LeftButton) {
    m_selectedPoint = -1;
    if (m_points.empty()) return;
    TPointD p = win2world(pos);
    TPointD q;
    double minDist2 = 1e6;
    int i;
    for (i = 0; i < (int)m_points.size(); i++) {
      TPointD paramPoint = m_points[i].m_param->getValue(m_frame);
      double d2          = tdistance2(p, paramPoint);
      if (m_selectedPoint < 0 || d2 < minDist2) {
        m_selectedPoint = i;
        minDist2        = d2;
        q               = paramPoint;
      }
    }
    if (m_selectedPoint >= 0) {
      m_pointPosDelta = q - p;
      TPoint d        = world2win(p) - world2win(q);
      int dd2         = d.x * d.x + d.y * d.y;
      if (dd2 > 400)
        m_selectedPoint = -1;
      else {
        std::string name   = m_points[m_selectedPoint].m_param->getName();
        std::string prefix = matchSuffix(name, "_b");
        if (prefix != "") {
          std::string otherName = prefix + "_a";
          int n                 = (int)m_points.size();
          int j;
          for (j = 0; j < n; j++)
            if (i != j && m_points[j].m_param->getName() == otherName) break;
          if (j < n) {
            TPoint dist = world2win(m_points[m_selectedPoint].m_param->getValue(
                              m_frame)) -
                          world2win(m_points[j].m_param->getValue(m_frame));
            int ddist2 = dist.x * dist.x + dist.y * dist.y;
            if (ddist2 < 100) m_selectedPoint = j;
          }
        }
      }
    }
    update();
  } else if (m_mouseButton == Qt::MidButton) {
    m_pos        = pos;
    m_firstPos   = pos;
    m_oldContent = getContent();
    if (m_oldContent)
      m_curContent = TRaster32P(m_oldContent->getSize());
    else
      m_curContent = TRaster32P();
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::mouseMoveEvent(QMouseEvent *event) {
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen &&
      !m_stylusUsed) {
    return;
  }

  TPoint pos = TPoint(event->pos().x(), event->pos().y());
  if (m_mouseButton == Qt::LeftButton) {
    if (m_selectedPoint < 0 || m_selectedPoint >= (int)m_points.size()) return;
    TPointD p = win2world(pos) + m_pointPosDelta;
    int index = m_points[m_selectedPoint].m_index;
    emit pointPositionChanged(index, p);

    // It seems that mouse move events may jeopardize our rendering notification
    // deliveries
    // - probably because Qt considers them 'higher priority stuff' with respect
    // to common queued signal-slot connections.

    // In order to allow processing of the ContentRender::started() and
    // ContentRender::finished()
    // signals, we need to process events. We will exclude user input events (ie
    // other mouse events)
    // to avoid unnecessary recursions.

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
  } else if (m_mouseButton == Qt::MidButton) {
    pan(pos - m_pos);
    m_pos = pos;
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::mouseReleaseEvent(QMouseEvent *event) {
  m_mouseButton   = Qt::NoButton;
  m_selectedPoint = -1;
  TPoint pos      = TPoint(event->pos().x(), event->pos().y());
  if (event->button() == Qt::MidButton) {
    if (!m_oldContent || !m_curContent) return;
    TPointD p = convert(pos - m_pos);
    setAff(TTranslation(p.x, -p.y) * m_aff);
    update();
  }
  m_gestureActive = false;
  m_zooming       = false;
  m_panning       = false;
  m_stylusUsed    = false;
}

//-----------------------------------------------------------------------------

void SwatchViewer::wheelEvent(QWheelEvent *event) {
  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    if (event->modifiers() & Qt::AltModifier)
      delta = event->angleDelta().x();
    else
      delta = event->angleDelta().y();
    break;
  }

  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
    {
      std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                   "Qt::MouseEventSynthesizedByApplication"
                << std::endl;
      break;
    }

  }  // end switch

  if (abs(delta) > 0) {
    if ((m_gestureActive == true &&
         m_touchDevice == QTouchDevice::TouchScreen) ||
        m_gestureActive == false) {
      TPoint center(event->pos().x() - width() / 2,
                    -event->pos().y() + height() / 2);
      zoom(center, exp(0.001 * event->delta()));
    }
  }
  event->accept();
}

//-----------------------------------------------------------------------------

void SwatchViewer::keyPressEvent(QKeyEvent *event) {
  int key = event->key();
  std::string keyStr =
      QKeySequence(key + event->modifiers()).toString().toStdString();
  QAction *action = CommandManager::instance()->getActionFromShortcut(keyStr);
  if (action) {
    std::string actionId = CommandManager::instance()->getIdFromAction(action);
    if (actionId == V_ZoomFit) {
      fitView();
      return;
    } else if (actionId == V_ZoomReset) {
      resetView();
      return;
    }
  }

  if (key == '+' || key == '-' || key == '0') {
    zoom(key == '+', key == '0');
  }
}

//-----------------------------------------------------------------------------

void SwatchViewer::hideEvent(QHideEvent *event) {
  // Clear the swatch cache
  ::setFxForCaching(0);
}

//-------------------------------------------------------------------------------

void SwatchViewer::mouseDoubleClickEvent(QMouseEvent *event) {
  // qDebug() << "[mouseDoubleClickEvent]";
  if (m_gestureActive && !m_stylusUsed) {
    m_gestureActive = false;
    fitView();
    return;
  }
}

//------------------------------------------------------------------

void SwatchViewer::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);

  QAction *reset = menu->addAction(tr("Reset View"));
  reset->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(V_ZoomReset)));
  connect(reset, SIGNAL(triggered()), SLOT(resetView()));

  QAction *fit = menu->addAction(tr("Fit To Window"));
  fit->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(V_ZoomFit)));
  connect(fit, SIGNAL(triggered()), SLOT(fitView()));

  menu->exec(event->globalPos());

  delete menu;
  update();
}

//------------------------------------------------------------------

void SwatchViewer::tabletEvent(QTabletEvent *e) {
  // qDebug() << "[tabletEvent]";
  if (e->type() == QTabletEvent::TabletPress) {
    m_stylusUsed = e->pointerType() ? true : false;
  } else if (e->type() == QTabletEvent::TabletRelease) {
    m_stylusUsed = false;
  }

  e->accept();
}

//------------------------------------------------------------------

void SwatchViewer::gestureEvent(QGestureEvent *e) {
  // qDebug() << "[gestureEvent]";
  m_gestureActive = false;
  if (QGesture *swipe = e->gesture(Qt::SwipeGesture)) {
    m_gestureActive = true;
  } else if (QGesture *pan = e->gesture(Qt::PanGesture)) {
    m_gestureActive = true;
  }
  if (QGesture *pinch = e->gesture(Qt::PinchGesture)) {
    QPinchGesture *gesture = static_cast<QPinchGesture *>(pinch);
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    QPoint firstCenter                     = gesture->centerPoint().toPoint();
    if (m_touchDevice == QTouchDevice::TouchScreen)
      firstCenter = mapFromGlobal(firstCenter);

    if (gesture->state() == Qt::GestureStarted) {
      m_gestureActive = true;
    } else if (gesture->state() == Qt::GestureFinished) {
      m_gestureActive = false;
      m_zooming       = false;
      m_scaleFactor   = 0.0;
    } else {
      if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        double scaleFactor = gesture->scaleFactor();
        // the scale factor makes for too sensitive scaling
        // divide the change in half
        if (scaleFactor > 1) {
          double decimalValue = scaleFactor - 1;
          decimalValue /= 1.5;
          scaleFactor = 1 + decimalValue;
        } else if (scaleFactor < 1) {
          double decimalValue = 1 - scaleFactor;
          decimalValue /= 1.5;
          scaleFactor = 1 - decimalValue;
        }
        if (!m_zooming) {
          double delta = scaleFactor - 1;
          m_scaleFactor += delta;
          if (m_scaleFactor > .2 || m_scaleFactor < -.2) {
            m_zooming = true;
          }
        }
        if (m_zooming) {
          TPoint center(firstCenter.x() - width() / 2,
                        -firstCenter.y() + height() / 2);
          zoom(center, scaleFactor);
          m_panning = false;
        }
        m_gestureActive = true;
      }

      if (changeFlags & QPinchGesture::CenterPointChanged) {
        QPointF centerDelta =
            (gesture->centerPoint()) - (gesture->lastCenterPoint());
        if (centerDelta.manhattanLength() > 1) {
          // panQt(centerDelta.toPoint());
        }
        m_gestureActive = true;
      }
    }
  }
  e->accept();
}

void SwatchViewer::touchEvent(QTouchEvent *e, int type) {
  // qDebug() << "[touchEvent]";
  if (type == QEvent::TouchBegin) {
    m_touchActive   = true;
    m_firstPanPoint = e->touchPoints().at(0).pos();
    // obtain device type
    m_touchDevice = e->device()->type();
  } else if (m_touchActive) {
    // touchpads must have 2 finger panning for tools and navigation to be
    // functional on other devices, 1 finger panning is preferred
    if ((e->touchPoints().count() == 2 &&
         m_touchDevice == QTouchDevice::TouchPad) ||
        (e->touchPoints().count() == 1 &&
         m_touchDevice == QTouchDevice::TouchScreen)) {
      QTouchEvent::TouchPoint panPoint = e->touchPoints().at(0);
      if (!m_panning) {
        QPointF deltaPoint = panPoint.pos() - m_firstPanPoint;
        // minimize accidental and jerky zooming/rotating during 2 finger
        // panning
        if ((deltaPoint.manhattanLength() > 100) && !m_zooming) {
          m_panning = true;
        }
      }
      if (m_panning) {
        QPoint curPos  = panPoint.pos().toPoint();
        QPoint lastPos = panPoint.lastPos().toPoint();
        TPoint centerDelta =
            TPoint(curPos.x(), curPos.y()) - TPoint(lastPos.x(), lastPos.y());
        pan(centerDelta);
      }
    }
  }
  if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
    m_touchActive = false;
    m_panning     = false;
  }
  e->accept();
}

bool SwatchViewer::event(QEvent *e) {
  /*
  switch (e->type()) {
  case QEvent::TabletPress: {
  QTabletEvent *te = static_cast<QTabletEvent *>(e);
  qDebug() << "[event] TabletPress: pointerType(" << te->pointerType()
  << ") device(" << te->device() << ")";
  } break;
  case QEvent::TabletRelease:
  qDebug() << "[event] TabletRelease";
  break;
  case QEvent::TouchBegin:
  qDebug() << "[event] TouchBegin";
  break;
  case QEvent::TouchEnd:
  qDebug() << "[event] TouchEnd";
  break;
  case QEvent::TouchCancel:
  qDebug() << "[event] TouchCancel";
  break;
  case QEvent::MouseButtonPress:
  qDebug() << "[event] MouseButtonPress";
  break;
  case QEvent::MouseButtonDblClick:
  qDebug() << "[event] MouseButtonDblClick";
  break;
  case QEvent::MouseButtonRelease:
  qDebug() << "[event] MouseButtonRelease";
  break;
  case QEvent::Gesture:
  qDebug() << "[event] Gesture";
  break;
  default:
  break;
  }
  */

  if (e->type() == QEvent::Gesture &&
      CommandManager::instance()
          ->getAction(MI_TouchGestureControl)
          ->isChecked()) {
    gestureEvent(static_cast<QGestureEvent *>(e));
    return true;
  }
  if ((e->type() == QEvent::TouchBegin || e->type() == QEvent::TouchEnd ||
       e->type() == QEvent::TouchCancel || e->type() == QEvent::TouchUpdate) &&
      CommandManager::instance()
          ->getAction(MI_TouchGestureControl)
          ->isChecked()) {
    touchEvent(static_cast<QTouchEvent *>(e), e->type());
    m_gestureActive = true;
    return true;
  }
  return QWidget::event(e);
}

//=============================================================================
// SwatchViewer::ContentRender
//-----------------------------------------------------------------------------

SwatchViewer::ContentRender::ContentRender(TRasterFx *fx, int frame,
                                           const TDimension &size,
                                           SwatchViewer *viewer)
    : m_fx(fx)
    , m_raster(0)
    , m_frame(frame)
    , m_size(size)
    , m_aff(viewer->m_aff)
    , m_viewer(viewer)
    , m_started(false) {
  // Is there a less complicated way...?
  connect(this, SIGNAL(started(TThread::RunnableP)), this,
          SLOT(onStarted(TThread::RunnableP)));
  connect(this, SIGNAL(finished(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));
  connect(this, SIGNAL(exception(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));
  connect(this, SIGNAL(canceled(TThread::RunnableP)), this,
          SLOT(onCanceled(TThread::RunnableP)),
          Qt::QueuedConnection);  // Starts will need to come *strictly before*
                                  // cancels
}

//-----------------------------------------------------------------------------

SwatchViewer::ContentRender::~ContentRender() {}

//-----------------------------------------------------------------------------

void SwatchViewer::ContentRender::run() {
  if (suspendedRendering) return;

  unsigned long renderId = TRenderer::buildRenderId();

  TPassiveCacheManager::instance()->setContextName(renderId, "S");

  m_viewer->m_renderer.install(renderId);
  m_viewer->m_renderer.declareRenderStart(renderId);
  m_viewer->m_renderer.declareFrameStart(m_frame);

  TRenderSettings info;
  info.m_isSwatch = true;
  info.m_affine   = m_aff;

  TTile tile;
  m_fx->allocateAndCompute(tile, -0.5 * TPointD(m_size.lx, m_size.ly), m_size,
                           0, (double)m_frame, info);
  m_raster = tile.getRaster();

  m_viewer->m_renderer.declareFrameEnd(m_frame);
  m_viewer->m_renderer.declareRenderEnd(renderId);
  m_viewer->m_renderer.uninstall();
}

//-----------------------------------------------------------------------------

int SwatchViewer::ContentRender::taskLoad() { return 100; }

//-----------------------------------------------------------------------------

void SwatchViewer::ContentRender::onStarted(TThread::RunnableP task) {
  m_started             = true;
  m_viewer->m_computing = true;
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void SwatchViewer::ContentRender::onFinished(TThread::RunnableP task) {
  m_viewer->m_computing = false;

  m_viewer->setContent(m_raster, m_aff);
  if ((--submittedTasks == 0) && waitingLoop) waitingLoop->quit();
}

//-----------------------------------------------------------------------------

void SwatchViewer::ContentRender::onCanceled(TThread::RunnableP task) {
  if (m_started) return;

  if ((--submittedTasks == 0) && waitingLoop) waitingLoop->quit();
}

//*************************************************************************************
//    Swatch cache manager implementation
//*************************************************************************************

SwatchCacheManager *SwatchCacheManager::instance() {
  static SwatchCacheManager theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------------

void SwatchCacheManager::setFx(const TFxP &fx) {
  QMutexLocker locker(&m_mutex);

  // Update the fxs id data
  if (fx == TFxP()) {
    // Clear if no fx is set
    m_setFxId = 0;
    m_childrenFxIds.clear();
  } else {
    m_setFxId = fx->getIdentifier();
    m_childrenFxIds.clear();
    assert(m_setFxId != 0);

    TRasterFx *rfx = dynamic_cast<TRasterFx *>(fx.getPointer());
    assert(rfx);

    for (int i = 0; i < fx->getInputPortCount(); ++i) {
      // Fxs not allowing cache on the input port are skipped
      if (!rfx->allowUserCacheOnPort(i)) continue;

      TFxPort *iport = fx->getInputPort(i);
      if (iport && iport->isConnected()) {
        TFx *child = iport->getFx();

        // In the zerary case, extract the actual fx
        TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(child);
        if (zcfx) child       = zcfx->getZeraryFx();

        assert(child && child->getIdentifier() != 0);
        m_childrenFxIds.insert(child->getIdentifier());
      }
    }
  }

  // NOTE: Check if this should be avoided in some case...

  // Release the locks and clear the resources
  if (m_currEditedFxResult) m_currEditedFxResult->releaseLock();
  m_currEditedFxResult = TCacheResourceP();

  std::set<TCacheResourceP>::iterator it;
  for (it = m_swatchCacheContainer.begin(); it != m_swatchCacheContainer.end();
       ++it)
    (*it)->releaseLock();
  m_swatchCacheContainer.clear();

#ifdef USE_SQLITE_HDPOOL
  TCacheResourcePool::instance()->releaseReferences("S");
#else
  for (it = m_genericCacheContainer.begin();
       it != m_genericCacheContainer.end(); ++it)
    (*it)->releaseLock();
  m_genericCacheContainer.clear();
#endif
}

//-----------------------------------------------------------------------------

// This method is invoked by the swatch when its scene scale changes. Find it
// above.
void SwatchCacheManager::clearSwatchResults() {
  QMutexLocker locker(&m_mutex);

  if (m_currEditedFxResult) m_currEditedFxResult->releaseLock();
  m_currEditedFxResult = TCacheResourceP();

  std::set<TCacheResourceP>::iterator it;
  for (it = m_swatchCacheContainer.begin(); it != m_swatchCacheContainer.end();
       ++it)
    (*it)->releaseLock();
  m_swatchCacheContainer.clear();
}

//-----------------------------------------------------------------------------

void SwatchCacheManager::getResource(TCacheResourceP &resource,
                                     const std::string &alias, const TFxP &fx,
                                     double frame, const TRenderSettings &rs,
                                     ResourceDeclaration *resData) {
  // Only FX RESULTS are interesting - plus, avoid if we're not currently
  // editing an fx.
  if (!(fx && m_setFxId > 0)) return;

  QMutexLocker locker(&m_mutex);

  // Cache the result in case the fx's id is among the stored ones.
  unsigned long fxId = fx->getIdentifier();

  if (fxId == m_setFxId && rs.m_isSwatch) {
    if (!resource) resource = TCacheResourceP(alias, true);

    resource->addLock();
    if (m_currEditedFxResult) m_currEditedFxResult->releaseLock();

    m_currEditedFxResult = resource;
    return;
  }

  if (m_childrenFxIds.find(fxId) != m_childrenFxIds.end()) {
    if (!resource) resource = TCacheResourceP(alias, true);

    if (rs.m_isSwatch) {
      std::set<TCacheResourceP>::iterator it =
          m_swatchCacheContainer.find(resource);

      if (it == m_swatchCacheContainer.end()) {
        resource->addLock();
        m_swatchCacheContainer.insert(resource);
      }
    } else {
#ifdef USE_SQLITE_HDPOOL
      resource->enableBackup();
      TCacheResourcePool::instance()->addReference(resource, "S");
#else
      std::set<TCacheResourceP>::iterator it =
          m_genericCacheContainer.find(resource);

      if (it == m_genericCacheContainer.end()) {
        resource->addLock();
        m_genericCacheContainer.insert(resource);
      }
#endif
    }
  }
}
