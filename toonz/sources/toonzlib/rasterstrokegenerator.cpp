

#include "toonz/rasterstrokegenerator.h"
#include "trastercm.h"
#include "toonz/rasterbrush.h"
#include "trop.h"

RasterStrokeGenerator::RasterStrokeGenerator(const TRasterCM32P &raster,
                                             Tasks task, ColorType colorType,
                                             int styleId, const TThickPoint &p,
                                             bool selective, int selectedStyle,
                                             bool lockAlpha, bool keepAntialias,
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
    , m_isPaletteOrder(isPaletteOrder)
    , m_modifierLockAlpha(lockAlpha) {
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
void RasterStrokeGenerator::generateStroke(bool isPencil,
                                           bool isStraight) const {
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
    if (isStraight && size == 3) {
      partialPoints.push_back(points[0]);
      partialPoints.push_back(points[2]);
    } else {
      partialPoints.push_back(points[0]);
      partialPoints.push_back(points[1]);
    }
    rasterBrush(rasBuffer, partialPoints, m_styleId, !isPencil);
    placeOver(m_raster, rasBuffer, newOrigin);
  } else if (size % 2 == 1) /*-- In the case of odd numbers --*/
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
                                                       bool closeStroke,
                                                       bool isStraight) {
  std::vector<TThickPoint> points;
  int size = m_points.size();

  if (isStraight) {
    points.push_back(m_points[0]);
    points.push_back(m_points[2]);
  } else if (size == 3) {
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
    double radius = points[i].thick * 0.5;
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
    /*--Finger Tool Boundary Conditions --*/
    if (m_task == FINGER && (y == 0 || y == rOut->getLy() - 1)) continue;

    TPixelCM32 *inPix  = rIn->pixels(y);
    TPixelCM32 *outPix = rOut->pixels(y);
    TPixelCM32 *outEnd = outPix + rOut->getLx();
    for (; outPix < outEnd; ++inPix, ++outPix) {
      if (m_task == BRUSH) {
        int inTone  = inPix->getTone();
        int outTone = outPix->getTone();
        if (inPix->isPureInk() && !m_selective && !m_modifierLockAlpha) {
          *outPix = TPixelCM32(inPix->getInk(), outPix->getPaint(), inTone);
          continue;
        }
        if (m_modifierLockAlpha && !outPix->isPureInk() &&
            outPix->getPaint() == 0 && outPix->getTone() == 255) {
          *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(), outTone);
          continue;
        }
        if (outPix->isPureInk() && m_selective) {
          if (!m_isPaletteOrder || m_aboveStyleIds.contains(outPix->getInk())) {
            *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(), outTone);
            continue;
          }
        }
        if (inTone <= outTone) {
          *outPix = TPixelCM32(inPix->getInk(), outPix->getPaint(),
                               m_modifierLockAlpha ? outTone : inTone);
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
        int paintIdx     = outPix->getPaint();
        bool changePaint = (!m_selective && !m_modifierLockAlpha) ||
                           (m_selective && paintIdx == 0) ||
                           (m_modifierLockAlpha && paintIdx != 0);
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
        /*-- Boundary Conditions --*/
        if (outPix == rOut->pixels(y) || outPix == outEnd - 1) continue;

        int inkId = inPix->getInk();
        if (inkId == 0) continue;

        TPixelCM32 *neighbourPixels[4];
        neighbourPixels[0] = outPix - 1;               /* left */
        neighbourPixels[1] = outPix + 1;               /* right */
        neighbourPixels[2] = outPix - rOut->getWrap(); /* top */
        neighbourPixels[3] = outPix + rOut->getWrap(); /* bottom */
        int count          = 0;
        int tone           = outPix->getTone();

        /*--- When Invert is off: Fill hole operation ---*/
        if (!m_selective) {
          /*-- For 4 neighborhood pixels --*/
          int minTone = tone;
          for (int p = 0; p < 4; p++) {
            /*-- Count up the items that have darker lines (lower Tone) than the
             * current pixel. --*/
            if (neighbourPixels[p]->getTone() < tone) {
              count++;
              if (neighbourPixels[p]->getTone() < minTone)
                minTone = neighbourPixels[p]->getTone();
            }
          }

          /*--- If 3 or more surrounding pixels are darker, replace with the
           * minimum Tone ---*/
          if (count <= 2) continue;
          *outPix = TPixelCM32(inkId, outPix->getPaint(), minTone);
        }
        /*--- When Invert is ON: Operation to trim protrusion ---*/
        else {
          if (outPix->isPurePaint() || outPix->getInk() != inkId) continue;

          /*-- For 4 neighborhood pixels --*/
          int maxTone = tone;
          for (int p = 0; p < 4; p++) {
            /*--
             * Count up items whose Ink# is not Current or whose line is thinner
             * than your Pixel (Tone is higher).
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

          /*---  If 3 or more surrounding pixels are thinner, replace with the
           * maximum Tone ---*/
          if (count <= 2) continue;
          *outPix = TPixelCM32((maxTone == 255) ? 0 : inkId, outPix->getPaint(),
                               maxTone);
        }
      }
    }
  }
}

//-----------------------------------------------------------

TRect RasterStrokeGenerator::getLastRect(bool isStraight) const {
  std::vector<TThickPoint> points;
  int size = m_points.size();

  if (isStraight) {
    points.push_back(m_points[0]);
    points.push_back(m_points[2]);
  } else if (size == 3) {
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
