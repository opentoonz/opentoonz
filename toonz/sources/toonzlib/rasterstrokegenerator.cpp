

#include "toonz/rasterstrokegenerator.h"
#include "trastercm.h"
#include "toonz/rasterbrush.h"
#include "trop.h"

RasterStrokeGenerator::RasterStrokeGenerator(const TRasterCM32P &raster,
                                             Tasks task, ColorType colorType,
                                             int styleId, const TThickPoint &p,
                                             bool selective, int selectedStyle,
                                             bool keepAntialias,
                                             bool isPaletteOrder)
    : m_raster(raster)
    , m_boxOfRaster(TRect(raster->getSize()))
    , m_styleId(styleId)
    , m_selective(selective)
    , m_task(task)
    , m_colorType(colorType)
    , m_eraseStyle(4095)
    , m_selectedStyle(selectedStyle)
    , m_keepAntiAlias(keepAntialias)
    , m_doAnArc(false)
    , m_isPaletteOrder(isPaletteOrder) {
  TThickPoint pp = p;
  m_points.push_back(pp);
  if (task == ERASE) m_styleId = m_eraseStyle;
}

//-----------------------------------------------------------

RasterStrokeGenerator::~RasterStrokeGenerator() {}

//-----------------------------------------------------------

void RasterStrokeGenerator::add(const TThickPoint &p) {
  TThickPoint pp = p;
  TThickPoint mid((m_points.back() + pp) * 0.5,
                  (p.thick + m_points.back().thick) * 0.5);
  m_points.push_back(mid);
  m_points.push_back(pp);
}

//-----------------------------------------------------------

// Disegna il tratto interamente
void RasterStrokeGenerator::generateStroke(bool isPencil) const {
  std::vector<TThickPoint> points(m_points);
  int size = points.size();
  // Prende un buffer trasparente di appoggio
  TRect box        = getBBox(points);
  TPoint newOrigin = box.getP00();
  TRasterCM32P rasBuffer(box.getSize());
  rasBuffer->clear();

  // Trasla i punti secondo il nuovo sitema di riferimento
  translatePoints(points, newOrigin);

  std::vector<TThickPoint> partialPoints;
  if (size == 1) {
    rasterBrush(rasBuffer, points, m_styleId, !isPencil);
    placeOver(m_raster, rasBuffer, newOrigin);
  } else if (size <= 3) {
    std::vector<TThickPoint> partialPoints;
    partialPoints.push_back(points[0]);
    partialPoints.push_back(points[1]);
    rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
    placeOver(m_raster, rasBuffer, newOrigin);
  } else if (size % 2 == 1) /*-- 奇数の場合 --*/
  {
    int strokeCount = (size - 1) / 2 - 1;
    std::vector<TThickPoint> partialPoints;
    partialPoints.push_back(points[0]);
    partialPoints.push_back(points[1]);
    rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
    placeOver(m_raster, rasBuffer, newOrigin);
    for (int i = 0; i < strokeCount; i++) {
      partialPoints.clear();
      rasBuffer->clear();
      partialPoints.push_back(points[i * 2 + 1]);
      partialPoints.push_back(points[i * 2 + 2]);
      partialPoints.push_back(points[i * 2 + 3]);
      if (i == strokeCount - 1) partialPoints.push_back(points[i * 2 + 4]);

      rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
      placeOver(m_raster, rasBuffer, newOrigin);
    }
  } else {
    std::vector<TThickPoint> partialPoints;
    partialPoints.push_back(points[0]);
    partialPoints.push_back(points[1]);
    rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
    placeOver(m_raster, rasBuffer, newOrigin);
    if (size > 2) {
      partialPoints.clear();
      std::vector<TThickPoint>::iterator it = points.begin();
      it++;
      partialPoints.insert(partialPoints.begin(), it, points.end());
      rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
      placeOver(m_raster, rasBuffer, newOrigin);
    }
  }
}

//-----------------------------------------------------------

TRect RasterStrokeGenerator::generateLastPieceOfStroke(bool isPencil,
                                                       bool closeStroke) {
  std::vector<TThickPoint> points;
  int size = m_points.size();

  if (size == 3) {
    points.push_back(m_points[0]);
    points.push_back(m_points[1]);
  } else if (size == 1)
    points.push_back(m_points[0]);
  else {
    points.push_back(m_points[size - 4]);
    points.push_back(m_points[size - 3]);
    points.push_back(m_points[size - 2]);
    if (closeStroke) points.push_back(m_points[size - 1]);
  }

  TRect box        = getBBox(points);
  TPoint newOrigin = box.getP00();
  TRasterCM32P rasBuffer(box.getSize());
  rasBuffer->clear();

  // Trasla i punti secondo il nuovo sitema di riferimento
  translatePoints(points, newOrigin);

  rasterBrush(rasBuffer, points, m_styleId, !isPencil);
  placeOver(m_raster, rasBuffer, newOrigin);
  return box;
}

//-----------------------------------------------------------

// Ritorna il rettangolo contenente i dischi generati con centri in "points" e
// diametro "points.thick" +3 pixel a bordo
TRect RasterStrokeGenerator::getBBox(
    const std::vector<TThickPoint> &points) const {
  double x0 = (std::numeric_limits<double>::max)(),
         y0 = (std::numeric_limits<double>::max)(),
         x1 = -(std::numeric_limits<double>::max)(),
         y1 = -(std::numeric_limits<double>::max)();
  for (int i = 0; i < (int)points.size(); i++) {
    double radius                     = points[i].thick * 0.5;
    if (points[i].x - radius < x0) x0 = points[i].x - radius;
    if (points[i].x + radius > x1) x1 = points[i].x + radius;
    if (points[i].y - radius < y0) y0 = points[i].y - radius;
    if (points[i].y + radius > y1) y1 = points[i].y + radius;
  }
  return TRect(TPoint((int)floor(x0 - 3), (int)floor(y0 - 3)),
               TPoint((int)ceil(x1 + 3), (int)ceil(y1 + 3)));
}

//-----------------------------------------------------------

// Ricalcola i punti in un nuovo sistema di riferimento
void RasterStrokeGenerator::translatePoints(std::vector<TThickPoint> &points,
                                            const TPoint &newOrigin) const {
  TPointD p(newOrigin.x, newOrigin.y);
  for (int i = 0; i < (int)points.size(); i++) points[i] -= p;
}

//-----------------------------------------------------------

// Effettua la over.
void RasterStrokeGenerator::placeOver(const TRasterCM32P &out,
                                      const TRasterCM32P &in,
                                      const TPoint &p) const {
  TRect inBox  = in->getBounds() + p;
  TRect outBox = out->getBounds();
  TRect box    = inBox * outBox;
  if (box.isEmpty()) return;
  TRasterCM32P rOut = out->extract(box);
  TRect box2        = box - p;
  TRasterCM32P rIn  = in->extract(box2);
  for (int y = 0; y < rOut->getLy(); y++) {
    /*-- Finger Toolの境界条件 --*/
    if (m_task == FINGER && (y == 0 || y == rOut->getLy() - 1)) continue;

    TPixelCM32 *inPix  = rIn->pixels(y);
    TPixelCM32 *outPix = rOut->pixels(y);
    TPixelCM32 *outEnd = outPix + rOut->getLx();
    for (; outPix < outEnd; ++inPix, ++outPix) {
      if (m_task == BRUSH) {
        int inTone  = inPix->getTone();
        int outTone = outPix->getTone();
        if (inPix->isPureInk() && !m_selective) {
          *outPix = TPixelCM32(inPix->getInk(), outPix->getPaint(), inTone);
          continue;
        }
        if (outPix->isPureInk() && m_selective) {
          if (!m_isPaletteOrder || m_aboveStyleIds.contains(outPix->getInk())) {
            *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(), outTone);
            continue;
          }
        }
        if (inTone <= outTone) {
          *outPix = TPixelCM32(inPix->getInk(), outPix->getPaint(), inTone);
        }
      }
      if (m_task == ERASE) {
        if (m_colorType == INK) {
          if (!m_keepAntiAlias) {
            if (inPix->getTone() == 0 &&
                (!m_selective ||
                 (m_selective && outPix->getInk() == m_selectedStyle))) {
              outPix->setTone(255);
            }
          } else if (inPix->getTone() < 255 &&
                     (!m_selective ||
                      (m_selective && outPix->getInk() == m_selectedStyle))) {
            outPix->setTone(
                std::max(outPix->getTone(), 255 - inPix->getTone()));
          }
        }
        if (m_colorType == PAINT) {
          if (inPix->getTone() == 0 &&
              (!m_selective ||
               (m_selective && outPix->getPaint() == m_selectedStyle)))
            outPix->setPaint(0);
        }
        if (m_colorType == INKNPAINT) {
          if (inPix->getTone() < 255 &&
              (!m_selective ||
               (m_selective && outPix->getPaint() == m_selectedStyle)))
            outPix->setPaint(0);
          if (!m_keepAntiAlias) {
            if (inPix->getTone() == 0 &&
                (!m_selective ||
                 (m_selective && outPix->getInk() == m_selectedStyle))) {
              outPix->setTone(255);
            }
          } else if (inPix->getTone() < 255 &&
                     (!m_selective ||
                      (m_selective && outPix->getInk() == m_selectedStyle))) {
            outPix->setTone(
                std::max(outPix->getTone(), 255 - inPix->getTone()));
          }
        }
      } else if (m_task == PAINTBRUSH) {
        if (!inPix->isPureInk()) continue;
        bool changePaint =
            !m_selective || (m_selective && outPix->getPaint() == 0);
        if (m_colorType == INK)
          *outPix = TPixelCM32(inPix->getInk(), outPix->getPaint(),
                               outPix->getTone());
        if (m_colorType == PAINT)
          if (changePaint)
            *outPix = TPixelCM32(outPix->getInk(), inPix->getInk(),
                                 outPix->getTone());
        if (m_colorType == INKNPAINT)
          *outPix =
              TPixelCM32(inPix->getInk(),
                         changePaint ? inPix->getInk() : outPix->getPaint(),
                         outPix->getTone());
      }

      /*-- Finger tool --*/
      else if (m_task == FINGER) {
        /*-- 境界条件 --*/
        if (outPix == rOut->pixels(y) || outPix == outEnd - 1) continue;

        int inkId = inPix->getInk();
        if (inkId == 0) continue;

        TPixelCM32 *neighbourPixels[4];
        neighbourPixels[0] = outPix - 1;               /* 左 */
        neighbourPixels[1] = outPix + 1;               /* 右 */
        neighbourPixels[2] = outPix - rOut->getWrap(); /* 上 */
        neighbourPixels[3] = outPix + rOut->getWrap(); /* 下 */
        int count          = 0;
        int tone           = outPix->getTone();

        /*--- Invertがオフのとき：穴を埋める操作 ---*/
        if (!m_selective) {
          /*-- 4近傍のピクセルについて --*/
          int minTone = tone;
          for (int p = 0; p < 4; p++) {
            /*-- 自分Pixelより線が濃い（Toneが低い）ものを集計する --*/
            if (neighbourPixels[p]->getTone() < tone) {
              count++;
              if (neighbourPixels[p]->getTone() < minTone)
                minTone = neighbourPixels[p]->getTone();
            }
          }

          /*--- 周りの３つ以上のピクセルが濃ければ、最小Toneと置き換える ---*/
          if (count <= 2) continue;
          *outPix = TPixelCM32(inkId, outPix->getPaint(), minTone);
        }
        /*--- InvertがONのとき：出っ張りを削る操作 ---*/
        else {
          if (outPix->isPurePaint() || outPix->getInk() != inkId) continue;

          /*-- 4近傍のピクセルについて --*/
          int maxTone = tone;
          for (int p = 0; p < 4; p++) {
            /*--
             * Ink＃がCurrentでない、または、自分Pixelより線が薄い（Toneが高い）ものを集計する
             * --*/
            if (neighbourPixels[p]->getInk() != inkId) {
              count++;
              maxTone = 255;
            } else if (neighbourPixels[p]->getTone() > tone) {
              count++;
              if (neighbourPixels[p]->getTone() > maxTone)
                maxTone = neighbourPixels[p]->getTone();
            }
          }

          /*--- 周りの３つ以上のピクセルが薄ければ、最大Toneと置き換える ---*/
          if (count <= 2) continue;
          *outPix = TPixelCM32((maxTone == 255) ? 0 : inkId, outPix->getPaint(),
                               maxTone);
        }
      }
    }
  }
}

//-----------------------------------------------------------

TRect RasterStrokeGenerator::getLastRect() const {
  std::vector<TThickPoint> points;
  int size = m_points.size();

  if (size == 3) {
    points.push_back(m_points[0]);
    points.push_back(m_points[1]);
  } else if (size == 1)
    points.push_back(m_points[0]);
  else {
    points.push_back(m_points[size - 4]);
    points.push_back(m_points[size - 3]);
    points.push_back(m_points[size - 2]);
    points.push_back(m_points[size - 1]);
  }
  return getBBox(points);
}
