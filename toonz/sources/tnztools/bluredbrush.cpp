

#include "bluredbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"

#include <QColor>

namespace {

static QVector<QRgb> colorTable;

QImage rasterToQImage(const TRasterP &ras, bool premultiplied = false) {
  QImage image;
  if (TRaster32P ras32 = ras)
    image = QImage(ras->getRawData(), ras->getLx(), ras->getLy(),
                   premultiplied ? QImage::Format_ARGB32_Premultiplied
                                 : QImage::Format_ARGB32);
  else if (TRasterGR8P ras8 = ras) {
    image = QImage(ras->getRawData(), ras->getLx(), ras->getLy(),
                   ras->getWrap(), QImage::Format_Indexed8);
    image.setColorTable(colorTable);
  }
  return image;
}

//----------------------------------------------------------------------------------
// drawOrderMode : 0=OverAll, 1=UnderAll, 2=PaletteOrder
void putOnRasterCM(const TRasterCM32P &out, const TRaster32P &in, int styleId,
                   int drawOrderMode, const QSet<int> &aboveStyleIds) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  int x, y;
  if (drawOrderMode == 0) {  // OverAll
    for (y = 0; y < out->getLy(); y++) {
      for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
        assert(x >= 0 && x < in->getLx());
        assert(y >= 0 && y < in->getLy());
        assert(x >= 0 && x < out->getLx());
        assert(y >= 0 && y < out->getLy());
#endif
        TPixel32 *inPix = &in->pixels(y)[x];
        if (inPix->m == 0) continue;
        TPixelCM32 *outPix = &out->pixels(y)[x];
        bool sameStyleId   = styleId == outPix->getInk();
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
        int ink = !sameStyleId && outPix->getTone() < 255 - inPix->m
                      ? outPix->getInk()
                      : styleId;
        *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  } else if (drawOrderMode == 1) {  // UnderAll
    for (y = 0; y < out->getLy(); y++) {
      for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
        assert(x >= 0 && x < in->getLx());
        assert(y >= 0 && y < in->getLy());
        assert(x >= 0 && x < out->getLx());
        assert(y >= 0 && y < out->getLy());
#endif
        TPixel32 *inPix = &in->pixels(y)[x];
        if (inPix->m == 0) continue;
        TPixelCM32 *outPix = &out->pixels(y)[x];
        bool sameStyleId   = styleId == outPix->getInk();
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
        int ink = !sameStyleId && outPix->getTone() <= 255 - inPix->m
                      ? outPix->getInk()
                      : styleId;
        *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  } else {  // PaletteOrder
    for (y = 0; y < out->getLy(); y++) {
      for (x = 0; x < out->getLx(); x++) {
        TPixel32 *inPix = &in->pixels(y)[x];
        if (inPix->m == 0) continue;
        TPixelCM32 *outPix = &out->pixels(y)[x];
        bool sameStyleId   = styleId == outPix->getInk();
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
        bool chooseOutPixInk = outPix->getTone() < 255 - inPix->m ||
                               (outPix->getTone() == 255 - inPix->m &&
                                aboveStyleIds.contains(outPix->getInk()));
        int ink = !sameStyleId && chooseOutPixInk ? outPix->getInk() : styleId;
        *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  }
}

//----------------------------------------------------------------------------------

void eraseFromRasterCM(const TRasterCM32P &out, const TRaster32P &in,
                       bool selective, int selectedStyleId,
                       const std::wstring &mode) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  bool eraseLine  = mode == L"Lines" || mode == L"Lines & Areas";
  bool eraseAreas = mode == L"Areas" || mode == L"Lines & Areas";
  int x, y;

  for (y = 0; y < out->getLy(); y++) {
    for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
      assert(y >= 0 && y < in->getLy());
      assert(y >= 0 && y < out->getLy());
#endif
      TPixel32 *inPix = &in->pixels(y)[x];
      if (inPix->m == 0) continue;
      TPixelCM32 *outPix = &out->pixels(y)[x];
      bool eraseInk =
          !selective || (selective && selectedStyleId == outPix->getInk());
      bool erasePaint =
          !selective || (selective && selectedStyleId == outPix->getPaint());
      int paint = eraseAreas && erasePaint ? 0 : outPix->getPaint();
      int tone  = inPix->m > 0 && eraseLine && eraseInk
                     ? std::max(outPix->getTone(), (int)inPix->m)
                     : outPix->getTone();
      *outPix = TPixelCM32(outPix->getInk(), paint, tone);
    }
  }
}

//----------------------------------------------------------------------------------

TRasterP rasterFromQImage(
    const QImage &image)  // no need of const& - Qt uses implicit sharing...
{
  QImage::Format format = image.format();
  if (format == QImage::Format_ARGB32 ||
      format == QImage::Format_ARGB32_Premultiplied)
    return TRaster32P(image.width(), image.height(), image.width(),
                      (TPixelRGBM32 *)image.bits(), false);
  if (format == QImage::Format_Indexed8)
    return TRasterGR8P(image.width(), image.height(), image.bytesPerLine(),
                       (TPixelGR8 *)image.bits(), false);
  return TRasterP();
}
}

//=======================================================
//
// BluredBrush
//
//=======================================================

BluredBrush::BluredBrush(const TRaster32P &ras, int size,
                         const QRadialGradient &gradient, bool doDinamicOpacity)
    : m_ras(ras)
    , m_size(size)
    , m_lastPoint(0, 0)
    , m_oldOpacity(0)
    , m_enableDinamicOpacity(doDinamicOpacity) {
  m_rasImage = rasterToQImage(m_ras, false);
  m_gradient = gradient;

  if (colorTable.size() == 0) {
    int i;
    for (i = 0; i < 256; i++) colorTable.append(QColor(i, i, i).rgb());
  }
}

//----------------------------------------------------------------------------------

BluredBrush::~BluredBrush() {}

//----------------------------------------------------------------------------------

void BluredBrush::addPoint(const TThickPoint &p, double opacity) {
  double radius      = p.thick * 0.5;
  double brushRadius = m_size * 0.5;
  double scaleFactor = radius / brushRadius;

  QPainter painter(&m_rasImage);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(m_gradient);
  painter.setMatrix(
      QMatrix(scaleFactor, 0.0, 0.0, scaleFactor, p.x - radius, p.y - radius),
      false);
  if (m_enableDinamicOpacity) painter.setOpacity(opacity);
  painter.drawEllipse(0, 0, m_size, m_size);
  painter.end();

  m_lastPoint  = p;
  m_oldOpacity = opacity;
}

//----------------------------------------------------------------------------------

void BluredBrush::addArc(const TThickPoint &pa, const TThickPoint &pb,
                         const TThickPoint &pc, double opacityA,
                         double opacityC) {
  QPainter painter(&m_rasImage);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  // painter.setBrush(m_gradient);

  TThickQuadratic q(pa, pb, pc);
  double brushRadius = m_size * 0.5;
  double t           = 0;
  while (t <= 1) {
    t = getNextPadPosition(q, t);
    if (t > 1) break;
    TThickPoint point  = q.getThickPoint(t);
    double radius      = point.thick * 0.5;
    double scaleFactor = radius / brushRadius;

    painter.setMatrix(QMatrix(scaleFactor, 0.0, 0.0, scaleFactor,
                              point.x - radius, point.y - radius),
                      false);
    if (m_enableDinamicOpacity) {
      double opacity = opacityA + ((opacityC - opacityA) * t);
      if (fabs(opacity - m_oldOpacity) > 0.01)
        opacity =
            opacity > m_oldOpacity ? m_oldOpacity + 0.01 : m_oldOpacity - 0.01;
      painter.setOpacity(opacity);
      painter.setCompositionMode(QPainter::CompositionMode_DestinationAtop);
      m_oldOpacity = opacity;
      painter.setBrush(QColor(0, 0, 0, 255));
    } else
      painter.setBrush(m_gradient);
    painter.drawEllipse(0, 0, m_size, m_size);

    m_lastPoint = point;
  }
  painter.end();
}

//----------------------------------------------------------------------------------

double BluredBrush::getNextPadPosition(const TThickQuadratic &q,
                                       double t) const {
  TThickPoint p = m_lastPoint;
  double d      = 0.12 * p.thick;
  d             = d >= 1.0 ? d : 1.0;
  double d2     = d * d;
  if (norm2(q.getP2() - p) < d2) return 2.0;
  double t2        = (t + 1) * 0.5;
  TThickPoint p2   = q.getThickPoint(t2);
  double distance2 = norm2(p2 - p);
  double lastTMin  = t;
  double lastTMax  = 1;
  while (!areAlmostEqual(d2, distance2, 0.25) && t2 != lastTMin &&
         t2 != lastTMax) {
    if (distance2 > d2) {
      lastTMax = t2;
      t2       = (lastTMin + t2) * 0.5;
    } else {
      lastTMin = t2;
      t2       = (lastTMax + t2) * 0.5;
    }
    p2        = q.getThickPoint(t2);
    distance2 = norm2(p2 - p);
  }
  return t2;
}

//----------------------------------------------------------------------------------

void BluredBrush::updateDrawing(const TRasterP ras, const TRasterP rasBackup,
                                const TPixel32 &color, const TRect &bbox,
                                double opacity) const {
  TRect rasRect    = ras->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;
  QImage image = rasterToQImage(ras, true);
  QRect qTargetRect(targetRect.x0, targetRect.y0, targetRect.getLx(),
                    targetRect.getLy());

  QImage app(qTargetRect.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter p2(&app);
  p2.setBrush(QColor(color.r, color.g, color.b));
  p2.drawRect(app.rect().adjusted(-1, -1, 0, 0));
  p2.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  p2.drawImage(QPoint(), m_rasImage, qTargetRect);
  p2.end();

  if (ras->getPixelSize() == 4) {
    QPainter p(&image);
    p.setClipRect(qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(qTargetRect, rasterToQImage(rasBackup, true), qTargetRect);
    p.end();

    p.begin(&image);
    p.setOpacity(m_enableDinamicOpacity ? 1 : opacity);
    p.drawImage(qTargetRect, app, app.rect());
    p.end();
  } else {
    QImage targetImage = rasterToQImage(rasBackup).copy(qTargetRect);
    targetImage        = targetImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied, colorTable);

    QPainter p(&targetImage);
    p.setOpacity(m_enableDinamicOpacity ? 1 : opacity);
    p.drawImage(QPoint(), app, app.rect());
    p.end();
    targetImage =
        targetImage.convertToFormat(QImage::Format_Indexed8, colorTable);

    TRasterGR8P targetRas = rasterFromQImage(targetImage);
    ras->copy(targetRas, targetRect.getP00());
  }
}

//----------------------------------------------------------------------------------

void BluredBrush::eraseDrawing(const TRasterP ras, const TRasterP rasBackup,
                               const TRect &bbox, double opacity) const {
  if (!ras) return;

  TRect rasRect    = ras->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;
  QRect qTargetRect(targetRect.x0, targetRect.y0, targetRect.getLx(),
                    targetRect.getLy());
  if (ras->getPixelSize() == 4) {
    QImage image = rasterToQImage(ras, true);
    QPainter p(&image);
    p.setClipRect(qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(qTargetRect, rasterToQImage(rasBackup, true), qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setOpacity(opacity);
    p.drawImage(qTargetRect, m_rasImage, qTargetRect);
    p.end();
  } else if (ras->getPixelSize() != 4) {
    QImage targetImage = rasterToQImage(rasBackup).copy(qTargetRect);
    targetImage        = targetImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied, colorTable);

    QImage app(qTargetRect.size(), QImage::Format_ARGB32_Premultiplied);
    QPainter p2(&app);
    p2.setBrush(QColor(255, 255, 255));
    p2.drawRect(app.rect().adjusted(-1, -1, 0, 0));
    p2.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p2.drawImage(QPoint(), m_rasImage, qTargetRect);
    p2.end();

    QPainter p(&targetImage);
    p.setOpacity(opacity);
    p.drawImage(QPoint(), app, app.rect());
    p.end();
    targetImage =
        targetImage.convertToFormat(QImage::Format_Indexed8, colorTable);

    TRasterGR8P targetRas = rasterFromQImage(targetImage);
    ras->copy(targetRas, targetRect.getP00());
  }
}

//----------------------------------------------------------------------------------

void BluredBrush::updateDrawing(const TRasterCM32P rasCM,
                                const TRasterCM32P rasBackupCM,
                                const TRect &bbox, int styleId,
                                int drawOrderMode) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->copy(rasBackupCM->extract(targetRect), targetRect.getP00());
  putOnRasterCM(rasCM->extract(targetRect), m_ras->extract(targetRect), styleId,
                drawOrderMode, m_aboveStyleIds);
}

//----------------------------------------------------------------------------------

void BluredBrush::eraseDrawing(const TRasterCM32P rasCM,
                               const TRasterCM32P rasBackupCM,
                               const TRect &bbox, bool selective,
                               int selectedStyleId,
                               const std::wstring &mode) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->extract(targetRect)->copy(rasBackupCM->extract(targetRect));
  eraseFromRasterCM(rasCM->extract(targetRect), m_ras->extract(targetRect),
                    selective, selectedStyleId, mode);
}

//----------------------------------------------------------------------------------

TRect BluredBrush::getBoundFromPoints(
    const std::vector<TThickPoint> &points) const {
  assert(points.size() <= 3);
  TThickPoint p = points[0];
  double radius = p.thick * 0.5;
  TRectD rectD(p - TPointD(radius, radius), p + TPointD(radius, radius));
  int i;
  for (i = 1; i < (int)points.size(); i++) {
    p      = points[i];
    radius = p.thick * 0.5;
    rectD  = rectD +
            TRectD(p - TPointD(radius, radius), p + TPointD(radius, radius));
  }
  TRect rect(tfloor(rectD.x0), tfloor(rectD.y0), tceil(rectD.x1),
             tceil(rectD.y1));
  return rect;
}
